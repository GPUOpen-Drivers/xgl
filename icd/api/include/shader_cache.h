/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  shader_cache.h
* @brief Contains declaration of ShaderCache
***********************************************************************************************************************
*/
#ifndef __SHADER_CACHE_H__
#define __SHADER_CACHE_H__

#include "include/compiler_solution.h"
#include "include/vk_utils.h"

namespace vk
{

class PipelineCompiler;

// Unified shader cache interface
class ShaderCache
{
public:
    union ShaderCachePtr
    {
    };

    ShaderCache();

    void Init(PipelineCompilerType cacheType, ShaderCachePtr cachePtr);

    VkResult Serialize(void* pBlob, size_t* pSize);

    VkResult Merge(uint32_t srcCacheCount, const ShaderCachePtr* ppSrcCaches);

    void Destroy(PipelineCompiler* pCompiler);

    PipelineCompilerType GetCacheType() const { return m_cacheType; }

    ShaderCachePtr GetCachePtr() const { return m_cache; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderCache);

    PipelineCompilerType  m_cacheType;
    ShaderCachePtr        m_cache;
};

}

#endif
