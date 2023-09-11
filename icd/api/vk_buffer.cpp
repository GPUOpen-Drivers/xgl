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

#include "include/vk_buffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"

#include "palGpuMemory.h"
#include "palDevice.h"
#include "palEventDefs.h"
#include "palQueue.h"
#include "palInlineFuncs.h"

namespace vk
{

// =====================================================================================================================
Buffer::Buffer(
    Device*                      pDevice,
    const VkBufferCreateInfo*    pCreateInfo,
    Pal::IGpuMemory**            pGpuMemory,
    BufferFlags                  internalFlags)
    :
    m_size(pCreateInfo->size),
    m_memOffset(0),
    m_barrierPolicy(
        pDevice,
        Device::GetBufferUsageFlagBits(pCreateInfo),
        pCreateInfo->sharingMode,
        pCreateInfo->queueFamilyIndexCount,
        pCreateInfo->pQueueFamilyIndices)
{
    m_internalFlags.u32All                = internalFlags.u32All;

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pGpuMemory[deviceIdx] != nullptr)
        {
            m_perGpu[deviceIdx].pGpuMemory  = pGpuMemory[deviceIdx];
            m_perGpu[deviceIdx].gpuVirtAddr = pGpuMemory[deviceIdx]->Desc().gpuVirtAddr;
        }
        else
        {
            m_perGpu[deviceIdx].pGpuMemory  = nullptr;
            m_perGpu[deviceIdx].gpuVirtAddr = 0;
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
    Pal::IGpuMemory*         pGpuMemory[MaxPalDevices] = {};
    Pal::GpuMemoryCreateInfo gpuMemoryCreateInfo       = {};

    VkResult result     = VK_SUCCESS;
    size_t   apiSize    = ObjectSize(pDevice);
    size_t   palMemSize = 0;
    bool     isSparse   = (pCreateInfo->flags & SparseEnablingFlags) != 0;

    BufferExtStructs bufferExtStructs = {};

    HandleExtensionStructs(pCreateInfo, &bufferExtStructs);

    if (isSparse)
    {
        const void* pNext = pCreateInfo->pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (pHeader->sType)
            {
                case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO:
                {
                    const auto* pOpaqueCreateInfo = static_cast<const VkBufferOpaqueCaptureAddressCreateInfo*>(pNext);

                    gpuMemoryCreateInfo.vaRange         = Pal::VaRange::CaptureReplay;
                    gpuMemoryCreateInfo.replayVirtAddr  = pOpaqueCreateInfo->opaqueCaptureAddress;

                    break;
                }
            default:
                break;
            }
            pNext = pHeader->pNext;
        };

        // We need virtual remapping support for all sparse resources
        VK_ASSERT(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsVirtualRemappingSupported());

        // We need support for sparse buffers for sparse buffer residency
        if ((pCreateInfo->flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) != 0)
        {
            VK_ASSERT(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureBuffer);
        }

        if ((pCreateInfo->flags & VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT) != 0)
        {
            gpuMemoryCreateInfo.vaRange = Pal::VaRange::CaptureReplay;
        }

        gpuMemoryCreateInfo.alignment          = pDevice->GetProperties().virtualMemAllocGranularity;
        gpuMemoryCreateInfo.size               = Util::RoundUpToMultiple(pCreateInfo->size, gpuMemoryCreateInfo.alignment);
        gpuMemoryCreateInfo.flags.virtualAlloc = 1;
        gpuMemoryCreateInfo.flags.globalGpuVa  = pDevice->IsGlobalGpuVaEnabled();
        gpuMemoryCreateInfo.heapAccess         = Pal::GpuHeapAccess::GpuHeapAccessExplicit;

        // Virtual resource should return 0 on unmapped read if residencyNonResidentStrict is set.
        if (pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
        {
            gpuMemoryCreateInfo.virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
        }

        Pal::Result palResult;

        palMemSize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemorySize(gpuMemoryCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);
    }

#if defined(__unix__)
    gpuMemoryCreateInfo.flags.initializeToZero = pDevice->GetRuntimeSettings().initializeVramToZero;
#endif

    // Allocate memory for the dispatchable object and for sparse buffers, the VA-only memory object
    void* pMemory = pDevice->AllocApiObject(
                        pAllocator,
                        apiSize + (palMemSize * pDevice->NumPalDevices()));

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else if (isSparse)
    {
        void*       pPalMemory = Util::VoidPtrInc(pMemory, apiSize);
        Pal::Result palResult  = Pal::Result::Success;

        for (uint32_t deviceIdx = 0;
            (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
            deviceIdx++)
        {
            if (deviceIdx != DefaultDeviceIndex)
            {
                VK_ASSERT(palMemSize == pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(gpuMemoryCreateInfo,
                                                                                        &palResult));
                VK_ASSERT(palResult == Pal::Result::Success);
            }

            // Create the VA-only memory object needed for sparse buffer support
            palResult = pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
                gpuMemoryCreateInfo,
                pPalMemory,
                &pGpuMemory[deviceIdx]);

            pPalMemory = Util::VoidPtrInc(pPalMemory, palMemSize);
        }

        result = PalToVkResult(palResult);
    }

    if (result == VK_SUCCESS)
    {
        BufferFlags bufferFlags;
        CalculateBufferFlags(pDevice, pCreateInfo, bufferExtStructs, &bufferFlags);

        // Construct API buffer object.
        VK_PLACEMENT_NEW (pMemory) Buffer (pDevice,
                                           pCreateInfo,
                                           pGpuMemory,
                                           bufferFlags);

        *pBuffer = Buffer::HandleFromVoidPointer(pMemory);

        LogBufferCreate(pCreateInfo, *pBuffer, pDevice);
    }

    return result;
}

// =====================================================================================================================
// Logs the creation of a new buffer to PAL
void Buffer::LogBufferCreate(
    const VkBufferCreateInfo* pCreateInfo,
    VkBuffer                  buffer,
    const Device*             pDevice)
{
    VK_ASSERT(pCreateInfo != nullptr);
    VK_ASSERT(pDevice != nullptr);

    // The RMT spec copies the Vulkan spec when it comes to create flags and usage flags for buffer creation.
    // These static asserts are in place to flag any changes to bit position since we copy the full
    // flags value directly.
    typedef Pal::ResourceDescriptionBufferCreateFlags PalCreateFlag;
    static_assert(VK_BUFFER_CREATE_SPARSE_BINDING_BIT ==
        static_cast<uint32_t>(PalCreateFlag::SparseBinding), "Create Flag Mismatch");
    static_assert(VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT ==
        static_cast<uint32_t>(PalCreateFlag::SparseResidency), "Create Flag Mismatch");
    static_assert(VK_BUFFER_CREATE_SPARSE_ALIASED_BIT ==
        static_cast<uint32_t>(PalCreateFlag::SparseAliased), "Create Flag Mismatch");
    static_assert(VK_BUFFER_CREATE_PROTECTED_BIT ==
        static_cast<uint32_t>(PalCreateFlag::Protected), "Create Flag Mismatch");
    static_assert(VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT ==
        static_cast<uint32_t>(PalCreateFlag::DeviceAddressCaptureReplay), "Create Flag Mismatch");

    typedef Pal::ResourceDescriptionBufferUsageFlags PalUsageFlag;
    static_assert(VK_BUFFER_USAGE_TRANSFER_SRC_BIT ==
        static_cast<uint32_t>(PalUsageFlag::TransferSrc), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_TRANSFER_DST_BIT ==
        static_cast<uint32_t>(PalUsageFlag::TransferDst), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::UniformTexelBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::StorageTexelBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::UniformBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::StorageBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_INDEX_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::IndexBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::VertexBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT ==
        static_cast<uint32_t>(PalUsageFlag::IndirectBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT ==
        static_cast<uint32_t>(PalUsageFlag::TransformFeedbackBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT ==
        static_cast<uint32_t>(PalUsageFlag::TransformFeedbackCounterBuffer), "Usage Flag Mismatch");
    static_assert(VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT ==
        static_cast<uint32_t>(PalUsageFlag::ConditionalRendering), "Usage Flag Mismatch");
#if VKI_RAY_TRACING
    static_assert(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR ==
        static_cast<uint32_t>(PalUsageFlag::RayTracing), "Usage Flag Mismatch");
#endif
    static_assert(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT ==
        static_cast<uint32_t>(PalUsageFlag::ShaderDeviceAddress), "Usage Flag Mismatch");

    Pal::ResourceDescriptionBuffer desc = {};
    desc.size        = pCreateInfo->size;
    desc.createFlags = pCreateInfo->flags;
    desc.usageFlags  = pCreateInfo->usage;

    Buffer* pBufferObj = Buffer::ObjectFromHandle(buffer);

    Pal::ResourceCreateEventData data = {};
    data.type              = Pal::ResourceType::Buffer;
    data.pResourceDescData = &desc;
    data.resourceDescSize  = sizeof(Pal::ResourceDescriptionBuffer);
    data.pObj              = pBufferObj;

    pDevice->VkInstance()->PalPlatform()->LogEvent(
        Pal::PalEvent::GpuMemoryResourceCreate,
        &data,
        sizeof(Pal::ResourceCreateEventData));

    // If there is already memory bound, log it now.
    // @NOTE - This only handles the single GPU case currently.  MGPU is not supported by RMV v1
    Pal::IGpuMemory* pPalMemory = pBufferObj->PalMemory(DefaultDeviceIndex);
    if (pPalMemory != nullptr)
    {
        pBufferObj->LogGpuMemoryBind(pDevice, pPalMemory, pBufferObj->MemOffset());
    }
}

// =====================================================================================================================
// Logs the binding of GPU Memory to a Buffer.
void Buffer::LogGpuMemoryBind(
    const Device*          pDevice,
    const Pal::IGpuMemory* pPalMemory,
    VkDeviceSize           memOffset
    ) const
{
    VK_ASSERT(pDevice != nullptr);

    Pal::GpuMemoryResourceBindEventData bindData = {};
    bindData.pObj               = this;
    bindData.pGpuMemory         = pPalMemory;
    bindData.requiredGpuMemSize = m_size;
    bindData.offset             = memOffset;

    pDevice->VkInstance()->PalPlatform()->LogEvent(
        Pal::PalEvent::GpuMemoryResourceBind,
        &bindData,
        sizeof(Pal::GpuMemoryResourceBindEventData));
}

// =====================================================================================================================
// Destroy a buffer object
VkResult Buffer::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    Pal::ResourceDestroyEventData data = {};
    data.pObj = this;

    pDevice->VkInstance()->PalPlatform()->LogEvent(
        Pal::PalEvent::GpuMemoryResourceDestroy,
        &data,
        sizeof(Pal::ResourceDestroyEventData));

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        Pal::IGpuMemory* pMemoryObj = m_perGpu[deviceIdx].pGpuMemory;

        if (m_internalFlags.internalMemBound == true)
        {
            if (IsSparse() == false)
            {
                pDevice->RemoveMemReference(pDevice->PalDevice(deviceIdx), pMemoryObj);
            }

            // Destroy the memory object of the buffer only if it's a sparse buffer as that's when we created a private
            // VA-only memory object

            pMemoryObj->Destroy();
        }
    }

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Bind GPU memory to buffer objects
VkResult Buffer::BindMemory(
    const Device*      pDevice,
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
        Memory*pMemory = Memory::ObjectFromHandle(mem);

        if (pDevice->IsMultiGpu() == false)
        {
            const uint32_t singleIdx = DefaultDeviceIndex;

            Pal::IGpuMemory* pPalMemory = pMemory->PalMemory(singleIdx);
            m_perGpu[singleIdx].pGpuMemory  = pPalMemory;
            m_perGpu[singleIdx].gpuVirtAddr = pPalMemory->Desc().gpuVirtAddr + memOffset;

            // @NOTE - This only handles the single GPU case currently.  MGPU is not supported by RMV v1
            LogGpuMemoryBind(pDevice, pPalMemory, memOffset);
        }
        else
        {
            for (uint32_t localDeviceIdx = 0; localDeviceIdx < pDevice->NumPalDevices(); localDeviceIdx++)
            {
                // it is VkMemory to handle the m_multiInstance
                const uint32_t sourceMemInst = pDeviceIndices ? pDeviceIndices[localDeviceIdx] : localDeviceIdx;

                m_perGpu[localDeviceIdx].pGpuMemory  = pMemory->PalMemory(localDeviceIdx, sourceMemInst);
                m_perGpu[localDeviceIdx].gpuVirtAddr =
                    m_perGpu[localDeviceIdx].pGpuMemory->Desc().gpuVirtAddr + memOffset;
            }
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Get the buffer's memory requirements from VkBuffer itself
void Buffer::GetMemoryRequirements(
    const Device*         pDevice,
    VkMemoryRequirements* pMemoryRequirements)
{
    GetBufferMemoryRequirements(pDevice, &m_internalFlags, m_size, pMemoryRequirements);
}

// =====================================================================================================================
// Get the buffer's memory requirements from VkBufferCreateInfo
void Buffer::CalculateMemoryRequirements(
    const Device*                              pDevice,
    const VkDeviceBufferMemoryRequirementsKHR* pInfo,
    VkMemoryRequirements2*                     pMemoryRequirements)
{
    BufferExtStructs bufferExtStructs = {};
    BufferFlags      bufferFlags;

    HandleExtensionStructs(pInfo->pCreateInfo, &bufferExtStructs);
    CalculateBufferFlags(pDevice, pInfo->pCreateInfo, bufferExtStructs, &bufferFlags);

    VkMemoryDedicatedRequirements* pMemDedicatedRequirements =
        static_cast<VkMemoryDedicatedRequirements*>(pMemoryRequirements->pNext);

    if ((pMemDedicatedRequirements != nullptr) &&
        (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS))
    {
        pMemDedicatedRequirements->prefersDedicatedAllocation  = bufferFlags.dedicatedRequired;
        pMemDedicatedRequirements->requiresDedicatedAllocation = bufferFlags.dedicatedRequired;
    }

    GetBufferMemoryRequirements(pDevice,
                                &bufferFlags,
                                pInfo->pCreateInfo->size,
                                &pMemoryRequirements->memoryRequirements);
}

// =====================================================================================================================
// Get the buffer's memory requirements
void Buffer::GetBufferMemoryRequirements(
    const Device*         pDevice,
    const BufferFlags*    pBufferFlags,
    const VkDeviceSize    size,
    VkMemoryRequirements* pMemoryRequirements)
{
    pMemoryRequirements->alignment = 4;
    pMemoryRequirements->size      = size;

    // In case of sparse buffers the alignment and granularity is the page size
    if (pBufferFlags->createSparseBinding)
    {
        const VkDeviceSize sparseAllocGranularity = pDevice->GetProperties().virtualMemAllocGranularity;

        pMemoryRequirements->alignment = Util::Max(pMemoryRequirements->alignment, sparseAllocGranularity);

        pMemoryRequirements->size = Util::RoundUpToMultiple(pMemoryRequirements->size, sparseAllocGranularity);
    }

    if (pBufferFlags->usageUniformBuffer)
    {
        const VkPhysicalDeviceLimits& limits = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetLimits();

        pMemoryRequirements->alignment = Util::Max(pMemoryRequirements->alignment,
                                                   limits.minUniformBufferOffsetAlignment);
    }

#if VKI_RAY_TRACING
    if (pBufferFlags->usageAccelStorage)
    {
        pMemoryRequirements->alignment = Util::Max(pMemoryRequirements->alignment,
                                                   static_cast<VkDeviceSize>(GpuRt::RayTraceAccelMemoryBaseAlignment));
    }
#endif

    // MemoryRequirements cannot return smaller size than buffer size.
    // MAX_UINT64 can be used as buffer size.
    if (size > pMemoryRequirements->size)
    {
        pMemoryRequirements->size = size;
    }

    // Allow all available memory types for buffers
    pMemoryRequirements->memoryTypeBits = pDevice->GetMemoryTypeMask();

    // cpu read/write visible heap through thunderbolt has very limited performance.
    // for buffer object application my use cpu to upload or download from gpu visible, so
    // it is better off to not expose visible heap for buffer to application.
    if(pDevice->GetProperties().connectThroughThunderBolt)
    {
        uint32_t visibleMemIndexBits;

        if (pDevice->GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeap::GpuHeapLocal, &visibleMemIndexBits))
        {
            pMemoryRequirements->memoryTypeBits &= ~visibleMemIndexBits;
        }
    }

    // Limit heaps to those compatible with pinned system memory
    if (pBufferFlags->externalPinnedHost)
    {
        pMemoryRequirements->memoryTypeBits &= pDevice->GetPinnedSystemMemoryTypes();

        VK_ASSERT(pMemoryRequirements->memoryTypeBits != 0);
    }
    else if (pBufferFlags->externallyShareable)
    {
        pMemoryRequirements->memoryTypeBits &= pDevice->GetMemoryTypeMaskForExternalSharing();
    }

    if (pBufferFlags->createProtected)
    {
        // If the buffer is protected only keep the protected type
        pMemoryRequirements->memoryTypeBits &= pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_PROTECTED_BIT);
    }
    else
    {
        // Remove the protected types
        pMemoryRequirements->memoryTypeBits &= ~pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_PROTECTED_BIT);
    }

    if (pDevice->GetEnabledFeatures().deviceCoherentMemory == false)
    {
        // If the state of the device coherent memory feature (defined by the extension VK_AMD_device_coherent_memory) is disabled,
        // remove the device coherent memory type
        pMemoryRequirements->memoryTypeBits &= ~pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD);
    }

    if (pBufferFlags->usageDescriptor)
    {
        pMemoryRequirements->memoryTypeBits &= pDevice->GetMemoryTypeMaskForDescriptorBuffers();
    }
    else
    {
        pMemoryRequirements->memoryTypeBits &= ~pDevice->GetMemoryTypeMaskForDescriptorBuffers();
    }
}

void Buffer::CalculateBufferFlags(
    const Device*             pDevice,
    const VkBufferCreateInfo* pCreateInfo,
    const BufferExtStructs&   pExtStructs,
    BufferFlags*              pBufferFlags)
{
    pBufferFlags->u32All = 0;

    BufferUsageFlagBits usage = Device::GetBufferUsageFlagBits(pCreateInfo);

    pBufferFlags->usageUniformBuffer    = (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)    ? 1 : 0;
#if VKI_RAY_TRACING
    pBufferFlags->usageAccelStorage     = (usage &
                                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) ? 1 : 0;
#endif
    pBufferFlags->usageDescriptor       = (usage &
                                             (VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                                              VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT  |
                                              VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT)) ? 1 : 0;
    pBufferFlags->createSparseBinding   = (pCreateInfo->flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)   ? 1 : 0;
    pBufferFlags->createSparseResidency = (pCreateInfo->flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) ? 1 : 0;
    pBufferFlags->createProtected       = (pCreateInfo->flags & VK_BUFFER_CREATE_PROTECTED_BIT)        ? 1 : 0;
    // Note: The VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT is only used for sparse buffers

    bool isSparse = (pCreateInfo->flags & SparseEnablingFlags) != 0;

    if (pExtStructs.pExternalMemoryBufferCreateInfo != nullptr)
    {
        VkExternalMemoryProperties externalMemoryProperties = {};

        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetExternalMemoryProperties(
            isSparse,
            false,
            static_cast<VkExternalMemoryHandleTypeFlagBits>(pExtStructs.pExternalMemoryBufferCreateInfo->handleTypes),
            &externalMemoryProperties);

        if (externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
        {
            pBufferFlags->dedicatedRequired = true;
        }

        if (externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT |
                                                               VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        {
            pBufferFlags->externallyShareable = true;

            if (pExtStructs.pExternalMemoryBufferCreateInfo->handleTypes &
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
            {
                pBufferFlags->externalPinnedHost = true;
            }
        }
    }

     pBufferFlags->internalMemBound = isSparse;
}

// =====================================================================================================================
void Buffer::HandleExtensionStructs(
    const VkBufferCreateInfo*   pCreateInfo,
    BufferExtStructs*           pExtStructs)
{
    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
        {
            pExtStructs->pExternalMemoryBufferCreateInfo =
                static_cast<const VkExternalMemoryBufferCreateInfo*>(pNext);
            break;
        }
        default:
            // Skip any unknown extension structures.
            break;
        }

        pNext = pHeader->pNext;
    }

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
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
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

    return Buffer::ObjectFromHandle(buffer)->BindMemory(pDevice, memory, memoryOffset, nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements*                       pMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Buffer::ObjectFromHandle(buffer)->GetMemoryRequirements(pDevice, pMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2(
    VkDevice                                    device,
    const VkBufferMemoryRequirementsInfo2*      pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Buffer* pBuffer = Buffer::ObjectFromHandle(pInfo->buffer);
    VkMemoryRequirements* pRequirements = &pMemoryRequirements->memoryRequirements;
    pBuffer->GetMemoryRequirements(pDevice, pRequirements);

    VkMemoryDedicatedRequirements* pMemDedicatedRequirements =
        static_cast<VkMemoryDedicatedRequirements*>(pMemoryRequirements->pNext);

    if ((pMemDedicatedRequirements != nullptr) &&
        (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS))
    {
        pMemDedicatedRequirements->prefersDedicatedAllocation  = pBuffer->DedicatedMemoryRequired();
        pMemDedicatedRequirements->requiresDedicatedAllocation = pBuffer->DedicatedMemoryRequired();
    }
}

// =====================================================================================================================
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddress(
    VkDevice                                    device,
    const VkBufferDeviceAddressInfo* const      pInfo)
{
    Buffer* const pBuffer = Buffer::ObjectFromHandle(pInfo->buffer);

    return pBuffer->GpuVirtAddr(DefaultDeviceIndex);
}

// =====================================================================================================================
VKAPI_ATTR uint64_t VKAPI_CALL vkGetBufferOpaqueCaptureAddress(
    VkDevice                                    device,
    const VkBufferDeviceAddressInfo*            pInfo)
{
    Buffer* const pBuffer = Buffer::ObjectFromHandle(pInfo->buffer);

    uint64_t gpuVirtAddr = 0;

    if (pBuffer->IsSparse())
    {
        gpuVirtAddr = pBuffer->GpuVirtAddr(DefaultDeviceIndex);
    }

    return gpuVirtAddr;
}

} // namespace entry

} // namespace vk
