/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_PIPELINE_BINARY_H__
#define __VK_PIPELINE_BINARY_H__

#pragma once

#include "include/vk_dispatch.h"
#include "include/vk_pipeline.h"

#include "palMetroHash.h"

namespace vk
{
class Device;

class PipelineBinary final : public NonDispatchable<VkPipelineBinaryKHR, PipelineBinary>
{
public:
    static VkResult CreatePipelineBinaries(
        Device*                                     pDevice,
        const VkPipelineBinaryCreateInfoKHR*        pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkPipelineBinaryHandlesInfoKHR*             pBinaries);

    VkResult DestroyPipelineBinary(
        Device*                                     pDevice,
        const VkAllocationCallbacks*                pAllocator);

    static VkResult GetPipelineKey(
        const Device*                               pDevice,
        const VkPipelineCreateInfoKHR*              pPipelineCreateInfo,
        VkPipelineBinaryKeyKHR*                     pPipelineBinaryKey);

    VkResult GetPipelineBinaryData(
        VkPipelineBinaryKeyKHR*                     pPipelineBinaryKey,
        size_t*                                     pPipelineBinaryDataSize,
        void*                                       pPipelineBinaryData);

    static VkResult ReleaseCapturedPipelineData(
        Device*                                     pDevice,
        Pipeline*                                   pPipeline,
        const VkAllocationCallbacks*                pAllocator);

    static void ReadFromPipelineBinaryKey(
        const VkPipelineBinaryKeyKHR&               inKey,
        Util::MetroHash::Hash*                      pOutKey);

    const Util::MetroHash::Hash& BinaryKey() const
        { return m_binaryKey; }

    const Vkgc::BinaryData& BinaryData() const
        { return m_binaryData; }

protected:

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineBinary);

    PipelineBinary(
        const Util::MetroHash::Hash&                binaryKey,
        const Vkgc::BinaryData&                     binaryData);

    // Pipeline binary doesn't contain the key itself.
    static VkResult Create(
        Device*                                     pDevice,
        const Util::MetroHash::Hash&                binaryKey,
        const Vkgc::BinaryData&                     binaryData,
        const VkAllocationCallbacks*                pAllocator,
        VkPipelineBinaryKHR*                        pPipelineBinary);

    static void WriteToPipelineBinaryKey(
        const void*                                 pSrcData,
        const size_t                                dataSize,
        VkPipelineBinaryKeyKHR*                     pDstKey);

    const Util::MetroHash::Hash                     m_binaryKey;
    const Vkgc::BinaryData                          m_binaryData;
};

} // namespace vk

#endif /* __VK_PIPELINE_BINARY_H__ */
