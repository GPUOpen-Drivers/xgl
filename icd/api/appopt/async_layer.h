/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  async_layer.h
* @brief Declaration of async compiler layer
***********************************************************************************************************************
*/

#ifndef __ASYNC_LAYER_H__
#define __ASYNC_LAYER_H__

#pragma once

#include "opt_layer.h"
#include "async_task_thread.h"

namespace vk
{

class Device;
class AsyncLayer;
class PalAllocator;

namespace async { class ShaderModule; class PartialPipeline; }

// Represents the shader module async compile info
struct ShaderModuleTask
{
    VkShaderModuleCreateInfo info;        // Shader module create info
    async::ShaderModule*     pObj;        // Output shader module object
};

// Represents the pipeline async compile info
struct PartialPipelineTask
{
    VkShaderModule              shaderModuleHandle; // Shader module handle
    async::PartialPipeline*     pObj;               // Output shader module object
};

// Thread task type
enum TaskType : uint32_t
{
    ShaderModuleTaskType = 0,
    PartialPipelineTaskType,
    MaxTaskType,
};

// =====================================================================================================================
// Class that specifies dispatch table override behavior for async compiler layers
class AsyncLayer final : public OptLayer
{
public:
    AsyncLayer(Device* pDevice);
    virtual ~AsyncLayer();

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

    Device* GetDevice() { return m_pDevice; }

    void* GetTaskThread(TaskType type)
    {
        VK_ASSERT(type < MaxTaskType);
        if (type == ShaderModuleTaskType)
        {
            return (m_activeThreadCount[type] > 0) ?
                    m_pModuleTaskThreads[(m_taskId[type]++) % m_activeThreadCount[type]] :
                    nullptr;
        }
        else
        {
            return (m_activeThreadCount[type] > 0) ?
                    m_pPipelineTaskThreads[(m_taskId[type]++) % m_activeThreadCount[type]] :
                    nullptr;
        }
    }

    void SyncAll();

protected:
    static constexpr uint32_t        MaxThreads = 8;  // Max thread count for shader module compile
    Device*                          m_pDevice;                  // Vulkan Device object
    async::TaskThread<ShaderModuleTask>* m_pModuleTaskThreads[MaxThreads]; // Async compiler threads
    async::TaskThread<PartialPipelineTask>* m_pPipelineTaskThreads[MaxThreads]; // Async compiler threads
    uint32_t                         m_taskId[MaxTaskType];                   // Hint to select compile thread
    uint32_t                         m_activeThreadCount[MaxTaskType];        // Active thread count
    // Internal buffer for m_taskThreadBuffer
    uint8_t                          m_moduleTaskThreadBuffer[MaxThreads][sizeof(async::TaskThread<ShaderModuleTask>)];
    uint8_t                          m_pipelineTaskThreadBuffer[MaxThreads]
                                                               [sizeof(async::TaskThread<PartialPipelineTask>)];

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(AsyncLayer);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ASYNC_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::async::func_name

#define ASYNC_OVERRIDE_ENTRY(entry_name) ASYNC_OVERRIDE_ALIAS(entry_name, entry_name)
// Helper function to call the next layer's function by name
#define ASYNC_CALL_NEXT_LAYER(entry_name) \
    pAsyncLayer->GetNextLayer()->GetEntryPoints().entry_name

} // namespace vk

#endif /* __OPT_LAYER_H__ */
