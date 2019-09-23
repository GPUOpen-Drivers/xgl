/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
struct PalAllocator;

namespace async { class ShaderModule; }

// Represents the shader module async compile info
struct ShaderModuleTask
{
    VkShaderModuleCreateInfo info;        // Shader module create info
    async::ShaderModule*     pObj;        // Output shader module object
};

// =====================================================================================================================
// Class that specifies dispatch table override behavior for async compiler layers
class AsyncLayer : public OptLayer
{
public:
    AsyncLayer(Device* pDevice);
    virtual ~AsyncLayer();

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

    VK_INLINE Device* GetDevice() { return m_pDevice; }

    template<class Task>
    async::TaskThread<Task>* GetTaskThread()
    {
        static_assert(sizeof(Task) == sizeof(ShaderModuleTask), "Unexpected type");
        return (m_activeThreadCount > 0) ? m_pTaskThreads[(m_taskId++) % m_activeThreadCount] : nullptr;
    }

    void SyncAll();

protected:
    static constexpr uint32_t        MaxShaderModuleThreads = 8;  // Max thread count for shader module compile
    Device*                          m_pDevice;                  // Vulkan Device object
    async::TaskThread<ShaderModuleTask>* m_pTaskThreads[MaxShaderModuleThreads]; // Async compiler threads
    uint32_t                         m_taskId;                   // Hint to select compile thread
    uint32_t                         m_activeThreadCount;        // Active thread count
    // Internal buffer for m_pTaskThreads
    uint8_t                          m_taskThreadBuffer[MaxShaderModuleThreads]
                                                       [sizeof(async::TaskThread<ShaderModuleTask>)];
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
