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
 * @file  vk_platform.h
 * @brief Proxy to the real Khronos Vulkan platform header.
 ***********************************************************************************************************************
 */

#ifndef __VK_PLATFORM_H_PROXY__
#define __VK_PLATFORM_H_PROXY__

#if EXTERNAL_VULKAN_HEADERS
#include "vulkan/vk_platform.h"
#else
#include "sdk-1.3/vulkan/vk_platform.h"
#endif

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

#endif /* __VK_PLATFORM_H_PROXY__ */
