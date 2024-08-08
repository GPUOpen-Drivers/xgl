/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_indirect_commands_layout.cpp
 * @brief Contains implementation of Vulkan indirect commands layout objects.
 ***********************************************************************************************************************
 */

#include "include/vk_indirect_commands_layout.h"
#include "include/vk_buffer.h"
#include "include/vk_conv.h"

namespace vk
{
// =====================================================================================================================
// Creates an indirect commands layout object.
VkResult IndirectCommandsLayoutNV::Create(
    Device*                                         pDevice,
    const VkIndirectCommandsLayoutCreateInfoNV*     pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkIndirectCommandsLayoutNV*                     pLayout)
{
    VkResult result = VK_SUCCESS;
    Pal::Result palResult;

    IndirectCommandsLayoutNV* pObject = nullptr;

    Pal::IndirectCmdGeneratorCreateInfo createInfo = {};
    Pal::IndirectParam indirectParams[MaxIndirectTokenCount] = {};
    createInfo.pParams = &indirectParams[0];

    Pal::IIndirectCmdGenerator* pPalGenerator[MaxPalDevices] = {};

    const size_t apiSize = sizeof(IndirectCommandsLayoutNV);
    size_t totalSize     = apiSize;
    size_t palSize       = 0;

    void* pMemory = nullptr;

    IndirectCommandsInfo info = {};

    VK_ASSERT(pCreateInfo->streamCount == 1);
    VK_ASSERT(pCreateInfo->tokenCount > 0);
    VK_ASSERT(pCreateInfo->tokenCount <= MaxIndirectTokenCount);

    if (pCreateInfo->tokenCount == 1)
    {
        VK_NOT_IMPLEMENTED;
    }

    const VkIndirectCommandsLayoutTokenNV lastToken = pCreateInfo->pTokens[pCreateInfo->tokenCount - 1];

    switch (lastToken.tokenType)
    {
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_NV:
        info.actionType = IndirectCommandsActionType::Draw;
        break;

    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV:
        info.actionType = IndirectCommandsActionType::DrawIndexed;
        break;

    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV:
        info.actionType = IndirectCommandsActionType::Dispatch;
        break;

    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV:
        info.actionType = IndirectCommandsActionType::MeshTask;
        break;

    default:
        VK_ALERT_ALWAYS_MSG("Indirect tokens can only end up with one type of actions.");
        result = VK_ERROR_UNKNOWN;
        break;
    }

    if (result == VK_SUCCESS)
    {
        BuildPalCreateInfo(pDevice, pCreateInfo, &indirectParams[0], &createInfo);

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            const size_t size = pDevice->PalDevice(deviceIdx)->GetIndirectCmdGeneratorSize(createInfo,
                                                                                           &palResult);
            if (palResult == Pal::Result::Success)
            {
                palSize += size;
            }
            else
            {
                result = PalToVkResult(palResult);
                break;
            }
        }

        totalSize += palSize;
    }

    if (result == VK_SUCCESS)
    {
        pMemory = pDevice->AllocApiObject(pAllocator, totalSize);
        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS)
    {
        void* pPalMemory = Util::VoidPtrInc(pMemory, apiSize);

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            const size_t size = pDevice->PalDevice(deviceIdx)->GetIndirectCmdGeneratorSize(createInfo,
                                                                                           &palResult);

            if (palResult == Pal::Result::Success)
            {
                palResult = pDevice->PalDevice(deviceIdx)->CreateIndirectCmdGenerator(createInfo,
                                                                                      pPalMemory,
                                                                                      &pPalGenerator[deviceIdx]);
            }

            if (palResult == Pal::Result::Success)
            {
                pPalMemory = Util::VoidPtrInc(pPalMemory, size);
            }
            else
            {
                result = PalToVkResult(palResult);
                break;
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        pObject = VK_PLACEMENT_NEW(pMemory) IndirectCommandsLayoutNV(
            pDevice,
            info,
            pPalGenerator,
            createInfo);

        result = pObject->Initialize(pDevice);
    }

    if (result == VK_SUCCESS)
    {
        *pLayout = IndirectCommandsLayoutNV::HandleFromVoidPointer(pMemory);
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            if (pPalGenerator[deviceIdx] != nullptr)
            {
                pPalGenerator[deviceIdx]->Destroy();
            }
        }

        Util::Destructor(pObject);

        pDevice->FreeApiObject(pAllocator, pMemory);
    }

    return result;
}

// =====================================================================================================================
IndirectCommandsLayoutNV::IndirectCommandsLayoutNV(
    const Device*                                   pDevice,
    const IndirectCommandsInfo&                     info,
    Pal::IIndirectCmdGenerator**                    ppPalGenerator,
    const Pal::IndirectCmdGeneratorCreateInfo&      palCreateInfo)
    :
    m_info(info),
    m_palCreateInfo(palCreateInfo),
    m_internalMem()
{
    memcpy(m_pPalGenerator, ppPalGenerator, sizeof(m_pPalGenerator));
}

// =====================================================================================================================
VkResult IndirectCommandsLayoutNV::Initialize(
    Device*                                         pDevice)
{
    VkResult result = VK_SUCCESS;

    constexpr bool ReadOnly            = false;
    constexpr bool RemoveInvisibleHeap = true;
    constexpr bool PersistentMapped    = false;

    // Allocate and bind GPU memory for the object
    result = pDevice->MemMgr()->AllocAndBindGpuMem(
        pDevice->NumPalDevices(),
        reinterpret_cast<Pal::IGpuMemoryBindable**>(&m_pPalGenerator),
        ReadOnly,
        &m_internalMem,
        pDevice->GetPalDeviceMask(),
        RemoveInvisibleHeap,
        PersistentMapped,
        VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV,
        IndirectCommandsLayoutNV::IntValueFromHandle(IndirectCommandsLayoutNV::HandleFromObject(this)));

    return result;
}

// =====================================================================================================================
void IndirectCommandsLayoutNV::BuildPalCreateInfo(
    const Device*                                   pDevice,
    const VkIndirectCommandsLayoutCreateInfoNV*     pCreateInfo,
    Pal::IndirectParam*                             pIndirectParams,
    Pal::IndirectCmdGeneratorCreateInfo*            pPalCreateInfo)
{
    uint32_t paramCount = 0;
    uint32_t expectedOffset = 0;
    uint32_t bindingArgsSize = 0;

    bool useNativeIndexType = true;

    const bool isDispatch = (pCreateInfo->pTokens[pCreateInfo->tokenCount - 1].tokenType
                                == VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV);

    for (uint32_t i = 0; i < pCreateInfo->tokenCount; ++i)
    {
        const VkIndirectCommandsLayoutTokenNV& token = pCreateInfo->pTokens[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 889
        // Set a padding operation to handle non tightly packed indirect arguments buffers
        VK_ASSERT(token.offset >= expectedOffset);
        if (token.offset > expectedOffset)
        {
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::Padding;
            pIndirectParams[paramCount].sizeInBytes = token.offset - expectedOffset;

            bindingArgsSize += pIndirectParams[paramCount].sizeInBytes;
            paramCount++;
        }
#endif

        switch (token.tokenType)
        {
        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_NV:
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::Draw;
            pIndirectParams[paramCount].sizeInBytes = sizeof(Pal::DrawIndirectArgs);
            static_assert(sizeof(Pal::DrawIndirectArgs) == sizeof(VkDrawIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV:
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::DrawIndexed;
            pIndirectParams[paramCount].sizeInBytes = sizeof(Pal::DrawIndexedIndirectArgs);
            static_assert(sizeof(Pal::DrawIndexedIndirectArgs) == sizeof(VkDrawIndexedIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV:
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::Dispatch;
            pIndirectParams[paramCount].sizeInBytes = sizeof(Pal::DispatchIndirectArgs);
            static_assert(sizeof(Pal::DispatchIndirectArgs) == sizeof(VkDispatchIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV:
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::BindIndexData;
            pIndirectParams[paramCount].sizeInBytes = sizeof(Pal::BindIndexDataIndirectArgs);
            useNativeIndexType = (token.indexTypeCount == 0);
            static_assert(sizeof(Pal::BindIndexDataIndirectArgs) == sizeof(VkBindIndexBufferIndirectCommandNV));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV:
            pIndirectParams[paramCount].type                = Pal::IndirectParamType::BindVertexData;
            pIndirectParams[paramCount].sizeInBytes         = sizeof(Pal::BindVertexDataIndirectArgs);
            pIndirectParams[paramCount].vertexData.bufferId = token.vertexBindingUnit;
            pIndirectParams[paramCount].userDataShaderUsage = Pal::ApiShaderStageVertex;
            static_assert(sizeof(Pal::BindVertexDataIndirectArgs) == sizeof(VkBindVertexBufferIndirectCommandNV));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV:
            pIndirectParams[paramCount].type        = Pal::IndirectParamType::DispatchMesh;
            pIndirectParams[paramCount].sizeInBytes = sizeof(Pal::DispatchMeshIndirectArgs);
            static_assert(sizeof(Pal::DispatchMeshIndirectArgs) == sizeof(VkDrawMeshTasksIndirectCommandEXT));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV:
        {
            const PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(token.pushconstantPipelineLayout);
            const UserDataLayout& userDataLayout  = pPipelineLayout->GetInfo().userDataLayout;

            uint32_t startInDwords  = token.pushconstantOffset / sizeof(uint32_t);
            uint32_t lengthInDwords = PipelineLayout::GetPushConstantSizeInDword(token.pushconstantSize);

            pIndirectParams[paramCount].type                = Pal::IndirectParamType::SetUserData;
            pIndirectParams[paramCount].userData.entryCount = lengthInDwords;
            pIndirectParams[paramCount].sizeInBytes         = sizeof(uint32_t) * lengthInDwords;
            pIndirectParams[paramCount].userData.firstEntry = userDataLayout.common.pushConstRegBase + startInDwords;
            pIndirectParams[paramCount].userDataShaderUsage = VkToPalShaderStageMask(token.pushconstantShaderStageFlags);
            break;
        }

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV:
        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_STATE_FLAGS_NV:
        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_TASKS_NV:
        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV:
            VK_NOT_IMPLEMENTED;
            break;

        default:
            VK_NEVER_CALLED();
            break;

        }

        // Override userDataShaderUsage to compute shader only for dispatch type
        if (isDispatch)
        {
            pIndirectParams[paramCount].userDataShaderUsage = Pal::ShaderStageFlagBits::ApiShaderStageCompute;
        }

        if (i < (pCreateInfo->tokenCount - 1))
        {
            bindingArgsSize += pIndirectParams[paramCount].sizeInBytes;
        }

        // Calculate expected offset of the next token assuming indirect arguments buffers are tightly packed
        expectedOffset = token.offset + pIndirectParams[paramCount].sizeInBytes;
        paramCount++;
    }

    for (uint32_t i = 0; i < pCreateInfo->streamCount; ++i)
    {
        uint32_t stride = pCreateInfo->pStreamStrides[i];
        pPalCreateInfo->strideInBytes += stride;
    }

    pPalCreateInfo->paramCount = paramCount;

    constexpr uint32_t DxgiIndexTypeUint8  = 62;
    constexpr uint32_t DxgiIndexTypeUint16 = 57;
    constexpr uint32_t DxgiIndexTypeUint32 = 42;

    pPalCreateInfo->indexTypeTokens[0] = useNativeIndexType ? VK_INDEX_TYPE_UINT8_KHR : DxgiIndexTypeUint8;
    pPalCreateInfo->indexTypeTokens[1] = useNativeIndexType ? VK_INDEX_TYPE_UINT16    : DxgiIndexTypeUint16;
    pPalCreateInfo->indexTypeTokens[2] = useNativeIndexType ? VK_INDEX_TYPE_UINT32    : DxgiIndexTypeUint32;
}

// =====================================================================================================================
void IndirectCommandsLayoutNV::CalculateMemoryRequirements(
    const Device*                                   pDevice,
    VkMemoryRequirements2*                          pMemoryRequirements
    ) const
{
    // Our CP packet solution have no preprocess step. Gpu memory is not required.
    pMemoryRequirements->memoryRequirements.size            = 0;
    pMemoryRequirements->memoryRequirements.alignment       = 0;
    pMemoryRequirements->memoryRequirements.memoryTypeBits  = 0;

    Pal::GpuMemoryRequirements memReqs = {};
    memReqs.flags.cpuAccess = 0;
    memReqs.heapCount       = 2;
    memReqs.heaps[0]        = Pal::GpuHeap::GpuHeapInvisible;
    memReqs.heaps[1]        = Pal::GpuHeap::GpuHeapLocal;

    for (uint32_t i = 0; i < memReqs.heapCount; ++i)
    {
        uint32_t typeIndexBits;

        if (pDevice->GetVkTypeIndexBitsFromPalHeap(memReqs.heaps[i], &typeIndexBits))
        {
            pMemoryRequirements->memoryRequirements.memoryTypeBits |= typeIndexBits;
        }
    }
}

// =====================================================================================================================
VkResult IndirectCommandsLayoutNV::Destroy(
    Device*                                         pDevice,
    const VkAllocationCallbacks*                    pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalGenerator[deviceIdx] != nullptr)
        {
            m_pPalGenerator[deviceIdx]->Destroy();
        }
    }

    pDevice->MemMgr()->FreeGpuMem(&m_internalMem);

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

} // namespace vk
