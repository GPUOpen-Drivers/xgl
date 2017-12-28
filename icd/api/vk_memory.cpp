/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/peer_resource.h"

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
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};

    size_t   gpuMemorySize = 0;
    uint8_t *pSystemMem = nullptr;
    Memory*  pMemory    = nullptr;

    Pal::Result palResult;
    VkResult    vkResult   = VK_SUCCESS;
    union
    {
        const VkStructHeader*               pHeader;
        const VkMemoryAllocateInfo*         pInfo;
    };

    VK_ASSERT(pDevice != nullptr);
    VK_ASSERT(pAllocInfo != nullptr);
    VK_ASSERT(pMemoryHandle != nullptr);

    const Pal::DeviceProperties&            palProperties    = pDevice->VkPhysicalDevice()->PalProperties();
    const VkPhysicalDeviceMemoryProperties& memoryProperties = pDevice->VkPhysicalDevice()->GetMemoryProperties();

    // Create a mask to indicate the devices the memory allocations happened on
    bool multiInstanceHeap  = false;
    uint32_t allocationMask = (1 << DefaultDeviceIndex);

    // indicate whether it is a allocation that supposed to be imported.
    Pal::OsExternalHandle handle    = 0;
    bool sharedViaNtHandle          = false;
    bool isExternal                 = false;
    const Pal::gpusize palAlignment = Util::Max(palProperties.gpuMemoryProperties.virtualMemAllocGranularity,
                                                palProperties.gpuMemoryProperties.realMemAllocGranularity);

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
                createInfo.size = Util::Pow2Align(pInfo->allocationSize, palAlignment);

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
                        VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHX) != 0;

                    if (multiInstanceHeap)
                    {
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
                    VK_ASSERT(pImportMemoryFdInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
                    handle = pImportMemoryFdInfo->fd;
                    isExternal = true;
                }
                break;
            case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR:
                {
                const VkExportMemoryAllocateInfoKHR* pExportMemory =
                    reinterpret_cast<const VkExportMemoryAllocateInfoKHR *>(pHeader);
                    VK_ASSERT(pExportMemory->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
                    // Todo: we'd better to pass in the handleTypes to the Pal as well.
                    // The supported handleType should also be provided by Pal as Device Capabilities.
                }
                break;

            case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHX:
            {
                const VkMemoryAllocateFlagsInfoKHX * pMemoryAllocateFlags =
                    reinterpret_cast<const VkMemoryAllocateFlagsInfoKHX *>(pHeader);

                if ((pMemoryAllocateFlags->flags & VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHX) != 0)
                {
                    VK_ASSERT(pMemoryAllocateFlags->deviceMask != 0);
                    VK_ASSERT((pDevice->GetPalDeviceMask() & pMemoryAllocateFlags->deviceMask) ==
                        pMemoryAllocateFlags->deviceMask);

                    allocationMask = pMemoryAllocateFlags->deviceMask;
                }
            }
            break;
            case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR:
            {
                const VkMemoryDedicatedAllocateInfoKHR* pDedicatedInfo =
                    reinterpret_cast<const VkMemoryDedicatedAllocateInfoKHR *>(pHeader);
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
                default:
                    // Skip any unknown extension structures
                    break;
                }
                break;
        }
    }

    if (vkResult == VK_SUCCESS)
    {
        if (isExternal)
        {
            vkResult = OpenExternalMemory(pDevice, handle, sharedViaNtHandle, &pMemory);
        }
        else
        {
            if (createInfo.size != 0)
            {
                // Get CPU memory requirements for PAL
                gpuMemorySize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemorySize(createInfo, &palResult);
                VK_ASSERT(palResult == Pal::Result::Success);

                const size_t apiSize        = sizeof(Memory);
                const size_t palSize        = gpuMemorySize * pDevice->NumPalDevices();
                const size_t peerMemorySize = PeerMemory::GetMemoryRequirements(
                    pDevice, multiInstanceHeap, allocationMask, static_cast<uint32_t>(gpuMemorySize));

                // Allocate enough for the PAL memory object and our own dispatchable memory
                pSystemMem = static_cast<uint8_t*>(
                        pAllocator->pfnAllocation(
                            pAllocator->pUserData,
                            apiSize + palSize + peerMemorySize,
                            VK_DEFAULT_MEM_ALIGN,
                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

                // Check for out of memory
                if (pSystemMem == nullptr)
                {
                    vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }

                if (vkResult == VK_SUCCESS)
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

                            VK_ASSERT(palResult == Pal::Result::Success);

                            // On success...
                            if (palResult == Pal::Result::Success)
                            {
                                // Add the GPU memory object to the residency list
                                pDevice->AddMemReference(pPalDevice, pGpuMemory[deviceIdx]);
                            }
                        }

                        palMemOffset += gpuMemorySize;
                    }

                    if (palResult == Pal::Result::Success)
                    {
                        vkResult = VK_SUCCESS;

                        PeerMemory* pPeerMemory = nullptr;
                        if (peerMemorySize > 0)
                        {
                            VK_ASSERT(multiInstanceHeap);
                            pPeerMemory = VK_PLACEMENT_NEW(Util::VoidPtrInc(pSystemMem, apiSize + palSize)) PeerMemory(
                                pDevice, pGpuMemory, static_cast<uint32_t>(gpuMemorySize));
                        }

                        // Initialize dispatchable memory object and return to application
                        pMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(
                            pDevice, pGpuMemory, pPeerMemory, allocationMask, createInfo);
                    }
                    else if (palResult == Pal::Result::ErrorOutOfGpuMemory)
                    {
                        vkResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
                    }
                    else
                    {
                        VK_ASSERT(palResult == Pal::Result::ErrorOutOfMemory);
                        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                    }

                    if (palResult != Pal::Result::Success)
                    {
                        // Construction of PAL memory object failed.
                        // Free the memory before returning to application.
                        pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);
                    }
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

                if (pSystemMem == nullptr)
                {
                    vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }

                // Initialize dispatchable memory object and return to application
                constexpr Pal::IGpuMemory** pDummyPalGpuMemory = nullptr;

                pMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(
                    pDevice, pDummyPalGpuMemory, nullptr, allocationMask, createInfo);
            }
        }
    }

    if (vkResult == VK_SUCCESS)
    {
        palResult = pMemory->Init();

        if (palResult == Pal::Result::Success)
        {
            vkResult = VK_SUCCESS;
        }
        else if (palResult == Pal::Result::ErrorOutOfGpuMemory)
        {
            vkResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        else
        {
            VK_ASSERT(palResult == Pal::Result::ErrorOutOfMemory);
            vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if ((vkResult != VK_SUCCESS) && (vkResult != VK_ERROR_TOO_MANY_OBJECTS))
    {
        // decrease back the allocation count if allocation failed.
        pDevice->DecreaseAllocationCount();
    }
    else if (vkResult == VK_SUCCESS)
    {
        // Initialize tiny host visible allocations to zero
        const uint32_t NumBytesToZero = 32;

        if ((pAllocInfo->allocationSize < NumBytesToZero) &&
            (createInfo.heaps[0] != Pal::GpuHeapInvisible))
        {
            void*    pData  = nullptr;
            VkResult result = pMemory->Map(0, 0, NumBytesToZero, &pData);

            VK_ASSERT(createInfo.size >= NumBytesToZero);

            if (result == VK_SUCCESS)
            {
                memset(pData, 0, NumBytesToZero);

                pMemory->Unmap();
            }
        }

        // notify the memory object that it is counted so that the destructor can decrease the counter accordingly
        pMemory->SetAllocationCounted();

        *pMemoryHandle = Memory::HandleFromObject(pMemory);
    }

    return vkResult;
}

// =====================================================================================================================
VkResult Memory::OpenExternalSharedImage(
    Device*                      pDevice,
    Image*                       pBoundImage,
    const Pal::OsExternalHandle  handle,
    bool                         isNtHandle,
    Memory**                     ppVkMemory)
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

    palOpenInfo.resourceInfo.hExternalResource = handle;
    palOpenInfo.resourceInfo.flags.ntHandle    = isNtHandle;

    Pal::Result palResult = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedImageSizes(
        palOpenInfo,
        &palImgSize,
        &palMemSize,
        &palImgCreateInfo);

    const size_t totalSize = palImgSize + sizeof(Memory) + palMemSize;

    void* pMemMemory = static_cast<uint8_t*>(pDevice->VkPhysicalDevice()->VkInstance()->AllocMem(
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
            pPalMemory);
    }

    result = PalToVkResult(palResult);

    if (result == VK_SUCCESS)
    {
        // Add the GPU memory object to the residency list
        pDevice->AddMemReference(pDevice->PalDevice(), pPalMemory[DefaultDeviceIndex]);

        const uint32_t allocationMask = (1 << DefaultMemoryInstanceIdx);
        // Initialize dispatchable memory object and return to application
        *ppVkMemory = VK_PLACEMENT_NEW(pMemMemory) Memory(pDevice,
                                                          pPalMemory,
                                                          nullptr,
                                                          allocationMask,
                                                          palMemCreateInfo,
                                                          pExternalImage);
    }

    return result;
}
// =====================================================================================================================
Memory::Memory(
    vk::Device*         pDevice,
    Pal::IGpuMemory**   pPalMemory,
    PeerMemory*         pPeerMemory,
    uint32_t            allocationMask,
    const Pal::GpuMemoryCreateInfo& info,
    Pal::IImage*        pExternalImage)
:
    m_pDevice(pDevice),
    m_pPeerMemory(pPeerMemory),
    m_info(info),
    m_priority(info.priority, info.priorityOffset),
    m_allocationMask(allocationMask),
    m_mirroredAllocationMask(0),
    m_multiInstance(pPeerMemory != nullptr),
    m_allocationCounted(false),
    m_pExternalPalImage(pExternalImage)
{
    memcpy(m_pPalMemory, pPalMemory, sizeof(m_pPalMemory));
}

// =====================================================================================================================
Memory::Memory(
    Device*           pDevice,
    Pal::IGpuMemory** pPalMemory,
    PeerMemory*       pPeerMemory,
    uint32_t          allocationMask)
:
    m_pDevice(pDevice),
    m_pPeerMemory(pPeerMemory),
    m_allocationMask(allocationMask),
    m_mirroredAllocationMask(0),
    m_multiInstance(pPeerMemory != nullptr),
    m_allocationCounted(false),
    m_pExternalPalImage(nullptr)
{
    // PAL info is not available for memory objects allocated for presentable images
    memset(&m_info, 0, sizeof(m_info));

    memcpy(m_pPalMemory, pPalMemory, sizeof(m_pPalMemory));
}

// =====================================================================================================================
// Free a GPU memory object - also destroys the API memory object
VkResult Memory::Free(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    if (m_pPeerMemory != nullptr)
    {
        m_pPeerMemory->Destroy(pDevice);
    }

    if (m_pExternalPalImage != nullptr)
    {
        m_pExternalPalImage->Destroy();
        m_pExternalPalImage = nullptr;
    }

    // Iterate memory objects in reverse order, to ensure that any mirrored memory is destroyed before the parent
    for (int32_t i = m_pDevice->NumPalDevices()-1; i >= 0 ; --i)
    {
        Pal::IGpuMemory* pGpuMemory = m_pPalMemory[i];
        if (pGpuMemory != nullptr)
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(i);
            pDevice->RemoveMemReference(pPalDevice, pGpuMemory);

            // Destroy PAL memory object
            pGpuMemory->Destroy();
        }
    }

    // decrease the allocation count
    if (m_allocationCounted)
    {
        m_pDevice->DecreaseAllocationCount();
    }

    // Call destructor
    Util::Destructor(this);

    // Free outer container
    pAllocator->pfnFree(pAllocator->pUserData, this);

    // Never fail
    return VK_SUCCESS;
}

// =====================================================================================================================
Pal::Result Memory::Init()
{
    Pal::Result palResult = Pal::Result::Success;

    if (m_info.flags.shareable != 0)
    {
        palResult = MirrorSharedAllocation();
    }

    return palResult;
}

// =====================================================================================================================
Pal::Result Memory::MirrorSharedAllocation()
{
    Pal::Result palResult = Pal::Result::Success;

    // We mirror only the first memory instance.
    VK_ASSERT((m_allocationMask & (1 << DefaultMemoryInstanceIdx)) != 0);
    VK_ASSERT(m_mirroredAllocationMask == 0);

    const size_t gpuMemorySize = m_pDevice->PalDevice()->GetGpuMemorySize(m_info, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    void* pPalMemory = Util::VoidPtrInc(static_cast<void*>(this + 1),
                                        gpuMemorySize * Util::CountSetBits(m_allocationMask));

    for (uint32_t deviceIdx = 1;
        (deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        const uint32_t deviceMask = 1 << deviceIdx;

        // Only allocate mirrored memory for devices which do not have their own allocations.
        if ((m_allocationMask & deviceMask) == 0)
        {
            VK_ASSERT(m_pPalMemory[deviceIdx] == nullptr);

            Pal::GpuMemoryOpenInfo shareMem = {};
            shareMem.pSharedMem = m_pPalMemory[DefaultMemoryInstanceIdx];

            Pal::IDevice* pPalDevice = m_pDevice->PalDevice(deviceIdx);
            palResult = pPalDevice->OpenSharedGpuMemory(shareMem, pPalMemory, &m_pPalMemory[deviceIdx]);

            if (palResult == Pal::Result::Success)
            {
                // Add the GPU memory object to the residency list
                m_pDevice->AddMemReference(pPalDevice, m_pPalMemory[deviceIdx]);

                m_mirroredAllocationMask |= deviceMask;
                pPalMemory = Util::VoidPtrInc(pPalMemory, gpuMemorySize);
            }
        }
    }

    return palResult;
}

// =====================================================================================================================
// Opens a POSIX external shared handle and creates a memory object corresponding to it.
VkResult Memory::OpenExternalMemory(
    Device*                         pDevice,
    const Pal::OsExternalHandle     handle,
    bool                            isNtHandle,
    Memory**                        pMemory)
{
    Pal::ExternalGpuMemoryOpenInfo openInfo = {};
    Pal::GpuMemoryCreateInfo createInfo = {};
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};
    Pal::Result palResult;
    size_t gpuMemorySize;
    uint8_t *pSystemMem;
    VkResult vkResult;

    VK_ASSERT(pDevice != nullptr);
    VK_ASSERT(pMemory != nullptr);

    const uint32_t allocationMask = (1 << DefaultMemoryInstanceIdx);

    openInfo.resourceInfo.hExternalResource = handle;

    openInfo.resourceInfo.flags.ntHandle = isNtHandle;

    // Get CPU memory requirements for PAL
    gpuMemorySize = pDevice->PalDevice()->GetExternalSharedGpuMemorySize(&palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Allocate enough for the PAL memory object and our own dispatchable memory
    pSystemMem = static_cast<uint8_t*>(pDevice->VkPhysicalDevice()->VkInstance()->AllocMem(
        gpuMemorySize + sizeof(Memory),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

    // Check for out of memory
    if (pSystemMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Allocate the PAL memory object
    palResult = pDevice->PalDevice()->OpenExternalSharedGpuMemory(openInfo,
                                                                  pSystemMem + sizeof(Memory),
                                                                  &createInfo,
                                                                  &pGpuMemory[DefaultDeviceIndex]);
    vkResult = PalToVkResult(palResult);

    // On success...
    if (vkResult == VK_SUCCESS)
    {
        // Add the GPU memory object to the residency list
        pDevice->AddMemReference(pDevice->PalDevice(), pGpuMemory[DefaultDeviceIndex]);

        // Initialize dispatchable memory object and return to application
        *pMemory = VK_PLACEMENT_NEW(pSystemMem) Memory(pDevice, pGpuMemory, nullptr, allocationMask, createInfo);
    }
    else
    {
        // Construction of PAL memory object failed. Free the memory before returning to application.
        pDevice->VkPhysicalDevice()->VkInstance()->FreeMem(pSystemMem);
    }

    return vkResult;
}

// =====================================================================================================================
// Returns the external shared handle of the memory object.
Pal::OsExternalHandle Memory::GetShareHandle(VkExternalMemoryHandleTypeFlagBitsKHR handleType)
{
    VK_ASSERT(m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_EXTERNAL_MEMORY_FD) ||
              m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_EXTERNAL_MEMORY_WIN32));

    return PalMemory()->GetSharedExternalHandle();
}

// =====================================================================================================================
// Map GPU memory into client address space. Simply calls through to PAL.
VkResult Memory::Map(
    VkFlags      flags,
    VkDeviceSize offset,
    VkDeviceSize size,
    void**       ppData)
{
    Pal::Result palResult = Pal::Result::Success;

    const uint32_t count = m_multiInstance ? m_pDevice->NumPalDevices() : 1;

    for (uint32_t deviceIdx = 0; (deviceIdx <count) && (palResult == Pal::Result::Success); deviceIdx++)
    {
        if (PalMemory(deviceIdx) != nullptr)
        {
            void* pData;

            palResult = PalMemory(deviceIdx)->Map(&pData);

            if (palResult == Pal::Result::Success)
            {
                ppData[deviceIdx] = Util::VoidPtrInc(pData, static_cast<size_t>(offset));
            }
        }
    }

    return (palResult == Pal::Result::Success) ? VK_SUCCESS : VK_ERROR_MEMORY_MAP_FAILED;
}

// =====================================================================================================================
// Unmap previously mapped memory object. Just calls PAL.
VkResult Memory::Unmap(void)
{
    Pal::Result palResult = Pal::Result::Success;

    const uint32_t count = m_multiInstance ? m_pDevice->NumPalDevices() : 1;

    for (uint32_t deviceIdx = 0; (deviceIdx < count) && (palResult == Pal::Result::Success); deviceIdx++)
    {
        if (m_pPalMemory[deviceIdx] != nullptr)
        {
            palResult = m_pPalMemory[deviceIdx]->Unmap();
            VK_ASSERT(palResult == Pal::Result::Success);
        }
    }

    return PalToVkResult(palResult);
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
    VK_ASSERT(pGetFdInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);

    *pFd = Memory::ObjectFromHandle(pGetFdInfo->memory)->GetShareHandle(pGetFdInfo->handleType);

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
    VkDevice                                device,
    VkExternalMemoryHandleTypeFlagBitsKHR   handleType,
    int                                     fd,
    VkMemoryFdPropertiesKHR*                pMemoryFdProperties)
{
    return VK_SUCCESS;
}

} // namespace entry

} // namespace vk
