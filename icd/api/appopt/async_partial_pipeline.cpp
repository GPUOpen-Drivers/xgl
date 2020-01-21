/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  async_partial_pipeline.cpp
* @brief Implementation of class async::PartialPipeline
***********************************************************************************************************************
*/
#include "async_layer.h"
#include "async_partial_pipeline.h"

#include "include/vk_device.h"
#include "include/vk_shader.h"
#include "palListImpl.h"

namespace vk
{

namespace async
{
// =====================================================================================================================
PartialPipeline::PartialPipeline(
    const VkAllocationCallbacks*    pAllocator)
    :
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
// Creates async partial pipeline object
PartialPipeline* PartialPipeline::Create(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    const size_t objSize = sizeof(PartialPipeline);
    void* pMemory = pDevice->AllocApiObject(objSize, pAllocator);

    if (pMemory == nullptr)
    {
        return nullptr;
    }

    VK_PLACEMENT_NEW(pMemory) PartialPipeline(pAllocator);

    return static_cast<PartialPipeline*>(pMemory);
}

// =====================================================================================================================
// Destory async partial pipeline object
VkResult PartialPipeline::Destroy()
{
    m_pAllocator->pfnFree(m_pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Builds partial pipeline in async mode
void PartialPipeline::AsyncBuildPartialPipeline(
    AsyncLayer* pAsyncLayer,
    VkShaderModule asyncShaderModule)
{
    auto pTaskThread = reinterpret_cast<async::TaskThread<PartialPipelineTask>*>
                       (pAsyncLayer->GetTaskThread(PartialPipelineTaskType));
    if (pTaskThread != nullptr)
    {
        PartialPipelineTask task = {};

        task.shaderModuleHandle = asyncShaderModule;
        task.pObj = this;
        pTaskThread->AddTask(&task);
    }
    else
    {
        Destroy();
    }
}

static const uint32_t OffsetStrideInDwords = 12;
// =====================================================================================================================
// Creat ResourceMappingNode from module data
void PartialPipeline::CreatePipelineLayoutFromModuleData(
    AsyncLayer*                         pAsyncLayer,
    Llpc::ShaderModuleEntryData*        pShaderModuleEntryData,
    const Llpc::ResourceMappingNode**   ppResourceMappingNode,
    uint32_t*                           pMappingNodeCount)
{
    const Llpc::ResourceNodeData* pResourceNodeData = pShaderModuleEntryData->pResNodeDatas;
    uint32_t resNodeDataCount = pShaderModuleEntryData->resNodeDataCount;
    uint32_t pushConstSize = pShaderModuleEntryData->pushConstSize;
    uint32_t setCount = 0;
    uint32_t set = 0;

    if (resNodeDataCount > 0)
    {
        set = pResourceNodeData[0].set;
        setCount = 1;
        for (uint32_t i = 1; i < resNodeDataCount; ++i)
        {
            if (set != pResourceNodeData[i].set)
            {
                set = pResourceNodeData[i].set;
                ++setCount;
            }
        }
    }

    // 1 reperents push constant
    uint32_t totalNodes = pushConstSize != 0 ? resNodeDataCount + setCount + 1 : resNodeDataCount + setCount;

    Device* pDevice = pAsyncLayer->GetDevice();
    auto pSets = static_cast<Llpc::ResourceMappingNode*>(pDevice->AllocApiObject(
            totalNodes * sizeof(Llpc::ResourceMappingNode), m_pAllocator));
    auto pNodes = pSets + setCount + 1;
    uint32_t topLevelOffset = 0;

    for (uint32_t i = 0; i < resNodeDataCount; ++i)
    {
        pNodes[i].type = pResourceNodeData[i].type;
        pNodes[i].sizeInDwords = OffsetStrideInDwords * pResourceNodeData[i].arraySize;
        pNodes[i].offsetInDwords = pResourceNodeData[i].binding * OffsetStrideInDwords;
        pNodes[i].srdRange.set = pResourceNodeData[i].set;
        pNodes[i].srdRange.binding = pResourceNodeData[i].binding;
        if ((i == 0) || (set != pNodes[i].srdRange.set))
        {
            set = pNodes[i].srdRange.set;
            pSets[set].tablePtr.pNext = &pNodes[i];
            pSets[set].type = Llpc::ResourceMappingNodeType::DescriptorTableVaPtr;
            pSets[set].sizeInDwords = 1;
            pSets[set].offsetInDwords = topLevelOffset;
            topLevelOffset += pSets[set].sizeInDwords;
        }
        ++pSets[pResourceNodeData[i].set].tablePtr.nodeCount;
    }

    // Add UseDynamic options for below cases:
    // 1. Force all uniform buffer are dynamic buffer in auto layout pipeline layout
    // 2. Force all storage buffer are dynamic buffer in auto layout pipeline layout

    if (pushConstSize)
    {
        // Add a node for push consts at the end of root descriptor list.
        pSets[resNodeDataCount + setCount].type = Llpc::ResourceMappingNodeType::PushConst;
        pSets[resNodeDataCount + setCount].sizeInDwords = pushConstSize;
        pSets[resNodeDataCount + setCount].offsetInDwords = topLevelOffset;
    }

    *pMappingNodeCount = setCount;
    *ppResourceMappingNode = pSets;
}

// =====================================================================================================================
// Creat color target from module data
void PartialPipeline::CreateColorTargetFromModuleData(
    Llpc::ShaderModuleDataEx* pShaderModuleDataEx,
    Llpc::ColorTarget* pTarget)
{
    for (uint32_t i = 0; i < pShaderModuleDataEx->extra.fsOutInfoCount; ++i)
    {
        uint32_t location = pShaderModuleDataEx->extra.pFsOutInfos[i].location;
        uint32_t componentCount =  pShaderModuleDataEx->extra.pFsOutInfos[i].componentCount;
        Llpc::BasicType basicType = pShaderModuleDataEx->extra.pFsOutInfos[i].basicType;

        VK_ASSERT(location < Llpc::MaxColorTargets);
        pTarget[location].channelWriteMask = (1U << componentCount) - 1;
        // Further optimization is app profile for color format according to fsOutInfos.
        switch (basicType)
        {
        case Llpc::BasicType::Float:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R32_SFLOAT,
                    VK_FORMAT_R32G32_SFLOAT,
                    VK_FORMAT_R32G32B32_SFLOAT,
                    VK_FORMAT_R32G32B32A32_SFLOAT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Double:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R64_SFLOAT,
                    VK_FORMAT_R64G64_SFLOAT,
                    VK_FORMAT_R64G64B64_SFLOAT,
                    VK_FORMAT_R64G64B64A64_SFLOAT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Int:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R32_SINT,
                    VK_FORMAT_R32G32_SINT,
                    VK_FORMAT_R32G32B32_SINT,
                    VK_FORMAT_R32G32B32A32_SINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Uint:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R32_UINT,
                    VK_FORMAT_R32G32_UINT,
                    VK_FORMAT_R32G32B32_UINT,
                    VK_FORMAT_R32G32B32A32_UINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Int64:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R64_SINT,
                    VK_FORMAT_R64G64_SINT,
                    VK_FORMAT_R64G64B64_SINT,
                    VK_FORMAT_R64G64B64A64_SINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Uint64:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R64_UINT,
                    VK_FORMAT_R64G64_UINT,
                    VK_FORMAT_R64G64B64_UINT,
                    VK_FORMAT_R64G64B64A64_UINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Float16:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R16_SFLOAT,
                    VK_FORMAT_R16G16_SFLOAT,
                    VK_FORMAT_R16G16B16_SFLOAT,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Int16:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R16_SINT,
                    VK_FORMAT_R16G16_SINT,
                    VK_FORMAT_R16G16B16_SINT,
                    VK_FORMAT_R16G16B16A16_SINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Uint16:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R16_UINT,
                    VK_FORMAT_R16G16_UINT,
                    VK_FORMAT_R16G16B16_UINT,
                    VK_FORMAT_R16G16B16A16_UINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Int8:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R8_SINT,
                    VK_FORMAT_R8G8_SINT,
                    VK_FORMAT_R8G8B8_SINT,
                    VK_FORMAT_R8G8B8A8_SINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        case Llpc::BasicType::Uint8:
            {
                static const VkFormat formatTable[] =
                {
                    VK_FORMAT_R8_UINT,
                    VK_FORMAT_R8G8_UINT,
                    VK_FORMAT_R8G8B8_UINT,
                    VK_FORMAT_R8G8B8A8_UINT,
                };
                pTarget[location].format = formatTable[componentCount - 1];
                break;
            }
        default:
                break;
        }
    }
}

// =====================================================================================================================
// Creates partial pipeline with partial pipeline opt enabled.
void PartialPipeline::Execute(
    AsyncLayer*      pAsyncLayer,
    PartialPipelineTask* pTask)
{
    Device* pDevice = pAsyncLayer->GetDevice();
    PipelineCompilerType compilerType = pDevice->GetCompiler(0)->GetShaderCacheType();
    if (compilerType != PipelineCompilerTypeLlpc)
    {
        return;
    }

    vk::ShaderModule* pShaderModule = vk::ShaderModule::ObjectFromHandle(pTask->shaderModuleHandle);
    void* pShaderModuleData =  pShaderModule->GetShaderData(compilerType);
    auto pShaderModuleDataEx = reinterpret_cast<Llpc::ShaderModuleDataEx*>(pShaderModuleData);
    Llpc::ShaderModuleEntryData* pShaderModuleEntryData = nullptr;
    Llpc::ColorTarget pColorTarget[Llpc::MaxColorTargets] = {};
    if ((pShaderModuleDataEx->extra.entryCount == 1) &&
        (pShaderModuleDataEx->extra.entryDatas[0].stage == Llpc::ShaderStageCompute))
    {
        pShaderModuleEntryData = &pShaderModuleDataEx->extra.entryDatas[0];
    }
    else
    {
        for (uint32_t i = 0; i < pShaderModuleDataEx->extra.entryCount; ++i)
        {
            if (pShaderModuleDataEx->extra.entryDatas[i].stage == Llpc::ShaderStageFragment)
            {
                CreateColorTargetFromModuleData(pShaderModuleDataEx, pColorTarget);
                if (pColorTarget[0].format == VK_FORMAT_UNDEFINED)
                {
                    break;
                }

                pShaderModuleEntryData = &pShaderModuleDataEx->extra.entryDatas[i];
                break;
            }
        }
    }
    if (pShaderModuleEntryData != nullptr)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            const Llpc::ResourceMappingNode*    pResourceMappingNode = nullptr;
            uint32_t                            mappingNodeCount = 0;
            CreatePipelineLayoutFromModuleData(pAsyncLayer, pShaderModuleEntryData, &pResourceMappingNode, &mappingNodeCount);

            auto result = pDevice->GetCompiler(deviceIdx)->CreatePartialPipelineBinary(deviceIdx,
                pShaderModuleData, pShaderModuleEntryData, pResourceMappingNode, mappingNodeCount, pColorTarget);
            VK_ASSERT(result == VK_SUCCESS);
            m_pAllocator->pfnFree(m_pAllocator->pUserData, (void*)pResourceMappingNode);
        }
    }
    Destroy();
}

} // namespace async

} // namespace vk
