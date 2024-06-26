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
VkResult IndirectCommandsLayout::Create(
    const Device*                                   pDevice,
    const VkIndirectCommandsLayoutCreateInfoNV*     pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkIndirectCommandsLayoutNV*                     pLayout)
{
    VkResult result = VK_SUCCESS;
    Pal::Result palResult;

    Pal::IndirectCmdGeneratorCreateInfo createInfo = {};
    Pal::IndirectParam indirectParams[MaxIndirectTokenCount] = {};
    createInfo.pParams = &indirectParams[0];

    Pal::IIndirectCmdGenerator* pGenerators[MaxPalDevices] = {};
    Pal::IGpuMemory*            pGpuMemory[MaxPalDevices]  = {};

    const size_t apiSize = ObjectSize(pDevice);
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
                                                                                      &pGenerators[deviceIdx]);
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
        result = BindGpuMemory(pDevice, pAllocator, pGenerators, pGpuMemory);
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) IndirectCommandsLayout(
            pDevice,
            info,
            pGenerators,
            pGpuMemory,
            createInfo);

        *pLayout = IndirectCommandsLayout::HandleFromVoidPointer(pMemory);
    }

    return result;
}

// =====================================================================================================================
IndirectCommandsLayout::IndirectCommandsLayout(
    const Device*                                   pDevice,
    const IndirectCommandsInfo&                     info,
    Pal::IIndirectCmdGenerator**                    pGenerators,
    Pal::IGpuMemory**                               pGpuMemory,
    const Pal::IndirectCmdGeneratorCreateInfo&      palCreateInfo)
    :
    m_info(info),
    m_palCreateInfo(palCreateInfo)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        m_perGpu[deviceIdx].pGenerator  = pGenerators[deviceIdx];
        m_perGpu[deviceIdx].pGpuMemory  = pGpuMemory[deviceIdx];
    }
}

// =====================================================================================================================
void IndirectCommandsLayout::BuildPalCreateInfo(
    const Device*                                   pDevice,
    const VkIndirectCommandsLayoutCreateInfoNV*     pCreateInfo,
    Pal::IndirectParam*                             pIndirectParams,
    Pal::IndirectCmdGeneratorCreateInfo*            pPalCreateInfo)
{
    uint32_t bindingArgsSize = 0;

    const bool isDispatch = (pCreateInfo->pTokens[pCreateInfo->tokenCount - 1].tokenType
                                == VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV);

    for (uint32_t i = 0; i < pCreateInfo->tokenCount; ++i)
    {
        const VkIndirectCommandsLayoutTokenNV& token = pCreateInfo->pTokens[i];

        switch (token.tokenType)
        {
        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_NV:
            pIndirectParams[i].type         = Pal::IndirectParamType::Draw;
            pIndirectParams[i].sizeInBytes  = sizeof(Pal::DrawIndirectArgs);
            static_assert(sizeof(Pal::DrawIndirectArgs) == sizeof(VkDrawIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV:
            pIndirectParams[i].type         = Pal::IndirectParamType::DrawIndexed;
            pIndirectParams[i].sizeInBytes  = sizeof(Pal::DrawIndexedIndirectArgs);
            static_assert(sizeof(Pal::DrawIndexedIndirectArgs) == sizeof(VkDrawIndexedIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV:
            pIndirectParams[i].type         = Pal::IndirectParamType::Dispatch;
            pIndirectParams[i].sizeInBytes  = sizeof(Pal::DispatchIndirectArgs);
            static_assert(sizeof(Pal::DispatchIndirectArgs) == sizeof(VkDispatchIndirectCommand));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV:
            pIndirectParams[i].type         = Pal::IndirectParamType::BindIndexData;
            pIndirectParams[i].sizeInBytes  = sizeof(Pal::BindIndexDataIndirectArgs);
            static_assert(sizeof(Pal::BindIndexDataIndirectArgs) == sizeof(VkBindIndexBufferIndirectCommandNV));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV:
            pIndirectParams[i].type                 = Pal::IndirectParamType::BindVertexData;
            pIndirectParams[i].sizeInBytes          = sizeof(Pal::BindVertexDataIndirectArgs);
            pIndirectParams[i].vertexData.bufferId  = token.vertexBindingUnit;
            pIndirectParams[i].userDataShaderUsage  = Pal::ApiShaderStageVertex;
            static_assert(sizeof(Pal::BindVertexDataIndirectArgs) == sizeof(VkBindVertexBufferIndirectCommandNV));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV:
            pIndirectParams[i].type                 = Pal::IndirectParamType::DispatchMesh;
            pIndirectParams[i].sizeInBytes          = sizeof(Pal::DispatchMeshIndirectArgs);
            static_assert(sizeof(Pal::DispatchMeshIndirectArgs) == sizeof(VkDrawMeshTasksIndirectCommandEXT));
            break;

        case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV:
        {
            const PipelineLayout* pPipelineLayout   = PipelineLayout::ObjectFromHandle(token.pushconstantPipelineLayout);
            const UserDataLayout& userDataLayout    = pPipelineLayout->GetInfo().userDataLayout;

            uint32_t startInDwords                  = token.pushconstantOffset / sizeof(uint32_t);
            uint32_t lengthInDwords                 = PipelineLayout::GetPushConstantSizeInDword(token.pushconstantSize);

            pIndirectParams[i].type                 = Pal::IndirectParamType::SetUserData;
            pIndirectParams[i].userData.entryCount  = lengthInDwords;
            pIndirectParams[i].sizeInBytes          = sizeof(uint32_t) * lengthInDwords;
            pIndirectParams[i].userData.firstEntry  = userDataLayout.common.pushConstRegBase + startInDwords;
            pIndirectParams[i].userDataShaderUsage  = VkToPalShaderStageMask(token.pushconstantShaderStageFlags);
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

        if (i < (pCreateInfo->tokenCount - 1))
        {
            bindingArgsSize += pIndirectParams[i].sizeInBytes;
        }
    }

    for (uint32_t i = 0; i < pCreateInfo->streamCount; ++i)
    {
        uint32_t stride = pCreateInfo->pStreamStrides[i];
        pPalCreateInfo->strideInBytes += stride;
    }

    pPalCreateInfo->paramCount = pCreateInfo->tokenCount;

    // Override userDataShaderUsage to compute shader only for dispatch type
    if (isDispatch)
    {
        for (uint32_t i = 0; i < pPalCreateInfo->paramCount; ++i)
        {
            pIndirectParams[i].userDataShaderUsage = Pal::ShaderStageFlagBits::ApiShaderStageCompute;
        }
    }
}

// =====================================================================================================================
void IndirectCommandsLayout::CalculateMemoryRequirements(
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
    memReqs.heaps[0]        = Pal::GpuHeap::GpuHeapInvisible;
    memReqs.heapCount       = 1;

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
VkResult IndirectCommandsLayout::BindGpuMemory(
    const Device*                                   pDevice,
    const VkAllocationCallbacks*                    pAllocator,
    Pal::IIndirectCmdGenerator**                    pGenerators,
    Pal::IGpuMemory**                               pGpuMemory)
{
    VkResult result = VK_SUCCESS;
    Pal::Result palResult;

    Pal::GpuMemoryRequirements  memReqs[MaxPalDevices]        = {};
    Pal::GpuMemoryCreateInfo    memCreateInfos[MaxPalDevices] = {};

    size_t totalSize = 0;

    void* pMemory = nullptr;

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        pGenerators[deviceIdx]->GetGpuMemoryRequirements(&memReqs[deviceIdx]);

        memCreateInfos[deviceIdx].size          = memReqs[deviceIdx].size;
        memCreateInfos[deviceIdx].alignment     = memReqs[deviceIdx].alignment;
        memCreateInfos[deviceIdx].priority      = Pal::GpuMemPriority::Normal;
        memCreateInfos[deviceIdx].heapCount     = memReqs[deviceIdx].heapCount;

        for (uint32 i = 0; i < memReqs[deviceIdx].heapCount; ++i)
        {
            memCreateInfos[deviceIdx].heaps[i] = memReqs[deviceIdx].heaps[i];
        }

        const size_t size = pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(memCreateInfos[deviceIdx],
                                                                            &palResult);

        if (palResult == Pal::Result::Success)
        {
            totalSize += size;
        }
        else
        {
            result = PalToVkResult(palResult);
            break;
        }
    }

    if (result == VK_SUCCESS)
    {
        pMemory = pAllocator->pfnAllocation(pAllocator->pUserData,
                                            totalSize,
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS)
    {
        void* pPalMemory = pMemory;

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            const size_t size = pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(memCreateInfos[deviceIdx],
                                                                                &palResult);

            if (palResult == Pal::Result::Success)
            {
                palResult = pDevice->PalDevice(deviceIdx)->CreateGpuMemory(memCreateInfos[deviceIdx],
                                                                           pPalMemory,
                                                                           &pGpuMemory[deviceIdx]);
            }

            if (palResult == Pal::Result::Success)
            {
                // Gpu memory binding for IndirectCmdGenerator to build SRD containing properties and parameter data.
                palResult = pGenerators[deviceIdx]->BindGpuMemory(pGpuMemory[deviceIdx], 0);
            }
            else
            {
                result = PalToVkResult(palResult);
                break;
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

    return result;
}

// =====================================================================================================================
VkResult IndirectCommandsLayout::Destroy(
    Device*                                         pDevice,
    const VkAllocationCallbacks*                    pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_perGpu[deviceIdx].pGenerator != nullptr)
        {
            m_perGpu[deviceIdx].pGenerator->Destroy();
        }

        if (m_perGpu[deviceIdx].pGpuMemory != nullptr)
        {
            m_perGpu[deviceIdx].pGpuMemory->Destroy();
        }
    }

    if (m_perGpu[DefaultDeviceIndex].pGpuMemory != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, m_perGpu[DefaultDeviceIndex].pGpuMemory);
    }

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

} // namespace vk
