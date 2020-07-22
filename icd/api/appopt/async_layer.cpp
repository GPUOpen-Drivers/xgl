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
* @file  async_layer.cpp
* @brief Implementation of async compiler layer.
***********************************************************************************************************************
*/
#include "async_layer.h"
#include "async_shader_module.h"
#include "async_partial_pipeline.h"

#include "include/vk_device.h"
#include "include/vk_shader.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_compute_pipeline.h"
#include "palListImpl.h"

namespace vk
{

namespace entry
{

namespace async
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkShaderModule*                             pShaderModule)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();
    return vk::async::ShaderModule::Create(pDevice, pCreateInfo, pAllocCB, pShaderModule);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(
    VkDevice                                    device,
    VkShaderModule                              shaderModule,
    const VkAllocationCallbacks*                pAllocator)
{
    if (shaderModule != VK_NULL_HANDLE)
    {
        Device* pDevice  = ApiDevice::ObjectFromHandle(device);
        AsyncLayer* pAsyncLayer = pDevice->GetAsyncLayer();
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        pAsyncLayer->SyncAll();
        vk::async::ShaderModule::ObjectFromHandle(shaderModule)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice                            device,
    VkPipelineCache                     pipelineCache,
    uint32_t                            createInfoCount,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipelines)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    AsyncLayer* pAsyncLayer = pDevice->GetAsyncLayer();
    VkResult result = VK_SUCCESS;

    for (uint32_t i = 0; (i < createInfoCount) && (result == VK_SUCCESS); ++i)
    {
        VkGraphicsPipelineCreateInfo createInfo = pCreateInfos[i];
        VkPipelineShaderStageCreateInfo stages[ShaderStage::ShaderStageGfxCount];
        VK_ASSERT(createInfo.stageCount <= ShaderStage::ShaderStageGfxCount);
        for (uint32_t stage = 0; stage < createInfo.stageCount; ++stage)
        {
            stages[stage] = createInfo.pStages[stage];
            vk::async::ShaderModule* pModule = vk::async::ShaderModule::ObjectFromHandle(stages[stage].module);
            stages[stage].module = pModule->GetNextLayerModule();
        }
        createInfo.pStages = stages;
        result = ASYNC_CALL_NEXT_LAYER(vkCreateGraphicsPipelines)(device,
                                                                  pipelineCache,
                                                                  1,
                                                                  &createInfo,
                                                                  pAllocator,
                                                                  pPipelines + i);
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    AsyncLayer* pAsyncLayer = pDevice->GetAsyncLayer();
    VkResult result = VK_SUCCESS;

    for (uint32_t i = 0; (i < createInfoCount) && (result == VK_SUCCESS); ++i)
    {
        VkComputePipelineCreateInfo createInfo = pCreateInfos[i];
        VK_ASSERT(createInfo.stage.module != VK_NULL_HANDLE);
        vk::async::ShaderModule* pModule = vk::async::ShaderModule::ObjectFromHandle(createInfo.stage.module);
        createInfo.stage.module = pModule->GetNextLayerModule();
        result = ASYNC_CALL_NEXT_LAYER(vkCreateComputePipelines)(device,
                                                                 pipelineCache,
                                                                 1,
                                                                 &createInfo,
                                                                 pAllocator,
                                                                 pPipelines + i);
    }

    return result;
}

} // namespace async

} // namespace entry

// =====================================================================================================================
AsyncLayer::AsyncLayer(Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pModuleTaskThreads(),
    m_pPipelineTaskThreads()
{
    Util::SystemInfo sysInfo = {};
    Util::QuerySystemInfo(&sysInfo);

    for (uint32_t i = 0; i < MaxTaskType; ++i)
    {
        m_taskId[i] = 0;
        m_activeThreadCount[i] = Util::Min(MaxThreads, sysInfo.cpuLogicalCoreCount / 2);
    }
    for (uint32_t i = 0; i < m_activeThreadCount[0]; ++i)
    {
        m_pModuleTaskThreads[i] = VK_PLACEMENT_NEW(m_moduleTaskThreadBuffer[i])
                                  async::TaskThread<ShaderModuleTask>(this, pDevice->VkInstance()->Allocator());
        m_pModuleTaskThreads[i]->Begin();

        m_pPipelineTaskThreads[i] = VK_PLACEMENT_NEW(m_pipelineTaskThreadBuffer[i])
                                    async::TaskThread<PartialPipelineTask>(this, pDevice->VkInstance()->Allocator());
        m_pPipelineTaskThreads[i]->Begin();
    }
}

// =====================================================================================================================
AsyncLayer::~AsyncLayer()
{
    for (uint32_t i = 0; i < m_activeThreadCount[0]; ++i)
    {
        m_pModuleTaskThreads[i]->SetStop();
        m_pModuleTaskThreads[i]->Join();
        Util::Destructor(m_pModuleTaskThreads[i]);
        m_pModuleTaskThreads[i] = nullptr;

        m_pPipelineTaskThreads[i]->SetStop();
        m_pPipelineTaskThreads[i]->Join();
        Util::Destructor(m_pPipelineTaskThreads[i]);
        m_pPipelineTaskThreads[i] = nullptr;
    }
}

// =====================================================================================================================
void AsyncLayer::SyncAll()
{
    for (uint32_t i = 0; i < m_activeThreadCount[0]; ++i)
    {
        m_pModuleTaskThreads[i]->SyncAll();
        m_pPipelineTaskThreads[i]->SyncAll();
    }
}

// =====================================================================================================================
void AsyncLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    ASYNC_OVERRIDE_ENTRY(vkCreateShaderModule);
    ASYNC_OVERRIDE_ENTRY(vkDestroyShaderModule);
    ASYNC_OVERRIDE_ENTRY(vkCreateGraphicsPipelines);
    ASYNC_OVERRIDE_ENTRY(vkCreateComputePipelines);
}

} // namespace vk
