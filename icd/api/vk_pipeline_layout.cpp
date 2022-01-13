/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/graphics_pipeline_common.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_pipeline_layout.h"
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
    VkResult                   result                         = VK_SUCCESS;
    uint32_t                   pushConstantsSizeInBytes       = 0;
    uint32_t                   pushConstantsUserDataNodeCount = 0;
    const PipelineLayoutScheme scheme                         = DeterminePipelineLayoutScheme(pDevice, pIn);

    pInfo->setCount = pIn->setLayoutCount;

    ProcessPushConstantsInfo(
        pIn,
        &pushConstantsSizeInBytes,
        &pushConstantsUserDataNodeCount);

    if (scheme == PipelineLayoutScheme::Indirect)
    {
        result = BuildIndirectSchemeInfo(
            pDevice,
            pIn,
            pushConstantsSizeInBytes,
            pInfo,
            pPipelineInfo,
            pSetUserDataLayouts);
    }
    else if (scheme == PipelineLayoutScheme::Compact)
    {
        result = BuildCompactSchemeInfo(
            pDevice,
            pIn,
            pushConstantsSizeInBytes,
            pushConstantsUserDataNodeCount,
            pInfo,
            pPipelineInfo,
            pSetUserDataLayouts);
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return result;
}

// =====================================================================================================================
void PipelineLayout::ProcessPushConstantsInfo(
    const VkPipelineLayoutCreateInfo* pIn,
    uint32_t*                         pPushConstantsSizeInBytes,
    uint32_t*                         pPushConstantsUserDataNodeCount)
{
    uint32_t pushConstantsSizeInBytes       = 0;
    uint32_t pushConstantsUserDataNodeCount = 1;

    for (uint32_t i = 0; i < pIn->pushConstantRangeCount; ++i)
    {
        const VkPushConstantRange* pRange = &pIn->pPushConstantRanges[i];

        // Test if this push constant range is active in at least one stage
        if (pRange->stageFlags != 0)
        {
            pushConstantsSizeInBytes = Util::Max(pushConstantsSizeInBytes, pRange->offset + pRange->size);
        }
    }

    *pPushConstantsSizeInBytes       = pushConstantsSizeInBytes;
    *pPushConstantsUserDataNodeCount = pushConstantsUserDataNodeCount;
}

// =====================================================================================================================
VkResult PipelineLayout::BuildCompactSchemeInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    const uint32_t                    pushConstantsSizeInBytes,
    const uint32_t                    pushConstantsUserDataNodeCount,
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

    VkResult      result          = VK_SUCCESS;
    auto*         pUserDataLayout = &pInfo->userDataLayout.compact;

    memset(pPipelineInfo,            0, sizeof(PipelineInfo));
    memset(&(pInfo->userDataLayout), 0, sizeof(UserDataLayout));
    pInfo->userDataLayout.scheme = PipelineLayoutScheme::Compact;

    // Always allocates 1 extra user data node for the vertex buffer table pointer
    pPipelineInfo->numUserDataNodes = 1;

    if (pDevice->GetRuntimeSettings().enableEarlyCompile)
    {
        // Early compile mode will enable uber-fetch shader and spec constant buffer on vertex shader and
        // fragment shader implicitly. so we need three reserved node.
        pPipelineInfo->numUserDataNodes += 3;
        pInfo->userDataRegCount         += 6; // Each buffer consume 2 user data register now.
    }
    else if (pDevice->GetRuntimeSettings().enableUberFetchShader)
    {
        // Reserve one user data nodes for uber-fetch shader.
        pPipelineInfo->numUserDataNodes += 1;
        pInfo->userDataRegCount         += 2;
    }

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);

    // Total number of dynamic descriptors across all descriptor sets
    uint32_t totalDynDescCount = 0;

    // Populate user data layouts for each descriptor set that is active
    pUserDataLayout->setBindingRegBase = pInfo->userDataRegCount;

    for (uint32_t i = 0; i < pIn->setLayoutCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pSetUserDataLayouts[i];

        // Initialize the set layout info
        const auto& setLayoutInfo = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i])->Info();

        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescCount         = setLayoutInfo.numDynamicDescriptors;
        pSetUserData->firstRegOffset       = pInfo->userDataRegCount - pUserDataLayout->setBindingRegBase;
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
                pSetUserData->totalRegCount  += SetPtrRegCount;
            }
        }

        // Add the number of user data regs used by this set to the total count for the whole layout
        pInfo->userDataRegCount += pSetUserData->totalRegCount;
        if (pDevice->GetRuntimeSettings().pipelineLayoutMode == PipelineLayoutAngle)
        {
            // Force next set firstRegOffset align to AngleDescPattern.
            if ((i + 1) < Util::ArrayLen(AngleDescPattern::DescriptorSetOffset))
            {
                if (pInfo->userDataRegCount < AngleDescPattern::DescriptorSetOffset[i + 1])
                {
                    pInfo->userDataRegCount = AngleDescPattern::DescriptorSetOffset[i + 1];
                }
            }
        }
    }

    // Calculate total number of user data regs used for active descriptor set data
    pUserDataLayout->setBindingRegCount = pInfo->userDataRegCount - pUserDataLayout->setBindingRegBase;

    VK_ASSERT(totalDynDescCount <= MaxDynamicDescriptors);

    // Allocate user data for push constants
    pPipelineInfo->numUserDataNodes += pushConstantsUserDataNodeCount;

    uint32_t pushConstRegCount = pushConstantsSizeInBytes / sizeof(uint32_t);

    pUserDataLayout->pushConstRegBase  = pInfo->userDataRegCount;
    pUserDataLayout->pushConstRegCount = pushConstRegCount;
    pInfo->userDataRegCount           += pushConstRegCount;

    // Reserve an user-data to store the VA of buffer for transform feedback.
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        pUserDataLayout->transformFeedbackRegBase  = pInfo->userDataRegCount;
        pUserDataLayout->transformFeedbackRegCount = 1;
        pInfo->userDataRegCount                   += pUserDataLayout->transformFeedbackRegCount;
        pPipelineInfo->numUserDataNodes           += 1;
    }

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
VkResult PipelineLayout::BuildIndirectSchemeInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    const uint32_t                    pushConstantsSizeInBytes,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo,
    SetUserDataLayout*                pSetUserDataLayouts)
{
    // Indirect mode is designed for the case that the pipeline layout only contains part of layout information of the
    // final executable pipeline. So that user data is used in a conservative way to make sure that this pipeline layout
    // is always compatible with other layouts which may contain the descriptor which is not known currently.
    //
    // The user data registers for various resources is allocated in the following fashion:
    // 1. one user data entry for the vertex buffer table pointer
    // 2. one user data entry for the push constant buffer pointer
    // 3. one user data entry for transform feedback buffer (if extension is enabled)
    // 5. MaxDescriptorSets sets of user data entries which store the information for each descriptor set. Each set
    //    contains 2 user data entry: the 1st is for the dynamic descriptors and the 2nd is for static descriptors.
    //
    // TODO: The following features have not been supported by indirect scheme:
    //       1. Uber-fetch shader
    //       2. PipelineLayoutAngle mode

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);
    VK_ASSERT(pDevice->GetRuntimeSettings().pipelineLayoutMode != PipelineLayoutAngle);
    VK_ASSERT(pDevice->GetRuntimeSettings().enableEarlyCompile == false);

    VkResult      result            = VK_SUCCESS;
    auto*         pUserDataLayout   = &pInfo->userDataLayout.indirect;
    uint32_t      totalDynDescCount = 0;

    memset(pPipelineInfo,            0, sizeof(PipelineInfo));
    memset(&(pInfo->userDataLayout), 0, sizeof(UserDataLayout));
    pInfo->userDataLayout.scheme = PipelineLayoutScheme::Indirect;

    VK_ASSERT(totalDynDescCount <= MaxDynamicDescriptors);

    // Allocate user data for vertex buffer table
    pPipelineInfo->numUserDataNodes += 1;
    pPipelineInfo->numRsrcMapNodes  += Pal::MaxVertexBuffers;
    pInfo->userDataRegCount         += 1;

    // Allocate user data for push constant buffer pointer
    pUserDataLayout->pushConstPtrRegBase  = pInfo->userDataRegCount;
    pUserDataLayout->pushConstSizeInDword = pushConstantsSizeInBytes / sizeof(uint32_t);
    pPipelineInfo->numUserDataNodes      += 1;
    pPipelineInfo->numRsrcMapNodes       += 1;
    pInfo->userDataRegCount              += 1;

    // Allocate user data for transform feedback buffer
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        pUserDataLayout->transformFeedbackRegBase = pInfo->userDataRegCount;
        pPipelineInfo->numUserDataNodes          += 1;
        pInfo->userDataRegCount                  += 1;
    }

    // Allocate user data for descriptor sets
    pUserDataLayout->setBindingPtrRegBase = pInfo->userDataRegCount;
    pInfo->userDataRegCount              += 2 * SetPtrRegCount * MaxDescriptorSets;

    // This simulate the descriptor set user data register layout of compact scheme
    // so as to fill pSetUserDataLayouts[]
    // Indirect scheme also need to fill pSetUserDataLayouts[] because we need the data
    // in this array to locate the descriptor set data managed by vk::CmdBuffer
    uint32_t setBindingCompactRegBase = pUserDataLayout->setBindingPtrRegBase;

    for (uint32_t i = 0; i < pIn->setLayoutCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pSetUserDataLayouts[i];

        const DescriptorSetLayout::CreateInfo& setLayoutInfo =
            DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i])->Info();

        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescCount         = setLayoutInfo.numDynamicDescriptors;
        pSetUserData->firstRegOffset       = setBindingCompactRegBase - pUserDataLayout->setBindingPtrRegBase;
        pSetUserData->totalRegCount        = 0;

        if (setLayoutInfo.activeStageMask != 0)
        {
            // Add space for static descriptors
            if (setLayoutInfo.sta.numRsrcMapNodes > 0)
            {
                pPipelineInfo->numUserDataNodes += 1;
                pPipelineInfo->numRsrcMapNodes  += setLayoutInfo.sta.numRsrcMapNodes;

                // Add count for FMASK nodes
                if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
                {
                    pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;
                }
            }

            // Add space for immutable sampler descriptor storage needed by the set
            pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

            // Add space for dynamic descriptors
            if (setLayoutInfo.dyn.numRsrcMapNodes > 0)
            {
                pPipelineInfo->numUserDataNodes += 1;
                pPipelineInfo->numRsrcMapNodes  += setLayoutInfo.dyn.numRsrcMapNodes;
                totalDynDescCount               += setLayoutInfo.numDynamicDescriptors;
            }

            // Fill set user data layout
            pSetUserData->dynDescDataRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
            pSetUserData->totalRegCount +=
                pSetUserData->dynDescCount * DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);
            if (setLayoutInfo.sta.numRsrcMapNodes > 0)
            {
                pSetUserData->setPtrRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
                pSetUserData->totalRegCount += SetPtrRegCount;
            }
        }

        setBindingCompactRegBase += pSetUserData->totalRegCount;
    }

    // Calculate the buffer size necessary for all resource mapping
    pPipelineInfo->mappingBufferSize =
        (pPipelineInfo->numUserDataNodes       * GetMaxResMappingRootNodeSize()) +
        (pPipelineInfo->numRsrcMapNodes        * GetMaxResMappingNodeSize()    ) +
        (pPipelineInfo->numDescRangeValueNodes * GetMaxStaticDescValueSize()   );

    // If we go past our user data limit, we can't support this pipeline
    if (pInfo->userDataRegCount >=
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
PipelineLayoutScheme PipelineLayout::DeterminePipelineLayoutScheme(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn)
{
    PipelineLayoutScheme scheme = PipelineLayoutScheme::Compact;

    const RuntimeSettings&     settings     = pDevice->GetRuntimeSettings();

    switch (settings.pipelineLayoutSchemeSelectionStrategy)
    {
    case AppControlled:
        {
            scheme = PipelineLayoutScheme::Compact;
        }
        break;
    case ForceCompact:
        scheme = PipelineLayoutScheme::Compact;
        break;
    case ForceIndirect:
        scheme = PipelineLayoutScheme::Indirect;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return scheme;
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
                                    layoutInfo.userDataLayout.compact.pushConstRegCount * sizeof(uint32_t));

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
        nodeType = Vkgc::ResourceMappingNodeType::InlineBuffer;
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
            pNode->srdRange.set         = setIndex;
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
                pNode->node.type = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact:
                    Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
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
// Builds the VKGC resource mapping nodes for the static descriptors in a descriptor set
void PipelineLayout::BuildLlpcStaticSetMapping(
    const DescriptorSetLayout*   pLayout,
    const uint32_t               visibility,
    const uint32_t               setIndex,
    Vkgc::ResourceMappingNode*   pNodes,
    uint32_t*                    pNodeCount,
    Vkgc::StaticDescriptorValue* pDescriptorRangeValue,
    uint32_t*                    pDescriptorRangeCount
    ) const
{
    *pNodeCount            = 0;
    *pDescriptorRangeCount = 0;

    for (uint32_t bindingIndex = 0; bindingIndex < pLayout->Info().count; ++bindingIndex)
    {
        const DescriptorSetLayout::BindingInfo& binding = pLayout->Binding(bindingIndex);

        if (binding.sta.dwSize > 0)
        {
            Vkgc::ResourceMappingNode* pNode = pNodes + *pNodeCount;

            pNode->type             = MapLlpcResourceNodeType(binding.info.descriptorType);
            pNode->offsetInDwords   = binding.sta.dwOffset;
            pNode->sizeInDwords     = binding.sta.dwSize;
            pNode->srdRange.binding = binding.info.binding;
            pNode->srdRange.set     = setIndex;
            (*pNodeCount)++;

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
    }
}

// =====================================================================================================================
// Fill a root resource mapping node for a dynamic descriptor node
template <>
void PipelineLayout::FillDynamicSetNode(
    const Vkgc::ResourceMappingNodeType type,
    const uint32_t                      visibility,
    const uint32_t                      setIndex,
    const uint32_t                      bindingIndex,
    const uint32_t                      offsetInDwords,
    const uint32_t                      sizeInDwords,
    const uint32_t                      userDataRegBase,
    Vkgc::ResourceMappingRootNode*      pNode
    ) const
{
    pNode->node.type             = type;
    pNode->node.offsetInDwords   = userDataRegBase + offsetInDwords;
    pNode->node.sizeInDwords     = sizeInDwords;
    pNode->node.srdRange.binding = bindingIndex;
    pNode->node.srdRange.set     = setIndex;
    pNode->visibility            = visibility;
}

// =====================================================================================================================
// Fill a normal resource mapping node for a dynamic descriptor node
template <>
void PipelineLayout::FillDynamicSetNode(
    const Vkgc::ResourceMappingNodeType type,
    const uint32_t                      visibility,
    const uint32_t                      setIndex,
    const uint32_t                      bindingIndex,
    const uint32_t                      offsetInDwords,
    const uint32_t                      sizeInDwords,
    const uint32_t                      userDataRegBase,
    Vkgc::ResourceMappingNode*          pNode
    ) const
{
    pNode->type             = type;
    pNode->offsetInDwords   = offsetInDwords;
    pNode->sizeInDwords     = sizeInDwords;
    pNode->srdRange.binding = bindingIndex;
    pNode->srdRange.set     = setIndex;
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for the dynamic descriptors in a descriptor set
template <typename NodeType>
void PipelineLayout::BuildLlpcDynamicSetMapping(
    const DescriptorSetLayout* pLayout,
    const uint32_t             visibility,
    const uint32_t             setIndex,
    const uint32_t             userDataRegBase,
    NodeType*                  pNodes,
    uint32_t*                  pNodeCount
    ) const
{
    static_assert(std::is_same<NodeType, Vkgc::ResourceMappingRootNode>::value ||
                  std::is_same<NodeType, Vkgc::ResourceMappingNode>::value,
                  "Unexpected resouce mapping node type!");

    *pNodeCount = 0;

    for (uint32_t bindingIndex = 0; bindingIndex < pLayout->Info().count; ++bindingIndex)
    {
        const DescriptorSetLayout::BindingInfo& binding = pLayout->Binding(bindingIndex);

        if (binding.dyn.dwSize > 0)
        {
            VK_ASSERT((binding.info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                      (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC));

            Vkgc::ResourceMappingNodeType nodeType = Vkgc::ResourceMappingNodeType::Unknown;
            if (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                nodeType = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorBufferCompact :
                    Vkgc::ResourceMappingNodeType::DescriptorBuffer;

            }
            else
            {
                nodeType = (binding.dyn.dwArrayStride == 2) ?
                    Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact :
                    Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
            }

            FillDynamicSetNode(
                nodeType,
                visibility,
                setIndex,
                binding.info.binding,
                binding.dyn.dwOffset,
                binding.dyn.dwSize,
                userDataRegBase,
                pNodes + *pNodeCount);

            (*pNodeCount)++;
        }
    }
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for vertex buffer table
void PipelineLayout::BuildLlpcVertexBufferTableMapping(
    const VbBindingInfo*           pVbInfo,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pNode,
    uint32_t*                      pNodeCount
    ) const
{
    *pNodeCount = 0;

    if (pVbInfo != nullptr)
    {
        // Build the table description itself
        const uint32_t srdDwSize = m_pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);
        const uint32_t vbTableSize = pVbInfo->bindingTableSize * srdDwSize;

        // Add the set pointer node pointing to this table
        pNode->node.type                     = Vkgc::ResourceMappingNodeType::IndirectUserDataVaPtr;
        pNode->node.offsetInDwords           = offsetInDwords;
        pNode->node.sizeInDwords             = sizeInDwords;
        pNode->node.userDataPtr.sizeInDwords = vbTableSize;
        pNode->visibility                    = Vkgc::ShaderStageVertexBit;

        *pNodeCount = 1;
    }
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for transform feedback buffer
void PipelineLayout::BuildLlpcTransformFeedbackMapping(
    const uint32_t                 stageMask,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pNode,
    uint32_t*                      pNodeCount
    ) const
{
    uint32_t xfbStages         = (stageMask & (Vkgc::ShaderStageFragmentBit - 1)) >> 1;
    uint32_t lastXfbStageBit   = Vkgc::ShaderStageVertexBit;

    *pNodeCount = 0;

    while (xfbStages > 0)
    {
        lastXfbStageBit <<= 1;
        xfbStages >>= 1;
    }

    if (lastXfbStageBit != 0)
    {
        pNode->node.type           = Vkgc::ResourceMappingNodeType::StreamOutTableVaPtr;
        pNode->node.offsetInDwords = offsetInDwords;
        pNode->node.sizeInDwords   = sizeInDwords;
        pNode->visibility          = lastXfbStageBit;

        *pNodeCount = 1;
    }
}

// =====================================================================================================================
// Populates the resouce mapping nodes in compact scheme
VkResult PipelineLayout::BuildCompactSchemeLlpcPipelineMapping(
    const uint32_t             stageMask,
    const VbBindingInfo*       pVbInfo,
    const bool                 appendFetchShaderCb,
    void*                      pBuffer,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VK_ASSERT(m_info.userDataLayout.scheme == PipelineLayoutScheme::Compact);

    VkResult            result         = VK_SUCCESS;
    const auto&         userDataLayout = m_info.userDataLayout.compact;

    Vkgc::ResourceMappingRootNode* pUserDataNodes = static_cast<Vkgc::ResourceMappingRootNode*>(pBuffer);
    Vkgc::ResourceMappingNode* pResourceNodes =
        reinterpret_cast<Vkgc::ResourceMappingNode*>(pUserDataNodes + m_pipelineInfo.numUserDataNodes);
    Vkgc::StaticDescriptorValue* pDescriptorRangeValues =
        reinterpret_cast<Vkgc::StaticDescriptorValue*>(pResourceNodes + m_pipelineInfo.numRsrcMapNodes);

    uint32_t userDataNodeCount    = 0; // Number of consumed ResourceMappingRootNodes
    uint32_t mappingNodeCount     = 0; // Number of consumed ResourceMappingNodes (only sub-nodes)
    uint32_t descriptorRangeCount = 0; // Number of consumed StaticResourceValues

    constexpr uint32_t InternalCbRegCount = 2;

    if (appendFetchShaderCb && pVbInfo != nullptr)
    {
        // Append node for uber fetch shader constant buffer
        auto pFetchShaderCbNode = &pUserDataNodes[userDataNodeCount];
        pFetchShaderCbNode->node.type             = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
        pFetchShaderCbNode->node.offsetInDwords   = FetchShaderInternalBufferOffset;
        pFetchShaderCbNode->node.sizeInDwords     = InternalCbRegCount;
        pFetchShaderCbNode->node.srdRange.set     = Vkgc::InternalDescriptorSetId;
        pFetchShaderCbNode->node.srdRange.binding = Vkgc::FetchShaderInternalBufferBinding;
        pFetchShaderCbNode->visibility            = Vkgc::ShaderStageVertexBit;

        userDataNodeCount += 1;
    }

    if (m_pDevice->GetRuntimeSettings().enableEarlyCompile)
    {
        if (stageMask & Vkgc::ShaderStageVertexBit)
        {
            auto pSpecConstVertexCbNode = &pUserDataNodes[userDataNodeCount];
            pSpecConstVertexCbNode->node.type             = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
            pSpecConstVertexCbNode->node.offsetInDwords   = SpecConstBufferVertexOffset;
            pSpecConstVertexCbNode->node.sizeInDwords     = InternalCbRegCount;
            pSpecConstVertexCbNode->node.srdRange.set     = Vkgc::InternalDescriptorSetId;
            pSpecConstVertexCbNode->node.srdRange.binding = SpecConstVertexInternalBufferBindingId;
            pSpecConstVertexCbNode->visibility            = Vkgc::ShaderStageVertexBit;

            userDataNodeCount += 1;
        }

        if (stageMask & Vkgc::ShaderStageFragmentBit)
        {
            auto pSpecConstVertexCbNode = &pUserDataNodes[userDataNodeCount];
            pSpecConstVertexCbNode->node.type             = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
            pSpecConstVertexCbNode->node.offsetInDwords   = SpecConstBufferFragmentOffset;
            pSpecConstVertexCbNode->node.sizeInDwords     = InternalCbRegCount;
            pSpecConstVertexCbNode->node.srdRange.set     = Vkgc::InternalDescriptorSetId;
            pSpecConstVertexCbNode->node.srdRange.binding = SpecConstFragmentInternalBufferBindingId;
            pSpecConstVertexCbNode->visibility            = Vkgc::ShaderStageVertexBit;

            userDataNodeCount += 1;
        }
    }

    // Build descriptor for each set
    for (uint32_t setIndex = 0; setIndex < m_info.setCount; ++setIndex)
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

            BuildLlpcDynamicSetMapping(
                pSetLayout,
                visibility,
                setIndex,
                userDataLayout.setBindingRegBase + pSetUserData->dynDescDataRegOffset,
                pDynNodes,
                &dynNodeCount);

            BuildLlpcStaticSetMapping(
                pSetLayout,
                visibility,
                setIndex,
                pStaNodes,
                &staNodeCount,
                pDescValues,
                &descRangeCount);

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
                pSetPtrNode->node.offsetInDwords     = userDataLayout.setBindingRegBase +
                                                       pSetUserData->setPtrRegOffset;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pStaNodes;
                pSetPtrNode->visibility              = visibility;
                userDataNodeCount++;
            }
        }
    }

    // TODO: Build the internal push constant resource mapping
        if (userDataLayout.pushConstRegCount > 0)
        {
            auto pPushConstNode = &pUserDataNodes[userDataNodeCount];
            pPushConstNode->node.type = Vkgc::ResourceMappingNodeType::PushConst;
            pPushConstNode->node.offsetInDwords = userDataLayout.pushConstRegBase;
            pPushConstNode->node.sizeInDwords = userDataLayout.pushConstRegCount;
            pPushConstNode->node.srdRange.set = Vkgc::InternalDescriptorSetId;
            pPushConstNode->visibility = stageMask;

            userDataNodeCount += 1;
        }

    if (userDataLayout.transformFeedbackRegCount > 0)
    {
        uint32_t nodeCount;

        BuildLlpcTransformFeedbackMapping(
            stageMask,
            userDataLayout.transformFeedbackRegBase,
            userDataLayout.transformFeedbackRegCount,
            &pUserDataNodes[userDataNodeCount],
            &nodeCount);

        userDataNodeCount += nodeCount;
    }

    if (pVbInfo != nullptr)
    {
        // Build the internal vertex buffer table mapping
        constexpr uint32_t VbTablePtrRegCount = 1; // PAL requires all indirect user data tables to be 1DW

        if ((m_info.userDataRegCount + VbTablePtrRegCount) <=
            m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
        {
            uint32_t nodeCount;

            BuildLlpcVertexBufferTableMapping(
                pVbInfo, m_info.userDataRegCount, VbTablePtrRegCount, &pUserDataNodes[userDataNodeCount], &nodeCount);

            userDataNodeCount += nodeCount;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    // If you hit these assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(userDataNodeCount    <= m_pipelineInfo.numUserDataNodes);
    VK_ASSERT(mappingNodeCount     <= m_pipelineInfo.numRsrcMapNodes);
    VK_ASSERT(descriptorRangeCount <= m_pipelineInfo.numDescRangeValueNodes);

    pResourceMapping->pUserDataNodes             = pUserDataNodes;
    pResourceMapping->userDataNodeCount          = userDataNodeCount;
    pResourceMapping->pStaticDescriptorValues    = pDescriptorRangeValues;
    pResourceMapping->staticDescriptorValueCount = descriptorRangeCount;

    return result;
}

// =====================================================================================================================
// Populates the resouce mapping nodes in indirect scheme
void PipelineLayout::BuildIndirectSchemeLlpcPipelineMapping(
    const uint32_t             stageMask,
    const VbBindingInfo*       pVbInfo,
    void*                      pBuffer,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VK_ASSERT(m_info.userDataLayout.scheme == PipelineLayoutScheme::Indirect);

    constexpr uint32_t VbTablePtrRegCount         = 1; // PAL requires all indirect user data tables to be 1DW
    constexpr uint32_t PushConstPtrRegCount       = 1;
    constexpr uint32_t TransformFeedbackRegCount  = 1;
    constexpr uint32_t DescSetsPtrRegCount        = 2 * SetPtrRegCount * MaxDescriptorSets;

    const bool transformFeedbackEnabled =
        m_pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK);

    const uint32_t vbTablePtrRegBase        = 0;
    const uint32_t pushConstPtrRegBase      = vbTablePtrRegBase + VbTablePtrRegCount;
    const uint32_t transformFeedbackRegBase =
        (transformFeedbackEnabled == false) ? InvalidReg : (pushConstPtrRegBase + PushConstPtrRegCount);
    const uint32_t setBindingPtrRegBase =
        transformFeedbackEnabled ? (transformFeedbackRegBase + TransformFeedbackRegCount) :
                                   (pushConstPtrRegBase + PushConstPtrRegCount);

    const auto& userDataLayout = m_info.userDataLayout.indirect;

    Vkgc::ResourceMappingRootNode* pUserDataNodes = static_cast<Vkgc::ResourceMappingRootNode*>(pBuffer);
    Vkgc::ResourceMappingNode* pResourceNodes =
        reinterpret_cast<Vkgc::ResourceMappingNode*>(pUserDataNodes + m_pipelineInfo.numUserDataNodes);
    Vkgc::StaticDescriptorValue* pDescriptorRangeValues =
        reinterpret_cast<Vkgc::StaticDescriptorValue*>(pResourceNodes + m_pipelineInfo.numRsrcMapNodes);

    uint32_t userDataNodeCount    = 0; // Number of consumed ResourceMappingRootNodes
    uint32_t mappingNodeCount     = 0; // Number of consumed ResourceMappingNodes (only sub-nodes)
    uint32_t descriptorRangeCount = 0; // Number of consumed StaticResourceValues

    // Build the internal vertex buffer table mapping
    if (pVbInfo != nullptr)
    {
        uint32_t nodeCount;

        BuildLlpcVertexBufferTableMapping(
            pVbInfo, vbTablePtrRegBase, VbTablePtrRegCount, &pUserDataNodes[userDataNodeCount], &nodeCount);

        userDataNodeCount += nodeCount;
    }

    // Build push constants mapping
    if (userDataLayout.pushConstSizeInDword > 0)
    {
        // Build mapping for push constant resouce
        Vkgc::ResourceMappingNode* pPushConstNode = &pResourceNodes[mappingNodeCount];

        pPushConstNode->type           = Vkgc::ResourceMappingNodeType::PushConst;
        pPushConstNode->offsetInDwords = 0;
        pPushConstNode->sizeInDwords   = userDataLayout.pushConstSizeInDword;
        pPushConstNode->srdRange.set   = Vkgc::InternalDescriptorSetId;

        ++mappingNodeCount;

        // Build mapping for the pointer pointing to push constants buffer
        Vkgc::ResourceMappingRootNode* pPushConstPtrNode = &pUserDataNodes[userDataNodeCount];

        pPushConstPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
        pPushConstPtrNode->node.offsetInDwords     = pushConstPtrRegBase;
        pPushConstPtrNode->node.sizeInDwords       = PushConstPtrRegCount;
        pPushConstPtrNode->node.tablePtr.nodeCount = 1;
        pPushConstPtrNode->node.tablePtr.pNext     = pPushConstNode;
        pPushConstPtrNode->visibility              = stageMask;

        userDataNodeCount += 1;
    }

    // Build transform feedback buffer mapping
    if (transformFeedbackEnabled)
    {
        uint32_t nodeCount;

        BuildLlpcTransformFeedbackMapping(
            stageMask,
            transformFeedbackRegBase,
            TransformFeedbackRegCount,
            &pUserDataNodes[userDataNodeCount],
            &nodeCount);

        userDataNodeCount += nodeCount;
    }

    // Build mapping for each set of descriptors
    VK_ASSERT(setBindingPtrRegBase == userDataLayout.setBindingPtrRegBase);

    for (uint32_t setIndex = 0; setIndex < m_info.setCount; ++setIndex)
    {
        const DescriptorSetLayout* pSetLayout = GetSetLayouts(setIndex);

        const uint32_t visibility = stageMask & VkToVkgcShaderStageMask(pSetLayout->Info().activeStageMask);

        if (visibility != 0)
        {
            uint32_t dynNodeCount   = 0;
            uint32_t staNodeCount   = 0;
            uint32_t descRangeCount = 0;

            Vkgc::ResourceMappingNode* pDynNodes = &pResourceNodes[mappingNodeCount];
            BuildLlpcDynamicSetMapping(
                pSetLayout, visibility, setIndex, 0, pDynNodes, &dynNodeCount);

            Vkgc::ResourceMappingNode*   pStaNodes   = &pResourceNodes[mappingNodeCount + dynNodeCount];
            Vkgc::StaticDescriptorValue* pDescValues = &pDescriptorRangeValues[descriptorRangeCount];
            BuildLlpcStaticSetMapping(
                pSetLayout, visibility, setIndex, pStaNodes, &staNodeCount, pDescValues, &descRangeCount);

            // Increase the number of mapping nodes used by the number of static section nodes added.
            mappingNodeCount += (dynNodeCount + staNodeCount);

            // Increase the number of descriptor range value nodes used by immutable samplers
            descriptorRangeCount += descRangeCount;

            // Add a top-level user data node entry for dynamic nodes.
            if (pSetLayout->Info().dyn.numRsrcMapNodes > 0)
            {
                Vkgc::ResourceMappingRootNode* pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = 2 * setIndex * SetPtrRegCount + setBindingPtrRegBase;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = dynNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pDynNodes;
                pSetPtrNode->visibility              = visibility;

                ++userDataNodeCount;
            }

            // Add a top-level user data node entry for static nodes.
            if (pSetLayout->Info().sta.numRsrcMapNodes > 0)
            {
                Vkgc::ResourceMappingRootNode* pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = (2 * setIndex + 1) * SetPtrRegCount + setBindingPtrRegBase;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pStaNodes;
                pSetPtrNode->visibility              = visibility;

                ++userDataNodeCount;
            }
        }
    }

    // If you hit these assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(userDataNodeCount    <= m_pipelineInfo.numUserDataNodes);
    VK_ASSERT(mappingNodeCount     <= m_pipelineInfo.numRsrcMapNodes);
    VK_ASSERT(descriptorRangeCount <= m_pipelineInfo.numDescRangeValueNodes);

    pResourceMapping->pUserDataNodes             = pUserDataNodes;
    pResourceMapping->userDataNodeCount          = userDataNodeCount;
    pResourceMapping->pStaticDescriptorValues    = pDescriptorRangeValues;
    pResourceMapping->staticDescriptorValueCount = descriptorRangeCount;
}

// =====================================================================================================================
// This function populates the resource mapping node details to the shader-stage specific pipeline info structure.
VkResult PipelineLayout::BuildLlpcPipelineMapping(
    const uint32_t             stageMask,
    const VbBindingInfo*       pVbInfo,
    const bool                 appendFetchShaderCb,
    void*                      pBuffer,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VkResult result = VK_SUCCESS;

    if (m_info.userDataLayout.scheme == PipelineLayoutScheme::Compact)
    {
        result = BuildCompactSchemeLlpcPipelineMapping(
            stageMask, pVbInfo, appendFetchShaderCb, pBuffer, pResourceMapping);
    }
    else if (m_info.userDataLayout.scheme == PipelineLayoutScheme::Indirect)
    {
        BuildIndirectSchemeLlpcPipelineMapping(
            stageMask, pVbInfo, pBuffer, pResourceMapping);
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return result;
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
