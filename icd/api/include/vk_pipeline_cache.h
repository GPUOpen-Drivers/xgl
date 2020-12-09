/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/pipeline_binary_cache.h"
#include "include/pipeline_compiler.h"

namespace vk
{

class Device;

// Layout for pipeline cache private header, all fields are written with LSB first.
struct PipelineCachePrivateHeaderData
{
    PipelineCompilerType cacheType;     // Cache data type
    uint64_t blobSize[MaxPalDevices];   // Blob data size for each device
};

// =====================================================================================================================
// Implementation of Vulkan pipeline cache object
class PipelineCache : public NonDispatchable<VkPipelineCache, PipelineCache>
{
public:
    static VkResult Create(
        Device*                             pDevice,
        const VkPipelineCacheCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkPipelineCache*                    pPipelineCache);

    VkResult Destroy(
        Device*                             pDevice,
        const VkAllocationCallbacks*        pAllocator);

    VkResult GetData(void* pData, size_t* pSize);

    ShaderCache GetShaderCache(uint32_t deviceIdx) const
    {
        VK_ASSERT(deviceIdx < MaxPalDevices);
        return m_shaderCaches[deviceIdx];
    }

    VkResult Merge(uint32_t srcCacheCount, const PipelineCache** ppSrcCaches);

    VK_INLINE PipelineBinaryCache* GetPipelineCache() const { return m_pBinaryCache; }
    Vkgc::ICache* GetCacheAdapter() const { return m_pBinaryCache->GetCacheAdapter(); }

protected:
    PipelineCache(const Device*  pDevice,
            ShaderCache*         pShaderCaches,
            PipelineBinaryCache* pBinaryCache
            );

    virtual ~PipelineCache();

    const Device*const  m_pDevice;
    ShaderCache         m_shaderCaches[MaxPalDevices];

    PipelineBinaryCache* m_pBinaryCache;       // Pipeline binary cache object
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineCacheData(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    size_t*                                     pDataSize,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkMergePipelineCaches(
    VkDevice                                    device,
    VkPipelineCache                             dstCache,
    uint32_t                                    srcCacheCount,
    const VkPipelineCache*                      pSrcCaches);

};

} // namespace vk
