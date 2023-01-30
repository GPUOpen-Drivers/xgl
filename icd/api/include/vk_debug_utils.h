/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_DEBUG_UTILS_H__
#define __VK_DEBUG_UTILS_H__

#pragma once

#include "include/vk_dispatch.h"

namespace vk
{

// =====================================================================================================================
// Vulkan implementation of VK_EXT_debug_utils extension
class DebugUtilsMessenger : public NonDispatchable<VkDebugUtilsMessengerEXT, DebugUtilsMessenger>
{
public:
    static VkResult Create(
        Instance*                                   pInstance,
        const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDebugUtilsMessengerEXT*                   pMessenger);

    void Destroy(
        Instance*                                   pInstance,
        const VkAllocationCallbacks*                pAllocator);

    VkDebugUtilsMessageSeverityFlagsEXT  GetMessageSeverityFlags();
    VkDebugUtilsMessageTypeFlagsEXT      GetMessageTypeFlags();

    PFN_vkDebugUtilsMessengerCallbackEXT GetCallbackFunc();

    void* GetUserData();

protected:
    DebugUtilsMessenger()
    {
    };

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DebugUtilsMessenger);

    VkDebugUtilsMessengerCreateInfoEXT m_createInfo;

};

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger);

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    VkDebugUtilsMessengerEXT                    messenger,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
    VkInstance                                  instance,
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData);
} // namespace entry

} // namespace vk

#endif /* __VK_DEBUG_UTILS_H__ */
