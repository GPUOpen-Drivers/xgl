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

#ifndef __GPUMEMORY_EVENT_HANDLER_H__
#define __GPUMEMORY_EVENT_HANDLER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"

#include "palHashMap.h"
#include "palMutex.h"
#include "palUtil.h"
#include "palVector.h"
#include "palDeveloperHooks.h"

namespace vk
{

class Instance;
class Device;
class PalAllocator;

// =====================================================================================================================
// GPU Memory Event Handler processes GPU memory events from Pal for VK_EXT_device_memory_report and
// VK_EXT_device_address_binding_report extensions
class GpuMemoryEventHandler
{
public:
    static VkResult Create(
        Instance*                    pInstance,
        GpuMemoryEventHandler**      ppObject);

    void Destroy();

    void PalDeveloperCallback(
        Pal::Developer::CallbackType type,
        void*                        pCbData);

    typedef struct
    {
        PFN_vkDeviceMemoryReportCallbackEXT callback;
        void*                               pData;
        const Device*                       pDevice;
    } DeviceMemoryReportCallback;

    typedef Util::Vector<DeviceMemoryReportCallback, 1, PalAllocator> DeviceMemoryReportCallbacks;

    void RegisterDeviceMemoryReportCallback(
        const DeviceMemoryReportCallback&   callback);

    void UnregisterDeviceMemoryReportCallbacks(
        const Device*                       pDevice);

    void VulkanAllocateEvent(
        const Pal::IGpuMemory*           pGpuMemory,
        uint64_t                         objectHandle,
        Util::gpusize                    allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         memoryObjectId,
        uint64_t                         heapIndex,
        bool                             isImport);

    void DeviceMemoryReportAllocateEvent(
        uint64_t                         objectHandle,
        Util::gpusize                    allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         memoryObjectId,
        uint64_t                         heapIndex,
        bool                             isImport);

    void DeviceMemoryReportAllocationFailedEvent(
        Util::gpusize                    allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         heapIndex);

    void DeviceMemoryReportFreeEvent(
        uint64_t                         objectHandle,
        VkObjectType                     objectType,
        uint64_t                         memoryObjectId,
        bool                             isUnimport);

protected:

private:
    GpuMemoryEventHandler(Instance* pInstance);

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemoryEventHandler);

    void SendDeviceMemoryReportEvent(
        const VkDeviceMemoryReportCallbackDataEXT& callbackData);

    // Generates an ID, unique within the instance, for a GPU memory object
    uint64_t GenerateMemoryObjectId() { return Util::AtomicIncrement64(&m_memoryObjectId); }

    Instance* m_pInstance;

    DeviceMemoryReportCallbacks m_callbacks;

    typedef struct
    {
        Pal::Developer::GpuMemoryData allocationData;
        uint64_t                      memoryObjectId;
        bool                          correlatedWithVulkan;
        bool                          reportedToDeviceMemoryReport;
    } AllocationData;

    typedef Util::HashMap<const Pal::IGpuMemory*, AllocationData, PalAllocator> GpuMemoryAllocationHashMap;

    GpuMemoryAllocationHashMap m_allocationHashMap;

    volatile uint64_t m_memoryObjectId; // Seed for memoryObjectId generation
};

} // namespace vk

#endif /* __GPUMEMORY_EVENT_HANDLER_H__ */
