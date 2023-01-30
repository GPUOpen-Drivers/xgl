/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_descriptor_buffer.h
 * @brief Functionality related to Vulkan descriptor buffer.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_BUFFER_H__
#define __VK_DESCRIPTOR_BUFFER_H__

#pragma once

#include "include/khronos/vulkan.h"

namespace vk
{

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSizeEXT(
    VkDevice                                    device,
    VkDescriptorSetLayout                       layout,
    VkDeviceSize*                               pLayoutSizeInBytes);

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutBindingOffsetEXT(
    VkDevice                                    device,
    VkDescriptorSetLayout                       layout,
    uint32_t                                    binding,
    VkDeviceSize*                               pOffset);

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorEXT(
    VkDevice                                    device,
    const VkDescriptorGetInfoEXT*               pDescriptorInfo,
    size_t                                      dataSize,
    void*                                       pDescriptor);

VKAPI_ATTR VkResult VKAPI_CALL vkGetBufferOpaqueCaptureDescriptorDataEXT(
    VkDevice                                    device,
    const VkBufferCaptureDescriptorDataInfoEXT* pInfo,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkGetImageOpaqueCaptureDescriptorDataEXT(
    VkDevice                                    device,
    const VkImageCaptureDescriptorDataInfoEXT*  pInfo,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkGetImageViewOpaqueCaptureDescriptorDataEXT(
    VkDevice                                       device,
    const VkImageViewCaptureDescriptorDataInfoEXT* pInfo,
    void*                                          pData);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSamplerOpaqueCaptureDescriptorDataEXT(
    VkDevice                                     device,
    const VkSamplerCaptureDescriptorDataInfoEXT* pInfo,
    void*                                        pData);

VKAPI_ATTR VkResult VKAPI_CALL vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(
    VkDevice                                                   device,
    const VkAccelerationStructureCaptureDescriptorDataInfoEXT* pInfo,
    void*                                                      pData);

} // namespace entry

} // namespace vk

#endif /* __VK_DESCRIPTOR_BUFFER_H__ */
