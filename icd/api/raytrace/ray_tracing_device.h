/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once
#ifndef __RAY_TRACING_DEVICE_H__
#define __RAY_TRACING_DEVICE_H__

#include "gpurt/gpurt.h"

#include "khronos/vulkan.h"
#include "vk_defines.h"

namespace vk
{

class Device;
class Queue;
class InternalMemory;

// Device-level structure for managing state related to ray-tracing.  Instantiated as part of a VkDevice.
class RayTracingDevice
{
public:

    struct CmdContext
    {
        Pal::IDevice*       pDevice;
        Pal::ICmdBuffer*    pCmdBuffer;
        Pal::IQueue*        pQueue;
        Pal::IFence*        pFence;
    };

    static const uint32_t BufferViewDwords = 4;

    struct AccelStructTrackerResources
    {
        InternalMemory* pMem;
        uint32_t        srd[BufferViewDwords];
    };

    RayTracingDevice(Device* pDevice);
    ~RayTracingDevice();

    VkResult Init();
    void Destroy();

    void CreateGpuRtDeviceSettings(GpuRt::DeviceSettings* pDeviceSettings);
    GpuRt::IDevice* GpuRt(uint32_t deviceIdx) { return m_pGpuRtDevice[deviceIdx]; }
    const GpuRt::DeviceSettings& DeviceSettings() const { return m_gpurtDeviceSettings; }

    Pal::Result InitCmdContext(uint32_t deviceIdx);
    CmdContext* GetCmdContext(uint32_t deviceIdx) { return &m_cmdContext[deviceIdx]; }

    VkResult                   InitAccelStructTracker();
    bool                       AccelStructTrackerEnabled(uint32_t deviceIdx) const;
    GpuRt::AccelStructTracker* GetAccelStructTracker(uint32_t deviceIdx) const;
    Pal::gpusize               GetAccelStructTrackerGpuVa(uint32_t deviceIdx) const;
    const uint32_t*            GetAccelStructTrackerSrd(uint32_t deviceIdx) const
        { return &m_accelStructTrackerResources[deviceIdx].srd[0]; }

    uint64_t GetAccelerationStructureUUID(const Pal::DeviceProperties& palProps);

    uint32_t GetProfileRayFlags() const { return m_profileRayFlags; }
    uint32_t GetProfileMaxIterations() const { return m_profileMaxIterations; }

    GpuRt::TraceRayCounterMode TraceRayCounterMode(uint32_t deviceIdx) const;

    bool RayHistoryTraceActive(uint32_t deviceIdx) const
        { return m_pGpuRtDevice[deviceIdx]->RayHistoryTraceActive(); }

    void TraceDispatch(
        uint32_t                               deviceIdx,
        Pal::ICmdBuffer*                       pPalCmdBuffer,
        GpuRt::RtPipelineType                  pipelineType,
        uint32_t                               width,
        uint32_t                               height,
        uint32_t                               depth,
        uint32_t                               shaderCount,
        uint64_t                               apiHash,
        const VkStridedDeviceAddressRegionKHR* pRaygenSbt,
        const VkStridedDeviceAddressRegionKHR* pMissSbt,
        const VkStridedDeviceAddressRegionKHR* pHitSbt,
        GpuRt::DispatchRaysConstants*          pConstants);

    void TraceIndirectDispatch(
        uint32_t                               deviceIdx,
        GpuRt::RtPipelineType                  pipelineType,
        uint32_t                               originalThreadGroupSizeX,
        uint32_t                               originalThreadGroupSizeY,
        uint32_t                               originalThreadGroupSizeZ,
        uint32_t                               shaderCount,
        uint64_t                               apiHash,
        const VkStridedDeviceAddressRegionKHR* pRaygenSbt,
        const VkStridedDeviceAddressRegionKHR* pMissSbt,
        const VkStridedDeviceAddressRegionKHR* pHitSbt,
        Pal::gpusize*                          pCounterMetadataVa,
        void*                                  pConstants);

private:
    Device*                         m_pDevice;

    GpuRt::IDevice*                 m_pGpuRtDevice[MaxPalDevices];
    GpuRt::DeviceSettings           m_gpurtDeviceSettings;

    uint32_t                        m_profileRayFlags;           // Ray flag override for profiling
    uint32_t                        m_profileMaxIterations;      // Max traversal iterations

    CmdContext                      m_cmdContext[MaxPalDevices];

    // GPURT Callback Functions
    static Pal::Result ClientAllocateGpuMemory(
        const GpuRt::DeviceInitInfo& initInfo,
        uint64_t            sizeInBytes,
        ClientGpuMemHandle* pGpuMem,
        Pal::gpusize*       pDestGpuVa,
        void**              ppMappedData);

    static Pal::Result ClientCreateInternalComputePipeline(
        const GpuRt::DeviceInitInfo&        initInfo,
        const GpuRt::PipelineBuildInfo&     buildInfo,
        const GpuRt::CompileTimeConstants&  compileConstants,
        Pal::IPipeline**                    ppResultPipeline,
        void**                              ppResultMemory);

    static void ClientDestroyInternalComputePipeline(
        const GpuRt::DeviceInitInfo&    initInfo,
        Pal::IPipeline*                 pPipeline,
        void*                           pMemory);

    static void ClientInsertRGPMarker(
        Pal::ICmdBuffer*    pCmdBuffer,
        const char*         pMarker,
        bool                isPush);

    static Pal::Result ClientAccelStructBuildDumpEvent(
        Pal::ICmdBuffer*                    pPalCmdbuf,
        const GpuRt::AccelStructInfo&       info,
        const GpuRt::AccelStructBuildInfo&  buildInfo,
        Pal::gpusize*                       pDumpGpuVirtAddr);

    static Pal::Result ClientAccelStatsBuildDumpEvent(
        Pal::ICmdBuffer*                pPalCmdbuf,
        GpuRt::AccelStructInfo*         pInfo);

    static Pal::Result ClientAcquireCmdContext(
        const GpuRt::DeviceInitInfo&    initInfo,
        ClientCmdContextHandle*         pContext,
        Pal::ICmdBuffer**               ppCmdBuffer);

    static Pal::Result ClientFlushCmdContext(
        ClientCmdContextHandle          context);

    static void ClientFreeGpuMem(
        const GpuRt::DeviceInitInfo&    initInfo,
        ClientGpuMemHandle              gpuMem);

    void SetDispatchInfo(
        GpuRt::RtPipelineType                  pipelineType,
        uint32_t                               width,
        uint32_t                               height,
        uint32_t                               depth,
        uint32_t                               shaderCount,
        uint64_t                               apiHash,
        const VkStridedDeviceAddressRegionKHR* pRaygenSbt,
        const VkStridedDeviceAddressRegionKHR* pMissSbt,
        const VkStridedDeviceAddressRegionKHR* pHitSbt,
        GpuRt::RtDispatchInfo*                 pDispatchInfo) const;

    AccelStructTrackerResources     m_accelStructTrackerResources[MaxPalDevices];
};

};

#endif
