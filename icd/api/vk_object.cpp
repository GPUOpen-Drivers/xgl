/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  vk_object.cpp
 * @brief Contains implementation of functionality common to all Vulkan objects.
 ***********************************************************************************************************************
 */

#include "include/vk_object.h"
#include "include/vk_utils.h"
#include "include/vk_pipeline.h"
#include "include/vk_buffer.h"
#include "include/vk_buffer_view.h"
#include "include/vk_descriptor_pool.h"
#include "include/vk_descriptor_set.h"
#include "include/vk_event.h"
#include "include/vk_fence.h"
#include "include/vk_framebuffer.h"
#include "include/vk_query.h"
#include "include/vk_sampler.h"
#include "include/vk_shader.h"
#include "include/vk_semaphore.h"
#include "include/vk_device.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_image_view.h"
#include "include/vk_render_pass.h"

namespace vk
{

// =====================================================================================================================
// This function handles any error checking for returning data from GetObjectInfo and also writes the data size.
// If it returns true, it's okay to write data through pData.
bool NeedGetObjectInfoData(
    size_t      reqDataSize, // Required data size for the requested info type
    size_t*     pDataSize,   // Either output pointer for data size, or provided input data size
    const void* pData,       // Input data pointer
    VkResult*   pResult)     // Output result; will be updated if any errors are generated
{
    VK_ASSERT(pResult != nullptr);

    if (pDataSize == nullptr)
    {
        *pResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    // If app is asking for data, check that enough output space is specified
    if (pData != nullptr)
    {
        if (*pDataSize < reqDataSize)
        {
            *pResult = VK_ERROR_INITIALIZATION_FAILED;
            return false;
        }
    }

    // Write the required data size
    *pDataSize = reqDataSize;
    *pResult   = VK_SUCCESS;

    return (pData != nullptr);
}

} // namespace vk
