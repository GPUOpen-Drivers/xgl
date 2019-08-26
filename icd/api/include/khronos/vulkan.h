/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vulkan.h
 * @brief Proxy to the real Khronos Vulkan header.
 ***********************************************************************************************************************
 */

#ifndef __VULKAN_H_PROXY__
#define __VULKAN_H_PROXY__

#include "vk_platform.h"

#include "sdk-1.1/vulkan.h"

#ifdef VK_USE_PLATFORM_XLIB_KHR
#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif
#ifdef setbit
#undef setbit
#endif
#endif

// Internal (under development) extension definitions

#include "devext/vk_amd_gpa_interface.h"

#include "devext/vk_amd_device_coherent_memory.h"

enum class DynamicStatesInternal : uint32_t {
    VIEWPORT = 0,
    SCISSOR,
    LINE_WIDTH,
    DEPTH_BIAS,
    BLEND_CONSTANTS,
    DEPTH_BOUNDS,
    STENCIL_COMPARE_MASK,
    STENCIL_WRITE_MASK,
    STENCIL_REFERENCE,
    VIEWPORT_W_SCALING_NV,
    DISCARD_RECTANGLE_EXT,
    SAMPLE_LOCATIONS_EXT,
    DynamicStatesInternalCount
};

#endif
