/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vk_ext_external_memory_host.h
* @brief Header for VK_EXT_external_memory_host extension.
**********************************************************************************************************************
*/
#ifndef VK_EXT_EXTERNAL_MEMORY_HOST_H_
#define VK_EXT_EXTERNAL_MEMORY_HOST_H_

#include "vk_internal_ext_helper.h"

#define VK_EXT_external_memory_host 1
#define VK_EXT_EXTERNAL_MEMORY_HOST_SPEC_VERSION 1
#define VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME "VK_EXT_external_memory_host"
#define VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NUMBER 179

#define VK_EXT_EXTERNAL_MEMORY_HOST_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NUMBER, type, offset)

typedef struct VkImportMemoryHostPointerInfoEXT {
    VkStructureType                          sType;
    const void*                              pNext;
    VkExternalMemoryHandleTypeFlagBitsKHR    handleType;
    void*                                    pHostPointer;
} VkImportMemoryHostPointerInfoEXT;

typedef struct VkMemoryHostPointerPropertiesEXT {
    VkStructureType    sType;
    void*              pNext;
    uint32_t           memoryTypeBits;
} VkMemoryHostPointerPropertiesEXT;

typedef struct VkPhysicalDeviceExternalMemoryHostPropertiesEXT {
    VkStructureType    sType;
    void*              pNext;
    VkDeviceSize       minImportedHostPointerAlignment;
} VkPhysicalDeviceExternalMemoryHostPropertiesEXT;

typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryHostPointerPropertiesEXT)(VkDevice device, VkExternalMemoryHandleTypeFlagBitsKHR handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryHostPointerPropertiesEXT(
    VkDevice                                    device,
    VkExternalMemoryHandleTypeFlagBitsKHR       handleType,
    const void*                                 pHostPointer,
    VkMemoryHostPointerPropertiesEXT*           pMemoryHostPointerProperties);
#endif

#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT                VK_EXTENSION_BIT(VkExternalMemoryHandleTypeFlagBitsKHR, 7)
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT     VK_EXTENSION_BIT(VkExternalMemoryHandleTypeFlagBitsKHR, 8)

#define VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT                 VK_EXT_EXTERNAL_MEMORY_HOST_ENUM(VkStructureType, 0)
#define VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT                  VK_EXT_EXTERNAL_MEMORY_HOST_ENUM(VkStructureType, 1)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT VK_EXT_EXTERNAL_MEMORY_HOST_ENUM(VkStructureType, 2)

#endif
