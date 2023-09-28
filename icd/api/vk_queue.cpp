/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_queue.cpp
 * @brief Contains implementation of Vulkan query pool.
 ***********************************************************************************************************************
 */

#include "include/cmd_buffer_ring.h"
#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_fence.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_queue.h"
#include "include/vk_semaphore.h"
#include "include/vk_swapchain.h"
#include "include/vk_utils.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#endif

#include "sqtt/sqtt_layer.h"

#include "palQueue.h"
namespace vk
{

// =====================================================================================================================
Queue::Queue(
    Device*                 pDevice,
    uint32_t                queueFamilyIndex,
    uint32_t                queueIndex,
    uint32_t                queueFlags,
    Pal::IQueue**           ppPalQueues,
    Pal::IQueue**           ppPalTmzQueues,
    Pal::IQueueSemaphore**  ppPalTmzSemaphores,
    VirtualStackAllocator*  pStackAllocator,
    CmdBufferRing*          pCmdBufferRing,
    Pal::IQueue**           ppPalBackupQueues,
    Pal::IQueue**           ppPalBackupTmzQueues,
    Pal::IQueueSemaphore**  ppSwitchToPalBackupSemaphore,
    Pal::IQueueSemaphore**  ppSwitchFromPalBackupSemaphore,
    const bool              isDeviceIndependent)
    :
    m_lastSubmissionProtected(false),
    m_pDevice(pDevice),
    m_queueFamilyIndex(queueFamilyIndex),
    m_queueIndex(queueIndex),
    m_queueFlags(queueFlags),
    m_pDevModeMgr(pDevice->VkInstance()->GetDevModeMgr()),
    m_pStackAllocator(pStackAllocator),
    m_pCmdBufferRing(pCmdBufferRing),
    m_isDeviceIndependent(isDeviceIndependent)
{
    if (ppPalQueues != nullptr)
    {
        memcpy(m_pPalQueues, ppPalQueues, sizeof(ppPalQueues[0]) * pDevice->NumPalDevices());
    }
    else
    {
        memset(&m_pPalQueues, 0, sizeof(ppPalQueues[0]) * pDevice->NumPalDevices());
    }

    if ((ppPalTmzQueues != nullptr) && (ppPalTmzSemaphores != nullptr))
    {
        memcpy(m_pPalTmzQueues, ppPalTmzQueues, sizeof(ppPalTmzQueues[0]) * pDevice->NumPalDevices());
        memcpy(m_pPalTmzSemaphore, ppPalTmzSemaphores, sizeof(ppPalTmzSemaphores[0]) * pDevice->NumPalDevices());
    }
    else
    {
        memset(&m_pPalTmzQueues, 0, sizeof(ppPalTmzQueues[0]) * pDevice->NumPalDevices());
        memset(&m_pPalTmzSemaphore, 0, sizeof(ppPalTmzSemaphores[0]) * pDevice->NumPalDevices());
    }

    if ((ppPalBackupQueues != nullptr) &&
        (ppSwitchToPalBackupSemaphore != nullptr) &&
        (ppSwitchFromPalBackupSemaphore != nullptr))
    {
        memcpy(m_pPalBackupQueues, ppPalBackupQueues, sizeof(ppPalBackupQueues[0]) * pDevice->NumPalDevices());
        memcpy(m_pSwitchToPalBackupSemaphore,
            ppSwitchToPalBackupSemaphore,
            sizeof(ppSwitchToPalBackupSemaphore[0]) * pDevice->NumPalDevices());
        memcpy(m_pSwitchFromPalBackupSemaphore,
            ppSwitchFromPalBackupSemaphore,
            sizeof(ppSwitchFromPalBackupSemaphore[0]) * pDevice->NumPalDevices());

        if (ppPalBackupTmzQueues != nullptr)
        {
            memcpy(m_pPalBackupTmzQueues,
                ppPalBackupTmzQueues,
                sizeof(ppPalBackupTmzQueues[0]) * pDevice->NumPalDevices());
        }
        else
        {
            memset(&m_pPalBackupTmzQueues, 0, sizeof(ppPalBackupTmzQueues[0]) * pDevice->NumPalDevices());
        }
    }
    else
    {
        memset(&m_pPalBackupQueues, 0, sizeof(ppPalBackupQueues[0]) * pDevice->NumPalDevices());
        memset(&m_pPalBackupTmzQueues, 0, sizeof(ppPalBackupTmzQueues[0]) * pDevice->NumPalDevices());
        memset(&m_pSwitchToPalBackupSemaphore,
            0,
            sizeof(m_pSwitchToPalBackupSemaphore[0]) * pDevice->NumPalDevices());
        memset(&m_pSwitchFromPalBackupSemaphore,
            0,
            sizeof(m_pSwitchFromPalBackupSemaphore[0]) * pDevice->NumPalDevices());
    }

    memset(&m_palFrameMetadataControl, 0, sizeof(Pal::PerSourceFrameMetadataControl));

    for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; deviceIdx++)
    {
        m_pDummyCmdBuffer[deviceIdx] = nullptr;
    }

    const Pal::DeviceProperties& deviceProps = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
    m_tmzPerQueue = (deviceProps.engineProperties->tmzSupportLevel == Pal::TmzSupportLevel::PerQueue) ? 1 : 0;

}

// =====================================================================================================================
VkResult Queue::Create(
    Device*                        pDevice,
    const VkAllocationCallbacks*   pAllocator,
    const uint32_t                 flags,
    const uint32_t                 queueFamilyIndex,
    const uint32_t                 engineIndex,
    const VkQueueGlobalPriorityKHR globalPriority,
    const uint32_t                 dedicatedComputeUnits,
    const bool                     isDeviceIndependent,
    VkQueue*                       pQueue)
{
    size_t       palQueueMemorySize = 0;
    VkResult     result             = VK_SUCCESS;
    const size_t apiQueueSize       = sizeof(ApiQueue);
    void*        pSysMem            = nullptr;

    result = GetPalQueueMemorySize(pDevice,
                                   queueFamilyIndex,
                                   engineIndex,
                                   dedicatedComputeUnits,
                                   globalPriority,
                                   flags,
                                   &palQueueMemorySize);

    if (result == VK_SUCCESS)
    {
        pSysMem = pDevice->AllocApiObject(pAllocator, apiQueueSize + palQueueMemorySize);

        if (pSysMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS)
    {
        Pal::Result  palResult        = Pal::Result::Success;
        void*        pPalQueueMemory  = Util::VoidPtrInc(pSysMem, apiQueueSize);

        size_t                 palQueueMemoryOffset = 0;
        uint32                 tmzQueueIndex        = 0;
        CmdBufferRing*         pCmdBufferRing       = nullptr;
        VirtualStackAllocator* pQueueStackAllocator = nullptr;

        Pal::IQueue*          pPalQueuesBase[MaxPalDevices]                = {};
        Pal::IQueue*          pPalTmzQueues[MaxPalDevices]                 = {};
        Pal::IQueueSemaphore* pPalTmzSemaphores[MaxPalDevices]             = {};

        Pal::IQueue*          pPalBackupQueues[MaxPalDevices]              = {};
        Pal::IQueue*          pPalBackupTmzQueues[MaxPalDevices]           = {};
        Pal::IQueueSemaphore* pSwitchToPalBackupSemaphore[MaxPalDevices]   = {};
        Pal::IQueueSemaphore* pSwitchFromPalBackupSemaphore[MaxPalDevices] = {};

        uint32_t deviceIdx;
        palResult =  CreatePalQueues(pDevice,
                                     queueFamilyIndex,
                                     engineIndex,
                                     dedicatedComputeUnits,
                                     globalPriority,
                                     flags,
                                     &palQueueMemoryOffset,
                                     &deviceIdx,
                                     pPalQueueMemory,
                                     pPalQueuesBase,
                                     pPalTmzQueues,
                                     pPalTmzSemaphores,
                                     pPalBackupQueues,
                                     pPalBackupTmzQueues,
                                     pSwitchToPalBackupSemaphore,
                                     pSwitchFromPalBackupSemaphore);

        if (palResult == Pal::Result::Success)
        {
            palResult = pDevice->VkInstance()->StackMgr()->AcquireAllocator(&pQueueStackAllocator);
        }

        if (palResult == Pal::Result::Success)
        {
            Pal::EngineType engineType = pDevice->GetQueueFamilyPalEngineType(queueFamilyIndex);
            Pal::QueueType  queueType  = pDevice->GetQueueFamilyPalQueueType(queueFamilyIndex);

            pCmdBufferRing = CmdBufferRing::Create(pDevice, engineType, queueType);

            if (pCmdBufferRing == nullptr)
            {
                palResult = Pal::Result::ErrorOutOfMemory;
            }
        }

        if (palResult == Pal::Result::Success)
        {
            VK_INIT_DISPATCHABLE(Queue, pSysMem, (pDevice,
                                                  queueFamilyIndex,
                                                  engineIndex,
                                                  flags,
                                                  pPalQueuesBase,
                                                  pPalTmzQueues,
                                                  pPalTmzSemaphores,
                                                  pQueueStackAllocator,
                                                  pCmdBufferRing,
                                                  pPalBackupQueues,
                                                  pPalBackupTmzQueues,
                                                  pSwitchToPalBackupSemaphore,
                                                  pSwitchFromPalBackupSemaphore,
                                                  isDeviceIndependent));
        }
        else
        {
            while (deviceIdx-- > 0)
            {
                if (pPalQueuesBase[deviceIdx] != nullptr)
                {
                    pPalQueuesBase[deviceIdx]->Destroy();
                }

                if (pPalTmzQueues[deviceIdx] != nullptr)
                {
                    pPalTmzQueues[deviceIdx]->Destroy();
                }

                if (pPalTmzSemaphores[deviceIdx] != nullptr)
                {
                    pPalTmzSemaphores[deviceIdx]->Destroy();
                }

                if (pPalBackupQueues[deviceIdx] != nullptr)
                {
                    pPalBackupQueues[deviceIdx]->Destroy();
                }

                if (pPalBackupTmzQueues[deviceIdx] != nullptr)
                {
                    pPalBackupTmzQueues[deviceIdx]->Destroy();
                }

                if (pSwitchFromPalBackupSemaphore[deviceIdx] != nullptr)
                {
                    pSwitchFromPalBackupSemaphore[deviceIdx]->Destroy();
                }

                if (pSwitchToPalBackupSemaphore[deviceIdx] != nullptr)
                {
                    pSwitchToPalBackupSemaphore[deviceIdx]->Destroy();
                }
            }

            if (pQueueStackAllocator != nullptr)
            {
                pDevice->VkInstance()->StackMgr()->ReleaseAllocator(pQueueStackAllocator);
            }
        }

        result = PalToVkResult(palResult);
    }

    if ((result != VK_SUCCESS) && (pSysMem!= nullptr))
    {
        pDevice->FreeApiObject(pAllocator, pSysMem);
    }
    else
    {
        *pQueue = reinterpret_cast<VkQueue>(pSysMem);
    }

    return result;
}

// =====================================================================================================================
void Queue::ConstructQueueCreateInfo(
    const PhysicalDevice&          physicalDevice,
    const uint32_t                 queueFamilyIndex,
    const uint32_t                 queueIndex,
    const uint32_t                 dedicatedComputeUnits,
    const VkQueueGlobalPriorityKHR queuePriority,
    Pal::QueueCreateInfo*          pQueueCreateInfo,
    const bool                     useComputeAsTransferQueue,
    const bool                     isTmzQueue)
{
    const auto&        palProperties    = physicalDevice.PalProperties();

    // Some configs can use this feature with any priority, but it's not useful for
    // lower priorities.
    constexpr uint32_t ReasonableTunnelPriorities = Pal::SupportQueuePriorityHigh |
        Pal::SupportQueuePriorityRealtime;

    // Get the sub engine index of vr high priority
    // UINT32_MAX is returned if the required vr high priority sub engine is not available
    const uint32_t vrHighPriorityIndex           = physicalDevice.GetVrHighPrioritySubEngineIndex();
    const uint32_t rtCuHighComputeSubEngineIndex = physicalDevice.GetRtCuHighComputeSubEngineIndex();
    const uint32_t tunnelComputeSubEngineIndex   = physicalDevice.GetTunnelComputeSubEngineIndex();
    const uint32_t tunnelPriorities              = (physicalDevice.GetTunnelPrioritySupport() &
                                                    ReasonableTunnelPriorities);

    Pal::QueueType palQueueType =
        physicalDevice.GetQueueFamilyPalQueueType(queueFamilyIndex);

    bool changeDMAToCompute = false;

    if ((palQueueType == Pal::QueueType::QueueTypeDma) && useComputeAsTransferQueue)
    {
        palQueueType        = Pal::QueueType::QueueTypeCompute;
        changeDMAToCompute  = true;
    }
    else if ((palQueueType == Pal::QueueType::QueueTypeCompute) &&
        (physicalDevice.GetRuntimeSettings().useUniversalAsComputeQueue))
    {
        palQueueType = Pal::QueueType::QueueTypeUniversal;
    }

    pQueueCreateInfo->tmzOnly = isTmzQueue;

    if ((dedicatedComputeUnits > 0) &&
        (rtCuHighComputeSubEngineIndex != UINT32_MAX))
    {
        VK_ASSERT(queuePriority == VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR);

        pQueueCreateInfo->engineType = Pal::EngineType::EngineTypeCompute;
        pQueueCreateInfo->engineIndex = rtCuHighComputeSubEngineIndex;
        pQueueCreateInfo->numReservedCu = dedicatedComputeUnits;
    }
    else if (palQueueType == Pal::QueueType::QueueTypeCompute)
    {
        constexpr uint32_t VrHighPriority    = Pal::SupportQueuePriorityMedium |
                                               Pal::SupportQueuePriorityHigh |
                                               Pal::SupportQueuePriorityRealtime;

        const uint32_t     queuePriorityMask = ((queuePriority == VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR)        ?
                                                Pal::SupportQueuePriorityHigh                               :
                                                ((queuePriority == VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR) ?
                                                 Pal::SupportQueuePriorityRealtime : Pal::SupportQueuePriorityNormal));

        pQueueCreateInfo->engineType = Pal::EngineType::EngineTypeCompute;

        if ((tunnelComputeSubEngineIndex != UINT32_MAX) &&
            TestAnyFlagSet(tunnelPriorities, queuePriorityMask))
        {
            pQueueCreateInfo->engineIndex = tunnelComputeSubEngineIndex;
            pQueueCreateInfo->dispatchTunneling = 1;
        }
        else if ((vrHighPriorityIndex != UINT32_MAX) &&
                 TestAnyFlagSet(VrHighPriority, queuePriorityMask))
        {
            pQueueCreateInfo->engineIndex = vrHighPriorityIndex;
        }
        else if (changeDMAToCompute)
        {
            pQueueCreateInfo->engineIndex = physicalDevice.GetCompQueueEngineIndex(0);
        }
        else
        {
            pQueueCreateInfo->engineIndex = physicalDevice.GetCompQueueEngineIndex(queueIndex);
        }
    }
    else
    {
        if (palQueueType == Pal::QueueType::QueueTypeUniversal)
        {
            pQueueCreateInfo->engineType = Pal::EngineType::EngineTypeUniversal;
        }
        else
        {
            pQueueCreateInfo->engineType = physicalDevice.GetQueueFamilyPalEngineType(queueFamilyIndex);
        }

        if (palQueueType == Pal::QueueType::QueueTypeUniversal)
        {
            pQueueCreateInfo->engineIndex = physicalDevice.GetUniversalQueueEngineIndex(queueIndex);
        }
        else
        {
            pQueueCreateInfo->engineIndex = queueIndex;
        }
    }

    pQueueCreateInfo->forceWaitIdleOnRingResize = 1;
    pQueueCreateInfo->queueType                 = palQueueType;
    pQueueCreateInfo->priority                  = VkToPalGlobalPriority(queuePriority,
        palProperties.engineProperties[pQueueCreateInfo->engineType].capabilities[pQueueCreateInfo->engineIndex]);
#if defined(__unix__)
    pQueueCreateInfo->enableGpuMemoryPriorities = 1;
#endif
}

// =====================================================================================================================
Pal::Result Queue::CreatePalQueue(
    const PhysicalDevice&          physicalDevice,
    Pal::IDevice*                  pPalDevice,
    const uint32_t                 queueFamilyIndex,
    const uint32_t                 queueIndex,
    const uint32_t                 dedicatedComputeUnit,
    const VkQueueGlobalPriorityKHR queuePriority,
    Pal::QueueCreateInfo*          pQueueCreateInfo,
    void*                          pPalQueueMemory,
    const size_t                   palQueueMemoryOffset,
    Pal::IQueue**                  ppPalQueue,
    const wchar_t*                 pExecutableName,
    const wchar_t*                 pExecutablePath,
    const bool                     useComputeAsTransferQueue,
    const bool                     isTmzQueue)
{
    Pal::Result palResult = Pal::Result::Success;

    ConstructQueueCreateInfo(physicalDevice,
                             queueFamilyIndex,
                             queueIndex,
                             dedicatedComputeUnit,
                             queuePriority,
                             pQueueCreateInfo,
                             useComputeAsTransferQueue,
                             isTmzQueue);

    palResult = pPalDevice->CreateQueue(*pQueueCreateInfo,
        Util::VoidPtrInc(pPalQueueMemory, palQueueMemoryOffset),
        ppPalQueue);

    if (palResult == Pal::Result::Success)
    {
        // On the creation of each command queue, the escape
        // KMD_ESUBFUNC_UPDATE_APP_PROFILE_POWER_SETTING needs to be called, to provide the app's
        // executable name and path. This lets KMD use the context created per queue for tracking
        // the app.
        palResult = (*ppPalQueue)->UpdateAppPowerProfile(static_cast<const wchar_t*>(pExecutableName),
            static_cast<const wchar_t*>(pExecutablePath));

        if ((palResult == Pal::Result::Unsupported) ||
            (palResult == Pal::Result::ErrorInvalidValue) ||
            (palResult == Pal::Result::ErrorUnavailable))
        {
            palResult = Pal::Result::Success;
        }
    }

    return palResult;
}

// =====================================================================================================================
VkResult Queue::GetPalQueueMemorySize(
    const Device*                  pDevice,
    const uint32_t                 queueFamilyIndex,
    const uint32_t                 queueIndex,
    const uint32_t                 dedicatedComputeUnits,
    const VkQueueGlobalPriorityKHR globalPriority,
    const uint32_t                 flags,
    size_t*                        palQueueMemorySize)
{

    Pal::Result                  palResult  = Pal::Result::Success;
    const RuntimeSettings&       settings   = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();
    const Pal::DeviceProperties& properties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    const bool useComputeAsTransferQueue = pDevice->UseComputeAsTransfer();

    for (uint32_t deviceIdx = 0;
         (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
         ++deviceIdx)
    {
        const PhysicalDevice& physicalDevice = *(pDevice->VkPhysicalDevice(deviceIdx));
        const Pal::IDevice*   pPalDevice     = physicalDevice.PalDevice();

        Pal::QueueCreateInfo queueCreateInfo = {};

        ConstructQueueCreateInfo(physicalDevice,
                                 queueFamilyIndex,
                                 queueIndex,
                                 dedicatedComputeUnits,
                                 globalPriority,
                                 &queueCreateInfo,
                                 useComputeAsTransferQueue,
                                 false);
        *palQueueMemorySize += pPalDevice->GetQueueSize(queueCreateInfo, &palResult);

        if ((palResult == Pal::Result::Success) &&
            (flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) &&
            (properties.engineProperties[queueCreateInfo.engineType].tmzSupportLevel ==
                Pal::TmzSupportLevel::PerQueue))
        {
            Pal::QueueCreateInfo tmzQueueCreateInfo = {};

            ConstructQueueCreateInfo(physicalDevice,
                                     queueFamilyIndex,
                                     queueIndex,
                                     dedicatedComputeUnits,
                                     globalPriority,
                                     &tmzQueueCreateInfo,
                                     useComputeAsTransferQueue,
                                     true);

            *palQueueMemorySize += pPalDevice->GetQueueSize(tmzQueueCreateInfo, &palResult);

            // Create TMZ semaphore for each tmz queue.
            Pal::QueueSemaphoreCreateInfo tmzSemaphoreCreateInfo = {};
            tmzSemaphoreCreateInfo.maxCount = 1;

            if (palResult == Pal::Result::Success)
            {
                *palQueueMemorySize += pPalDevice->GetQueueSemaphoreSize(
                    tmzSemaphoreCreateInfo, &palResult);
            }
        }

        if ((queueCreateInfo.queueType == Pal::QueueType::QueueTypeDma)               &&
            pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsComputeEngineSupported() &&
            settings.useBackupCmdbuffer)
        {
            // create a backup compute queue for dma queue
            Pal::QueueCreateInfo backupQueueCreateInfo = {};

            ConstructQueueCreateInfo(physicalDevice,
                                     queueFamilyIndex,
                                     queueIndex,
                                     dedicatedComputeUnits,
                                     globalPriority,
                                     &backupQueueCreateInfo,
                                     true,
                                     false);

            *palQueueMemorySize += pPalDevice->GetQueueSize(backupQueueCreateInfo, &palResult);

            // Create semaphores for switch to backup queue.
            Pal::QueueSemaphoreCreateInfo switchToBackupSemaphoreCreateInfo = {};
            switchToBackupSemaphoreCreateInfo.maxCount = 1;

            *palQueueMemorySize += pPalDevice->GetQueueSemaphoreSize(
                switchToBackupSemaphoreCreateInfo,
                &palResult);

            // Create semaphores for switch from backup queue.
            Pal::QueueSemaphoreCreateInfo switchFromBackupSemaphoreCreateInfo = {};
            switchFromBackupSemaphoreCreateInfo.maxCount = 1;

            *palQueueMemorySize += pPalDevice->GetQueueSemaphoreSize(
                switchFromBackupSemaphoreCreateInfo,
                &palResult);

            if ((flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) &&
                (properties.engineProperties[backupQueueCreateInfo.engineType].tmzSupportLevel ==
                    Pal::TmzSupportLevel::PerQueue))
            {
                Pal::QueueCreateInfo backupTmzQueueCreateInfo = {};

                ConstructQueueCreateInfo(physicalDevice,
                                         queueFamilyIndex,
                                         queueIndex,
                                         dedicatedComputeUnits,
                                         globalPriority,
                                         &backupTmzQueueCreateInfo,
                                         true,
                                         true);

                *palQueueMemorySize += pPalDevice->GetQueueSize(backupTmzQueueCreateInfo, &palResult);
            }
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
Pal::Result Queue::CreatePalQueues(
    const Device*                  pDevice,
    const uint32_t                 queueFamilyIndex,
    const uint32_t                 queueIndex,
    const uint32_t                 dedicatedComputeUnits,
    const VkQueueGlobalPriorityKHR globalPriority,
    const uint32_t                 flags,
    size_t*                        pPalQueueMemoryOffset,
    uint32_t*                      pDeviceIdx,
    void*                          pPalQueueMemory,
    Pal::IQueue**                  ppPalQueuesBase,
    Pal::IQueue**                  ppPalTmzQueues,
    Pal::IQueueSemaphore**         ppPalTmzSemaphores,
    Pal::IQueue**                  ppPalBackupQueues,
    Pal::IQueue**                  ppPalBackupTmzQueues,
    Pal::IQueueSemaphore**         ppSwitchToPalBackupSemaphore,
    Pal::IQueueSemaphore**         ppSwitchFromPalBackupSemaphore)
{
    Pal::Result                  palResult  = Pal::Result::Success;
    const RuntimeSettings&       settings   = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();
    const Pal::DeviceProperties& properties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    const bool useComputeAsTransferQueue = pDevice->UseComputeAsTransfer();

    wchar_t executableName[PATH_MAX];
    wchar_t executableePath[PATH_MAX];
    utils::GetExecutableNameAndPath(executableName, executableePath);

    for (*pDeviceIdx = 0; *pDeviceIdx < pDevice->NumPalDevices(); (*pDeviceIdx)++)
    {
        const PhysicalDevice& physicalDevice = *(pDevice->VkPhysicalDevice(*pDeviceIdx));
        Pal::IDevice* pPalDevice = physicalDevice.PalDevice();

        Pal::QueueCreateInfo queueCreateInfo = {};
        palResult = CreatePalQueue(physicalDevice,
                                   pPalDevice,
                                   queueFamilyIndex,
                                   queueIndex,
                                   dedicatedComputeUnits,
                                   globalPriority,
                                   &queueCreateInfo,
                                   pPalQueueMemory,
                                   *pPalQueueMemoryOffset,
                                   &ppPalQueuesBase[*pDeviceIdx],
                                   executableName,
                                   executableePath,
                                   useComputeAsTransferQueue,
                                   false);

        if (palResult != Pal::Result::Success)
        {
            break;
        }
        *pPalQueueMemoryOffset += pPalDevice->GetQueueSize(queueCreateInfo, &palResult);

        // Create a TMZ queue at the protected capability queue creation time
        // when this engine support per queue level tmz.
        if ((palResult == Pal::Result::Success) && (flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) &&
            (properties.engineProperties[ppPalQueuesBase[*pDeviceIdx]->GetEngineType()].tmzSupportLevel ==
                Pal::TmzSupportLevel::PerQueue))
        {
            Pal::QueueCreateInfo tmzQueueCreateInfo = {};

            palResult = CreatePalQueue(physicalDevice,
                                       pPalDevice,
                                       queueFamilyIndex,
                                       queueIndex,
                                       dedicatedComputeUnits,
                                       globalPriority,
                                       &tmzQueueCreateInfo,
                                       pPalQueueMemory,
                                       *pPalQueueMemoryOffset,
                                       &ppPalTmzQueues[*pDeviceIdx],
                                       executableName,
                                       executableePath,
                                       useComputeAsTransferQueue,
                                       true);

            if (palResult != Pal::Result::Success)
            {
                break;
            }

            *pPalQueueMemoryOffset += pPalDevice->GetQueueSize(tmzQueueCreateInfo, &palResult);

            // Create TMZ semaphore for each tmz queue.
            Pal::QueueSemaphoreCreateInfo tmzSemaphoreCreateInfo = {};
            tmzSemaphoreCreateInfo.maxCount = 1;

            palResult = pPalDevice->CreateQueueSemaphore(
                tmzSemaphoreCreateInfo,
                Util::VoidPtrInc(pPalQueueMemory, *pPalQueueMemoryOffset),
                &ppPalTmzSemaphores[*pDeviceIdx]);

            *pPalQueueMemoryOffset += pPalDevice->GetQueueSemaphoreSize(tmzSemaphoreCreateInfo,
                &palResult);

            if (palResult != Pal::Result::Success)
            {
                break;
            }
        }

        // Create a backup queue when this is a dma queue type.
        if ((queueCreateInfo.queueType == Pal::QueueType::QueueTypeDma)               &&
            pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsComputeEngineSupported() &&
            settings.useBackupCmdbuffer)
        {
            Pal::QueueCreateInfo backupQueueCreateInfo = {};

            // If we are inside here, compute engine is guaranteed to be supported by the device and thus hardcoding
            // useComputeAsTransferQueue = true is fine.
            palResult = CreatePalQueue(physicalDevice,
                                       pPalDevice,
                                       queueFamilyIndex,
                                       queueIndex,
                                       dedicatedComputeUnits,
                                       globalPriority,
                                       &backupQueueCreateInfo,
                                       pPalQueueMemory,
                                       *pPalQueueMemoryOffset,
                                       &ppPalBackupQueues[*pDeviceIdx],
                                       executableName,
                                       executableePath,
                                       true,
                                       false);

            if (palResult != Pal::Result::Success)
            {
                break;
            }

            *pPalQueueMemoryOffset += pPalDevice->GetQueueSize(backupQueueCreateInfo, &palResult);

            // Create backup semaphore for each backup queue.
            Pal::QueueSemaphoreCreateInfo switchToBackupSemaphoreCreateInfo = {};
            switchToBackupSemaphoreCreateInfo.maxCount = 1;

            palResult = pPalDevice->CreateQueueSemaphore(
                switchToBackupSemaphoreCreateInfo,
                Util::VoidPtrInc(pPalQueueMemory, *pPalQueueMemoryOffset),
                &ppSwitchToPalBackupSemaphore[*pDeviceIdx]);

            *pPalQueueMemoryOffset += pPalDevice->GetQueueSemaphoreSize(
                switchToBackupSemaphoreCreateInfo,
                &palResult);

            if (palResult != Pal::Result::Success)
            {
                break;
            }

            // Create backup semaphore for each backup queue.
            Pal::QueueSemaphoreCreateInfo switchFromBackupSemaphoreCreateInfo = {};
            switchFromBackupSemaphoreCreateInfo.maxCount = 1;

            palResult = pPalDevice->CreateQueueSemaphore(
                switchFromBackupSemaphoreCreateInfo,
                Util::VoidPtrInc(pPalQueueMemory, *pPalQueueMemoryOffset),
                &ppSwitchFromPalBackupSemaphore[*pDeviceIdx]);

            *pPalQueueMemoryOffset += pPalDevice->GetQueueSemaphoreSize(
                switchFromBackupSemaphoreCreateInfo,
                &palResult);

            if (palResult != Pal::Result::Success)
            {
                break;
            }

            if ((flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) &&
                (properties.engineProperties[backupQueueCreateInfo.engineType].tmzSupportLevel ==
                    Pal::TmzSupportLevel::PerQueue))
            {
                Pal::QueueCreateInfo backupTmzQueueCreateInfo = {};

                // If we are inside here, Compute engine is guaranteed to be supported by the device and thus hardcoding
                // useComputeAsTransferQueue = true is fine.
                palResult = CreatePalQueue(physicalDevice,
                                           pPalDevice,
                                           queueFamilyIndex,
                                           queueIndex,
                                           dedicatedComputeUnits,
                                           globalPriority,
                                           &backupTmzQueueCreateInfo,
                                           pPalQueueMemory,
                                           *pPalQueueMemoryOffset,
                                           &ppPalBackupTmzQueues[*pDeviceIdx],
                                           executableName,
                                           executableePath,
                                           true,
                                           true);

                if (palResult != Pal::Result::Success)
                {
                    break;
                }

                *pPalQueueMemoryOffset += pPalDevice->GetQueueSize(
                    backupTmzQueueCreateInfo,
                    &palResult);

            }
        }
    }

    return palResult;
}

// =====================================================================================================================
Queue::~Queue()
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (m_pDummyCmdBuffer[deviceIdx] != nullptr)
        {
            m_pDummyCmdBuffer[deviceIdx]->Destroy();
            m_pDevice->VkInstance()->FreeMem(m_pDummyCmdBuffer[deviceIdx]);
        }
    }

    if (m_pCmdBufferRing != nullptr)
    {
        m_pCmdBufferRing->Destroy(m_pDevice);
    }

    if (m_pStackAllocator != nullptr)
    {
        // Release the stack allocator
        m_pDevice->VkInstance()->StackMgr()->ReleaseAllocator(m_pStackAllocator);
    }

    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
    {
        if (m_pPalQueues[i] != nullptr)
        {
            m_pPalQueues[i]->Destroy();
        }

        if (m_pPalTmzQueues[i] != nullptr)
        {
            m_pPalTmzQueues[i]->Destroy();
        }

        if (m_pPalTmzSemaphore[i] != nullptr)
        {
            m_pPalTmzSemaphore[i]->Destroy();
        }

        if (m_pSwitchToPalBackupSemaphore[i] != nullptr)
        {
            m_pSwitchToPalBackupSemaphore[i]->Destroy();
        }

        if (m_pSwitchFromPalBackupSemaphore[i] != nullptr)
        {
            m_pSwitchFromPalBackupSemaphore[i]->Destroy();
        }

        if (m_pPalBackupQueues[i] != nullptr)
        {
            m_pPalBackupQueues[i]->Destroy();
        }

        if (m_pPalBackupTmzQueues[i] != nullptr)
        {
            m_pPalBackupTmzQueues[i]->Destroy();
        }
    }

}

// =====================================================================================================================
void Queue::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    this->~Queue();
    pDevice->FreeApiObject(pAllocator, ApiQueue::FromObject(this));
}

// =====================================================================================================================
// Create a dummy command buffer for the present queue
VkResult Queue::CreateDummyCmdBuffer()
{
    Pal::Result palResult = Pal::Result::ErrorUnknown;

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        Pal::CmdBufferCreateInfo palCreateInfo = {};
        palCreateInfo.pCmdAllocator = m_pDevice->GetSharedCmdAllocator(deviceIdx);
        palCreateInfo.queueType     = m_pDevice->GetQueueFamilyPalQueueType(m_queueFamilyIndex);
        palCreateInfo.engineType    = m_pDevice->GetQueueFamilyPalEngineType(m_queueFamilyIndex);

        Pal::IDevice* const pPalDevice = m_pDevice->PalDevice(deviceIdx);
        const size_t palSize = pPalDevice->GetCmdBufferSize(palCreateInfo, &palResult);

        if (palResult == Pal::Result::Success)
        {
            void* pMemory = m_pDevice->VkInstance()->AllocMem(palSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
            if (pMemory != nullptr)
            {
                palResult = pPalDevice->CreateCmdBuffer(palCreateInfo, pMemory, &(m_pDummyCmdBuffer[deviceIdx]));

                if (palResult == Pal::Result::Success)
                {
                    Pal::CmdBufferBuildInfo buildInfo = {};
                    palResult = m_pDummyCmdBuffer[deviceIdx]->Begin(buildInfo);
                    if (palResult == Pal::Result::Success)
                    {
                        palResult = m_pDummyCmdBuffer[deviceIdx]->End();
                    }
                }
            }
            else
            {
                palResult = Pal::Result::ErrorOutOfMemory;
            }
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Submit a dummy command buffer with associated command buffer info to KMD for FRTC/TurboSync/DVR features
VkResult Queue::NotifyFlipMetadata(
    uint32_t                     deviceIdx,
    Pal::IQueue*                 pPresentQueue,
    CmdBufState*                 pCmdBufState,
    const Pal::IGpuMemory*       pGpuMemory,
    FullscreenFrameMetadataFlags flags,
    bool                         forceSubmit)
{
    VkResult result = VK_SUCCESS;

    if ((flags.frameBeginFlag == 1) || (flags.frameEndFlag == 1) || (flags.primaryHandle == 1) ||
        (pCmdBufState != nullptr) || forceSubmit)
    {
        Pal::IQueue*    pPalQueue  = m_pPalQueues[deviceIdx];
        Pal::CmdBufInfo cmdBufInfo = {};

        cmdBufInfo.isValid = 1;

        if (flags.frameBeginFlag == 1)
        {
            cmdBufInfo.frameBegin = 1;
        }
        else if (flags.frameEndFlag == 1)
        {
            cmdBufInfo.frameEnd = 1;
        }

        if (flags.primaryHandle == 1)
        {
            cmdBufInfo.pPrimaryMemory = pGpuMemory;
        }

        // Submit the flip metadata to the appropriate device and present queue for the software compositing path.
        if ((pPresentQueue != nullptr) && (pPresentQueue != pPalQueue))
        {
            result = m_pDevice->SwCompositingNotifyFlipMetadata(pPresentQueue, cmdBufInfo);
        }
        else
        {
            // If there's already a command buffer that needs to be submitted, use it instead of a dummy one.
            if (pCmdBufState != nullptr)
            {
                result = m_pCmdBufferRing->SubmitCmdBuffer(m_pDevice, deviceIdx, pPalQueue, cmdBufInfo, pCmdBufState);
            }
            else
            {
                if (m_pDummyCmdBuffer[deviceIdx] == nullptr)
                {
                    result = CreateDummyCmdBuffer();
                }

                if (result == VK_SUCCESS)
                {
                    Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
                    perSubQueueInfo.cmdBufferCount  = 1;
                    perSubQueueInfo.ppCmdBuffers    = &m_pDummyCmdBuffer[deviceIdx];
                    perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

                    Pal::SubmitInfo submitInfo = {};
                    submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
                    submitInfo.perSubQueueInfoCount = 1;

                    result = PalToVkResult(pPalQueue->Submit(submitInfo));
                }
            }
        }

        VK_ASSERT(result == VK_SUCCESS);
    }

    return result;
}

// =====================================================================================================================
// Submit command buffer info with frameEndFlag and primaryHandle before frame present
VkResult Queue::NotifyFlipMetadataBeforePresent(
    uint32_t                         deviceIdx,
    Pal::IQueue*                     pPresentQueue,
    const Pal::PresentSwapChainInfo* pPresentInfo,
    CmdBufState*                     pCmdBufState,
    const Pal::IGpuMemory*           pGpuMemory,
    bool                             forceSubmit,
    bool                             skipFsfmFlags)
{
    FullscreenFrameMetadataFlags flags       = {};
    const Pal::DeviceProperties& deviceProps = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    if (skipFsfmFlags == false)
    {

        if (deviceProps.osProperties.flags.requireFrameEnd == 1)
        {
            flags.frameEndFlag = 1;
        }
    }

    return NotifyFlipMetadata(deviceIdx, pPresentQueue, pCmdBufState, pGpuMemory, flags, forceSubmit);
}

// =====================================================================================================================
// Submit command buffer info with frameBeginFlag after frame present
VkResult Queue::NotifyFlipMetadataAfterPresent(
    uint32_t                         deviceIdx,
    const Pal::PresentSwapChainInfo* pPresentInfo)
{
    VkResult result = VK_SUCCESS;

    return result;
}

// =====================================================================================================================
// Submit an array of command buffers to a queue
template<typename SubmitInfoType>
VkResult Queue::Submit(
    uint32_t              submitCount,
    const SubmitInfoType* pSubmits,
    VkFence               fence)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) && pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif
    Fence* pFence = Fence::ObjectFromHandle(fence);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    VkResult result = VK_SUCCESS;

    const bool isSynchronization2 = std::is_same<SubmitInfoType, VkSubmitInfo2KHR>::value;

    // The fence should be only used in the last submission to PAL. The implicit ordering guarantees provided by PAL
    // make sure that the fence is only signaled when all submissions complete.
    if ((submitCount == 0) && (pFence != nullptr))
    {
        Pal::IFence* pPalFence = nullptr;

        // If the submit count is zero but there is a fence, do a dummy submit just so the fence is signaled.
        Pal::SubmitInfo            submitInfo      = {};
        Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};

        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.gpuMemRefCount       = 0;
        submitInfo.pGpuMemoryRefs       = nullptr;

        Pal::Result palResult = Pal::Result::Success;

        pFence->SetActiveDevice(DefaultDeviceIndex);
        pPalFence = pFence->PalFence(DefaultDeviceIndex);

        submitInfo.ppFences   = &pPalFence;
        submitInfo.fenceCount = 1;

        palResult = PalQueue(DefaultDeviceIndex)->Submit(submitInfo);

        result = PalToVkResult(palResult);
    }
    else
    {
        for (uint32_t submitIdx = 0; (submitIdx < submitCount) && (result == VK_SUCCESS); ++submitIdx)
        {
            const SubmitInfoType& submitInfo = pSubmits[submitIdx];
            const VkDeviceGroupSubmitInfo* pDeviceGroupInfo = nullptr;
            const VkProtectedSubmitInfo* pProtectedSubmitInfo = nullptr;
            bool  protectedSubmit = false;

            uint32_t        waitValueCount         = 0;
            const uint64_t* pWaitSemaphoreValues   = nullptr;
            uint32_t        signalValueCount       = 0;
            const uint64_t* pSignalSemaphoreValues = nullptr;

            const void* pNext = submitInfo.pNext;

#if VKI_RAY_TRACING
#endif

            while (pNext != nullptr)
            {
                const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

                switch (static_cast<int32_t>(pHeader->sType))
                {
                case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
                    VK_ASSERT(isSynchronization2 == false);
                    pDeviceGroupInfo = static_cast<const VkDeviceGroupSubmitInfo*>(pNext);
                    break;

                case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
                {
                    const VkTimelineSemaphoreSubmitInfo* pTimelineSemaphoreInfo =
                        static_cast<const VkTimelineSemaphoreSubmitInfo*>(pNext);

                    VK_ASSERT(isSynchronization2 == false);

                    waitValueCount         = pTimelineSemaphoreInfo->waitSemaphoreValueCount;
                    pWaitSemaphoreValues   = pTimelineSemaphoreInfo->pWaitSemaphoreValues;
                    signalValueCount       = pTimelineSemaphoreInfo->signalSemaphoreValueCount;
                    pSignalSemaphoreValues = pTimelineSemaphoreInfo->pSignalSemaphoreValues;
                    break;
                }
                case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
                    VK_ASSERT(isSynchronization2 == false);

                    pProtectedSubmitInfo = static_cast<const VkProtectedSubmitInfo*>(pNext);
                    protectedSubmit      = pProtectedSubmitInfo->protectedSubmit;
                    break;

                default:
                    // Skip any unknown extension structures
                    break;
                }

                pNext = pHeader->pNext;
            }

            // Allocate space to store the PAL command buffer handles
            VkCommandBuffer* pCmdBuffers = nullptr;
            uint32_t cmdBufferCount      = 0;
            uint32_t waitSemaphoreCount  = 0;

            if (isSynchronization2)
            {
                const VkSubmitInfo2KHR* pSubmitInfoKhr =
                    reinterpret_cast<const VkSubmitInfo2KHR*>(&pSubmits[submitIdx]);

                if ((pSubmitInfoKhr->flags & VK_SUBMIT_PROTECTED_BIT_KHR) != 0)
                {
                    protectedSubmit = true;
                }

                if ((result == VK_SUCCESS) && (pSubmitInfoKhr->waitSemaphoreInfoCount > 0))
                {
                    waitValueCount = pSubmitInfoKhr->waitSemaphoreInfoCount;

                    VkSemaphore* pWaitSemaphores          = virtStackFrame.AllocArray<VkSemaphore>(waitValueCount);
                    uint64_t* pWaitSemaphoreInfoValues    = virtStackFrame.AllocArray<uint64_t>(waitValueCount);
                    uint32_t* pWaitSemaphoreDeviceIndices = virtStackFrame.AllocArray<uint32_t>(waitValueCount);

                    for (uint32_t i = 0; i < waitValueCount; i++)
                    {
                        pWaitSemaphores[i]             = pSubmitInfoKhr->pWaitSemaphoreInfos[i].semaphore;
                        pWaitSemaphoreInfoValues[i]    = pSubmitInfoKhr->pWaitSemaphoreInfos[i].value;
                        pWaitSemaphoreDeviceIndices[i] = pSubmitInfoKhr->pWaitSemaphoreInfos[i].deviceIndex;
                    }

                    result = PalWaitSemaphores(
                        waitValueCount,
                        pWaitSemaphores,
                        pWaitSemaphoreInfoValues,
                        waitValueCount,
                        pWaitSemaphoreDeviceIndices);

                    virtStackFrame.FreeArray(pWaitSemaphores);
                    virtStackFrame.FreeArray(pWaitSemaphoreInfoValues);
                    virtStackFrame.FreeArray(pWaitSemaphoreDeviceIndices);
                }

                pCmdBuffers = pSubmitInfoKhr->commandBufferInfoCount > 0 ?
                    virtStackFrame.AllocArray<VkCommandBuffer>(pSubmitInfoKhr->commandBufferInfoCount) : nullptr;

                for (uint32_t i = 0; i < pSubmitInfoKhr->commandBufferInfoCount; i++)
                {
                    pCmdBuffers[i] = pSubmitInfoKhr->pCommandBufferInfos[i].commandBuffer;
                }

                cmdBufferCount     = pSubmitInfoKhr->commandBufferInfoCount;
                waitSemaphoreCount = pSubmitInfoKhr->waitSemaphoreInfoCount;
            }
            else
            {
                const VkSubmitInfo* pSubmitInfoOld = reinterpret_cast<const VkSubmitInfo*>(&pSubmits[submitIdx]);

                if ((result == VK_SUCCESS) && (pSubmitInfoOld->waitSemaphoreCount > 0))
                {
                    VK_ASSERT((pWaitSemaphoreValues == nullptr) ||
                        (pSubmitInfoOld->waitSemaphoreCount == waitValueCount));

                    result = PalWaitSemaphores(
                        pSubmitInfoOld->waitSemaphoreCount,
                        pSubmitInfoOld->pWaitSemaphores,
                        pWaitSemaphoreValues,
                        (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->waitSemaphoreCount : 0),
                        (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->pWaitSemaphoreDeviceIndices : nullptr));
                }

                pCmdBuffers        = const_cast<VkCommandBuffer*>(pSubmitInfoOld->pCommandBuffers);
                cmdBufferCount     = pSubmitInfoOld->commandBufferCount;
                waitSemaphoreCount = pSubmitInfoOld->waitSemaphoreCount;
            }

            Pal::ICmdBuffer** pPalCmdBuffers = (cmdBufferCount > 0) ?
                                                virtStackFrame.AllocArray<Pal::ICmdBuffer*>(cmdBufferCount) :
                                                nullptr;

            Pal::CmdBufInfo* pCmdBufInfos = (cmdBufferCount > 0) ?
                                            virtStackFrame.AllocArray<Pal::CmdBufInfo>(cmdBufferCount) :
                                            nullptr;

            result = (((pPalCmdBuffers != nullptr) && (pCmdBufInfos != nullptr)) ||
                      (cmdBufferCount == 0)) ? result : VK_ERROR_OUT_OF_HOST_MEMORY;

            if (pCmdBufInfos != nullptr)
            {
                memset(pCmdBufInfos, 0, sizeof(Pal::CmdBufInfo) * cmdBufferCount);
            }

            bool lastBatch = (submitIdx == submitCount - 1);

            Pal::IFence* pPalFence = nullptr;
            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};

            perSubQueueInfo.cmdBufferCount  = 0;
            perSubQueueInfo.ppCmdBuffers    = pPalCmdBuffers;
            perSubQueueInfo.pCmdBufInfoList = pCmdBufInfos;

            Pal::SubmitInfo palSubmitInfo = {};

            palSubmitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
            palSubmitInfo.perSubQueueInfoCount = 1;
            palSubmitInfo.gpuMemRefCount       = 0;
            palSubmitInfo.pGpuMemoryRefs       = nullptr;

            const uint32_t deviceCount = (pDeviceGroupInfo == nullptr) ? 1 : m_pDevice->NumPalDevices();
            for (uint32_t deviceIdx = 0; (deviceIdx < deviceCount) && (result == VK_SUCCESS); deviceIdx++)
            {
                Pal::Result palResult = Pal::Result::Success;

                // Get the PAL command buffer object from each Vulkan object and put it
                // in the local array before submitting to PAL.
                ApiCmdBuffer* const * pCommandBuffers =
                    reinterpret_cast<ApiCmdBuffer*const*>(pCmdBuffers);

                perSubQueueInfo.cmdBufferCount = 0;

                palSubmitInfo.stackSizeInDwords = 0;

                const uint32_t deviceMask = 1 << deviceIdx;

                for (uint32_t i = 0; i < cmdBufferCount; ++i)
                {
                    if ((deviceCount > 1) &&
                        (pDeviceGroupInfo->pCommandBufferDeviceMasks != nullptr) &&
                        (pDeviceGroupInfo->pCommandBufferDeviceMasks[i] & deviceMask) == 0)
                    {
                        continue;
                    }

                    const CmdBuffer& cmdBuf = *(*pCommandBuffers[i]);

                    if (cmdBuf.IsProtected() == protectedSubmit)
                    {
#if VKI_RAY_TRACING
                        if (cmdBuf.HasRayTracing())
                        {
                            pCmdBufInfos[i].isValid = true;
                            pCmdBufInfos[i].rayTracingExecuted = true;
                        }
#endif

                        (*pCommandBuffers[i])->GetDebugPrintf()->PreQueueSubmit(m_pDevice, deviceIdx);
                        pPalCmdBuffers[perSubQueueInfo.cmdBufferCount++] = cmdBuf.PalCmdBuffer(deviceIdx);

                        if (cmdBuf.IsBackupBufferUsed())
                        {
                            // declare backup PAL command buffer handles
                            Pal::ICmdBuffer* pBackupPalCmdBuffer = nullptr;
                            Pal::IQueue* pMainQueue =
                                (cmdBuf.IsProtected() && m_tmzPerQueue) ?
                                m_pPalTmzQueues[deviceIdx] :
                                m_pPalQueues[deviceIdx];
                            Pal::IQueue* pBackupQueue =
                                (cmdBuf.IsProtected() && m_tmzPerQueue) ?
                                m_pPalBackupTmzQueues[deviceIdx] :
                                m_pPalBackupQueues[deviceIdx];

                            //prepare backup submit info
                            Pal::PerSubQueueSubmitInfo backupPerSubQueueInfo = {};
                            backupPerSubQueueInfo.cmdBufferCount = 0;
                            backupPerSubQueueInfo.ppCmdBuffers = &pBackupPalCmdBuffer;
                            Pal::SubmitInfo backupPalSubmitInfo = {};
                            backupPalSubmitInfo.pPerSubQueueInfo = &backupPerSubQueueInfo;
                            backupPalSubmitInfo.perSubQueueInfoCount = 1;

                            if (palResult == Pal::Result::Success)
                            {
                                palResult = pMainQueue->Submit(palSubmitInfo);
                            }

                            backupPerSubQueueInfo.cmdBufferCount = 1;
                            pBackupPalCmdBuffer = cmdBuf.BackupPalCmdBuffer(deviceIdx);
                            if (palResult == Pal::Result::Success)
                            {
                                palResult = pMainQueue->SignalQueueSemaphore(
                                        m_pSwitchToPalBackupSemaphore[deviceIdx],
                                        0);

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = pBackupQueue->WaitQueueSemaphore(
                                        m_pSwitchToPalBackupSemaphore[deviceIdx],
                                        0);
                                }

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = pBackupQueue->Submit(backupPalSubmitInfo);
                                }

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = pBackupQueue->SignalQueueSemaphore(
                                        m_pSwitchFromPalBackupSemaphore[deviceIdx],
                                        0);
                                }

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = pMainQueue->WaitQueueSemaphore(
                                        m_pSwitchFromPalBackupSemaphore[deviceIdx],
                                        0);
                                }

                                perSubQueueInfo.cmdBufferCount = 0;
                            }
                        }

                        const uint32_t stackSizeInDwords =
                            Util::NumBytesToNumDwords(cmdBuf.PerGpuState(deviceIdx)->maxPipelineStackSize);

                        palSubmitInfo.stackSizeInDwords =
                            Util::Max(palSubmitInfo.stackSizeInDwords, stackSizeInDwords);
                    }
                    else
                    {
                        VK_NEVER_CALLED();
                    }
                }

                if (lastBatch && (pFence != nullptr))
                {
                    pPalFence = pFence->PalFence(deviceIdx);
                    palSubmitInfo.ppFences   = &pPalFence;
                    palSubmitInfo.fenceCount = 1;

                    pFence->SetActiveDevice(deviceIdx);
                }

                if ((perSubQueueInfo.cmdBufferCount > 0) ||
                    (palSubmitInfo.fenceCount > 0)     ||
                    (waitSemaphoreCount > 0))
                {
                    if (timedQueueEvents == false)
                    {
                        const Pal::DeviceProperties& deviceProps =
                            m_pDevice->VkPhysicalDevice(deviceIdx)->PalProperties();

                        // PerQueue level tmz submission supported. Prepare this path for compute engine.
                        if (m_tmzPerQueue)
                        {
                            bool isProtected = false;

                            if (perSubQueueInfo.cmdBufferCount > 0)
                            {
                                const CmdBuffer& cmdBuf = *(*pCommandBuffers[0]);
                                isProtected = cmdBuf.IsProtected();
                            }

                            if (isProtected)
                            {
                                // If the previous submit is non-tmz, but the current submit is tmz, trigger queue switch.
                                // Add semaphore between tmz and none-tmz queue, tmz queue wait non-tmz queue done.
                                if (m_lastSubmissionProtected == false)
                                {
                                    palResult = PalQueue(deviceIdx)->SignalQueueSemaphore(m_pPalTmzSemaphore[deviceIdx], 0);

                                    if (palResult == Pal::Result::Success)
                                    {
                                        palResult =
                                            PalTmzQueue(deviceIdx)->WaitQueueSemaphore(m_pPalTmzSemaphore[deviceIdx], 0);
                                    }
                                }

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = PalTmzQueue(deviceIdx)->Submit(palSubmitInfo);
                                }

                                VK_ASSERT(palResult == Pal::Result::Success);
                                // After submiting TMZ commands, set previous submission state to TMZ
                                m_lastSubmissionProtected = true;
                            }
                            else
                            {
                                // If the previous submit is tmz, but the current submit is non-tmz, trigger queue switch.
                                // Add semaphore between tmz and none-tmz queue, non-tmz queue wait tmz queue done.
                                if (m_lastSubmissionProtected)
                                {
                                    palResult =
                                        PalTmzQueue(deviceIdx)->SignalQueueSemaphore(m_pPalTmzSemaphore[deviceIdx], 0);

                                    if (palResult == Pal::Result::Success)
                                    {
                                        palResult =
                                            PalQueue(deviceIdx)->WaitQueueSemaphore(m_pPalTmzSemaphore[deviceIdx], 0);
                                    }
                                }

                                if (palResult == Pal::Result::Success)
                                {
                                    palResult = PalQueue(deviceIdx)->Submit(palSubmitInfo);
                                }

                                VK_ASSERT(palResult == Pal::Result::Success);
                                // After submiting non-TMZ commands, set previous submission state to non-TMZ
                                m_lastSubmissionProtected = false;
                            }
                        }
                        else
                        {
                            palResult = PalQueue(deviceIdx)->Submit(palSubmitInfo);
                        }
                    }
                    else
                    {
#if ICD_GPUOPEN_DEVMODE_BUILD
                        // TMZ is NOT supported for GPUOPEN path.
                        VK_ASSERT((*pCommandBuffers[0])->IsProtected() == false);

                        palResult = m_pDevModeMgr->TimedQueueSubmit(
                            deviceIdx,
                            this,
                            cmdBufferCount,
                            pCmdBuffers,
                            palSubmitInfo,
                            &virtStackFrame);
#else
                        VK_NEVER_CALLED();
#endif
                    }

                    result = PalToVkResult(palResult);
                }

            }

            if (pCmdBufInfos != nullptr)
            {
                virtStackFrame.FreeArray(pCmdBufInfos);
            }

            if (isSynchronization2 && (pCmdBuffers != nullptr))
            {
                virtStackFrame.FreeArray(pCmdBuffers);
            }

            virtStackFrame.FreeArray(pPalCmdBuffers);

            if (isSynchronization2)
            {
                const VkSubmitInfo2KHR* pSubmitInfoKhr =
                    reinterpret_cast<const VkSubmitInfo2KHR*>(&pSubmits[submitIdx]);

                if ((result == VK_SUCCESS) && (pSubmitInfoKhr->signalSemaphoreInfoCount > 0))
                {
                    signalValueCount = pSubmitInfoKhr->signalSemaphoreInfoCount;

                    VkSemaphore* pSignalSemaphores          = virtStackFrame.AllocArray<VkSemaphore>(signalValueCount);
                    uint64_t* pSignalSemaphoreInfoValues    = virtStackFrame.AllocArray<uint64_t>(signalValueCount);
                    uint32_t* pSignalSemaphoreDeviceIndices = virtStackFrame.AllocArray<uint32_t>(signalValueCount);

                    for (uint32_t i = 0; i < signalValueCount; i++)
                    {
                        pSignalSemaphores[i]             = pSubmitInfoKhr->pSignalSemaphoreInfos[i].semaphore;
                        pSignalSemaphoreInfoValues[i]    = pSubmitInfoKhr->pSignalSemaphoreInfos[i].value;
                        pSignalSemaphoreDeviceIndices[i] = pSubmitInfoKhr->pSignalSemaphoreInfos[i].deviceIndex;
                    }

                    result = PalSignalSemaphores(
                        signalValueCount,
                        pSignalSemaphores,
                        pSignalSemaphoreInfoValues,
                        signalValueCount,
                        pSignalSemaphoreDeviceIndices);

                    virtStackFrame.FreeArray(pSignalSemaphores);
                    virtStackFrame.FreeArray(pSignalSemaphoreInfoValues);
                    virtStackFrame.FreeArray(pSignalSemaphoreDeviceIndices);
                }
            }
            else
            {
                const VkSubmitInfo* pSubmitInfoOld = reinterpret_cast<const VkSubmitInfo*>(&pSubmits[submitIdx]);

                if ((result == VK_SUCCESS) && (pSubmitInfoOld->signalSemaphoreCount > 0))
                {
                    VK_ASSERT((pSignalSemaphoreValues == nullptr) ||
                              (pSubmitInfoOld->signalSemaphoreCount == signalValueCount));

                    result = PalSignalSemaphores(
                        pSubmitInfoOld->signalSemaphoreCount,
                        pSubmitInfoOld->pSignalSemaphores,
                        pSignalSemaphoreValues,
                        (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->signalSemaphoreCount          : 0),
                        (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->pSignalSemaphoreDeviceIndices : nullptr));
                }
            }

            DebugPrintf::PostQueueSubmit(m_pDevice, pCmdBuffers, cmdBufferCount);

#if VKI_RAY_TRACING
#endif
        }
    }

    return result;
}

// =====================================================================================================================
// Wait for a queue to go idle
VkResult Queue::WaitIdle(void)
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        palResult = PalQueue(deviceIdx)->WaitIdle();
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Signal a queue semaphore
// If semaphoreCount > semaphoreDeviceIndicesCount, the last device index will be used for the remaining semaphores.
// This can be used with semaphoreDeviceIndicesCount = 1 to signal all semaphores from the same device.
VkResult Queue::PalSignalSemaphores(
    uint32_t            semaphoreCount,
    const VkSemaphore*  pSemaphores,
    const uint64_t*     pSemaphoreValues,
    const uint32_t      semaphoreDeviceIndicesCount,
    const uint32_t*     pSemaphoreDeviceIndices)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) &&
                             pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif

    Pal::Result palResult = Pal::Result::Success;
    uint32_t    deviceIdx = DefaultDeviceIndex;

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        if (pSemaphores[i] != VK_NULL_HANDLE)
        {
            if (i < semaphoreDeviceIndicesCount)
            {
                VK_ASSERT(pSemaphoreDeviceIndices != nullptr);
                deviceIdx = pSemaphoreDeviceIndices[i];
            }

            VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

            Semaphore* pVkSemaphore = Semaphore::ObjectFromHandle(pSemaphores[i]);
            Pal::IQueueSemaphore* pPalSemaphore = pVkSemaphore->PalSemaphore(deviceIdx);
            uint64_t              pointValue    = 0;

            if (pVkSemaphore->IsTimelineSemaphore())
            {
                if (pSemaphoreValues == nullptr)
                {
                    palResult = Pal::Result::ErrorInvalidPointer;
                    break;
                }
                else
                {
                    VK_ASSERT(pSemaphoreValues[i] != 0);
                    pointValue = pSemaphoreValues[i];
                }
            }

            if (timedQueueEvents == false)
            {
                palResult = PalQueue(deviceIdx)->SignalQueueSemaphore(pPalSemaphore, pointValue);
            }
            else
            {
#if ICD_GPUOPEN_DEVMODE_BUILD
                palResult = pDevModeMgr->TimedSignalQueueSemaphore(deviceIdx, this, pSemaphores[i], pointValue,
                                                                   pPalSemaphore);
#else
                VK_NEVER_CALLED();

                palResult = Pal::Result::ErrorUnknown;
#endif
            }
        }
    }

    return  (palResult == Pal::Result::ErrorUnknown) ? VK_ERROR_DEVICE_LOST : PalToVkResult(palResult);
}

// =====================================================================================================================
// Wait for a queue semaphore to become signaled
// If semaphoreCount > semaphoreDeviceIndicesCount, the last device index will be used for the remaining semaphores.
// This can be used with semaphoreDeviceIndicesCount = 1 to signal all semaphores from the same device.
VkResult Queue::PalWaitSemaphores(
    uint32_t            semaphoreCount,
    const VkSemaphore*  pSemaphores,
    const uint64_t*     pSemaphoreValues,
    const uint32_t      semaphoreDeviceIndicesCount,
    const uint32_t*     pSemaphoreDeviceIndices)
{
    Pal::Result palResult = Pal::Result::Success;
    uint32_t    deviceIdx = DefaultDeviceIndex;

#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) &&
                             pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        if (pSemaphores[i] != VK_NULL_HANDLE)
        {
            Semaphore*  pSemaphore              = Semaphore::ObjectFromHandle(pSemaphores[i]);
            Pal::IQueueSemaphore* pPalSemaphore = nullptr;
            uint64_t              pointValue    = 0;

            if (pSemaphore->IsTimelineSemaphore())
            {
                if (pSemaphoreValues == nullptr)
                {
                    palResult = Pal::Result::ErrorInvalidPointer;
                    break;
                }
                else
                {
                    pointValue = pSemaphoreValues[i];
                }
            }

            if (i < semaphoreDeviceIndicesCount)
            {
                VK_ASSERT(pSemaphoreDeviceIndices != nullptr);
                deviceIdx = pSemaphoreDeviceIndices[i];
            }

            VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

            // Wait for the semaphore.
            pPalSemaphore = pSemaphore->PalSemaphore(deviceIdx);
            pSemaphore->RestoreSemaphore();

            if (pPalSemaphore != nullptr)
            {
                if (timedQueueEvents == false)
                {
                    palResult = PalQueue(deviceIdx)->WaitQueueSemaphore(pPalSemaphore, pointValue);
                }
                else
                {
#if ICD_GPUOPEN_DEVMODE_BUILD
                    palResult = pDevModeMgr->TimedWaitQueueSemaphore(deviceIdx, this, pSemaphores[i], pointValue,
                                                                     pPalSemaphore);
#else
                    VK_NEVER_CALLED();

                    palResult = Pal::Result::ErrorUnknown;
#endif
                }
            }
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Update present flip status
VkResult Queue::UpdateFlipStatus(
    const Pal::PresentSwapChainInfo* pPresentInfo,
    const SwapChain*                 pSwapChain)
{
    bool isOwner = false;
    Pal::IDevice* pPalDevice = m_pDevice->PalDevice(DefaultDeviceIndex);
    uint32_t vidPnSourceId = pSwapChain->GetFullscreenMgr()->GetVidPnSourceId();

    Pal::Result palResult = pPalDevice->GetFlipStatus(vidPnSourceId, &m_flipStatus.flipFlags, &isOwner);
    if (palResult == Pal::Result::Success)
    {
        m_flipStatus.isValid     = true;
        m_flipStatus.isFlipOwner = isOwner;
    }
    else
    {
        memset(&m_flipStatus, 0, sizeof(m_flipStatus));
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Return true if pace present is needed, should sync flip (timer queue hold present queue) if pSyncFlip is true
bool Queue::NeedPacePresent(
    Pal::PresentSwapChainInfo* pPresentInfo,
    SwapChain*                 pSwapChain,
    bool*                      pSyncFlip,
    bool*                      pPostFrameTimerSubmission)
{
    VkResult result = VK_SUCCESS;

    return false;
}

// =====================================================================================================================
// Present a swap chain image
VkResult Queue::Present(
    const VkPresentInfoKHR* pPresentInfo)
{
    uint32_t presentationDeviceIdx = 0;
    bool     needSemaphoreFlush    = false;

    const VkPresentRegionsKHR* pVkRegions = nullptr;
    const VkDeviceGroupPresentInfoKHR* pDeviceGroupPresentInfoKHR = nullptr;

    if (pPresentInfo == nullptr)
    {
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameEnd(this,
                                                                     DevModeMgr::FrameDelimiterType::QueuePresent);
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameBegin(this,
                                                                       DevModeMgr::FrameDelimiterType::QueuePresent);
        }
#endif
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const void* pNext = pPresentInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR:
        {
            pDeviceGroupPresentInfoKHR = static_cast<const VkDeviceGroupPresentInfoKHR*>(pNext);
            VK_ASSERT(pDeviceGroupPresentInfoKHR->swapchainCount == 1);
            const uint32_t deviceMask = *pDeviceGroupPresentInfoKHR->pDeviceMasks;
            VK_ASSERT(Util::CountSetBits(deviceMask) == 1);
            Util::BitMaskScanForward(&presentationDeviceIdx, deviceMask);
            break;
        }
        case VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR:
        {
            pVkRegions = static_cast<const VkPresentRegionsKHR*>(pNext);
            VK_ASSERT(pVkRegions->swapchainCount == pPresentInfo->swapchainCount);
            break;
        }
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }

    VkResult result = VK_SUCCESS;

    // Query driver feature settings that could change from frame to frame.
    uint32_t rsFeaturesChangedMask = 0;
    {
        uint32_t rsFeaturesQueriedMask = 0;

#if VKI_RAY_TRACING
#endif

        Pal::Result palResult = m_pDevice->PalDevice(DefaultDeviceIndex)->DidRsFeatureSettingsChange(
            rsFeaturesQueriedMask,
            &rsFeaturesChangedMask);

        if ((palResult == Pal::Result::Success) && (rsFeaturesChangedMask != 0))
        {
            // Update the feature settings from the app profile or the global settings.
            m_pDevice->UpdateFeatureSettings();
        }
    }

    if (pPresentInfo->waitSemaphoreCount > 0)
    {
        result = PalWaitSemaphores(
            pPresentInfo->waitSemaphoreCount,
            pPresentInfo->pWaitSemaphores,
            nullptr,
            0,
            nullptr);

#if __unix__
        // On Linux, a submission is required for queuePresent wait semaphore. The present buffer object is implicitly
        // synced BO, which means its fence must be updated through a submission to ensure the compositing commands are
        // executed after the rendering.
        needSemaphoreFlush = (result == VK_SUCCESS) ? true : false;
#endif
    }

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Get presentable image object
    for (uint32_t curSwapchain = 0; curSwapchain < pPresentInfo->swapchainCount; curSwapchain++)
    {
        // Get the swap chain
        SwapChain* pSwapChain = SwapChain::ObjectFromHandle(pPresentInfo->pSwapchains[curSwapchain]);

        // Get the presentable image index
        const uint32_t imageIndex = pPresentInfo->pImageIndices[curSwapchain];

        Pal::PresentSwapChainInfo presentInfo = {};

        Pal::Rect* pPresentRects = nullptr;

        if ((pVkRegions != nullptr) &&
            (pVkRegions->pRegions != nullptr))
        {
            const VkPresentRegionKHR* pVkRegion = &pVkRegions->pRegions[curSwapchain];

            if ((pVkRegion->rectangleCount > 0) &&
                (pVkRegion->pRectangles != nullptr))
            {
                pPresentRects = virtStackFrame.AllocArray<Pal::Rect>(pVkRegion->rectangleCount);
                if (pPresentRects == nullptr)
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    break;
                }
                for (uint32_t r = 0; r < pVkRegion->rectangleCount; ++r)
                {
                    const VkRectLayerKHR& rectLayer = pVkRegion->pRectangles[r];
                    const VkRect2D        rect2D = { rectLayer.offset, rectLayer.extent };
                    pPresentRects[r] = VkToPalRect(rect2D);
                }
                presentInfo.rectangleCount = pVkRegion->rectangleCount;
                presentInfo.pRectangles    = pPresentRects;
            }
        }

        // Fill in present information and obtain the PAL memory of the presentable image.
        Pal::IGpuMemory* pGpuMemory = pSwapChain->UpdatePresentInfo(presentationDeviceIdx,
                                                                    imageIndex,
                                                                    &presentInfo,
                                                                    m_flipStatus.flipFlags);

        CmdBufState* pCmdBufState = m_pCmdBufferRing->AcquireCmdBuffer(m_pDevice, presentationDeviceIdx);

        // Ensure metadata is available before post processing.
        if (pSwapChain->GetFullscreenMgr() != nullptr)
        {
            Pal::Result palResult = m_pDevice->PalDevice(DefaultDeviceIndex)->PollFullScreenFrameMetadataControl(
                pSwapChain->GetFullscreenMgr()->GetVidPnSourceId(),
                &m_palFrameMetadataControl);

            VK_ASSERT(palResult == Pal::Result::Success);
        }

        // This must happen after the fullscreen manager has updated its overlay information and before the software
        // compositor has an opportunity to copy the presentable image in order to include the overlay itself.
        bool hasPostProcessing = BuildPostProcessCommands(presentationDeviceIdx,
                                                          pCmdBufState,
                                                          nullptr,
                                                          &presentInfo,
                                                          pSwapChain);

        // For MGPU, the swapchain and device properties might perform software composition and return
        // a different presentation device for the present of the intermediate surface.
        Pal::IQueue* pPresentQueue = pSwapChain->PrePresent(presentationDeviceIdx,
                                                            &presentInfo,
                                                            &pGpuMemory,
                                                            this,
                                                            pCmdBufState,
                                                            &hasPostProcessing);

        // Notify gpuopen developer mode that we're about to present (frame-end boundary)
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameEnd(this,
                                                                     DevModeMgr::FrameDelimiterType::QueuePresent);
        }
#endif

        bool syncFlip = false;
        bool postFrameTimerSubmission = false;
        bool needFramePacing = NeedPacePresent(&presentInfo, pSwapChain, &syncFlip, &postFrameTimerSubmission);
        bool skipFsfmFlags   = false;

        result = NotifyFlipMetadataBeforePresent(presentationDeviceIdx,
                                                 pPresentQueue,
                                                 &presentInfo,
                                                 (hasPostProcessing ? pCmdBufState : nullptr),
                                                 pGpuMemory,
                                                 needSemaphoreFlush,
                                                 skipFsfmFlags);

        if (result != VK_SUCCESS)
        {
            break;
        }
        else
        {
            needSemaphoreFlush = false;
        }

        // Perform the actual present
        Pal::Result palResult = pPresentQueue->PresentSwapChain(presentInfo);

        result = NotifyFlipMetadataAfterPresent(presentationDeviceIdx, &presentInfo);

        if (result != VK_SUCCESS)
        {
            break;
        }

        // Notify swap chain that a present occurred
        pSwapChain->PostPresent(presentInfo, &palResult);

        // Notify gpuopen developer mode that a present occurred (frame-begin boundary)
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameBegin(this,
                                                                       DevModeMgr::FrameDelimiterType::QueuePresent);
        }
#endif

        VkResult curResult = PalToVkResult(palResult);

        if ((curResult == VK_SUCCESS) && pSwapChain->IsSuboptimal(presentationDeviceIdx))
        {
            curResult = VK_SUBOPTIMAL_KHR;
        }

        if ((curResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) &&
             ((pSwapChain->GetFullscreenMgr() == nullptr) ||
              (pSwapChain->GetFullscreenMgr()->GetFullScreenMode() != FullscreenMgr::Explicit)))
        {
            curResult = VK_SUCCESS;
        }

        if (pPresentInfo->pResults)
        {
            pPresentInfo->pResults[curSwapchain] = curResult;
        }

        // We need to keep track of the most severe error code reported and
        // make sure that's the one we ultimately return
        if ((curResult == VK_ERROR_DEVICE_LOST) ||
            (curResult == VK_ERROR_SURFACE_LOST_KHR && result != VK_ERROR_DEVICE_LOST) ||
            (curResult == VK_ERROR_OUT_OF_DATE_KHR && result != VK_ERROR_SURFACE_LOST_KHR) ||
            (curResult == VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR &&
                result != VK_ERROR_SURFACE_LOST_KHR))
        {
            result = curResult;
        }
        else if (curResult != VK_SUCCESS)
        {
            VK_ASSERT(!"Unexpected VK ERROR set, check spec to ensure it is valid.");

            result = VK_ERROR_DEVICE_LOST;
        }

        if (pPresentRects != nullptr)
        {
            virtStackFrame.FreeArray(pPresentRects);
        }
    }

#if VKI_RAY_TRACING
#endif

    return result;
}

// =====================================================================================================================
// Adds an entry to the remap range array.
VkResult Queue::AddVirtualRemapRange(
    uint32_t           resourceDeviceIndex,
    Pal::IGpuMemory*   pVirtualGpuMem,
    VkDeviceSize       virtualOffset,
    Pal::IGpuMemory*   pRealGpuMem,
    VkDeviceSize       realOffset,
    VkDeviceSize       size,
    VirtualRemapState* pRemapState,
    bool               noWait)
{
    VkResult result = VK_SUCCESS;

    VK_ASSERT(pRemapState->rangeCount < pRemapState->maxRangeCount);

    Pal::VirtualMemoryRemapRange* pRemapRange = &pRemapState->pRanges[pRemapState->rangeCount++];

    if (m_pDevice->VkPhysicalDevice(resourceDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
    {
        pRemapRange->virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
    }
    else
    {
        pRemapRange->virtualAccessMode = Pal::VirtualGpuMemAccessMode::Undefined;
    }

    pRemapRange->pVirtualGpuMem     = pVirtualGpuMem;
    pRemapRange->virtualStartOffset = virtualOffset;
    pRemapRange->pRealGpuMem        = pRealGpuMem;
    pRemapRange->realStartOffset    = realOffset;
    pRemapRange->size               = size;

    // If we've hit our limit of batched remaps, send them to PAL and reset
    if (pRemapState->rangeCount >= pRemapState->maxRangeCount)
    {
        result = CommitVirtualRemapRanges(resourceDeviceIndex, pRemapState, noWait);
    }

    return result;
}

// =====================================================================================================================
// Sends any pending virtual remap ranges to PAL and resets the state.  This function also handles remap fence
// signaling if requested.
VkResult Queue::CommitVirtualRemapRanges(
    uint32_t           deviceIndex,
    VirtualRemapState* pRemapState,
    bool               noWait)
{
    Pal::Result result = Pal::Result::Success;

    if (pRemapState->rangeCount > 0)
    {
        result = PalQueue(deviceIndex)->RemapVirtualMemoryPages(
            pRemapState->rangeCount,
            pRemapState->pRanges,
            noWait,
            nullptr);

        pRemapState->rangeCount = 0;
    }

    return (result == Pal::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Generate virtual remap entries for a single bind sparse info record
VkResult Queue::BindSparseEntry(
    const VkBindSparseInfo& bindInfo,
    uint32_t                resourceDeviceIndex,
    uint32_t                memoryDeviceIndex,
    VkDeviceSize            prtTileSize,
    VirtualRemapState*      pRemapState,
    bool                    noWait)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t j = 0; j < bindInfo.bufferBindCount; ++j)
    {
        const auto& bufBindInfo = bindInfo.pBufferBinds[j];
        const Buffer& buffer = *Buffer::ObjectFromHandle(bufBindInfo.buffer);

        VK_ASSERT(buffer.IsSparse());

        Pal::IGpuMemory* pVirtualGpuMem = buffer.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < bufBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = bufBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                Memory* pMemory = Memory::ObjectFromHandle(bind.memory);

                pRealGpuMem = pMemory->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            VK_ASSERT(bind.flags == 0);

            result = AddVirtualRemapRange(
                resourceDeviceIndex,
                pVirtualGpuMem,
                bind.resourceOffset,
                pRealGpuMem,
                bind.memoryOffset,
                bind.size,
                pRemapState,
                noWait);

            if (result != VK_SUCCESS)
            {
                goto End;
            }
        }
    }

    for (uint32_t j = 0; j < bindInfo.imageOpaqueBindCount; ++j)
    {
        const auto& imgBindInfo = bindInfo.pImageOpaqueBinds[j];
        const Image& image = *Image::ObjectFromHandle(imgBindInfo.image);

        VK_ASSERT(image.IsSparse());

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = imgBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                Memory* pMemory = Memory::ObjectFromHandle(bind.memory);

                pRealGpuMem = pMemory->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            result = AddVirtualRemapRange(
                resourceDeviceIndex,
                pVirtualGpuMem,
                bind.resourceOffset,
                pRealGpuMem,
                bind.memoryOffset,
                bind.size,
                pRemapState,
                noWait);

            if (result != VK_SUCCESS)
            {
                goto End;
            }
        }
    }

    for (uint32_t j = 0; j < bindInfo.imageBindCount; ++j)
    {
        const auto& imgBindInfo = bindInfo.pImageBinds[j];
        const Image& image = *Image::ObjectFromHandle(imgBindInfo.image);

        VK_ASSERT(image.IsSparse());

        const VkExtent3D& tileSize = image.GetTileSize();

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseImageMemoryBind& bind = imgBindInfo.pBinds[k];

            VK_ASSERT(bind.flags == 0);

            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                Memory* pMemory = Memory::ObjectFromHandle(bind.memory);

                pRealGpuMem = pMemory->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            // Get the subresource layout to be able to figure out its offset
            Pal::Result       palResult;
            Pal::SubresLayout subResLayout = {};
            Pal::SubresId     subResId     = {};

            subResId.plane      = VkToPalImagePlaneSingle(image.GetFormat(),
                bind.subresource.aspectMask, m_pDevice->GetRuntimeSettings());

            subResId.mipLevel   = bind.subresource.mipLevel;
            subResId.arraySlice = bind.subresource.arrayLayer;

            palResult = image.PalImage(resourceDeviceIndex)->GetSubresourceLayout(subResId, &subResLayout);

            if (palResult != Pal::Result::Success)
            {
                goto End;
            }

            // Calculate subresource row and depth pitch in tiles
            // In Gfx9, the tiles within same mip level may not continuous thus we have to take
            // the mipChainPitch into account when calculate the offset of next tile.
            VkDeviceSize prtTileRowPitch   = subResLayout.rowPitch * tileSize.height * tileSize.depth;
            VkDeviceSize prtTileDepthPitch = subResLayout.depthPitch * tileSize.depth;

            // tileSize is in texels, prtTileRowPitch and prtTileDepthPitch shall be adjusted for compressed
            // formats. depth of blockDim will always be 1 so skip the adjustment.
            const VkFormat aspectFormat = vk::Formats::GetAspectFormat(image.GetFormat(), bind.subresource.aspectMask);
            const Pal::ChNumFormat palAspectFormat =
                VkToPalFormat(aspectFormat, m_pDevice->GetRuntimeSettings()).format;

            if (Pal::Formats::IsBlockCompressed(palAspectFormat))
            {
                Pal::Extent3d blockDim = Pal::Formats::CompressedBlockDim(palAspectFormat);
                VK_ASSERT(blockDim.depth == 1);
                prtTileRowPitch /= blockDim.height;
            }

            // Calculate the offsets in tiles
            const VkOffset3D offsetInTiles =
            {
                bind.offset.x / static_cast<int32_t>(tileSize.width),
                bind.offset.y / static_cast<int32_t>(tileSize.height),
                bind.offset.z / static_cast<int32_t>(tileSize.depth)
            };

            // Calculate the extents in tiles
            const VkExtent3D extentInTiles =
            {
                Util::RoundUpToMultiple(bind.extent.width,  tileSize.width) / tileSize.width,
                Util::RoundUpToMultiple(bind.extent.height, tileSize.height) / tileSize.height,
                Util::RoundUpToMultiple(bind.extent.depth,  tileSize.depth) / tileSize.depth
            };

            // Calculate byte size to remap per row
            VkDeviceSize sizePerRow = extentInTiles.width * prtTileSize;
            VkDeviceSize realOffset = bind.memoryOffset;

            const VkDeviceSize tileOffsetX = offsetInTiles.x * prtTileSize;
            const VkDeviceSize tileOffsetY = offsetInTiles.y * prtTileRowPitch;
            const VkDeviceSize tileOffsetZ = offsetInTiles.z * prtTileDepthPitch;

            const VkDeviceSize virtualOffsetBase = subResLayout.offset + tileOffsetX + tileOffsetY + tileOffsetZ;

            for (uint32_t tileZ = 0; tileZ < extentInTiles.depth; ++tileZ)
            {
                const VkDeviceSize virtualOffsetBaseY = virtualOffsetBase + tileZ * prtTileDepthPitch;

                for (uint32_t tileY = 0; tileY < extentInTiles.height; ++tileY)
                {
                    const VkDeviceSize virtualOffset = virtualOffsetBaseY + tileY * prtTileRowPitch;

                    result = AddVirtualRemapRange(
                        resourceDeviceIndex,
                        pVirtualGpuMem,
                        virtualOffset,
                        pRealGpuMem,
                        realOffset,
                        sizePerRow,
                        pRemapState,
                        noWait);

                    if (result != VK_SUCCESS)
                    {
                        goto End;
                    }

                    realOffset += sizePerRow;
                }
            }
        }
    }

End:
    return result;
}

// =====================================================================================================================
// Peek the resource and memory device indices from a chained VkDeviceGroupBindSparseInfo structure.
static void PeekDeviceGroupBindSparseDeviceIndicesAndSemaphoreInfo(
    const VkBindSparseInfo& bindInfo,
    uint32_t*               pResourceDeviceIndex,
    uint32_t*               pMemoryDeviceIndex,
    const VkTimelineSemaphoreSubmitInfo** ppTimelineSemaphoreInfo)
{
    const void* pNext = bindInfo.pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO:
            {
                const auto* pExtInfo = static_cast<const VkDeviceGroupBindSparseInfo*>(pNext);

                if (pResourceDeviceIndex != nullptr)
                {
                    *pResourceDeviceIndex = pExtInfo->resourceDeviceIndex;
                }
                if (pMemoryDeviceIndex != nullptr)
                {
                    *pMemoryDeviceIndex = pExtInfo->memoryDeviceIndex;
                }

                break;
            }
            case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
            {
                const auto* pExtInfo = static_cast<const VkTimelineSemaphoreSubmitInfo*>(pNext);
                *ppTimelineSemaphoreInfo = pExtInfo;
                break;
            }
            default:
                // Skip any unknown extension structures
                break;
        }

        pNext = pHeader->pNext;
    }
}
// =====================================================================================================================
// Update sparse bindings.
VkResult Queue::BindSparse(
    uint32_t                bindInfoCount,
    const VkBindSparseInfo* pBindInfo,
    VkFence                 fence)
{
    VkResult result = VK_SUCCESS;

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Initialize state to track batches of sparse bind calls
    VirtualRemapState remapState = {};

    // Max number of sparse bind operations per batch
    constexpr uint32_t MaxVirtualRemapRangesPerBatch = 1024;

    remapState.maxRangeCount =
        Util::Min(
            MaxVirtualRemapRangesPerBatch,
            static_cast<uint32_t>(m_pStackAllocator->Remaining() / sizeof(Pal::VirtualMemoryRemapRange)));

    // Allocate temp memory for one batch of remaps
    remapState.pRanges = virtStackFrame.AllocArray<Pal::VirtualMemoryRemapRange>(remapState.maxRangeCount);

    if (remapState.pRanges == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Fence will be signalled after the remaps
    uint32_t signalFenceDeviceMask = 0;

    for (uint32_t i = 0; (i < bindInfoCount) && (result == VK_SUCCESS); ++i)
    {
        const bool  lastEntry               = (i == (bindInfoCount - 1));
        const auto& bindInfo                = pBindInfo[i];
        uint32_t    resourceDeviceIndex     = DefaultDeviceIndex;
        uint32_t    memoryDeviceIndex       = DefaultDeviceIndex;
        uint32_t    nextResourceDeviceIndex = DefaultDeviceIndex;
        bool        noWait                  = (bindInfo.waitSemaphoreCount == 0);
        const VkTimelineSemaphoreSubmitInfo* pTimelineSemaphoreInfo = nullptr;
        const VkTimelineSemaphoreSubmitInfo* pNextTimelineSemaphoreInfo = nullptr;

        PeekDeviceGroupBindSparseDeviceIndicesAndSemaphoreInfo(bindInfo, &resourceDeviceIndex, &memoryDeviceIndex,
                                                               &pTimelineSemaphoreInfo);

        if (!lastEntry)
        {
            PeekDeviceGroupBindSparseDeviceIndicesAndSemaphoreInfo(pBindInfo[i + 1], &nextResourceDeviceIndex, nullptr,
                                                                   &pNextTimelineSemaphoreInfo);
        }
        // Keep track of the PAL devices that need to signal the fence
        signalFenceDeviceMask |= (1 << resourceDeviceIndex);

        if (bindInfo.waitSemaphoreCount > 0)
        {
            VK_ASSERT((pTimelineSemaphoreInfo == nullptr) ||
                      (bindInfo.waitSemaphoreCount == pTimelineSemaphoreInfo->waitSemaphoreValueCount));
            result = PalWaitSemaphores(
                    bindInfo.waitSemaphoreCount,
                    bindInfo.pWaitSemaphores,
                    ((pTimelineSemaphoreInfo != nullptr) ? pTimelineSemaphoreInfo->pWaitSemaphoreValues : nullptr),
                    1,  // number of device indices
                    &resourceDeviceIndex);
        }

        if (result == VK_SUCCESS)
        {
            // Byte size of a PRT sparse tile
            const VkDeviceSize prtTileSize =
                m_pDevice->VkPhysicalDevice(resourceDeviceIndex)->PalProperties().imageProperties.prtTileSize;

            result = BindSparseEntry(
                bindInfo,
                resourceDeviceIndex,
                memoryDeviceIndex,
                prtTileSize,
                &remapState,
                noWait);
        }

        // Commit any batched remap operations immediately if:
        // - this is the last batch,
        // - resourceDeviceIndex is going to change,
        // - the app is requesting us to signal a queue semaphore.
        if (lastEntry || (nextResourceDeviceIndex != resourceDeviceIndex) || (bindInfo.signalSemaphoreCount > 0))
        {
            // Commit any remaining remaps
            if (result == VK_SUCCESS)
            {
                result = CommitVirtualRemapRanges(resourceDeviceIndex, &remapState, noWait);
            }

            // Signal any semaphores depending on the preceding remap operations
            if (result == VK_SUCCESS)
            {
                VK_ASSERT((pTimelineSemaphoreInfo == nullptr) ||
                          (bindInfo.signalSemaphoreCount == pTimelineSemaphoreInfo->signalSemaphoreValueCount));
                result = PalSignalSemaphores(
                    bindInfo.signalSemaphoreCount,
                    bindInfo.pSignalSemaphores,
                    ((pTimelineSemaphoreInfo != nullptr) ? pTimelineSemaphoreInfo->pSignalSemaphoreValues : nullptr),
                    1,  // number of device indices
                    &resourceDeviceIndex);
            }
        }
    }

    // Clean up
    VK_ASSERT((remapState.rangeCount == 0) || (result != VK_SUCCESS));

    if ((result == VK_SUCCESS) && (fence != VK_NULL_HANDLE))
    {
        // Signal the fence in the devices that have binding request.
        // If there is no bindInfo, we just signal the fence in the DeviceDeviceIndex.
        if (signalFenceDeviceMask == 0)
        {
            signalFenceDeviceMask = 1 << DefaultDeviceIndex;
        }

        Pal::IFence*               pPalFence       = nullptr;
        Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
        Pal::SubmitInfo            submitInfo      = {};

        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.perSubQueueInfoCount = 1;

        Fence* pFence = Fence::ObjectFromHandle(fence);

        utils::IterateMask deviceGroup(signalFenceDeviceMask);

        do
        {
            const uint32_t deviceIndex = deviceGroup.Index();
            pPalFence = pFence->PalFence(deviceIndex);
            submitInfo.ppFences = &pPalFence;
            submitInfo.fenceCount = 1;

            // set the active device mask for the fence.
            // the following fence wait would only be applied to the fence one the active device index.
            // the active device index would be cleared in the reset.
            pFence->SetActiveDevice(deviceIndex);

            result = PalToVkResult(PalQueue(deviceIndex)->Submit(submitInfo));
        }
        while ((result == VK_SUCCESS) && deviceGroup.IterateNext());
    }

    virtStackFrame.FreeArray(remapState.pRanges);

    return result;
}

// =====================================================================================================================
// Build post processing commands
bool Queue::BuildPostProcessCommands(
    uint32_t                         deviceIdx,
    CmdBufState*                     pCmdBufState,
    const Pal::IImage*               pImage,
    const Pal::PresentSwapChainInfo* pPresentInfo,
    const SwapChain*                 pSwapChain)
{
    VK_ASSERT(((pPresentInfo != nullptr) && (pSwapChain != nullptr)) || (pImage != nullptr));

    bool hasWork = false;

    if (pCmdBufState != nullptr)
    {
        Pal::ICmdBuffer* pCmdBuf = pCmdBufState->pCmdBuf;

        if (pSwapChain != nullptr)
        {
            hasWork = pSwapChain->BuildPostProcessingCommands(pCmdBuf, pPresentInfo, m_pDevice);

        }

        Pal::CmdPostProcessFrameInfo frameInfo = {};

        if ((pPresentInfo != nullptr) && (pSwapChain != nullptr))
        {
            const DisplayableSurfaceInfo& displayableInfo = pSwapChain->GetProperties().displayableInfo;

            frameInfo.pSrcImage                = pPresentInfo->pSrcImage;
            frameInfo.debugOverlay.presentMode = pPresentInfo->presentMode;
            frameInfo.debugOverlay.wsiPlatform = displayableInfo.palPlatform;
            frameInfo.debugOverlay.presentKey = pSwapChain->IsDxgiEnabled()
                ? Pal::PresentKeyFromPointer(pPresentInfo->pSwapChain)
                : Pal::PresentKeyFromOsWindowHandle(displayableInfo.windowHandle);
        }
        else
        {
            frameInfo.pSrcImage                = pImage;
            frameInfo.debugOverlay.presentMode = Pal::PresentMode::Unknown;
        }

        frameInfo.fullScreenFrameMetadataControlFlags.u32All = m_palFrameMetadataControl.flags.u32All;

        bool wasGpuWorkAdded = false;
        pCmdBuf->CmdPostProcessFrame(frameInfo, &wasGpuWorkAdded);

        hasWork = (hasWork || wasGpuWorkAdded);
    }

    return hasWork;
}

// =====================================================================================================================
// Submits an internally managed command buffer to this queue
VkResult Queue::SubmitInternalCmdBuf(
    CmdBufferRing*          pCmdBufferRing,
    uint32_t                deviceIdx,
    const Pal::CmdBufInfo&  cmdBufInfo,
    CmdBufState*            pCmdBufState)
{
    CmdBufferRing* pRing = (pCmdBufferRing != nullptr) ? pCmdBufferRing : m_pCmdBufferRing;

    return pRing->SubmitCmdBuffer(m_pDevice, deviceIdx, m_pPalQueues[deviceIdx], cmdBufInfo, pCmdBufState);
}

// =====================================================================================================================
// Synchronize back buffer memory by doing a dummy submit with the written primary field set.
VkResult Queue::SynchronizeBackBuffer(
    Memory*  pMemory,
    uint32_t deviceIdx)
{
    VkResult result = VK_SUCCESS;

    if (m_pDummyCmdBuffer[deviceIdx] == nullptr)
    {
        result = CreateDummyCmdBuffer();
    }

    if (result == VK_SUCCESS)
    {
        Pal::IGpuMemory* pGpuMem = pMemory->PalMemory(deviceIdx);

        Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};

        perSubQueueInfo.cmdBufferCount  = 1;
        perSubQueueInfo.ppCmdBuffers    = &m_pDummyCmdBuffer[deviceIdx];
        perSubQueueInfo.pCmdBufInfoList = nullptr;

        Pal::SubmitInfo submitInfo = {};

        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.blockIfFlippingCount = 1;
        submitInfo.ppBlockIfFlipping    = &pGpuMem;

        result = PalToVkResult(m_pPalQueues[deviceIdx]->Submit(submitInfo));
    }

    return result;
}

VkResult Queue::CreateSqttState(
    void* pMemory)
{
    m_pSqttState = VK_PLACEMENT_NEW(pMemory) SqttQueueState(this);

    return m_pSqttState->Init();
}

// =====================================================================================================================
// Contains the core implementation of vkInsertDebugUtilsLabelEXT. The SQTT layer (when on) extends this functionality.
void Queue::InsertDebugUtilsLabel(
    const VkDebugUtilsLabelEXT*                 pLabelInfo)
{
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (strcmp(pLabelInfo->pLabelName, settings.devModeEndFrameDebugUtilsLabel) == 0)
    {
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameEnd(this, DevModeMgr::FrameDelimiterType::QueueLabel);

            if (settings.devModeBlockingEndFrameDebugUtils)
            {
                // RGP tracing does not allow for overlapping frames. This WaitIdle() may be
                // disabled in situations where applications synchronize the frames themselves
                VkResult tempResult = WaitIdle();
                VK_ASSERT(tempResult == VK_SUCCESS);
            }
        }
#endif

    }

    if (strcmp(pLabelInfo->pLabelName, settings.devModeStartFrameDebugUtilsLabel) == 0)
    {
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameBegin(this, DevModeMgr::FrameDelimiterType::QueueLabel);
        }
#endif
}
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
     return ApiQueue::ObjectFromHandle(queue)->Submit<VkSubmitInfo>(
        submitCount,
        pSubmits,
        fence);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo2KHR*                     pSubmits,
    VkFence                                     fence)
{
    return ApiQueue::ObjectFromHandle(queue)->Submit<VkSubmitInfo2KHR>(
        submitCount,
        pSubmits,
        fence);
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(
    VkQueue                                     queue)
{
    return ApiQueue::ObjectFromHandle(queue)->WaitIdle();
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueBindSparse(
    VkQueue                                     queue,
    uint32_t                                    bindInfoCount,
    const VkBindSparseInfo*                     pBindInfo,
    VkFence                                     fence)
{
    return ApiQueue::ObjectFromHandle(queue)->BindSparse(bindInfoCount, pBindInfo, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue                                      queue,
    const VkPresentInfoKHR*                      pPresentInfo)
{
    return ApiQueue::ObjectFromHandle(queue)->Present(pPresentInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pLabelInfo)
{
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(
    VkQueue                                     queue)
{
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pLabelInfo)
{
    ApiQueue::ObjectFromHandle(queue)->InsertDebugUtilsLabel(pLabelInfo);
}

} // namespace entry

} // namespace vk
