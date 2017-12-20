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

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"

namespace Pal
{

class IShaderCache;

}

namespace Llpc
{

class IShaderCache;

}

namespace vk
{

class Device;

// Layout for pipeline cache header version VK_PIPELINE_CACHE_HEADER_VERSION_ONE, all fields are written with LSB first.
struct PipelineCacheHeaderData
{
    uint32_t headerLength;       // Length in bytes of the entire pipeline cache header.
    uint32_t headerVersion;      // A VkPipelineCacheHeaderVersion value.
    uint32_t vendorID;           // A vendor ID equal to VkPhysicalDeviceProperties::vendorID.
    uint32_t deviceID;           // A device ID equal to VkPhysicalDeviceProperties::deviceID.
    uint8_t  UUID[VK_UUID_SIZE]; // A pipeline cache ID equal to VkPhysicalDeviceProperties::pipelineCacheUUID.
};

// Enumerates the cache type in the pipeline cache
enum PipelineCacheType : uint32_t
{
    PipelineCacheTypePal,   // Use shader cache provided by PAL
    PipelineCacheTypeLlpc   // Use shader cache provided by LLPC
};

// Layout for pipeline cache private header, all fields are written with LSB first.
struct PipelineCachePrivateHeaderData
{
    PipelineCacheType cacheType;        // Cache data type
    uint64_t blobSize[MaxPalDevices];   // Blob data size for each device
};

// Unified shader cache interface
union IShaderCachePtr
{
    Pal::IShaderCache*  pPalShaderCache;  // Pointer to PAL shader cache object
    Llpc::IShaderCache* pLlpcShaderCache; // Pointer to LLPC shader cache object
};

// =====================================================================================================================
// Implementation of Vulkan pipeline cache object
class PipelineCache : public NonDispatchable<VkPipelineCache, PipelineCache>
{

public:
    static VkResult Create(
        const Device*                       pDevice,
        const VkPipelineCacheCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkPipelineCache*                    pPipelineCache);

    VkResult Destroy(
        const Device*                       pDevice,
        const VkAllocationCallbacks*        pAllocator);

    VkResult GetData(void* pData, size_t* pSize);

    IShaderCachePtr GetShaderCache(uint32_t deviceIdx) const
    {
        VK_ASSERT(deviceIdx < MaxPalDevices);
        return m_pShaderCaches[deviceIdx];
    }

    PipelineCacheType GetPipelineCacheType() const { return m_cacheType; }

    VkResult Merge(uint32_t srcCacheCount, const PipelineCache** ppSrcCaches);

protected:
    PipelineCache(const Device* pDevice, PipelineCacheType cacheType, IShaderCachePtr* pShaderCaches);

    virtual ~PipelineCache();

    const Device*const  m_pDevice;
    PipelineCacheType   m_cacheType;
    IShaderCachePtr     m_pShaderCaches[MaxPalDevices];
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
