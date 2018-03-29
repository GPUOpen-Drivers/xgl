/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "llpc.h"

#include "include/vert_buf_binding_mgr.h"

namespace vk
{

// =====================================================================================================================
PipelineLayout::PipelineLayout(
    const Device*       pDevice,
    const Info&         info,
    const PipelineInfo& pipelineInfo)
    :
    m_info(info),
    m_pipelineInfo(pipelineInfo),
    m_pDevice(pDevice)
{

}

// =====================================================================================================================
VkResult PipelineLayout::ConvertCreateInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo)
{
    // We currently allocate user data registers for various resources in the following fashion:
    // First user data registers will hold the descriptor set bindings in increasing order by set index.
    // For each descriptor set binding we first store the dynamic descriptor data (if there's dynamic section data)
    // followed by the set pointer (if there's static section data).
    // Push constants follow the descriptor set binding data.
    // Finally, the vertex buffer table pointer is in the last user data register when applicable.
    // This allocation allows the descriptor set bindings to easily persist across pipeline switches.

    VkResult result = VK_SUCCESS;

    if ((pIn->setLayoutCount > 0) && (pIn->pSetLayouts == nullptr))
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pPipelineInfo->numRsrcMapNodes        = 0;
    pPipelineInfo->numDescRangeValueNodes = 0;

    // Always allocates:
    // 1 extra user data node for the vertex buffer table pointer
    // 1 extra user data node for push constant
    pPipelineInfo->numUserDataNodes   = 2;
    pInfo->setCount = pIn->setLayoutCount;

    pInfo->userDataRegCount      = 0;

    pInfo->userDataLayout.setBindingRegCount = 0;
    pInfo->userDataLayout.setBindingRegBase  = 0;
    pInfo->userDataLayout.pushConstRegBase   = 0;
    pInfo->userDataLayout.pushConstRegCount  = 0;

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

    pInfo->userDataLayout.pushConstRegCount = pushConstRegCount;
    pInfo->userDataRegCount                += pushConstRegCount;

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);

    // Total number of dynamic descriptors across all descriptor sets
    uint32_t totalDynDescCount = 0;

    // Populate user data layouts for each descriptor set that is active
    pInfo->userDataLayout.setBindingRegBase = pInfo->userDataRegCount;

    for (uint32_t i = 0; i < pInfo->setCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pInfo->setUserData[i];

        // Initialize the set layout info
        const auto& setLayoutInfo = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i])->Info();

        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescDataRegCount  = 0;
        pSetUserData->dynDescCount         = setLayoutInfo.numDynamicDescriptors;
        pSetUserData->firstRegOffset       = pInfo->userDataRegCount - pInfo->userDataLayout.setBindingRegBase;
        pSetUserData->totalRegCount        = 0;

        // Test if this set is active in at least one stage
        if (setLayoutInfo.activeStageMask != 0)
        {
            // Accumulate the space needed by all resource nodes for this set
            pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;

            // Accumulate the space needed by all fmask resource nodes for this set
            pPipelineInfo->numRsrcMapNodes += setLayoutInfo.fmask.numRsrcMapNodes;

            // Add space for the user data node entries needed for dynamic descriptors
            pPipelineInfo->numUserDataNodes += setLayoutInfo.dyn.numRsrcMapNodes + 1;

            // Add space for immutable sampler descriptor storage needed by the set
            pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

            // Reserve user data register space for dynamic descriptor data
            pSetUserData->dynDescDataRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
            pSetUserData->dynDescDataRegCount  = pSetUserData->dynDescCount * DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);
            pSetUserData->totalRegCount += pSetUserData->dynDescDataRegCount;

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
    pPipelineInfo->numRsrcMapNodes += MaxVertexBuffers;

    // Add the user data nodes count to the total number of resource mapping nodes
    pPipelineInfo->numRsrcMapNodes += pPipelineInfo->numUserDataNodes;

    pPipelineInfo->tempStageSize = (pPipelineInfo->numRsrcMapNodes * sizeof(Llpc::ResourceMappingNode));

    pPipelineInfo->tempStageSize += (pPipelineInfo->numDescRangeValueNodes * sizeof(Llpc::DescriptorRangeValue));

    // Calculate scratch buffer size for building pipeline mappings for all shader stages based on this layout
    pPipelineInfo->tempBufferSize = (ShaderStageCount * pPipelineInfo->tempStageSize);

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
    Info info = {};
    PipelineInfo pipelineInfo = {};

    size_t setLayoutsOffset[MaxDescriptorSets];
    size_t setLayoutsArraySize = 0;

    memset(setLayoutsOffset, 0, MaxDescriptorSets * sizeof(size_t));

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
    {
        DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);

        setLayoutsOffset[i] = setLayoutsArraySize;
        setLayoutsArraySize += pLayout->GetObjectSize();
    }

    VkResult result = ConvertCreateInfo(
        pDevice,
        pCreateInfo,
        &info,
        &pipelineInfo);

    if (result != VK_SUCCESS)
    {
        return result;
    }

    // Need to add extra storage for descriptor set layouts
    const size_t apiSize = sizeof(PipelineLayout);
    const size_t objSize = apiSize + setLayoutsArraySize;

    void* pSysMem = pDevice->AllocApiObject(objSize, pAllocator);

    if (pSysMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
    {
        info.pSetLayouts[i] = reinterpret_cast<DescriptorSetLayout*>(Util::VoidPtrInc(pSysMem, apiSize + setLayoutsOffset[i]));

        const DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);

        // Copy the original descriptor set layout object
        pLayout->Copy(pDevice, info.pSetLayouts[i]);
    }

    VK_PLACEMENT_NEW(pSysMem) PipelineLayout(pDevice, info, pipelineInfo);

    *pPipelineLayout = PipelineLayout::HandleFromVoidPointer(pSysMem);

    return result;
}

// =====================================================================================================================
// Translates VkDescriptorType to LLPC ResourceMappingNodeType
Llpc::ResourceMappingNodeType PipelineLayout::MapLlpcResourceNodeType(
    VkDescriptorType descriptorType)
{
    auto nodeType = Llpc::ResourceMappingNodeType::Unknown;
    switch(descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorSampler;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorCombinedTexture;
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorResource;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        nodeType = Llpc::ResourceMappingNodeType::DescriptorResource;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }
    return nodeType;
}

// =====================================================================================================================
// Builds the LLPC resource mapping nodes for a descriptor set
VkResult PipelineLayout::BuildLlpcSetMapping(
    ShaderStage                  stage,
    uint32_t                     setIndex,
    const DescriptorSetLayout*   pLayout,
    Llpc::ResourceMappingNode*   pStaNodes,
    uint32_t*                    pStaNodeCount,
    Llpc::ResourceMappingNode*   pDynNodes,
    uint32_t*                    pDynNodeCount,
    Llpc::DescriptorRangeValue*  pDescriptorRangeValue,
    uint32_t*                    pDescriptorRangeCount,
    uint32_t                     userDataRegBase
    ) const
{
    *pStaNodeCount = 0;
    *pDynNodeCount = 0;
    *pDescriptorRangeCount = 0;

    for (uint32_t bindingIndex = 0; bindingIndex < pLayout->Info().count; ++bindingIndex)
    {
        auto binding = pLayout->Binding(bindingIndex);

        // If the binding has a static section then add a static section node for it.
        if (binding.sta.dwSize > 0)
        {
            auto pNode = &pStaNodes[*pStaNodeCount];

            pNode->type                = MapLlpcResourceNodeType(binding.info.descriptorType);
            pNode->offsetInDwords      = binding.sta.dwOffset;
            pNode->sizeInDwords        = binding.sta.dwSize;
            pNode->srdRange.binding    = binding.info.binding;
            pNode->srdRange.set        = setIndex;
            (*pStaNodeCount)++;

            if (m_pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (binding.fmask.dwSize > 0))
            {
                // If the binding has a fmask section then add a fmask static section node for it.
                pNode++;
                pNode->type             = Llpc::ResourceMappingNodeType::DescriptorFmask;
                pNode->offsetInDwords   = pLayout->Info().sta.dwSize + binding.fmask.dwOffset;
                pNode->sizeInDwords     = binding.fmask.dwSize;
                pNode->srdRange.binding = binding.info.binding;
                pNode->srdRange.set     = setIndex;
                (*pStaNodeCount)++;
            }

            if (binding.imm.dwSize > 0)
            {
                const uint32_t  arraySize             = binding.imm.dwSize / binding.imm.dwArrayStride;
                const uint32_t* pImmutableSamplerData = pLayout->Info().imm.pImmutableSamplerData +
                                                        binding.imm.dwOffset;

                pDescriptorRangeValue->type      = Llpc::ResourceMappingNodeType::DescriptorSampler;
                pDescriptorRangeValue->set       = setIndex;
                pDescriptorRangeValue->binding   = binding.info.binding;
                pDescriptorRangeValue->pValue    = pImmutableSamplerData;
                pDescriptorRangeValue->arraySize = arraySize;
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
            pNode->type                = (binding.dyn.dwArrayStride == 2) ?
                                         Llpc::ResourceMappingNodeType::DescriptorBufferCompact :
                                         Llpc::ResourceMappingNodeType::DescriptorBuffer;
            pNode->offsetInDwords      = userDataRegBase + binding.dyn.dwOffset;
            pNode->sizeInDwords        = binding.dyn.dwSize;
            pNode->srdRange.binding    = binding.info.binding;
            pNode->srdRange.set        = setIndex;
            (*pDynNodeCount)++;
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Builds the description of the internal descriptor set used to represent the VB table for SC.  Returns the number
// of ResourceMappingNodes consumed by this function.  This function does not add the node that describes the top-level
// pointer to this set.
int32_t PipelineLayout::BuildLlpcVertexInputDescriptors(
    const VkPipelineVertexInputStateCreateInfo* pInput,
    VbBindingInfo*                              pVbInfo) const
{
    VK_ASSERT(pVbInfo != nullptr);

    const uint32_t srdDwSize = m_pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);
    uint32_t activeBindings = 0;

    // Sort the strides by binding slot
    uint32_t strideByBindingSlot[MaxVertexBuffers] = {};

    for (uint32_t recordIndex = 0; recordIndex < pInput->vertexBindingDescriptionCount; ++recordIndex)
    {
        const VkVertexInputBindingDescription& record = pInput->pVertexBindingDescriptions[recordIndex];

        strideByBindingSlot[record.binding] = record.stride;
    }

    // Build the description of the VB table by inserting all of the active binding slots into it
    pVbInfo->bindingCount     = 0;
    pVbInfo->bindingTableSize = 0;
    // Find the set of active vertex buffer bindings by figuring out which vertex attributes are consumed by the
    // pipeline.
    //
    // (Note that this ignores inputs eliminated by whole program optimization, but considering that we have not yet
    // compiled the shader and have not performed whole program optimization, this is the best we can do; it's a
    // chicken-egg problem).

    for (uint32_t aindex = 0; aindex < pInput->vertexAttributeDescriptionCount; ++aindex)
    {
        const VkVertexInputAttributeDescription& attrib = pInput->pVertexAttributeDescriptions[aindex];

        VK_ASSERT(attrib.binding < MaxVertexBuffers);

        bool isNotActiveBinding = ((1 << attrib.binding) & activeBindings) == 0;

        if (isNotActiveBinding)
        {
            // Write out the meta information that the VB binding manager needs from pipelines
            auto* pOutBinding = &pVbInfo->bindings[pVbInfo->bindingCount++];
            activeBindings |= (1 << attrib.binding);

            pOutBinding->slot = attrib.binding;
            pOutBinding->byteStride = strideByBindingSlot[attrib.binding];

            pVbInfo->bindingTableSize = Util::Max(pVbInfo->bindingTableSize, attrib.binding + 1);
        }
    }

    return srdDwSize *pVbInfo->bindingTableSize;
}

// =====================================================================================================================
// This function populates the resource mapping node details to the shader-stage specific pipeline info structure.
VkResult PipelineLayout::BuildLlpcPipelineMapping(
    ShaderStage                                 stage,
    void*                                       pBuffer,
    const VkPipelineVertexInputStateCreateInfo* pVertexInput,
    Llpc::PipelineShaderInfo*                   pShaderInfo,
    VbBindingInfo*                              pVbInfo
    ) const
{
    VkResult result = VK_SUCCESS;
    // Vertex binding information should only be specified for the VS stage
    VK_ASSERT(stage == ShaderStageVertex || (pVertexInput == nullptr && pVbInfo == nullptr));

    // Buffer of all resource mapping nodes (enough to cover requirements by all sets in this pipeline layout)
    VK_ASSERT((stage + 1) * m_pipelineInfo.tempStageSize <= m_pipelineInfo.tempBufferSize);

    void* pStageBuffer = Util::VoidPtrInc(pBuffer, stage * m_pipelineInfo.tempStageSize);

    Llpc::ResourceMappingNode* pUserDataNodes = reinterpret_cast<Llpc::ResourceMappingNode*>(pStageBuffer);

    Llpc::ResourceMappingNode* pAllNodes = pUserDataNodes + m_pipelineInfo.numUserDataNodes;
    Llpc::DescriptorRangeValue* pDescriptorRangeValues =
        reinterpret_cast<Llpc::DescriptorRangeValue*>(pUserDataNodes + m_pipelineInfo.numRsrcMapNodes);
    uint32_t descriptorRangeCount = 0;

    uint32_t mappingNodeCount  = 0; // Number of consumed ResourceMappingNodes
    uint32_t userDataNodeCount = 0; // Number of consumed user data ResourceMappingNodes entries

    // TODO: Build the internal push constant resource mapping
    if (result == VK_SUCCESS)
    {
        if (m_info.userDataLayout.pushConstRegCount > 0)
        {
            Llpc::ResourceMappingNode* pPushConstNode = &pUserDataNodes[userDataNodeCount];
            pPushConstNode->type             = Llpc::ResourceMappingNodeType::PushConst;
            pPushConstNode->offsetInDwords   = m_info.userDataLayout.pushConstRegBase;
            pPushConstNode->sizeInDwords     = m_info.userDataLayout.pushConstRegCount;

            userDataNodeCount += 1;
        }
    }
    // Build descriptor for each set
    for (uint32_t setIndex = 0; (setIndex < m_info.setCount) && (result == VK_SUCCESS); ++setIndex)
    {
        const SetUserDataLayout* pSetUserData = &m_info.setUserData[setIndex];
        const auto* pSetLayout = m_info.pSetLayouts[setIndex];

        // Test if this descriptor set is active in this stage.
        if (Util::TestAnyFlagSet(pSetLayout->Info().activeStageMask, (1UL << stage)))
        {
            // Build the resource mapping nodes for the contents of this set.
            auto pStaNodes   = &pAllNodes[mappingNodeCount];
            auto pDynNodes   = &pUserDataNodes[userDataNodeCount];
            Llpc::DescriptorRangeValue* pDescValues = &pDescriptorRangeValues[descriptorRangeCount];

            uint32_t descRangeCount;
            uint32_t staNodeCount;
            uint32_t dynNodeCount;

            result = BuildLlpcSetMapping(
                stage,
                setIndex,
                pSetLayout,
                pStaNodes,
                &staNodeCount,
                pDynNodes,
                &dynNodeCount,
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

                pSetPtrNode->type               = Llpc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->offsetInDwords     = m_info.userDataLayout.setBindingRegBase +
                    pSetUserData->setPtrRegOffset;
                pSetPtrNode->sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->tablePtr.pNext     = pStaNodes;
                userDataNodeCount++;
            }
        }
    }

    // Build the internal vertex buffer table mapping
    constexpr uint32_t VbTablePtrRegCount = 1; // PAL requires all indirect user data tables to be 1DW

    if ((result == VK_SUCCESS) && (pVertexInput != nullptr))
    {
        if ((m_info.userDataRegCount + VbTablePtrRegCount) <=
            m_pDevice->VkPhysicalDevice()->PalProperties().gfxipProperties.maxUserDataEntries)
        {
            VK_ASSERT(pVbInfo != nullptr);

            // Build the table description itself
            auto vbTableSize = BuildLlpcVertexInputDescriptors(pVertexInput,
                                                               pVbInfo);

            // Add the set pointer node pointing to this table
            auto pVbTblPtrNode = &pUserDataNodes[userDataNodeCount];

            pVbTblPtrNode->type           = Llpc::ResourceMappingNodeType::IndirectUserDataVaPtr;
            pVbTblPtrNode->offsetInDwords = m_info.userDataRegCount;
            pVbTblPtrNode->sizeInDwords   = VbTablePtrRegCount;
            pVbTblPtrNode->userDataPtr.sizeInDwords = vbTableSize;

            userDataNodeCount += 1;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    pShaderInfo->pUserDataNodes   = pUserDataNodes;
    pShaderInfo->userDataNodeCount = userDataNodeCount;
    pShaderInfo->pDescriptorRangeValues    = pDescriptorRangeValues;
    pShaderInfo->descriptorRangeValueCount = descriptorRangeCount;

    // If you hit this assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(mappingNodeCount <= m_pipelineInfo.numRsrcMapNodes);

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
        m_info.pSetLayouts[i]->Destroy(pDevice, pAllocator, false);
    }

    this->~PipelineLayout();

    pAllocator->pfnFree(pAllocator->pUserData, this);

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
