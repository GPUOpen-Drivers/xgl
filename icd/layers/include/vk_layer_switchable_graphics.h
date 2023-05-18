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
 * @file  vk_layer_switchable_graphics.h
 * @brief Switchable graphics layer instance interfaces for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_LAYER_SWITCHABLE_GRAPHICS_H__
#define __VK_LAYER_SWITCHABLE_GRAPHICS_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/khronos/vk_layer.h"
#include "palHashMap.h"

namespace vk
{

struct NextLinkFuncPointers
{
    PFN_vkGetInstanceProcAddr                   pfnGetInstanceProcAddr;
    PFN_vkCreateInstance                        pfnCreateInstance;
    PFN_vkDestroyInstance                       pfnDestroyInstance;
    PFN_vkEnumeratePhysicalDevices              pfnEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties           pfnGetPhysicalDeviceProperties;
    PFN_vkEnumeratePhysicalDeviceGroups         pfnEnumeratePhysicalDeviceGroups;
    PFN_vkEnumeratePhysicalDeviceGroupsKHR      pfnEnumeratePhysicalDeviceGroupsKHR;
};

typedef Util::HashMap<VkInstance, NextLinkFuncPointers, vk::PalAllocator, Util::DefaultHashFunc, Util::DefaultEqualFunc,
    Util::HashAllocator<vk::PalAllocator>, 256> DispatchTableHashMap;

typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance_SG)(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance);

typedef void (VKAPI_PTR *PFN_vkDestroyInstance_SG)(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator);

typedef VkResult (VKAPI_PTR *PFN_vkEnumeratePhysicalDevices_SG)(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices);

typedef VkResult (VKAPI_PTR *PFN_vkEnumeratePhysicalDeviceGroups_SG)(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

typedef VkResult (VKAPI_PTR *PFN_vkEnumeratePhysicalDeviceGroupsKHR_SG)(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

typedef VkResult (VKAPI_PTR *PFN_EnumPhysDeviceGroupsFunc)(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

// Entry in a layer dispatch table of Vulkan entry points that maps a name to a function pointer implementation.
// An array of these makes up a dispatch table that represents one or more driver-internal layers's dispatch table,
// layers will expose GetIcdProcAddr interface to resolve a name to a function pointer.
struct LayerDispatchTableEntry
{
    const char*  pName;
    void*        pFunc;
};

// Helper macro used to build entries of LayerDispatchTableEntry arrays.
#define VK_LAYER_DISPATCH_ENTRY(entry_name, entry_func) \
    { \
        #entry_name, \
        reinterpret_cast<void *>(static_cast<PFN_##entry_name>(entry_func)), \
    }

// Helper macro to identify the end of a Vulkan layer dispatch table
#define VK_LAYER_DISPATCH_TABLE_END() { 0, 0 }

namespace entry
{

extern const LayerDispatchTableEntry g_LayerDispatchTable_SG[];

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance_SG(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance);

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance_SG(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroups_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroupsKHR_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

} // namespace entry
} // namespace vk

#endif /* __VK_LAYER_SWITCHABLE_GRAPHICS_H__ */
