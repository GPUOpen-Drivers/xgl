/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
 ***********************************************************************************************************************
 * @file  vk_pipeline_layout.cpp
 * @brief Contains implementation of Vulkan pipeline layout objects.
 ***********************************************************************************************************************
 */
#include "include/vk_pipeline_layout.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_shader.h"
#include "include/vk_sampler.h"
#include "include/vk_utils.h"
#include "palMetroHash.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
// Generates the API hash using the contents of the VkPipelineLayoutCreateInfo struct
uint64_t PipelineLayout::BuildApiHash(
    const VkPipelineLayoutCreateInfo* pCreateInfo)
{
    Util::MetroHash64 hasher;

    hasher.Update(pCreateInfo->flags);
    hasher.Update(pCreateInfo->setLayoutCount);

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
    {
        hasher.Update(DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i])->GetApiHash());
    }

    hasher.Update(pCreateInfo->pushConstantRangeCount);

    for (uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
    {
        hasher.Update(pCreateInfo->pPushConstantRanges[i]);
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t* const>(&hash));

    return hash;
}

// =====================================================================================================================
constexpr size_t PipelineLayout::GetMaxResMappingRootNodeSize()
{
    return
                sizeof(Vkgc::ResourceMappingRootNode)
        ;
}

constexpr size_t PipelineLayout::GetMaxResMappingNodeSize()
{
    return
                sizeof(Vkgc::ResourceMappingNode)
        ;
}

constexpr size_t PipelineLayout::GetMaxStaticDescValueSize()
{
    return
                sizeof(Vkgc::StaticDescriptorValue)
        ;
}

// =====================================================================================================================
PipelineLayout::PipelineLayout(
    const Device*       pDevice,
    const Info&         info,
    const PipelineInfo& pipelineInfo,
    uint64_t            apiHash)
    :
    m_info(info),
    m_pipelineInfo(pipelineInfo),
    m_pDevice(pDevice),
    m_apiHash(apiHash)
{

}

// =====================================================================================================================
VkResult PipelineLayout::ConvertCreateInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo,
    SetUserDataLayout*                pSetUserDataLayouts)
{
    // We currently allocate user data registers for various resources in the following fashion:
    // First user data registers will hold the descriptor set bindings in increasing order by set index.
    // For each descriptor set binding we first store the dynamic descriptor data (if there's dynamic section data)
    // followed by the set pointer (if there's static section data).
    // Push constants follow the descriptor set binding data.
    // Finally, the vertex buffer table pointer is in the last user data register when applicable.
    // This allocation allows the descriptor set bindings to easily persist across pipeline switches.

    VkResult result = VK_SUCCESS;

    pPipelineInfo->numRsrcMapNodes        = 0;
    pPipelineInfo->numDescRangeValueNodes = 0;

    // Always allocates:
    // 1 extra user data node for the vertex buffer table pointer
    // 1 extra user data node for push constant
    pPipelineInfo->numUserDataNodes   = 2;

    pInfo->setCount = pIn->setLayoutCount;

    pInfo->userDataRegCount      = 0;

    pInfo->userDataLayout.transformFeedbackRegBase  = 0;
    pInfo->userDataLayout.transformFeedbackRegCount = 0;
    pInfo->userDataLayout.pushConstRegBase          = 0;
    pInfo->userDataLayout.pushConstRegCount         = 0;
    pInfo->userDataLayout.setBindingRegCount        = 0;
    pInfo->userDataLayout.setBindingRegBase         = 0;

    // Reserve an user-data to store the VA of buffer for transform feedback.
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        pInfo->userDataLayout.transformFeedbackRegCount = 1;
        pInfo->userDataRegCount                        += pInfo->userDataLayout.transformFeedbackRegCount;
        pPipelineInfo->numUserDataNodes                += 1;
    }

    // Reserve one user data nodes for uber-fetch shader.
    if (pDevice->GetRuntimeSettings().enableUberFetchShader)
    {
        pPipelineInfo->numUserDataNodes += 1;
    }

    // Calculate the number of bytes needed for push constants
    uint32_t pushConstantsSizeInBytes = 0;

    for (uint32_t i = 0; i < pIn->pushConstantRangeCount; ++i)
    {
        const VkPushConstantRange* pRange = &pIn->pPushConstantRanges[i];

        // Test if this push constant range is active in at least one stage
        if (pRange->stageFlags != 0)
        {
            pushConstantsSizeInBytes = Util::Max(pushConstantsSizeInBytes, pRange->offset + pRange->size);
        }
    }

    uint32_t pushConstRegCount = pushConstantsSizeInBytes / sizeof(uint32_t);

    pInfo->userDataLayout.pushConstRegBase  = pInfo->userDataLayout.transformFeedbackRegCount;
    pInfo->userDataLayout.pushConstRegCount = pushConstRegCount;
    pInfo->userDataRegCount                += pushConstRegCount;

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);

    // Total number of dynamic descriptors across all descriptor sets
    uint32_t totalDynDescCount = 0;

    // Populate user data layouts for each descriptor set that is active
    pInfo->userDataLayout.setBindingRegBase = pInfo->userDataRegCount;

    for (uint32_t i = 0; i < pInfo->setCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pSetUserDataLayouts[i];

        // Initialize the set layout info
        const auto& setLayoutInfo = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i])->Info();

        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescCount         = setLayoutInfo.numDynamicDescriptors;
        pSetUserData->firstRegOffset       = pInfo->userDataRegCount - pInfo->userDataLayout.setBindingRegBase;
        pSetUserData->totalRegCount        = 0;

        // Test if this set is active in at least one stage
        if (setLayoutInfo.activeStageMask != 0)
        {
            // Accumulate the space needed by all resource nodes for this set
            pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;

            // Add count for FMASK nodes
            if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
            {
                pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;
            }

            // Add space for the user data node entries needed for dynamic descriptors
            pPipelineInfo->numUserDataNodes += setLayoutInfo.dyn.numRsrcMapNodes;

            // Add space for immutable sampler descriptor storage needed by the set
            pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

            // Reserve user data register space for dynamic descriptor data
            pSetUserData->dynDescDataRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;

            pSetUserData->totalRegCount += pSetUserData->dynDescCount * DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);

            totalDynDescCount += setLayoutInfo.numDynamicDescriptors;

            if (setLayoutInfo.sta.numRsrcMapNodes > 0)
            {
                // If the set has a static portion reserve an extra user data node entry for the set pointer
                pPipelineInfo->numUserDataNodes++;

                // In this case we also reserve the user data for the set pointer
                pSetUserData->setPtrRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
                pSetUserData->totalRegCount   += SetPtrRegCount;
            }
        }

        // Add the number of user data regs used by this set to the total count for the whole layout
        pInfo->userDataRegCount += pSetUserData->totalRegCount;
    }

    // Calculate total number of user data regs used for active descriptor set data
    pInfo->userDataLayout.setBindingRegCount = pInfo->userDataRegCount - pInfo->userDataLayout.setBindingRegBase;

    VK_ASSERT(totalDynDescCount <= MaxDynamicDescriptors);

    // In case we need an internal vertex buffer table, add nodes required for its entries, and its set pointer.
    pPipelineInfo->numRsrcMapNodes += Pal::MaxVertexBuffers;

    // Calculate the buffer size necessary for all resource mapping
    pPipelineInfo->mappingBufferSize =
        (pPipelineInfo->numUserDataNodes * GetMaxResMappingRootNodeSize()) +
        (pPipelineInfo->numRsrcMapNodes * GetMaxResMappingNodeSize()) +
        (pPipelineInfo->numDescRangeValueNodes * GetMaxStaticDescValueSize());

    // If we go past our user data limit, we can't support this pipeline
    if (pInfo->userDataRegCount >=
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Creates a pipeline layout object.
VkResult PipelineLayout::Create(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*      pAllocator,
    VkPipelineLayout*                 pPipelineLayout)
{
    VK_ASSERT((pCreateInfo->setLayoutCount == 0) || (pCreateInfo->pSetLayouts != nullptr));

    VkResult     result       = VK_SUCCESS;
    Info         info         = {};
    PipelineInfo pipelineInfo = {};
    uint64_t     apiHash      = BuildApiHash(pCreateInfo);

    size_t setLayoutsArraySize = 0;

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
    {
        DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);
        setLayoutsArraySize += pLayout->GetObjectSize(VK_SHADER_STAGE_ALL);
    }

    // Need to add extra storage for DescriptorSetLayout*, SetUserDataLayout, the descriptor set layouts themselves,
    // the resource mapping nodes, and the descriptor range values
    const size_t apiSize                 = sizeof(PipelineLayout);
    const size_t setUserDataLayoutSize   =
        Util::Pow2Align((pCreateInfo->setLayoutCount * sizeof(SetUserDataLayout)), ExtraDataAlignment());
    const size_t descriptorSetLayoutSize =
        Util::Pow2Align((pCreateInfo->setLayoutCount * sizeof(DescriptorSetLayout*)), ExtraDataAlignment());

    size_t objSize = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize + setLayoutsArraySize;

    void* pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

    if (pSysMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    SetUserDataLayout*    pSetUserData = nullptr;
    DescriptorSetLayout** ppSetLayouts = nullptr;

    if (result == VK_SUCCESS)
    {
        pSetUserData = static_cast<SetUserDataLayout*>(Util::VoidPtrInc(pSysMem, apiSize));
        ppSetLayouts = static_cast<DescriptorSetLayout**>(
            Util::VoidPtrInc(pSysMem, apiSize + setUserDataLayoutSize));

        result = ConvertCreateInfo(
            pDevice,
            pCreateInfo,
            &info,
            &pipelineInfo,
            pSetUserData);
    }

    if (result == VK_SUCCESS)
    {
        size_t currentSetLayoutOffset = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize;

        for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
        {
            ppSetLayouts[i] = reinterpret_cast<DescriptorSetLayout*>(Util::VoidPtrInc(pSysMem, currentSetLayoutOffset));

            const DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);

            // Copy the original descriptor set layout object
            pLayout->Copy(pDevice, VK_SHADER_STAGE_ALL, ppSetLayouts[i]);

            currentSetLayoutOffset += pLayout->GetObjectSize(VK_SHADER_STAGE_ALL);
        }

        VK_PLACEMENT_NEW(pSysMem) PipelineLayout(pDevice, info, pipelineInfo, apiHash);

        *pPipelineLayout = PipelineLayout::HandleFromVoidPointer(pSysMem);
    }

    if (result != VK_SUCCESS)
    {
        if (pSysMem != nullptr)
        {
            pDevice->FreeApiObject(pAllocator, pSysMem);
        }
    }

    return result;
}

// =====================================================================================================================
// Create a pipeline layout object via existing pipeline layouts
VkResult PipelineLayout::Create(
    const Device*                pDevice,
    const VkPipelineLayout*      pReference,
    const VkShaderStageFlags*    pRefShaderMask,
    const uint32_t               refCount,
    const VkAllocationCallbacks* pAllocator,
    VkPipelineLayout*            pPipelineLayout)
{
    VkResult result = VK_SUCCESS;

    VkPipelineLayoutCreateInfo createInfo   = {};
    Info                       info         = {};
    PipelineInfo               pipelineInfo = {};
    uint64_t                   apiHash      = 0;

    Util::Vector<size_t,                4, Util::GenericAllocator> mergedDescriptorSetLayoutsSize{ nullptr };
    Util::Vector<VkDescriptorSetLayout, 4, Util::GenericAllocator> mergedDescriptorSetLayouts{ nullptr };
    Util::Vector<VkShaderStageFlags,    4, Util::GenericAllocator> mergedShaderMasks{ nullptr };

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset     = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;

    size_t setLayoutsArraySize = 0;

    for (uint32_t set = 0; ; ++set)
    {
        Util::Vector<VkDescriptorSetLayout, 2, Util::GenericAllocator> setLayouts     = { nullptr };
        Util::Vector<VkShaderStageFlags,    2, Util::GenericAllocator> setShaderMasks = { nullptr };

        bool aboveLargestSet = true;

        for (uint32_t i = 0; i < refCount; ++i)
        {
            const PipelineLayout* pRef = PipelineLayout::ObjectFromHandle(pReference[i]);

            if (pRef != nullptr)
            {
                const PipelineLayout::Info& layoutInfo = pRef->GetInfo();

                if (set < layoutInfo.setCount)
                {
                    aboveLargestSet = false;

                    const DescriptorSetLayout* pLayout = pRef->GetSetLayouts(set);

                    if (pLayout->IsEmpty(pRefShaderMask[i]) == false)
                    {
                        setLayouts.PushBack(DescriptorSetLayout::HandleFromObject(pLayout));
                        setShaderMasks.PushBack(pRefShaderMask[i]);
                    }
                }
            }
        }

        if (aboveLargestSet == true)
        {
            break;
        }

        const size_t objSize =
            DescriptorSetLayout::GetObjectSize(setLayouts.Data(), setShaderMasks.Data(), setLayouts.size());
        setLayoutsArraySize += objSize;
        mergedDescriptorSetLayoutsSize.PushBack(objSize);
    }

    for (uint32_t i = 0; i < refCount; ++i)
    {
        const PipelineLayout* pRef = PipelineLayout::ObjectFromHandle(pReference[i]);

        if (pRef != nullptr)
        {
            const PipelineLayout::Info& layoutInfo = pRef->GetInfo();

            pushConstantRange.size =
                Util::Max<uint32_t>(pushConstantRange.size,
                                    layoutInfo.userDataLayout.pushConstRegCount * sizeof(uint32_t));

        }
    }

    uint32_t setLayoutCount = mergedDescriptorSetLayoutsSize.size();
    const size_t apiSize = sizeof(PipelineLayout);
    const size_t setUserDataLayoutSize =
        Util::Pow2Align((setLayoutCount * sizeof(SetUserDataLayout)), ExtraDataAlignment());
    const size_t descriptorSetLayoutSize =
        Util::Pow2Align((setLayoutCount * sizeof(DescriptorSetLayout*)), ExtraDataAlignment());

    size_t objSize = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize + setLayoutsArraySize;

    void* pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

    if (pSysMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        memset(pSysMem, 0, objSize);

        SetUserDataLayout*    pSetUserData = nullptr;
        DescriptorSetLayout** ppSetLayouts = nullptr;

        pSetUserData = static_cast<SetUserDataLayout*>(Util::VoidPtrInc(pSysMem, apiSize));
        ppSetLayouts = static_cast<DescriptorSetLayout**>(
            Util::VoidPtrInc(pSysMem, apiSize + setUserDataLayoutSize));

        size_t currentSetLayoutOffset = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize;

        for (uint32_t set = 0; ; ++set)
        {
            Util::Vector<VkDescriptorSetLayout, 2, Util::GenericAllocator> setLayouts     = { nullptr };
            Util::Vector<VkShaderStageFlags,    2, Util::GenericAllocator> setShaderMasks = { nullptr };

            bool aboveLargestSet = true;

            for (uint32_t i = 0; i < refCount; ++i)
            {
                const PipelineLayout* pRef = PipelineLayout::ObjectFromHandle(pReference[i]);

                if (pRef != nullptr)
                {
                    const PipelineLayout::Info& layoutInfo = pRef->GetInfo();

                    if (set < layoutInfo.setCount)
                    {
                        aboveLargestSet = false;

                        const DescriptorSetLayout* pLayout = pRef->GetSetLayouts(set);

                        if (pLayout->IsEmpty(pRefShaderMask[i]) == false)
                        {
                            setLayouts.PushBack(DescriptorSetLayout::HandleFromObject(pLayout));
                            setShaderMasks.PushBack(pRefShaderMask[i]);
                        }
                    }
                }
            }

            if (aboveLargestSet == true)
            {
                break;
            }

            ppSetLayouts[set] = reinterpret_cast<DescriptorSetLayout*>(Util::VoidPtrInc(pSysMem, currentSetLayoutOffset));

            DescriptorSetLayout::Merge(pDevice,
                                       setLayouts.Data(),
                                       setShaderMasks.Data(),
                                       setLayouts.size(),
                                       ppSetLayouts[set]);

            mergedDescriptorSetLayouts.PushBack(DescriptorSetLayout::HandleFromObject(ppSetLayouts[set]));

            currentSetLayoutOffset += mergedDescriptorSetLayoutsSize[set];
        }

        createInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pNext          = nullptr;
        createInfo.setLayoutCount = mergedDescriptorSetLayouts.size();
        createInfo.pSetLayouts    = mergedDescriptorSetLayouts.Data();
        if (pushConstantRange.size > 0)
        {
            createInfo.pushConstantRangeCount = 1;
            createInfo.pPushConstantRanges    = &pushConstantRange;
        }

        apiHash = BuildApiHash(&createInfo);

        result = ConvertCreateInfo(
            pDevice,
            &createInfo,
            &info,
            &pipelineInfo,
            pSetUserData);
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSysMem) PipelineLayout(pDevice, info, pipelineInfo, apiHash);

        *pPipelineLayout = PipelineLayout::HandleFromVoidPointer(pSysMem);
    }

    if (result != VK_SUCCESS)
    {
        if (pSysMem != nullptr)
        {
            pDevice->FreeApiObject(pAllocator, pSysMem);
        }
    }

    return result;
}

// =====================================================================================================================
// Translates VkDescriptorType to VKGC ResourceMappingNodeType
Vkgc::ResourceMappingNodeType PipelineLayout::MapLlpcResourceNodeType(
    VkDescriptorType descriptorType)
{
    auto nodeType = Vkgc::ResourceMappingNodeType::Unknown;
    switch (static_cast<uint32_t>(descriptorType))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorSampler;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorCombinedTexture;
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorResource;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 49
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorImage;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact;
        break;
#else
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorResource;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
        break;
#endif

    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
        break;
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        nodeType = Vkgc::ResourceMappingNodeType::PushConst;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }
    return nodeType;
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for a descriptor set
VkResult PipelineLayout::BuildLlpcSetMapping(
    uint32_t                       visibility,
    uint32_t                       setIndex,
    const DescriptorSetLayout*     pLayout,
    Vkgc::ResourceMappingRootNode* pDynNodes,
    uint32_t*                      pDynNodeCount,
    Vkgc::ResourceMappingNode*     pStaNodes,
    uint32_t*                      pStaNodeCount,
    Vkgc::StaticDescriptorValue*   pDescriptorRangeValue,
    uint32_t*                      pDescriptorRangeCount,
    uint32_t                       userDataRegBase
    ) const
{
    *pStaNodeCount         = 0;
    *pDynNodeCount         = 0;
    *pDescriptorRangeCount = 0;

    for (uint32_t bindingIndex = 0; bindingIndex < pLayout->Info().count; ++bindingIndex)
    {
        auto binding = pLayout->Binding(bindingIndex);

        // If the binding has a static section then add a static section node for it.
        if (binding.sta.dwSize > 0)
        {
            auto pNode = &pStaNodes[*pStaNodeCount];

            pNode->type                 = MapLlpcResourceNodeType(binding.info.descriptorType);
            pNode->offsetInDwords       = binding.sta.dwOffset;
            pNode->sizeInDwords         = binding.sta.dwSize;
            pNode->srdRange.binding     = binding.info.binding;
            pNode->srdRange.set         =
                setIndex;
            (*pStaNodeCount)++;

            if (binding.imm.dwSize > 0)
            {
                const uint32_t  arraySize             = binding.imm.dwSize / binding.imm.dwArrayStride;
                const uint32_t* pImmutableSamplerData = pLayout->Info().imm.pImmutableSamplerData +
                                                        binding.imm.dwOffset;

                if (binding.bindingFlags.ycbcrConversionUsage == 0)
                {
                    pDescriptorRangeValue->type = Vkgc::ResourceMappingNodeType::DescriptorSampler;
                }
                else
                {
                    pNode->type = Vkgc::ResourceMappingNodeType::DescriptorYCbCrSampler;
                    pDescriptorRangeValue->type = Vkgc::ResourceMappingNodeType::DescriptorYCbCrSampler;
                }

                pDescriptorRangeValue->set         = setIndex;
                pDescriptorRangeValue->binding     = binding.info.binding;
                pDescriptorRangeValue->pValue      = pImmutableSamplerData;
                pDescriptorRangeValue->arraySize   = arraySize;
                pDescriptorRangeValue->visibility  = visibility;
                ++pDescriptorRangeValue;
                ++(*pDescriptorRangeCount);
            }
        }

        // If the binding has a dynamic section then add a dynamic section node for it.
        if (binding.dyn.dwSize > 0)
        {
            VK_ASSERT((binding.info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                      (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC));
            auto pNode = &pDynNodes[*pDynNodeCount];
            if (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                pNode->node.type = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorBufferCompact :
                    Vkgc::ResourceMappingNodeType::DescriptorBuffer;

            }
            else
            {
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 49
                pNode->node.type = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact:
                    Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
#else
                pNode->node.type = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorBufferCompact :
                    Vkgc::ResourceMappingNodeType::DescriptorBuffer;
#endif
            }
            pNode->node.offsetInDwords      = userDataRegBase + binding.dyn.dwOffset;
            pNode->node.sizeInDwords        = binding.dyn.dwSize;
            pNode->node.srdRange.binding    = binding.info.binding;
            pNode->node.srdRange.set        = setIndex;
            pNode->visibility               = visibility;
            (*pDynNodeCount)++;
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// This function populates the resource mapping node details to the shader-stage specific pipeline info structure.
VkResult PipelineLayout::BuildLlpcPipelineMapping(
    const uint32_t             stageMask,
    VbInfo*                    pVbInfo,
    void*                      pBuffer,
    bool                       appendFetchShaderCb,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VkResult result = VK_SUCCESS;
    Vkgc::ResourceMappingRootNode* pUserDataNodes = static_cast<Vkgc::ResourceMappingRootNode*>(pBuffer);
    Vkgc::ResourceMappingNode* pResourceNodes =
        reinterpret_cast<Vkgc::ResourceMappingNode*>(pUserDataNodes + m_pipelineInfo.numUserDataNodes);
    Vkgc::StaticDescriptorValue* pDescriptorRangeValues =
        reinterpret_cast<Vkgc::StaticDescriptorValue*>(pResourceNodes + m_pipelineInfo.numRsrcMapNodes);

    uint32_t userDataNodeCount    = 0; // Number of consumed ResourceMappingRootNodes
    uint32_t mappingNodeCount     = 0; // Number of consumed ResourceMappingNodes (only sub-nodes)
    uint32_t descriptorRangeCount = 0; // Number of consumed StaticResourceValues

    if (m_info.userDataLayout.transformFeedbackRegCount > 0)
    {
        uint32_t xfbStages       = (stageMask & (Vkgc::ShaderStageFragmentBit - 1)) >> 1;
        uint32_t lastXfbStageBit = Vkgc::ShaderStageVertexBit;

        while (xfbStages > 0)
        {
            lastXfbStageBit <<= 1;
            xfbStages >>= 1;
        }

        if (lastXfbStageBit != 0)
        {
            auto pTransformFeedbackNode = &pUserDataNodes[userDataNodeCount];
            pTransformFeedbackNode->node.type           = Vkgc::ResourceMappingNodeType::StreamOutTableVaPtr;
            pTransformFeedbackNode->node.offsetInDwords = m_info.userDataLayout.transformFeedbackRegBase;
            pTransformFeedbackNode->node.sizeInDwords   = m_info.userDataLayout.transformFeedbackRegCount;
            pTransformFeedbackNode->visibility          = lastXfbStageBit;

            userDataNodeCount += 1;
        }
    }

    // TODO: Build the internal push constant resource mapping
        if (m_info.userDataLayout.pushConstRegCount > 0)
        {
            auto pPushConstNode = &pUserDataNodes[userDataNodeCount];
            pPushConstNode->node.type             = Vkgc::ResourceMappingNodeType::PushConst;
            pPushConstNode->node.offsetInDwords   = m_info.userDataLayout.pushConstRegBase;
            pPushConstNode->node.sizeInDwords     = m_info.userDataLayout.pushConstRegCount;
            pPushConstNode->node.srdRange.set     = Vkgc::InternalDescriptorSetId;
            pPushConstNode->visibility            = stageMask;

            userDataNodeCount += 1;
        }
    // Build descriptor for each set
    for (uint32_t setIndex = 0; (setIndex < m_info.setCount) && (result == VK_SUCCESS); ++setIndex)
    {
        const auto pSetUserData = &GetSetUserData(setIndex);
        const auto pSetLayout   = GetSetLayouts(setIndex);

        uint32_t visibility = stageMask & VkToVkgcShaderStageMask(pSetLayout->Info().activeStageMask);

        if (visibility != 0)
        {
            // Build the resource mapping nodes for the contents of this set.
            auto pDynNodes   = &pUserDataNodes[userDataNodeCount];
            auto pStaNodes   = &pResourceNodes[mappingNodeCount];
            auto pDescValues = &pDescriptorRangeValues[descriptorRangeCount];

            uint32_t dynNodeCount   = 0;
            uint32_t staNodeCount   = 0;
            uint32_t descRangeCount = 0;

            result = BuildLlpcSetMapping(
                visibility,
                setIndex,
                pSetLayout,
                pDynNodes,
                &dynNodeCount,
                pStaNodes,
                &staNodeCount,
                pDescValues,
                &descRangeCount,
                m_info.userDataLayout.setBindingRegBase + pSetUserData->dynDescDataRegOffset);

            // Increase the number of mapping nodes used by the number of static section nodes added.
            mappingNodeCount += staNodeCount;

            // Increase the number of user data nodes used by the number of dynamic section nodes added.
            userDataNodeCount += dynNodeCount;

            // Increase the number of descriptor range value nodes used by immutable samplers
            descriptorRangeCount += descRangeCount;

            // Add a top-level user data node entry for this set's pointer if there are static nodes.
            if (pSetUserData->setPtrRegOffset != InvalidReg)
            {
                auto pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = m_info.userDataLayout.setBindingRegBase +
                                                       pSetUserData->setPtrRegOffset;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pStaNodes;
                pSetPtrNode->visibility              = visibility;
                userDataNodeCount++;
            }
        }
    }

    if ((result == VK_SUCCESS) && (pVbInfo != nullptr))
    {
        // Build the internal vertex buffer table mapping
        constexpr uint32_t VbTablePtrRegCount = 1; // PAL requires all indirect user data tables to be 1DW

        if ((m_info.userDataRegCount + VbTablePtrRegCount) <=
            m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
        {
            VK_ASSERT(pVbInfo != nullptr);

            // Build the table description itself
            const uint32_t srdDwSize = m_pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);
            uint32_t vbTableSize = pVbInfo->bindingInfo.bindingTableSize * srdDwSize;

            // Add the set pointer node pointing to this table
            auto pVbTblPtrNode = &pUserDataNodes[userDataNodeCount];

            pVbTblPtrNode->node.type                     = Vkgc::ResourceMappingNodeType::IndirectUserDataVaPtr;
            pVbTblPtrNode->node.offsetInDwords           = m_info.userDataRegCount;
            pVbTblPtrNode->node.sizeInDwords             = VbTablePtrRegCount;
            pVbTblPtrNode->node.userDataPtr.sizeInDwords = vbTableSize;
            pVbTblPtrNode->visibility                    = Vkgc::ShaderStageVertexBit;

            userDataNodeCount += 1;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }

        if (appendFetchShaderCb)
        {
            // Append node for uber fetch shader constant buffer
            constexpr uint32_t FetchShaderCbRegCount = 2;
            if ((userDataNodeCount + FetchShaderCbRegCount) <=
                m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
            {
                auto pFetchShaderCbNode = &pUserDataNodes[userDataNodeCount];
                pFetchShaderCbNode->node.type             = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
                pFetchShaderCbNode->node.offsetInDwords   = m_info.userDataRegCount + VbTablePtrRegCount;
                pFetchShaderCbNode->node.sizeInDwords     = FetchShaderCbRegCount;
                pFetchShaderCbNode->node.srdRange.set     = Vkgc::InternalDescriptorSetId;
                pFetchShaderCbNode->node.srdRange.binding = Vkgc::FetchShaderInternalBufferBinding;
                pFetchShaderCbNode->visibility = Vkgc::ShaderStageVertexBit;

                pVbInfo->uberFetchShaderBuffer.userDataOffset = pFetchShaderCbNode->node.offsetInDwords;
                userDataNodeCount += 1;
            }
            else
            {
                VK_NEVER_CALLED();
                result = VK_ERROR_INITIALIZATION_FAILED;
            }

        }
    }

    // If you hit these assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(userDataNodeCount <= m_pipelineInfo.numUserDataNodes);
    VK_ASSERT(mappingNodeCount <= m_pipelineInfo.numRsrcMapNodes);
    VK_ASSERT(descriptorRangeCount <= m_pipelineInfo.numDescRangeValueNodes);

    pResourceMapping->pUserDataNodes             = pUserDataNodes;
    pResourceMapping->userDataNodeCount          = userDataNodeCount;
    pResourceMapping->pStaticDescriptorValues    = pDescriptorRangeValues;
    pResourceMapping->staticDescriptorValueCount = descriptorRangeCount;

    return VK_SUCCESS;
}

// =====================================================================================================================
// Destroy pipeline layout object
VkResult PipelineLayout::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    for (uint32_t i = 0; i < m_info.setCount; ++i)
    {
        GetSetLayouts(i)->Destroy(pDevice, pAllocator, false);
    }

    this->~PipelineLayout();

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout,
    const VkAllocationCallbacks*                pAllocator)
{
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        PipelineLayout::ObjectFromHandle(pipelineLayout)->Destroy(pDevice, pAllocCB);
    }
}
} // namespace entry

} // namespace vk
