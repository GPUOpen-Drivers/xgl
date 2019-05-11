/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 **********************************************************************************************************************
 * @file  vk_amd_device_coherent_memory.h
 * @brief Temporary internal header for wave limit control. Should be removed once the extension is published
 *        and the API gets included in the official Vulkan header.
 **********************************************************************************************************************
 */
#ifndef VKI_AMD_DEVICE_COHERENT_MEMORY_H_
#define VKI_AMD_DEVICE_COHERENT_MEMORY_H_

#include "vk_internal_ext_helper.h"

#define VK_AMD_device_coherent_memory 1
#define VK_AMD_DEVICE_COHERENT_MEMORY_SPEC_VERSION 1
#define VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME "VK_AMD_device_coherent_memory"

#define VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NUMBER 226
#define VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NUMBER, type, offset)

#define VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD VK_EXTENSION_BIT(VkMemoryPropertyFlagBits, 6)
#define VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD VK_EXTENSION_BIT(VkMemoryPropertyFlagBits, 7)

typedef struct VkPhysicalDeviceCoherentMemoryFeaturesAMD {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           deviceCoherentMemory;
} VkPhysicalDeviceCoherentMemoryFeaturesAMD;

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_ENUM(VkStructureType, 0)

#endif /* VKI_AMD_DEVICE_COHERENT_MEMORY_H_ */
