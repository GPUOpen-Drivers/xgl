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

#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#include "raytrace/ray_tracing_util.h"
#include "raytrace/vk_acceleration_structure.h"
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_shader.h"
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_rgp_annotations.h"
#include "palAutoBuffer.h"
#include "gpurt/gpurtLib.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

namespace vk
{

// =====================================================================================================================
RayTracingDevice::RayTracingDevice(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_cmdContext(),
    m_accelStructTrackerResources()
{

}

// =====================================================================================================================
RayTracingDevice::~RayTracingDevice()
{

}

// =====================================================================================================================
// Initialized during device creation when raytracing extensions are enabled.
VkResult RayTracingDevice::Init()
{
    VkResult result = VK_SUCCESS;

    if (InitAccelStructTracker() != VK_SUCCESS)
    {
        // Report soft failure, as this feature is optional
        VK_NEVER_CALLED();
    }

    CreateGpuRtDeviceSettings(&m_gpurtDeviceSettings);

    for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < m_pDevice->NumPalDevices()); ++deviceIdx)
    {
        void* pMemory =
            m_pDevice->VkInstance()->AllocMem(
                GpuRt::GetDeviceSize(),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        else
        {
            GpuRt::DeviceInitInfo initInfo = {};

            initInfo.pDeviceProperties         = &m_pDevice->VkPhysicalDevice(deviceIdx)->PalProperties();
            initInfo.gpuIdx                    = deviceIdx;
            initInfo.deviceSettings            = m_gpurtDeviceSettings;
            initInfo.pPalDevice                = m_pDevice->PalDevice(deviceIdx);
            initInfo.pPalPlatform              = m_pDevice->VkInstance()->PalPlatform();
            initInfo.pClientUserData           = m_pDevice;
            initInfo.pAccelStructTracker       = GetAccelStructTracker(deviceIdx);
            initInfo.accelStructTrackerGpuAddr = GetAccelStructTrackerGpuVa(deviceIdx);

            initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::None;
            switch (m_pDevice->GetRuntimeSettings().emulatedRtIpLevel)
            {
            case EmulatedRtIpLevelNone:
                break;
            case HardwareRtIpLevel1_1:
            case EmulatedRtIpLevel1_1:
                initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::RtIp1_1;
                break;
#if VKI_BUILD_GFX11
            case EmulatedRtIpLevel2_0:
                initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::RtIp2_0;
                break;
#endif
            default:
                break;
            }

            GpuRt::ClientCallbacks callbacks             = {};
            callbacks.pfnInsertRGPMarker                 = &RayTracingDevice::ClientInsertRGPMarker;
            callbacks.pfnConvertAccelStructBuildGeometry =
                &AccelerationStructure::ClientConvertAccelStructBuildGeometry;
            callbacks.pfnConvertAccelStructBuildInstanceBottomLevel =
                &AccelerationStructure::ClientConvertAccelStructBuildInstanceBottomLevel;
            callbacks.pfnConvertAccelStructPostBuildInfo =
                &AccelerationStructure::ClientConvertAccelStructPostBuildInfo;
            callbacks.pfnAccelStructBuildDumpEvent       = &RayTracingDevice::ClientAccelStructBuildDumpEvent;
            callbacks.pfnAccelStatsBuildDumpEvent        = &RayTracingDevice::ClientAccelStatsBuildDumpEvent;
            callbacks.pfnCreateInternalComputePipeline   = &RayTracingDevice::ClientCreateInternalComputePipeline;
            callbacks.pfnDestroyInternalComputePipeline  = &RayTracingDevice::ClientDestroyInternalComputePipeline;
            callbacks.pfnAcquireCmdContext               = &RayTracingDevice::ClientAcquireCmdContext;
            callbacks.pfnFlushCmdContext                 = &RayTracingDevice::ClientFlushCmdContext;
            callbacks.pfnAllocateGpuMemory               = &RayTracingDevice::ClientAllocateGpuMemory;
            callbacks.pfnFreeGpuMem                      = &RayTracingDevice::ClientFreeGpuMem;

            Pal::Result palResult = GpuRt::CreateDevice(initInfo, callbacks, pMemory, &m_pGpuRtDevice[deviceIdx]);

            if (palResult != Pal::Result::Success)
            {
                m_pDevice->VkInstance()->FreeMem(pMemory);

                VK_NEVER_CALLED();

                result = VK_ERROR_INITIALIZATION_FAILED;
            }

        }
    }

    return result;
}

// =====================================================================================================================
void RayTracingDevice::CreateGpuRtDeviceSettings(
    GpuRt::DeviceSettings* pDeviceSettings)
{
    *pDeviceSettings                = {};
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    pDeviceSettings->bvhCollapse                  = settings.rtEnableBVHCollapse;
    pDeviceSettings->topDownBuild                 = settings.rtEnableTopDownBuild;

    pDeviceSettings->rebraidType                  = ConvertGpuRtRebraidType(settings.rtEnableTreeRebraid);
    pDeviceSettings->fp16BoxNodesInBlasMode       =
        ConvertGpuRtFp16BoxNodesInBlasMode(settings.rtFp16BoxNodesInBlasMode);

    // Surface area threshold for fp16 box nodes mode MixedNodes
    // When given 0, defaults to 1.5f. Clamped to [1.0, 8.0]
    float fp16BoxMixedThreshold;

    if (settings.rtFp16BoxNodesInBlasModeMixedThreshold == 0.f)
    {
        fp16BoxMixedThreshold = 1.5f;
    }
    else
    {
        fp16BoxMixedThreshold = settings.rtFp16BoxNodesInBlasModeMixedThreshold;
    }

    pDeviceSettings->fp16BoxModeMixedSaThresh = Util::Clamp(fp16BoxMixedThreshold, 1.0f, 8.0f);
    pDeviceSettings->enableMortonCode30                = settings.rtEnableMortonCode30;
    pDeviceSettings->enableVariableBitsMortonCodes     = settings.enableVariableBitsMortonCodes;
    pDeviceSettings->enablePrefixScanDLB               = settings.rtEnablePrefixScanDLB;
    pDeviceSettings->triangleCompressionAutoMode       =
        ConvertGpuRtTriCompressionAutoMode(settings.rtTriangleCompressionAutoMode);
    pDeviceSettings->bvhBuildModeDefault               = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeDefault);
    pDeviceSettings->bvhBuildModeFastTrace             = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeFastTrace);
    pDeviceSettings->bvhBuildModeFastBuild             = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeFastBuild);
    pDeviceSettings->bvhBuildModeOverrideBLAS          = ConvertGpuRtBvhBuildMode(settings.bvhBuildModeOverrideBLAS);
    pDeviceSettings->bvhBuildModeOverrideTLAS          = ConvertGpuRtBvhBuildMode(settings.bvhBuildModeOverrideTLAS);
    pDeviceSettings->enableParallelUpdate              = settings.rtEnableUpdateParallel;
    pDeviceSettings->enableParallelBuild               = settings.rtEnableBuildParallel;
    pDeviceSettings->parallelBuildWavesPerSimd         = settings.buildParallelWavesPerSimd;
    pDeviceSettings->enableAcquireReleaseInterface     = settings.rtEnableAcquireReleaseInterface;
    pDeviceSettings->bvhCpuBuildModeFastTrace          = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);
    pDeviceSettings->bvhCpuBuildModeDefault            = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);
    pDeviceSettings->bvhCpuBuildModeFastBuild          = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);
    pDeviceSettings->enableTriangleSplitting           = settings.rtEnableTriangleSplitting;
    pDeviceSettings->triangleSplittingFactor           = settings.rtTriangleSplittingFactor;
    pDeviceSettings->enableFusedInstanceNode           = settings.enableFusedInstanceNode;
    pDeviceSettings->rebraidFactor                     = settings.rebraidFactor;
    pDeviceSettings->rebraidLengthPercentage           = settings.rebraidLengthPercentage;
    pDeviceSettings->maxTopDownBuildInstances          = settings.maxTopDownBuildInstances;
    pDeviceSettings->plocRadius                        = settings.plocRadius;
    pDeviceSettings->enablePairCompressionCostCheck    = settings.enablePairCompressionCostCheck;
    pDeviceSettings->accelerationStructureUUID         = GetAccelerationStructureUUID(
                                                     m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties());
    pDeviceSettings->enableMergeSort                   = settings.enableMergeSort;
    pDeviceSettings->fastBuildThreshold                = settings.fastBuildThreshold;
    pDeviceSettings->lbvhBuildThreshold                = settings.lbvhBuildThreshold;
    pDeviceSettings->enableBVHBuildDebugCounters       = settings.enableBVHBuildDebugCounters;
    pDeviceSettings->enableInsertBarriersInBuildAS     = settings.enableInsertBarriersInBuildAS;
    pDeviceSettings->numMortonSizeBits                 = settings.numMortonSizeBits;
    pDeviceSettings->allowFp16BoxNodesInUpdatableBvh   = settings.rtAllowFp16BoxNodesInUpdatableBVH;

    pDeviceSettings->enableBuildAccelStructScratchDumping = pDeviceSettings->enableBuildAccelStructDumping &&
                                                            settings.rtEnableAccelerationStructureScratchMemoryDump;

    // Enable AS stats based on panel setting
    pDeviceSettings->enableBuildAccelStructStats        = settings.rtEnableBuildAccelStructStats;

    pDeviceSettings->rgpBarrierReason   = RgpBarrierInternalRayTracingSync;
    m_profileRayFlags                   = TraceRayProfileFlagsToRayFlag(settings);
    m_profileMaxIterations              = TraceRayProfileMaxIterationsToMaxIterations(settings);
}

// =====================================================================================================================
void RayTracingDevice::Destroy()
{
    // Free cmd buffer resources
    for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; ++deviceIdx)
    {
        CmdContext* pCmdContext = &m_cmdContext[deviceIdx];

        if (pCmdContext->pFence != nullptr)
        {
            pCmdContext->pFence->Destroy();
        }

        if (pCmdContext->pCmdBuffer != nullptr)
        {
            pCmdContext->pCmdBuffer->Destroy();

            m_pDevice->VkInstance()->FreeMem(pCmdContext->pCmdBuffer);
        }
    }

    // Free accel struct tracker GPU memory and GpuRT::IDevice
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (m_accelStructTrackerResources[deviceIdx].pMem != nullptr)
        {
            m_pDevice->MemMgr()->FreeGpuMem(m_accelStructTrackerResources[deviceIdx].pMem);
        }

        if (m_pGpuRtDevice[deviceIdx] != nullptr)
        {
            m_pGpuRtDevice[deviceIdx]->Destroy();
            m_pDevice->VkInstance()->FreeMem(m_pGpuRtDevice[deviceIdx]);
        }
    }

    // Free accel struct tracker CPU memory
    if (m_accelStructTrackerResources[0].pMem != nullptr)
    {
        m_pDevice->VkInstance()->FreeMem(m_accelStructTrackerResources[0].pMem);
    }

    Util::Destructor(this);

    m_pDevice->VkInstance()->FreeMem(this);
}

// =====================================================================================================================
bool RayTracingDevice::AccelStructTrackerEnabled(
    uint32_t deviceIdx) const
{
    return (GetAccelStructTracker(deviceIdx) != nullptr) &&
            (m_pDevice->GetRuntimeSettings().enableTraceRayAccelStructTracking ||
             m_pGpuRtDevice[deviceIdx]->AccelStructTraceEnabled());
}

// =====================================================================================================================
GpuRt::TraceRayCounterMode RayTracingDevice::TraceRayCounterMode(
    uint32_t deviceIdx) const
{
    // If the PAL trace path is enabled, then force RayHistoryLight
    return m_pGpuRtDevice[deviceIdx]->RayHistoryTraceAvailable() ?
            GpuRt::TraceRayCounterMode::TraceRayCounterRayHistoryLight :
            static_cast<GpuRt::TraceRayCounterMode>(m_pDevice->GetRuntimeSettings().rtTraceRayCounterMode);
}

// =====================================================================================================================
GpuRt::AccelStructTracker* RayTracingDevice::GetAccelStructTracker(
    uint32_t deviceIdx) const
{
    GpuRt::AccelStructTracker* pTracker   = nullptr;
    auto*                      pResources = &m_accelStructTrackerResources[deviceIdx];

    if (pResources->pMem != nullptr)
    {
        pTracker = static_cast<GpuRt::AccelStructTracker*>(pResources->pMem->CpuAddr(deviceIdx));
    }

    return pTracker;
}

// =====================================================================================================================
Pal::gpusize RayTracingDevice::GetAccelStructTrackerGpuVa(
    uint32_t deviceIdx) const
{
    Pal::gpusize gpuAddr    = 0;
    auto*        pResources = &m_accelStructTrackerResources[deviceIdx];

    if (pResources->pMem != nullptr)
    {
        gpuAddr = pResources->pMem->GpuVirtAddr(deviceIdx);
    }

    return gpuAddr;
}

// =====================================================================================================================
VkResult RayTracingDevice::InitAccelStructTracker()
{
    Pal::Result result = Pal::Result::Success;

    size_t placementOffset = 0;
    void*  pSystemMemory   = m_pDevice->VkInstance()->AllocMem(
        sizeof(vk::InternalMemory) * m_pDevice->NumPalDevices(),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pSystemMemory == nullptr)
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    for (uint32_t deviceIdx = 0;
            (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
            ++deviceIdx)
    {
        Pal::IDevice* pPalDevice = m_pDevice->PalDevice(deviceIdx);
        auto*         pTracker   = &m_accelStructTrackerResources[deviceIdx];

        pTracker->pMem   = VK_PLACEMENT_NEW(Util::VoidPtrInc(pSystemMemory, placementOffset)) InternalMemory;
        placementOffset += sizeof(vk::InternalMemory);

        if (result == Pal::Result::Success)
        {
            uint32_t                  deviceMask = 1 << deviceIdx;
            vk::InternalMemCreateInfo allocInfo  = {};

            allocInfo.pal.alignment = 4;
            allocInfo.pal.size      = sizeof(GpuRt::AccelStructTracker);
            allocInfo.pal.vaRange   = Pal::VaRange::Default;
            allocInfo.pal.priority  = Pal::GpuMemPriority::Normal;

            allocInfo.flags.persistentMapped = true;

            allocInfo.pal.heapCount = 2;
            allocInfo.pal.heaps[0]  = Pal::GpuHeap::GpuHeapLocal;
            allocInfo.pal.heaps[1]  = Pal::GpuHeap::GpuHeapGartUswc;

            result = (m_pDevice->MemMgr()->AllocGpuMem(
                allocInfo,
                pTracker->pMem,
                deviceMask,
                VK_OBJECT_TYPE_DEVICE,
                ApiDevice::IntValueFromHandle(ApiDevice::FromObject(m_pDevice))) == VK_SUCCESS) ?
                    Pal::Result::Success : Pal::Result::ErrorUnknown;

            if (result != Pal::Result::Success)
            {
                // Set to nullptr so we don't try to free it later
                pTracker->pMem = nullptr;
            }
        }

        if (result == Pal::Result::Success)
        {
            GpuRt::AccelStructTracker* pAccelStructTracker =
                static_cast<GpuRt::AccelStructTracker*>(pTracker->pMem->CpuAddr(deviceIdx));

            // Clear the struct
            *pAccelStructTracker = {};

            // Create structured buffer view
            Pal::BufferViewInfo viewInfo = {};
            viewInfo.gpuAddr             = pTracker->pMem->GpuVirtAddr(deviceIdx);
            viewInfo.range               = sizeof(GpuRt::AccelStructTracker);
            viewInfo.stride              = sizeof(GpuRt::AccelStructTracker);

            // Ensure the SRD size matches with the GPURT header definition
            static_assert(sizeof(pTracker->srd) == sizeof(GpuRt::DispatchRaysTopLevelData::accelStructTrackerSrd),
                            "The size of the AccelStructTracker SRD mismatches between XGL and GPURT.");

            // Ensure the SRD size matches with the size reported by PAL
            VK_ASSERT(sizeof(pTracker->srd) ==
                m_pDevice->VkPhysicalDevice(deviceIdx)->PalProperties().gfxipProperties.srdSizes.bufferView);

            pPalDevice->CreateUntypedBufferViewSrds(1, &viewInfo, &pTracker->srd);
        }
    }

    if (result != Pal::Result::Success)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            if (m_accelStructTrackerResources[deviceIdx].pMem != nullptr)
            {
                m_pDevice->MemMgr()->FreeGpuMem(m_accelStructTrackerResources[deviceIdx].pMem);

                m_accelStructTrackerResources[deviceIdx].pMem = nullptr;
            }
        }

        if (pSystemMemory != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(pSystemMemory);
        }
    }

    // If accel struct tracker is disabled or if creation failed
    if (GetAccelStructTracker(DefaultDeviceIndex) == nullptr)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            const auto& props = m_pDevice->VkPhysicalDevice(deviceIdx)->PalProperties();

            // Create null view if tracking is disabled.
            memcpy(&m_accelStructTrackerResources[deviceIdx].srd[0],
                   props.gfxipProperties.nullSrds.pNullBufferView,
                   props.gfxipProperties.srdSizes.bufferView);
        }
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
Pal::Result RayTracingDevice::InitCmdContext(
    uint32_t deviceIdx)
{
    Pal::IDevice*            pPalDevice = m_pDevice->PalDevice(deviceIdx);
    Pal::CmdBufferCreateInfo cmdBufInfo = {};

    cmdBufInfo.pCmdAllocator = m_pDevice->GetSharedCmdAllocator(deviceIdx);

    // First try a compute queue
    cmdBufInfo.engineType = Pal::EngineTypeCompute;
    cmdBufInfo.queueType  = Pal::QueueTypeCompute;

    VkQueue queueHandle = m_pDevice->GetQueue(cmdBufInfo.engineType, cmdBufInfo.queueType);

    if (queueHandle == VK_NULL_HANDLE)
    {
        // Could not find a compute queue, try universal
        cmdBufInfo.engineType = Pal::EngineTypeUniversal;
        cmdBufInfo.queueType  = Pal::QueueTypeUniversal;

        queueHandle = m_pDevice->GetQueue(cmdBufInfo.engineType, cmdBufInfo.queueType);
    }

    Pal::Result result = (queueHandle != VK_NULL_HANDLE) ? Pal::Result::Success : Pal::Result::ErrorUnknown;

    void*  pStorage   = nullptr;
    size_t cmdBufSize = 0;
    size_t fenceSize  = 0;

    if (result == Pal::Result::Success)
    {
        cmdBufSize = pPalDevice->GetCmdBufferSize(cmdBufInfo, &result);
    }

    if (result == Pal::Result::Success)
    {
        fenceSize = pPalDevice->GetFenceSize(&result);
    }

    if (result == Pal::Result::Success)
    {
        pStorage = m_pDevice->VkInstance()->AllocMem(cmdBufSize + fenceSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    }

    Pal::ICmdBuffer* pCmdBuffer = nullptr;
    Pal::IFence*     pFence     = nullptr;

    if (pStorage != nullptr)
    {
        if (result == Pal::Result::Success)
        {
            result = pPalDevice->CreateCmdBuffer(cmdBufInfo, pStorage, &pCmdBuffer);
        }

        if (result == Pal::Result::Success)
        {
            Pal::FenceCreateInfo fenceInfo = {};

            result = pPalDevice->CreateFence(fenceInfo, Util::VoidPtrInc(pStorage, cmdBufSize), &pFence);
        }

        VK_ASSERT(static_cast<void*>(pCmdBuffer) == pStorage);
    }

    if (result == Pal::Result::Success)
    {
        CmdContext* pCmdContext = &m_cmdContext[deviceIdx];

        pCmdContext->pDevice    = pPalDevice;
        pCmdContext->pCmdBuffer = pCmdBuffer;
        pCmdContext->pFence     = pFence;
        pCmdContext->pQueue     = ApiQueue::ObjectFromHandle(queueHandle)->PalQueue(deviceIdx);
    }
    else
    {
        if (pCmdBuffer != nullptr)
        {
            pCmdBuffer->Destroy();
        }

        if (pFence != nullptr)
        {
            pFence->Destroy();
        }

        if (pStorage != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(pStorage);
        }
    }

    return result;
}

// =====================================================================================================================
uint64_t RayTracingDevice::GetAccelerationStructureUUID(
    const Pal::DeviceProperties& palProps)
{
    const uint32_t gfxip = static_cast<uint32_t>(palProps.gfxLevel);

    return static_cast<uint64_t>(gfxip) << 32 | vk::utils::GetBuildTimeHash();
}

// =====================================================================================================================
void RayTracingDevice::SetDispatchInfo(
    GpuRt::RtPipelineType                  pipelineType,
    uint32_t                               width,
    uint32_t                               height,
    uint32_t                               depth,
    uint32_t                               shaderCount,
    uint64_t                               apiHash,
    const VkStridedDeviceAddressRegionKHR* pRaygenSbt,
    const VkStridedDeviceAddressRegionKHR* pMissSbt,
    const VkStridedDeviceAddressRegionKHR* pHitSbt,
    GpuRt::RtDispatchInfo*                 pDispatchInfo) const
{
    const RuntimeSettings& settings    = m_pDevice->GetRuntimeSettings();
    GpuRt::RtDispatchInfo dispatchInfo = {};

    dispatchInfo.dimX                   = width;
    dispatchInfo.dimY                   = height;
    dispatchInfo.dimZ                   = depth;
    dispatchInfo.threadGroupSizeX       = 0;
    dispatchInfo.threadGroupSizeY       = 0;
    dispatchInfo.threadGroupSizeZ       = 0;

    dispatchInfo.pipelineShaderCount = shaderCount;
    dispatchInfo.stateObjectHash     = apiHash;

    dispatchInfo.boxSortMode         = settings.boxSortingHeuristic;
#if VKI_BUILD_GFX11
    dispatchInfo.usesNodePtrFlags    = settings.rtEnableNodePointerFlags ? 1 : 0;
#endif

    if (pipelineType == GpuRt::RtPipelineType::RayTracing)
    {
        dispatchInfo.raygenShaderTable.addr   = static_cast<Pal::gpusize>(pRaygenSbt->deviceAddress);
        dispatchInfo.raygenShaderTable.size   = static_cast<Pal::gpusize>(pRaygenSbt->size);
        dispatchInfo.raygenShaderTable.stride = static_cast<Pal::gpusize>(pRaygenSbt->stride);

        dispatchInfo.missShaderTable.addr     = static_cast<Pal::gpusize>(pMissSbt->deviceAddress);
        dispatchInfo.missShaderTable.size     = static_cast<Pal::gpusize>(pMissSbt->size);
        dispatchInfo.missShaderTable.stride   = static_cast<Pal::gpusize>(pMissSbt->stride);

        dispatchInfo.hitGroupTable.addr       = static_cast<Pal::gpusize>(pHitSbt->deviceAddress);
        dispatchInfo.hitGroupTable.size       = static_cast<Pal::gpusize>(pHitSbt->size);
        dispatchInfo.hitGroupTable.stride     = static_cast<Pal::gpusize>(pHitSbt->stride);
    }

    (*pDispatchInfo) = dispatchInfo;
}

// =====================================================================================================================
void RayTracingDevice::TraceDispatch(
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
    GpuRt::DispatchRaysConstants*          pConstants)
{
    if (m_pGpuRtDevice[deviceIdx]->RayHistoryTraceActive())
    {
        GpuRt::RtDispatchInfo dispatchInfo = {};
        SetDispatchInfo(pipelineType,
                        width,
                        height,
                        depth,
                        shaderCount,
                        apiHash,
                        pRaygenSbt,
                        pMissSbt,
                        pHitSbt,
                        &dispatchInfo);

        m_pGpuRtDevice[deviceIdx]->TraceRtDispatch(pPalCmdBuffer,
                                                   pipelineType,
                                                   dispatchInfo,
                                                   pConstants);
    }
}

// =====================================================================================================================
void RayTracingDevice::TraceIndirectDispatch(
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
    void*                                  pConstants)
{
    if (m_pGpuRtDevice[deviceIdx]->RayHistoryTraceActive())
    {
        GpuRt::RtDispatchInfo dispatchInfo = {};
        SetDispatchInfo(pipelineType,
                        0,
                        0,
                        0,
                        shaderCount,
                        apiHash,
                        pRaygenSbt,
                        pMissSbt,
                        pHitSbt,
                        &dispatchInfo);

        dispatchInfo.threadGroupSizeX = originalThreadGroupSizeX;
        dispatchInfo.threadGroupSizeY = originalThreadGroupSizeY;
        dispatchInfo.threadGroupSizeZ = originalThreadGroupSizeZ;

        m_pGpuRtDevice[deviceIdx]->TraceIndirectRtDispatch(pipelineType,
                                                           dispatchInfo,
                                                           1,
                                                           pCounterMetadataVa,
                                                           pConstants);
    }
}

// =====================================================================================================================
// Compile one of gpurt's internal pipelines.
Pal::Result RayTracingDevice::ClientCreateInternalComputePipeline(
    const GpuRt::DeviceInitInfo&        initInfo,              ///< [in]  Information about the host device
    const GpuRt::PipelineBuildInfo&     buildInfo,             ///< [in]  Information about the pipeline to be built
    const GpuRt::CompileTimeConstants&  compileConstants,      ///< [in]  Compile time constant buffer description
    Pal::IPipeline**                    ppResultPipeline,      ///< [out] Result PAL pipeline object pointer
    void**                              ppResultMemory)        ///< [out] (Optional) Result PAL pipeline memory,
                                                               ///< if different from obj
{
    uint64_t spvPassMask =
        static_cast<vk::Device*>(initInfo.pClientUserData)->GetRuntimeSettings().rtInternalPipelineSpvPassMask;
    vk::Device* pDevice = static_cast<vk::Device*>(initInfo.pClientUserData);
    const auto& settings = pDevice->GetRuntimeSettings();

    uint64_t shaderTypeMask = 1ull << static_cast<uint64_t>(buildInfo.shaderType);

    bool useSpvPass = (shaderTypeMask & spvPassMask);

    *ppResultMemory = nullptr;

    {
        VkResult result = VK_SUCCESS;

        vk::PipelineCompiler* pCompiler     = pDevice->GetCompiler(initInfo.gpuIdx);
        vk::ShaderModuleHandle shaderModule = {};
        const void* pPipelineBinary         = nullptr;
        size_t pipelineBinarySize           = 0;

        Vkgc::ResourceMappingRootNode nodes[GpuRt::MaxInternalPipelineNodes]    = {};
        Vkgc::ResourceMappingNode subNodes[GpuRt::MaxInternalPipelineNodes]     = {};
        uint32_t subNodeIndex = 0;
        const uint32_t bufferSrdSizeDw = pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);

        for (uint32_t nodeIndex = 0; nodeIndex < buildInfo.nodeCount; ++nodeIndex)
        {
            // Make sure we haven't exceeded our maximum number of nodes.
            VK_ASSERT(nodeIndex < GpuRt::MaxInternalPipelineNodes);

            const GpuRt::NodeMapping& node = buildInfo.pNodes[nodeIndex];

            nodes[nodeIndex].visibility = Vkgc::ShaderStageComputeBit;

            if (node.type == GpuRt::NodeType::Constant)
            {
                nodes[nodeIndex].node.type              = Vkgc::ResourceMappingNodeType::PushConst;
                nodes[nodeIndex].node.sizeInDwords      = node.dwSize;
                nodes[nodeIndex].node.offsetInDwords    = node.dwOffset;
                nodes[nodeIndex].node.srdRange.set      = Vkgc::InternalDescriptorSetId;
                nodes[nodeIndex].node.srdRange.binding  = node.binding;
            }
            else if (node.type == GpuRt::NodeType::ConstantBuffer)
            {
                nodes[nodeIndex].node.type              =
                    Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact;
                nodes[nodeIndex].node.sizeInDwords      = node.dwSize;
                nodes[nodeIndex].node.offsetInDwords    = node.dwOffset;
                nodes[nodeIndex].node.srdRange.set      = node.descSet;
                nodes[nodeIndex].node.srdRange.binding  = node.binding;
            }
            else if (node.type == GpuRt::NodeType::Uav)
            {
                nodes[nodeIndex].node.type              =
                    Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
                nodes[nodeIndex].node.sizeInDwords      = node.dwSize;
                nodes[nodeIndex].node.offsetInDwords    = node.dwOffset;
                nodes[nodeIndex].node.srdRange.set      = node.descSet;
                nodes[nodeIndex].node.srdRange.binding  = node.binding;
            }
            else if ((node.type == GpuRt::NodeType::ConstantBufferTable) ||
                (node.type == GpuRt::NodeType::UavTable) ||
                (node.type == GpuRt::NodeType::TypedUavTable))
            {
                Vkgc::ResourceMappingNode* pSubNode      = &subNodes[subNodeIndex++];
                nodes[nodeIndex].node.type               =
                    Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                nodes[nodeIndex].node.sizeInDwords       = 1;
                nodes[nodeIndex].node.offsetInDwords     = node.dwOffset;
                nodes[nodeIndex].node.tablePtr.nodeCount = 1;
                nodes[nodeIndex].node.tablePtr.pNext     = pSubNode;

                switch (node.type)
                {
                case GpuRt::NodeType::UavTable:
                    pSubNode->type = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
                    break;
                case GpuRt::NodeType::TypedUavTable:
                    pSubNode->type = Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
                    break;
                case GpuRt::NodeType::ConstantBufferTable:
                    pSubNode->type = Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
                    break;
                default:
                    VK_NEVER_CALLED();
                }
                pSubNode->offsetInDwords    = 0;
                pSubNode->sizeInDwords      = bufferSrdSizeDw;
                pSubNode->srdRange.set      = node.descSet;
                pSubNode->srdRange.binding  = node.binding;
            }
            else
            {
                VK_NEVER_CALLED();
            }
        }

        const uint32_t numConstants = compileConstants.numConstants;

        // Set up specialization constant info
        VK_ASSERT(numConstants <= 64);
        Util::AutoBuffer<VkSpecializationMapEntry, 64, vk::PalAllocator> mapEntries(
            numConstants,
            pDevice->VkInstance()->Allocator());

        for (uint32_t i = 0; i < numConstants; i++)
        {
            mapEntries[i] = { i, static_cast<uint32_t>(i * sizeof(uint32_t)), sizeof(uint32_t) };
        }

        VkSpecializationInfo specializationInfo =
        {
            numConstants,
            &mapEntries[0],
            numConstants * sizeof(uint32_t),
            compileConstants.pConstants
        };

        Vkgc::BinaryData spvBin = { buildInfo.code.spvSize, buildInfo.code.pSpvCode };

        bool forceWave64 = false;

        // Overide wave size for these GpuRT shader types
        if (((buildInfo.shaderType == GpuRt::InternalRayTracingCsType::BuildBVHTD) ||
             (buildInfo.shaderType == GpuRt::InternalRayTracingCsType::BuildBVHTDTR) ||
             (buildInfo.shaderType == GpuRt::InternalRayTracingCsType::BuildParallel) ||
             (buildInfo.shaderType == GpuRt::InternalRayTracingCsType::BuildQBVH)))
        {
            forceWave64 = true;
        }

        result = pDevice->CreateInternalComputePipeline(buildInfo.code.spvSize,
                                                        static_cast<const uint8_t*>(buildInfo.code.pSpvCode),
                                                        buildInfo.nodeCount,
                                                        nodes,
                                                        VK_INTERNAL_SHADER_FLAGS_RAY_TRACING_INTERNAL_SHADER_BIT,
                                                        forceWave64,
                                                        &specializationInfo,
                                                        &pDevice->GetInternalRayTracingPipeline());

        *ppResultPipeline = pDevice->GetInternalRayTracingPipeline().pPipeline[0];

        return result == VK_SUCCESS ? Pal::Result::Success : Pal::Result::ErrorUnknown;
    }
}

// =====================================================================================================================
// Destroy one of gpurt's internal pipelines.
void RayTracingDevice::ClientDestroyInternalComputePipeline(
    const GpuRt::DeviceInitInfo&    initInfo,
    Pal::IPipeline*                 pPipeline,
    void*                           pMemory)
{
    vk::Device* pDevice = reinterpret_cast<vk::Device*>(initInfo.pClientUserData);

    if (pMemory == nullptr)
    {
        pMemory = pPipeline;
    }

    pPipeline->Destroy();
    pPipeline = nullptr;
    pDevice->VkInstance()->FreeMem(pMemory);
}

// =====================================================================================================================
void RayTracingDevice::ClientInsertRGPMarker(
    Pal::ICmdBuffer* pCmdBuffer,
    const char*      pMarker,
    bool             isPush)
{
    vk::CmdBuffer* pCmdbuf = reinterpret_cast<vk::CmdBuffer*>(pCmdBuffer->GetClientData());

    if ((pCmdbuf != nullptr) && (pCmdbuf->GetSqttState() != nullptr))
    {
        pCmdbuf->GetSqttState()->WriteUserEventMarker(
            isPush ? vk::RgpSqttMarkerUserEventPush : vk::RgpSqttMarkerUserEventPop,
            pMarker);
    }
}

// =====================================================================================================================
// Called by GPURT during BVH build/update to request the driver to give it memory wherein to dump the BVH data.
//
// We keep this memory around for later and write it out to files.
Pal::Result RayTracingDevice::ClientAccelStructBuildDumpEvent(
    Pal::ICmdBuffer*                    pPalCmdbuf,
    const GpuRt::AccelStructInfo&       info,
    const GpuRt::AccelStructBuildInfo&  buildInfo,
    Pal::gpusize*                       pDumpGpuVirtAddr)
{
    Pal::Result result = Pal::Result::ErrorOutOfGpuMemory;

    return result;
}

// =====================================================================================================================
// Called by GPURT during BVH build/update to request the driver to give it memory wherein to dump the BVH statistics.
//
// We keep this memory around for later and write it out to files.
Pal::Result RayTracingDevice::ClientAccelStatsBuildDumpEvent(
    Pal::ICmdBuffer*                  pPalCmdbuf,
    GpuRt::AccelStructInfo*           pInfo)
{
    Pal::Result result = Pal::Result::ErrorOutOfGpuMemory;

    return result;
}

// =====================================================================================================================
// Client-provided function to provide exclusive access to a command context handle and command buffer from the client
// to GPURT.
Pal::Result RayTracingDevice::ClientAcquireCmdContext(
    const GpuRt::DeviceInitInfo&    initInfo,     // GpuRt device info
    ClientCmdContextHandle*         pContext,     // (out) Opaque command context handle
    Pal::ICmdBuffer**               ppCmdBuffer)  // (out) Command buffer for GPURT to fill
{
    VK_ASSERT(initInfo.pClientUserData != nullptr);
    VK_ASSERT(ppCmdBuffer              != nullptr);
    VK_ASSERT(pContext                 != nullptr);

    Pal::Result                       result      = Pal::Result::Success;
    vk::Device*                       pDevice     = static_cast<vk::Device*>(initInfo.pClientUserData);
    vk::RayTracingDevice::CmdContext* pCmdContext = pDevice->RayTrace()->GetCmdContext(initInfo.gpuIdx);

    // Defer CmdContext initialization until needed
    if (pCmdContext->pCmdBuffer == nullptr)
    {
        result = pDevice->RayTrace()->InitCmdContext(initInfo.gpuIdx);
    }

    if (result == Pal::Result::Success)
    {
        result = pCmdContext->pCmdBuffer->Reset(nullptr, true);
    }

    if (result == Pal::Result::Success)
    {
        Pal::CmdBufferBuildInfo buildInfo = {};

        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pCmdContext->pCmdBuffer->Begin(buildInfo);
    }

    if (result == Pal::Result::Success)
    {
        result = pCmdContext->pDevice->ResetFences(1, &pCmdContext->pFence);
    }

    if (result == Pal::Result::Success)
    {
        *ppCmdBuffer = pCmdContext->pCmdBuffer;
        *pContext    = reinterpret_cast<ClientCmdContextHandle*>(pCmdContext);
    }

    return result;
}

// =====================================================================================================================
// Client-provided function to submit the context's command buffer and wait for completion.
Pal::Result RayTracingDevice::ClientFlushCmdContext(
    ClientCmdContextHandle      context)
{
    vk::RayTracingDevice::CmdContext* pCmdContext = reinterpret_cast<vk::RayTracingDevice::CmdContext*>(context);

    VK_ASSERT(pCmdContext != nullptr);

    Pal::Result result = pCmdContext->pCmdBuffer->End();

    if (result == Pal::Result::Success)
    {
        Pal::CmdBufInfo            cmdBufInfo               = {};
        Pal::PerSubQueueSubmitInfo perSubQueueSubmitInfo    = {};
        Pal::MultiSubmitInfo       submitInfo               = {};

        perSubQueueSubmitInfo.cmdBufferCount    = 1;
        perSubQueueSubmitInfo.ppCmdBuffers      = &pCmdContext->pCmdBuffer;
        perSubQueueSubmitInfo.pCmdBufInfoList   = &cmdBufInfo;

        submitInfo.pPerSubQueueInfo     = &perSubQueueSubmitInfo;
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.ppFences             = &pCmdContext->pFence;
        submitInfo.fenceCount           = 1;

        result = pCmdContext->pQueue->Submit(submitInfo);
    }

    if (result == Pal::Result::Success)
    {
        result = pCmdContext->pDevice->WaitForFences(1, &pCmdContext->pFence, true, UINT64_MAX);
    }

    return result;
}

// =====================================================================================================================
// Client-provided function to allocate GPU memory
Pal::Result RayTracingDevice::ClientAllocateGpuMemory(
    const GpuRt::DeviceInitInfo&    initInfo,     // GpuRt device info
    uint64                          sizeInBytes,  // Buffer size in bytes
    ClientGpuMemHandle*             pGpuMem,      // (out) GPU video memory
    Pal::gpusize*                   pDestGpuVa,   // (out) Buffer GPU VA
    void**                          ppMappedData) // (out) Map data
{
    VK_ASSERT(initInfo.pClientUserData != nullptr);
    VK_ASSERT(pGpuMem != nullptr);

    Pal::Result         result = Pal::Result::Success;
    vk::Device* pDevice = static_cast<vk::Device*>(initInfo.pClientUserData);
    vk::InternalMemory* pInternalMemory = nullptr;

    void* pSystemMemory = pDevice->VkInstance()->AllocMem(
        sizeof(vk::InternalMemory),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pSystemMemory != nullptr)
    {
        pInternalMemory = VK_PLACEMENT_NEW(pSystemMemory) vk::InternalMemory;
    }
    else
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    if (result == Pal::Result::Success)
    {
        uint32_t                  deviceMask = 1 << initInfo.gpuIdx;
        vk::InternalMemCreateInfo allocInfo = {};

        allocInfo.pal.alignment = PAL_PAGE_BYTES;
        allocInfo.pal.size      = sizeInBytes;
        allocInfo.pal.vaRange   = Pal::VaRange::Default;
        allocInfo.pal.priority  = Pal::GpuMemPriority::Normal;

        if (ppMappedData != nullptr)
        {
            allocInfo.pal.heapCount             = 1;
            allocInfo.pal.heaps[0]              = Pal::GpuHeap::GpuHeapGartCacheable;
            allocInfo.flags.persistentMapped    = true;
        }
        else
        {
            allocInfo.pal.heapCount = 3;
            allocInfo.pal.heaps[0]  = Pal::GpuHeap::GpuHeapInvisible;
            allocInfo.pal.heaps[1]  = Pal::GpuHeap::GpuHeapLocal;
            allocInfo.pal.heaps[2]  = Pal::GpuHeap::GpuHeapGartUswc;
        }

        result = (pDevice->MemMgr()->AllocGpuMem(
            allocInfo,
            pInternalMemory,
            deviceMask,
            VK_OBJECT_TYPE_DEVICE,
            ApiDevice::IntValueFromHandle(ApiDevice::FromObject(pDevice))) == VK_SUCCESS) ?
                Pal::Result::Success : Pal::Result::ErrorUnknown;
    }

    if ((result == Pal::Result::Success) && (ppMappedData != nullptr))
    {
        result = pInternalMemory->Map(initInfo.gpuIdx, ppMappedData);

        if (result != Pal::Result::Success)
        {
            pDevice->MemMgr()->FreeGpuMem(pInternalMemory);
        }
    }

    if (result == Pal::Result::Success)
    {
        *pGpuMem = reinterpret_cast<ClientGpuMemHandle*>(pInternalMemory);

        if (pDestGpuVa != nullptr)
        {
            *pDestGpuVa = pInternalMemory->GpuVirtAddr(initInfo.gpuIdx);
        }
    }
    else
    {
        // Clean up upon failure
        if (pInternalMemory != nullptr)
        {
            Util::Destructor(pInternalMemory);
        }

        pDevice->VkInstance()->FreeMem(pSystemMemory);
    }

    return result;
}

// =====================================================================================================================
// Client-provided function to free GPU memory
void RayTracingDevice::ClientFreeGpuMem(
    const GpuRt::DeviceInitInfo&    initInfo,
    ClientGpuMemHandle              gpuMem)
{
    vk::Device*         pDevice         = reinterpret_cast<vk::Device*>(initInfo.pClientUserData);
    vk::InternalMemory* pInternalMemory = reinterpret_cast<vk::InternalMemory*>(gpuMem);

    VK_ASSERT(pInternalMemory != nullptr);

    pDevice->MemMgr()->FreeGpuMem(pInternalMemory);

    Util::Destructor(pInternalMemory);

    pDevice->VkInstance()->FreeMem(pInternalMemory);
}

}; // namespace vk

#endif
