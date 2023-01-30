/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  cache_adapter.cpp
* @brief Implementation of the compiler interface.
***********************************************************************************************************************
*/

#include "cache_adapter.h"
#include "include/vk_utils.h"

#include "palMetroHash.h"
#include "pipeline_binary_cache.h"

namespace vk
{

// =====================================================================================================================
CacheAdapter* CacheAdapter::Create(
    PipelineBinaryCache* pPipelineBinaryCache)
{
    CacheAdapter* pObj = nullptr;
    void*         pMem = pPipelineBinaryCache->AllocMem(sizeof(CacheAdapter));

    if (pMem != nullptr)
    {
        pObj = VK_PLACEMENT_NEW(pMem) CacheAdapter(pPipelineBinaryCache);
    }

    return pObj;
}

// =====================================================================================================================
void CacheAdapter::Destroy()
{
    PipelineBinaryCache* pCache = m_pPipelineBinaryCache;
    void*                pMem   = this;
    Util::Destructor(this);
    pCache->FreeMem(pMem);
}

// =====================================================================================================================
CacheAdapter::CacheAdapter(
    PipelineBinaryCache* pPipelineBinaryCache)
    :
    m_pPipelineBinaryCache { pPipelineBinaryCache }
{
}

// =====================================================================================================================
CacheAdapter::~CacheAdapter()
{
}

// =====================================================================================================================
Result CacheAdapter::GetEntry(
    HashId              hashId,
    bool                allocateOnMiss,
    EntryHandle*        pHandle)
{
    Result result = Result::Success;
    bool mustPopulate = false;
    Util::QueryResult* pQuery =
        static_cast<Util::QueryResult*>(m_pPipelineBinaryCache->AllocMem(sizeof(Util::QueryResult)));

    if (pQuery != nullptr)
    {
        Util::MetroHash::Hash cacheId = {};
        Util::MetroHash128 hash128 = {};
        hash128.Update(hashId);
        hash128.Finalize(cacheId.bytes);
        uint32_t flags  = Util::ICacheLayer::QueryFlags::AcquireEntryRef;

        if (allocateOnMiss)
        {
            flags |= Util::ICacheLayer::QueryFlags::ReserveEntryOnMiss;
        }
        Util::Result palResult = m_pPipelineBinaryCache->QueryPipelineBinary(&cacheId, flags, pQuery);
        if (palResult == Util::Result::Reserved)
        {
            mustPopulate = true;
            result = Result::NotFound;
        }
        else if (palResult == Util::Result::NotReady)
        {
            result = Result::NotReady;
        }
        else if (palResult != Util::Result::Success)
        {
            m_pPipelineBinaryCache->FreeMem(pQuery);
            pQuery = nullptr;
            result = Result::ErrorUnknown;
            if (palResult == Util::Result::NotFound)
            {
                result = Result::NotFound;
            }
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    if (pQuery != nullptr)
    {
        *pHandle = EntryHandle(this, pQuery, mustPopulate);
        VK_ASSERT(pHandle != nullptr);
    }

    return result;
}

// =====================================================================================================================
Result CacheAdapter::WaitForEntry(
    RawEntryHandle rawHandle)
{
    Util::QueryResult* pQuery  = static_cast<Util::QueryResult*>(rawHandle);

    Util::Result palResult = m_pPipelineBinaryCache->WaitPipelineBinary(&pQuery->hashId);
    if (palResult == Util::Result::Success)
    {
        // Update query result after a wait
        palResult = m_pPipelineBinaryCache->QueryPipelineBinary(&pQuery->hashId, 0, pQuery);
    }

    return (palResult == Util::Result::Success) ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
void CacheAdapter::ReleaseEntry(
    RawEntryHandle rawHandle)
{
    if (rawHandle != nullptr)
    {
        Util::QueryResult* pQuery  = static_cast<Util::QueryResult*>(rawHandle);
        m_pPipelineBinaryCache->ReleaseCacheRef(pQuery);
        m_pPipelineBinaryCache->FreeMem(rawHandle);
    }
}

// =====================================================================================================================
Result CacheAdapter::SetValue(
    RawEntryHandle rawHandle,
    bool success,
    const void* pData,
    size_t dataLen)
{
    Result result = Result::Success;
    Util::QueryResult* pQuery  = static_cast<Util::QueryResult*>(rawHandle);
    Util::Result palResult = Util::Result::Success;

    if (!success)
    {
        // Setting invalid data to entry means to set entry bad. which will be evicted when refcount becones zero.
        palResult = m_pPipelineBinaryCache->MarkEntryBad(pQuery);
    }
    else
    {
        palResult = m_pPipelineBinaryCache->StorePipelineBinary(&pQuery->hashId, dataLen, pData);
    }
    result = (palResult == Util::Result::Success) ? Result::Success : Result::ErrorUnknown;

    return result;
}

// =====================================================================================================================
Result CacheAdapter::GetValue(
    RawEntryHandle rawHandle,
    void*   pData,
    size_t* pDataLen)
{
    Result result = Result::NotReady;
    Util::Result palResult = Util::Result::Success;
    Util::QueryResult* pQuery  = static_cast<Util::QueryResult*>(rawHandle);
    if (palResult == Util::Result::Success)
    {
        if (pData == nullptr)
        {
            *pDataLen = pQuery->dataSize;
        }
        else
        {
            if (*pDataLen < pQuery->dataSize)
            {
                result = Result::ErrorInvalidValue;
            }
            else
            {
                palResult = m_pPipelineBinaryCache->GetPipelineBinary(pQuery, pData);
                if (palResult == Util::Result::Success)
                {
                    result = Result::Success;
                }
                else
                {
                    result = Result::ErrorUnknown;
                }
            }
        }
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
Result CacheAdapter::GetValueZeroCopy(
    RawEntryHandle rawHandle,
    const void** ppData,
    size_t* pDataLen)
{
    Result result = Result::NotReady;
    Util::Result palResult = Util::Result::Success;
    Util::QueryResult* pQuery  = static_cast<Util::QueryResult*>(rawHandle);

    palResult = m_pPipelineBinaryCache->GetCacheDataPtr(pQuery, ppData);
    if (palResult == Util::Result::Success)
    {
        *pDataLen = pQuery->dataSize;
        result = Result::Success;
    }
    else if (palResult == Util::Result::NotReady)
    {
        result = Result::NotReady;
    }
    else if (palResult == Util::Result::NotFound)
    {
        result = Result::NotFound;
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

} //namespace vk

