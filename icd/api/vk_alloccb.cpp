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
 ***********************************************************************************************************************
 * @file  vk_alloccb.cpp
 * @brief Contains memory allocation callback functions.
 ***********************************************************************************************************************
 */

#include "include/vk_alloccb.h"
#include "include/vk_utils.h"

#include "palSysMemory.h"

#include <new>

#if defined(__unix__)
#include <unistd.h>
#endif

namespace vk
{

namespace allocator
{

// ===============================================================================================
// Default memory allocation callback used when application does not supply a callback function
// of its own. Currently just punts to stock new []. Alignment and alloc types are ignored.
void* VKAPI_PTR
    DefaultAllocFunc(
    void*                                   pUserData,
    size_t                                  size,
    size_t                                  alignment,
    VkSystemAllocationScope                 allocType)
{
    void* pMemory;
#if _POSIX_VERSION >= 200112L
    // posix_memalign is unilaterally preferred over aligned_alloc for several reasons
    //  - Older versions of glibc have it (eg, for RHEL6)
    //  - Several shipping games override the global allocator, but use an old enough lib that aligned_alloc's aren't
    //    handled (exploding on free). This issue only appears on newer glibc due to some removed debug hooks that
    //    previously saved it despite the buggy tcmalloc_minimal.
    //    This occurs on DOTA2 and probably CS:GO.
    if (posix_memalign(&pMemory, Util::Pow2Align(alignment, sizeof(void*)), size))
    {
        pMemory = NULL;
    }
#else
#error "Unsupported platform"
#endif
    return pMemory;
}

// ===============================================================================================
// Default memory allocation callback used when application does not supply a callback function
// of its own. Since POSIX doesn't provide an aligned reallocation primitive, we don't support it
// either.
// If there's a future need to support it, reallocation could be implemented by prepending a
// metadata header to each allocation that contains the allocation size.
static void* VKAPI_PTR
    DefaultReallocFunc(
    void*                                   pUserData,
    void*                                   pOriginal,
    size_t                                  size,
    size_t                                  alignment,
    VkSystemAllocationScope                 allocType)
{
    VK_NEVER_CALLED();
    return nullptr;
}

// =====================================================================================================================
// Default memory free callback used when application does not supply a callback function of its own. Currently just
// punts to stock delete [].
void VKAPI_PTR DefaultFreeFunc(
    void*                                   pUserData,
    void*                                   pMem)
{
#if __STDC_VERSION__ >= 201112L
    free(pMem);
#elif _POSIX_VERSION >= 200112L
    free(pMem);
#else
#error "Unsupported platform"
#endif
}

// =====================================================================================================================
// Vulkan API style callback structure - points at default callbacks.
// [SPEC] is a pointer to an application-defined function,

void VKAPI_PTR DefaultAllocNotification(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    // according to the spec , this is callback function provided by the application and called by the implementation
    // may be left blank here
}

void VKAPI_PTR DefaultFreeNotification(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    // according to the spec , this is callback function provided by the application and called by the implementation
    // may be left blank here
}

extern const VkAllocationCallbacks g_DefaultAllocCallback =
{
    nullptr,
    DefaultAllocFunc,
    DefaultReallocFunc,
    DefaultFreeFunc,
    DefaultAllocNotification,
    DefaultFreeNotification
};

// =====================================================================================================================
// Delegation function that calls through to a Vulkan allocator on behalf of a PAL allocator callback. This allows PAL
// to call into the application's allocator callbacks.
void* PAL_STDCALL PalAllocFuncDelegator(
    void*                                   pClientData,
    size_t                                  size,
    size_t                                  alignment,
    Util::SystemAllocType                   allocType)
{
    const VkSystemAllocationScope allocTypes[] =
    {
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,    //mapping to AllocObject
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE,  //mapping to AllocInternal, assume long lifetime
        VK_SYSTEM_ALLOCATION_SCOPE_COMMAND,   //mapping to AllocInternalTemp, assume short life time
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT     //mapping to AllocInternalShader
    };

    const VkAllocationCallbacks* const pVkCallbacks = reinterpret_cast<const VkAllocationCallbacks*>(pClientData);

    VK_ASSERT(allocType >= Util::SystemAllocType::AllocObject);
    VK_ASSERT(allocType < (Util::SystemAllocType::AllocObject + VK_ARRAY_SIZE(allocTypes)));

    return pVkCallbacks->pfnAllocation(
        pVkCallbacks->pUserData,
        size,
        alignment,
        allocTypes[allocType - Util::SystemAllocType::AllocObject]);
}

// =====================================================================================================================
// The free component of the delegation callbacks.
void PAL_STDCALL PalFreeFuncDelegator(
    void* pClientData,
    void* pMem)
{
    const VkAllocationCallbacks* pVkCallbacks =
        reinterpret_cast<const VkAllocationCallbacks*>(pClientData);

    pVkCallbacks->pfnFree(pVkCallbacks->pUserData, pMem);
}

} // namespace allocator

// =====================================================================================================================
PalAllocator::PalAllocator(
    VkAllocationCallbacks* pCallbacks)
    :
#if PAL_MEMTRACK
    m_memTrackerAlloc(pCallbacks),
    m_memTracker(&m_memTrackerAlloc),
#endif
    m_pCallbacks(pCallbacks)
{
}

// =====================================================================================================================
void PalAllocator::Init()
{
#if PAL_MEMTRACK
    m_memTracker.Init();
#endif
}

// =====================================================================================================================
void* PalAllocator::Alloc(
    const Util::AllocInfo& allocInfo)
{
    void* pMem = nullptr;

#if PAL_MEMTRACK
    pMem = m_memTracker.Alloc(allocInfo);
#else
    pMem = allocator::PalAllocFuncDelegator(
        m_pCallbacks,
        allocInfo.bytes,
        allocInfo.alignment,
        allocInfo.allocType);

    if ((pMem != nullptr) && allocInfo.zeroMem)
    {
        memset(pMem, 0, allocInfo.bytes);
    }
#endif
    return pMem;
}

// =====================================================================================================================
void PalAllocator::Free(
    const Util::FreeInfo& freeInfo)
{
    if (freeInfo.pClientMem != nullptr)
    {
#if PAL_MEMTRACK
        m_memTracker.Free(freeInfo);
#else
        allocator::PalFreeFuncDelegator(m_pCallbacks, freeInfo.pClientMem);
#endif
    }
}

#if PAL_MEMTRACK
// =====================================================================================================================
void PalAllocator::MemTrackerAllocator::Free(
    const Util::FreeInfo& freeInfo)
{
    allocator::PalFreeFuncDelegator(m_pCallbacks, freeInfo.pClientMem);
}

// =====================================================================================================================
void* PalAllocator::MemTrackerAllocator::Alloc(const Util::AllocInfo& allocInfo)
{
    void* pMem = allocator::PalAllocFuncDelegator(
        m_pCallbacks,
        allocInfo.bytes,
        allocInfo.alignment,
        allocInfo.allocType);

    if ((pMem != nullptr) && allocInfo.zeroMem)
    {
        memset(pMem, 0, allocInfo.bytes);
    }

    return pMem;
}
#endif

} // namespace vk
