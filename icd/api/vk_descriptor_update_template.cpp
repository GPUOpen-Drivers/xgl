/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const Device*                                   pDevice,
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

    void* pSysMem = pAllocator->pfnAllocation(pAllocator->pUserData,
                                              objSize,
                                              VK_DEFAULT_MEM_ALIGN,
                                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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

            pEntries[ii].descriptorCount                = srcEntry.descriptorCount;
            pEntries[ii].srcOffset                      = srcEntry.offset;
            pEntries[ii].srcStride                      = srcEntry.stride;
            pEntries[ii].dstBindStaDwArrayStride        = dstBinding.sta.dwArrayStride;
            pEntries[ii].dstBindFmaskDwArrayStride      = dstBinding.fmask.dwArrayStride;
            pEntries[ii].dstBindDynDataDwArrayStride    = dstBinding.dyn.dwArrayStride;
            pEntries[ii].dstStaOffset                   =
                pLayout->GetDstStaOffset(dstBinding, srcEntry.dstArrayElement);
            pEntries[ii].dstFmaskOffset                 =
                pLayout->GetDstFmaskOffset(dstBinding, srcEntry.dstArrayElement);
            pEntries[ii].dstDynOffset                   =
                pLayout->GetDstDynOffset(dstBinding, srcEntry.dstArrayElement);
            pEntries[ii].pFunc                          =
                GetUpdateEntryFunc(pDevice, srcEntry.descriptorType, dstBinding);
        }

        VK_PLACEMENT_NEW(pSysMem) DescriptorUpdateTemplate(pCreateInfo->descriptorUpdateEntryCount);

        *pDescriptorUpdateTemplate = DescriptorUpdateTemplate::HandleFromVoidPointer(pSysMem);
    }

    return result;
}

// =====================================================================================================================
template <size_t imageDescSize, size_t samplerDescSize, size_t bufferDescSize>
DescriptorUpdateTemplate::PfnUpdateEntry DescriptorUpdateTemplate::GetUpdateEntryFunc(
    const Device*                           pDevice,
    VkDescriptorType                        descriptorType,
    const DescriptorSetLayout::BindingInfo& dstBinding)
{
    PfnUpdateEntry pFunc = NULL;

    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        pFunc = &UpdateEntrySampler<samplerDescSize>;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (dstBinding.fmask.dwSize > 0))
        {
            if (dstBinding.imm.dwSize != 0)
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, samplerDescSize, true, true>;
            }
            else
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, samplerDescSize, true, false>;
            }
        }
        else
        {
            if (dstBinding.imm.dwSize != 0)
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, samplerDescSize, false, true>;
            }
            else
            {
                pFunc = &UpdateEntryCombinedImageSampler<imageDescSize, samplerDescSize, false, false>;
            }
        }
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead && (dstBinding.fmask.dwSize > 0))
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, true>;
        }
        else
        {
            pFunc = &UpdateEntrySampledImage<imageDescSize, false>;
        }
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        pFunc = &UpdateEntryTexelBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        pFunc = &UpdateEntryTexelBuffer<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        pFunc = &UpdateEntryBuffer<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        pFunc = &UpdateEntryBuffer<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        pFunc = &UpdateEntryBuffer<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        pFunc = &UpdateEntryBuffer<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC>;
        break;
    default:
        VK_ASSERT(!"Unexpected descriptor type");
        break;
    }

    return pFunc;
}

// =====================================================================================================================
DescriptorUpdateTemplate::PfnUpdateEntry DescriptorUpdateTemplate::GetUpdateEntryFunc(
    const Device*                           pDevice,
    VkDescriptorType                        descriptorType,
    const DescriptorSetLayout::BindingInfo& dstBinding)
{
    const size_t imageDescSize      = pDevice->GetProperties().descriptorSizes.imageView;
    const size_t samplerDescSize    = pDevice->GetProperties().descriptorSizes.sampler;
    const size_t bufferDescSize     = pDevice->GetProperties().descriptorSizes.bufferView;

    DescriptorUpdateTemplate::PfnUpdateEntry pFunc = nullptr;

    if ((imageDescSize == 32) &&
        (samplerDescSize == 16) &&
        (bufferDescSize == 16))
    {
        pFunc = GetUpdateEntryFunc<32, 16, 16>(pDevice, descriptorType, dstBinding);
    }
    else
    {
        VK_NEVER_CALLED();
        pFunc = nullptr;
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
    const Device*                pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    this->~DescriptorUpdateTemplate();

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
void DescriptorUpdateTemplate::Update(
    const Device*   pDevice,
    uint32_t        deviceIdx,
    VkDescriptorSet descriptorSet,
    const void*     pData)
{
    auto pEntries = GetEntries();

    for (uint32_t i = 0; i < m_numEntries; ++i)
    {
        const void* pDescriptorInfo = Util::VoidPtrInc(pData, pEntries[i].srcOffset);

        pEntries[i].pFunc(pDevice, descriptorSet, deviceIdx, pDescriptorInfo, pEntries[i]);
    }
}

// =====================================================================================================================
template <size_t imageDescSize, size_t samplerDescSize, bool updateFmask, bool immutable>
void DescriptorUpdateTemplate::UpdateEntryCombinedImageSampler(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    uint32_t                    deviceIdx,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet* pDstSet = DescriptorSet::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t* pDestAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstStaOffset;

    if (immutable)
    {
        // If the sampler part of the combined image sampler is immutable then we should only update the image
        // descriptors, but have to make sure to still use the appropriate stride.
        DescriptorSet::WriteImageDescriptors<imageDescSize>(
            pImageInfo,
            deviceIdx,
            pDestAddr,
            entry.descriptorCount,
            entry.dstBindStaDwArrayStride,
            entry.srcStride);
    }
    else
    {
        DescriptorSet::WriteImageSamplerDescriptors<imageDescSize, samplerDescSize>(
            pImageInfo,
            deviceIdx,
            pDestAddr,
            entry.descriptorCount,
            entry.dstBindStaDwArrayStride,
            entry.srcStride);
    }

    if (updateFmask)
    {
        uint32_t* pDestFmaskAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstFmaskOffset;

        DescriptorSet::WriteFmaskDescriptors<imageDescSize>(
            pImageInfo,
            deviceIdx,
            pDestFmaskAddr,
            entry.descriptorCount,
            entry.dstBindFmaskDwArrayStride,
            entry.srcStride);
    }
}

// =====================================================================================================================
template <size_t bufferDescSize, VkDescriptorType descriptorType>
void DescriptorUpdateTemplate::UpdateEntryTexelBuffer(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    uint32_t                    deviceIdx,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet* pDstSet = DescriptorSet::ObjectFromHandle(descriptorSet);

    const VkBufferView* pTexelBufferView = static_cast<const VkBufferView*>(pDescriptorInfo);

    uint32_t* pDestAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstStaOffset;

    DescriptorSet::WriteBufferDescriptors<bufferDescSize, descriptorType>(
            pTexelBufferView,
            deviceIdx,
            pDestAddr,
            entry.descriptorCount,
            entry.dstBindStaDwArrayStride,
            entry.srcStride);
}

// =====================================================================================================================
template <VkDescriptorType descriptorType>
void DescriptorUpdateTemplate::UpdateEntryBuffer(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    uint32_t                    deviceIdx,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet* pDstSet = DescriptorSet::ObjectFromHandle(descriptorSet);

    const VkDescriptorBufferInfo* pBufferInfo = static_cast<const VkDescriptorBufferInfo*>(pDescriptorInfo);

    uint32_t* pDestAddr;
    uint32_t stride;

    if ((descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
        (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
    {
        // We need to treat dynamic buffer descriptors specially as we store the base buffer SRDs in
        // client memory.
        // NOTE: Nuke this once we have proper support for dynamic descriptors in SC.
        pDestAddr   = pDstSet->DynamicDescriptorData() + entry.dstDynOffset;
        stride      = entry.dstBindDynDataDwArrayStride;
    }
    else
    {
        pDestAddr   = pDstSet->CpuAddress(deviceIdx) + entry.dstStaOffset;
        stride      = entry.dstBindStaDwArrayStride;
    }

    DescriptorSet::WriteBufferInfoDescriptors<descriptorType>(
            pDevice,
            pBufferInfo,
            deviceIdx,
            pDestAddr,
            entry.descriptorCount,
            stride,
            entry.srcStride);
}

// =====================================================================================================================
template <size_t samplerDescSize>
void DescriptorUpdateTemplate::UpdateEntrySampler(
    const Device*               pDevice,
    VkDescriptorSet             descriptorSet,
    uint32_t                    deviceIdx,
    const void*                 pDescriptorInfo,
    const TemplateUpdateInfo&   entry)
{
    DescriptorSet* pDstSet = DescriptorSet::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t* pDestAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstStaOffset;

    DescriptorSet::WriteSamplerDescriptors<samplerDescSize>(
        pImageInfo,
        pDestAddr,
        entry.descriptorCount,
        entry.dstBindStaDwArrayStride,
        entry.srcStride);
}

// =====================================================================================================================
template <size_t imageDescSize, bool updateFmask>
void DescriptorUpdateTemplate::UpdateEntrySampledImage(
        const Device*               pDevice,
        VkDescriptorSet             descriptorSet,
        uint32_t                    deviceIdx,
        const void*                 pDescriptorInfo,
        const TemplateUpdateInfo&   entry)
{
    DescriptorSet* pDstSet = DescriptorSet::ObjectFromHandle(descriptorSet);

    const VkDescriptorImageInfo* pImageInfo = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);

    uint32_t* pDestAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstStaOffset;

    DescriptorSet::WriteImageDescriptors<imageDescSize>(
            pImageInfo,
            deviceIdx,
            pDestAddr,
            entry.descriptorCount,
            entry.dstBindStaDwArrayStride,
            entry.srcStride);

    if (updateFmask)
    {
        uint32_t* pDestFmaskAddr = pDstSet->CpuAddress(deviceIdx) + entry.dstFmaskOffset;

        DescriptorSet::WriteFmaskDescriptors<imageDescSize>(
            pImageInfo,
            deviceIdx,
            pDestFmaskAddr,
            entry.descriptorCount,
            entry.dstBindFmaskDwArrayStride,
            entry.srcStride);
    }
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

    uint32_t deviceIdx = 0;
    do
    {
        pTemplate->Update(pDevice, deviceIdx, descriptorSet, pData);
        deviceIdx++;
    }
    while (deviceIdx < pDevice->NumPalDevices());
}

} // namespace entry

} // namespace vk
