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
 * @file  devmode_ubertrace.cpp
 * @brief Contains UberTrace implementation of the GPU Open Developer Mode manager
 ***********************************************************************************************************************
 */

// Vulkan headers
#include "devmode/devmode_ubertrace.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_graphics_pipeline_library.h"
#include "include/vk_physical_device.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "include/pipeline_binary_cache.h"
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

// PAL headers
#include "pal.h"
#include "palAutoBuffer.h"
#include "palRenderOpTraceController.h"
#include "palCodeObjectTraceSource.h"
#include "palHashMapImpl.h"
#include "palQueueTimingsTraceSource.h"
#include "palStringTableTraceSource.h"
#include "palUserMarkerHistoryTraceSource.h"
#include "palVectorImpl.h"

// gpuopen headers
#include "devDriverServer.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/driverControlServer.h"
#include "protocols/ddPipelineUriService.h"
#include "protocols/ddEventServer.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_ray_tracing_pipeline.h"
#endif

namespace vk
{

class DevModeUberTraceStringTableTraceSource : public GpuUtil::StringTableTraceSource
{
public:
    DevModeUberTraceStringTableTraceSource(Pal::IPlatform* pPlatform, DevModeUberTrace* pDevMode)
        : StringTableTraceSource(pPlatform), m_pDevMode(pDevMode) {}
    virtual ~DevModeUberTraceStringTableTraceSource() {}

    virtual void OnTraceFinished() override
    {
        const auto& accelStructNames = m_pDevMode->GetAccelStructUserMarkerTable();
        const uint32_t numStrings = accelStructNames.GetNumEntries();

        if (numStrings > 0)
        {
            // Calculate the size of the string data for accelStruct names
            uint32_t stringDataSizeInBytes = 0;
            for (auto it = accelStructNames.Begin(); it.Get() != nullptr; it.Next())
            {
                stringDataSizeInBytes += it.Get()->value.length;
            }

            const uint32_t baseOffset = sizeof(uint32_t) * numStrings;
            AutoBuffer<uint32_t, 16, Pal::IPlatform> stringOffsets(numStrings, m_pPlatform);
            Vector<char, 128, Pal::IPlatform> stringData(m_pPlatform);
            constexpr uint32_t ExtraBytesPerString = 18;    // To host "RRA_RA:<address>:"
            // Reserve more space because we are storing strings in the format of "RRA_AS:<address>:<label>"
            stringData.Reserve(stringDataSizeInBytes + numStrings * ExtraBytesPerString);

            // Filling stringData and stringOffsets
            static constexpr char AsPatternStr[] = "RRA_AS:%llu:%s";
            constexpr uint32_t MaxStringLength = 128;
            char formatedString[MaxStringLength];
            uint32_t stringIdx = 0;
            uint32_t offset = 0;
            for (auto it = accelStructNames.Begin(); it.Get() != nullptr; it.Next())
            {
                uint64_t acAddr = it.Get()->key;
                const char* pAcLabel = it.Get()->value.string;

                uint32_t len = Util::Snprintf(formatedString, sizeof(formatedString),
                    AsPatternStr, acAddr, pAcLabel) + 1;
                stringData.Resize(stringData.size() + len);
                memcpy(stringData.Data() + offset, formatedString, len);
                stringOffsets[stringIdx] = offset + baseOffset;
                offset += len;
                stringIdx++;
            }

            uint32_t tableId = AcquireTableId();
            AddStringTable(tableId, numStrings, stringOffsets.Data(), stringData.Data(), stringData.size());
        }

        StringTableTraceSource::OnTraceFinished();
    }

private:
    DevModeUberTrace* m_pDevMode;
};

// =====================================================================================================================
DevModeUberTrace::DevModeUberTrace(
    Instance* pInstance)
    :
    m_pInstance(pInstance),
    m_pDevDriverServer(pInstance->PalPlatform()->GetDevDriverServer()),
    m_finalized(false),
    m_crashAnalysisEnabled(false),
    m_globalFrameIndex(1), // Must start from 1 according to RGP spec
    m_pTraceSession(pInstance->PalPlatform()->GetTraceSession()),
    m_pCodeObjectTraceSource(nullptr),
    m_pQueueTimingsTraceSource(nullptr),
    m_pStringTableTraceSource(nullptr),
    m_pUserMarkerHistoryTraceSource(nullptr),
    m_pRenderOpTraceController(nullptr),
    m_accelStructNames(64, m_pInstance->Allocator())
{
    m_accelStructNames.Init();
}

// =====================================================================================================================
DevModeUberTrace::~DevModeUberTrace()
{
    DestroyUberTraceResources();
}

// =====================================================================================================================
// Creates the UberTrace GPU Open Developer Mode manager class.
VkResult DevModeUberTrace::Create(
    Instance*              pInstance,
    DevModeUberTrace**     ppObject)
{
    Pal::Result result = Pal::Result::Success;

    void* pStorage = pInstance->AllocMem(sizeof(DevModeUberTrace), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pStorage != nullptr)
    {
        DevModeUberTrace* pMgr = VK_PLACEMENT_NEW(pStorage) DevModeUberTrace(pInstance);

        if (result == Pal::Result::Success)
        {
            *ppObject = pMgr;
        }
        else
        {
            pMgr->Destroy();
        }
    }
    else
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
void DevModeUberTrace::Finalize(
    uint32_t              deviceCount,
    VulkanSettingsLoader* settingsLoaders[])
{
    m_pDevDriverServer->GetDriverControlServer()->StartLateDeviceInit();

    // Finalize the devmode manager
    m_pDevDriverServer->Finalize();

    m_crashAnalysisEnabled = m_pInstance->PalPlatform()->IsCrashAnalysisModeEnabled();

    m_finalized = true;
}

// =====================================================================================================================
void DevModeUberTrace::Destroy()
{
    Util::Destructor(this);
    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
void DevModeUberTrace::NotifyFrameBegin(
    const Queue*       pQueue,
    FrameDelimiterType delimiterType)
{
    // Wait for the driver to be resumed in case it's been paused.
    WaitForDriverResume();
}

// =====================================================================================================================
void DevModeUberTrace::NotifyFrameEnd(
    const Queue*       pQueue,
    FrameDelimiterType delimiterType)
{
    if (IsQueueTimingActive(pQueue->VkDevice()))
    {
        // Call TimedQueuePresent() to insert commands that collect GPU timestamp.
        Pal::IQueue* pPalQueue = pQueue->PalQueue(DefaultDeviceIndex);

        // Currently nothing in the PresentInfo struct is used for inserting a timed present marker.
        GpuUtil::TimedQueuePresentInfo timedPresentInfo = {};
        Pal::Result result = m_pQueueTimingsTraceSource->TimedQueuePresent(pPalQueue, timedPresentInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    m_globalFrameIndex++;
}

// =====================================================================================================================
// Waits for the driver to be resumed if it's currently paused.
void DevModeUberTrace::WaitForDriverResume()
{
    auto* pDriverControlServer = m_pDevDriverServer->GetDriverControlServer();

    VK_ASSERT(pDriverControlServer != nullptr);
    pDriverControlServer->DriverTick();
}

// =====================================================================================================================
void DevModeUberTrace::PipelineCreated(
    Device*   pDevice,
    Pipeline* pPipeline)
{
    if (m_pCodeObjectTraceSource != nullptr)
    {
        GpuUtil::RegisterPipelineInfo pipelineInfo = { 0 };
        pipelineInfo.apiPsoHash = pPipeline->GetApiHash();
        if (pPipeline->PalPipeline(DefaultDeviceIndex) != nullptr)
        {
            bool isGplPipeline = false;
            GraphicsPipeline* pGraphicsPipeline = nullptr;
            if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_GRAPHICS)
            {
                pGraphicsPipeline = reinterpret_cast<GraphicsPipeline*>(pPipeline);
                isGplPipeline = pGraphicsPipeline->GetPalShaderLibrary(GraphicsLibraryPreRaster) != nullptr;
            }

            if (isGplPipeline)
            {
                GpuUtil::RegisterLibraryInfo libInfo = { pipelineInfo.apiPsoHash };
                for (uint32_t i = 0; i < GraphicsLibraryCount; i++)
                {
                    const Pal::IShaderLibrary* pLib =
                        pGraphicsPipeline->GetPalShaderLibrary(static_cast<GraphicsLibraryType>(i));
                    if (pLib != nullptr)
                    {
                        m_pCodeObjectTraceSource->RegisterLibrary(pLib, libInfo);
                    }
                }
            }
            else
            {
                m_pCodeObjectTraceSource->RegisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex), pipelineInfo);
            }
        }
    }
}

// =====================================================================================================================
void DevModeUberTrace::PipelineDestroyed(
    Device*   pDevice,
    Pipeline* pPipeline)
{
    if (m_pCodeObjectTraceSource != nullptr)
    {
        if (pPipeline->PalPipeline(DefaultDeviceIndex) != nullptr)
        {
            bool isGplPipeline = false;
            if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_GRAPHICS)
            {
                GraphicsPipeline*  pGraphicsPipeline = reinterpret_cast<GraphicsPipeline*>(pPipeline);
                isGplPipeline = pGraphicsPipeline->GetPalShaderLibrary(GraphicsLibraryPreRaster) != nullptr;
            }

            if (isGplPipeline == false)
            {
                m_pCodeObjectTraceSource->UnregisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex));
            }
        }
        else
        {
            if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_GRAPHICS)
            {
                GraphicsPipelineLibrary* pGraphicsLibrary = reinterpret_cast<GraphicsPipelineLibrary*>(pPipeline);
                const Pal::IShaderLibrary* pPalLibraries[GraphicsLibraryCount] = {};
                pGraphicsLibrary->GetOwnedPalShaderLibraries(pPalLibraries);
                for (uint32_t i = 0; i < GraphicsLibraryCount; i++)
                {
                    if (pPalLibraries[i] != nullptr)
                    {
                        m_pCodeObjectTraceSource->UnregisterLibrary(pPalLibraries[i]);
                    }
                }
            }
        }
    }
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void DevModeUberTrace::ShaderLibrariesCreated(
    Device*             pDevice,
    RayTracingPipeline* pPipeline)
{
    if (m_pCodeObjectTraceSource != nullptr)
    {
        for (uint32_t i = 0; i < pPipeline->GetShaderLibraryCount(); ++i)
        {
            GpuUtil::RegisterLibraryInfo pipelineInfo = { pPipeline->GetApiHash() };
            m_pCodeObjectTraceSource->RegisterLibrary(pPipeline->PalShaderLibrary(i), pipelineInfo);
        }
    }
}

// =====================================================================================================================
void DevModeUberTrace::ShaderLibrariesDestroyed(
    Device*             pDevice,
    RayTracingPipeline* pPipeline)
{
    if (m_pCodeObjectTraceSource != nullptr)
    {
        for (uint32_t i = 0; i < pPipeline->GetShaderLibraryCount(); ++i)
        {
            m_pCodeObjectTraceSource->UnregisterLibrary(pPipeline->PalShaderLibrary(i));
        }
    }
}
#endif

// =====================================================================================================================
Pal::Result DevModeUberTrace::RegisterQueuesForDevice(
    Device* pDevice)
{
    Pal::Result result = Pal::Result::Success;

    for (uint32_t familyIdx = 0; familyIdx < Queue::MaxQueueFamilies; ++familyIdx)
    {
        for (uint32_t queueIdx = 0;
            (queueIdx < Queue::MaxQueuesPerFamily) && (result == Pal::Result::Success);
            ++queueIdx)
        {
            VkQueue queueHandle = VK_NULL_HANDLE;
            pDevice->GetQueue(familyIdx, queueIdx, &queueHandle);

            if (queueHandle != VK_NULL_HANDLE)
            {
                Queue* pQueue = ApiQueue::ObjectFromHandle(queueHandle);
                Pal::IQueue* pPalQueue = pQueue->PalQueue(DefaultDeviceIndex);

                // Get the OS context handle for this queue (this is a thing that RGP needs on DX clients;
                // it may be optional for Vulkan, but we provide it anyway if available).
                Pal::KernelContextInfo kernelCxtInfo = {};
                Pal::Result resultQueryKernel = pPalQueue->QueryKernelContextInfo(&kernelCxtInfo);

                uint64_t queueId = reinterpret_cast<uint64_t>(ApiQueue::FromObject(pQueue));
                uint64_t queueContext = (resultQueryKernel == Pal::Result::Success)
                                      ? kernelCxtInfo.contextIdentifier
                                      : 0;

                result = m_pQueueTimingsTraceSource->RegisterTimedQueue(pPalQueue, queueId, queueContext);
            }
        }
    }

    return result;
}

// =====================================================================================================================
void DevModeUberTrace::PostDeviceCreate(
    Device* pDevice)
{
    Pal::Result result = InitUberTraceResources(pDevice->PalDevice(DefaultDeviceIndex));

    if (result == Pal::Result::Success)
    {
        result = RegisterQueuesForDevice(pDevice);
    }

    VK_ASSERT(result == Pal::Result::Success);

    auto* pDriverControlServer = m_pDevDriverServer->GetDriverControlServer();

    VK_ASSERT(pDriverControlServer != nullptr);

    // If the driver hasn't been marked as fully initialized yet, mark it now. We consider the time after the logical
    // device creation to be the fully initialized driver position. This is mainly because PAL is fully initialized
    // at this point and we also know whether or not the debug vmid has been acquired. External tools use this
    // information to decide when it's reasonable to make certain requests of the driver through protocol functions.
    if (pDriverControlServer->IsDriverInitialized() == false)
    {
        pDriverControlServer->FinishDeviceInit();
    }
}

// =====================================================================================================================
bool DevModeUberTrace::IsTracingEnabled() const
{
    return m_pTraceSession->IsTracingEnabled();
}

// =====================================================================================================================
void DevModeUberTrace::RecordRenderOps(
    uint32_t deviceIdx,
    Queue*   pQueue,
    uint32_t drawCallCount,
    uint32_t dispatchCallCount)
{
    if (m_pInstance->PalPlatform()->GetTraceSession()->GetActiveController() == m_pRenderOpTraceController)
    {
        Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

        GpuUtil::RenderOpCounts opCounts = {
            .drawCount     = drawCallCount,
            .dispatchCount = dispatchCallCount,
        };

        m_pRenderOpTraceController->RecordRenderOps(pPalQueue, opCounts);
    }
}

// =====================================================================================================================
Pal::Result DevModeUberTrace::TimedQueueSubmit(
    uint32_t               deviceIdx,
    Queue*                 pQueue,
    uint32_t               cmdBufferCount,
    const VkCommandBuffer* pCommandBuffers,
    const Pal::SubmitInfo& submitInfo,
    VirtualStackFrame*     pVirtStackFrame)
{
    VK_ASSERT(cmdBufferCount == submitInfo.pPerSubQueueInfo[0].cmdBufferCount);

    bool timingSupported = IsQueueTimingActive(pQueue->VkDevice()) && (submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);

    // Fill in extra meta-data information to associate the API command buffer data with the generated
    // timing information.
    GpuUtil::TimedSubmitInfo timedSubmitInfo = {};
    Pal::uint64* pApiCmdBufIds  = nullptr;
    Pal::uint32* pSqttCmdBufIds = nullptr;

    if (timingSupported)
    {
        pApiCmdBufIds  = pVirtStackFrame->AllocArray<Pal::uint64>(cmdBufferCount);
        pSqttCmdBufIds = pVirtStackFrame->AllocArray<Pal::uint32>(cmdBufferCount);

        timedSubmitInfo.pApiCmdBufIds  = pApiCmdBufIds;
        timedSubmitInfo.pSqttCmdBufIds = pSqttCmdBufIds;
        timedSubmitInfo.frameIndex     = m_globalFrameIndex;

        timingSupported &= (pApiCmdBufIds != nullptr) && (pSqttCmdBufIds != nullptr);
    }

    Pal::Result result = Pal::Result::NotReady;

    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    if (timingSupported)
    {
        for (uint32_t cbIdx = 0; cbIdx < cmdBufferCount; ++cbIdx)
        {
            uintptr_t intHandle = reinterpret_cast<uintptr_t>(pCommandBuffers[cbIdx]);

            pApiCmdBufIds[cbIdx] = intHandle;

            CmdBuffer* pCmdBuf = ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[cbIdx]);

            pSqttCmdBufIds[cbIdx] = 0;

            if (pCmdBuf->GetSqttState() != nullptr)
            {
                pSqttCmdBufIds[cbIdx] = pCmdBuf->GetSqttState()->GetId().u32All;
            }

            VK_ASSERT(pCmdBuf->PalCmdBuffer(DefaultDeviceIndex) == submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[cbIdx]);
        }

        // Do a timed submit of all the command buffers
        result = m_pQueueTimingsTraceSource->TimedSubmit(pPalQueue, submitInfo, timedSubmitInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    // Punt to non-timed submit if a timed submit fails (or is not supported)
    if (result != Pal::Result::Success)
    {
        result = Queue::PalQueueSubmit(pQueue->VkDevice(), pPalQueue, submitInfo);
    }

    if (pApiCmdBufIds != nullptr)
    {
        pVirtStackFrame->FreeArray(pApiCmdBufIds);
    }

    if (pSqttCmdBufIds != nullptr)
    {
        pVirtStackFrame->FreeArray(pSqttCmdBufIds);
    }

    return result;
}

// =====================================================================================================================
Pal::Result DevModeUberTrace::TimedSignalQueueSemaphore(
    uint32_t              deviceIdx,
    Queue*                pQueue,
    VkSemaphore           semaphore,
    uint64_t              value,
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (IsQueueTimingActive(pQueue->VkDevice()))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;
        result = m_pQueueTimingsTraceSource->TimedSignalQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->SignalQueueSemaphore(pQueueSemaphore, value);
    }

    return result;
}

// =====================================================================================================================
Pal::Result DevModeUberTrace::TimedWaitQueueSemaphore(
    uint32_t              deviceIdx,
    Queue*                pQueue,
    VkSemaphore           semaphore,
    uint64_t              value,
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (IsQueueTimingActive(pQueue->VkDevice()))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;
        result = m_pQueueTimingsTraceSource->TimedWaitQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->WaitQueueSemaphore(pQueueSemaphore, value);
    }

    return result;
}

// =====================================================================================================================
bool DevModeUberTrace::IsQueueTimingActive(
    const Device* /*pDevice*/
    ) const
{
    return (m_pQueueTimingsTraceSource != nullptr) ? m_pQueueTimingsTraceSource->IsTimingInProgress() : false;
}

// =====================================================================================================================
bool DevModeUberTrace::IsTraceRunning() const
{
    return m_pTraceSession->GetTraceSessionState() == GpuUtil::TraceSessionState::Running;
}

// =====================================================================================================================
Pal::Result DevModeUberTrace::InitUberTraceResources(
    Pal::IDevice* pPalDevice)
{
    Pal::Result result = Pal::Result::ErrorOutOfMemory;

    const size_t traceObjectsAllocSize = sizeof(GpuUtil::CodeObjectTraceSource) +
                                         sizeof(GpuUtil::QueueTimingsTraceSource) +
                                         sizeof(DevModeUberTraceStringTableTraceSource) +
                                         sizeof(GpuUtil::UserMarkerHistoryTraceSource) +
                                         sizeof(GpuUtil::RenderOpTraceController);

    void* pStorage = m_pInstance->AllocMem(traceObjectsAllocSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pStorage != nullptr)
    {
        // Emplace trace objects into the memory raft
        void* pObjStorage = pStorage;

        m_pCodeObjectTraceSource = VK_PLACEMENT_NEW(pObjStorage)
                                   GpuUtil::CodeObjectTraceSource(m_pInstance->PalPlatform());

        pObjStorage = VoidPtrInc(pObjStorage, sizeof(GpuUtil::CodeObjectTraceSource));

        m_pQueueTimingsTraceSource = VK_PLACEMENT_NEW(pObjStorage)
                                     GpuUtil::QueueTimingsTraceSource(m_pInstance->PalPlatform());

        pObjStorage = VoidPtrInc(pObjStorage, sizeof(GpuUtil::QueueTimingsTraceSource));

        m_pStringTableTraceSource = VK_PLACEMENT_NEW(pObjStorage)
                                    DevModeUberTraceStringTableTraceSource(m_pInstance->PalPlatform(), this);

        pObjStorage = VoidPtrInc(pObjStorage, sizeof(DevModeUberTraceStringTableTraceSource));

        m_pUserMarkerHistoryTraceSource = VK_PLACEMENT_NEW(pObjStorage)
                                          GpuUtil::UserMarkerHistoryTraceSource(m_pInstance->PalPlatform());

        pObjStorage = VoidPtrInc(pObjStorage, sizeof(GpuUtil::UserMarkerHistoryTraceSource));

        m_pRenderOpTraceController = VK_PLACEMENT_NEW(pObjStorage)
                                     GpuUtil::RenderOpTraceController(m_pInstance->PalPlatform(), pPalDevice);

        // Register and initialize created trace objects
        result = m_pTraceSession->RegisterSource(m_pCodeObjectTraceSource);

        if (result == Pal::Result::Success)
        {
            result = m_pQueueTimingsTraceSource->Init(pPalDevice);
        }
        if (result == Pal::Result::Success)
        {
            result = m_pTraceSession->RegisterSource(m_pQueueTimingsTraceSource);
        }
        if (result == Pal::Result::Success)
        {
            result = m_pTraceSession->RegisterSource(m_pStringTableTraceSource);
        }
        if (result == Pal::Result::Success)
        {
            result = m_pTraceSession->RegisterSource(m_pUserMarkerHistoryTraceSource);
        }
        if (result == Pal::Result::Success)
        {
            result = m_pTraceSession->RegisterController(m_pRenderOpTraceController);
        }

        if (result != Pal::Result::Success)
        {
            DestroyUberTraceResources();
        }
    }

    return result;
}

// =====================================================================================================================
void DevModeUberTrace::DestroyUberTraceResources()
{
    if (m_pUserMarkerHistoryTraceSource != nullptr)
    {
        m_pTraceSession->UnregisterSource(m_pUserMarkerHistoryTraceSource);
    }
    if (m_pStringTableTraceSource != nullptr)
    {
        m_pTraceSession->UnregisterSource(m_pStringTableTraceSource);
    }
    if (m_pQueueTimingsTraceSource != nullptr)
    {
        m_pTraceSession->UnregisterSource(m_pQueueTimingsTraceSource);
    }
    if (m_pCodeObjectTraceSource != nullptr)
    {
        m_pTraceSession->UnregisterSource(m_pCodeObjectTraceSource);
    }
    if (m_pRenderOpTraceController != nullptr)
    {
        m_pTraceSession->UnregisterController(m_pRenderOpTraceController);
    }

    // The trace objects are allocated in a single memory allocation
    // Free the 'head' of this allocation to free all of them
    m_pInstance->FreeMem(m_pCodeObjectTraceSource);

    m_pUserMarkerHistoryTraceSource = nullptr;
    m_pStringTableTraceSource = nullptr;
    m_pQueueTimingsTraceSource = nullptr;
    m_pCodeObjectTraceSource = nullptr;
    m_pRenderOpTraceController = nullptr;
}

// =====================================================================================================================
void DevModeUberTrace::ProcessMarkerTable(
    uint32        sqttCbId,
    uint32        numOps,
    const uint32* pUserMarkerOpHistory,
    uint32        numMarkerStrings,
    const uint32* pMarkerStringOffsets,
    uint32        markerStringDataSize,
    const char*   pMarkerStringData)
{
    uint32_t tableId = m_pStringTableTraceSource->AcquireTableId();

    m_pStringTableTraceSource->AddStringTable(tableId,
                                              numMarkerStrings, pMarkerStringOffsets,
                                              pMarkerStringData, markerStringDataSize);
    m_pUserMarkerHistoryTraceSource->AddUserMarkerHistory(sqttCbId, tableId, numOps, pUserMarkerOpHistory);
}

// =====================================================================================================================
void DevModeUberTrace::LabelAccelStruct(
    uint64_t    deviceAddress,
    const char* pString)
{
    if (pString != nullptr)
    {
        bool existed = false;
        AccelStructUserMarkerString* pUserMarker = nullptr;

        Pal::Result result = Pal::Result::Success;
        {
            Util::MutexAuto lock(&m_mutex);
            result = m_accelStructNames.FindAllocate(deviceAddress, &existed, &pUserMarker);
        }

        if (result == Pal::Result::Success)
        {
            VK_ASSERT(pUserMarker != nullptr);
            Strncpy(pUserMarker->string, pString, sizeof(pUserMarker->string));
            pUserMarker->length = strlen(pUserMarker->string);
        }
    }
}

} // namespace vk
