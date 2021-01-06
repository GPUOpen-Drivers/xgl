/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_descriptor_update_template.cpp
 * @brief Contains implementation of Vulkan descriptor update template objects.
 ***********************************************************************************************************************
 */

#include "include/vk_descriptor_set.h"
#include "include/vk_descriptor_update_template.h"
#include "include/vk_device.h"
#include "include/vk_utils.h"

namespace vk
{

// =====================================================================================================================
VkResult DescriptorUpdateTemplate::Create(
    Device*                                         pDevice,
    const VkDescriptorUpdateTemplateCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkDescriptorUpdateTemplate*                     pDescriptorUpdateTemplate)
{
    VkResult                    result      = VK_SUCCESS;
    const uint32_t              numEntries  = pCreateInfo->descriptorUpdateEntryCount;
    const DescriptorSetLayout*  pLayout     = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->descriptorSetLayout);
    const size_t                apiSize     = sizeof(DescriptorUpdateTemplate);
    const size_t                entriesSize = numEntries * sizeof(TemplateUpdateInfo);
    const size_t                objSize     = apiSize + entriesSize;

    void* pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

    if (pSysMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        // It's safe to ignore pCreateInfo.pipelineBindPoint, pCreateInfo.pipelineLayout, and pCreateInfo.set because
        // we don't support VK_KHR_push_descriptors.
        VK_ASSERT(pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET);

        TemplateUpdateInfo* pEntries = static_cast<TemplateUpdateInfo*>(Util::VoidPtrInc(pSysMem, apiSize));

        for (uint32_t ii = 0; ii < numEntries; ii++)
        {
            const VkDescriptorUpdateTemplateEntry&  srcEntry   = pCreateInfo->pDescriptorUpdateEntries[ii];
            const DescriptorSetLayout::BindingInfo& dstBinding = pLayout->Binding(srcEntry.dstBinding);
            uint32_t                                dstArrayElement;

            if (dstBinding.info.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
            {
                // Convert dstArrayElement to dword
                VK_ASSERT(Util::IsPow2Aligned(srcEntry.dstArrayElement, 4));
                dstArrayElement = srcEntry.dstArrayElement / 4;
            }
            else
            {
                dstArrayElement = srcEntry.dstArrayElement;
            }

            pEntries[ii].descriptorCount                = srcEntry.descriptorCount;
            pEntries[ii].srcOffset                      = srcEntry.offset;
            pEntries[ii].srcStride                      = srcEntry.stride;
            pEntries[ii].dstBindStaDwArrayStride        = dstBinding.sta.dwArrayStride;
            pEntries[ii].dstBindDynDataDwArrayStride    = dstBinding.dyn.dwArrayStride;

            pEntries[ii].dstStaOffset                   =
                pLayout->GetDstStaOffset(dstBinding, dstArrayElement);

            pEntries[ii].dstDynOffset                   =
                pLayout->GetDstDynOffset(dstBinding, dstArrayElement);

            pEntries[ii].pFunc                          =
                GetUpdateEntryFunc(pDevice, srcEntry.descriptorType, dstBinding);
        }

        VK_PLACEMENT_NEW(pSysMem) DescriptorUpdateTemplate(pCreateInfo->descriptorUpdateEntryCount);

        *pDescriptorUpdateTemplate = DescriptorUpdateTemplate::HandleFromVoidPointer(pSysMem);
    }

    return result;
}

// =====================================================================================================================
template <size_t imageDescSize,
          size_t fmaskDescSize,
          size_t samplerDescSize,
          size_t bufferDescSize,
          uint32_t numPalDevices>
DescriptorUpdateTemplate::PfnUpdateEntry DescriptorUpdateTemplate::GetUpdateEntryFunc(
    const Device*                           pDevice,
    VkDescriptorType                        descriptorType,
    const DescriptorSetLayout::BindingInfo& dstBinding)
{
    PfnUpdateEntry pFunc = NULL;

    switch (static_cast<uint32_t>(descriptorType))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        pFunc = &UpdateEntrySampler<samplerDescSize, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (dstBinding.sta.dwSize > 0))
        {
            if (dstBinding.imm.dwSize != 0)
            {
                if (dstBinding.bindingFlags.ycbcrConversionUsage != 0)
                {
                    pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, fmaskDescSize, samplerDescSize,
                        true, true, true, numPalDevices>;
                }
                else
                {
                    pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, fmaskDescSize, samplerDescSize,
                        true, true, false, numPalDevices>;
                }
            }
            else
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, fmaskDescSize, samplerDescSize,
                    true, false, false, numPalDevices>;
            }
        }
        else
        {
            if (dstBinding.imm.dwSize != 0)
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, fmaskDescSize, samplerDescSize,
                    false, true, false, numPalDevices>;
            }
            else
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, fmaskDescSize, samplerDescSize,
                    false, false, false, numPalDevices>;
            }
        }
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (dstBinding.sta.dwSize > 0))
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, fmaskDescSize, true, false, numPalDevices>;
        }
        else
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, fmaskDescSize, false, false, numPalDevices>;
        }
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (dstBinding.sta.dwSize > 0))
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, fmaskDescSize, true, true, numPalDevices>;
        }
        else
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, fmaskDescSize, false, true, numPalDevices>;
        }
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        pFunc = &UpdateEntryTexelBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        pFunc = &UpdateEntryTexelBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        pFunc = &UpdateEntryBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        pFunc = &UpdateEntryBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        pFunc = &UpdateEntryBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        pFunc = &UpdateEntryBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numPalDevices>;
        break;
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        pFunc = &UpdateEntryInlineUniformBlock<numPalDevices>;
        break;
    default:
        VK_ASSERT(!"Unexpected descriptor type");
        break;
    }

    return pFunc;
}

// =====================================================================================================================
template <uint32_t numPalDevices>
DescriptorUpdateTemplate::PfnUpdateEntry DescriptorUpdateTemplate::GetUpdateEntryFunc(
    const Device*                           pDevice,
    VkDescriptorType                        descriptorType,
    const DescriptorSetLayout::BindingInfo& dstBinding)
{
    const size_t imageDescSize      = pDevice->GetProperties().descriptorSizes.imageView;
    const size_t fmaskDescSize      = pDevice->GetProperties().descriptorSizes.fmaskView;
    const size_t samplerDescSize    = pDevice->GetProperties().descriptorSizes.sampler;
    const size_t bufferDescSize     = pDevice->GetProperties().descriptorSizes.bufferView;

    DescriptorUpdateTemplate::PfnUpdateEntry pFunc = nullptr;

    if ((imageDescSize == 32) &&
        (samplerDescSize == 16) &&
        (bufferDescSize == 16))

    {
        if (fmaskDescSize == 32)
        {
            pFunc = GetUpdateEntryFunc<
                32,
                32,
                16,
                16,
                numPalDevices>(pDevice, descriptorType, dstBinding);
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

// =====================================================================================================================
DescriptorUpdateTemplate::PfnUpdateEntry DescriptorUpdateTemplate::GetUpdateEntryFunc(
    const Device*                           pDevice,
    VkDescriptorType                        descriptorType,
    const DescriptorSetLayout::BindingInfo& dstBinding)
{
    DescriptorUpdateTemplate::PfnUpdateEntry pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
        case 1:
            pFunc = GetUpdateEntryFunc<1>(pDevice, descriptorType, dstBinding);
            break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        case 2:
            pFunc = GetUpdateEntryFunc<2>(pDevice, descriptorType, dstBinding);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
        case 3:
            pFunc = GetUpdateEntryFunc<3>(pDevice, descriptorType, dstBinding);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
        case 4:
            pFunc = GetUpdateEntryFunc<4>(pDevice, descriptorType, dstBinding);
            break;
#endif
        default:
            VK_NEVER_CALLED();
            pFunc = nullptr;
            break;
    }

    return pFunc;
}

// =====================================================================================================================
DescriptorUpdateTemplate::DescriptorUpdateTemplate(
    uint32_t                    numEntries)
    :
    m_numEntries(numEntries)
{
}

// =====================================================================================================================
DescriptorUpdateTemplate::~DescriptorUpdateTemplate()
{
}

// =====================================================================================================================
VkResult DescriptorUpdateTemplate::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    this->~DescriptorUpdateTemplate();

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
void DescriptorUpdateTemplate::Update(
    const Device*   pDevice,
    VkDescriptorSet descriptorSet,
    const void*     pData)
{
    auto pEntries = GetEntries();

    for (uint32_t i = 0; i < m_numEntries; ++i)
    {
        const void* pDescriptorInfo = Util::VoidPtrInc(pData, pEntries[i].srcOffset);

        pEntries[i].pFunc(pDevice, descriptorSet, pDescriptorInfo, pEntries[i]);
    }
}

// =====================================================================================================================
template <size_t imageDescSize, size_t fmaskDescSize, size_t samplerDescSize, bool updateFmask, bool immutable,
    bool ycbcrUsage, uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntryCombinedImageSampler(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;

        if (immutable)
        {
            if (ycbcrUsage == false)
            {
                // If the sampler part of the combined image sampler is immutable then we should only update the image
                // descriptors, but have to make sure to still use the appropriate stride.
                DescriptorUpdate::WriteImageDescriptors<imageDescSize, false>(
                    pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    entry.descriptorCount,
                    entry.dstBindStaDwArrayStride,
                    entry.srcStride);
            }
            else
            {
                DescriptorUpdate::WriteImageDescriptorsYcbcr<imageDescSize>(
                    pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    entry.descriptorCount,
                    entry.dstBindStaDwArrayStride,
                    entry.srcStride);
            }
        }
        else
        {
            DescriptorUpdate::WriteImageSamplerDescriptors<imageDescSize, samplerDescSize>(
                pImageInfo,
                deviceIdx,
                pDestAddr,
                entry.descriptorCount,
                entry.dstBindStaDwArrayStride,
                entry.srcStride);
        }

        if (updateFmask)
        {
            uint32_t* pDestFmaskAddr = pDstSet->FmaskCpuAddress(deviceIdx) + entry.dstStaOffset;

            DescriptorUpdate::WriteFmaskDescriptors<imageDescSize, fmaskDescSize>(
                pImageInfo,
                deviceIdx,
                pDestFmaskAddr,
                entry.descriptorCount,
                entry.dstBindStaDwArrayStride,
                entry.srcStride);
        }

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
template <size_t bufferDescSize, VkDescriptorType descriptorType, uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntryTexelBuffer(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const VkBufferView* pTexelBufferView = static_cast<const VkBufferView*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;

        DescriptorUpdate::WriteBufferDescriptors<bufferDescSize, descriptorType>(
                pTexelBufferView,
                deviceIdx,
                pDestAddr,
                entry.descriptorCount,
                entry.dstBindStaDwArrayStride,
                entry.srcStride);

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
template <size_t bufferDescSize, VkDescriptorType descriptorType, uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntryBuffer(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const VkDescriptorBufferInfo* pBufferInfo = static_cast<const VkDescriptorBufferInfo*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr;
        uint32_t stride;

        if ((descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
            (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
        {
            // We need to treat dynamic buffer descriptors specially as we store the base buffer SRDs in
            // client memory.
            // NOTE: Nuke this once we have proper support for dynamic descriptors in SC.
            pDestAddr   = pDstSet->DynamicDescriptorData(deviceIdx) + entry.dstDynOffset;
            stride      = entry.dstBindDynDataDwArrayStride;
        }
        else
        {
            pDestAddr   = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;
            stride      = entry.dstBindStaDwArrayStride;
        }

        DescriptorUpdate::WriteBufferInfoDescriptors<bufferDescSize, descriptorType>(
                pDevice,
                pBufferInfo,
                deviceIdx,
                pDestAddr,
                entry.descriptorCount,
                stride,
                entry.srcStride);

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
template <size_t samplerDescSize, uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntrySampler(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;

        DescriptorUpdate::WriteSamplerDescriptors<samplerDescSize>(
            pImageInfo,
            pDestAddr,
            entry.descriptorCount,
            entry.dstBindStaDwArrayStride,
            entry.srcStride);

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
template <size_t imageDescSize, size_t fmaskDescSize, bool updateFmask, bool isShaderStorageDesc,
    uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntrySampledImage(
        const Device*               pDevice,
        VkDescriptorSet             descriptorSet,
        const void*                 pDescriptorInfo,
        const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;

        DescriptorUpdate::WriteImageDescriptors<imageDescSize, isShaderStorageDesc>(
                pImageInfo,
                deviceIdx,
                pDestAddr,
                entry.descriptorCount,
                entry.dstBindStaDwArrayStride,
                entry.srcStride);

         if (updateFmask)
         {
             uint32_t* pDestFmaskAddr = pDstSet->FmaskCpuAddress(deviceIdx) + entry.dstStaOffset;

             DescriptorUpdate::WriteFmaskDescriptors<imageDescSize, fmaskDescSize>(
                 pImageInfo,
                 deviceIdx,
                 pDestFmaskAddr,
                 entry.descriptorCount,
                 entry.dstBindStaDwArrayStride,
                 entry.srcStride);
         }

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
void DescriptorUpdateTemplate::UpdateEntryInlineUniformBlock(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet<numPalDevices>* pDstSet = DescriptorSet<numPalDevices>::ObjectFromHandle(descriptorSet);

    const uint8_t* pData = static_cast<const uint8_t*>(pDescriptorInfo);

    uint32_t deviceIdx = 0;

    do
    {
        uint32_t* pDestAddr = pDstSet->StaticCpuAddress(deviceIdx) + entry.dstStaOffset;

        DescriptorUpdate::WriteInlineUniformBlock(
            pData,
            pDestAddr,
            entry.descriptorCount,
            0
            );

        deviceIdx++;
    } while (deviceIdx < numPalDevices);
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplate(
    VkDevice                                        device,
    VkDescriptorUpdateTemplate                      descriptorUpdateTemplate,
    const VkAllocationCallbacks*                    pAllocator)
{
    if (descriptorUpdateTemplate != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        DescriptorUpdateTemplate::ObjectFromHandle(descriptorUpdateTemplate)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplate(
    VkDevice                                        device,
    VkDescriptorSet                                 descriptorSet,
    VkDescriptorUpdateTemplate                      descriptorUpdateTemplate,
    const void*                                     pData)
{
    Device*                   pDevice   = ApiDevice::ObjectFromHandle(device);
    DescriptorUpdateTemplate* pTemplate = DescriptorUpdateTemplate::ObjectFromHandle(descriptorUpdateTemplate);

    pTemplate->Update(pDevice, descriptorSet, pData);
}

} // namespace entry

} // namespace vk
