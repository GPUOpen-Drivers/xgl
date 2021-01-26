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
* @file  binary_cache_serialization.cpp
* @brief Implementation of pipeline binary cache serialization in the xgl_cache_support library.
***********************************************************************************************************************
*/
#include "include/binary_cache_serialization.h"

#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palPlatformKey.h"

#include <cstdlib>
#include <cstring>

namespace vk
{

// =====================================================================================================================
// Writes Vulkan pipeline cache data object header into the provided output buffer.
Util::Result WriteVkPipelineCacheHeaderData(
    void*    pOutputBuffer,
    size_t   bufferSize,
    uint32_t vendorId,
    uint32_t deviceId,
    uint8_t* pUuid,
    size_t   uuidSize,
    size_t*  pBytesWritten
)
{
    PAL_ASSERT(pOutputBuffer != nullptr);
    PAL_ASSERT(pUuid != nullptr);
    PAL_ASSERT(uuidSize == sizeof(PipelineCacheHeaderData::UUID));

    Util::Result result = Util::Result::ErrorIncompleteResults;

    if (bufferSize < VkPipelineCacheHeaderDataSize)
    {
        if (pBytesWritten != nullptr)
        {
            *pBytesWritten = 0;
        }
    }
    else
    {
        PipelineCacheHeaderData header = {};
        static_assert(VkPipelineCacheHeaderDataSize == sizeof(header), "Size assumptions changed!");
        header.headerLength = VkPipelineCacheHeaderDataSize;
        header.headerVersion = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
        header.vendorID = vendorId;
        header.deviceID = deviceId;
        memcpy(header.UUID, pUuid, sizeof(header.UUID));

        memcpy(pOutputBuffer, &header, VkPipelineCacheHeaderDataSize);

        if (pBytesWritten != nullptr)
        {
            *pBytesWritten = VkPipelineCacheHeaderDataSize;
        }
        result = Util::Result::Success;
    }
    return result;
}

// =====================================================================================================================
Util::Result CalculatePipelineBinaryCacheHashId(
    VkAllocationCallbacks*    pAllocationCallbacks,
    const Util::IPlatformKey* pPlatformKey,
    const void*               pCacheData,
    size_t                    dataSize,
    uint8_t*                  pHashId)
{
    Util::Result        result       = Util::Result::Success;
    Util::IHashContext* pContext     = nullptr;
    size_t              contextSize  = pPlatformKey->GetKeyContext()->GetDuplicateObjectSize();
    void*               pContextMem  = pAllocationCallbacks->pfnAllocation(pAllocationCallbacks->pUserData,
                                                                           contextSize,
                                                                           16,
                                                                           VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pContextMem != nullptr)
    {
        result = pPlatformKey->GetKeyContext()->Duplicate(pContextMem, &pContext);
    }
    if (result == Util::Result::Success)
    {
        result = pContext->AddData(pCacheData, dataSize);
    }
    if (result == Util::Result::Success)
    {
        result = pContext->Finish(pHashId);
    }
    if (pContext != nullptr)
    {
        pContext->Destroy();
    }
    if (pContextMem != nullptr)
    {
        pAllocationCallbacks->pfnFree(pAllocationCallbacks->pUserData, pContextMem);
    }

    return result;
}

// =====================================================================================================================
// Returns Util::Result::Success on success or Util::Result::ErrorInvalidMemorySize if the provided buffer is too small
// to create a valid pipeline binary cache blob.
Util::Result PipelineBinaryCacheSerializer::Initialize(
    PipelineCacheBlobFormat blobFormat,
    size_t bufferCapacity,
    void*  pOutputBuffer)
{
    PAL_ASSERT(pOutputBuffer != nullptr);

    Util::Result result = Util::Result::ErrorInvalidMemorySize;
    m_blobFormat = blobFormat;
    m_pOutputBuffer = pOutputBuffer;
    if (bufferCapacity >= HeaderSize)
    {
        m_bufferCapacity = bufferCapacity;
        m_bytesUsed = HeaderSize;
        result = Util::Result::Success;
    }
    return result;
}

// =====================================================================================================================
// Copies the provided data into the internal buffer.
Util::Result PipelineBinaryCacheSerializer::AddPipelineBinary(
    const BinaryCacheEntry* pEntry,
    const void*             pData)
{
    PAL_ASSERT(pEntry != nullptr);
    PAL_ASSERT(pData != nullptr);

    Util::Result result = Util::Result::ErrorIncompleteResults;
    const size_t bytesToWrite = EntryHeaderSize + pEntry->dataSize;
    if (bytesToWrite <= (m_bufferCapacity - m_bytesUsed))
    {
        void *pOutputMem = Util::VoidPtrInc(m_pOutputBuffer, m_bytesUsed);
        memcpy(pOutputMem, pEntry, EntryHeaderSize);
        pOutputMem = Util::VoidPtrInc(pOutputMem, EntryHeaderSize);
        memcpy(pOutputMem, pData, pEntry->dataSize);
        m_bytesUsed += bytesToWrite;
        ++m_numEntries;
        result = Util::Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Writes a pipeline binary cache header based on the added data entries, producing a valid pipeline binary cache blob.
// No further data entries can be added after calling Finalize.
Util::Result PipelineBinaryCacheSerializer::Finalize(
    VkAllocationCallbacks*    pAllocationCallbacks,
    const Util::IPlatformKey* pKey,
    size_t*                   pCacheEntriesWritten,
    size_t*                   pBytesWritten)
{
    PAL_ASSERT(pAllocationCallbacks != nullptr);
    PAL_ASSERT(pKey != nullptr);

    auto pPrivateHeader         = static_cast<PipelineBinaryCachePrivateHeader*>(m_pOutputBuffer);
    void *pCacheDataBegin       = Util::VoidPtrInc(m_pOutputBuffer, HeaderSize);
    const size_t cacheDataBytes = m_bytesUsed - HeaderSize;

    if (pCacheEntriesWritten != nullptr)
    {
        *pCacheEntriesWritten = m_numEntries;
    }
    if (pBytesWritten != nullptr)
    {
        *pBytesWritten = m_bytesUsed;
    }

    pPrivateHeader->blobFormat = m_blobFormat;
    return CalculatePipelineBinaryCacheHashId(pAllocationCallbacks,
                                              pKey,
                                              pCacheDataBegin,
                                              cacheDataBytes,
                                              pPrivateHeader->hashId);
}

}
