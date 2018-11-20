/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_memory.cpp
 * @brief Contains implementation of Vulkan memory objects, representing GPU memory allocations.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_image.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_utils.h"

#include "palSysMemory.h"
#include "palGpuMemory.h"
#include "palSysUtil.h"

namespace vk
{

// =====================================================================================================================
// Creates a new GPU memory object
VkResult Memory::Create(
    Device*                         pDevice,
    const VkMemoryAllocateInfo*     pAllocInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkDeviceMemory*                 pMemoryHandle)
{
    Memory*  pMemory    = nullptr;

    VkResult vkResult   = VK_SUCCESS;

    union
    {
        const VkStructHeader*                   pHeader;
        const VkMemoryAllocateInfo*             pInfo;
        const VkImportMemoryHostPointerInfoEXT* pImportMemoryInfo;
    };

    VK_ASSERT(pDevice != nullptr);
    VK_ASSERT(pAllocInfo != nullptr);
    VK_ASSERT(pMemoryHandle != nullptr);

    const Pal::DeviceProperties&            palProperties    = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
    const VkPhysicalDeviceMemoryProperties& memoryProperties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryProperties();

    // Create a mask to indicate the devices the memory allocations happened on
    bool multiInstanceHeap  = false;
    uint32_t allocationMask = (1 << DefaultDeviceIndex);

    // indicate whether it is a allocation that supposed to be imported.
    Pal::OsExternalHandle handle    = 0;
    bool sharedViaNtHandle          = false;
    bool isExternal                 = false;
    bool isHostMappedForeign        = false;
    void* pPinnedHostPtr            = nullptr; // If non-null, this memory is allocated as pinned system memory

    Pal::GpuMemoryExportInfo exportInfo = {};

    // Determines towards which devices we have accounted memory size
    uint32_t sizeAccountedForDeviceMask = 0u;

    // take the allocation count ahead of time.
    // it will set the VK_ERROR_TOO_MANY_OBJECTS
    vkResult = pDevice->IncreaseAllocationCount();

    // Copy Vulkan API allocation info to local PAL version
    Pal::GpuMemoryCreateInfo createInfo = {};

    // Assign default priority based on panel setting (this may get elevated later by memory binds)
    MemoryPriority priority = MemoryPriority::FromSetting(pDevice->GetRuntimeSettings().memoryPriorityDefault);

    createInfo.priority       = priority.PalPriority();
    createInfo.priorityOffset = priority.PalOffset();
    Image* pBoundImage        = nullptr;

    for (pInfo = pAllocInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
            case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO:
                // Get memory requirements calls don't pad to our allocation granularity, which is preferred for
                // suballocation.  However, PAL requires that we respect this granularity on GPU memory allocs.
                createInfo.size = Util::Pow2Align(pInfo->allocationSize,
                                                  palProperties.gpuMemoryProperties.realMemAllocGranularity);

                // Calculate the required base address alignment for the given memory type.  These alignments are
                // roughly worst-case alignments required by images that may be hosted within this memory object.
                // The base address alignment of the memory object is large enough to cover the base address
                // requirements of most images, and images add internal padding for the most extreme alignment
                // requirements.
                if (createInfo.size != 0)
                {
                    createInfo.alignment = pDevice->GetMemoryBaseAddrAlignment(1UL << pInfo->memoryTypeIndex);
                }

                createInfo.heapCount = 1;
                createInfo.heaps[0]  = pDevice->GetPalHeapFromVkTypeIndex(pInfo->memoryTypeIndex);

                if (pDevice->NumPalDevices() > 1)
                {
                    multiInstanceHeap = (memoryProperties.memoryHeaps[pInfo->memoryTypeIndex].flags &
                                         VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) != 0;

                    if (multiInstanceHeap)
                    {
                        // In the MGPU scenario, the peerWritable is required to allocate the local video memory
                        // We should not set the peerWritable for remote heap.
                        createInfo.flags.peerWritable = 1;

                        allocationMask = pDevice->GetPalDeviceMask();
                    }
                    else
                    {
                        VK_ASSERT((createInfo.heaps[0] == Pal::GpuHeapGartCacheable) ||
                                  (createInfo.heaps[0] == Pal::GpuHeapGartUswc));

                        createInfo.flags.shareable = 1;
                        allocationMask = 1 << DefaultMemoryInstanceIdx;
                    }
                }

                if (pDevice->GetRuntimeSettings().memoryEnableRemoteBackupHeap)
                {
                    if ((createInfo.heaps[0] == Pal::GpuHeapLocal) ||
                        (createInfo.heaps[0] == Pal::GpuHeapInvisible))
                    {
                        createInfo.heaps[createInfo.heapCount++] = Pal::GpuHeapGartUswc;
                    }
                }
                break;
            case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
                {
                    const VkImportMemoryFdInfoKHR* pImportMemoryFdInfo =
                        reinterpret_cast<const VkImportMemoryFdInfoKHR *>(pHeader);
                    VK_ASSERT(pImportMemoryFdInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
                    handle = pImportMemoryFdInfo->fd;
                    isExternal = true;
                }
                break;
            case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
            {
                const VkExportMemoryAllocateInfo* pExportMemory =
                    reinterpret_cast<const VkExportMemoryAllocateInfo *>(pHeader);
                    VK_ASSERT(pExportMemory->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
                    createInfo.flags.interprocess = 1;
                    // Todo: we'd better to pass in the handleTypes to the Pal as well.
                    // The supported handleType should also be provided by Pal as Device Capabilities.
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
            {
                const VkMemoryAllocateFlagsInfo * pMemoryAllocateFlags =
                    reinterpret_cast<const VkMemoryAllocateFlagsInfo *>(pHeader);

                if ((pMemoryAllocateFlags->flags & VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT) != 0)
                {
                    VK_ASSERT(pMemoryAllocateFlags->deviceMask != 0);
                    VK_ASSERT((pDevice->GetPalDeviceMask() & pMemoryAllocateFlags->deviceMask) ==
                        pMemoryAllocateFlags->deviceMask);

                    allocationMask = pMemoryAllocateFlags->deviceMask;
                }
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
            {
                const VkMemoryDedicatedAllocateInfo* pDedicatedInfo =
                    reinterpret_cast<const VkMemoryDedicatedAllocateInfo *>(pHeader);
                if (pDedicatedInfo->image != VK_NULL_HANDLE)
                {
                    pBoundImage       = Image::ObjectFromHandle(pDedicatedInfo->image);
                    createInfo.pImage = pBoundImage->PalImage(DefaultDeviceIndex);
                }
            }
            break;

            default:
                switch (static_cast<uint32_t>(pHeader->sType))
                {
                case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:
                {
                    VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_EXTERNAL_MEMORY_HOST));

                    VK_ASSERT(pImportMemoryInfo->handleType &
                        (VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT));

                    pPinnedHostPtr = pImportMemoryInfo->pHostPointer;
                }
                break;

                default:
                    // Skip any unknown extension structures
                    break;
                }
                break;
        }
    }

    // Check for OOM before actually allocating to avoid overhead. Do not account for the memory allocation yet
    // since the commitment size can still increase
    if ((vkResult == VK_SUCCESS) &&
        (pDevice->IsAllocationSizeTrackingEnabled()) &&
        ((createInfo.heaps[0] == Pal::GpuHeap::GpuHeapInvisible) ||
         (createInfo.heaps[0] == Pal::GpuHeap::GpuHeapLocal)))
    {
        vkResult = pDevice->TryIncreaseAllocatedMemorySize(createInfo.size, allocationMask, createInfo.heaps[0]);

        if (vkResult == VK_SUCCESS)
        {
            sizeAccountedForDeviceMask = allocationMask;
        }
    }

    if (vkResult == VK_SUCCESS)
    {
        if (isExternal)
        {
            ImportMemoryInfo importInfo = {};
            importInfo.handle       = handle;
            importInfo.isNtHandle   = sharedViaNtHandle;
            vkResult = OpenExternalMemory(pDevice, importInfo, &pMemory);
        }
        else
        {
            if (pPinnedHostPtr == nullptr)
            {
                vkResult = CreateGpuMemory(
                    pDevice,
                    pAllocator,
                    createInfo,
                    exportInfo,
                    allocationMask,
                    multiInstanceHeap,
                    &pMemory);
            }
            else
            {
                vkResult = CreateGpuPinnedMemory(
                    pDevice,
                    pAllocator,
                    createInfo,
                    allocationMask,
                    multiInstanceHeap,
                    isHostMappedForeign,
                    pPinnedHostPtr,
                    &pMemory);
            }
        }
    }

    if ((vkResult == VK_SUCCESS) &&
        (sizeAccountedForDeviceMask != 0u))
    {
        // Account for committed size in logical device. The destructor will decrease the counter accordingly.
        vkResult = pDevice->IncreaseAllocatedMemorySize(pMemory->m_info.size, sizeAccountedForDeviceMask, pMemory->m_info.heaps[0]);

        if (vkResult != VK_SUCCESS)
        {
            pMemory->Free(pDevice, pAllocator);
        }
    }

    if (vkResult != VK_SUCCESS)
    {
        if (vkResult != VK_ERROR_TOO_MANY_OBJECTS)
        {
            // Something failed after the allocation count was incremented
            pDevice->DecreaseAllocationCount();
        }
    }
    else
    {
        // Notify the memory object that it is counted so that the destructor can decrease the counter accordingly
        pMemory->SetAllocationCounted(sizeAccountedForDeviceMask);

        *pMemoryHandle = Memory::HandleFromObject(pMemory);
    }

    return vkResult;
}

// =====================================================================================================================
// The function is used to acquire the primary index in case it is not a multi intance allocation.
// The returned pIndex refers to the index of least significant set bit of the allocationMask.
void Memory::GetPrimaryDeviceIndex(
    uint32_t  maxDevices,
    uint32_t  allocationMask,
    uint32_t* pIndex,
    bool*     pMultiInstance)
{
    if (Util::CountSetBits(allocationMask) > 1)
    {
        *pMultiInstance = true;
    }
    else
    {
        *pMultiInstance = false;
    }

    Util::BitMaskScanForward(pIndex, allocationMask);
}

// =====================================================================================================================
// Create GPU Memory on each required device.
// The function only create the PalMemory from device I and can be used on device I.
// The export/import for resource sharing across device is not covered here.
VkResult Memory::CreateGpuMemory(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator,
    const Pal::GpuMemoryCreateInfo& createInfo,
    const Pal::GpuMemoryExportInfo& exportInfo,
    uint32_t                        allocationMask,
    bool                            multiInstanceHeap,
    Memory**                        ppMemory)
{
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};
    VK_ASSERT(allocationMask != 0);

    size_t   gpuMemorySize = 0;
    uint8_t *pSystemMem = nullptr;

    uint32_t primaryIndex = 0;
    bool multiInstance    = false;

    GetPrimaryDeviceIndex(pDevice->NumPalDevices(), allocationMask, &primaryIndex, &multiInstance);

    Pal::Result palResult;
    VkResult    vkResult = VK_SUCCESS;

    VK_ASSERT(ppMemory != nullptr);

    if (createInfo.size != 0)
    {
        gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemorySize(createInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        const size_t apiSize = sizeof(Memory);
        const size_t palSize = gpuMemorySize * pDevice->NumPalDevices();

        // Allocate enough for the PAL memory object and our own dispatchable memory
        pSystemMem = static_cast<uint8_t*>(
            pAllocator->pfnAllocation(
                pAllocator->pUserData,
                apiSize + palSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (pSystemMem != nullptr)
        {
            size_t palMemOffset = apiSize;

            for (uint32_t deviceIdx = 0;
                (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                 deviceIdx++)
            {
                if (((1 << deviceIdx) & allocationMask) != 0)
                {
                    Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

                    // Allocate the PAL memory object
                    palResult = pPalDevice->CreateGpuMemory(
                        createInfo, Util::VoidPtrInc(pSystemMem, palMemOffset), &pGpuMemory[deviceIdx]);

                    if (palResult == Pal::Result::Success)
                    {
                        // Add the GPU memory object to the residency list
                        palResult = pDevice->AddMemReference(pPalDevice, pGpuMemory[deviceIdx]);

                        if (palResult != Pal::Result::Success)
                        {
                            pGpuMemory[deviceIdx]->Destroy();
                            pGpuMemory[deviceIdx] = nullptr;
                        }
                    }
                }
                palMemOffset += gpuMemorySize;
            }

            if (palResult == Pal::Result::Success)
            {
                Pal::OsExternalHandle handle = 0;

                // Initialize dispatchable memory object and return to application
                *ppMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice,
                                                                pGpuMemory,
                                                                handle,
                                                                createInfo,
                                                                multiInstance,
                                                                primaryIndex);
            }
            else
            {
                // Something went wrong, clean up
                for (int32_t deviceIdx = pDevice->NumPalDevices() - 1; deviceIdx >= 0; --deviceIdx)
                {
                    if (pGpuMemory[deviceIdx] != nullptr)
                    {
                        Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

                        pDevice->RemoveMemReference(pPalDevice, pGpuMemory[deviceIdx]);
                        pGpuMemory[deviceIdx]->Destroy();
                    }
                }

                pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);

                if (palResult == Pal::Result::ErrorOutOfGpuMemory)
                {
                    vkResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
                }
                else
                {
                    vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
        else
        {
            vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    else
    {
        // Allocate memory only for the dispatchable object
        pSystemMem = static_cast<uint8_t*>(
            pAllocator->pfnAllocation(
                pAllocator->pUserData,
                sizeof(Memory),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (pSystemMem != nullptr)
        {
            // Initialize dispatchable memory object and return to application
            Pal::IGpuMemory* pDummyPalGpuMemory[MaxPalDevices] = {};
            *ppMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice,
                                                            pDummyPalGpuMemory,
                                                            0,
                                                            createInfo,
                                                            false,
                                                            DefaultDeviceIndex);
        }
        else
        {
            vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return vkResult;
}

// =====================================================================================================================
// Create Pinned Memory on each required device.
// The function only create the PalMemory from device I and can be used on device I.
// The export/import for resource sharing across device is not covered here.
VkResult Memory::CreateGpuPinnedMemory(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator,
    const Pal::GpuMemoryCreateInfo& createInfo,
    uint32_t                        allocationMask,
    bool                            multiInstanceHeap,
    bool                            isHostMappedForeign,
    void*                           pPinnedHostPtr,
    Memory**                        ppMemory)
{
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};

    size_t   gpuMemorySize = 0;
    uint8_t *pSystemMem = nullptr;

    Pal::Result palResult;
    VkResult    vkResult = VK_SUCCESS;

    uint32_t primaryIndex  = 0;
    bool     multiInstance = false;

    GetPrimaryDeviceIndex(pDevice->NumPalDevices(), allocationMask, &primaryIndex, &multiInstance);

    // It is really confusing to see multiInstance pinned memory.
    // Assert has been added to catch the unexpected case.
    VK_ASSERT(!multiInstance);

    VK_ASSERT(ppMemory != nullptr);

    // Get CPU memory requirements for PAL
    Pal::PinnedGpuMemoryCreateInfo pinnedInfo = {};

    VK_ASSERT(Util::IsPow2Aligned(reinterpret_cast<uint64_t>(pPinnedHostPtr),
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gpuMemoryProperties.realMemAllocGranularity));

    pinnedInfo.size = static_cast<size_t>(createInfo.size);
    pinnedInfo.pSysMem = pPinnedHostPtr;
    pinnedInfo.vaRange = Pal::VaRange::Default;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 450
    pinnedInfo.alignment = createInfo.alignment;
#endif

    gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetPinnedGpuMemorySize(
        pinnedInfo, &palResult);

    if (palResult != Pal::Result::Success)
    {
        vkResult = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }

    const size_t apiSize = sizeof(Memory);
    const size_t palSize = gpuMemorySize * pDevice->NumPalDevices();

    if (vkResult == VK_SUCCESS)
    {
        // Allocate enough for the PAL memory object and our own dispatchable memory
        pSystemMem = static_cast<uint8_t*>(
            pAllocator->pfnAllocation(
                pAllocator->pUserData,
                apiSize + palSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        // Check for out of memory
        if (pSystemMem != nullptr)
        {
            size_t palMemOffset = apiSize;

            for (uint32_t deviceIdx = 0;
                (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                 deviceIdx++)
            {
                if (((1 << deviceIdx) & allocationMask) != 0)
                {
                    Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

                    // Allocate the PAL memory object
                    palResult = pPalDevice->CreatePinnedGpuMemory(
                        pinnedInfo, Util::VoidPtrInc(pSystemMem, palMemOffset), &pGpuMemory[deviceIdx]);

                    if (palResult == Pal::Result::Success)
                    {
                        // Add the GPU memory object to the residency list
                        palResult = pDevice->AddMemReference(pPalDevice, pGpuMemory[deviceIdx]);

                        if (palResult != Pal::Result::Success)
                        {
                            pGpuMemory[deviceIdx]->Destroy();
                            pGpuMemory[deviceIdx] = nullptr;
                        }
                    }
                }

                palMemOffset += gpuMemorySize;
            }

            if (palResult == Pal::Result::Success)
            {
                // Initialize dispatchable memory object and return to application
                *ppMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice,
                                                                pGpuMemory,
                                                                0,
                                                                createInfo,
                                                                multiInstance,
                                                                primaryIndex);
            }
            else
            {
                // Something went wrong, clean up
                for (int32_t deviceIdx = pDevice->NumPalDevices() - 1; deviceIdx >= 0; --deviceIdx)
                {
                    if (pGpuMemory[deviceIdx] != nullptr)
                    {
                        Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

                        pDevice->RemoveMemReference(pPalDevice, pGpuMemory[deviceIdx]);
                        pGpuMemory[deviceIdx]->Destroy();
                    }
                }

                pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);

                vkResult = VK_ERROR_INVALID_EXTERNAL_HANDLE;
            }
        }
        else
        {
            vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return vkResult;
}

// =====================================================================================================================
VkResult Memory::OpenExternalSharedImage(
    Device*                 pDevice,
    Image*                  pBoundImage,
    const ImportMemoryInfo& importInfo,
    Memory**                ppVkMemory)
{
    VkResult result = VK_SUCCESS;
    size_t palImgSize = 0;
    size_t palMemSize = 0;
    Pal::ImageCreateInfo palImgCreateInfo = {};
    Pal::GpuMemoryCreateInfo palMemCreateInfo = {};

    Pal::ExternalImageOpenInfo palOpenInfo = {};

    palOpenInfo.swizzledFormat  = VkToPalFormat(pBoundImage->GetFormat());
    palOpenInfo.usage           = VkToPalImageUsageFlags(pBoundImage->GetImageUsage(),
                                                         pBoundImage->GetFormat(),
                                                         1,
                                                         (VkImageUsageFlags)(0),
                                                         (VkImageUsageFlags)(0));

    palOpenInfo.resourceInfo.hExternalResource = importInfo.handle;
    palOpenInfo.resourceInfo.flags.ntHandle    = importInfo.isNtHandle;

    Pal::Result palResult = Pal::Result::Success;
    if (importInfo.handle == 0)
    {
    }

    palResult = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedImageSizes(
        palOpenInfo,
        &palImgSize,
        &palMemSize,
        &palImgCreateInfo);

    const size_t totalSize = palImgSize + sizeof(Memory) + palMemSize;

    void* pMemMemory = static_cast<uint8_t*>(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->AllocMem(
        totalSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

    if (pMemMemory == nullptr)
    {
        palResult = Pal::Result::ErrorOutOfMemory;
    }

    Pal::IGpuMemory* pPalMemory[MaxPalDevices] = {};
    Pal::IImage*     pExternalImage            = nullptr;
    if (palResult == Pal::Result::Success)
    {
        void* pPalMemAddr    = Util::VoidPtrInc(pMemMemory, sizeof(Memory));
        void* pImgMemoryAddr = Util::VoidPtrInc(pPalMemAddr, palMemSize);

        palResult = pDevice->PalDevice(DefaultDeviceIndex)->OpenExternalSharedImage(
            palOpenInfo,
            pImgMemoryAddr,
            pPalMemAddr,
            &palMemCreateInfo,
            &pExternalImage,
            &pPalMemory[DefaultDeviceIndex]);

        if (palResult == Pal::Result::Success)
        {
            // Add the GPU memory object to the residency list
            palResult = pDevice->AddMemReference(pDevice->PalDevice(DefaultDeviceIndex), pPalMemory[DefaultDeviceIndex]);

            if (palResult == Pal::Result::Success)
            {
                const uint32_t allocationMask = (1 << DefaultMemoryInstanceIdx);
                // Initialize dispatchable memory object and return to application
                *ppVkMemory = VK_PLACEMENT_NEW(pMemMemory) Memory(pDevice,
                                                                  pPalMemory,
                                                                  palOpenInfo.resourceInfo.hExternalResource,
                                                                  palMemCreateInfo,
                                                                  false,
                                                                  DefaultDeviceIndex,
                                                                  pExternalImage);
            }
            else
            {
                pExternalImage->Destroy();
                pPalMemory[DefaultDeviceIndex]->Destroy();
            }
        }

        if (palResult != Pal::Result::Success)
        {
            pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->FreeMem(pMemMemory);
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
void Memory::Init(
    Pal::IGpuMemory** ppPalMemory)
{
    memset(m_pPalMemory, 0, sizeof(m_pPalMemory));
    for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; deviceIdx++)
    {
        m_pPalMemory[deviceIdx][deviceIdx] = ppPalMemory[deviceIdx];
    }
}

// =====================================================================================================================
Memory::Memory(
    vk::Device*                     pDevice,
    Pal::IGpuMemory**               ppPalMemory,
    Pal::OsExternalHandle           sharedGpuMemoryHandle,
    const Pal::GpuMemoryCreateInfo& info,
    bool                            multiInstance,
    uint32_t                        primaryIndex,
    Pal::IImage*                    pExternalImage)
:
    m_pDevice(pDevice),
    m_info(info),
    m_priority(info.priority, info.priorityOffset),
    m_multiInstance(multiInstance),
    m_allocationCounted(false),
    m_sizeAccountedForDeviceMask(0),
    m_pExternalPalImage(pExternalImage),
    m_primaryDeviceIndex(primaryIndex),
    m_sharedGpuMemoryHandle(sharedGpuMemoryHandle)
{
    Init(ppPalMemory);
}

// =====================================================================================================================
Memory::Memory(
    Device*           pDevice,
    Pal::IGpuMemory** ppPalMemory,
    bool              multiInstance,
    uint32_t          primaryIndex)
:
    m_pDevice(pDevice),
    m_multiInstance(multiInstance),
    m_allocationCounted(false),
    m_sizeAccountedForDeviceMask(0),
    m_pExternalPalImage(nullptr),
    m_primaryDeviceIndex(primaryIndex)
{
    // PAL info is not available for memory objects allocated for presentable images
    memset(&m_info, 0, sizeof(m_info));
    Init(ppPalMemory);
}

// =====================================================================================================================
// Free a GPU memory object - also destroys the API memory object
void Memory::Free(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    if (m_pExternalPalImage != nullptr)
    {
        m_pExternalPalImage->Destroy();
        m_pExternalPalImage = nullptr;
    }

    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); ++i)
    {
        for (uint32_t j = 0; j < m_pDevice->NumPalDevices(); ++j)
        {
            // Free the child memory first
            if (i != j)
            {
                Pal::IGpuMemory* pGpuMemory = m_pPalMemory[i][j];
                if (pGpuMemory != nullptr)
                {
                    Pal::IDevice* pPalDevice = pDevice->PalDevice(i);
                    pDevice->RemoveMemReference(pPalDevice, pGpuMemory);

                    // Destroy PAL memory object
                    pGpuMemory->Destroy();

                    // the GpuMemory in [i,j] where i != j need to be freed explicitly.
                    pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->FreeMem(pGpuMemory);
                }
            }
        }
    }

    // Free the parent memory
    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); ++i)
    {
        Pal::IGpuMemory* pGpuMemory = m_pPalMemory[i][i];
        if (pGpuMemory != nullptr)
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(i);
            pDevice->RemoveMemReference(pPalDevice, pGpuMemory);

            // Destroy PAL memory object
            pGpuMemory->Destroy();
        }
    }

    // Decrease the allocation count
    if (m_allocationCounted)
    {
        m_pDevice->DecreaseAllocationCount();
    }

    // Decrease the allocation size
    if (m_sizeAccountedForDeviceMask != 0)
    {
        m_pDevice->DecreaseAllocatedMemorySize(m_info.size, m_sizeAccountedForDeviceMask, m_info.heaps[0]);
    }

    // Call destructor
    Util::Destructor(this);

    // Free outer container
    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Opens a POSIX external shared handle and creates a memory object corresponding to it.
// Open external memory should not be multi-instance allocation.
VkResult Memory::OpenExternalMemory(
    Device*                 pDevice,
    const ImportMemoryInfo& importInfo,
    Memory**                ppMemory)
{
    Pal::ExternalGpuMemoryOpenInfo openInfo = {};
    Pal::GpuMemoryCreateInfo createInfo = {};
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};
    Pal::Result palResult;
    size_t gpuMemorySize;
    uint8_t *pSystemMem;

    VK_ASSERT(pDevice  != nullptr);
    VK_ASSERT(ppMemory != nullptr);

    const uint32_t allocationMask = (1 << DefaultMemoryInstanceIdx);
    if (importInfo.handle == 0)
    {
    }
    else
    {
        openInfo.resourceInfo.hExternalResource = importInfo.handle;
    }

    openInfo.resourceInfo.flags.ntHandle    = importInfo.isNtHandle;
    // Get CPU memory requirements for PAL
    gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedGpuMemorySize(&palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Allocate enough for the PAL memory object and our own dispatchable memory
    pSystemMem = static_cast<uint8_t*>(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->AllocMem(
        gpuMemorySize + sizeof(Memory),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

    // Check for out of memory
    if (pSystemMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Allocate the PAL memory object
    palResult = pDevice->PalDevice(DefaultDeviceIndex)->OpenExternalSharedGpuMemory(openInfo,
                                                                  pSystemMem + sizeof(Memory),
                                                                  &createInfo,
                                                                  &pGpuMemory[DefaultDeviceIndex]);

    // On success...
    if (palResult == Pal::Result::Success)
    {
        // Add the GPU memory object to the residency list
        palResult = pDevice->AddMemReference(pDevice->PalDevice(DefaultDeviceIndex), pGpuMemory[DefaultDeviceIndex]);

        if (palResult == Pal::Result::Success)
        {
            // Initialize dispatchable memory object and return to application
            *ppMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice,
                                                           pGpuMemory,
                                                           openInfo.resourceInfo.hExternalResource,
                                                           createInfo,
                                                           false,
                                                           DefaultDeviceIndex);
        }
        else
        {
            pGpuMemory[DefaultDeviceIndex]->Destroy();
        }
    }

    if (palResult != Pal::Result::Success)
    {
        // Construction of PAL memory object failed. Free the memory before returning to application.
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->FreeMem(pSystemMem);
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Returns the external shared handle of the memory object.
Pal::OsExternalHandle Memory::GetShareHandle(
    VkExternalMemoryHandleTypeFlagBits handleType)
{
    VK_ASSERT((m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion() >= VK_MAKE_VERSION(1, 1, 0)) ||
              m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_EXTERNAL_MEMORY_FD)             ||
              m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_EXTERNAL_MEMORY_WIN32));
    Pal::OsExternalHandle handle = 0;

    Pal::GpuMemoryExportInfo exportInfo = {};
    handle = PalMemory(DefaultDeviceIndex)->ExportExternalHandle(exportInfo);

    return handle;
}

// =====================================================================================================================
// Map GPU memory into client address space. Simply calls through to PAL.
VkResult Memory::Map(
    VkFlags      flags,
    VkDeviceSize offset,
    VkDeviceSize size,
    void**       ppData)
{
    VkResult result = VK_SUCCESS;

    // According to spec, "memory must not have been allocated with multiple instances"
    // if it is multi-instance allocation, we should just return VK_ERROR_MEMORY_MAP_FAILED
    if (!m_multiInstance)
    {
        Pal::Result palResult = Pal::Result::Success;
        if (PalMemory(m_primaryDeviceIndex) != nullptr)
        {
            void* pData;

            palResult = PalMemory(m_primaryDeviceIndex)->Map(&pData);

            if (palResult == Pal::Result::Success)
            {
                *ppData = Util::VoidPtrInc(pData, static_cast<size_t>(offset));

            }
            result = (palResult == Pal::Result::Success) ? VK_SUCCESS : VK_ERROR_MEMORY_MAP_FAILED;
        }
        else
        {
            result = VK_ERROR_MEMORY_MAP_FAILED;
        }
    }
    else
    {
        result = VK_ERROR_MEMORY_MAP_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Unmap previously mapped memory object. Just calls PAL.
void Memory::Unmap(void)
{
    Pal::Result palResult = Pal::Result::Success;

    VK_ASSERT(m_multiInstance == false);

    palResult = PalMemory(m_primaryDeviceIndex)->Unmap();
    VK_ASSERT(palResult == Pal::Result::Success);
}

// =====================================================================================================================
// Returns the actual number of bytes that are currently committed to this memory object
VkResult Memory::GetCommitment(VkDeviceSize* pCommittedMemoryInBytes)
{
    VK_ASSERT(pCommittedMemoryInBytes != nullptr);

    // We never allocate memory lazily, so just return the size of the memory object
    *pCommittedMemoryInBytes = m_info.size;

    return VK_SUCCESS;
}

// =====================================================================================================================
// This function increases the priority of this memory's allocation to be at least that of the given priority.  This
// function may be called e.g. when this memory is bound to a high-priority VkImage.
void Memory::ElevatePriority(MemoryPriority priority)
{
    // Update PAL memory object's priority using a double-checked lock if the current priority is lower than
    // the new given priority.
    if (m_priority < priority)
    {
        Util::MutexAuto lock(m_pDevice->GetMemoryMutex());

        if (m_priority < priority)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                if ((PalMemory(deviceIdx) != nullptr) &&
                    (PalMemory(deviceIdx)->SetPriority(priority.PalPriority(), priority.PalOffset()) ==
                        Pal::Result::Success))
                {
                    m_priority = priority;
                }
            }
        }
    }
}

// =====================================================================================================================
// Decodes a priority setting value into a compatible PAL priority/offset pair.
MemoryPriority MemoryPriority::FromSetting(uint32_t value)
{
    static_assert(
        (static_cast<uint32_t>(Pal::GpuMemPriority::Unused)      == 0) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::VeryLow)     == 1) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Low)         == 2) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Normal)      == 3) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::High)        == 4) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::VeryHigh)    == 5) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Count)       == 6) &&
        (static_cast<uint32_t>(Pal::GpuMemPriorityOffset::Count) == 8),
        "PAL GpuMemPriority or GpuMemPriorityOffset values changed.  Update the panel setting description in "
        "settings.cfg for MemoryPriorityDefault");

    MemoryPriority priority = {};

    priority.priority = (value / 16);
    priority.offset   = (value % 16);

    return priority;
}

// =====================================================================================================================
// Provide the PalMemory according to the combination of resourceIndex and memoryIndex
Pal::IGpuMemory* Memory::PalMemory(uint32_t resourceIndex, uint32_t memoryIndex)
{
    // if it is not m_multiInstance, each PalMemory in peer device is imported from m_primaryDeviceIndex.
    // We could always return the PalMemory with memory index m_primaryDeviceIndex.
    uint32_t index = m_multiInstance ? memoryIndex : m_primaryDeviceIndex;

    if (m_pPalMemory[resourceIndex][index] == nullptr)
    {
        // Instantiate the required PalMemory.
        Pal::IGpuMemory* pBaseMemory = nullptr;
        if (m_multiInstance)
        {
            // we need to import the memory from [memoryIndex][memoryIndex]
            VK_ASSERT(m_pPalMemory[index][index] != nullptr);
            pBaseMemory = m_pPalMemory[index][index];
        }
        else
        {
            // we need to import the memory from [m_primaryDeviceIndex][m_primaryDeviceIndex]
            VK_ASSERT(m_pPalMemory[m_primaryDeviceIndex][m_primaryDeviceIndex] != nullptr);
            pBaseMemory = m_pPalMemory[m_primaryDeviceIndex][m_primaryDeviceIndex];
        }

        Pal::PeerGpuMemoryOpenInfo peerMem   = {};
        Pal::GpuMemoryOpenInfo     sharedMem = {};

        Pal::Result palResult = Pal::Result::Success;

        // Call OpenSharedGpuMemory to construct Pal::GpuMemory for memory in remote heap.
        // Call OpenPeerGpuMemory to construct Pal::GpuMemory for memory in peer device's local heap.
        const bool openSharedMemory = (pBaseMemory->Desc().preferredHeap == Pal::GpuHeap::GpuHeapGartUswc) ||
                                      (pBaseMemory->Desc().preferredHeap == Pal::GpuHeap::GpuHeapGartCacheable);

        Pal::GpuMemoryCreateInfo createInfo = {};
        size_t gpuMemorySize = 0;
        if (openSharedMemory)
        {
            sharedMem.pSharedMem = pBaseMemory;
            gpuMemorySize        =  m_pDevice->PalDevice(resourceIndex)->GetSharedGpuMemorySize(sharedMem, &palResult);
        }
        else
        {
            peerMem.pOriginalMem = pBaseMemory;
            gpuMemorySize        = m_pDevice->PalDevice(resourceIndex)->GetPeerGpuMemorySize(peerMem, &palResult);
        }

        void* pPalMemory = static_cast<uint8_t*>(m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->AllocMem(
                                        gpuMemorySize,
                                        VK_DEFAULT_MEM_ALIGN,
                                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        VK_ASSERT(pPalMemory != nullptr);

        Pal::IDevice* pPalDevice = m_pDevice->PalDevice(resourceIndex);

        if (openSharedMemory)
        {
            palResult = pPalDevice->OpenSharedGpuMemory(sharedMem, pPalMemory, &m_pPalMemory[resourceIndex][index]);
        }
        else
        {
            palResult = pPalDevice->OpenPeerGpuMemory(peerMem, pPalMemory, &m_pPalMemory[resourceIndex][index]);
        }

        if (palResult == Pal::Result::Success)
        {
            // Add the GPU memory object to the residency list
            palResult =  m_pDevice->AddMemReference(pPalDevice, m_pPalMemory[resourceIndex][index]);

            if (palResult != Pal::Result::Success)
            {
                m_pPalMemory[resourceIndex][index]->Destroy();
                m_pPalMemory[resourceIndex][index] = nullptr;
            }
        }
        else
        {
            m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->FreeMem(pPalMemory);
        }
    }

    VK_ASSERT(m_pPalMemory[resourceIndex][index] != nullptr);

    return m_pPalMemory[resourceIndex][index];
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    const VkAllocationCallbacks*                pAllocator)
{
    if (memory != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Memory::ObjectFromHandle(memory)->Free(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize                                offset,
    VkDeviceSize                                size,
    VkMemoryMapFlags                            flags,
    void**                                      ppData)
{
    return Memory::ObjectFromHandle(memory)->Map(flags, offset, size, ppData);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory)
{
    Memory::ObjectFromHandle(memory)->Unmap();
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges)
{
    Util::FlushCpuWrites();

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges)
{
    Util::FlushCpuWrites();

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceMemoryCommitment(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize*                               pCommittedMemoryInBytes)
{
    Memory::ObjectFromHandle(memory)->GetCommitment(pCommittedMemoryInBytes);
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdKHR(
    VkDevice                                device,
    const VkMemoryGetFdInfoKHR*             pGetFdInfo,
    int*                                    pFd)
{
    VK_ASSERT(pGetFdInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);

    *pFd = Memory::ObjectFromHandle(pGetFdInfo->memory)->GetShareHandle(pGetFdInfo->handleType);

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
    VkDevice                                device,
    VkExternalMemoryHandleTypeFlagBits      handleType,
    int                                     fd,
    VkMemoryFdPropertiesKHR*                pMemoryFdProperties)
{
    return VK_SUCCESS;
}

} // namespace entry

} // namespace vk
