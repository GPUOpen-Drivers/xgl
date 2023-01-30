/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "temp_mem_arena.h"

namespace vk { namespace utils {

// =====================================================================================================================
TempMemArena::TempMemArena(
    const VkAllocationCallbacks* pAllocator,
    VkSystemAllocationScope      allocScope)
    :
    m_allocator(*pAllocator),
    m_allocScope(allocScope),
    m_totalMemSize(0),
    m_chunkSize(64 * 1024),
    m_pFirstAvailableChunk(nullptr),
    m_pFirstUsedChunk(nullptr)
#if DEBUG
    , m_nextAllocId(0)
#endif
{

}

// =====================================================================================================================
// Resets all memory back to free.  Does not actually free the backing memory.
void TempMemArena::Reset()
{
    // Reset all available chunks
    MemChunk* pChunk = m_pFirstAvailableChunk;

    while (pChunk != nullptr)
    {
        ResetChunk(pChunk);

        pChunk = pChunk->pNext;
    }

    // Reset all used chunks and splice the lists together
    pChunk = m_pFirstUsedChunk;

    while (pChunk != nullptr)
    {
        ResetChunk(pChunk);

        if (pChunk->pNext == nullptr)
        {
            pChunk->pNext = m_pFirstAvailableChunk;
        }
    }

    m_pFirstAvailableChunk = m_pFirstUsedChunk;
    m_pFirstUsedChunk      = nullptr;
    m_totalMemSize         = 0;
}

// =====================================================================================================================
TempMemArena::~TempMemArena()
{
    FreeChunks(m_pFirstUsedChunk);

    m_pFirstUsedChunk = nullptr;

    FreeChunks(m_pFirstAvailableChunk);

    m_pFirstAvailableChunk = nullptr;
}

#if DEBUG
// =====================================================================================================================
void TempMemArena::CheckSentinels(
    const MemChunk* pChunk
    ) const
{
    const Sentinel* pSentinel = pChunk->pFirstSentinel;

    while (pSentinel != nullptr)
    {
        VK_ASSERT(pSentinel->value == 0xcafebabe);

        pSentinel = pSentinel->pNext;
    }
}
#endif

// =====================================================================================================================
void TempMemArena::ResetChunk(
    MemChunk* pChunk)
{
#if DEBUG
    memset(pChunk->pData, 0xcd, pChunk->capacity);
#endif

    pChunk->tail = 0;

#if DEBUG
    pChunk->pFirstSentinel = nullptr;
#endif
}

// =====================================================================================================================
void TempMemArena::FreeChunks(
    MemChunk* pFirstChunk)
{
    MemChunk* pCurrent = pFirstChunk;

    while (pCurrent != nullptr)
    {
#if DEBUG
        CheckSentinels(pCurrent);
#endif

        MemChunk* pNext = pCurrent->pNext;

        m_allocator.pfnFree(m_allocator.pUserData, pCurrent);

        pCurrent = pNext;
    }
}

// =====================================================================================================================
void* TempMemArena::AllocFromChunk(
    MemChunk* pChunk,
    size_t    size)
{
#if DEBUG
    size += sizeof(Sentinel);
#endif

    // Bump up to machine alignment
    size = Util::Pow2Align(size, VK_DEFAULT_MEM_ALIGN);

    void* pData = nullptr;

    if (pChunk->tail + size <= pChunk->capacity)
    {
        pData = Util::VoidPtrInc(pChunk->pData, pChunk->tail);

        pChunk->tail += size;

#if DEBUG
        Sentinel* pNewSentinel = reinterpret_cast<Sentinel*>(Util::VoidPtrInc(pData, size - sizeof(Sentinel)));

        pNewSentinel->value          = 0xcafebabe;
        pNewSentinel->id             = m_nextAllocId++;
        pNewSentinel->pNext          = pChunk->pFirstSentinel;
        pChunk->pFirstSentinel       = pNewSentinel;
#endif

        m_totalMemSize += size;
    }

    return pData;
}

// =====================================================================================================================
// Allocates memory using the arena
void* TempMemArena::Alloc(size_t size)
{
    if (size == 0)
    {
        return nullptr;
    }

    void* pData = nullptr;

    MemChunk* pChunk = m_pFirstAvailableChunk;

    while ((pChunk != nullptr) && (pData == nullptr))
    {
        pData = AllocFromChunk(pChunk, size);

        if (pData == nullptr)
        {
            MemChunk* pNext = pChunk->pNext;

            // If we are getting close to the end of this chunk
            if ((size <= pChunk->capacity) && (pChunk->capacity - pChunk->tail < pChunk->capacity / 4))
            {
                // Pop it from the available list and into the used list
                pChunk->pNext     = m_pFirstUsedChunk;
                m_pFirstUsedChunk = pChunk;

                if (pChunk == m_pFirstAvailableChunk)
                {
                    m_pFirstAvailableChunk = nullptr;
                }
            }

            pChunk = pNext;
        }
    }

    if (pData == nullptr)
    {
        pData = AllocFromNewChunk(size);
    }

    return pData;
}

// =====================================================================================================================
void* TempMemArena::Alloc(const Util::AllocInfo& allocInfo)
{
    size_t paddedSize = allocInfo.bytes;

    if (allocInfo.alignment != 0)
    {
        paddedSize += allocInfo.alignment - 1;
    }

    void* pMem = Alloc(paddedSize);

    if (pMem != nullptr)
    {
        void* pOrig = pMem;

        if (allocInfo.alignment != 0)
        {
            pMem = Util::VoidPtrAlign(pMem, allocInfo.alignment);
        }

        VK_ASSERT(static_cast<char*>(pMem) - static_cast<char*>(pOrig) + allocInfo.bytes <= paddedSize);

        if (allocInfo.zeroMem)
        {
            memset(pMem, 0, allocInfo.bytes);
        }
    }

    return pMem;
}

// =====================================================================================================================
void TempMemArena::Free(const Util::FreeInfo& freeInfo)
{
    // Memory is not freed by the arena until Reset() or the destructor is called.
}

// =====================================================================================================================
// Creates a new chunk and allocates from it
void* TempMemArena::AllocFromNewChunk(size_t size)
{
    size_t allocSize = size;

#if DEBUG
    allocSize += sizeof(Sentinel);
#endif

    allocSize = Util::Pow2Align(allocSize, VK_DEFAULT_MEM_ALIGN);

    const size_t chunkSize = Util::Max(m_chunkSize, allocSize);
    const size_t totalSize = sizeof(MemChunk) + chunkSize;

    MemChunk* pChunk = reinterpret_cast<MemChunk*>(m_allocator.pfnAllocation(
        m_allocator.pUserData, totalSize, VK_DEFAULT_MEM_ALIGN, m_allocScope));

    void* pData = nullptr;

    if (pChunk != nullptr)
    {
        pChunk->pNext          = m_pFirstAvailableChunk;
        pChunk->pData          = Util::VoidPtrInc(pChunk, sizeof(MemChunk));
        pChunk->capacity       = chunkSize;
        pChunk->tail           = 0;

#if DEBUG
        pChunk->pFirstSentinel = nullptr;
#endif

        m_pFirstAvailableChunk = pChunk;

        pData = AllocFromChunk(pChunk, size);

        VK_ASSERT(pData != nullptr);
    }

    return pData;
}

}; };
