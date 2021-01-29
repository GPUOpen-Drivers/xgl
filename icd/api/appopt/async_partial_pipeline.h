/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  async_partial_pipeline.h
* @brief Header file of class async::PartialPipeline
***********************************************************************************************************************
*/

#ifndef __ASYNC_PARTIAL_PIPELINE_H__
#define __ASYNC_PARTIAL_PIPELINE_H__

#pragma once

#include "include/vk_dispatch.h"
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 39
#include "vkgcDefs.h"
#else
#include "llpc.h"
#endif

namespace vk
{

namespace async
{

// =====================================================================================================================
// Implementation of a async shader module
class PartialPipeline
{
public:
    static PartialPipeline* Create(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult Destroy();

    void CreatePipelineLayoutFromModuleData(
        AsyncLayer*                           pAsyncLayer,
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 39
        Vkgc::ShaderModuleEntryData*          pShaderModuleEntryData,
        const Vkgc::ResourceMappingRootNode** ppResourceMappingNode,
#else
        Llpc::ShaderModuleEntryData*          pShaderModuleEntryData,
        const Llpc::ResourceMappingRootNode** ppResourceMappingNode,
#endif
        uint32_t*                             pMappingNodeCount);

    void CreateColorTargetFromModuleData(
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 39
        Vkgc::ShaderModuleDataEx* pShaderModuleDataEx,
        Vkgc::ColorTarget* pTarget);
#else
        Llpc::ShaderModuleDataEx* pShaderModuleDataEx,
        Llpc::ColorTarget* pTarget);
#endif

    void Execute(AsyncLayer* pAsyncLayer, PartialPipelineTask* pTask);

    void AsyncBuildPartialPipeline(AsyncLayer* pAsyncLayer, VkShaderModule asyncShaderModule);

protected:
    PartialPipeline(const VkAllocationCallbacks* pAllocator);
private:
    const VkAllocationCallbacks*    m_pAllocator;
};

} // namespace async

} // namespace vk

#endif
