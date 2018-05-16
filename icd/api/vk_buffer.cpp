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

#include "include/vk_buffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"
#include "include/peer_resource.h"

#include "palGpuMemory.h"
#include "palDevice.h"
#include "palQueue.h"
#include "palInlineFuncs.h"

namespace vk
{

// =====================================================================================================================
Buffer::Buffer(
    Device*                     pDevice,
    VkBufferCreateFlags         flags,
    VkBufferUsageFlags          usage,
    Pal::IGpuMemory**           pGpuMemory,
    const BufferBarrierPolicy&  barrierPolicy,
    VkDeviceSize                size,
    BufferFlags                 internalFlags)
    :
    m_pMemory(nullptr),
    m_size(size),
    m_memOffset(0),
    m_pDevice(pDevice),
    m_flags(flags),
    m_usage(usage),
    m_barrierPolicy(barrierPolicy)
{
    m_internalFlags.u32All = internalFlags.u32All;

    memset(m_gpuVirtAddr, 0, sizeof(m_gpuVirtAddr));
    memset(m_pGpuMemory, 0, sizeof(m_pGpuMemory));
    test to break the build;
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pGpuMemory[deviceIdx] != nullptr)
        {
            m_pGpuMemory[deviceIdx]  = pGpuMemory[deviceIdx];
            m_gpuVirtAddr[deviceIdx] = pGpuMemory[deviceIdx]->Desc().gpuVirtAddr;
        }
    }
}

// =====================================================================================================================
// Create a new Vulkan Buffer object
VkResult Buffer::Create(
    Device*                         pDevice,
    const VkBufferCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkBuffer*                       pBuffer)
{
    VkDeviceSize     size;
    Instance* const  pInstance = pDevice->VkInstance();
    void*            pMemory   = nullptr;
    Pal::IGpuMemory* pGpuMemory[MaxPalDevices] = {};

    Pal::Result palResult = Pal::Result::Success;

    // We ignore sharing information for buffers, it has no relevance for us currently.
    VK_IGNORE(pCreateInfo->sharingMode);

    size = pCreateInfo->size;
    bool isSparse = (pCreateInfo->flags & SparseEnablingFlags) != 0;

    if (isSparse && (size != 0))
    {
        // We need virtual remapping support for all sparse resources
        VK_ASSERT(pDevice->VkPhysicalDevice()->IsVirtualRemappingSupported());

        // We need support for sparse buffers for sparse buffer residency
        if ((pCreateInfo->flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) != 0)
        {
            VK_ASSERT(pDevice->VkPhysicalDevice()->GetPrtFeatures() & Pal::PrtFeatureBuffer);
        }

        size_t                   apiSize = sizeof(Buffer);
        size_t                   palMemSize;
        Pal::GpuMemoryCreateInfo info = { };

        info.alignment          = pDevice->VkPhysicalDevice()->PalProperties().
                                                                gpuMemoryProperties.virtualMemAllocGranularity;
        info.size               = Util::Pow2Align(size, info.alignment);
        info.flags.u32All       = 0;
        info.flags.virtualAlloc = 1;

        // Virtual resource should return 0 on unmapped read if residencyNonResidentStrict is set.
        if (pDevice->VkPhysicalDevice()->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
        {
            info.virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
        }

        palMemSize = pDevice->PalDevice()->GetGpuMemorySize(info, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        // Allocate enough system memory to also store the VA-only memory object
        pMemory = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            apiSize + (palMemSize * pDevice->NumPalDevices()),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        void* pPalMemory = Util::VoidPtrInc(pMemory, apiSize);

        for (uint32_t deviceIdx = 0;
            (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
            deviceIdx++)
        {
            // Create the VA-only memory object needed for sparse buffer support
            palResult = pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
                info,
                (uint8_t*)pPalMemory,
                &pGpuMemory[deviceIdx]);

            pPalMemory = Util::VoidPtrInc(pPalMemory, palMemSize);
        }
    }
    else
    {
        // Allocate memory only for the dispatchable object
        pMemory = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(Buffer),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    BufferFlags bufferFlags;

    bufferFlags.u32All = 0;

    if (palResult == Pal::Result::Success)
    {
        const VkExternalMemoryBufferCreateInfo* pExternalInfo =
            static_cast<const VkExternalMemoryBufferCreateInfo*>(pCreateInfo->pNext);
        if ((pExternalInfo != nullptr) &&
            (pExternalInfo->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO))
        {
            VkExternalMemoryProperties externalMemoryProperties = {};

            pDevice->VkPhysicalDevice()->GetExternalMemoryProperties(
                isSparse,
                static_cast<VkExternalMemoryHandleTypeFlagBits>(pExternalInfo->handleTypes),
                &externalMemoryProperties);

            if (externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
            {
                bufferFlags.dedicatedRequired = true;
            }

            if (externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT |
                                                                   VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
            {
                bufferFlags.externallyShareable = true;

                if (pExternalInfo->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
                {
                    bufferFlags.externalPinnedHost = true;
                }
            }
        }
    }

    if (palResult == Pal::Result::Success)
    {
        bufferFlags.internalMemBound = isSparse;

        // Create barrier policy for the buffer.
        BufferBarrierPolicy barrierPolicy(pDevice,
                                          pCreateInfo->usage);

        // Construct API buffer object.
        VK_PLACEMENT_NEW (pMemory) Buffer (pDevice,
                                           pCreateInfo->flags,
                                           pCreateInfo->usage,
                                           pGpuMemory,
                                           barrierPolicy,
                                           size,
                                           bufferFlags);

        *pBuffer = Buffer::HandleFromVoidPointer(pMemory);
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Destroy a buffer object
VkResult Buffer::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        Pal::IGpuMemory* pMemoryObj = m_pGpuMemory[deviceIdx];

        if (m_internalFlags.internalMemBound == true)
        {
            if (IsSparse() == false)
            {
                m_pDevice->RemoveMemReference(pDevice->PalDevice(deviceIdx), pMemoryObj);
            }

            // Destroy the memory object of the buffer only if it's a sparse buffer as that's when we created a private
            // VA-only memory object

            pMemoryObj->Destroy();
        }
    }

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Bind GPU memory to buffer objects
VkResult Buffer::BindMemory(
    VkDeviceMemory     mem,
    VkDeviceSize       memOffset,
    const uint32_t*    pDeviceIndices)
{
    // The buffer must not be sparse
    VK_ASSERT(IsSparse() == false);

    // Simply use the passed memory object and offset directly
    m_memOffset = memOffset;

    if (mem != VK_NULL_HANDLE)
    {
        VK_ASSERT(m_pMemory == nullptr);

        m_pMemory = Memory::ObjectFromHandle(mem);

        if (m_pDevice->IsMultiGpu() == false)
        {
            const uint32_t singleIdx = DefaultDeviceIndex;

            Pal::IGpuMemory* pPalMemory = m_pMemory->PalMemory(singleIdx);
            m_pGpuMemory[singleIdx]     = pPalMemory;
            m_gpuVirtAddr[singleIdx]    = pPalMemory->Desc().gpuVirtAddr + memOffset;
        }
        else
        {
            const bool multiInstance = m_pMemory->IsMultiInstance();

            PeerMemory* pPeerMemory = m_pMemory->GetPeerMemory();

            for (uint32_t localDeviceIdx = 0; localDeviceIdx < m_pDevice->NumPalDevices(); localDeviceIdx++)
            {
                const uint32_t sourceMemInst = (pDeviceIndices != nullptr) ?
                                            pDeviceIndices[localDeviceIdx]
                                            :
                                            (multiInstance ? localDeviceIdx : DefaultMemoryInstanceIdx);

                m_multiInstanceIndices[localDeviceIdx] = static_cast<uint8_t>(sourceMemInst);

                Pal::IGpuMemory* pPalMemory = nullptr;

                if ((localDeviceIdx == sourceMemInst) || m_pMemory->IsMirroredAllocation(localDeviceIdx))
                {
                    pPalMemory = m_pMemory->PalMemory(localDeviceIdx);
                }
                else
                {
                    pPalMemory = pPeerMemory->AllocatePeerMemory(
                        m_pDevice->PalDevice(localDeviceIdx), localDeviceIdx, sourceMemInst);
                }
                m_pGpuMemory[localDeviceIdx]  = pPalMemory;
                m_gpuVirtAddr[localDeviceIdx] = pPalMemory->Desc().gpuVirtAddr + memOffset;
            }
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Get the buffer's memory requirements
VkResult Buffer::GetMemoryRequirements(
    VkMemoryRequirements* pMemoryRequirements)
{
    const VkDeviceSize ubRequiredAlignment = static_cast<VkDeviceSize>(sizeof(float) * 4);
    const bool         sparseUsageEnabled  = ((m_flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0);
    const bool         ubUsageEnabled      = ((m_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0);

    if (sparseUsageEnabled)
    {
        // In case of sparse buffers the alignment and granularity is the page size
        pMemoryRequirements->alignment = m_pDevice->GetProperties().virtualMemPageSize;
        pMemoryRequirements->size      = Util::RoundUpToMultiple(m_size, pMemoryRequirements->alignment);
    }
    else
    {
        // Otherwise the granularity is the size itself and there's no special alignment requirement,
        // however, we'll specify such an alignment requirement which should fit formatted buffer use
        // with any kind of format
        pMemoryRequirements->alignment = (ubUsageEnabled) ? ubRequiredAlignment : 4;
        pMemoryRequirements->size      = Util::RoundUpToMultiple(m_size, pMemoryRequirements->alignment);
    }

    // Allow all available memory types for buffers
    pMemoryRequirements->memoryTypeBits = m_pDevice->GetMemoryTypeMask();

    // cpu read/write visible heap through thunderbolt has very limited performance.
    // for buffer object application my use cpu to upload or download from gpu visible, so
    // it is better off to not expose visible heap for buffer to application.
    if(m_pDevice->GetProperties().connectThroughThunderBolt)
    {
        uint32_t visibleMemIndex;

        if (m_pDevice->GetVkTypeIndexFromPalHeap(Pal::GpuHeap::GpuHeapLocal, &visibleMemIndex))
        {
            uint32_t visibleMemBit = 1 << visibleMemIndex;
            pMemoryRequirements->memoryTypeBits &= ~visibleMemBit;
        }
    }

    // Limit heaps to those compatible with pinned system memory
    if (m_internalFlags.externalPinnedHost)
    {
        pMemoryRequirements->memoryTypeBits &= m_pDevice->GetPinnedSystemMemoryTypes();

        VK_ASSERT(pMemoryRequirements->memoryTypeBits != 0);
    }

    return VK_SUCCESS;
}

namespace entry
{
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    const VkAllocationCallbacks*                pAllocator)
{
    if (buffer != VK_NULL_HANDLE)
    {
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Buffer::ObjectFromHandle(buffer)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return Buffer::ObjectFromHandle(buffer)->BindMemory(memory, memoryOffset, nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements*                       pMemoryRequirements)
{

    Buffer::ObjectFromHandle(buffer)->GetMemoryRequirements(pMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2(
    VkDevice                                    device,
    const VkBufferMemoryRequirementsInfo2*      pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);
    VK_ASSERT((pDevice->VkPhysicalDevice()->GetEnabledAPIVersion() >= VK_MAKE_VERSION(1, 1, 0)) ||
              pDevice->IsExtensionEnabled(DeviceExtensions::KHR_GET_MEMORY_REQUIREMENTS2));

    union
    {
        const VkStructHeader*                  pHeader;
        const VkBufferMemoryRequirementsInfo2* pRequirementsInfo2;
    };

    pRequirementsInfo2 = pInfo;
    pHeader = utils::GetExtensionStructure(pHeader, VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2);
    if (pHeader != nullptr)
    {
        Buffer* pBuffer = Buffer::ObjectFromHandle(pRequirementsInfo2->buffer);
        VkMemoryRequirements* pRequirements = &pMemoryRequirements->memoryRequirements;
        pBuffer->GetMemoryRequirements(pRequirements);

        if (pMemoryRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2)
        {
            VkMemoryDedicatedRequirements* pMemDedicatedRequirements =
                static_cast<VkMemoryDedicatedRequirements*>(pMemoryRequirements->pNext);

            if ((pMemDedicatedRequirements != nullptr) &&
                (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS))
            {
                pMemDedicatedRequirements->prefersDedicatedAllocation  = pBuffer->DedicatedMemoryRequired();
                pMemDedicatedRequirements->requiresDedicatedAllocation = pBuffer->DedicatedMemoryRequired();
            }
        }
    }
}

} // namespace entry

} // namespace vk
