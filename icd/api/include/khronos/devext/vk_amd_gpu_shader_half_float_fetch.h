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
 **********************************************************************************************************************
 * @file  vk_amd_gpu_shader_half_float_fetch.h
 * @brief Temporary internal header for GPU shader half float fetchextension. Should be removed once the extension is
 *        published and the API gets included in the official Vulkan header.
 **********************************************************************************************************************
 */
#ifndef VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_H_
#define VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_H_

#include "vk_internal_ext_helper.h"

#define VK_AMD_gpu_shader_half_float_fetch                             1
#define VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_SPEC_VERSION                1
#define VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_EXTENSION_NAME              "VK_AMD_gpu_shader_half_float_fetch"

#define VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_EXTENSION_NUMBER            44

#define VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_EXTENSION_NUMBER, type, offset)

#endif /* VK_AMD_GPU_SHADER_HALF_FLOAT_FETCH_H_ */
