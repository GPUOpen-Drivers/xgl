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
    m_memoryObjectId(1)
{
    m_allocationHashMap.Init();
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
        auto* pGpuMemoryData = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);

        bool exists = false;
        AllocationData* pAllocationData = nullptr;

        Pal::Result palResult = m_allocationHashMap.FindAllocate(pGpuMemoryData->pGpuMemory, &exists, &pAllocationData);

        if (palResult == Pal::Result::Success)
        {
            // Add the new value if it did not exist already.
            if (exists == false)
            {
                // Store the allocation
                pAllocationData->allocationData = *pGpuMemoryData;

                // If this is a Pal internal allocation, also report it to device_memory_report now
                if (pGpuMemoryData->flags.isClient == 0)
                {
                    pAllocationData->memoryObjectId               = GenerateMemoryObjectId();
                    pAllocationData->reportedToDeviceMemoryReport = true;

                    auto iter = m_callbacks.Begin();
                    uint64_t vulkanHeapIndex = 0;

                    if (iter.IsValid())
                    {
                        uint32_t heapIndex = 0;
                        auto* pPhysicalDevice = (iter.Get().pDevice)->VkPhysicalDevice(DefaultDeviceIndex);
                        bool result = pPhysicalDevice->GetVkHeapIndexFromPalHeap(pGpuMemoryData->heap, &heapIndex);

                        if (result == true)
                        {
                            vulkanHeapIndex = heapIndex;
                        }
                    }

                    DeviceMemoryReportAllocateEvent(
                        reinterpret_cast<uint64_t>(m_pInstance),
                        pGpuMemoryData->size,
                        VK_OBJECT_TYPE_INSTANCE,
                        pAllocationData->memoryObjectId,
                        vulkanHeapIndex,
                        pGpuMemoryData->flags.isExternal);
                }
            }
        }

        break;
    }
    case Pal::Developer::CallbackType::FreeGpuMemory:
    {
        auto*           pGpuMemoryData  = reinterpret_cast<Pal::Developer::GpuMemoryData*>(pCbData);
        AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemoryData->pGpuMemory);

        if (pAllocationData != nullptr)
        {
            if (pAllocationData->reportedToDeviceMemoryReport == true)
            {
                if (pGpuMemoryData->flags.isClient == 0)
                {
                    DeviceMemoryReportFreeEvent(
                        reinterpret_cast<uint64_t>(m_pInstance),
                        VK_OBJECT_TYPE_INSTANCE,
                        pAllocationData->memoryObjectId,
                        pGpuMemoryData->flags.isExternal);
                }
            }

            m_allocationHashMap.Erase(pGpuMemoryData->pGpuMemory);
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
void GpuMemoryEventHandler::RegisterDeviceMemoryReportCallback(
    const DeviceMemoryReportCallback& callback)
{
    m_callbacks.PushBack(callback);
}

// =====================================================================================================================
void GpuMemoryEventHandler::UnregisterDeviceMemoryReportCallbacks(
    const Device*                     pDevice)
{
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
    Util::gpusize                    allocatedSize,
    VkObjectType                     objectType,
    uint64_t                         memoryObjectId,
    uint64_t                         heapIndex,
    bool                             isImport)
{
    AllocationData* pAllocationData = m_allocationHashMap.FindKey(pGpuMemory);

    if (pAllocationData != nullptr)
    {
        pAllocationData->correlatedWithVulkan = true;
        pAllocationData->reportedToDeviceMemoryReport = true;
        pAllocationData->memoryObjectId = memoryObjectId;

        DeviceMemoryReportAllocateEvent(
            objectHandle,
            allocatedSize,
            objectType,
            memoryObjectId,
            heapIndex,
            isImport);
    }
}

// =====================================================================================================================
void GpuMemoryEventHandler::DeviceMemoryReportAllocateEvent(
    uint64_t                         objectHandle,
    Util::gpusize                    allocatedSize,
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
    Util::gpusize                    allocatedSize,
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
void GpuMemoryEventHandler::SendDeviceMemoryReportEvent(
    const VkDeviceMemoryReportCallbackDataEXT& callbackData)
{
    for (auto iter = m_callbacks.Begin(); iter.IsValid(); iter.Next())
    {
        iter.Get().callback(&callbackData, iter.Get().pData);
    }
}

} // namespace vk
