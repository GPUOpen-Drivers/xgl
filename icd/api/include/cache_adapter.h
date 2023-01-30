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
* @file  cache_adapter.h
* @brief Declaration of an adapter class that implements ICache interfaces.
***********************************************************************************************************************
*/
#pragma once

#include "vkgcDefs.h"
#include "include/vk_utils.h"

namespace vk
{
using namespace Vkgc;

class PipelineBinaryCache;

// An adapter class that implements ICache in terms of a simple Get/Set interface.
class CacheAdapter : public ICache
{
public:
    static CacheAdapter* Create(PipelineBinaryCache* pPipelineBinaryCache);

    ~CacheAdapter();
    void Destroy();

    Vkgc::Result GetEntry(HashId hashId,
                   bool allocateOnMiss,
                   EntryHandle* pHandle) override;

    Vkgc::Result WaitForEntry(RawEntryHandle rawHandle) override;

    void ReleaseEntry(RawEntryHandle rawHandle) override;

    Vkgc::Result SetValue(RawEntryHandle rawHandle,
                    bool success,
                    const void* pData,
                    size_t dataLen) override;

    Vkgc::Result GetValue(RawEntryHandle rawHandle,
                    void*   pData,
                    size_t* pDataLen) override;

    Vkgc::Result GetValueZeroCopy(void* rawHandle,
                            const void** ppData,
                            size_t* pDataLen) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(CacheAdapter);

    explicit CacheAdapter(PipelineBinaryCache* pPipelineBinaryCache);

    PipelineBinaryCache* m_pPipelineBinaryCache;
};

} // namespace vk

