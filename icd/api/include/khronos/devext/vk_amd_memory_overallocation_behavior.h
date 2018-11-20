/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vk_amd_memory_overallocation_behavior.h
* @brief Temporary internal header for overallocation behavior. Should be removed once the extension is published
*        and the API gets included in the official Vulkan header.
**********************************************************************************************************************
*/
#ifndef VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_H_
#define VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_H_

#include "vk_internal_ext_helper.h"

#define VK_AMD_memory_overallocation_behavior                   1
#define VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_SPEC_VERSION      1
#define VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME    "VK_AMD_memory_overallocation_behavior"

#define VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NUMBER  190

#define VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NUMBER, type, offset)

typedef enum VkMemoryOverallocationBehaviorAMD {
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_DEFAULT_AMD = 0,
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD = 1,
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD = 2,
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_BEGIN_RANGE_AMD = VK_MEMORY_OVERALLOCATION_BEHAVIOR_DEFAULT_AMD,
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_END_RANGE_AMD = VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD,
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_RANGE_SIZE_AMD = (VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD - VK_MEMORY_OVERALLOCATION_BEHAVIOR_DEFAULT_AMD + 1),
    VK_MEMORY_OVERALLOCATION_BEHAVIOR_MAX_ENUM_AMD = 0x7FFFFFFF

} VkMemoryOverallocationBehaviorAMD;

typedef struct VkDeviceMemoryOverallocationCreateInfoAMD {
    VkStructureType                      sType;                 // Must be VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD
    const void*                          pNext;
    VkMemoryOverallocationBehaviorAMD    overallocationBehavior;

} VkDeviceMemoryOverallocationCreateInfoAMD;

#define VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD  VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_ENUM(VkStructureType, 0)

#endif /* VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_H */
