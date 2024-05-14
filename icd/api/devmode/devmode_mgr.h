/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  devmode_mgr.h
* @brief Contains the GPU Open Developer Mode interface (IDevMode)
***********************************************************************************************************************
*/

#ifndef __DEVMODE_DEVMODE_MGR_H__
#define __DEVMODE_DEVMODE_MGR_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_queue.h"
#include "include/vk_device.h"

// PAL headers
#include "palQueue.h"

// Vulkan forward declarations
namespace vk
{
class Instance;
class Pipeline;
#if VKI_RAY_TRACING
class RayTracingPipeline;
#endif
class CmdBuffer;
class PipelineBinaryCache;
};

namespace vk
{

// =====================================================================================================================
// This class provides functionality to interact with the GPU Open Developer Mode message passing service and the rest
// of the driver.
class IDevMode
{
#if ICD_GPUOPEN_DEVMODE_BUILD
public:
    // Pipeline hash used for instruction tracing whenever no pipeline is being targetted.
    static constexpr uint64_t InvalidTargetPipelineHash = 0;

    virtual void Finalize(
        uint32_t              deviceCount,
        VulkanSettingsLoader* settingsLoaders[]) = 0;

    virtual void Destroy() = 0;

    enum class FrameDelimiterType : uint32_t
    {
        QueuePresent,
        QueueLabel,
        CmdBufferTag,
        Count
    };

    virtual void NotifyFrameBegin(const Queue* pQueue, FrameDelimiterType delimiterType) = 0;
    virtual void NotifyFrameEnd(const Queue* pQueue, FrameDelimiterType delimiterType) = 0;
    virtual void WaitForDriverResume() = 0;
    virtual void PipelineCreated(Device* pDevice, Pipeline* pPipeline) = 0;
    virtual void PipelineDestroyed(Device* pDevice, Pipeline* pPipeline) = 0;
#if VKI_RAY_TRACING
    virtual void ShaderLibrariesCreated(Device* pDevice, RayTracingPipeline* pPipeline) = 0;
    virtual void ShaderLibrariesDestroyed(Device* pDevice, RayTracingPipeline* pPipeline) = 0;
#endif
    virtual void PostDeviceCreate(Device* pDevice) = 0;
    virtual void PreDeviceDestroy(Device* pDevice) = 0;
    virtual void NotifyPreSubmit() = 0;

    virtual uint64_t GetInstructionTraceTargetHash() = 0;
    virtual void StartInstructionTrace(CmdBuffer* pCmdBuffer) = 0;
    virtual void StopInstructionTrace(CmdBuffer* pCmdBuffer) = 0;

    virtual bool IsTracingEnabled() const = 0;
    virtual bool IsCrashAnalysisEnabled() const = 0;

    virtual Pal::Result TimedQueueSubmit(
        uint32_t               deviceIdx,
        Queue*                 pQueue,
        uint32_t               cmdBufferCount,
        const VkCommandBuffer* pCommandBuffers,
        const Pal::SubmitInfo& submitInfo,
        VirtualStackFrame*     pVirtStackFrame) = 0;

    virtual Pal::Result TimedSignalQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore) = 0;

    virtual Pal::Result TimedWaitQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore) = 0;

    virtual bool IsQueueTimingActive(const Device* pDevice) const = 0;
    virtual bool GetTraceFrameBeginTag(uint64_t* pTag) const = 0;
    virtual bool GetTraceFrameEndTag(uint64_t* pTag) const = 0;

    virtual Util::Result RegisterPipelineCache(
        PipelineBinaryCache* pPipelineCache,
        uint32_t             postSizeLimit) = 0;

    virtual void DeregisterPipelineCache(
        PipelineBinaryCache* pPipelineCache) = 0;
#endif
};

}

#endif /* __DEVMODE_DEVMODE_MGR_H__ */
