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
#include "include/vk_utils.h"

#include "palIntrusiveList.h"
#include "palHashMap.h"
#include "palHashSet.h"
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

    void EnableGpuMemoryEvents(
        const Device* pDevice);

    void DisableGpuMemoryEvents(
        const Device* pDevice);

    VK_FORCEINLINE bool IsGpuMemoryEventHandlerEnabled() { return m_deviceCount > 0; }

    struct DeviceMemoryReportCallback
    {
        PFN_vkDeviceMemoryReportCallbackEXT callback;
        void*                               pData;
        const Device*                       pDevice;
    };

    typedef Util::Vector<DeviceMemoryReportCallback, 1, PalAllocator> DeviceMemoryReportCallbacks;

    void RegisterDeviceMemoryReportCallback(
        const DeviceMemoryReportCallback&   callback);

    void UnregisterDeviceMemoryReportCallbacks(
        const Device*                       pDevice);

    void VulkanAllocateEvent(
        const Device*                    pDevice,
        const Pal::IGpuMemory*           pGpuMemory,
        uint64_t                         objectHandle,
        VkObjectType                     objectType,
        uint64_t                         heapIndex,
        bool                             isBuddyAllocated
        );

    void VulkanAllocationFailedEvent(
        const Device*                    pDevice,
        Pal::gpusize                     allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         heapIndex);

    void VulkanSubAllocateEvent(
        const Device*                    pDevice,
        const Pal::IGpuMemory*           pGpuMemory,
        Pal::gpusize                     offset,
        Pal::gpusize                     subAllocationSize,
        uint64_t                         objectHandle,
        VkObjectType                     objectType,
        uint64_t                         heapIndex);

    void VulkanSubFreeEvent(
        const Device*                    pDevice,
        const Pal::IGpuMemory*           pGpuMemory,
        Pal::gpusize                     offset);

    void ReportDeferredPalSubAlloc(
        const Device*                    pDevice,
        Pal::gpusize                     gpuVirtAddr,
        Pal::gpusize                     offset,
        const uint64_t                   objectHandle,
        const VkObjectType               objectType);

protected:

private:
    GpuMemoryEventHandler(Instance* pInstance);

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemoryEventHandler);

    struct AllocationData
    {
        Pal::Developer::GpuMemoryData allocationData;
        uint64_t                      objectHandle;
        VkObjectType                  objectType;
        bool                          reportedToDeviceMemoryReport;
        bool                          isBuddyAllocated;
        uint64_t                      memoryObjectId;
        bool                          isExternal;
    };

    struct SubAllocationData
    {
        Pal::Developer::GpuMemoryData allocationData;
        uint64_t                      objectHandle;
        VkObjectType                  objectType;
        bool                          reportedToDeviceMemoryReport;
        uint64_t                      memoryObjectId;
        Pal::gpusize                  subAllocationSize;
        Pal::gpusize                  offset;
        uint64_t                      heapIndex;
    };

    struct BindData
    {
        Pal::Developer::BindGpuMemoryData bindGpuMemoryData;
        uint64_t                          objectHandle;
        VkObjectType                      objectType;
        bool                              reportedToDeviceAddressBindingReport;
    };

    class BindDataListNode
    {
    public:
        static void Create(
            Instance*                          pInstance,
            Pal::Developer::BindGpuMemoryData* pBindGpuMemoryData,
            BindDataListNode**                 ppObject);

        void Destroy();

        BindData*                                  GetData() { return &m_data; }
        Util::IntrusiveListNode<BindDataListNode>* GetNode() { return &m_node; }

    private:
        BindDataListNode(
            Instance*                          pInstance,
            Pal::Developer::BindGpuMemoryData* pBindGpuMemoryData);

        Instance*                                 m_pInstance;
        BindData                                  m_data;
        Util::IntrusiveListNode<BindDataListNode> m_node;

        PAL_DISALLOW_COPY_AND_ASSIGN(BindDataListNode);
    };

    struct Interval
    {
        Interval()
            : m_offset(0), m_size(0)
        {
        }

        Interval(const Pal::gpusize offset,const Pal::gpusize size)
            : m_offset(offset), m_size(size)
        {
        }

        Pal::gpusize m_offset;
        Pal::gpusize m_size;
    };

    static_assert(std::is_standard_layout<Interval>::value);

    void HandlePalDeveloperCallback(
        Pal::Developer::CallbackType type,
        void*                        pCbData);

    void DeviceMemoryReportAllocateEvent(
        uint64_t                         objectHandle,
        Pal::gpusize                     allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         memoryObjectId,
        uint64_t                         heapIndex,
        bool                             isImport);

    void DeviceMemoryReportAllocationFailedEvent(
        Pal::gpusize                     allocatedSize,
        VkObjectType                     objectType,
        uint64_t                         heapIndex);

    void DeviceMemoryReportFreeEvent(
        uint64_t                         objectHandle,
        VkObjectType                     objectType,
        uint64_t                         memoryObjectId,
        bool                             isUnimport);

    void SendDeviceMemoryReportEvent(
        const VkDeviceMemoryReportCallbackDataEXT& callbackData);

    // The caller of this function must hold the m_bindHashMapLock lock for read/write
    void DeviceAddressBindingReportUnbindEventCommon(
        const Pal::IGpuMemory*                pGpuMemory,
        const Interval&                       interval);

    void DeviceAddressBindingReportAllocBindEvent(
        const AllocationData* pAllocationData);

    void DeviceAddressBindingReportAllocUnbindEvent(
        const AllocationData* pAllocationData);

    void DeviceAddressBindingReportSuballocBindEvent(
        const SubAllocationData* pSubAllocData);

    void DeviceAddressBindingReportSuballocUnbindEvent(
        const SubAllocationData* pSubAllocData);

    void DeviceAddressBindingReportNewBindEvent(
        BindData* pNewBindData);

    void DeviceAddressBindingReportNewUnbindEvent(
        BindData* pNewBindData);

    void DeviceAddressBindingReportCallback(
        uint64_t                        objectHandle,
        VkObjectType                    objectType,
        VkDeviceAddressBindingTypeEXT   bindingType,
        VkDeviceAddress                 bindingAddress,
        VkDeviceSize                    allocatedSize,
        bool                            isInternal);

    void ReportBindEvent(
        BindData*                       pBindData,
        uint64_t                        objectHandle,
        VkObjectType                    objectType);

    void ReportUnbindEvent(
        BindData*                       pBindData);

    bool CheckIntervalsIntersect(
        const Interval&                 intervalOne,
        const Interval&                 intervalTwo) const;

    Instance* m_pInstance;

    DeviceMemoryReportCallbacks m_callbacks;
    Util::RWLock                m_callbacksLock;

    typedef Util::HashMap<const Pal::IGpuMemory*,
                          AllocationData,
                          PalAllocator> GpuMemoryAllocationHashMap;

    GpuMemoryAllocationHashMap m_allocationHashMap;
    Util::RWLock               m_allocationHashMapLock;

    struct SubAllocationKey
    {
        Pal::gpusize                  gpuVirtAddr;
        Pal::gpusize                  offset;
    };

    typedef Util::HashMap<SubAllocationKey,
                          SubAllocationData,
                          PalAllocator,
                          Util::JenkinsHashFunc> GpuMemorySubAllocationHashMap;

    typedef Util::IntrusiveList<BindDataListNode> BindDataList;

    typedef Util::HashMap<const Pal::IGpuMemory*,
                          BindDataList,
                          PalAllocator> GpuMemoryBindHashMap;

    GpuMemorySubAllocationHashMap m_vulkanSubAllocationHashMap;
    Util::RWLock                  m_vulkanSubAllocationHashMapLock;

    GpuMemorySubAllocationHashMap m_palSubAllocationHashMap;
    Util::RWLock                  m_palSubAllocationHashMapLock;

    GpuMemoryBindHashMap          m_bindHashMap;
    Util::Mutex                   m_bindHashMapMutex;

    typedef Util::HashSet<const Device*, PalAllocator> DeviceHashSet;

    DeviceHashSet                 m_deviceHashSet;
    Util::RWLock                  m_deviceHashSetLock;

    volatile uint32_t m_deviceCount;                // The number of devices with extensions that require memory events
};

} // namespace vk

#endif /* __GPUMEMORY_EVENT_HANDLER_H__ */
