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
    const VkDescriptorUpdateTemplateCreateInfoKHR*  pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkDescriptorUpdateTemplateKHR*                  pDescriptorUpdateTemplate)
{
    VkResult     result      = VK_SUCCESS;
    const size_t apiSize     = sizeof(DescriptorUpdateTemplate);
    const size_t entriesSize = pCreateInfo->descriptorUpdateEntryCount * sizeof(VkDescriptorUpdateTemplateEntryKHR);
    const size_t objSize     = apiSize + entriesSize;

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
        VK_ASSERT(pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR);

        VkDescriptorUpdateTemplateEntryKHR* pEntries = static_cast<VkDescriptorUpdateTemplateEntryKHR*>(
                                                            Util::VoidPtrInc(pSysMem, apiSize));

        memcpy(pEntries, pCreateInfo->pDescriptorUpdateEntries, entriesSize);

        VK_PLACEMENT_NEW(pSysMem) DescriptorUpdateTemplate(pEntries, pCreateInfo->descriptorUpdateEntryCount);

        *pDescriptorUpdateTemplate = DescriptorUpdateTemplate::HandleFromVoidPointer(pSysMem);
    }

    return result;
}

// =====================================================================================================================
DescriptorUpdateTemplate::DescriptorUpdateTemplate(
    const VkDescriptorUpdateTemplateEntryKHR* pEntries,
    uint32_t                                  numEntries)
    :
    m_pEntries(pEntries),
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
    Device*         pDevice,
    uint32_t        deviceIdx,
    VkDescriptorSet descriptorSet,
    const void*     pData)
{
    const Device::Properties& deviceProperties = pDevice->GetProperties();

    // Use descriptor write structure as params to share write code path with vkUpdateDescriptorSets.
    VkWriteDescriptorSet descriptorWrite;

    descriptorWrite.sType  = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.pNext  = nullptr;
    descriptorWrite.dstSet = descriptorSet;

    for (uint32_t i = 0; i < m_numEntries; ++i)
    {
        const void* pDescriptorInfo = Util::VoidPtrInc(pData, m_pEntries[i].offset);

        descriptorWrite.dstBinding       = m_pEntries[i].dstBinding;
        descriptorWrite.dstArrayElement  = m_pEntries[i].dstArrayElement;
        descriptorWrite.descriptorCount  = m_pEntries[i].descriptorCount;
        descriptorWrite.descriptorType   = m_pEntries[i].descriptorType;
        // Decide which descriptor info is relevant later using descriptorType.
        descriptorWrite.pImageInfo       = static_cast<const VkDescriptorImageInfo*>(pDescriptorInfo);
        descriptorWrite.pBufferInfo      = static_cast<const VkDescriptorBufferInfo*>(pDescriptorInfo);
        descriptorWrite.pTexelBufferView = static_cast<const VkBufferView*>(pDescriptorInfo);

        DescriptorSet::WriteDescriptorSets(pDevice,
                                           deviceIdx,
                                           deviceProperties,
                                           1,
                                           &descriptorWrite,
                                           m_pEntries[i].stride);
    }
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplateKHR(
    VkDevice                                        device,
    VkDescriptorUpdateTemplateKHR                   descriptorUpdateTemplate,
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
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplateKHR(
    VkDevice                                        device,
    VkDescriptorSet                                 descriptorSet,
    VkDescriptorUpdateTemplateKHR                   descriptorUpdateTemplate,
    const void*                                     pData)
{
    Device*                   pDevice   = ApiDevice::ObjectFromHandle(device);
    DescriptorUpdateTemplate* pTemplate = DescriptorUpdateTemplate::ObjectFromHandle(descriptorUpdateTemplate);

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        pTemplate->Update(pDevice, deviceIdx, descriptorSet, pData);
    }
}

} // namespace entry

} // namespace vk
