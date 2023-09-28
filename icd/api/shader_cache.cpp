/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  shader_cache.cpp
* @brief Contains implementation of ShaderCache
***********************************************************************************************************************
*/
#include "include/shader_cache.h"
#include "include/pipeline_compiler.h"
#include "include/vk_conv.h"

namespace vk
{
// =====================================================================================================================
ShaderCache::ShaderCache()
    :
    m_cacheType(),
    m_cache()
{

}

// =====================================================================================================================
// Initializes shader cache object.
void ShaderCache::Init(
    PipelineCompilerType  cacheType,  // Shader cache type
    ShaderCachePtr        cachePtr)   // Pointer of the shader cache implementation
{
    m_cacheType = cacheType;
    m_cache = cachePtr;
}

// =====================================================================================================================
// Serializes the shader cache data or queries the size required for serialization.
VkResult ShaderCache::Serialize(
    void*   pBlob,
    size_t* pSize)
{
    VkResult result = VK_SUCCESS;

    return result;
}

// =====================================================================================================================
// Merges the provided source shader caches' content into this shader cache.
VkResult ShaderCache::Merge(
    uint32_t              srcCacheCount,
    const ShaderCachePtr* ppSrcCaches)
{
    VkResult result = VK_SUCCESS;

    return result;
}

// =====================================================================================================================
// Frees all resources associated with this object.
void ShaderCache::Destroy(
    PipelineCompiler* pCompiler)
{
}

}
