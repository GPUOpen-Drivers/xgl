/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palVectorImpl.h"
#include "palArchiveFile.h"
#include "gpurt/gpurtLib.h"
#include "g_gpurtOptions.h"
#include "devmode/devmode_mgr.h"

namespace vk
{

// =====================================================================================================================
RayTracingDevice::RayTracingDevice(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_gpurtOptions(pDevice->VkInstance()->Allocator()),
    m_cmdContext(),
    m_pBvhBatchLayer(nullptr),
    m_pSplitRaytracingLayer(nullptr),
    m_pAccelStructAsyncBuildLayer(nullptr),
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
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (InitAccelStructTracker() != VK_SUCCESS)
    {
        // Report soft failure, as this feature is optional
        VK_NEVER_CALLED();
    }

    CreateGpuRtDeviceSettings(&m_gpurtDeviceSettings);
    CollectGpurtOptions(&m_gpurtOptions);

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
            switch (settings.emulatedRtIpLevel)
            {
            case EmulatedRtIpLevelNone:
                break;
            case HardwareRtIpLevel1_1:
            case EmulatedRtIpLevel1_1:
                initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::RtIp1_1;
                break;
            case EmulatedRtIpLevel2_0:
                initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::RtIp2_0;
                break;
#if VKI_BUILD_GFX12
            case EmulatedRtIpLevel3_1:
            case HardwareRtIpLevel3_1:
                initInfo.deviceSettings.emulatedRtIpLevel = Pal::RayTracingIpLevel::RtIp3_1;
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
            callbacks.pfnClientGetTemporaryGpuMemory     = &RayTracingDevice::ClientGetTemporaryGpuMemory;

            result = PalToVkResult(GpuRt::CreateDevice(initInfo, callbacks, pMemory, &m_pGpuRtDevice[deviceIdx]));

            if (result == VK_SUCCESS)
            {
                result = BvhBatchLayer::CreateLayer(m_pDevice, &m_pBvhBatchLayer);
            }

            if (result == VK_SUCCESS)
            {
                result = SplitRaytracingLayer::CreateLayer(m_pDevice, &m_pSplitRaytracingLayer);
            }

            if ((result == VK_SUCCESS) && settings.accelerationStructureAsyncBuild)
            {
                result = AccelStructAsyncBuildLayer::CreateLayer(m_pDevice, &m_pAccelStructAsyncBuildLayer);
            }

            if (result != VK_SUCCESS)
            {
                VK_NEVER_CALLED();

                m_pDevice->VkInstance()->FreeMem(pMemory);

                if (m_pBvhBatchLayer != nullptr)
                {
                    m_pBvhBatchLayer->DestroyLayer();
                }

                if (m_pSplitRaytracingLayer != nullptr)
                {
                    m_pSplitRaytracingLayer->DestroyLayer();
                }

                if (m_pAccelStructAsyncBuildLayer != nullptr)
                {
                    m_pAccelStructAsyncBuildLayer->Destroy();
                }
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

    pDeviceSettings->enableRebraid                = settings.rtEnableRebraid;
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

    pDeviceSettings->fp16BoxModeMixedSaThresh          = Util::Clamp(fp16BoxMixedThreshold, 1.0f, 8.0f);
    pDeviceSettings->enableMortonCode30                = settings.rtEnableMortonCode30;
    pDeviceSettings->mortonFlags                       = settings.mortonFlags;
    pDeviceSettings->enablePrefixScanDLB               = settings.rtEnablePrefixScanDlb;

    switch (settings.rtTriangleCompressionMode)
    {
    case NoTriangleCompression:
        pDeviceSettings->triangleCompressionAutoMode   = GpuRt::TriangleCompressionAutoMode::Disabled;
        break;
    case PairTriangleCompression:
        pDeviceSettings->triangleCompressionAutoMode   = GpuRt::TriangleCompressionAutoMode::AlwaysEnabled;
        break;
    case AutoTriangleCompression:
        pDeviceSettings->triangleCompressionAutoMode   =
            ConvertGpuRtTriCompressionAutoMode(settings.rtTriangleCompressionAutoMode);
        break;
    default:
        VK_NEVER_CALLED();
        pDeviceSettings->triangleCompressionAutoMode   = GpuRt::TriangleCompressionAutoMode::Disabled;
        break;
    }

    pDeviceSettings->bvhBuildModeDefault               = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeDefault);
    pDeviceSettings->bvhBuildModeFastTrace             = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeFastTrace);
    pDeviceSettings->bvhBuildModeFastBuild             = ConvertGpuRtBvhBuildMode(settings.rtBvhBuildModeFastBuild);
    pDeviceSettings->bvhBuildModeOverrideBLAS          = ConvertGpuRtBvhBuildMode(settings.bvhBuildModeOverrideBlas);
    pDeviceSettings->bvhBuildModeOverrideTLAS          = ConvertGpuRtBvhBuildMode(settings.bvhBuildModeOverrideTlas);
    pDeviceSettings->enableParallelUpdate              = settings.rtEnableUpdateParallel;
    pDeviceSettings->enableParallelBuild               = settings.rtEnableBuildParallel;
    pDeviceSettings->parallelBuildWavesPerSimd         = settings.buildParallelWavesPerSimd;
    pDeviceSettings->bvhCpuBuildModeFastTrace          = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);
    pDeviceSettings->bvhCpuBuildModeDefault            = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);
    pDeviceSettings->bvhCpuBuildModeFastBuild          = static_cast<GpuRt::BvhCpuBuildMode>(settings.rtBvhCpuBuildMode);

    pDeviceSettings->enableFusedInstanceNode           = settings.enableFusedInstanceNode;
    pDeviceSettings->rebraidFactor                     = settings.rebraidFactor;
    pDeviceSettings->numRebraidIterations              = settings.numRebraidIterations;
    pDeviceSettings->rebraidQualityHeuristic           = settings.rebraidQualityHeuristicType;
#if VKI_BUILD_GFX12
    pDeviceSettings->rebraidOpenMinPrims               = settings.rebraidOpenMinPrims;
    pDeviceSettings->rebraidOpenSAFactor               = settings.rebraidOpenSurfaceAreaFactor;
#endif
    pDeviceSettings->plocRadius                        = settings.plocRadius;
#if VKI_SUPPORT_HPLOC
    pDeviceSettings->hplocRadius                       = settings.hplocRadius;
#endif
    pDeviceSettings->enablePairCompressionCostCheck    = settings.enablePairCompressionCostCheck;
    pDeviceSettings->accelerationStructureUUID         = GetAccelerationStructureUUID(
                                                     m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties());
    pDeviceSettings->enableMergeSort                   = settings.enableMergeSort;
    pDeviceSettings->lbvhBuildThreshold                = settings.lbvhBuildThreshold;
    pDeviceSettings->enableBVHBuildDebugCounters       = settings.enableBvhBuildDebugCounters;
    pDeviceSettings->enableInsertBarriersInBuildAS     = settings.enableInsertBarriersInBuildAs;
    pDeviceSettings->numMortonSizeBits                 = settings.numMortonSizeBits;
    pDeviceSettings->allowFp16BoxNodesInUpdatableBvh   = settings.rtAllowFp16BoxNodesInUpdatableBvh;
    pDeviceSettings->fp16BoxNodesRequireCompaction     = settings.fp16BoxNodesRequireCompactionFlag;
#if VKI_BUILD_GFX12
    pDeviceSettings->highPrecisionBoxNodeEnable        = settings.rtEnableHighPrecisionBoxNode;
    pDeviceSettings->bvh8Enable                        = settings.rtEnableBvh8;
#endif

#if VKI_BUILD_GFX12
    if (m_pDevice->GetProperties().rayTracingIpLevel >= Pal::RayTracingIpLevel::RtIp3_1)
    {
        pDeviceSettings->enableOrientedBoundingBoxes        = settings.enableOrientedBoundingBoxes;
        pDeviceSettings->boxSplittingFlags                  = settings.boxSplittingFlags;
        pDeviceSettings->obbNumLevels                       = settings.obbNumLevels;
        pDeviceSettings->obbDisableBuildFlags               = settings.obbDisableBuildFlags;
        pDeviceSettings->instanceMode                       = settings.rtBvhInstanceMode;
        pDeviceSettings->primCompressionFlags               = settings.rtPrimCompressionFlags;
        pDeviceSettings->maxPrimRangeSize                   = settings.rtMaxPrimRangeSize;
        pDeviceSettings->enableBvhChannelBalancing          = settings.rtEnableBvhChannelBalancing;
        pDeviceSettings->trivialBuilderMaxPrimThreshold     = settings.rtTrivialBuilderMaxPrimThreshold;
        pDeviceSettings->enableSingleThreadGroupBuild       = settings.rtEnableSingleThreadGroupBuild;
        pDeviceSettings->tlasRefittingMode                  = settings.rtTlasRefittingMode;
    }
#endif

    // Enable AS stats based on panel setting
    pDeviceSettings->enableBuildAccelStructStats        = settings.rtEnableBuildAccelStructStats;

    pDeviceSettings->rgpBarrierReason   = RgpBarrierInternalRayTracingSync;
    m_profileRayFlags                   = TraceRayProfileFlagsToRayFlag(settings);
    m_profileMaxIterations              = TraceRayProfileMaxIterationsToMaxIterations(settings);

    pDeviceSettings->gpuDebugFlags               = settings.rtGpuDebugFlags;
    pDeviceSettings->enableRemapScratchBuffer    = settings.enableRemapScratchBuffer;
    pDeviceSettings->enableEarlyPairCompression  = settings.enableEarlyPairCompression;
    pDeviceSettings->trianglePairingSearchRadius = settings.trianglePairingSearchRadius;

    pDeviceSettings->enableMergedEncodeBuild  = settings.enableMergedEncodeBuild;
    pDeviceSettings->enableMergedEncodeUpdate = settings.enableMergedEncodeUpdate;
    pDeviceSettings->checkBufferOverlapsInBatch = settings.rtCheckBufferOverlapsInBatch;
    pDeviceSettings->disableCompaction          = settings.rtDisableAccelStructCompaction;
    pDeviceSettings->disableRdfCompression      = (settings.enableGpurtRdfCompression == false);
    pDeviceSettings->disableDegenPrims          = settings.disableDegenPrims;
}

// =====================================================================================================================
void RayTracingDevice::CollectGpurtOptions(
    GpurtOptions* const pGpurtOptions
    ) const
{
    const uint32_t optionCount = sizeof(GpuRt::OptionDefaults) / sizeof(GpuRt::OptionDefaults[0]);

    // Set up option defaults so that it won't break when a newly added option has non-zero default.
    Util::HashMap<uint32_t, uint64_t, PalAllocator> optionMap(optionCount, pGpurtOptions->GetAllocator());
    optionMap.Init();
    for (uint32_t i = 0; i < optionCount; i++)
    {
        // We should not have duplicated option defaults.
        VK_ASSERT(optionMap.FindKey(GpuRt::OptionDefaults[i].nameHash) == nullptr);
        optionMap.Insert(GpuRt::OptionDefaults[i].nameHash, GpuRt::OptionDefaults[i].value);
    }

    auto& settings = m_pDevice->GetRuntimeSettings();

    uint32_t threadTraceEnabled = 0;
    if (settings.rtEmitRayTracingShaderDataToken ||
        m_pDevice->VkInstance()->PalPlatform()->IsRaytracingShaderDataTokenRequested())
    {
        threadTraceEnabled = 1;
    }
    *optionMap.FindKey(GpuRt::ThreadTraceEnabledOptionNameHash) = threadTraceEnabled;

    *optionMap.FindKey(GpuRt::PersistentLaunchEnabledOptionNameHash) =
        settings.rtPersistentDispatchRays ? 1 : 0;

    pGpurtOptions->Clear();
    for (auto it = optionMap.Begin(); it.Get() != nullptr; it.Next())
    {
        pGpurtOptions->PushBack({ it.Get()->key, it.Get()->value });
    }
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

    if (m_pBvhBatchLayer != nullptr)
    {
        m_pBvhBatchLayer->DestroyLayer();
    }

    if (m_pSplitRaytracingLayer != nullptr)
    {
        m_pSplitRaytracingLayer->DestroyLayer();
    }

    if (m_pAccelStructAsyncBuildLayer != nullptr)
    {
        m_pAccelStructAsyncBuildLayer->Destroy();
    }

    Util::Destructor(this);

    m_pDevice->VkInstance()->FreeMem(this);
}

// =====================================================================================================================
bool RayTracingDevice::AccelStructTrackerEnabled(
    uint32_t deviceIdx
    ) const
{
    // Enable tracking when forced on in the panel or the GPURT trace source is enabled.
    return ((GetAccelStructTracker(deviceIdx) != nullptr) && (
            m_pGpuRtDevice[deviceIdx]->AccelStructTraceEnabled()));
}

// ====================================================================================================================
// Synchronize Rt Commands for indirect argument generation shader or ray tracing dispatches
void RayTracingDevice::SyncRtCommands(
    Pal::ICmdBuffer* pCmdBuffer,
    RtBarrierMode    barrierMode)
{
    Pal::AcquireReleaseInfo acqRelInfo    = {};
    Pal::MemBarrier         memTransition = {};

    memTransition.srcStageMask  = Pal::PipelineStageCs;
    memTransition.srcAccessMask = Pal::CoherShader;

    switch (barrierMode)
    {
    case RtBarrierMode::Dispatch:
        memTransition.dstStageMask  = Pal::PipelineStageCs;
        memTransition.dstAccessMask = Pal::CoherShader;
        break;
    case RtBarrierMode::IndirectArg:
        memTransition.dstStageMask  = Pal::PipelineStageFetchIndirectArgs;
        memTransition.dstAccessMask = Pal::CoherShader | Pal::CoherIndirectArgs;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    acqRelInfo.pMemoryBarriers    = &memTransition;
    acqRelInfo.memoryBarrierCount = 1;
    acqRelInfo.reason             = RgpBarrierInternalRayTracingSync;

    pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
}

// =====================================================================================================================
bool RayTracingDevice::RayHistoryTraceActive(
    uint32_t deviceIdx
    ) const
{
    return (m_pGpuRtDevice[deviceIdx]->RayHistoryTraceActive() ||
            (m_pDevice->GetRuntimeSettings().rtTraceRayCounterMode != TraceRayCounterDisable));
}

// =====================================================================================================================
GpuRt::TraceRayCounterMode RayTracingDevice::TraceRayCounterMode(
    uint32_t deviceIdx
    ) const
{
    // If the PAL trace path is enabled, then force RayHistoryLight
    return m_pGpuRtDevice[deviceIdx]->RayHistoryTraceAvailable() ?
            GpuRt::TraceRayCounterMode::TraceRayCounterRayHistoryLight :
            static_cast<GpuRt::TraceRayCounterMode>(m_pDevice->GetRuntimeSettings().rtTraceRayCounterMode);
}

// =====================================================================================================================
GpuRt::AccelStructTracker* RayTracingDevice::GetAccelStructTracker(
    uint32_t deviceIdx
    ) const
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
    uint32_t deviceIdx
    ) const
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
#if VKI_BUILD_GFX12
            viewInfo.compressionMode     = m_pDevice->GetBufferViewCompressionMode();
#endif

            // Ensure the SRD size matches with the GPURT header definition
            static_assert(sizeof(pTracker->srd) == sizeof(GpuRt::DispatchRaysTopLevelData::accelStructTrackerSrd),
                            "The size of the AccelStructTracker SRD mismatches between XGL and GPURT.");

            // Ensure the SRD size matches with the size reported by PAL
            VK_ASSERT(sizeof(pTracker->srd) >=
                m_pDevice->VkPhysicalDevice(deviceIdx)->PalProperties().gfxipProperties.srdSizes.untypedBufferView);

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
                   props.gfxipProperties.srdSizes.untypedBufferView);
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

        if (queueHandle == VK_NULL_HANDLE)
        {
            // Could not find a universal queue, try transfer
            cmdBufInfo.engineType = Pal::EngineTypeDma;
            cmdBufInfo.queueType  = Pal::QueueTypeDma;

            queueHandle = m_pDevice->GetQueue(cmdBufInfo.engineType, cmdBufInfo.queueType);
        }
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
    uint64_t                               userMarkerContext,
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
    dispatchInfo.usesNodePtrFlags    = settings.rtEnableNodePointerFlags ? 1 : 0;

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

        dispatchInfo.userMarkerContext        = userMarkerContext;
    }

    (*pDispatchInfo) = dispatchInfo;
}

// =====================================================================================================================
void RayTracingDevice::TraceDispatch(
    uint32_t                               deviceIdx,
    CmdBuffer*                             pCmdBuffer,
    GpuRt::RtPipelineType                  pipelineType,
    uint32_t                               width,
    uint32_t                               height,
    uint32_t                               depth,
    uint32_t                               shaderCount,
    uint64_t                               apiHash,
    uint64_t                               userMarkerContext,
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
                        userMarkerContext,
                        pRaygenSbt,
                        pMissSbt,
                        pHitSbt,
                        &dispatchInfo);

        m_pGpuRtDevice[deviceIdx]->TraceRtDispatch(pCmdBuffer->PalCmdBuffer(deviceIdx),
                                                   pipelineType,
                                                   dispatchInfo,
                                                   pConstants);
    }

}

// =====================================================================================================================
void RayTracingDevice::TraceIndirectDispatch(
    uint32_t                               deviceIdx,
    CmdBuffer*                             pCmdBuffer,
    GpuRt::RtPipelineType                  pipelineType,
    uint32_t                               originalThreadGroupSizeX,
    uint32_t                               originalThreadGroupSizeY,
    uint32_t                               originalThreadGroupSizeZ,
    uint32_t                               shaderCount,
    uint64_t                               apiHash,
    uint64_t                               userMarkerContext,
    const VkStridedDeviceAddressRegionKHR* pRaygenSbt,
    const VkStridedDeviceAddressRegionKHR* pMissSbt,
    const VkStridedDeviceAddressRegionKHR* pHitSbt,
    Pal::gpusize*                          pCounterMetadataVa,
    void*                                  pConstants)
{
    GpuRt::RtDispatchInfo dispatchInfo = {};

    SetDispatchInfo(pipelineType,
                    0,
                    0,
                    0,
                    shaderCount,
                    apiHash,
                    userMarkerContext,
                    pRaygenSbt,
                    pMissSbt,
                    pHitSbt,
                    &dispatchInfo);

    dispatchInfo.threadGroupSizeX = originalThreadGroupSizeX;
    dispatchInfo.threadGroupSizeY = originalThreadGroupSizeY;
    dispatchInfo.threadGroupSizeZ = originalThreadGroupSizeZ;

    if (m_pGpuRtDevice[deviceIdx]->RayHistoryTraceActive())
    {
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
    ClientPipelineHandle*               pResultPipeline,       ///< [out] Result PAL pipeline object pointer
    void**                              ppResultMemory)        ///< [out] (Optional) Result PAL pipeline memory,
                                                               ///< if different from obj
{
    vk::Device* pDevice  = static_cast<vk::Device*>(initInfo.pClientUserData);
    const auto& settings = pDevice->GetRuntimeSettings();

    *ppResultMemory = nullptr;

    VkResult result = VK_SUCCESS;

    vk::PipelineCompiler* pCompiler     = pDevice->GetCompiler(initInfo.gpuIdx);
    vk::ShaderModuleHandle shaderModule = {};
    const void* pPipelineBinary         = nullptr;
    size_t pipelineBinarySize           = 0;

    Vkgc::BinaryData spvBin =
        {
            .codeSize = buildInfo.code.spvSize,
            .pCode    = buildInfo.code.pSpvCode
        };

    // The "+ 1" is for the possible debug printf user node
    Vkgc::ResourceMappingRootNode nodes[GpuRt::MaxInternalPipelineNodes + 1]    = {};
    Vkgc::ResourceMappingNode subNodes[GpuRt::MaxInternalPipelineNodes + 1]     = {};
    uint32_t subNodeIndex = 0;
    const uint32_t typedBufferSrdSizeDw   =
        pDevice->GetProperties().descriptorSizes.typedBufferView / sizeof(uint32_t);
    const uint32_t untypedBufferSrdSizeDw =
        pDevice->GetProperties().descriptorSizes.untypedBufferView / sizeof(uint32_t);

    const uint32_t imageBufferSrdSizeDw = pDevice->GetProperties().descriptorSizes.imageView / sizeof(uint32_t);
    uint32_t alignment = Util::Lcm(typedBufferSrdSizeDw, untypedBufferSrdSizeDw);
    alignment = Util::Lcm(alignment, imageBufferSrdSizeDw);
    const uint32_t maxBufferTableSize = Util::RoundDownToMultiple(UINT_MAX, alignment);

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
        else if (node.type == GpuRt::NodeType::Srv)
        {
            nodes[nodeIndex].node.type = Vkgc::ResourceMappingNodeType::DescriptorResource;

            if (node.srdStride == 2)
            {
                nodes[nodeIndex].node.type = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
            }
            else if (node.srdStride == 4)
            {
                nodes[nodeIndex].node.type = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
            }

            nodes[nodeIndex].node.sizeInDwords      = node.dwSize;
            nodes[nodeIndex].node.offsetInDwords    = node.dwOffset;
            nodes[nodeIndex].node.srdRange.set      = node.descSet;
            nodes[nodeIndex].node.srdRange.binding  = node.binding;
        }
        else if ((node.type == GpuRt::NodeType::ConstantBufferTable) ||
                    (node.type == GpuRt::NodeType::UavTable) ||
                    (node.type == GpuRt::NodeType::TypedUavTable) ||
                    (node.type == GpuRt::NodeType::SrvTable) ||
                    (node.type == GpuRt::NodeType::TypedSrvTable))
        {
            Vkgc::ResourceMappingNode* pSubNode      = &subNodes[subNodeIndex++];
            nodes[nodeIndex].node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
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
            case GpuRt::NodeType::SrvTable:
                pSubNode->type = Vkgc::ResourceMappingNodeType::DescriptorResource;
                pSubNode->srdRange.strideInDwords = untypedBufferSrdSizeDw;
                break;
            case GpuRt::NodeType::TypedSrvTable:
                pSubNode->type = Vkgc::ResourceMappingNodeType::DescriptorResource;
                pSubNode->srdRange.strideInDwords = typedBufferSrdSizeDw;
                break;
            default:
                VK_NEVER_CALLED();
            }
            pSubNode->offsetInDwords    = 0;
            pSubNode->srdRange.set      = node.descSet;
            pSubNode->srdRange.binding  = node.binding;
            pSubNode->sizeInDwords      = maxBufferTableSize;
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

    constexpr uint32_t CompilerOptionWaveSize      = Util::HashLiteralString("waveSize");
    constexpr uint32_t CompilerOptionValueWave32   = Util::HashLiteralString("Wave32");
    constexpr uint32_t CompilerOptionValueWave64   = Util::HashLiteralString("Wave64");

    ShaderWaveSize waveSize = ShaderWaveSize::WaveSizeAuto;

    for (uint32_t i = 0; i < buildInfo.hashedCompilerOptionCount; ++i)
    {
        const GpuRt::PipelineCompilerOption& compilerOption = buildInfo.pHashedCompilerOptions[i];

        switch (compilerOption.hashedOptionName)
        {
        case CompilerOptionWaveSize:
            if (compilerOption.value == CompilerOptionValueWave32)
            {
                waveSize = ShaderWaveSize::WaveSize32;
            }
            else if (compilerOption.value == CompilerOptionValueWave64)
            {
                waveSize = ShaderWaveSize::WaveSize64;
            }
        break;
        default:
            VK_ASSERT_ALWAYS_MSG("Unknown GPURT setting! Handle it!");
        }
    }

    uint32_t nodeCount = buildInfo.nodeCount;
    if (pDevice->GetEnabledFeatures().enableDebugPrintf)
    {
        uint32_t debugPrintfOffset = nodes[nodeCount - 1].node.offsetInDwords +
            nodes[nodeCount - 1].node.sizeInDwords;

        PipelineLayout::BuildLlpcDebugPrintfMapping(
            Vkgc::ShaderStageComputeBit,
            debugPrintfOffset,
            1u,
            &nodes[nodeCount],
            &nodeCount,
            &subNodes[subNodeIndex],
            &subNodeIndex);
    }

    result = pDevice->CreateInternalComputePipeline(spvBin.codeSize,
                                                    static_cast<const uint8_t*>(spvBin.pCode),
                                                    nodeCount,
                                                    nodes,
                                                    ShaderModuleInternalRayTracingShader,
                                                    waveSize,
                                                    &specializationInfo,
                                                    &pDevice->GetInternalRayTracingPipeline());

    *pResultPipeline = pDevice->GetInternalRayTracingPipeline().pPipeline[0];

    return result == VK_SUCCESS ? Pal::Result::Success : Pal::Result::ErrorUnknown;
}

// =====================================================================================================================
// Destroy one of gpurt's internal pipelines.
void RayTracingDevice::ClientDestroyInternalComputePipeline(
    const GpuRt::DeviceInitInfo&    initInfo,
    ClientPipelineHandle            pipeline,
    void*                           pMemory)
{
    vk::Device* pDevice = static_cast<vk::Device*>(initInfo.pClientUserData);
    Pal::IPipeline* pPipeline = static_cast<Pal::IPipeline*>(pipeline);

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
    ClientCmdBufferHandle   cmdBuffer,
    const char*             pMarker,
    bool                    isPush)
{
    Pal::ICmdBuffer* pPalCmdbuf = static_cast<Pal::ICmdBuffer*>(cmdBuffer);
    vk::CmdBuffer* pCmdbuf = static_cast<vk::CmdBuffer*>(pPalCmdbuf->GetClientData());

    if (pCmdbuf != nullptr)
    {
        if (pCmdbuf->GetSqttState() != nullptr)
        {
            pCmdbuf->GetSqttState()->WriteUserEventMarker(
                isPush ? vk::RgpSqttMarkerUserEventPush : vk::RgpSqttMarkerUserEventPop,
                pMarker);
        }

        pCmdbuf->InsertDebugMarker(pMarker, isPush);
    }
}

// =====================================================================================================================
// Called by GPURT during BVH build/update to request the driver to give it memory wherein to dump the BVH data.
//
// We keep this memory around for later and write it out to files.
Pal::Result RayTracingDevice::ClientAccelStructBuildDumpEvent(
    ClientCmdBufferHandle               cmdbuf,
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
    ClientCmdBufferHandle             cmdbuf,
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
    ClientCmdBufferHandle*          pCmdBuffer)   // (out) Command buffer for GPURT to fill
{
    VK_ASSERT(initInfo.pClientUserData != nullptr);
    VK_ASSERT(pCmdBuffer               != nullptr);
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
        *pCmdBuffer = pCmdContext->pCmdBuffer;
        *pContext   = reinterpret_cast<ClientCmdContextHandle*>(pCmdContext);
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
        result = pCmdContext->pDevice->WaitForFences(1, &pCmdContext->pFence, true, std::chrono::nanoseconds::max());
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
// Client-provided function to get temporary mapped GPU memory
Pal::Result RayTracingDevice::ClientGetTemporaryGpuMemory(
    ClientCmdBufferHandle cmdbuf,       // PAL command buffer that will handle the allocation
    uint64                sizeInBytes,  // Buffer size in bytes
    Pal::gpusize*         pDestGpuVa,   // (out) Buffer GPU VA
    void**                ppMappedData) // (out) Map data
{
    Pal::Result         result      = Pal::Result::ErrorOutOfGpuMemory;
    Pal::ICmdBuffer*    pPalCmdbuf  = static_cast<Pal::ICmdBuffer*>(cmdbuf);
    vk::CmdBuffer*      pCmdbuf     = static_cast<CmdBuffer*>(pPalCmdbuf->GetClientData());
    VK_ASSERT(pCmdbuf != nullptr);
    vk::Device*         pDevice     = pCmdbuf->VkDevice();

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (pCmdbuf->PalCmdBuffer(deviceIdx) != pPalCmdbuf)
            continue;

        InternalMemory* pVidMem = nullptr;
        if (pCmdbuf->GetScratchVidMem(sizeInBytes, InternalPoolDescriptorTable, &pVidMem) == VK_SUCCESS)
        {
            if (pVidMem != nullptr)
            {
                if (pVidMem->Map(deviceIdx, ppMappedData) == Pal::Result::Success)
                {
                    *pDestGpuVa = pVidMem->GpuVirtAddr(deviceIdx);
                    result = Pal::Result::Success;
                }
                else
                {
                    result = Pal::Result::ErrorNotMappable;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Client-provided function to free GPU memory
void RayTracingDevice::ClientFreeGpuMem(
    const GpuRt::DeviceInitInfo&    initInfo,
    ClientGpuMemHandle              gpuMem)
{
    vk::Device*         pDevice         = static_cast<vk::Device*>(initInfo.pClientUserData);
    vk::InternalMemory* pInternalMemory = static_cast<vk::InternalMemory*>(gpuMem);

    VK_ASSERT(pInternalMemory != nullptr);

    pDevice->MemMgr()->FreeGpuMem(pInternalMemory);

    Util::Destructor(pInternalMemory);

    pDevice->VkInstance()->FreeMem(pInternalMemory);
}

}; // namespace vk

#endif
