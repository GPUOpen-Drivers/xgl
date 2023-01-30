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

#ifndef __VK_DEBUG_REPORT_H__
#define __VK_DEBUG_REPORT_H__

#pragma once

#include "include/vk_dispatch.h"

namespace vk
{

// =====================================================================================================================
// Vulkan implementation of VK_EXT_debug_report extension
class DebugReportCallback final : public NonDispatchable<VkDebugReportCallbackEXT, DebugReportCallback>
{
public:
    static VkResult Create(
        Instance*                                 pInstance,
        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks*              pAllocator,
        VkDebugReportCallbackEXT*                 pCallback);

    void Destroy(
        Instance*                                 pInstance,
        const VkAllocationCallbacks*              pAllocator);

    VkDebugReportFlagsEXT GetFlags();

    PFN_vkDebugReportCallbackEXT GetCallbackFunc();

    void* GetUserData();

protected:
    DebugReportCallback()
    {
    };

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DebugReportCallback)

    VkDebugReportCallbackCreateInfoEXT m_createInfo;
};

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(
    VkInstance                                instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks*              pAllocator,
    VkDebugReportCallbackEXT*                 pCallback);

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(
    VkInstance                                instance,
    VkDebugReportCallbackEXT                  callback,
    const VkAllocationCallbacks*              pAllocator);

VKAPI_ATTR void VKAPI_CALL vkDebugReportMessageEXT(
    VkInstance                                instance,
    VkDebugReportFlagsEXT                     flags,
    VkDebugReportObjectTypeEXT                objectType,
    uint64_t                                  object,
    size_t                                    location,
    int32_t                                   messageCode,
    const char*                               pLayerPrefix,
    const char*                               pMessage);
} // namespace entry

} // namespace vk

#endif /* __VK_DEBUG_REPORT_H__ */
