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
 * @file  vk_utils.h
 * @brief Utility functions for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_UTILS_H__
#define __VK_UTILS_H__

#pragma once

#include "vk_defines.h"
#include "include/khronos/vulkan.h"
#include "pal.h"
#include "palAssert.h"
#include "palSysUtil.h"
#include "palSysMemory.h"
#include "palFormatInfo.h"
#include "palCmdBuffer.h"
#include "palHashLiteralString.h"

#include <cwchar>
#include <cctype>
#include <type_traits>
#include <malloc.h>

#if defined(__unix__)
#include <unistd.h>
#include <linux/limits.h>
#else
#define PATH_MAX 512
#endif

// Reuse some PAL macros here
#define VK_ASSERT PAL_ASSERT
#define VK_ASSERT_MSG PAL_ASSERT_MSG
#define VK_DEBUG_BUILD_ONLY_ASSERT PAL_DEBUG_BUILD_ONLY_ASSERT
#define VK_ALERT PAL_ALERT
#define VK_ALERT_ALWAYS_MSG PAL_ALERT_ALWAYS_MSG
#define VK_SOFT_ASSERT(expr) VK_ALERT(!(expr))
#define VK_NEW PAL_NEW
#define VK_PLACEMENT_NEW PAL_PLACEMENT_NEW
#define VK_NOT_IMPLEMENTED do { PAL_NOT_IMPLEMENTED(); } while (0)
#define VK_NEVER_CALLED PAL_NEVER_CALLED
#define VK_NOT_TESTED PAL_NOT_TESTED

#if DEBUG
#define VK_DBG_DECL(decl)      decl
#define VK_DBG_EXPR(expr)      expr
#define VK_DBG_CHECK(cond,msg) VK_ASSERT((cond) && (msg))
#else
#define VK_DBG_DECL(decl)
#define VK_DBG_EXPR(expr)
#define VK_DBG_CHECK(cond,msg)
#endif

#if   defined(__GNUG__)
#define VK_FORCEINLINE __attribute__((always_inline))
#else
#define VK_FORCEINLINE inline
#endif

// Wrap _malloca and _freea for compilers other than MSVS
#define VK_ALLOC_A(_numBytes) alloca(_numBytes)

// Default alignment for memory allocation
#define VK_DEFAULT_MEM_ALIGN 16

#define VK_ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))

#define VK_ENUM_IN_RANGE(value, type)   (((value) >= type##_BEGIN_RANGE) && ((value) <= type##_END_RANGE))
#define VK_ENUM_IN_RANGE_AMD(value, type)   (((value) >= type##_BEGIN_RANGE_AMD) && ((value) <= type##_END_RANGE_AMD))

#define NANOSECONDS_IN_A_SECOND      1000000000ull

namespace vk
{

// This structure represents the common data that's at the front of almost all
// Vulkan API structures. It can be used to walk opaque lists of structure definitions.
// New Vulkan structures should probably include this at the front of them rather than
// explicitly including sType and pNext. For now, stick it here although we could
// promote it to the Vulkan header.
struct VkStructHeader
{
    VkStructureType         sType;
    const VkStructHeader*   pNext;
};

struct VkStructHeaderNonConst
{
    VkStructureType          sType;
    VkStructHeaderNonConst*  pNext;
};

#if VKI_RAY_TRACING
constexpr uint32_t RayTraceShaderStages =
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
    VK_SHADER_STAGE_CALLABLE_BIT_KHR;
#endif

typedef VkPipelineStageFlags2KHR PipelineStageFlags;
typedef VkAccessFlags2KHR        AccessFlags;

namespace utils
{

inline uint64_t TicksToNano(uint64_t ticks)
{
    return (ticks * NANOSECONDS_IN_A_SECOND) / static_cast<Pal::uint64>(Util::GetPerfFrequency());
}

// =====================================================================================================================
// Get driver build time hash
uint32_t GetBuildTimeHash();

#if DEBUG
// =====================================================================================================================
// If turned on and exe name is a match, this function spins idle until we have a debugger hooked.
void WaitIdleForDebugger(bool waitIdleToggled, const char* pWaitIdleExeName, uint32_t debugTimeout);
#endif

// =====================================================================================================================
// This function can be used to get the right externsion structure of specific type in case there are more than one
// extension is supported
inline const VkStructHeader* GetExtensionStructure(const VkStructHeader* pHeader, VkStructureType sType)
{
    const VkStructHeader* pIter = pHeader;
    while(pIter != nullptr)
    {
        if (pIter->sType == sType)
        {
            return pIter;
        }
        else
        {
            pIter = pIter->pNext;
        }
    }
    return nullptr;
}

// =====================================================================================================================
template<typename ExtStruct, typename SrcStruct>
const ExtStruct* GetExtensionStructure(
    const SrcStruct* pHeader,
    VkStructureType  sType)
{
    return reinterpret_cast<const ExtStruct*>(
        GetExtensionStructure(reinterpret_cast<const VkStructHeader*>(pHeader), sType));
}

// =====================================================================================================================
// Returns the number of indices of a particular index type that fit into a buffer of the given byte-size.
inline uint32_t BufferSizeToIndexCount(Pal::IndexType indexType, VkDeviceSize bufferSize)
{
    static_assert((static_cast<int32_t>(Pal::IndexType::Idx8)  == 0) &&
                  (static_cast<int32_t>(Pal::IndexType::Idx16) == 1) &&
                  (static_cast<int32_t>(Pal::IndexType::Idx32) == 2),
                  "Pal::IndexType enum has changed, need to update this function");

    return static_cast<uint32_t>(bufferSize >> static_cast<uint32_t>(indexType));
}

// =====================================================================================================================
inline void GetExecutableNameAndPath(wchar_t* pExecutableName, wchar_t* pExecutablePath)
{
    // Get the wchar_t executable name and path
    wchar_t  executableNameAndPathBuffer[PATH_MAX];

    wchar_t* pExecutablePtr;
    Pal::Result palResult = Util::GetExecutableName(&executableNameAndPathBuffer[0],
                                                    &pExecutablePtr,
                                                    PATH_MAX);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Extract the executable path and add the null terminator
    const size_t executablePathLength = static_cast<size_t>(pExecutablePtr - executableNameAndPathBuffer);
    memcpy(pExecutablePath, executableNameAndPathBuffer, executablePathLength * sizeof(wchar_t));
    pExecutablePath[executablePathLength] = L'\0';

    // Copy the executable name and add the null terminator
    const size_t executableNameLength = wcslen(executableNameAndPathBuffer) - executablePathLength;
    memcpy(pExecutableName, pExecutablePtr, executableNameLength * sizeof(wchar_t));
    pExecutableName[executableNameLength] = L'\0';
}

// =====================================================================================================================
inline void GetExecutableNameAndPath(char* pExecutableName, char* pExecutablePath)
{
    // Get the executable name and path
    char  executableNameAndPathBuffer[PATH_MAX];

    char* pExecutablePtr;
    Pal::Result palResult = Util::GetExecutableName(&executableNameAndPathBuffer[0],
                                                    &pExecutablePtr,
                                                    sizeof(executableNameAndPathBuffer));
    VK_ASSERT(palResult == Pal::Result::Success);

    // Extract the executable path and add the null terminator
    const size_t executablePathLength = static_cast<size_t>(pExecutablePtr - executableNameAndPathBuffer);
    memcpy(pExecutablePath, executableNameAndPathBuffer, executablePathLength * sizeof(char));
    pExecutablePath[executablePathLength] = '\0';

    // Copy the executable name and add the null terminator
    const size_t executableNameLength = strlen(executableNameAndPathBuffer) - executablePathLength;
    memcpy(pExecutableName, pExecutablePtr, executableNameLength * sizeof(char));
    pExecutableName[executableNameLength] = '\0';
}

// =====================================================================================================================
inline int StrCmpCaseInsensitive(
    const char* a,
    const char* b)
{
    while (true)
    {
        const char ac = tolower(*a);
        const char bc = tolower(*b);

        if ((ac != bc) || (ac == '\0') || (bc == '\0'))
        {
            if (ac == bc)
            {
                return 0;
            }
            else
            {
                return (ac < bc) ? -1 : 1;
            }
        }

        a++;
        b++;
    }
}

// =====================================================================================================================
// Return true if Big Software Release 6.0 is supported.
inline bool BigSW60Supported(const Pal::BigSoftwareReleaseInfo& bigSwInfo)
{
    return ((bigSwInfo.majorVersion > 2019) ||
            ((bigSwInfo.majorVersion == 2019) && (bigSwInfo.minorVersion >= 1)));
}

// =====================================================================================================================
class IterateMask
{
public:
    IterateMask(uint32_t mask) :
        m_index(0),
        m_mask(mask)
    {
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        if (Util::BitMaskScanForward(&m_index, m_mask) == true)
        {
            m_mask ^= (1 << m_index);
        }
#endif
    }

    bool IterateNext()
    {
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        if (Util::BitMaskScanForward(&m_index, m_mask) == true)
        {
            m_mask ^= (1 << m_index);
            return true;
        }
#endif
        return false;
    }

    uint32_t Index() const
    {
        return m_index;
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(IterateMask);

    uint32_t    m_index;
    uint32_t    m_mask;
};

// =====================================================================================================================
// A "view" into an array of elements that are not tightly packed in memory. The use case is iterating over structures
// nested in an array of structures, e.g. VkSparseImageMemoryRequirements inside VkSparseImageMemoryRequirements2.
template<class ElementT>
class ArrayView
{
    // The constness of char* must match that of ElementT*
    template<class T> struct InferCharType          { typedef       char type; };
    template<class T> struct InferCharType<const T> { typedef const char type; };

    using CharT = typename InferCharType<ElementT>::type;

public:
    // Create a view into an array of ElementT with stride determined by OuterT.
    template<class OuterT>
    ArrayView(OuterT* pData, ElementT* pFirstElement) :
        m_pData(reinterpret_cast<CharT*>(pData)),
        m_stride(sizeof(OuterT))
    {
        if (m_pData != nullptr)
        {
            const auto offset = reinterpret_cast<CharT*>(pFirstElement) - m_pData;

            VK_ASSERT((offset >= 0) && ((offset + sizeof(ElementT)) <= sizeof(OuterT)));

            m_pData += offset;
        }
    }

    // Use this form to achieve tight packing of elements, if needed.
    explicit ArrayView(ElementT* pData) :
        m_pData(reinterpret_cast<CharT*>(pData)),
        m_stride(sizeof(ElementT))
    {
    }

    bool IsNull() const
    {
        return m_pData == nullptr;
    }

    ElementT& operator[](int32_t ndx) const
    {
        return *reinterpret_cast<ElementT*>(m_pData + ndx * m_stride);
    }

private:
    CharT*  m_pData;
    size_t  m_stride;
};

// =====================================================================================================================
// PlacementHelper is a utility to lay out objects in a block of memory.
//
// Example usage:
//
//   int*     pMyInts   = nullptr;
//   float*   pMyFloats = nullptr;
//   IObject* pMyObject = nullptr;
//
//   auto placement = PlacementHelper<3>(               // number of elements
//     nullptr,                                         // placement base pointer, use nullptr to determine size first
//     PlacementElement<int>    {&pMyInts,   6},        // place 6 integers at pMyInts
//     PlacementElement<float>  {&pMyFloats, 4},        // place 4 floats at pMyFloats
//     PlacementElement<IObject>{&pMyObject, 2, 64});   // place a block of memory with 2 * 64 size (see note below)
//
//   auto pMemory = malloc(placement.SizeOf());         // allocate memory of required size
//
//   placement.FixupPtrs(pMemory);                      // assign correct values to pMy* pointers
//
// NOTE: If an explicit size is given, as in the pMyObject example, remember that the pointer can no longer be treated
//       as an array of IObject. It's just a pointer to a block of memory with IObject* type and 128 bytes of storage.
//       If the assumption is that a second IObject is placed at the offset 64, then that pointer must be computed
//       manually, e.g. by the use of Util::VoidPtrInc().
//
//       If the correct alignment is important, make sure that the pointer passed to FixupPtrs() is itself aligned
//       to the largest required alignment among the placed objects.
//
template<class T>
struct PlacementElement
{
    using Type = T;

    T**    outPtr    = nullptr;    // destination pointer where the objects are placed
    size_t count     = 1;          // number of objects to place at the pointer
    size_t size      = 0;          // optional object size, otherwise 0 means sizeof(T)
    size_t alignment = 0;          // optional object alignment (must be power of 2), otherwise 0 means alignof(T)
};

template<size_t ElementCount>
class PlacementHelper
{
public:
    template<class... Element>
    PlacementHelper(void* basePtr, Element... elements)
    {
        // sizeof ... (type) counts the number of types in the parameter pack (not byte size).
        static_assert(ElementCount == (sizeof ... (Element)), "Wrong number of elements");

        // Start unpacking the arguments recursively.
        ExpandCtorArguments(basePtr, 0, 0, elements...);
    }

    size_t SizeOf() const
    {
        return m_totalSize;
    }

    void FixupPtrs(void* basePtr) const
    {
        // If the layout has been done with a basePtr == nullptr, the pointers will be offsets from zero.
        // Here we can move them relative to the correct memory base offset.
        // This is typically needed if we allocate memory after determining the layout and size requirements.
        for (uint32_t ndx = 0; ndx < ElementCount; ++ndx)
        {
            *m_outPtrs[ndx] = Util::VoidPtrInc(basePtr, reinterpret_cast<uintptr_t>(*m_outPtrs[ndx]));
        }
    }

private:
    template<class FirstElement, class... Element>
    void ExpandCtorArguments(void* basePtr, size_t idx, size_t offset, FirstElement head, Element... tail)
    {
        using HeadType = typename FirstElement::Type;

        // basePtr *may* be a nullptr.
        // head.count *may* be 0
        VK_ASSERT(head.outPtr != nullptr);

        // If no explicit size is given, we derive the size from the element type. Keep track of the total size.
        size_t size = (head.size != 0) ? head.size : sizeof(HeadType);
        size *= head.count;
        m_totalSize += size;

        // Ensure the placement offset is aligned for this type
        const size_t alignment = (head.alignment != 0) ? head.alignment : alignof(HeadType);
        const size_t offsetMisalignment = Util::Pow2Align(offset, alignment) - offset;

        m_totalSize += offsetMisalignment;
        offset += offsetMisalignment;

        // Save the output pointer in case we need to modify it later.
        // Then write the output pointer anyway (we need to save the offsets somewhere at least).
        m_outPtrs[idx] = reinterpret_cast<void**>(head.outPtr);
        *head.outPtr = static_cast<HeadType*>(Util::VoidPtrInc(basePtr, offset));

        // Process the next element.
        ExpandCtorArguments(basePtr, idx + 1, offset + size, tail...);
    }

    void ExpandCtorArguments(const void* basePtr, size_t idx, size_t offset)
    {
        // All elements have been already consumed.
    }

    size_t  m_totalSize = 0;
    void**  m_outPtrs[ElementCount] = {};
};

template<typename T>
constexpr T StaticMax(T a, T b) { return (a > b) ? a : b; };

} // namespace utils

} // namespace vk

#endif /* __VK_RESULT_H__ */
