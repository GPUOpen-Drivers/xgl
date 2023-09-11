/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  gpumemory_event_handler.cpp
 * @brief Contains implementation of the GPU Memory Event Handler
 ***********************************************************************************************************************
 */

#include "include/gpumemory_event_handler.h"
#include "include/vk_instance.h"
#include "include/vk_device.h"

#include "palIntrusiveListImpl.h"
#include "palHashMapImpl.h"
#include "palHashSetImpl.h"
#include "palVectorImpl.h"
#include "palGpuUtil.h"

namespace vk
{

// Alloc, suballoc and bind entries have NullHandle assigned as object handle,
// if the correlation information was not provided yet to gpu memory event handler
constexpr uint64_t NullHandle = 0;

// =====================================================================================================================
GpuMemoryEventHandler::GpuMemoryEventHandler(Instance* pInstance)
    :
    m_pInstance(pInstance),
    m_callbacks(pInstance->Allocator()),
    m_allocationHashMap(32, pInstance->Allocator()),
    m_vulkanSubAllocationHashMap(32, pInstance->Allocator()),
    m_palSubAllocationHashMap(32, pInstance->Allocator()),
    m_bindHashMap(32, pInstance->Allocator()),
    m_deviceHashSet(32, pInstance->Allocator()),
    m_deviceCount(0)
{
    m_allocationHashMap.Init();
    m_vulkanSubAllocationHashMap.Init();
    m_palSubAllocationHashMap.Init();
    m_deviceHashSet.Init();
    m_bindHashMap.Init();
}

// =====================================================================================================================
// Creates the GPU Memory Event Handler class.
VkResult GpuMemoryEventHandler::Create(
    Instance*                    pInstance,
    GpuMemoryEventHandler**      ppEventHandler)
{
    VkResult result = VK_SUCCESS;

    void* pSystemMem = pInstance->AllocMem(sizeof(GpuMemoryEventHandler), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pSystemMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        GpuMemoryEventHandler* pEventHandler = VK_PLACEMENT_NEW(pSystemMem) GpuMemoryEventHandler(pInstance);

        *ppEventHandler = pEventHandler;
    }

    return result;
}

// =====================================================================================================================
void GpuMemoryEventHandler::Destroy()
{
    PAL_ALERT_MSG(m_vulkanSubAllocationHashMap.GetNumEntries() != 0, "Vulkan suballocations were not freed.");
    PAL_ALERT_MSG(m_palSubAllocationHashMap.GetNumEntries() != 0, "Pal suballocations were not freed.");

    m_bindHashMapMutex.Lock();
    for (auto iter = m_bindHashMap.Begin(); iter.Get() != nullptr; iter.Next())
    {
        const BindDataList& bindDataList = iter.Get()->value;
        if (bindDataList.IsEmpty() == false)
        {
            PAL_ALERT_MSG(bindDataList.IsEmpty() == false, "Memory binds map is not empty.");
        }
    }
    m_bindHashMapMutex.Unlock();

    Util::Destructor(this);

    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
void GpuMemoryEventHandler::PalDeveloperCallback(
    Pal::Developer::CallbackType type,
    void*                        pCbData)
{
    switch (type)
    {
    case Pal::Developer::CallbackType::AllocGpuMemory:
    {
        Util::RWLockAuto<RWLock::ReadWrite> lock(&m_allocationHashMapLock);
        auto*const      pGpuMemoryData  = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);
        bool            exists          = false;
        AllocationData* pAllocationData = nullptr;

        Pal::Result palResult = m_allocationHashMap.FindAllocate(pGpuMemoryData->pGpuMemory, &exists, &pAllocationData);

        if (palResult == Pal::Result::Success)
        {
            // Add the new allocation if it did not exist already.
            if (exists == false)
            {
                // Store the allocation information
                pAllocationData->allocationData               = *pGpuMemoryData;
                pAllocationData->objectHandle                 = NullHandle;
                pAllocationData->objectType                   = VK_OBJECT_TYPE_UNKNOWN;
                pAllocationData->memoryObjectId               = pGpuMemoryData->pGpuMemory->Desc().uniqueId;
                pAllocationData->isExternal                   = pAllocationData->allocationData.flags.isExternal;
                pAllocationData->reportedToDeviceMemoryReport = false;

                // If this is a Pal internal allocation that is not suballocated report it to device_memory_report now
                if ((pAllocationData->allocationData.flags.isClient       == 0) && // Pal internal, not Vulkan
                    (pAllocationData->allocationData.flags.isCmdAllocator == 0) && // Command allocator is suballocated
                    (pAllocationData->allocationData.flags.buddyAllocated == 0) && // Buddy allocator is suballocated
                    (pAllocationData->allocationData.flags.isExternal     == 0))   // vkCreateMemory handles external
                {
                    // This is a Pal internal allocation that is not suballocated report it to device_memory_report now
                    m_deviceHashSetLock.LockForRead();
                    const Device*   pDevice         = m_deviceHashSet.Begin().Get()->key;
                    m_deviceHashSetLock.UnlockForRead();
                    PhysicalDevice* pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
                    uint32_t        heapIndex       = 0;
                    bool            validHeap       = pPhysicalDevice->GetVkHeapIndexFromPalHeap(pGpuMemoryData->heap,
                                                                                                 &heapIndex);
                    VK_ASSERT(validHeap);
                    // Physical device is the default Vulkan object for allocations not specifically tracked otherwise.
                    auto* const pHandle                           = ApiPhysicalDevice::FromObject(pPhysicalDevice);
                    pAllocationData->objectHandle                 = ApiPhysicalDevice::IntValueFromHandle(pHandle);
                    pAllocationData->objectType                   = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
                    pAllocationData->reportedToDeviceMemoryReport = true;

                    DeviceMemoryReportAllocateEvent(
                        pAllocationData->objectHandle,
                        pAllocationData->allocationData.size,
                        pAllocationData->objectType,
                        pAllocationData->memoryObjectId,
                        heapIndex,
                        pAllocationData->isExternal);
                }
            }
            else
            {
                PAL_ASSERT_ALWAYS_MSG("Allocation already exists");
            }
        }

        break;
    }
    case Pal::Developer::CallbackType::FreeGpuMemory:
    {
        Util::RWLockAuto<RWLock::ReadWrite> lock(&m_allocationHashMapLock);
        auto*const      pGpuMemoryData  = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);
        AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemoryData->pGpuMemory);

        if (pAllocationData != nullptr)
        {
            // Report non-suballocated frees to device_memory_report and device_address_binding_report now,
            // including Vulkan allocations
            if ((pAllocationData->allocationData.flags.isCmdAllocator == 0) &&   // Command allocator is suballocated
                (pAllocationData->allocationData.flags.buddyAllocated == 0) &&   // Pal Buddy allocator is suballocated
                (pAllocationData->isBuddyAllocated                    == false)) // Vulkan Buddy allocator, suballocated
            {
                if (pAllocationData->reportedToDeviceMemoryReport == true)
                {
                    DeviceMemoryReportFreeEvent(
                        pAllocationData->objectHandle,
                        pAllocationData->objectType,
                        pAllocationData->memoryObjectId,
                        pAllocationData->isExternal);
                }
                else if ((pAllocationData->allocationData.flags.isClient  == 1) &&
                         (pAllocationData->allocationData.flags.isVirtual == 1))
                {
                    // Vulkan virtual only base allocation ok to never report to device_memory_report
                }
                else if ((pAllocationData->allocationData.flags.isClient  == 1) &&
                         (pAllocationData->allocationData.flags.isVirtual == 0))
                {
                    PAL_ALERT_ALWAYS_MSG("Vulkan base allocation freed, was never reported to device_memory_report.");
                }
                else if (pAllocationData->allocationData.flags.isExternal == 1)
                {
                    PAL_ALERT_ALWAYS_MSG("External base allocation freed, was never reported to device_memory_report");
                }
                else
                {
                    PAL_ALERT_ALWAYS_MSG("Unknown base allocation freed, was never reported to device_memory_report");
                }

                DeviceAddressBindingReportAllocUnbindEvent(pAllocationData);
            }

            m_allocationHashMap.Erase(pGpuMemoryData->pGpuMemory);
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Free reported for untracked allocation");
        }

        break;
    }
    case Pal::Developer::CallbackType::SubAllocGpuMemory:
    {
        Util::RWLockAuto<RWLock::ReadWrite> lock(&m_palSubAllocationHashMapLock);
        auto*const pGpuMemoryData = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);

        PAL_ASSERT_MSG((pGpuMemoryData->flags.isClient        == 0)  && // Pal internal allocation
                       ((pGpuMemoryData->flags.isCmdAllocator == 1)  || // Command allocator, suballocated Pal internal
                        (pGpuMemoryData->flags.buddyAllocated == 1)) && // Buddy allocator is suballocated Pal internal
                       (pGpuMemoryData->flags.isExternal      == 0)  && // External memory is handled by vkCreateMemory
                       (pGpuMemoryData->size < pGpuMemoryData->pGpuMemory->Desc().size), // Suballoc should be smaller
                       "The base GPU allocation of this Pal internal suballocation is not as expected.");

        SubAllocationKey key = {pGpuMemoryData->pGpuMemory->Desc().gpuVirtAddr,
                                pGpuMemoryData->offset};

        bool               exists        = false;
        SubAllocationData* pSubAllocData = nullptr;

        Pal::Result palResult = m_palSubAllocationHashMap.FindAllocate(key, &exists, &pSubAllocData);

        if (palResult == Pal::Result::Success)
        {
            // Add the new Pal suballocation if it did not exist already.
            if (exists == false)
            {
                m_deviceHashSetLock.LockForRead();
                const Device*   pDevice         = m_deviceHashSet.Begin().Get()->key;
                m_deviceHashSetLock.UnlockForRead();
                PhysicalDevice* pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
                uint32_t        heapIndex       = 0;
                bool            validHeap       = pPhysicalDevice->GetVkHeapIndexFromPalHeap(pGpuMemoryData->heap,
                                                                                             &heapIndex);
                VK_ASSERT(validHeap);

                // Store the Pal suballocation information
                pSubAllocData->allocationData               = *pGpuMemoryData;
                pSubAllocData->memoryObjectId               = GpuUtil::GenerateGpuMemoryUniqueId(false);
                pSubAllocData->heapIndex                    = heapIndex;
                pSubAllocData->offset                       = pGpuMemoryData->offset;
                pSubAllocData->subAllocationSize            = pGpuMemoryData->size;
                pSubAllocData->objectHandle                 = NullHandle;
                pSubAllocData->objectType                   = VK_OBJECT_TYPE_UNKNOWN;
                pSubAllocData->reportedToDeviceMemoryReport = false;

                // Defer reporting of application requested Pal internal suballocations to device_memory_report until
                // ReportDeferredPalSubAlloc() but report all other Pal internal suballocations now
                if (pDevice->GetEnabledFeatures().deviceMemoryReport && (pGpuMemoryData->flags.appRequested == 0))
                {
                    auto* const pHandle                         = ApiPhysicalDevice::FromObject(pPhysicalDevice);
                    pSubAllocData->objectHandle                 = ApiPhysicalDevice::IntValueFromHandle(pHandle);
                    pSubAllocData->objectType                   = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
                    pSubAllocData->reportedToDeviceMemoryReport = true;

                     DeviceMemoryReportAllocateEvent(
                        pSubAllocData->objectHandle,
                        pSubAllocData->allocationData.size,
                        pSubAllocData->objectType,
                        pSubAllocData->memoryObjectId,
                        pSubAllocData->heapIndex,
                        pSubAllocData->allocationData.flags.isExternal);
                }
            }
            else
            {
                PAL_ASSERT_ALWAYS_MSG("SubAlloc of a Pal suballocation that already exists");
            }
        }

        break;
    }
    case Pal::Developer::CallbackType::SubFreeGpuMemory:
    {
        Util::RWLockAuto<RWLock::ReadWrite> lock(&m_palSubAllocationHashMapLock);
        auto*const pGpuMemoryData = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);

        SubAllocationKey key = {pGpuMemoryData->pGpuMemory->Desc().gpuVirtAddr,
                                pGpuMemoryData->offset};

        SubAllocationData* pSubAllocData = m_palSubAllocationHashMap.FindKey(key);

        if (pSubAllocData != nullptr)
        {
            m_deviceHashSetLock.LockForRead();
            const Device* pDevice = m_deviceHashSet.Begin().Get()->key;
            m_deviceHashSetLock.UnlockForRead();

            if (pDevice->GetEnabledFeatures().deviceMemoryReport)
            {
                if (pSubAllocData->reportedToDeviceMemoryReport == true)
                {
                    DeviceMemoryReportFreeEvent(
                        pSubAllocData->objectHandle,
                        pSubAllocData->objectType,
                        pSubAllocData->memoryObjectId,
                        pSubAllocData->allocationData.flags.isExternal);
                }
                else if (pSubAllocData->allocationData.flags.isCmdAllocator == 1)
                {
                    PAL_ALERT_ALWAYS_MSG("SubFree: CmdAllocator suballoc was never reported to device_memory_report");
                }
                else if (pSubAllocData->allocationData.flags.buddyAllocated == 1)
                {
                    PAL_ALERT_ALWAYS_MSG("SubFree: Buddy Allocated suballoc never reported to device_memory_report");
                }
                else
                {
                    PAL_ALERT_ALWAYS_MSG("SubFree: Unknown suballoc was never reported to device_memory_report");
                }
            }

            DeviceAddressBindingReportSuballocUnbindEvent(pSubAllocData);

            m_palSubAllocationHashMap.Erase(key);
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("SubFree reported for untracked Pal suballocation");
        }

        break;
    }
    case Pal::Developer::CallbackType::BindGpuMemory:
    {
        auto* const pBindGpuMemoryData = reinterpret_cast<Pal::Developer::BindGpuMemoryData*>(pCbData);

        if (pBindGpuMemoryData->isSystemMemory == false)
        {
            bool          exists        = false;
            BindDataList* pBindDataList = nullptr;
            BindDataListNode* pBindDataListNode = nullptr;

            m_bindHashMapMutex.Lock();
            Pal::Result palResult = m_bindHashMap.FindAllocate(pBindGpuMemoryData->pGpuMemory, &exists, &pBindDataList);

            if (palResult == Pal::Result::Success)
            {
                if (exists == false)
                {
                    pBindDataList = VK_PLACEMENT_NEW(pBindDataList) BindDataList();
                }

                BindDataListNode::Create(m_pInstance, pBindGpuMemoryData, &pBindDataListNode);

                if (pBindDataListNode != nullptr)
                {
                    DeviceAddressBindingReportNewUnbindEvent(pBindDataListNode->GetData());

                    pBindDataList->PushFront(pBindDataListNode->GetNode());
                }
            }
            m_bindHashMapMutex.Unlock();

            if ((palResult == Pal::Result::Success) && (pBindDataListNode != nullptr))
            {
                DeviceAddressBindingReportNewBindEvent(pBindDataListNode->GetData());
            }
        }

        break;
    }
    default:
        break;
    }
}

// =====================================================================================================================
// GpuMemoryEventHandler events are required for VK_EXT_device_memory_report and VK_EXT_device_address_binding_report.
// Increment the count of devices when one or more of these extensions enabled.
void GpuMemoryEventHandler::EnableGpuMemoryEvents(
    const Device* pDevice)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_deviceHashSetLock);
    VK_ASSERT(m_deviceHashSet.Contains(pDevice) == false);

    Util::AtomicIncrement(&m_deviceCount);

    m_deviceHashSet.Insert(pDevice);
}

// =====================================================================================================================
// Decrement the count of devices to remove a device with one or more extensions enabled requiring GPU memory events.
void GpuMemoryEventHandler::DisableGpuMemoryEvents(
    const Device* pDevice)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_deviceHashSetLock);
    VK_ASSERT(m_deviceHashSet.Contains(pDevice));

    Util::AtomicDecrement(&m_deviceCount);

    m_deviceHashSet.Erase(pDevice);
}

// =====================================================================================================================
void GpuMemoryEventHandler::RegisterDeviceMemoryReportCallback(
    const DeviceMemoryReportCallback& callback)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_callbacksLock);
    m_callbacks.PushBack(callback);
}

// =====================================================================================================================
void GpuMemoryEventHandler::UnregisterDeviceMemoryReportCallbacks(
    const Device*                     pDevice)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_callbacksLock);

    for (DeviceMemoryReportCallbacks::Iter iter = m_callbacks.Begin(); iter.IsValid(); iter.Next())
    {
        if (iter.Get().pDevice == pDevice)
        {
            m_callbacks.Erase(iter);
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanAllocateEvent(
    const Device*                    pDevice,
    const Pal::IGpuMemory*           pGpuMemory,
    uint64_t                         objectHandle,
    VkObjectType                     objectType,
    uint64_t                         heapIndex,
    bool                             isBuddyAllocated
    )
{
    Util::RWLockAuto<RWLock::ReadOnly> lock(&m_allocationHashMapLock);
    AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemory);

    if (pAllocationData != nullptr)
    {
        pAllocationData->objectType                   = objectType;
        pAllocationData->objectHandle                 = objectHandle;
        pAllocationData->allocationData.pGpuMemory    = pGpuMemory;
        pAllocationData->isBuddyAllocated             = isBuddyAllocated;

        if (pDevice->GetEnabledFeatures().deviceMemoryReport && (isBuddyAllocated == false))
        {
            if (pAllocationData->reportedToDeviceMemoryReport == false)
            {
                pAllocationData->reportedToDeviceMemoryReport = true;

                const auto& gpuMemoryDesc = pAllocationData->allocationData.pGpuMemory->Desc();

                DeviceMemoryReportAllocateEvent(
                    pAllocationData->objectHandle,
                    gpuMemoryDesc.size,
                    pAllocationData->objectType,
                    pAllocationData->memoryObjectId,
                    heapIndex,
                    pAllocationData->isExternal);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the allocation of an already reported allocation.");
            }
        }

        if (pDevice->GetEnabledFeatures().deviceAddressBindingReport)
        {
            DeviceAddressBindingReportAllocBindEvent(pAllocationData);
        }
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to correlate an untracked allocation.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanAllocationFailedEvent(
    const Device*                    pDevice,
    Pal::gpusize                     allocatedSize,
    VkObjectType                     objectType,
    uint64_t                         heapIndex)
{
    if (pDevice->GetEnabledFeatures().deviceMemoryReport)
    {
        DeviceMemoryReportAllocationFailedEvent(allocatedSize, objectType, heapIndex);
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanSubAllocateEvent(
    const Device*                    pDevice,
    const Pal::IGpuMemory*           pGpuMemory,
    Pal::gpusize                     offset,
    Pal::gpusize                     subAllocationSize,
    uint64_t                         objectHandle,
    VkObjectType                     objectType,
    uint64_t                         heapIndex)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_vulkanSubAllocationHashMapLock);
    SubAllocationKey key = {pGpuMemory->Desc().gpuVirtAddr,
                            offset};

    bool exists = false;
    SubAllocationData* pSubAllocData = nullptr;

    Pal::Result palResult = m_vulkanSubAllocationHashMap.FindAllocate(key, &exists, &pSubAllocData);

    if (palResult == Pal::Result::Success)
    {
        if (exists == false)
        {
            pSubAllocData->allocationData.pGpuMemory    = pGpuMemory;
            pSubAllocData->memoryObjectId               = GpuUtil::GenerateGpuMemoryUniqueId(false);
            pSubAllocData->objectType                   = objectType;
            pSubAllocData->offset                       = offset;
            pSubAllocData->subAllocationSize            = subAllocationSize;
            pSubAllocData->objectHandle                 = objectHandle;
            pSubAllocData->heapIndex                    = heapIndex;

            if (pDevice->GetEnabledFeatures().deviceMemoryReport)
            {
                pSubAllocData->reportedToDeviceMemoryReport = true;

                DeviceMemoryReportAllocateEvent(
                    pSubAllocData->objectHandle,
                    pSubAllocData->subAllocationSize,
                    pSubAllocData->objectType,
                    pSubAllocData->memoryObjectId,
                    pSubAllocData->heapIndex,
                    pSubAllocData->allocationData.pGpuMemory->Desc().flags.isExternal);
            }

            if (pDevice->GetEnabledFeatures().deviceAddressBindingReport)
            {
                DeviceAddressBindingReportSuballocBindEvent(pSubAllocData);
            }
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Vulkan is reporting an already reported Vulkan suballocation.");
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanSubFreeEvent(
    const Device*                    pDevice,
    const Pal::IGpuMemory*           pGpuMemory,
    Pal::gpusize                     offset)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_vulkanSubAllocationHashMapLock);
    SubAllocationKey key = {pGpuMemory->Desc().gpuVirtAddr,
                            offset};

    SubAllocationData* pSubAllocData = m_vulkanSubAllocationHashMap.FindKey(key);

    if (pSubAllocData != nullptr)
    {
        if (pDevice->GetEnabledFeatures().deviceMemoryReport)
        {
            if (pSubAllocData->reportedToDeviceMemoryReport)
            {
                DeviceMemoryReportFreeEvent(
                    pSubAllocData->objectHandle,
                    pSubAllocData->objectType,
                    pSubAllocData->memoryObjectId,
                    pSubAllocData->allocationData.pGpuMemory->Desc().flags.isExternal);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the free of an unreported Vulkan suballocation.");
            }
        }

        if (pDevice->GetEnabledFeatures().deviceAddressBindingReport)
        {
            DeviceAddressBindingReportSuballocUnbindEvent(pSubAllocData);
        }

        m_vulkanSubAllocationHashMap.Erase(key);
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the free of an untracked Vulkan suballocation.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceMemoryReportAllocateEvent(
    uint64_t                         objectHandle,
    Pal::gpusize                     allocatedSize,
    VkObjectType                     objectType,
    uint64_t                         memoryObjectId,
    uint64_t                         heapIndex,
    bool                             isImport)
{
    VK_ASSERT(objectType != VK_OBJECT_TYPE_UNKNOWN);

    VkDeviceMemoryReportCallbackDataEXT callbackData = {};
    callbackData.sType          = VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT;
    callbackData.pNext          = nullptr;
    callbackData.flags          = 0;

    if (isImport == true)
    {
        callbackData.type       = VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_IMPORT_EXT;
    }
    else
    {
        callbackData.type       = VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT;
    }

    callbackData.memoryObjectId = memoryObjectId;
    callbackData.size           = allocatedSize;
    callbackData.objectType     = objectType;
    callbackData.objectHandle   = objectHandle;
    callbackData.heapIndex      = heapIndex;

    SendDeviceMemoryReportEvent(callbackData);
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceMemoryReportAllocationFailedEvent(
    Pal::gpusize                     allocatedSize,
    VkObjectType                     objectType,
    uint64_t                         heapIndex)
{
    VK_ASSERT(objectType != VK_OBJECT_TYPE_UNKNOWN);

    VkDeviceMemoryReportCallbackDataEXT callbackData = {};
    callbackData.sType          = VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT;
    callbackData.pNext          = nullptr;
    callbackData.flags          = 0;
    callbackData.type           = VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATION_FAILED_EXT;
    callbackData.memoryObjectId = 0;              // memoryObjectId is presumed undefined for allocation failed events
    callbackData.size           = allocatedSize;
    callbackData.objectType     = objectType;
    callbackData.objectHandle   = 0;              // objectHandle is undefined for allocation failed events
    callbackData.heapIndex      = heapIndex;

    SendDeviceMemoryReportEvent(callbackData);
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceMemoryReportFreeEvent(
    uint64_t                         objectHandle,
    VkObjectType                     objectType,
    uint64_t                         memoryObjectId,
    bool                             isUnimport)
{
    VK_ASSERT(objectType != VK_OBJECT_TYPE_UNKNOWN);

    VkDeviceMemoryReportCallbackDataEXT callbackData = {};
    callbackData.sType          = VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT;
    callbackData.pNext          = nullptr;
    callbackData.flags          = 0;

    if (isUnimport == true)
    {
        callbackData.type       = VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_UNIMPORT_EXT;
    }
    else
    {
        callbackData.type       = VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT;
    }

    callbackData.memoryObjectId = memoryObjectId;
    callbackData.size           = 0;            // size is undefined for free events.
    callbackData.objectType     = objectType;
    callbackData.objectHandle   = objectHandle;
    callbackData.heapIndex      = 0;            // heapIndex is undefined for free events.

    SendDeviceMemoryReportEvent(callbackData);
}

// =====================================================================================================================
void GpuMemoryEventHandler::ReportDeferredPalSubAlloc(
    const Device*                    pDevice,
    Pal::gpusize                     gpuVirtAddr,
    Pal::gpusize                     offset,
    const uint64_t                   objectHandle,
    const VkObjectType               objectType)
{
    Util::RWLockAuto<RWLock::ReadOnly> lock(&m_palSubAllocationHashMapLock);

    SubAllocationKey key = {gpuVirtAddr,
                            offset};

    SubAllocationData* pSubAllocData = m_palSubAllocationHashMap.FindKey(key);

    if (pSubAllocData != nullptr)
    {
        pSubAllocData->objectHandle = objectHandle;
        pSubAllocData->objectType   = objectType;

        if (pDevice->GetEnabledFeatures().deviceMemoryReport)
        {
            if (pSubAllocData->reportedToDeviceMemoryReport == false)
            {
                // Report deferred Pal suballocation to device_memory_report now
                pSubAllocData->reportedToDeviceMemoryReport = true;

                DeviceMemoryReportAllocateEvent(
                    pSubAllocData->objectHandle,
                    pSubAllocData->allocationData.size,
                    pSubAllocData->objectType,
                    pSubAllocData->memoryObjectId,
                    pSubAllocData->heapIndex,
                    pSubAllocData->allocationData.flags.isExternal);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report allocation of an already reported Pal suballoc.");
            }
        }

        if (pDevice->GetEnabledFeatures().deviceAddressBindingReport)
        {
            DeviceAddressBindingReportSuballocBindEvent(pSubAllocData);
        }
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the allocation of an untracked Pal suballocation.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::SendDeviceMemoryReportEvent(
    const VkDeviceMemoryReportCallbackDataEXT& callbackData)
{
    Util::RWLockAuto<RWLock::ReadOnly> lock(&m_callbacksLock);

    for (auto iter = m_callbacks.Begin(); iter.IsValid(); iter.Next())
    {
        iter.Get().callback(&callbackData, iter.Get().pData);
    }
}

// =====================================================================================================================
// Creates the BindDataListNode class.
void GpuMemoryEventHandler::BindDataListNode::Create(
    Instance*                                 pInstance,
    Pal::Developer::BindGpuMemoryData*        pBindGpuMemoryData,
    GpuMemoryEventHandler::BindDataListNode** ppObject)
{
    void* pSystemMem = pInstance->AllocMem(
        sizeof(GpuMemoryEventHandler::BindDataListNode),
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pSystemMem != nullptr)
    {
        BindDataListNode* pBindDataListNode =
            VK_PLACEMENT_NEW(pSystemMem) BindDataListNode(pInstance, pBindGpuMemoryData);

        *ppObject = pBindDataListNode;
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::BindDataListNode::Destroy()
{
    Util::Destructor(this);

    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
GpuMemoryEventHandler::BindDataListNode::BindDataListNode(
    Instance*                          pInstance,
    Pal::Developer::BindGpuMemoryData* pBindGpuMemoryData)
    : m_pInstance(pInstance)
    , m_node(this)
{
    m_data.bindGpuMemoryData                    = *pBindGpuMemoryData;
    m_data.objectHandle                         = NullHandle;
    m_data.objectType                           = VK_OBJECT_TYPE_UNKNOWN;
    m_data.reportedToDeviceAddressBindingReport = false;
}

// =====================================================================================================================
bool GpuMemoryEventHandler::CheckIntervalsIntersect(
    const Interval& intervalOne,
    const Interval& intervalTwo
    ) const
{
    bool intersect = false;

    if (intervalOne.m_offset < intervalTwo.m_offset)
    {
        intersect = intervalTwo.m_offset < (intervalOne.m_offset + intervalOne.m_size);
    }
    else
    {
        intersect = intervalOne.m_offset < (intervalTwo.m_offset + intervalTwo.m_size);
    }

    return intersect;
}

// =====================================================================================================================
// The caller of this function must hold the m_bindHashMapMutex mutex
void GpuMemoryEventHandler::DeviceAddressBindingReportUnbindEventCommon(
    const Pal::IGpuMemory* pGpuMemory,
    const Interval&        interval)
{
    BindDataList* pBindDataList = m_bindHashMap.FindKey(pGpuMemory);

    if (pBindDataList != nullptr)
    {
        auto iter = pBindDataList->Begin();
        while(iter.IsValid())
        {
            BindData* pBindData = iter.Get()->GetData();
            bool intersect      = true;

            if (interval.m_size > 0)
            {
                intersect = CheckIntervalsIntersect(
                    Interval(pBindData->bindGpuMemoryData.offset, pBindData->bindGpuMemoryData.requiredGpuMemSize),
                    interval);
            }

            if (intersect)
            {
                BindDataListNode* pBindDataListNode = iter.Get();
                if (pBindData->reportedToDeviceAddressBindingReport)
                {
                    ReportUnbindEvent(pBindData);
                }
                else
                {
                    PAL_ALERT_ALWAYS_MSG("Trying to report unbind, but bind was not reported previously.");
                }

                pBindDataList->Erase(&iter);
                pBindDataListNode->Destroy();
            }
            else
            {
                iter.Next();
            }
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportAllocBindEvent(
    const AllocationData* pAllocationData)
{
    Util::MutexAuto lock(&m_bindHashMapMutex);
    BindDataList* pBindDataList = m_bindHashMap.FindKey(pAllocationData->allocationData.pGpuMemory);

    if (pBindDataList != nullptr)
    {
        for (auto iter = pBindDataList->Begin(); iter.IsValid(); iter.Next())
        {
            ReportBindEvent(iter.Get()->GetData(), pAllocationData->objectHandle, pAllocationData->objectType);
        }
    }
}
// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportAllocUnbindEvent(
    const AllocationData* pAllocationData)
{
    Util::MutexAuto lock(&m_bindHashMapMutex);
    DeviceAddressBindingReportUnbindEventCommon(pAllocationData->allocationData.pGpuMemory, Interval());
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportSuballocBindEvent(
    const SubAllocationData* pSubAllocData)
{
    Util::MutexAuto lock(&m_bindHashMapMutex);
    BindDataList* pBindDataList = m_bindHashMap.FindKey(pSubAllocData->allocationData.pGpuMemory);

    if (pBindDataList != nullptr)
    {
        for (auto iter = pBindDataList->Begin(); iter.IsValid(); iter.Next())
        {
            BindData* pBindData = iter.Get()->GetData();

            bool intersect = CheckIntervalsIntersect(
                Interval(pSubAllocData->offset, pSubAllocData->subAllocationSize),
                Interval(pBindData->bindGpuMemoryData.offset, pBindData->bindGpuMemoryData.requiredGpuMemSize));

            if (intersect)
            {
                ReportBindEvent(pBindData, pSubAllocData->objectHandle, pSubAllocData->objectType);
            }
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportSuballocUnbindEvent(
    const SubAllocationData* pSubAllocData)
{
    Util::MutexAuto lock(&m_bindHashMapMutex);
    DeviceAddressBindingReportUnbindEventCommon(
        pSubAllocData->allocationData.pGpuMemory,
        Interval(pSubAllocData->offset, pSubAllocData->subAllocationSize));
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportNewBindEvent(
    BindData* pNewBindData)
{
    // The caller of this function must hold the m_bindHashMapMutex mutex
    m_allocationHashMapLock.LockForRead();

    const AllocationData* pAllocationData = m_allocationHashMap.FindKey(pNewBindData->bindGpuMemoryData.pGpuMemory);

    if (pAllocationData != nullptr)
    {
        if (pAllocationData->objectHandle != NullHandle)
        {
            ReportBindEvent(pNewBindData, pAllocationData->objectHandle, pAllocationData->objectType);
        }
        else if ((pAllocationData->allocationData.flags.isClient        == 0) &&
                 (pAllocationData->allocationData.flags.buddyAllocated  == 0) &&
                 (pAllocationData->allocationData.flags.isCmdAllocator  == 0) &&
                 (pAllocationData->allocationData.flags.isExternal      == 0))
        {
            m_deviceHashSetLock.LockForRead();
            const Device*   pDevice         = m_deviceHashSet.Begin().Get()->key;
            m_deviceHashSetLock.UnlockForRead();
            PhysicalDevice* pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
            auto* const     pHandle         = ApiPhysicalDevice::FromObject(pPhysicalDevice);

            // Pal internal allocation; attribute this to the physical device
            ReportBindEvent(
                pNewBindData,
                ApiPhysicalDevice::IntValueFromHandle(pHandle),
                VK_OBJECT_TYPE_PHYSICAL_DEVICE);
        }
    }
    m_allocationHashMapLock.UnlockForRead();

    if (pNewBindData->reportedToDeviceAddressBindingReport == false)
    {
        m_palSubAllocationHashMapLock.LockForRead();
        for (auto iter = m_palSubAllocationHashMap.Begin(); iter.Get() != nullptr; iter.Next())
        {
            const SubAllocationData* pSubAllocData = &iter.Get()->value;
            if (pSubAllocData->allocationData.pGpuMemory != pNewBindData->bindGpuMemoryData.pGpuMemory)
            {
                continue;
            }

            bool intersect = CheckIntervalsIntersect(
                Interval(pSubAllocData->offset, pSubAllocData->subAllocationSize),
                Interval(pNewBindData->bindGpuMemoryData.offset, pNewBindData->bindGpuMemoryData.requiredGpuMemSize));

            if (intersect)
            {
                const bool isPalInternalSuballoc = ((pSubAllocData->allocationData.flags.buddyAllocated == 1)  &&
                                                    (pSubAllocData->allocationData.flags.appRequested   == 0)) ||
                                                   (pSubAllocData->allocationData.flags.isCmdAllocator  == 1);

                if (pSubAllocData->objectHandle != NullHandle)
                {
                    ReportBindEvent(pNewBindData, pSubAllocData->objectHandle, pSubAllocData->objectType);
                }
                else if ((pSubAllocData->allocationData.flags.isClient   == 0) && // Pal internal
                         (pSubAllocData->allocationData.flags.isExternal == 0) && // External is handled by Vulkan
                         isPalInternalSuballoc)                                   // Not app requested
                {
                    m_deviceHashSetLock.LockForRead();
                    const Device*   pDevice         = m_deviceHashSet.Begin().Get()->key;
                    m_deviceHashSetLock.UnlockForRead();
                    PhysicalDevice* pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
                    auto* const     pHandle         = ApiPhysicalDevice::FromObject(pPhysicalDevice);

                    // Pal internal allocation; attribute this to the physical device
                    ReportBindEvent(
                        pNewBindData,
                        ApiPhysicalDevice::IntValueFromHandle(pHandle),
                        VK_OBJECT_TYPE_PHYSICAL_DEVICE);
                }
                break;
            }
        }
        m_palSubAllocationHashMapLock.UnlockForRead();
    }

    if (pNewBindData->reportedToDeviceAddressBindingReport == false)
    {
        m_vulkanSubAllocationHashMapLock.LockForRead();
        for (auto iter = m_vulkanSubAllocationHashMap.Begin(); iter.Get() != nullptr; iter.Next())
        {
            const SubAllocationData* pSubAllocData = &iter.Get()->value;
            if (pSubAllocData->allocationData.pGpuMemory != pNewBindData->bindGpuMemoryData.pGpuMemory)
            {
                continue;
            }

            if (pSubAllocData->objectHandle == NullHandle)
            {
                continue;
            }

            bool intersect = CheckIntervalsIntersect(
                Interval(pSubAllocData->offset, pSubAllocData->subAllocationSize),
                Interval(pNewBindData->bindGpuMemoryData.offset, pNewBindData->bindGpuMemoryData.requiredGpuMemSize));

            if (intersect)
            {
                ReportBindEvent(pNewBindData, pSubAllocData->objectHandle, pSubAllocData->objectType);
                break;
            }
        }
        m_vulkanSubAllocationHashMapLock.UnlockForRead();
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportNewUnbindEvent(
    BindData* pNewBindData)
{
    // The caller of this function must hold the m_bindHashMapMutex mutex
    DeviceAddressBindingReportUnbindEventCommon(
        pNewBindData->bindGpuMemoryData.pGpuMemory,
        Interval(pNewBindData->bindGpuMemoryData.offset, pNewBindData->bindGpuMemoryData.requiredGpuMemSize));
}

// =====================================================================================================================
void GpuMemoryEventHandler::ReportBindEvent(
    BindData*                       pBindData,
    uint64_t                        objectHandle,
    VkObjectType                    objectType)
{
    if (pBindData->reportedToDeviceAddressBindingReport == false)
    {
        pBindData->objectHandle                         = objectHandle;
        pBindData->objectType                           = objectType;
        pBindData->reportedToDeviceAddressBindingReport = true;

        DeviceAddressBindingReportCallback(
            pBindData->objectHandle,
            pBindData->objectType,
            VK_DEVICE_ADDRESS_BINDING_TYPE_BIND_EXT,
            pBindData->bindGpuMemoryData.pGpuMemory->Desc().gpuVirtAddr + pBindData->bindGpuMemoryData.offset,
            pBindData->bindGpuMemoryData.requiredGpuMemSize,
            pBindData->objectHandle == NullHandle);
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report an already reported bind");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::ReportUnbindEvent(
    BindData*                       pBindData)
{
    if (pBindData->reportedToDeviceAddressBindingReport)
    {
        DeviceAddressBindingReportCallback(
            pBindData->objectHandle,
            pBindData->objectType,
            VK_DEVICE_ADDRESS_BINDING_TYPE_UNBIND_EXT,
            pBindData->bindGpuMemoryData.pGpuMemory->Desc().gpuVirtAddr + pBindData->bindGpuMemoryData.offset,
            pBindData->bindGpuMemoryData.requiredGpuMemSize,
            pBindData->objectHandle == NullHandle);
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report unbind of an unreported Vulkan bind.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceAddressBindingReportCallback(
    uint64_t                        objectHandle,
    VkObjectType                    objectType,
    VkDeviceAddressBindingTypeEXT   bindingType,
    VkDeviceAddress                 bindingAddress,
    VkDeviceSize                    allocatedSize,
    bool                            isInternal)
{
    VkDeviceAddressBindingCallbackDataEXT bindingCallbackData = {};

    bindingCallbackData.sType       = VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT;
    bindingCallbackData.pNext       = nullptr;
    bindingCallbackData.flags       = isInternal ? VK_DEVICE_ADDRESS_BINDING_INTERNAL_OBJECT_BIT_EXT : 0;
    bindingCallbackData.baseAddress = bindingAddress;
    bindingCallbackData.size        = allocatedSize;
    bindingCallbackData.bindingType = bindingType;

    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};

    objectNameInfo.sType            = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.pNext            = nullptr;
    objectNameInfo.objectType       = objectType;
    objectNameInfo.objectHandle     = objectHandle;
    objectNameInfo.pObjectName      = nullptr;

    VkDebugUtilsMessengerCallbackDataEXT callbackData = {};

    callbackData.sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pNext              = &bindingCallbackData;
    callbackData.flags              = 0; // reserved for future use
    callbackData.pMessageIdName     = nullptr;
    callbackData.messageIdNumber    = 0;
    callbackData.pMessage           = nullptr;
    callbackData.queueLabelCount    = 0;
    callbackData.pQueueLabels       = nullptr;
    callbackData.cmdBufLabelCount   = 0;
    callbackData.pCmdBufLabels      = nullptr;
    callbackData.objectCount        = 1;
    callbackData.pObjects           = &objectNameInfo;

    m_pInstance->CallExternalMessengers(
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        &callbackData);
}

} // namespace vk
