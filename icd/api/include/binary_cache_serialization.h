/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Google LLC. All Rights Reserved.
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
* @file  binary_cache_serialization.h
* @brief Declaration of pipeline binary cache serialization interface in the xgl_cache_support library.
***********************************************************************************************************************
*/
#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "palMetroHash.h"
#include "palUtil.h"

#include <cstddef>
#include <cstdint>

struct VkAllocationCallbacks;

namespace Util
{
class IPlatformKey;
}

namespace vk
{

// Layout for pipeline cache header version VK_PIPELINE_CACHE_HEADER_VERSION_ONE, all fields are written with LSB first.
struct PipelineCacheHeaderData
{
    uint32_t headerLength;       // Length in bytes of the entire pipeline cache header.
    uint32_t headerVersion;      // A VkPipelineCacheHeaderVersion value.
    uint32_t vendorID;           // A vendor ID equal to VkPhysicalDeviceProperties::vendorID.
    uint32_t deviceID;           // A device ID equal to VkPhysicalDeviceProperties::deviceID.
    uint8_t  UUID[VK_UUID_SIZE]; // A pipeline cache ID equal to VkPhysicalDeviceProperties::pipelineCacheUUID.
};

constexpr size_t VkPipelineCacheHeaderDataSize = sizeof(PipelineCacheHeaderData);

Util::Result WriteVkPipelineCacheHeaderData(
    void*    pOutputBuffer,
    size_t   bufferSize,
    uint32_t vendorId,
    uint32_t deviceId,
    uint8_t* pUuid,
    size_t   uuidSize,
    size_t*  pBytesWritten
);

// Layout for pipeline binary cache entry header, all fields are written with LSB first.
struct BinaryCacheEntry
{
    Util::MetroHash::Hash hashId;
    size_t                dataSize;
};

enum class PipelineCacheBlobFormat : uint32_t {
    Strict = 0,
    Portable = 1
};

// Layout for pipeline binary cache header, all fields are written with LSB first.
constexpr size_t SHA_DIGEST_LENGTH = 20;
struct PipelineBinaryCachePrivateHeader
{
    PipelineCacheBlobFormat blobFormat;
    uint8_t  hashId[SHA_DIGEST_LENGTH];
};

Util::Result CalculatePipelineBinaryCacheHashId(
    VkAllocationCallbacks*    pAllocationCallbacks,
    const Util::IPlatformKey* pPlatformKey,
    const void*               pCacheData,
    size_t                    dataSize,
    uint8_t*                  pHashId);

// =====================================================================================================================
// Class for serializing in-memory cache data into valid pipeline binary cache blobs.
class PipelineBinaryCacheSerializer
{
public:
    // Returns an upper bound for the size of the final pipeline binary cache blob.
    // This can be used to create an appropriately-sized buffer for the serialized pipeline binary cache.
    // Note that this doesn't take into account the Vulkan pipeline cache data.
    static size_t CalculateAnticipatedCacheBlobSize(
        size_t numEntries,
        size_t totalPipelineBinariesSize)
    {
        return HeaderSize + (numEntries * EntryHeaderSize) + totalPipelineBinariesSize;
    }

    PipelineBinaryCacheSerializer() = default;

    Util::Result Initialize(
        PipelineCacheBlobFormat blobFormat,
        size_t                  bufferCapacity,
        void*                   pOutputBuffer);

    Util::Result AddPipelineBinary(
        const BinaryCacheEntry* pEntry,
        const void*             pData);

    Util::Result Finalize(
        VkAllocationCallbacks*    pAllocationCallbacks,
        const Util::IPlatformKey* pKey,
        size_t*                   pCacheEntriesWritten,
        size_t*                   pBytesWritten);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineBinaryCacheSerializer);

    static constexpr size_t HeaderSize       = sizeof(PipelineBinaryCachePrivateHeader);
    static constexpr size_t EntryHeaderSize  = sizeof(BinaryCacheEntry);

    PipelineCacheBlobFormat m_blobFormat     = {};
    size_t                  m_numEntries     = 0;
    void*                   m_pOutputBuffer  = nullptr;
    size_t                  m_bufferCapacity = 0;
    size_t                  m_bytesUsed      = 0;
};

}
