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
 * @file  vk_memory.cpp
 * @brief Contains implementation of Vulkan memory objects, representing GPU memory allocations.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_image.h"
#include "include/vk_memory.h"
#include "include/vk_utils.h"

#include "palSysMemory.h"
#include "palEventDefs.h"
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

    VK_ASSERT(pDevice != nullptr);
    VK_ASSERT(pAllocInfo != nullptr);
    VK_ASSERT(pMemoryHandle != nullptr);

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
    bool isCaptureReplay            = false;

    // If not 0, use this address as the VA address
    uint64_t baseReplayAddress      = 0;

    Pal::GpuMemoryExportInfo exportInfo = {};

    // Copy Vulkan API allocation info to local PAL version
    Pal::GpuMemoryCreateInfo createInfo = {};
    createInfo.heapAccess = Pal::GpuHeapAccess::GpuHeapAccessExplicit;

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    // Assign default priority based on panel setting (this may get elevated later by memory binds)
    MemoryPriority priority = MemoryPriority::FromSetting(settings.memoryPriorityDefault);

    Image*  pBoundImage       = nullptr;
    VkImage  dedicatedImage   = VK_NULL_HANDLE;
    VkBuffer dedicatedBuffer  = VK_NULL_HANDLE;

    VK_ASSERT(pAllocInfo->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

    createInfo.size = pAllocInfo->allocationSize;
#if defined(__unix__)
    createInfo.flags.initializeToZero = settings.initializeVramToZero;
#endif

    // Calculate the required base address alignment for the given memory type.  These alignments are
    // roughly worst-case alignments required by images that may be hosted within this memory object.
    // The base address alignment of the memory object is large enough to cover the base address
    // requirements of most images, and images add internal padding for the most extreme alignment
    // requirements.
    if (createInfo.size != 0)
    {
        createInfo.alignment = pDevice->GetMemoryBaseAddrAlignment(1UL << pAllocInfo->memoryTypeIndex);
    }

    createInfo.heapCount = 1;
    createInfo.heaps[0] = pDevice->GetPalHeapFromVkTypeIndex(pAllocInfo->memoryTypeIndex);

    if (pDevice->OverallocationRequestedForPalHeap(createInfo.heaps[0]))
    {
        createInfo.heaps[createInfo.heapCount++] = Pal::GpuHeapGartUswc;

        if (createInfo.heaps[0] != Pal::GpuHeapLocal)
        {
            createInfo.heaps[createInfo.heapCount++] = Pal::GpuHeapLocal;
        }
    }

    if (pDevice->NumPalDevices() > 1)
    {
        const uint32_t heapIndex = memoryProperties.memoryTypes[pAllocInfo->memoryTypeIndex].heapIndex;
        multiInstanceHeap = (memoryProperties.memoryHeaps[heapIndex].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) != 0;

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
    else if ((((settings.overrideHeapChoiceToLocal & OverrideChoiceForGartUswc) &&
               (createInfo.heaps[0] == Pal::GpuHeapGartUswc)) ||
              ((settings.overrideHeapChoiceToLocal & OverrideChoiceForGartCacheable) &&
               (createInfo.heaps[0] == Pal::GpuHeapGartCacheable))) &&
             pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsOverrideHeapChoiceToLocalWithinBudget(createInfo.size))
    {
        // When this setting is active (not supported by MGPU), prefer local visible before the requested heap until
        // the allowable budget for it is reached. OverallocationRequestedForPalHeap's choice may be updated here.
        createInfo.heaps[1] = createInfo.heaps[0];
        createInfo.heaps[0] = Pal::GpuHeapLocal;
    }

    if (settings.overrideHeapGartCacheableToUswc && (createInfo.heaps[0] == Pal::GpuHeapGartCacheable))
    {
        createInfo.heaps[0] = Pal::GpuHeapGartUswc;
    }

    VkMemoryPropertyFlags propertyFlags = memoryProperties.memoryTypes[pAllocInfo->memoryTypeIndex].propertyFlags;

    if ((propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) &&
        pDevice->GetEnabledFeatures().deviceCoherentMemory)
    {
        createInfo.flags.gl2Uncached = 1;
    }

    if ((propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
    {
        createInfo.flags.cpuInvisible = 1;
    }

    if ((propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0)
    {
        createInfo.flags.tmzProtected = 1;
    }

    createInfo.flags.globalGpuVa = pDevice->IsGlobalGpuVaEnabled();

    const void* pNext = pAllocInfo->pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<int32>(pHeader->sType))
        {
#if defined(__unix__)
            case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
            {
                const auto* pExtInfo = reinterpret_cast<const VkImportMemoryFdInfoKHR *>(pHeader);
                VK_ASSERT(pExtInfo->handleType &
                    (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT   |
                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));
                handle = pExtInfo->fd;
                isExternal = true;
            }
            break;
#endif
            case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
            {
                const auto* pExtInfo = reinterpret_cast<const VkExportMemoryAllocateInfo *>(pHeader);
#if defined(__unix__)
                    VK_ASSERT(pExtInfo->handleTypes &
                           (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT      |
                            VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT
                               ));
#endif
                    createInfo.flags.interprocess = 1;
                    createInfo.flags.explicitSync = 1;
                    // Todo: we'd better to pass in the handleTypes to the Pal as well.
                    // The supported handleType should also be provided by Pal as Device Capabilities.
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
            {
                const auto * pExtInfo = reinterpret_cast<const VkMemoryAllocateFlagsInfo *>(pHeader);

                if ((pExtInfo->flags & VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT) != 0)
                {
                    VK_ASSERT(pExtInfo->deviceMask != 0);
                    VK_ASSERT((pDevice->GetPalDeviceMask() & pExtInfo->deviceMask) ==
                        pExtInfo->deviceMask);

                    allocationMask = pExtInfo->deviceMask;
                }

                // Test if capture replay has been specified for the memory allocation
                if (pExtInfo->flags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT)
                {
                    createInfo.vaRange = Pal::VaRange::CaptureReplay;
                }
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
            {
                const auto* pExtInfo = reinterpret_cast<const VkMemoryDedicatedAllocateInfo *>(pHeader);
                if (pExtInfo->image != VK_NULL_HANDLE)
                {
                    pBoundImage       = Image::ObjectFromHandle(pExtInfo->image);
                    createInfo.pImage = pBoundImage->PalImage(DefaultDeviceIndex);

                    VkMemoryRequirements reqs = pBoundImage->GetMemoryRequirements();
                    VK_ASSERT(pAllocInfo->allocationSize >= reqs.size);
                    createInfo.alignment = reqs.alignment;
                }
                dedicatedImage  = pExtInfo->image;
                dedicatedBuffer = pExtInfo->buffer;
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT:
            {
                const auto* pExtInfo = reinterpret_cast<const VkMemoryPriorityAllocateInfoEXT *>(pHeader);

                priority = MemoryPriority::FromVkMemoryPriority(pExtInfo->priority);
            }
            break;

            case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO:
            {
                const auto* pExtInfo = reinterpret_cast<const VkMemoryOpaqueCaptureAddressAllocateInfo *>(pHeader);

                VkDeviceAddress baseVaAddress = pExtInfo->opaqueCaptureAddress;
                if (baseVaAddress != 0)
                {
                    // For Replay Specify VA Range and Base Address
                    createInfo.replayVirtAddr = baseVaAddress;
                    createInfo.vaRange        = Pal::VaRange::CaptureReplay;
                }
            }
            break;

            case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:
            {
                VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_EXTERNAL_MEMORY_HOST));
                const auto* pExtInfo = reinterpret_cast<const VkImportMemoryHostPointerInfoEXT*>(pNext);

                VK_ASSERT(pExtInfo->handleType &
                    (VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT |
                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT));

                if (pExtInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT)
                {
                    isHostMappedForeign = true;
                }

                pPinnedHostPtr = pExtInfo->pHostPointer;
            }
            break;

            default:
                // Skip any unknown extension structures
                break;
        }

        pNext = pHeader->pNext;
    }

    // For the descriptor table VA range for descriptor buffers
    if (pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskForDescriptorBuffers() &
        (1 << pAllocInfo->memoryTypeIndex))
    {
        createInfo.vaRange = Pal::VaRange::DescriptorTable;
    }

    // Check for OOM before actually allocating to avoid overhead. Do not account for the memory allocation yet
    // since the commitment size can still increase
    if ((vkResult == VK_SUCCESS) &&
        (pDevice->IsAllocationSizeTrackingEnabled()) &&
        ((createInfo.heaps[0] == Pal::GpuHeap::GpuHeapInvisible) ||
         (createInfo.heaps[0] == Pal::GpuHeap::GpuHeapLocal)))
    {
        vkResult = pDevice->TryIncreaseAllocatedMemorySize(createInfo.size, allocationMask, createInfo.heaps[0]);
    }

    if (vkResult == VK_SUCCESS)
    {
        if (isExternal
            )
        {
            ImportMemoryInfo importInfo = {};
            importInfo.handle           = handle;
            importInfo.isNtHandle       = sharedViaNtHandle;

            if (pBoundImage != nullptr)
            {
                vkResult = OpenExternalSharedImage(pDevice, pBoundImage, importInfo, &pMemory);
            }
            else
            {
                vkResult = OpenExternalMemory(pDevice, importInfo, &pMemory);
            }
        }
        else
        {
            createInfo.priority       = priority.PalPriority();
            createInfo.priorityOffset = priority.PalOffset();

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

    const Device::DeviceFeatures& deviceFeatures = pDevice->GetEnabledFeatures();

    if (vkResult == VK_SUCCESS)
    {
        // Account for committed size in logical device. The destructor will decrease the counter accordingly.
        pDevice->IncreaseAllocatedMemorySize(pMemory->m_size, allocationMask, pMemory->m_heap0);

        // Notify the memory object that it is allocated so that the destructor can decrease the allocated
        // memory size accordingly.
        pMemory->MarkAllocatedMemory(allocationMask);

        *pMemoryHandle = Memory::HandleFromObject(pMemory);

        Pal::ResourceDescriptionHeap desc = {};
        desc.size             = createInfo.size;
        desc.alignment        = createInfo.alignment;
        desc.preferredGpuHeap = createInfo.heaps[0];
        desc.flags            = 0;

        Pal::ResourceCreateEventData data = {};
        data.type              = Pal::ResourceType::Heap;
        data.pObj              = pMemory;
        data.pResourceDescData = &desc;
        data.resourceDescSize  = sizeof(Pal::ResourceDescriptionHeap);

        pDevice->VkInstance()->PalPlatform()->LogEvent(
            Pal::PalEvent::GpuMemoryResourceCreate,
            &data,
            sizeof(Pal::ResourceCreateEventData));

        // @NOTE - This only handles the single GPU case currently.  MGPU is not supported by RMV v1
        Pal::IGpuMemory* pPalGpuMem = pMemory->PalMemory(DefaultDeviceIndex);

        if (pPalGpuMem != nullptr)
        {
            if (deviceFeatures.gpuMemoryEventHandler)
            {
                pDevice->VkInstance()->GetGpuMemoryEventHandler()->VulkanAllocateEvent(
                    pDevice,
                    pPalGpuMem,
                    Memory::IntValueFromHandle(*pMemoryHandle),
                    VK_OBJECT_TYPE_DEVICE_MEMORY,
                    pAllocInfo->memoryTypeIndex,
                    false
                );
            }

            Pal::GpuMemoryResourceBindEventData bindData = {};
            bindData.pObj               = pMemory;
            bindData.pGpuMemory         = pPalGpuMem;
            bindData.requiredGpuMemSize = pMemory->m_size;
            bindData.offset             = 0;

            pDevice->VkInstance()->PalPlatform()->LogEvent(
                Pal::PalEvent::GpuMemoryResourceBind,
                &bindData,
                sizeof(Pal::GpuMemoryResourceBindEventData));
        }

        // When share a dedicated image, metadata(width/height/mips/...) info is necessary in handle,
        // so driver calls bindMemory here to update metadata at allocation time.
        // For dedicated buffer, only base address and total size needed to be filled in handle for sharing,
        // so we don't need to update buffer handle's metadata.
        if (dedicatedImage != VK_NULL_HANDLE)
        {
            VkDeviceSize memOffset = 0;

#if defined(__unix__)
            if ((pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.hasModifier != 0) &&
                (pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierPlaneCount > 0))
            {
                memOffset =
                    pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierMemoryPlaneOffset[0];
            }
#endif
            pBoundImage->BindMemory(pDevice, *pMemoryHandle, memOffset, 0, nullptr, 0, nullptr);
        }
    }
    else
    {
        if (deviceFeatures.deviceMemoryReport)
        {
            pDevice->VkInstance()->GetGpuMemoryEventHandler()->VulkanAllocationFailedEvent(
                pDevice,
                pAllocInfo->allocationSize,
                VK_OBJECT_TYPE_DEVICE_MEMORY,
                pAllocInfo->memoryTypeIndex);
        }
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

    Pal::GpuMemoryCreateInfo localCreateInfo = createInfo;

    localCreateInfo.flags.globalGpuVa = pDevice->IsGlobalGpuVaEnabled();

    GetPrimaryDeviceIndex(pDevice->NumPalDevices(), allocationMask, &primaryIndex, &multiInstance);

    Pal::Result palResult;
    VkResult    vkResult = VK_SUCCESS;

    VK_ASSERT(ppMemory != nullptr);

    if (localCreateInfo.size != 0)
    {
        gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemorySize(localCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        const size_t apiSize = sizeof(Memory);
        const size_t palSize = gpuMemorySize * pDevice->NumPalDevices();

        // Allocate enough for the PAL memory object and our own dispatchable memory
        pSystemMem = static_cast<uint8_t*>(
            pDevice->AllocApiObject(
                pAllocator,
                apiSize + palSize));

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

                    VK_ASSERT(localCreateInfo.heapAccess == Pal::GpuHeapAccess::GpuHeapAccessExplicit);

                    // Other GPU memory objects use the same GPU VA reserved by the first GPU memory object.
                    if ((localCreateInfo.flags.globalGpuVa == 1) &&
                        (deviceIdx != primaryIndex))
                    {
                        localCreateInfo.flags.useReservedGpuVa = 1;
                        localCreateInfo.pReservedGpuVaOwner    = pGpuMemory[primaryIndex];
                    }

                    // Allocate the PAL memory object
                    palResult = pPalDevice->CreateGpuMemory(
                        localCreateInfo, Util::VoidPtrInc(pSystemMem, palMemOffset), &pGpuMemory[deviceIdx]);

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
                                                                localCreateInfo,
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

                pDevice->FreeApiObject(pAllocator, pSystemMem);

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
            pDevice->AllocApiObject(
                pAllocator,
                sizeof(Memory)));

        if (pSystemMem != nullptr)
        {
            // Initialize dispatchable memory object and return to application
            Pal::IGpuMemory* pDummyPalGpuMemory[MaxPalDevices] = {};
            *ppMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice,
                                                            pDummyPalGpuMemory,
                                                            0,
                                                            localCreateInfo,
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

    pinnedInfo.size      = static_cast<size_t>(createInfo.size);
    pinnedInfo.pSysMem   = pPinnedHostPtr;
    pinnedInfo.vaRange   = createInfo.vaRange;
    pinnedInfo.alignment = createInfo.alignment;
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
            pDevice->AllocApiObject(
                pAllocator,
                apiSize + palSize));

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

                pDevice->FreeApiObject(pAllocator, pSystemMem);

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

    palMemCreateInfo.heapAccess        = Pal::GpuHeapAccess::GpuHeapAccessExplicit;
    palMemCreateInfo.flags.globalGpuVa = pDevice->IsGlobalGpuVaEnabled();

    Pal::ExternalImageOpenInfo palOpenInfo = {};

    palOpenInfo.extent          = pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().extent;
    palOpenInfo.swizzledFormat  = VkToPalFormat(pBoundImage->GetFormat(), pDevice->GetRuntimeSettings());
    palOpenInfo.usage           = VkToPalImageUsageFlags(pBoundImage->GetImageUsage(),
                                                         1,
                                                         (VkImageUsageFlags)(0),
                                                         (VkImageUsageFlags)(0));

    palOpenInfo.resourceInfo.hExternalResource        = importInfo.handle;
    palOpenInfo.resourceInfo.flags.ntHandle           = importInfo.isNtHandle;
#if defined(__unix__)
    palOpenInfo.resourceInfo.handleType               = Pal::HandleType::DmaBufFd;

    if (pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.hasModifier != 0)
    {
        palOpenInfo.flags.hasModifier  = 1;
        palOpenInfo.modifier           = pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifier;
        palOpenInfo.modifierPlaneCount =
            pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierPlaneCount;
        palOpenInfo.gpuMemOffset       =
            pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierMemoryPlaneOffset[0];

        if (palOpenInfo.modifierPlaneCount > 1)
        {
            if (palOpenInfo.modifierPlaneCount == 2)
            {
                palOpenInfo.dccOffset        =
                    pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierMemoryPlaneOffset[1];
            }
            else if (palOpenInfo.modifierPlaneCount == 3)
            {
                palOpenInfo.dccOffset        =
                    pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierMemoryPlaneOffset[2];
                palOpenInfo.displayDccOffset =
                    pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifierMemoryPlaneOffset[1];
            }
        }
    }
#endif
    palOpenInfo.flags.perSubresInit = pBoundImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.perSubresInit;
    Pal::Result palResult = Pal::Result::Success;
    const bool openedViaName = (importInfo.handle == 0);
    if (openedViaName)
    {
    }

    palResult = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedImageSizes(
        palOpenInfo,
        &palImgSize,
        &palMemSize,
        &palImgCreateInfo);

    const size_t totalSize = palImgSize + sizeof(Memory) + palMemSize;

    void* pMemMemory = static_cast<uint8_t*>(pDevice->AllocApiObject(
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->GetAllocCallbacks(),
        totalSize));

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
            if (pExternalImage->GetImageCreateInfo().flags.optimalShareable == 1)
            {
                // Vulkan informs other Pal-clients that it is going to read and write shared metadata.
                pExternalImage->SetOptimalSharingLevel(Pal::MetadataSharingLevel::FullOptimal);
            }

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
            pDevice->FreeApiObject(
                pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->GetAllocCallbacks(),
                pMemMemory);
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
    m_pExternalPalImage(pExternalImage),
    m_sharedGpuMemoryHandle(sharedGpuMemoryHandle),
    m_priority(info.priority, info.priorityOffset),
    m_sizeAccountedForDeviceMask(0),
    m_primaryDeviceIndex(primaryIndex)
{
    m_size = info.size;
    m_heap0 = info.heaps[0];

    m_flags.u32All = 0;
    m_flags.sharedViaNtHandle = info.flags.sharedViaNtHandle;
    m_flags.multiInstance = multiInstance ? 1 : 0;

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
    m_pExternalPalImage(nullptr),
    m_sharedGpuMemoryHandle(0),
    m_sizeAccountedForDeviceMask(0),
    m_primaryDeviceIndex(primaryIndex)
{
    // PAL info is not available for memory objects allocated for presentable images
    m_size = 0;
    m_heap0 = Pal::GpuHeap::GpuHeapLocal;

    m_flags.u32All = 0;
    m_flags.multiInstance = multiInstance ? 1 : 0;

    Init(ppPalMemory);
}

// =====================================================================================================================
// Free a GPU memory object - also destroys the API memory object
void Memory::Free(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    if (m_pExternalPalImage != nullptr)
    {
        m_pExternalPalImage->Destroy();
        m_pExternalPalImage = nullptr;
    }

    Pal::ResourceDestroyEventData data = {};
    data.pObj = this;

    pDevice->VkInstance()->PalPlatform()->LogEvent(
        Pal::PalEvent::GpuMemoryResourceDestroy,
        &data,
        sizeof(Pal::ResourceDestroyEventData));

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

    // Decrease the allocation size
    if (m_sizeAccountedForDeviceMask != 0)
    {
        m_pDevice->DecreaseAllocatedMemorySize(m_size, m_sizeAccountedForDeviceMask, m_heap0);
    }

    // Call destructor
    Util::Destructor(this);

    // Free outer container
    pDevice->FreeApiObject(pAllocator, this);
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

    createInfo.flags.globalGpuVa = pDevice->IsGlobalGpuVaEnabled();
    createInfo.heapAccess        = Pal::GpuHeapAccess::GpuHeapAccessExplicit;

    VK_ASSERT(pDevice  != nullptr);
    VK_ASSERT(ppMemory != nullptr);

    const uint32_t allocationMask = (1 << DefaultMemoryInstanceIdx);
    const bool openedViaName      = (importInfo.handle == 0);

    if (openedViaName)
    {
    }
    else
    {
        openInfo.resourceInfo.hExternalResource = importInfo.handle;
    }

    openInfo.resourceInfo.flags.ntHandle           = importInfo.isNtHandle;
    // Get CPU memory requirements for PAL
    gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedGpuMemorySize(&palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Allocate enough for the PAL memory object and our own dispatchable memory
    pSystemMem = static_cast<uint8_t*>(pDevice->AllocApiObject(
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->GetAllocCallbacks(),
        gpuMemorySize + sizeof(Memory)));

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
        pDevice->FreeApiObject(
            pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->GetAllocCallbacks(),
            pSystemMem);
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Returns the external shared handle of the memory object.
Pal::OsExternalHandle Memory::GetShareHandle(
    VkExternalMemoryHandleTypeFlagBits handleType)
{
#if DEBUG
    bool condition = m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_EXTERNAL_MEMORY_FD);

    uint32_t enabledAPIVersion;
    enabledAPIVersion = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion();
    condition |= enabledAPIVersion >= VK_MAKE_API_VERSION( 0, 1, 1, 0);
    VK_ASSERT(condition);
#endif

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
    if (m_flags.multiInstance == 0)
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

    VK_ASSERT(m_flags.multiInstance == 0);

    palResult = PalMemory(m_primaryDeviceIndex)->Unmap();
    VK_ASSERT(palResult == Pal::Result::Success);
}

// =====================================================================================================================
// Returns the actual number of bytes that are currently committed to this memory object
VkResult Memory::GetCommitment(
    VkDeviceSize* pCommittedMemoryInBytes)
{
    VK_ASSERT(pCommittedMemoryInBytes != nullptr);

    // We never allocate memory lazily, so just return the size of the memory object
    *pCommittedMemoryInBytes = m_size;

    return VK_SUCCESS;
}

// =====================================================================================================================
// This function increases the priority of this memory's allocation to be at least that of the given priority.  This
// function may be called e.g. when this memory is bound to a high-priority VkImage.
void Memory::ElevatePriority(
    MemoryPriority priority)
{
    // Update PAL memory object's priority using a double-checked lock if the current priority is lower than
    // the new given priority.
    if (m_priority < priority)
    {
        SetPriority(priority, true);
    }
}

// =====================================================================================================================
// This function set new priority of this memory's allocation.
void Memory::SetPriority(
    const MemoryPriority    priority,
    const bool              mustBeLower)
{
    Util::MutexAuto lock(m_pDevice->GetMemoryMutex());
    if (((mustBeLower == false) && (m_priority != priority)) ||
        ((mustBeLower == true)  && (m_priority < priority)))
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

// =====================================================================================================================
// Decodes a priority setting value into a compatible PAL priority/offset pair.
MemoryPriority MemoryPriority::FromSetting(
    uint32_t value)
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
// Convert VkMemoryPriority(from VkMemoryPriorityAllocateInfoEXT) value to a compatible PAL priority/offset pair.
MemoryPriority MemoryPriority::FromVkMemoryPriority(
    float value)
{
    static_assert(
        (static_cast<uint32_t>(Pal::GpuMemPriority::Unused) == 0) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::VeryLow) == 1) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Low) == 2) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Normal) == 3) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::High) == 4) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::VeryHigh) == 5) &&
        (static_cast<uint32_t>(Pal::GpuMemPriority::Count) == 6) &&
        (static_cast<uint32_t>(Pal::GpuMemPriorityOffset::Count) == 8),
        "PAL GpuMemPriority or GpuMemPriorityOffset values changed. Consider to update strategy to convert"
        "VkMemoryPriority to compatible PAL priority/offset pair");

    // From Vulkan Spec, 0.0 <= value <= 1.0, and the granularity of the priorities is implementation-dependent.
    // One thing Spec forced is that if VkMemoryPriority not specified as default behavior, it is as if the
    // priority value is 0.5. Our strategy is that map 0.5 to GpuMemPriority::Normal-GpuMemPriorityOffset::Offset0,
    // which is consistent to MemoryPriorityDefault. We adopts GpuMemPriority::VeryLow, GpuMemPriority::Low,
    // GpuMemPriority::Normal, GpuMemPriority::High, 4 priority grades, each of which contains 8 steps of offests.
    // This maps [0.0-1.0) to totally 32 steps. Finally, 1.0 maps to GpuMemPriority::VeryHigh.
    VK_ASSERT((value >= 0.0f) && (value <= 1.0f));
    static constexpr uint32_t TotalMemoryPrioritySteps = 32;
    uint32_t uintValue = static_cast<uint32_t>(value * TotalMemoryPrioritySteps);

    MemoryPriority priority = {};
    priority.priority = ((uintValue / 8) + 1);
    priority.offset   = (uintValue % 8);
    return priority;
}

// =====================================================================================================================
// Provide the PalMemory according to the combination of resourceIndex and memoryIndex
Pal::IGpuMemory* Memory::PalMemory(
    uint32_t resourceIndex,
    uint32_t memoryIndex)
{
    // if it is not m_flags.multiInstance, each PalMemory in peer device is imported from m_primaryDeviceIndex.
    // We could always return the PalMemory with memory index m_primaryDeviceIndex.
    uint32_t index = m_flags.multiInstance ? memoryIndex : m_primaryDeviceIndex;

    if (m_pPalMemory[resourceIndex][index] == nullptr)
    {
        // Instantiate the required PalMemory.
        Pal::IGpuMemory* pBaseMemory = nullptr;
        if (m_flags.multiInstance)
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
        const bool openSharedMemory = (pBaseMemory->Desc().heapCount > 0) &&
                                      ((pBaseMemory->Desc().heaps[0] == Pal::GpuHeap::GpuHeapGartUswc) ||
                                       (pBaseMemory->Desc().heaps[0] == Pal::GpuHeap::GpuHeapGartCacheable));

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
        Device* pDevice = ApiDevice::ObjectFromHandle(device);
        Memory* pMemory = Memory::ObjectFromHandle(memory);

        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        pMemory->Free(pDevice, pAllocCB);
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
VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory2KHR(
    VkDevice                                    device,
    const                                       VkMemoryMapInfoKHR* pMemoryMapInfo,
    void**                                      ppData)
{
    return Memory::ObjectFromHandle(pMemoryMapInfo->memory)->Map(
            pMemoryMapInfo->flags,
            pMemoryMapInfo->offset,
            pMemoryMapInfo->size,
            ppData);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkUnmapMemory2KHR(
    VkDevice                                    device,
    const VkMemoryUnmapInfoKHR*                 pMemoryUnmapInfo)
{
    Memory::ObjectFromHandle(pMemoryUnmapInfo->memory)->Unmap();

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges)
{
    // All of our host visible memory heaps are coherent.

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges)
{
    // All of our host visible memory heaps are coherent.

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

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdKHR(
    VkDevice                                device,
    const VkMemoryGetFdInfoKHR*             pGetFdInfo,
    int*                                    pFd)
{
    VK_ASSERT(pGetFdInfo->handleType &
        (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT   |
         VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));

    *pFd = Memory::ObjectFromHandle(pGetFdInfo->memory)->GetShareHandle(pGetFdInfo->handleType);

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
    VkDevice                                device,
    VkExternalMemoryHandleTypeFlagBits      handleType,
    int                                     fd,
    VkMemoryFdPropertiesKHR*                pMemoryFdProperties)
{
    VkResult result = VK_SUCCESS;

    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    switch (handleType)
    {
        case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
        {
            pMemoryFdProperties->memoryTypeBits = pDevice->GetMemoryTypeMaskForExternalSharing();
            break;
        }
        default:
        {
            pMemoryFdProperties->memoryTypeBits = 0;
            result = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        }
    }

    return result;
}
#endif

// =====================================================================================================================
VKAPI_ATTR uint64_t VKAPI_CALL vkGetDeviceMemoryOpaqueCaptureAddress(
    VkDevice                                         device,
    const VkDeviceMemoryOpaqueCaptureAddressInfo*    pInfo)
{
    const Memory* pMemory = Memory::ObjectFromHandle(pInfo->memory);

    return pMemory->PalMemory(DefaultDeviceIndex)->Desc().gpuVirtAddr;
}

} // namespace entry

} // namespace vk
