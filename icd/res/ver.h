/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Bump Major version to match the supported vulkan header file
// and zero minor and subminor version numbers

#define MKSTR(x) #x
#define MAKE_VERSION_STRING(x) MKSTR(x)

// This value is used for the VkPhysicalDeviceProperties uint32 driverVersion which is OS agnostic
#define VULKAN_ICD_MAJOR_VERSION    2

#define VERSION_MAJOR               VULKAN_ICD_MAJOR_VERSION
#define VERSION_MAJOR_STR           MAKE_VERSION_STRING(VULKAN_ICD_MAJOR_VERSION) "\0"

// Bump up after each promotion to mainline
#define VULKAN_ICD_BUILD_VERSION   285

// String version is needed with leading zeros and extra termination (unicode)
#define VERSION_NUMBER_MINOR        VULKAN_ICD_BUILD_VERSION
#define VERSION_NUMBER_MINOR_STR    MAKE_VERSION_STRING(VULKAN_ICD_BUILD_VERSION) "\0"

// These values specify the driver ID and driver info string
#define VULKAN_DRIVER_ID            VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR  // "AMDOPEN"
#define VULKAN_DRIVER_NAME_STR      "AMD open-source driver"
#define VULKAN_DRIVER_INFO_STR      "2023.Q3.3"
#define VULKAN_DRIVER_INFO_STR_LLPC "(LLPC)"

// These values tell which version of the conformance test the driver is compliant against
#define CTS_VERSION_MAJOR           1
#define CTS_VERSION_MINOR           3
#define CTS_VERSION_SUBMINOR        3
#define CTS_VERSION_PATCH           1
