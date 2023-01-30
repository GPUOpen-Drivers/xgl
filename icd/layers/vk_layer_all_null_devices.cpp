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
/**
 ***********************************************************************************************************************
 * @file  vk_layer_all_null_devices.cpp
 * @brief When the AMDVLK_NULL_GPU=ALL environment variable is set, this layer will provide a list of all possible
 *        NULL devices to the application. No physical device objects will be exposed to the application because there
 *        is a limit on the number of physical device objects that can be created. VkPhysicalDeviceProperties pointers
 *        are exposed through the VkPhysicalDevice handles in vkEnumeratePhysicalDevices_ND so that
 *        vkGetPhysicalDeviceProperties_ND can expose the properties for the appropriate NULL device.
 ***********************************************************************************************************************
 */

#include "include/internal_layer_hooks.h"

#include "include/vk_dispatch.h"
#include "include/vk_instance.h"
#include "include/vk_physical_device_manager.h"

#include "include/khronos/vulkan.h"

namespace vk
{

namespace entry
{

// =====================================================================================================================
// Layer's implementation for vkEnumeratePhysicalDevices, which generates VkPhysicalDeviceProperties per NULL device
// and stores their pointers inside the VkPhysicalDevice handles
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices_ND(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
    VK_ASSERT(pPhysicalDeviceCount != nullptr);

    // Store the VkPhysicalDeviceProperties pointers in place of the VkPhysicalDevice handles
    return Instance::ObjectFromHandle(instance)->EnumerateAllNullPhysicalDeviceProperties(
        pPhysicalDeviceCount,
        reinterpret_cast<VkPhysicalDeviceProperties**>(pPhysicalDevices));
}

// =====================================================================================================================
// Layer's implementation for vkGetPhysicalDeviceProperties, which retrieves VkPhysicalDeviceProperties from the
// VkPhysicalDevice handle
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
    VK_ASSERT(pProperties != nullptr);

    if (pProperties != nullptr)
    {
        // VkPhysicalDevice handle is actaully a VkPhysicalDeviceProperties* (per vkEnumeratePhysicalDevices_ND)
        *pProperties = *reinterpret_cast<VkPhysicalDeviceProperties*>(physicalDevice);
    }
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice_ND(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice)
{
    VK_NEVER_CALLED();
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
    VK_NEVER_CALLED();
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
    VK_NEVER_CALLED();

    if (pQueueFamilyPropertyCount != nullptr)
    {
        *pQueueFamilyPropertyCount = 0u;
    }
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkSampleCountFlagBits                       samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pPropertyCount,
    VkSparseImageFormatProperties*              pProperties)
{
    VK_NEVER_CALLED();

    if (pPropertyCount != nullptr)
    {
        *pPropertyCount = 0u;
    }
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroups_ND(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties)
{
    VK_NEVER_CALLED();

    if (pPhysicalDeviceGroupCount != nullptr)
    {
        *pPhysicalDeviceGroupCount = 0u;
    }

    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalBufferProperties_ND(
    VkPhysicalDevice                                physicalDevice,
    const VkPhysicalDeviceExternalBufferInfo*       pExternalBufferInfo,
    VkExternalBufferProperties*                     pExternalBufferProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFenceProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo*    pExternalFenceInfo,
    VkExternalFenceProperties*                  pExternalFenceProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphoreProperties_ND(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo*    pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties*                  pExternalSemaphoreProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties_ND(
    VkPhysicalDevice                            physicalDevice,
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties)
{
    VK_NEVER_CALLED();

    if (pPropertyCount != nullptr)
    {
        *pPropertyCount = 0u;
    }

    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties2*                pProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures2*                  pFeatures)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2_ND(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties2*                        pFormatProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2_ND(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2*     pImageFormatInfo,
    VkImageFormatProperties2*                   pImageFormatProperties)
{
    VK_NEVER_CALLED();
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2_ND(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties2*          pMemoryProperties)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2_ND(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2*                   pQueueFamilyProperties)
{
    VK_NEVER_CALLED();

    if (pQueueFamilyPropertyCount != nullptr)
    {
        *pQueueFamilyPropertyCount = 0u;
    }
}

// =====================================================================================================================
// Stub function
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2_ND(
    VkPhysicalDevice                                    physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2*       pFormatInfo,
    uint32_t*                                           pPropertyCount,
    VkSparseImageFormatProperties2*                     pProperties)
{
    VK_NEVER_CALLED();

    if (pPropertyCount != nullptr)
    {
        *pPropertyCount = 0u;
    }
}

} // namespace entry

#define OVERRIDE_ALIAS_ND(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::func_name

void OverrideDispatchTable_ND(DispatchTable* pDispatchTable)
{
    OVERRIDE_ALIAS_ND(vkEnumeratePhysicalDevices,    vkEnumeratePhysicalDevices_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceProperties_ND);
    OVERRIDE_ALIAS_ND(vkCreateDevice, vkCreateDevice_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceFeatures, vkGetPhysicalDeviceFeatures_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceFormatProperties, vkGetPhysicalDeviceFormatProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceImageFormatProperties, vkGetPhysicalDeviceImageFormatProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceSparseImageFormatProperties, vkGetPhysicalDeviceSparseImageFormatProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceExternalBufferProperties, vkGetPhysicalDeviceExternalBufferProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceExternalFenceProperties, vkGetPhysicalDeviceExternalFenceProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceExternalSemaphoreProperties, vkGetPhysicalDeviceExternalSemaphoreProperties_ND);
    OVERRIDE_ALIAS_ND(vkEnumerateDeviceExtensionProperties, vkEnumerateDeviceExtensionProperties_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceProperties2, vkGetPhysicalDeviceProperties2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceFeatures2, vkGetPhysicalDeviceFeatures2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceFormatProperties2, vkGetPhysicalDeviceFormatProperties2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceImageFormatProperties2, vkGetPhysicalDeviceImageFormatProperties2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceMemoryProperties2, vkGetPhysicalDeviceMemoryProperties2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceQueueFamilyProperties2, vkGetPhysicalDeviceQueueFamilyProperties2_ND);
    OVERRIDE_ALIAS_ND(vkGetPhysicalDeviceSparseImageFormatProperties2, vkGetPhysicalDeviceSparseImageFormatProperties2_ND);
}

} // namespace vk
