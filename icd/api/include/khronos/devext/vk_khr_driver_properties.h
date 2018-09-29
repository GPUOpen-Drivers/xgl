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
 **********************************************************************************************************************
 * @file  vk_khr_driver_properties.h
 * @brief
 **********************************************************************************************************************
 */
#ifndef VK_KHR_DRIVER_PROPERTIES_H_
#define VK_KHR_DRIVER_PROPERTIES_H_

#define VK_KHR_driver_properties                        1
#define VK_KHR_DRIVER_PROPERTIES_EXTENSION_NUMBER       197
#define VK_KHR_DRIVER_PROPERTIES_SPEC_VERSION           1
#define VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME         "VK_KHR_driver_properties"

#define VK_KHR_DRIVER_PROPERTIES_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NUMBER, type, offset)

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR     VK_KHR_DRIVER_PROPERTIES_ENUM(VkStructureType, 0)

#define VK_MAX_DRIVER_NAME_SIZE_KHR       256
#define VK_MAX_DRIVER_INFO_SIZE_KHR       256

typedef struct VkConformanceVersionKHR {
    uint8_t    major;
    uint8_t    minor;
    uint8_t    subminor;
    uint8_t    patch;
} VkConformanceVersionKHR;

typedef struct VkPhysicalDeviceDriverPropertiesKHR {
    VkStructureType            sType;
    void*                      pNext;
    uint32_t                   driverID;
    char                       driverName[VK_MAX_DRIVER_NAME_SIZE_KHR];
    char                       driverInfo[VK_MAX_DRIVER_INFO_SIZE_KHR];
    VkConformanceVersionKHR    conformanceVersion;
} VkPhysicalDeviceDriverPropertiesKHR;

#endif /* VK_KHR_DRIVER_PROPERTIES_H_ */
