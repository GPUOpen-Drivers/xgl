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
* @file  async_shader_module.cpp
* @brief Implementation of class async::ShaderModule
***********************************************************************************************************************
*/
#include "async_layer.h"
#include "async_shader_module.h"
#include "async_partial_pipeline.h"

#include "include/vk_device.h"
#include "include/vk_shader.h"
#include "palListImpl.h"

namespace vk
{

namespace async
{

// =====================================================================================================================
ShaderModule::ShaderModule(
    VkShaderModule immedModule)
    :
    m_immedModule(immedModule),
    m_asyncModule(VK_NULL_HANDLE)
{
}

// =====================================================================================================================
// Creates async shdaer module object
VkResult ShaderModule::Create(
    Device*                         pDevice,
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkShaderModule*                 pShaderModule)
{
    AsyncLayer* pAsyncLayer = pDevice->GetAsyncLayer();
    VkShaderModule immedModule = VK_NULL_HANDLE;

    VK_ASSERT(pCreateInfo->flags == 0);

    // Build shader module with immedidate mode
    auto result = ASYNC_CALL_NEXT_LAYER(vkCreateShaderModule)(
        VkDevice(ApiDevice::FromObject(pDevice)),
        pCreateInfo,
        pAllocator,
        &immedModule);

    if (result == VK_SUCCESS)
    {
        const size_t objSize = sizeof(ShaderModule);
        void* pMemory = pDevice->AllocApiObject(objSize, pAllocator);

        if (pMemory == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        VK_PLACEMENT_NEW(pMemory) ShaderModule(immedModule);
        ShaderModule* pShaderModuleObj = static_cast<ShaderModule*>(pMemory);
        *pShaderModule = ShaderModule::HandleFromVoidPointer(pMemory);

        // Build shader module in async mode
        pShaderModuleObj->AsyncBuildShaderModule(pDevice->GetAsyncLayer());
    }

    return result;
}

// =====================================================================================================================
// Destory async shader module object
VkResult ShaderModule::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    AsyncLayer* pAsyncLayer = pDevice->GetAsyncLayer();
    if (m_asyncModule == VK_NULL_HANDLE)
    {
        pAsyncLayer->SyncAll();
    }

    if (m_immedModule != VK_NULL_HANDLE)
    {
        ASYNC_CALL_NEXT_LAYER(vkDestroyShaderModule)(
            VkDevice(ApiDevice::FromObject(pDevice)),
            m_immedModule,
            pAllocator);
    }

    if (m_asyncModule != VK_NULL_HANDLE)
    {
        ASYNC_CALL_NEXT_LAYER(vkDestroyShaderModule)(
            VkDevice(ApiDevice::FromObject(pDevice)),
            m_asyncModule,
            pAllocator);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Builds shader module in async mode
void ShaderModule::AsyncBuildShaderModule(
    AsyncLayer* pAsyncLayer)
{
    auto pTaskThread = reinterpret_cast<async::TaskThread<ShaderModuleTask>*>
                       (pAsyncLayer->GetTaskThread(ShaderModuleTaskType));
    if (pTaskThread != nullptr)
    {
        vk::ShaderModule* pNextLayerModule = vk::ShaderModule::ObjectFromHandle(m_immedModule);

        ShaderModuleTask task = {};
        task.info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        task.info.pCode = reinterpret_cast<const uint32_t*>(pNextLayerModule->GetCode());
        task.info.codeSize = pNextLayerModule->GetCodeSize();
        task.info.flags = VK_SHADER_MODULE_ENABLE_OPT_BIT;
        task.pObj = this;
        pTaskThread->AddTask(&task);
    }
}

// =====================================================================================================================
// Creates shader module with shader module opt enabled.
void ShaderModule::Execute(
    AsyncLayer*      pAsyncLayer,
    ShaderModuleTask* pTask)
{
    Device* pDevice = pAsyncLayer->GetDevice();
    ASYNC_CALL_NEXT_LAYER(vkCreateShaderModule)(VkDevice(ApiDevice::FromObject(pDevice)),
                                                &pTask->info,
                                                nullptr,
                                                &m_asyncModule);
    const RuntimeSettings& settings  = pDevice->GetRuntimeSettings();
    if (settings.enablePartialPipelineCompile)
    {
        const VkAllocationCallbacks* pAllocCB = pDevice->VkInstance()->GetAllocCallbacks();
        auto pPartialPipelineObj = vk::async::PartialPipeline::Create(pDevice, pAllocCB);

        if ((pPartialPipelineObj != nullptr) && (m_asyncModule != VK_NULL_HANDLE))
        {
            // Build partial pipeline in async mode
            pPartialPipelineObj->AsyncBuildPartialPipeline(pDevice->GetAsyncLayer(), m_asyncModule);
        }
    }
}

} // namespace async

} // namespace vk
