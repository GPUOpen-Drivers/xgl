/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_descriptor_update_template.h
 * @brief Functionality related to Vulkan descriptor update template objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_UPDATE_TEMPLATE_H__
#define __VK_DESCRIPTOR_UPDATE_TEMPLATE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"

namespace vk
{

class DescriptorSet;
class Device;

// =====================================================================================================================
// A Vulkan descriptor update template provides a way to update a descriptor set using with a pointer to user defined
// data, which describes the descriptor writes.
class DescriptorUpdateTemplate : public NonDispatchable<VkDescriptorUpdateTemplateKHR, DescriptorUpdateTemplate>
{
public:
    static VkResult Create(
        const VkDescriptorUpdateTemplateCreateInfoKHR*  pCreateInfo,
        const VkAllocationCallbacks*                    pAllocator,
        VkDescriptorUpdateTemplateKHR*                  pDescriptorUpdateTemplate);

    VkResult Destroy(
        const Device*                pDevice,
        const VkAllocationCallbacks* pAllocator);

    void Update(
        Device*         pDevice,
        uint32_t        deviceIdx,
        VkDescriptorSet descriptorSet,
        const void*     pData);

protected:
    DescriptorUpdateTemplate(
        const VkDescriptorUpdateTemplateEntryKHR* pEntries,
        uint32_t                                  numEntries);

    virtual ~DescriptorUpdateTemplate();

    const VkDescriptorUpdateTemplateEntryKHR* m_pEntries;
    uint32_t                                  m_numEntries;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplateKHR(
    VkDevice                                        device,
    VkDescriptorUpdateTemplateKHR                   descriptorUpdateTemplate,
    const VkAllocationCallbacks*                    pAllocator);

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplateKHR(
    VkDevice                                        device,
    VkDescriptorSet                                 descriptorSet,
    VkDescriptorUpdateTemplateKHR                   descriptorUpdateTemplate,
    const void*                                     pData);

} // namespace entry

} // namespace vk

#endif /* __VK_DESCRIPTOR_UPDATE_TEMPLATE_H__ */
