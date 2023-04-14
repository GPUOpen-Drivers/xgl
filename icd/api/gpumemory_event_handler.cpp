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

#include "palHashMapImpl.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
GpuMemoryEventHandler::GpuMemoryEventHandler(Instance* pInstance)
    :
    m_pInstance(pInstance),
    m_callbacks(pInstance->Allocator()),
    m_allocationHashMap(32, pInstance->Allocator()),
    m_vulkanSubAllocationHashMap(32, pInstance->Allocator()),
    m_palSubAllocationHashMap(32, pInstance->Allocator()),
    m_memoryObjectId(0),
    m_memoryEventEnables(0)
{
    m_allocationHashMap.Init();
    m_vulkanSubAllocationHashMap.Init();
    m_palSubAllocationHashMap.Init();
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
                pAllocationData->allocationData = *pGpuMemoryData;

                // If this is a Pal internal allocation that is not suballocated report it to device_memory_report now
                if ((pAllocationData->allocationData.flags.isClient       == 0) && // Pal internal, not Vulkan
                    (pAllocationData->allocationData.flags.isCmdAllocator == 0) && // Command allocator is suballocated
                    (pAllocationData->allocationData.flags.buddyAllocated == 0) && // Buddy allocator is suballocated
                    (pAllocationData->allocationData.flags.isExternal     == 0))   // vkCreateMemory handles external
                {
                    m_callbacksLock.LockForRead();
                    auto     iter            = m_callbacks.Begin();
                    uint32_t heapIndex = 0;

                    if (iter.IsValid())
                    {
                        auto*const pPhysicalDevice = (iter.Get().pDevice)->VkPhysicalDevice(DefaultDeviceIndex);
                        bool validHeap = pPhysicalDevice->GetVkHeapIndexFromPalHeap(pGpuMemoryData->heap, &heapIndex);
                        VK_ASSERT(validHeap);
                    }
                    m_callbacksLock.UnlockForRead();

                    // The instance is the default Vulkan object for allocations not specifically tracked otherwise.
                    pAllocationData->objectHandle = Instance::IntValueFromHandle(Instance::FromObject(m_pInstance));
                    pAllocationData->objectType   = VK_OBJECT_TYPE_INSTANCE;
                    pAllocationData->reportedToDeviceMemoryReport = true;

                    DeviceMemoryReportAllocateEvent(
                        pAllocationData->objectHandle,
                        pAllocationData->allocationData.size,
                        pAllocationData->objectType,
                        pAllocationData->allocationData.pGpuMemory->Desc().uniqueId,
                        heapIndex,
                        pAllocationData->allocationData.flags.isExternal);
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
            // If this is a Pal internal free that is not suballocated report it to device_memory_report now
            if ((pAllocationData->allocationData.flags.isClient       == 0) && // Pal internal, not Vulkan
                (pAllocationData->allocationData.flags.isCmdAllocator == 0) && // Command allocator is suballocated
                (pAllocationData->allocationData.flags.buddyAllocated == 0) && // Buddy allocator is suballocated
                (pAllocationData->allocationData.flags.isExternal     == 0))   // vkCreateMemory handles external
            {
                if (pAllocationData->reportedToDeviceMemoryReport == true)
                {
                    DeviceMemoryReportFreeEvent(
                        pAllocationData->objectHandle,
                        pAllocationData->objectType,
                        pAllocationData->allocationData.pGpuMemory->Desc().uniqueId,
                        pAllocationData->allocationData.flags.isExternal);
                }
                else
                {
                    PAL_ALERT_ALWAYS_MSG("Allocation freed that was never reported to device_memory_report");
                }
            }

            m_allocationHashMap.Erase(pGpuMemoryData->pGpuMemory);
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Free reported for untracked allocation");
        }

        break;
    }
    case Pal::Developer::CallbackType::SubAllocGpuMemory:
    {
        Util::RWLockAuto<RWLock::ReadWrite> lock(&m_palSubAllocationHashMapLock);
        auto*const pGpuMemoryData = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);

        PAL_ASSERT_MSG((pGpuMemoryData->flags.isClient       == 0) && // Pal internal allocation
                       (pGpuMemoryData->flags.isCmdAllocator == 0) && // Command allocator is suballocated Pal internal
                       (pGpuMemoryData->flags.buddyAllocated == 1) && // Buddy allocator is suballocated Pal internal
                       (pGpuMemoryData->flags.isExternal     == 0) && // External memory is handled by vkCreateMemory
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
                // Store the Pal suballocation information
                pSubAllocData->allocationData = *pGpuMemoryData;
                pSubAllocData->memoryObjectId = GenerateMemoryObjectId();

                m_callbacksLock.LockForRead();
                auto iter = m_callbacks.Begin();

                if (iter.IsValid())
                {
                    uint32_t heapIndex = 0;
                    auto*const pPhysicalDevice = (iter.Get().pDevice)->VkPhysicalDevice(DefaultDeviceIndex);
                    bool validHeap = pPhysicalDevice->GetVkHeapIndexFromPalHeap(pGpuMemoryData->heap, &heapIndex);
                    VK_ASSERT(validHeap);

                    pSubAllocData->heapIndex = heapIndex;
                }
                m_callbacksLock.UnlockForRead();

                // Defer reporting of Pal suballocations to device_memory_report to ReportDeferredPalSubAlloc()
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
            if (pSubAllocData->reportedToDeviceMemoryReport == true)
            {
                DeviceMemoryReportFreeEvent(
                    pSubAllocData->objectHandle,
                    pSubAllocData->objectType,
                    pSubAllocData->memoryObjectId,
                    pSubAllocData->allocationData.flags.isExternal);
            }
            else
            {
                //PAL_ALERT_ALWAYS_MSG("SubFree of a Pal suballocation that was never reported to device_memory_report");
            }

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
        break;
    }
    default:
        break;
    }
}

// =====================================================================================================================
// GpuMemoryEventHandler is requested by VK_EXT_device_memory_report and/or VK_EXT_device_address_binding_report.
// Increment the reference count of requests for GPU memory events.
void GpuMemoryEventHandler::EnableGpuMemoryEvents()
{
    Util::AtomicIncrement(&m_memoryEventEnables);
}

// =====================================================================================================================
// Decrement the reference count of requests for GPU memory events.
void GpuMemoryEventHandler::DisableGpuMemoryEvents()
{
    Util::AtomicDecrement(&m_memoryEventEnables);
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
    const Pal::IGpuMemory*           pGpuMemory,
    uint64_t                         objectHandle,
    VkObjectType                     objectType,
    uint64_t                         heapIndex)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_allocationHashMapLock);
    AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemory);

    if (pAllocationData != nullptr)
    {
        if (pAllocationData->reportedToDeviceMemoryReport == false)
        {
            pAllocationData->reportedToDeviceMemoryReport = true;
            pAllocationData->objectType                   = objectType;
            pAllocationData->objectHandle                 = objectHandle;
            pAllocationData->allocationData.pGpuMemory    = pGpuMemory;

            const auto& gpuMemoryDesc = pAllocationData->allocationData.pGpuMemory->Desc();

            DeviceMemoryReportAllocateEvent(
                pAllocationData->objectHandle,
                gpuMemoryDesc.size,
                pAllocationData->objectType,
                gpuMemoryDesc.uniqueId,
                heapIndex,
                gpuMemoryDesc.flags.isExternal);
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the allocation of an already reported allocation.");
        }
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to correlate an untracked allocation.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanAllocationFailedEvent(
    Pal::gpusize                     allocatedSize,
    VkObjectType                     objectType,
    uint64_t                         heapIndex)
{
    DeviceMemoryReportAllocationFailedEvent(allocatedSize, objectType, heapIndex);
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanFreeEvent(
    const Pal::IGpuMemory*           pGpuMemory)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_allocationHashMapLock);
    AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemory);

    if (pAllocationData != nullptr)
    {
        if (pAllocationData->reportedToDeviceMemoryReport == true)
        {
            const auto& gpuMemoryDesc = pAllocationData->allocationData.pGpuMemory->Desc();

            DeviceMemoryReportFreeEvent(
                pAllocationData->objectHandle,
                pAllocationData->objectType,
                gpuMemoryDesc.uniqueId,
                gpuMemoryDesc.flags.isExternal);
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the free of an unreported allocation.");
        }
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the free of an untracked allocation.");
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanSubAllocateEvent(
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
            pSubAllocData->reportedToDeviceMemoryReport = true;
            pSubAllocData->allocationData.pGpuMemory    = pGpuMemory;
            pSubAllocData->memoryObjectId               = GenerateMemoryObjectId();
            pSubAllocData->objectType                   = objectType;
            pSubAllocData->offset                       = offset;
            pSubAllocData->subAllocationSize            = subAllocationSize;
            pSubAllocData->objectHandle                 = objectHandle;
            pSubAllocData->heapIndex                    = heapIndex;

            DeviceMemoryReportAllocateEvent(
                pSubAllocData->objectHandle,
                pSubAllocData->subAllocationSize,
                pSubAllocData->objectType,
                pSubAllocData->memoryObjectId,
                pSubAllocData->heapIndex,
                pSubAllocData->allocationData.pGpuMemory->Desc().flags.isExternal);
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Vulkan is reporting an already reported Vulkan suballocation.");
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::VulkanSubFreeEvent(
    const Pal::IGpuMemory*           pGpuMemory,
    Pal::gpusize                     offset)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_vulkanSubAllocationHashMapLock);
    SubAllocationKey key = {pGpuMemory->Desc().gpuVirtAddr,
                            offset};

    SubAllocationData* pSubAllocData = m_vulkanSubAllocationHashMap.FindKey(key);

    if (pSubAllocData != nullptr)
    {
        if (pSubAllocData->reportedToDeviceMemoryReport == true)
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
    Pal::gpusize                     gpuVirtAddr,
    Pal::gpusize                     offset,
    const uint64_t                   objectHandle,
    const VkObjectType               objectType)
{
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_palSubAllocationHashMapLock);

    SubAllocationKey key = {gpuVirtAddr,
                            offset};

    SubAllocationData* pSubAllocData = m_palSubAllocationHashMap.FindKey(key);

    if (pSubAllocData != nullptr)
    {
        if (pSubAllocData->reportedToDeviceMemoryReport == false)
        {
            // Report deferred Pal suballocation to device_memory_report now
            pSubAllocData->objectHandle                 = objectHandle;
            pSubAllocData->objectType                   = objectType;
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
            PAL_ALERT_ALWAYS_MSG("Vulkan is trying to report the allocation of an already reported Pal suballocation.");
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
    Util::RWLockAuto<RWLock::ReadWrite> lock(&m_callbacksLock);

    for (auto iter = m_callbacks.Begin(); iter.IsValid(); iter.Next())
    {
        iter.Get().callback(&callbackData, iter.Get().pData);
    }
}

} // namespace vk
