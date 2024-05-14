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
* @file  devmode_ubertrace.h
* @brief Contains the UberTrace implementation of the GPU Open Developer Mode (DevModeUberTrace)
***********************************************************************************************************************
*/

#ifndef __DEVMODE_DEVMODE_UBERTRACE_H__
#define __DEVMODE_DEVMODE_UBERTRACE_H__

#pragma once

#include "devmode/devmode_mgr.h"
#include "palTraceSession.h"

// GPUOpen forward declarations
namespace DevDriver
{
class DevDriverServer;
}

namespace GpuUtil
{
class CodeObjectTraceSource;
class QueueTimingsTraceSource;
}

namespace vk
{

// =====================================================================================================================
// This class provides functionality to interact with the GPU Open Developer Mode message passing service and the rest
// of the driver.
class DevModeUberTrace final : public IDevMode
{
#if ICD_GPUOPEN_DEVMODE_BUILD
public:
    ~DevModeUberTrace();

    static VkResult Create(Instance* pInstance, DevModeUberTrace** ppObject);

    virtual void Finalize(
        uint32_t              deviceCount,
        VulkanSettingsLoader* settingsLoaders[]) override;

    virtual void Destroy() override;

    virtual void NotifyFrameBegin(const Queue* pQueue, FrameDelimiterType delimiterType) override;
    virtual void NotifyFrameEnd(const Queue* pQueue, FrameDelimiterType delimiterType) override;
    virtual void WaitForDriverResume() override;
    virtual void PipelineCreated(Device* pDevice, Pipeline* pPipeline) override;
    virtual void PipelineDestroyed(Device* pDevice, Pipeline* pPipeline) override;
#if VKI_RAY_TRACING
    virtual void ShaderLibrariesCreated(Device* pDevice, RayTracingPipeline* pPipeline) override;
    virtual void ShaderLibrariesDestroyed(Device* pDevice, RayTracingPipeline* pPipeline) override;
#endif
    virtual void PostDeviceCreate(Device* pDevice) override;
    virtual void PreDeviceDestroy(Device* pDevice) override { };
    virtual void NotifyPreSubmit() override { };

    virtual bool IsTracingEnabled() const override;
    virtual bool IsCrashAnalysisEnabled() const override { return m_crashAnalysisEnabled; }
    virtual bool IsQueueTimingActive(const Device* pDevice) const override;

    virtual Pal::Result TimedQueueSubmit(
        uint32_t               deviceIdx,
        Queue*                 pQueue,
        uint32_t               cmdBufferCount,
        const VkCommandBuffer* pCommandBuffers,
        const Pal::SubmitInfo& submitInfo,
        VirtualStackFrame*     pVirtStackFrame) override;

    virtual Pal::Result TimedSignalQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore) override;

    virtual Pal::Result TimedWaitQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore) override;

    // Deprecated functionality
    virtual uint64_t GetInstructionTraceTargetHash() override { return InvalidTargetPipelineHash; };
    virtual void StartInstructionTrace(CmdBuffer* pCmdBuffer) override { };
    virtual void StopInstructionTrace(CmdBuffer* pCmdBuffer) override { };

    virtual bool GetTraceFrameBeginTag(uint64_t* pTag) const override { return false; };
    virtual bool GetTraceFrameEndTag(uint64_t* pTag) const override { return false; };

    virtual Util::Result RegisterPipelineCache(
        PipelineBinaryCache* pPipelineCache,
        uint32_t             postSizeLimit) override { return Util::Result::Success; };

    virtual void DeregisterPipelineCache(
        PipelineBinaryCache* pPipelineCache) override { };

private:
    DevModeUberTrace(Instance* pInstance);

    Pal::Result InitUberTraceResources(Pal::IDevice* pPalDevice);
    void DestroyUberTraceResources();

    Pal::Result RegisterQueuesForDevice(Device* pDevice);

    Instance*                           m_pInstance;
    DevDriver::DevDriverServer*         m_pDevDriverServer;
    bool                                m_finalized;
    bool                                m_crashAnalysisEnabled;
    uint32_t                            m_globalFrameIndex;

    GpuUtil::TraceSession*              m_pTraceSession;
    GpuUtil::CodeObjectTraceSource*     m_pCodeObjectTraceSource;
    GpuUtil::QueueTimingsTraceSource*   m_pQueueTimingsTraceSource;
#endif
};

}

#endif /* __DEVMODE_DEVMODE_UBERTRACE_H__ */
