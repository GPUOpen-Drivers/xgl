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
 * @file  vk_descriptor_set.cpp
 * @brief Contains implementation of Vulkan descriptor set objects.
 ***********************************************************************************************************************
 */

#include "include/vk_descriptor_set.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_descriptor_pool.h"
#include "include/vk_device.h"
#include "include/vk_buffer_view.h"
#include "include/vk_image_view.h"
#include "include/vk_sampler.h"
#include "vk_conv.h"
#include "vk_framebuffer.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_acceleration_structure.h"
#endif

namespace vk
{

// =====================================================================================================================
template <uint32_t numPalDevices>
DescriptorSet<numPalDevices>::DescriptorSet(
    uint32_t           heapIndex)
    :
    m_pLayout(nullptr),
    m_pAllocHandle(nullptr),
    m_heapIndex(heapIndex)
{
    memset(m_addresses, 0, sizeof(m_addresses));
}

// =====================================================================================================================
// Assigns a GPU range and layout to a descriptor set on allocation.  This is called from a descriptor pool when it
// allocates memory for this set during vkAllocDescriptorSets.
//
// NOTE: The given handle's value may be modified by this function.
template <uint32_t numPalDevices>
void DescriptorSet<numPalDevices>::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    DescriptorAddr*             pBaseAddrs,
    void*                       pAllocHandle)
{
    m_pLayout = pLayout;
    m_pAllocHandle = pAllocHandle;

    for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
    {
        // When memory is assigned to this descriptor set let's cache its mapped CPU address as we anyways use
        // persistent mapped memory for descriptor pools.
        m_addresses[deviceIdx].staticCpuAddr = static_cast<uint32_t*>(Util::VoidPtrInc(pBaseAddrs[deviceIdx].staticCpuAddr, static_cast<intptr_t>(gpuMemOffset)));
        VK_ASSERT(Util::IsPow2Aligned(reinterpret_cast<uint64_t>(m_addresses[deviceIdx].staticCpuAddr), sizeof(uint32_t)));

        m_addresses[deviceIdx].staticGpuAddr = pBaseAddrs[deviceIdx].staticGpuAddr + gpuMemOffset;

        if (pBaseAddrs[deviceIdx].fmaskCpuAddr != nullptr)
        {
            m_addresses[deviceIdx].fmaskCpuAddr = static_cast<uint32_t*>(Util::VoidPtrInc(pBaseAddrs[deviceIdx].fmaskCpuAddr, static_cast<intptr_t>(gpuMemOffset)));
            VK_ASSERT(Util::IsPow2Aligned(reinterpret_cast<uint64_t>(m_addresses[deviceIdx].fmaskCpuAddr), sizeof(uint32_t)));
        }
    }
}

// =====================================================================================================================
// Writes the immutable samplers in the layout to memory.
template <uint32_t numPalDevices>
void DescriptorSet<numPalDevices>::WriteImmutableSamplers(
    uint32_t imageDescSizeInBytes)
{
    for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
    {
        for (uint32_t bindingIndex = 0; bindingIndex < Layout()->Info().count; ++bindingIndex)
        {
            const DescriptorSetLayout::BindingInfo& bindingInfo = Layout()->Binding(bindingIndex);

            if (bindingInfo.imm.dwSize != 0)
            {
                uint32_t* pSamplerDesc = Layout()->Info().imm.pImmutableSamplerData + bindingInfo.imm.dwOffset;
                const size_t srcArrayStrideInDW = bindingInfo.imm.dwArrayStride;
                uint32_t numOfSamplers = bindingInfo.info.descriptorCount;

                for (uint32_t descriptorIdx = 0; descriptorIdx < numOfSamplers; ++descriptorIdx)
                {
                  size_t destOffset = Layout()->GetDstStaOffset(bindingInfo, descriptorIdx);
                  uint32_t* pDestAddr = StaticCpuAddress(deviceIdx) + destOffset;
                  if (bindingInfo.info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                  {
                    pDestAddr += (imageDescSizeInBytes / sizeof(uint32_t));
                  }

                  memcpy(pDestAddr, pSamplerDesc,(sizeof(uint32) * bindingInfo.imm.dwSize) / numOfSamplers);

                  pSamplerDesc += srcArrayStrideInDW;
                }
            }
        }
    }
}

// =====================================================================================================================
// Resets a DescriptorSet to an intial state
template <uint32_t numPalDevices>
void DescriptorSet<numPalDevices>::Reset()
{
    m_pLayout = nullptr;
    m_pAllocHandle = nullptr;

    memset(m_addresses, 0, sizeof(m_addresses));
}

// =====================================================================================================================
// Write sampler descriptors
template <size_t samplerDescSize>
void DescriptorUpdate::WriteSamplerDescriptors(
    const VkDescriptorImageInfo* pDescriptors,
    uint32_t*                    pDestAddr,
    uint32_t                     count,
    uint32_t                     dwStride,
    size_t                       descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes :
                                                                                    sizeof(VkDescriptorImageInfo);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pImageInfo->sampler == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, samplerDescSize);
        }
        else
        {
            const void* pSamplerDesc = Sampler::ObjectFromHandle(pImageInfo->sampler)->Descriptor();

            memcpy(pDestAddr, pSamplerDesc, samplerDescSize);
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write combined image-sampler descriptors
template <size_t imageDescSize, size_t samplerDescSize>
void DescriptorUpdate::WriteImageSamplerDescriptors(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                                  : sizeof(VkDescriptorImageInfo);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pImageInfo->imageView == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, imageDescSize);
        }
        else
        {
            const void* pImageDesc = ImageView::ObjectFromHandle(pImageInfo->imageView)->
                Descriptor(deviceIdx, false, imageDescSize);

            memcpy(pDestAddr, pImageDesc, imageDescSize);
        }

        if (pImageInfo->sampler == VK_NULL_HANDLE)
        {
            memset(pDestAddr + (imageDescSize / sizeof(uint32_t)), 0, samplerDescSize);
        }
        else
        {
            const void* pSamplerDesc = Sampler::ObjectFromHandle(pImageInfo->sampler)->Descriptor();

            memcpy(pDestAddr + (imageDescSize / sizeof(uint32_t)), pSamplerDesc, samplerDescSize);
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write image view descriptors (including input attachments)
template <size_t imageDescSize, bool isShaderStorageDesc>
void DescriptorUpdate::WriteImageDescriptors(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                                  : sizeof(VkDescriptorImageInfo);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pImageInfo->imageView == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, imageDescSize);
        }
        else
        {
            const void* pImageDesc = ImageView::ObjectFromHandle(pImageInfo->imageView)->
                Descriptor(deviceIdx, isShaderStorageDesc, imageDescSize);

            memcpy(pDestAddr, pImageDesc, imageDescSize);
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write image view descriptors
template <size_t imageDescSize>
void DescriptorUpdate::WriteImageDescriptorsYcbcr(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                                  : sizeof(VkDescriptorImageInfo);

    uint32_t pOutImageDescStride = Util::NumBytesToNumDwords(imageDescSize);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pImageInfo->imageView == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, imageDescSize);
        }
        else
        {
            const uint32_t multiPlaneCount =
                Formats::GetYuvPlaneCounts(ImageView::ObjectFromHandle(pImageInfo->imageView)->GetViewFormat());
            const void* pImageDesc = ImageView::ObjectFromHandle(pImageInfo->imageView)->
                Descriptor(deviceIdx, false, imageDescSize * multiPlaneCount);

            uint32_t* pOutImageDesc = pDestAddr;

            for (uint32_t plane = 0; plane < multiPlaneCount; ++plane, pOutImageDesc += pOutImageDescStride)
            {
                static_assert(
                    (imageDescSize == 32) || (imageDescSize == 48),
                    "Unexpected WriteImageDescriptorsYcbcr imageDescSize specialization.");

                memcpy(pOutImageDesc, pImageDesc, 32);

                if (imageDescSize == 48)
                {
                    memset(pOutImageDesc + 8, 0, 16);
                }

                pImageDesc = Util::VoidPtrInc(pImageDesc, imageDescSize * ImageView::SrdIndexType::SrdCount);
            }
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write fmask descriptors
template <size_t imageDescSize, size_t fmaskDescSize>
void DescriptorUpdate::WriteFmaskDescriptors(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                                  : sizeof(VkDescriptorImageInfo);
    VK_ASSERT(dwStride * sizeof(uint32_t) >= fmaskDescSize);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pImageInfo->imageView == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, fmaskDescSize);
        }
        else
        {
            const ImageView* const pImageView = ImageView::ObjectFromHandle(pImageInfo->imageView);
            const void* pImageDesc = pImageView->Descriptor(deviceIdx, false, 0);

            if (pImageView->NeedsFmaskViewSrds())
            {
                // Copy over FMASK descriptor
                // Image descriptors including shader read and write descriptors.
                const void* pSrcFmaskAddr = Util::VoidPtrInc(pImageDesc, imageDescSize * ImageView::SrdIndexType::SrdCount);

                memcpy(pDestAddr, pSrcFmaskAddr, fmaskDescSize);
            }
            else
            {
                // If no FMASK descriptor, need clear the memory to 0.
                memset(pDestAddr, 0, fmaskDescSize);
            }
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write buffer descriptors
template <size_t bufferDescSize, VkDescriptorType type>
void DescriptorUpdate::WriteBufferDescriptors(
    const VkBufferView*                 pDescriptors,
    uint32_t                            deviceIdx,
    uint32_t*                           pDestAddr,
    uint32_t                            count,
    uint32_t                            dwStride,
    size_t                              descriptorStrideInBytes)
{
    const VkBufferView* pBufferView      = pDescriptors;
    const size_t        bufferViewStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                          : sizeof(VkBufferView);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (*pBufferView == VK_NULL_HANDLE)
        {
            memset(pDestAddr, 0, bufferDescSize);
        }
        else
        {
            const void* pBufferDesc = BufferView::ObjectFromHandle(*pBufferView)->Descriptor(type, deviceIdx);

            memcpy(pDestAddr, pBufferDesc, bufferDescSize);
        }

        pBufferView = static_cast<const VkBufferView*>(Util::VoidPtrInc(pBufferView, bufferViewStride));
    }
}

// =====================================================================================================================
// Write buffer descriptors using bufferInfo field used with uniform and storage buffers
template <size_t bufferDescSize, VkDescriptorType type>
void DescriptorUpdate::WriteBufferInfoDescriptors(
    const Device*                   pDevice,
    const VkDescriptorBufferInfo*   pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes)
{
    const VkDescriptorBufferInfo* pBufferInfo      = pDescriptors;
    const size_t                  bufferInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes
                                                                                    : sizeof(VkDescriptorBufferInfo);
    Pal::BufferViewInfo info = {};

    VK_ASSERT((type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)         ||
              (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
              (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)         ||
              (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC));

    // Setup and create SRD for storage buffer case
    info.swizzledFormat = Pal::UndefinedSwizzledFormat;
    info.stride         = 0; // Raw buffers have a zero byte stride

    Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

    // Build the SRD
    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        if (pBufferInfo->buffer == VK_NULL_HANDLE)
        {
            if (pDevice->UseCompactDynamicDescriptors() &&
                ((type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) ||
                 (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)))
            {
                pDestAddr[0] = 0;
                pDestAddr[1] = 0;
            }
            else
            {
                memset(pDestAddr, 0, bufferDescSize);
            }
        }
        else
        {
            info.gpuAddr = Buffer::ObjectFromHandle(pBufferInfo->buffer)->GpuVirtAddr(deviceIdx) + pBufferInfo->offset;

            if (pDevice->UseCompactDynamicDescriptors() &&
                ((type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) ||
                 (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)))
            {
                pDestAddr[0] = Util::LowPart(info.gpuAddr);
                pDestAddr[1] = Util::HighPart(info.gpuAddr);
            }
            else
            {
                if (pBufferInfo->range == VK_WHOLE_SIZE)
                {
                    info.range = reinterpret_cast<Buffer*>(pBufferInfo->buffer)->GetSize() - pBufferInfo->offset;
                }
                else
                {
                    info.range = pBufferInfo->range;
                }

                // Align the buffer range in srd to dword. This should be safe since the buffer memory size will be
                // dword-aligned - we have an at least 4-byte alignment requirement in vkGetBufferMemoryRequirements.
                info.range = Util::RoundUpToMultiple(info.range, static_cast<Pal::gpusize>(sizeof(uint32_t)));

                pPalDevice->CreateUntypedBufferViewSrds(1, &info, pDestAddr);
            }
        }

        pBufferInfo = static_cast<const VkDescriptorBufferInfo*>(Util::VoidPtrInc(pBufferInfo, bufferInfoStride));
    }
}

#if VKI_RAY_TRACING
void DescriptorUpdate::SetAccelerationDescriptorsBufferViewFlags(
    const Device*           pDevice,
    Pal::BufferViewInfo*    pBufferViewInfo)
{
    // Bypass Mall cache read/write if no alloc policy is set for SRDs.
    // This global setting applies to every BVH SRD.
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocBvh))
    {
        pBufferViewInfo->flags.bypassMallRead  = 1;
        pBufferViewInfo->flags.bypassMallWrite = 1;
    }
}

void DescriptorUpdate::WriteAccelerationStructureDescriptors(
    const Device*                       pDevice,
    const VkAccelerationStructureKHR*   pDescriptors,
    uint32_t                            deviceIdx,
    uint32_t*                           pDestAddr,
    uint32_t                            count,
    uint32_t                            dwStride,
    size_t                              descriptorStrideInBytes)
{
    for (uint32_t arrElem = 0; arrElem < count; ++arrElem)
    {
        const AccelerationStructure* pAccel = AccelerationStructure::ObjectFromHandle(
            pDescriptors[arrElem]);

        Pal::BufferViewInfo bufferViewInfo = {};

        if (pAccel != nullptr)
        {
            bufferViewInfo.gpuAddr = pAccel->GetDeviceAddress(deviceIdx);
            bufferViewInfo.range = pAccel->GetPrebuildInfo().resultDataMaxSizeInBytes;
        }

        SetAccelerationDescriptorsBufferViewFlags(pDevice, &bufferViewInfo);

        pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &bufferViewInfo, pDestAddr);

        pDestAddr += dwStride;
    }
}
#endif

// =====================================================================================================================
// Write data to the inline uniform block
void DescriptorUpdate::WriteInlineUniformBlock(
    const void*                     pData,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride
)
{
    memcpy(pDestAddr + dwStride, pData, count);
}

// =====================================================================================================================
// Write to descriptor sets using the provided descriptors for resources
template <size_t imageDescSize,
          size_t fmaskDescSize,
          size_t samplerDescSize,
          size_t bufferDescSize,
          uint32_t numPalDevices>
void DescriptorUpdate::WriteDescriptorSets(
    const Device*                pDevice,
    uint32_t                     deviceIdx,
    uint32_t                     descriptorWriteCount,
    const VkWriteDescriptorSet*  pDescriptorWrites)
{
    for (uint32_t i = 0; i < descriptorWriteCount; ++i)
    {
        const VkWriteDescriptorSet& params = pDescriptorWrites[i];

        VK_ASSERT(params.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);

        DescriptorSet<numPalDevices>* pDestSet  = DescriptorSet<numPalDevices>::ObjectFromHandle(params.dstSet);
        const DescriptorSetLayout::BindingInfo& destBinding = pDestSet->Layout()->Binding(params.dstBinding);
        uint32_t* pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) +
                              pDestSet->Layout()->GetDstStaOffset(destBinding, params.dstArrayElement);

        uint32_t* pDestFmaskAddr = pDestSet->FmaskCpuAddress(deviceIdx) +
                                   pDestSet->Layout()->GetDstStaOffset(destBinding, params.dstArrayElement);

        // Determine whether the binding has immutable sampler descriptors.
        bool hasImmutableSampler = (destBinding.imm.dwSize != 0);

        VK_ASSERT(params.descriptorType != VK_DESCRIPTOR_TYPE_MUTABLE_EXT);

        switch (static_cast<uint32_t>(params.descriptorType))
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            if (hasImmutableSampler)
            {
                VK_ASSERT(!"Immutable samplers cannot be updated");
            }
            else
            {
                WriteSamplerDescriptors<samplerDescSize>(
                    params.pImageInfo,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
            }
            break;

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if (hasImmutableSampler)
            {
                if (destBinding.bindingFlags.ycbcrConversionUsage == 0)
                {
                    // If the sampler part of the combined image sampler is immutable then we should only update the image
                    // descriptors, but have to make sure to still use the appropriate stride.
                    WriteImageDescriptors<imageDescSize, false>(
                        params.pImageInfo,
                        deviceIdx,
                        pDestAddr,
                        params.descriptorCount,
                        destBinding.sta.dwArrayStride);
                }
                else
                {
                    WriteImageDescriptorsYcbcr<imageDescSize>(
                        params.pImageInfo,
                        deviceIdx,
                        pDestAddr,
                        params.descriptorCount,
                        destBinding.sta.dwArrayStride);
                }
            }
            else
            {
                WriteImageSamplerDescriptors<imageDescSize, samplerDescSize>(
                    params.pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
            }

            if (fmaskDescSize != 0)
            {
                 WriteFmaskDescriptors<imageDescSize, fmaskDescSize>(
                     params.pImageInfo,
                     deviceIdx,
                     pDestFmaskAddr,
                     params.descriptorCount,
                     destBinding.sta.dwArrayStride);
            }

            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            WriteImageDescriptors<imageDescSize, true>(
                params.pImageInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            WriteImageDescriptors<imageDescSize, false>(
                params.pImageInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);

            if (fmaskDescSize != 0)
            {
                WriteFmaskDescriptors<imageDescSize, fmaskDescSize>(
                    params.pImageInfo,
                    deviceIdx,
                    pDestFmaskAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
            }
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            WriteBufferDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(
                params.pTexelBufferView,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            WriteBufferDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(
                params.pTexelBufferView,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(
                pDevice,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(
                pDevice,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            // Dynamic buffer descriptors reside in client memory to be read when the descriptor set is bound.
            pDestAddr = pDestSet->DynamicDescriptorData(deviceIdx) +
                        pDestSet->Layout()->GetDstDynOffset(destBinding, params.dstArrayElement);

            WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(
                pDevice,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.dyn.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            // Dynamic buffer descriptors reside in client memory to be read when the descriptor set is bound.
            pDestAddr = pDestSet->DynamicDescriptorData(deviceIdx) +
                        pDestSet->Layout()->GetDstDynOffset(destBinding, params.dstArrayElement);

            WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC>(
                pDevice,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.dyn.dwArrayStride);
            break;

        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        {
            VK_ASSERT(params.pNext != nullptr);
            VK_ASSERT(Util::IsPow2Aligned(params.dstArrayElement, 4));
            VK_ASSERT(Util::IsPow2Aligned(params.descriptorCount, 4));

            const VkWriteDescriptorSetInlineUniformBlockEXT *inlineUniformBlockParams =
                reinterpret_cast<const VkWriteDescriptorSetInlineUniformBlockEXT*>(params.pNext);
            VK_ASSERT(inlineUniformBlockParams->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT);
            VK_ASSERT(inlineUniformBlockParams->dataSize == params.descriptorCount);

            pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) + destBinding.sta.dwOffset;

            WriteInlineUniformBlock(
                inlineUniformBlockParams->pData,
                pDestAddr,
                params.descriptorCount,
                params.dstArrayElement / 4);

            break;
        }

#if VKI_RAY_TRACING
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        {
            const auto* pWriteAccelStructKHR =
                reinterpret_cast<const VkWriteDescriptorSetAccelerationStructureKHR*>(
                utils::GetExtensionStructure(reinterpret_cast<const VkStructHeader*>(params.pNext),
                static_cast<VkStructureType>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)));

            VK_ASSERT(pWriteAccelStructKHR != nullptr);
            VK_ASSERT(pWriteAccelStructKHR->accelerationStructureCount == params.descriptorCount);

            WriteAccelerationStructureDescriptors(
                pDevice,
                pWriteAccelStructKHR->pAccelerationStructures,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride);

            break;
        }
#endif
        default:
            VK_ASSERT(!"Unexpected descriptor type");
            break;
        }
    }
}

// =====================================================================================================================
// Copy from one descriptor set to another
template <size_t imageDescSize, size_t fmaskDescSize, uint32_t numPalDevices>
void DescriptorUpdate::CopyDescriptorSets(
    const Device*                pDevice,
    uint32_t                     deviceIdx,
    uint32_t                     descriptorCopyCount,
    const VkCopyDescriptorSet*   pDescriptorCopies)
{
    for (uint32_t i = 0; i < descriptorCopyCount; ++i)
    {
        const VkCopyDescriptorSet& params = pDescriptorCopies[i];
        uint32_t count = params.descriptorCount;

        VK_ASSERT(params.sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
        VK_ASSERT(params.pNext == nullptr);

        DescriptorSet<numPalDevices>* pSrcSet   = DescriptorSet<numPalDevices>::ObjectFromHandle(params.srcSet);
        DescriptorSet<numPalDevices>* pDestSet  = DescriptorSet<numPalDevices>::ObjectFromHandle(params.dstSet);

        const DescriptorSetLayout::BindingInfo& srcBinding  = pSrcSet->Layout()->Binding(params.srcBinding);
        const DescriptorSetLayout::BindingInfo& destBinding = pDestSet->Layout()->Binding(params.dstBinding);

        // Determine whether the bindings have immutable sampler descriptors. If one has both must.
        VK_ASSERT((srcBinding.imm.dwSize != 0) == (destBinding.imm.dwSize != 0));
        bool hasImmutableSampler  = (destBinding.imm.dwSize != 0);

        // Source and destination descriptor types are expected to match.
        VK_ASSERT((srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) ||
            (destBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) ||
            (srcBinding.info.descriptorType == destBinding.info.descriptorType));

        // Cannot copy between sampler descriptors that are immutable and thus don't have any mutable portion
        VK_ASSERT((hasImmutableSampler == false) || (srcBinding.info.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER));

        if ((srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) ||
            (destBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT))
        {
            VK_ASSERT(destBinding.sta.dwArrayStride > 0);
            VK_ASSERT(srcBinding.sta.dwArrayStride > 0);
            uint32_t* pSrcAddr  = pSrcSet->StaticCpuAddress(deviceIdx) + srcBinding.sta.dwOffset
                                + params.srcArrayElement * srcBinding.sta.dwArrayStride;

            uint32_t* pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) + destBinding.sta.dwOffset
                                + params.dstArrayElement * destBinding.sta.dwArrayStride;

            if (srcBinding.sta.dwArrayStride == destBinding.sta.dwArrayStride)
            {
                // Source and destination have the same memory layout of array elements.
                memcpy(pDestAddr, pSrcAddr, srcBinding.sta.dwArrayStride * sizeof(uint32_t) * count);
            }
            else
            {
                const auto arrayElementSize = Util::Min(
                            destBinding.sta.dwArrayStride * sizeof(uint32_t),
                            srcBinding.sta.dwArrayStride * sizeof(uint32_t));

                for (uint32_t j = 0; j < count; ++j)
                {
                    memcpy(
                        pDestAddr + j * destBinding.sta.dwArrayStride,
                        pSrcAddr  + j * srcBinding.sta.dwArrayStride,
                        arrayElementSize);
                }
            }
        }
        else if ((srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                 (srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
        {
            // Dynamic buffer descriptors reside in client memory to be read when the descriptor set is bound.
            uint32_t* pSrcAddr  = pSrcSet->DynamicDescriptorData(deviceIdx) + srcBinding.dyn.dwOffset
                                + params.srcArrayElement * srcBinding.dyn.dwArrayStride;

            uint32_t* pDestAddr = pDestSet->DynamicDescriptorData(deviceIdx) + destBinding.dyn.dwOffset
                                + params.dstArrayElement * destBinding.dyn.dwArrayStride;

            // Source and destination strides are expected to match as only copies between the same type of descriptors
            // is supported.
            VK_ASSERT(srcBinding.dyn.dwArrayStride == destBinding.dyn.dwArrayStride);

            // Just to a straight memcpy covering the entire range.
            memcpy(pDestAddr, pSrcAddr, srcBinding.dyn.dwArrayStride * sizeof(uint32_t) * count);
        }
        else if (srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
            VK_ASSERT(Util::IsPow2Aligned(params.srcArrayElement, 4));
            VK_ASSERT(Util::IsPow2Aligned(params.dstArrayElement, 4));

            // Values srcArrayElement, dstArrayElement and count are in bytes
            uint32_t* pSrcAddr  = pSrcSet->StaticCpuAddress(deviceIdx) + srcBinding.sta.dwOffset
                                + (params.srcArrayElement / 4);

            uint32_t* pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) + destBinding.sta.dwOffset
                                + (params.dstArrayElement / 4);

            // Just do a straight memcpy covering the entire range.
            memcpy(pDestAddr, pSrcAddr, count);
        }
        else
        {
            uint32_t* pSrcAddr  = pSrcSet->StaticCpuAddress(deviceIdx) + srcBinding.sta.dwOffset
                                + params.srcArrayElement * srcBinding.sta.dwArrayStride;

            uint32_t* pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) + destBinding.sta.dwOffset
                                + params.dstArrayElement * destBinding.sta.dwArrayStride;

            // Source and destination strides are expected to match as only copies between the same type of descriptors
            // is supported.
            VK_ASSERT(srcBinding.sta.dwArrayStride == destBinding.sta.dwArrayStride);

            if (hasImmutableSampler)
            {
                // If we have immutable samplers inline with the image data to copy then we have to do a per array
                // element copy to ensure we don't overwrite the immutable sampler data

                for (uint32_t j = 0; j < count; ++j)
                {
                    memcpy(pDestAddr, pSrcAddr, imageDescSize);

                    pSrcAddr  += srcBinding.sta.dwArrayStride;
                    pDestAddr += destBinding.sta.dwArrayStride;
                }
            }
            else
            {
                // Just to a straight memcpy covering the entire range.
                memcpy(pDestAddr, pSrcAddr, srcBinding.sta.dwArrayStride * sizeof(uint32_t) * count);
            }

            if ((fmaskDescSize != 0) &&
                ((srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
                 (srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                 (srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)))
            {
                uint32_t* pSrcFmaskAddr = pSrcSet->FmaskCpuAddress(deviceIdx)
                                        + srcBinding.sta.dwOffset
                                        + (params.srcArrayElement * srcBinding.sta.dwArrayStride);

                uint32_t* pDestFmaskAddr = pDestSet->FmaskCpuAddress(deviceIdx)
                                         + destBinding.sta.dwOffset
                                         + (params.dstArrayElement * destBinding.sta.dwArrayStride);

                VK_ASSERT(srcBinding.sta.dwArrayStride == destBinding.sta.dwArrayStride);

                // Copy fmask descriptors covering the entire range
                if (srcBinding.sta.dwArrayStride == fmaskDescSize / sizeof(uint32_t))
                {
                    memcpy(pDestFmaskAddr, pSrcFmaskAddr, srcBinding.sta.dwArrayStride * sizeof(uint32_t) * count);
                }
                else
                {
                    VK_ASSERT(srcBinding.sta.dwArrayStride > fmaskDescSize / sizeof(uint32_t));
                    for (uint32_t j = 0; j < count; ++j)
                    {
                        memcpy(pDestFmaskAddr, pSrcFmaskAddr, fmaskDescSize);
                        pDestFmaskAddr += srcBinding.sta.dwArrayStride;
                        pSrcFmaskAddr += srcBinding.sta.dwArrayStride;
                    }
                }
            }
        }
    }
}

// =====================================================================================================================
template <size_t imageDescSize,
          size_t fmaskDescSize,
          size_t samplerDescSize,
          size_t bufferDescSize,
          uint32_t numPalDevices>
VKAPI_ATTR void VKAPI_CALL DescriptorUpdate::UpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    descriptorCopyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
    {
        WriteDescriptorSets<imageDescSize, fmaskDescSize, samplerDescSize, bufferDescSize, numPalDevices>(
            pDevice,
            deviceIdx,
            descriptorWriteCount,
            pDescriptorWrites);

        CopyDescriptorSets<imageDescSize, fmaskDescSize, numPalDevices>(
            pDevice,
            deviceIdx,
            descriptorCopyCount,
            pDescriptorCopies);
    }
}

// =====================================================================================================================
PFN_vkUpdateDescriptorSets DescriptorUpdate::GetUpdateDescriptorSetsFunc(
    const Device* pDevice)
{
    PFN_vkUpdateDescriptorSets pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
        case 1:
            pFunc = GetUpdateDescriptorSetsFunc<1>(pDevice);
            break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        case 2:
            pFunc = GetUpdateDescriptorSetsFunc<2>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
        case 3:
            pFunc = GetUpdateDescriptorSetsFunc<3>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
        case 4:
            pFunc = GetUpdateDescriptorSetsFunc<4>(pDevice);
            break;
#endif
        default:
            break;
    }

    return pFunc;
}

// =====================================================================================================================
template <uint32_t numPalDevices>
PFN_vkUpdateDescriptorSets DescriptorUpdate::GetUpdateDescriptorSetsFunc(
    const Device* pDevice)
{
    const size_t imageDescSize      = pDevice->GetProperties().descriptorSizes.imageView;
    const size_t fmaskDescSize      = pDevice->GetProperties().descriptorSizes.fmaskView;
    const size_t samplerDescSize    = pDevice->GetProperties().descriptorSizes.sampler;
    const size_t bufferDescSize     = pDevice->GetProperties().descriptorSizes.bufferView;
    PFN_vkUpdateDescriptorSets pFunc = nullptr;

    if ((imageDescSize == 32) &&
        (samplerDescSize == 16) &&
        (bufferDescSize == 16))
    {
        if ((pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead == false) || (fmaskDescSize == 0))
        {
            pFunc = &UpdateDescriptorSets<
                32,
                0,
                16,
                16,
                numPalDevices>;
        }
        else if (fmaskDescSize == 32)
        {
            pFunc = &UpdateDescriptorSets<
                32,
                32,
                16,
                16,
                numPalDevices>;
        }
        else
        {
            VK_NEVER_CALLED();
            pFunc = nullptr;
        }
    }
    else
    {
        VK_NEVER_CALLED();
        pFunc = nullptr;
    }

    return pFunc;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    descriptorCopyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies)
{
    const Device*             pDevice          = ApiDevice::ObjectFromHandle(device);

    pDevice->GetEntryPoints().vkUpdateDescriptorSets(
        device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

} // namespace entry

// =====================================================================================================================
// Template instantiation needed for references in other files.  Linux complains if we don't do this.

template
void DescriptorUpdate::WriteFmaskDescriptors<32, 32>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteFmaskDescriptors<32, 0>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteSamplerDescriptors<16>(
    const VkDescriptorImageInfo* pDescriptors,
    uint32_t*                    pDestAddr,
    uint32_t                     count,
    uint32_t                     dwStride,
    size_t                       descriptorStrideInBytes);

template
void DescriptorUpdate::WriteImageSamplerDescriptors<32, 16>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteImageDescriptors<32, false>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteImageDescriptors<32, true>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteImageDescriptorsYcbcr<32>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

// If needed the YCbCr combined image samplers can be padded with 16 bytes to match the regular comb. img. samp.
template
void DescriptorUpdate::WriteImageDescriptorsYcbcr<32+16>(
    const VkDescriptorImageInfo*    pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferDescriptors<16, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(
    const VkBufferView*                 pDescriptors,
    uint32_t                            deviceIdx,
    uint32_t*                           pDestAddr,
    uint32_t                            count,
    uint32_t                            dwStride,
    size_t                              descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferDescriptors<16, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(
    const VkBufferView*                 pDescriptors,
    uint32_t                            deviceIdx,
    uint32_t*                           pDestAddr,
    uint32_t                            count,
    uint32_t                            dwStride,
    size_t                              descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferInfoDescriptors<16, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(
    const Device*                   pDevice,
    const VkDescriptorBufferInfo*   pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferInfoDescriptors<16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(
    const Device*                   pDevice,
    const VkDescriptorBufferInfo*   pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferInfoDescriptors<16, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(
    const Device*                   pDevice,
    const VkDescriptorBufferInfo*   pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
void DescriptorUpdate::WriteBufferInfoDescriptors<16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC>(
    const Device*                   pDevice,
    const VkDescriptorBufferInfo*   pDescriptors,
    uint32_t                        deviceIdx,
    uint32_t*                       pDestAddr,
    uint32_t                        count,
    uint32_t                        dwStride,
    size_t                          descriptorStrideInBytes);

template
DescriptorSet<1>::DescriptorSet(uint32_t heapIndex);

template
void DescriptorSet<1>::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    DescriptorAddr*             pBaseAddrs,
    void*                       pAllocHandle);

template
void DescriptorSet<1>::Reset();

template
void DescriptorSet<1>::WriteImmutableSamplers(
    uint32_t imageDescSizeInBytes);

template
DescriptorSet<2>::DescriptorSet(uint32_t heapIndex);

template
void DescriptorSet<2>::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    DescriptorAddr*             pBaseAddrs,
    void*                       pAllocHandle);

template
void DescriptorSet<2>::Reset();

template
void DescriptorSet<2>::WriteImmutableSamplers(
    uint32_t imageDescSizeInBytes);

template
DescriptorSet<3>::DescriptorSet(uint32_t heapIndex);

template
void DescriptorSet<3>::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    DescriptorAddr*             pBaseAddrs,
    void*                       pAllocHandle);

template
void DescriptorSet<3>::Reset();

template
void DescriptorSet<3>::WriteImmutableSamplers(
    uint32_t imageDescSizeInBytes);

template
DescriptorSet<4>::DescriptorSet(uint32_t heapIndex);

template
void DescriptorSet<4>::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    DescriptorAddr*             pBaseAddrs,
    void*                       pAllocHandle);

template
void DescriptorSet<4>::Reset();

template
void DescriptorSet<4>::WriteImmutableSamplers(
    uint32_t imageDescSizeInBytes);

} // namespace vk
