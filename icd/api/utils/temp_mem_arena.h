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
/**
**************************************************************************************************
* @file  temp_mem_arena.h
* @brief An object for allocating temporary memory (released when object is destroyed).
**************************************************************************************************
*/
#ifndef __UTILS_TEMP_MEM_ARENA_H__
#define __UTILS_TEMP_MEM_ARENA_H__
#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"

#include "palSysMemory.h"

namespace vk
{

class Instance;

namespace utils
{

// =====================================================================================================================
// This is a class for allocating short-term temporary memory for the purpose of constructing objects.  It only
// allocates memory and does not free it until this object is destroyed.  A pointer to this object can be used as a
// PAL-compatible allocator.
struct TempMemArena
{
    TempMemArena(const VkAllocationCallbacks* pAllocator, VkSystemAllocationScope allocScope);
    ~TempMemArena();

    void* Alloc(size_t size);
    void* Alloc(const Util::AllocInfo& allocInfo);
    void  Free(const Util::FreeInfo& freeInfo);
    void  Reset();
    size_t GetTotalAllocated() const { return m_totalMemSize; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(TempMemArena);

#if DEBUG
    struct Sentinel
    {
        uint32_t  value;
        uint32_t  id;
        Sentinel* pNext;
    };
#endif

    // A chunk of memory for amortizing the cost of memory allocation
    struct MemChunk
    {
        size_t    capacity; // Total number of bytes in the chunk
        size_t    tail;     // First free byte in the chunk
        void*     pData;    // Pointer to the start of the chunk
        MemChunk* pNext;    // Next chunk pointer

#if DEBUG
        Sentinel* pFirstSentinel;
#endif
    };

    void* AllocFromNewChunk(size_t size);
    void* AllocFromChunk(MemChunk* pChunk, size_t size);
    void ResetChunk(MemChunk* pChunk);
    void FreeChunks(MemChunk* pChunk);

#if DEBUG
    void CheckSentinels(const MemChunk* pChunk) const;
#endif

    VkAllocationCallbacks        m_allocator;            // Allocation callback
    VkSystemAllocationScope      m_allocScope;           // Type of allocations being made
    size_t                       m_totalMemSize;         // Total amount of memory allocated since last reset
    size_t                       m_chunkSize;            // Minimum size of a chunk
    MemChunk*                    m_pFirstAvailableChunk; // List of empty memory chunks
    MemChunk*                    m_pFirstUsedChunk;      // List of full memory chunks
#if DEBUG
    uint32_t                m_nextAllocId;
#endif
};

};

};

#endif /* __UTILS_TEMP_MEM_ARENA_H__ */
