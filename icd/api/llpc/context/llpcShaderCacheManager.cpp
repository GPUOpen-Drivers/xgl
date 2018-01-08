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
 ***********************************************************************************************************************
 @file llpcShaderCacheManager.cpp
 @brief LLPC source file: contains implementation of class Llpc::ShaderCacheManager.
 ***********************************************************************************************************************
*/

#define DEBUG_TYPE "llpc-shader-cache-manager"

#include "llpcShaderCacheManager.h"

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// The global ShaderCacheManager object
ShaderCacheManager* ShaderCacheManager::m_pManager = nullptr;

// =====================================================================================================================
// Destroy all objects
ShaderCacheManager::~ShaderCacheManager()
{
    for (auto cacheIt = m_shaderCaches.begin(), endIt = m_shaderCaches.end();
        cacheIt != endIt; ++cacheIt)
    {
        // Deletes managed object
        (*cacheIt).reset();
    }

    m_shaderCaches.clear();
}

// =====================================================================================================================
// Get ShaderCache instance with specified create info
ShaderCachePtr ShaderCacheManager::GetShaderCacheObject(
    const ShaderCacheCreateInfo*    pCreateInfo,    // [in] Shader cache create info
    const ShaderCacheAuxCreateInfo* pAuxCreateInfo) // [in] Shader cache auxiliary info (static fields)
{
    ShaderCachePtr pShaderCache;
    auto cacheIt = m_shaderCaches.begin();
    auto endIt = m_shaderCaches.end();

    for (; cacheIt != endIt; ++cacheIt)
    {
        if ((*cacheIt)->IsCompatible(pCreateInfo, pAuxCreateInfo))
        {
            pShaderCache = (*cacheIt);
            break;
        }
    }

    // No compatible object is found, create a new one
    if (cacheIt == endIt)
    {
        pShaderCache = std::make_shared<ShaderCache>();
        m_shaderCaches.push_back(pShaderCache);
        pShaderCache->Init(pCreateInfo, pAuxCreateInfo);
    }

    return pShaderCache;
}

// =====================================================================================================================
// Release ShaderCache instance
void ShaderCacheManager::ReleaseShaderCacheObject(
    ShaderCachePtr& shaderCachePtr)                 // [in] ShaderCache instance to be released
{
    auto cacheIt = m_shaderCaches.begin();
    auto endIt = m_shaderCaches.end();
    for (; cacheIt != endIt; ++cacheIt)
    {
        if ((*cacheIt).get() == shaderCachePtr.get())
        {
            break;
        }
    }

    LLPC_ASSERT(cacheIt != endIt);

    shaderCachePtr.reset();
}

} // Llpc
