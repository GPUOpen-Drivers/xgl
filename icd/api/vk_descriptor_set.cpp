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

namespace vk
{

// =====================================================================================================================
DescriptorSet::DescriptorSet(
    DescriptorPool*    pPool,
    uint32_t           heapIndex,
    DescriptorSetFlags flags)
    :
    m_pLayout(nullptr),
    m_pAllocHandle(nullptr),
    m_pPool(pPool),
    m_heapIndex(heapIndex),
    m_flags(flags)
{
    memset(m_gpuAddress, 0, sizeof(m_gpuAddress));
    memset(m_pCpuAddress, 0, sizeof(m_pCpuAddress));
}

// =====================================================================================================================
VkResult DescriptorSet::Destroy(Device* pDevice)
{
    VkDescriptorSet set = DescriptorSet::HandleFromObject(this);

    return m_pPool->FreeDescriptorSets(1, &set);
}

// =====================================================================================================================
// Assigns a GPU range and layout to a descriptor set on allocation.  This is called from a descriptor pool when it
// allocates memory for this set during vkAllocDescriptorSets.
//
// NOTE: The given handle's value may be modified by this function.
void DescriptorSet::Reassign(
    const DescriptorSetLayout*  pLayout,
    Pal::gpusize                gpuMemOffset,
    Pal::gpusize*               gpuBaseAddress,
    uint32_t**                  cpuBaseAddress,
    uint32_t                    numPalDevices,
    const InternalMemory* const pInternalMem,
    void*                       pAllocHandle,
    VkDescriptorSet*            pHandle)
{
    m_pLayout = pLayout;
    m_pAllocHandle = pAllocHandle;

    if (pInternalMem != nullptr)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
        {
            m_gpuAddress[deviceIdx] = gpuBaseAddress[deviceIdx] + gpuMemOffset;
        }
    }

    if (pHandle != nullptr)
    {
        if (pInternalMem != nullptr)
        {
            // When memory is assigned to this descriptor set let's cache its mapped CPU address as we anyways use
            // persistent mapped memory for descriptor pools.
            for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
            {
                m_pCpuAddress[deviceIdx] = static_cast<uint32_t*>(Util::VoidPtrInc(cpuBaseAddress[deviceIdx], static_cast<intptr_t>(gpuMemOffset)));
                VK_ASSERT(Util::IsPow2Aligned(reinterpret_cast<uint64_t>(m_pCpuAddress[deviceIdx]), sizeof(uint32_t)));
            }

            // In this case we also have to copy the immutable sampler data from the descriptor set layout to the
            // descriptor set's appropriate memory locations.
            InitImmutableDescriptors(numPalDevices);
        }
        else
        {
            // This path can only be hit if the set doesn't need GPU memory
            // i.e. it doesn't have static section and fmask section data
            VK_ASSERT((pLayout->Info().sta.dwSize + pLayout->Info().fmask.dwSize) == 0);

            memset(m_pCpuAddress, 0, sizeof(m_pCpuAddress[0]) * numPalDevices);
        }
    }
    else
    {
        memset(m_pCpuAddress, 0, sizeof(m_pCpuAddress[0]) * numPalDevices);
    }

}

// =====================================================================================================================
// Initialize immutable descriptor data in the descriptor set.
void DescriptorSet::InitImmutableDescriptors(uint32_t numPalDevices)
{
    const size_t imageDescDwSize = m_pLayout->VkDevice()->GetProperties().descriptorSizes.imageView / sizeof(uint32_t);
    const size_t samplerDescSize = m_pLayout->VkDevice()->GetProperties().descriptorSizes.sampler;

    uint32_t immutableSamplersLeft = m_pLayout->Info().imm.numImmutableSamplers;
    uint32_t binding = 0;

    uint32_t* pSrcData  = m_pLayout->Info().imm.pImmutableSamplerData;

    while (immutableSamplersLeft > 0)
    {
        const DescriptorSetLayout::BindingInfo& bindingInfo = m_pLayout->Info().bindings[binding];
        uint32_t desCount = bindingInfo.info.descriptorCount;

        if (bindingInfo.imm.dwSize > 0)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
            {
                if (bindingInfo.info.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                {
                    // If it's a pure immutable sampler descriptor binding then we can copy all descriptors in one shot.
                    memcpy(m_pCpuAddress[deviceIdx] + bindingInfo.sta.dwOffset,
                           pSrcData  + bindingInfo.imm.dwOffset,
                           bindingInfo.imm.dwSize * sizeof(uint32_t));
                }
                else
                {
                    // Otherwise, if it's a combined image sampler descriptor with immutable sampler then we have to
                    // copy each element individually because the source and destination strides don't match.
                    VK_ASSERT(bindingInfo.info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                    for (uint32_t i = 0; i < desCount; ++i)
                    {
                        memcpy(m_pCpuAddress[deviceIdx] + bindingInfo.sta.dwOffset +
                                                                (i * bindingInfo.sta.dwArrayStride) + imageDescDwSize,
                               pSrcData + bindingInfo.imm.dwOffset + (i * bindingInfo.imm.dwArrayStride),
                               samplerDescSize);
                    }
                }
            }
            // Update the remaining number of immutable samplers to copy.
            immutableSamplersLeft -= desCount;
        }

        binding++;
    }
}

// =====================================================================================================================
// Write sampler descriptors
VK_INLINE void DescriptorSet::WriteSamplerDescriptors(
    const Device::Properties&    deviceProperties,
    const VkDescriptorImageInfo* pDescriptors,
    uint32_t*                    pDestAddr,
    uint32_t                     count,
    uint32_t                     dwStride,
    size_t                       descriptorStrideInBytes)
{
    const VkDescriptorImageInfo* pImageInfo      = pDescriptors;
    const size_t                 imageInfoStride = (descriptorStrideInBytes != 0) ? descriptorStrideInBytes :
                                                                                    sizeof(VkDescriptorImageInfo);
    const size_t                 samplerDescSize = deviceProperties.descriptorSizes.sampler;

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        const void* pSamplerDesc = Sampler::ObjectFromHandle(pImageInfo->sampler)->Descriptor();

        memcpy(pDestAddr, pSamplerDesc, samplerDescSize);

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write combined image-sampler descriptors
VK_INLINE void DescriptorSet::WriteImageSamplerDescriptors(
    const Device::Properties&       deviceProperties,
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
    const size_t                 imageDescSize   = deviceProperties.descriptorSizes.imageView;
    const size_t                 samplerDescSize = deviceProperties.descriptorSizes.sampler;

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        const void* pImageDesc      = ImageView::ObjectFromHandle(pImageInfo->imageView)->
                                          Descriptor(pImageInfo->imageLayout, deviceIdx, imageDescSize);
        const void* pSamplerDesc    = Sampler::ObjectFromHandle(pImageInfo->sampler)->Descriptor();

        memcpy(pDestAddr, pImageDesc, imageDescSize);
        memcpy(pDestAddr + (imageDescSize / sizeof(uint32_t)), pSamplerDesc, samplerDescSize);

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write image view descriptors (including input attachments)
VK_INLINE void DescriptorSet::WriteImageDescriptors(
    VkDescriptorType                descType,
    const Device::Properties&       deviceProperties,
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
    const size_t                 imageDescSize   = deviceProperties.descriptorSizes.imageView;

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        const void* pImageDesc = ImageView::ObjectFromHandle(pImageInfo->imageView)->
                                        Descriptor(pImageInfo->imageLayout, deviceIdx, imageDescSize);

        memcpy(pDestAddr, pImageDesc, imageDescSize);

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write fmask descriptors
VK_INLINE void DescriptorSet::WriteFmaskDescriptors(
    const Device*                   pDevice,
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
    const size_t                 imageDescSize   = pDevice->GetProperties().descriptorSizes.imageView;
    VK_ASSERT((pDevice->GetProperties().descriptorSizes.fmaskView / sizeof(uint32_t)) == dwStride);

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        const ImageView* const pImageView = ImageView::ObjectFromHandle(pImageInfo->imageView);
        const void*            pImageDesc = pImageView->Descriptor(pImageInfo->imageLayout, deviceIdx, 0);

        VK_ASSERT(FmaskBasedMsaaReadEnabled() == true);

        if (pImageView->NeedsFmaskViewSrds())
        {
            // Copy over FMASK descriptor
            // Image descriptors including shader read and write descriptors.
            const void* pSrcFmaskAddr = Util::VoidPtrInc(pImageDesc, imageDescSize * 2);

            memcpy(pDestAddr, pSrcFmaskAddr, dwStride * sizeof(uint32_t));
        }
        else
        {
            // If no FMASK descriptor, need clear the memory to 0.
            memset(pDestAddr, 0, dwStride * sizeof(uint32_t));
        }

        pImageInfo = static_cast<const VkDescriptorImageInfo*>(Util::VoidPtrInc(pImageInfo, imageInfoStride));
    }
}

// =====================================================================================================================
// Write buffer descriptors
VK_INLINE void DescriptorSet::WriteBufferDescriptors(
    const Device::Properties&           deviceProperties,
    VkDescriptorType                    type,
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
    const size_t        bufferDescSize   = deviceProperties.descriptorSizes.bufferView;

    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        const void* pBufferDesc = BufferView::ObjectFromHandle(*pBufferView)->Descriptor(type, deviceIdx);

        memcpy(pDestAddr, pBufferDesc, bufferDescSize);

        pBufferView = static_cast<const VkBufferView*>(Util::VoidPtrInc(pBufferView, bufferViewStride));
    }
}

// =====================================================================================================================
// Write buffer descriptors using bufferInfo field used with uniform and storage buffers
VK_INLINE void DescriptorSet::WriteBufferInfoDescriptors(
    const Device*                   pDevice,
    VkDescriptorType                type,
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

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        // Setup and create SRD for storage buffer case
        info.swizzledFormat = Pal::UndefinedSwizzledFormat;
        info.stride         = 0; // Raw buffers have a zero byte stride
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

    // Build the SRD
    for (uint32_t arrayElem = 0; arrayElem < count; ++arrayElem, pDestAddr += dwStride)
    {
        info.gpuAddr = Buffer::ObjectFromHandle(pBufferInfo->buffer)->GpuVirtAddr(deviceIdx) + pBufferInfo->offset;

        if ((pDevice->GetEnabledFeatures().robustBufferAccess == false) &&
            ((type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)        ||
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

            pPalDevice->CreateUntypedBufferViewSrds(1, &info, pDestAddr);
        }

        pBufferInfo = static_cast<const VkDescriptorBufferInfo*>(Util::VoidPtrInc(pBufferInfo, bufferInfoStride));
    }
}

// =====================================================================================================================
// Write to descriptor sets using the provided descriptors for resources
//
// NOTE: descriptorStrideInBytes is used for VK_KHR_descriptor_update_template's sparsely packed imageInfo, bufferInfo,
//       or bufferView array elements and defaults to 0, i.e. vkUpdateDescriptorSets behavior
void DescriptorSet::WriteDescriptorSets(
    const Device*                pDevice,
    uint32_t                     deviceIdx,
    const Device::Properties&    deviceProperties,
    uint32_t                     descriptorWriteCount,
    const VkWriteDescriptorSet*  pDescriptorWrites,
    size_t                       descriptorStrideInBytes)
{
    for (uint32_t i = 0; i < descriptorWriteCount; ++i)
    {
        const VkWriteDescriptorSet& params = pDescriptorWrites[i];

        VK_ASSERT(params.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        VK_ASSERT(params.pNext == nullptr);

        DescriptorSet* pDestSet  = DescriptorSet::ObjectFromHandle(params.dstSet);
        const DescriptorSetLayout::BindingInfo& destBinding = pDestSet->Layout()->Binding(params.dstBinding);
        uint32_t* pDestAddr = pDestSet->CpuAddress(deviceIdx) + destBinding.sta.dwOffset
                            + (params.dstArrayElement * destBinding.sta.dwArrayStride);

        uint32_t* pDestFmaskAddr = pDestSet->CpuAddress(deviceIdx) + pDestSet->Layout()->Info().sta.dwSize
                                 + destBinding.fmask.dwOffset + (params.dstArrayElement * destBinding.fmask.dwArrayStride);

        // Determine whether the binding has immutable sampler descriptors.
        bool hasImmutableSampler = (destBinding.imm.dwSize != 0);

        switch (params.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            if (hasImmutableSampler)
            {
                VK_ASSERT(!"Immutable samplers cannot be updated");
            }
            else
            {
                pDestSet->WriteSamplerDescriptors(
                    deviceProperties,
                    params.pImageInfo,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride,
                    descriptorStrideInBytes);
            }
            break;

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if (hasImmutableSampler)
            {
                // If the sampler part of the combined image sampler is immutable then we should only update the image
                // descriptors, but have to make sure to still use the appropriate stride.
                pDestSet->WriteImageDescriptors(
                    params.descriptorType,
                    deviceProperties,
                    params.pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride,
                    descriptorStrideInBytes);
            }
            else
            {
                pDestSet->WriteImageSamplerDescriptors(
                    deviceProperties,
                    params.pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride,
                    descriptorStrideInBytes);
            }

            if (pDestSet->FmaskBasedMsaaReadEnabled() && (destBinding.fmask.dwSize > 0))
            {
                pDestSet->WriteFmaskDescriptors(
                    pDevice,
                    params.pImageInfo,
                    deviceIdx,
                    pDestFmaskAddr,
                    params.descriptorCount,
                    destBinding.fmask.dwArrayStride,
                    descriptorStrideInBytes);
            }

            break;

        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            pDestSet->WriteImageDescriptors(
                params.descriptorType,
                deviceProperties,
                params.pImageInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride,
                descriptorStrideInBytes);

            if (pDestSet->FmaskBasedMsaaReadEnabled() && (destBinding.fmask.dwSize > 0))
            {
                pDestSet->WriteFmaskDescriptors(
                    pDevice,
                    params.pImageInfo,
                    deviceIdx,
                    pDestFmaskAddr,
                    params.descriptorCount,
                    destBinding.fmask.dwArrayStride,
                    descriptorStrideInBytes);
            }
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            pDestSet->WriteBufferDescriptors(
                deviceProperties,
                params.descriptorType,
                params.pTexelBufferView,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride,
                descriptorStrideInBytes);

            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            pDestSet->WriteBufferInfoDescriptors(
                pDevice,
                params.descriptorType,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.sta.dwArrayStride,
                descriptorStrideInBytes);

            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            // We need to treat dynamic buffer descriptors specially as we store the base buffer SRDs in
            // client memory.
            // NOTE: Nuke this once we have proper support for dynamic descriptors in SC.
            pDestAddr = pDestSet->DynamicDescriptorData() + destBinding.dyn.dwOffset
                      + params.dstArrayElement * destBinding.dyn.dwArrayStride;

            pDestSet->WriteBufferInfoDescriptors(
                pDevice,
                params.descriptorType,
                params.pBufferInfo,
                deviceIdx,
                pDestAddr,
                params.descriptorCount,
                destBinding.dyn.dwArrayStride,
                descriptorStrideInBytes);

            break;

        default:
            VK_ASSERT(!"Unexpected descriptor type");
            break;
        }
    }
}

// =====================================================================================================================
// Copy from one descriptor set to another
void DescriptorSet::CopyDescriptorSets(
    const Device*                pDevice,
    uint32_t                     deviceIdx,
    const Device::Properties&    deviceProperties,
    uint32_t                     descriptorCopyCount,
    const VkCopyDescriptorSet*   pDescriptorCopies)
{
    for (uint32_t i = 0; i < descriptorCopyCount; ++i)
    {
        const VkCopyDescriptorSet& params = pDescriptorCopies[i];
        uint32_t count = params.descriptorCount;

        VK_ASSERT(params.sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
        VK_ASSERT(params.pNext == nullptr);

        DescriptorSet* pSrcSet  = DescriptorSet::ObjectFromHandle(params.srcSet);
        DescriptorSet* pDestSet = DescriptorSet::ObjectFromHandle(params.dstSet);

        const DescriptorSetLayout::BindingInfo& srcBinding  = pSrcSet->Layout()->Binding(params.srcBinding);
        const DescriptorSetLayout::BindingInfo& destBinding = pDestSet->Layout()->Binding(params.dstBinding);

        // Determine whether the bindings have immutable sampler descriptors. If one has both must.
        VK_ASSERT((srcBinding.imm.dwSize != 0) == (destBinding.imm.dwSize != 0));
        bool hasImmutableSampler  = (destBinding.imm.dwSize != 0);

        // Source and destination descriptor types are expected to match.
        VK_ASSERT(srcBinding.info.descriptorType == destBinding.info.descriptorType);

        // Cannot copy between sampler descriptors that are immutable and thus don't have any mutable portion
        VK_ASSERT((hasImmutableSampler == false) || (srcBinding.info.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER));

        if ((srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
            (srcBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
        {
            // We need to treat dynamic buffer descriptors specially as we store the base buffer SRDs in
            // client memory.
            // NOTE: Nuke this once we have proper support for dynamic descriptors in SC.
            uint32_t* pSrcAddr  = pSrcSet->DynamicDescriptorData() + srcBinding.dyn.dwOffset
                                + params.srcArrayElement * srcBinding.dyn.dwArrayStride;

            uint32_t* pDestAddr = pDestSet->DynamicDescriptorData() + destBinding.dyn.dwOffset
                                + params.dstArrayElement * destBinding.dyn.dwArrayStride;

            // Source and destination strides are expected to match as only copies between the same type of descriptors
            // is supported.
            VK_ASSERT(srcBinding.dyn.dwArrayStride == destBinding.dyn.dwArrayStride);

            // Just to a straight memcpy covering the entire range.
            memcpy(pDestAddr, pSrcAddr, srcBinding.dyn.dwArrayStride * sizeof(uint32_t) * count);
        }
        else
        {
            uint32_t* pSrcAddr  = pSrcSet->CpuAddress(deviceIdx) + srcBinding.sta.dwOffset
                                + params.srcArrayElement * srcBinding.sta.dwArrayStride;

            uint32_t* pDestAddr = pDestSet->CpuAddress(deviceIdx) + destBinding.sta.dwOffset
                                + params.dstArrayElement * destBinding.sta.dwArrayStride;

            // Source and destination strides are expected to match as only copies between the same type of descriptors
            // is supported.
            VK_ASSERT(srcBinding.sta.dwArrayStride == destBinding.sta.dwArrayStride);

            if (hasImmutableSampler)
            {
                // If we have immutable samplers inline with the image data to copy then we have to do a per array
                // element copy to ensure we don't overwrite the immutable sampler data
                const size_t imageDescSize = deviceProperties.descriptorSizes.imageView;

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

            if (pSrcSet->FmaskBasedMsaaReadEnabled() && srcBinding.fmask.dwSize > 0)
            {
                uint32_t* pSrcFmaskAddr  = pSrcSet->CpuAddress(deviceIdx) + pSrcSet->Layout()->Info().sta.dwSize
                                         + srcBinding.fmask.dwOffset + params.srcArrayElement * srcBinding.fmask.dwArrayStride;

                uint32_t* pDestFmaskAddr = pDestSet->CpuAddress(deviceIdx) + pDestSet->Layout()->Info().sta.dwSize
                                         + destBinding.fmask.dwOffset + params.dstArrayElement * destBinding.fmask.dwArrayStride;

                VK_ASSERT(srcBinding.fmask.dwArrayStride == destBinding.fmask.dwArrayStride);

                // Copy fmask descriptors covering the entire range
                memcpy(pDestFmaskAddr, pSrcFmaskAddr, srcBinding.fmask.dwArrayStride * sizeof(uint32_t) * count);
            }
        }
    }
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
    const Device::Properties& deviceProperties = pDevice->GetProperties();

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        DescriptorSet::WriteDescriptorSets(pDevice,
                                           deviceIdx,
                                           deviceProperties,
                                           descriptorWriteCount,
                                           pDescriptorWrites);

        DescriptorSet::CopyDescriptorSets(pDevice,
                                          deviceIdx,
                                          deviceProperties,
                                          descriptorCopyCount,
                                          pDescriptorCopies);
    }
}

} // namespace entry

} // namespace vk
