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
 * @file  virtual_stack_mgr.h
 * @brief Virtual stack manager class declaration.
 **************************************************************************************************
 */

#ifndef __VIRTUAL_STACK_MGR_H__
#define __VIRTUAL_STACK_MGR_H__

#pragma once

#include "include/vk_alloccb.h"

#include "pal.h"
#include "palLinearAllocator.h"
#include "palIntrusiveList.h"
#include "palMutex.h"

namespace vk
{

// Forward declarations
class Instance;

// Virtual stack allocator base type
typedef Util::VirtualLinearAllocatorWithNode VirtualStackAllocator;

// =====================================================================================================================
// Virtual stack frame helper class
class VirtualStackFrame final : public Util::LinearAllocatorAuto<VirtualStackAllocator>
{
public:
    VirtualStackFrame(VirtualStackAllocator* pAllocator)
        : Util::LinearAllocatorAuto<VirtualStackAllocator>(pAllocator, false)
    {}

    template <typename Elem> Elem* AllocArray(size_t arraySize)
    {
        Elem* pMem = PAL_NEW_ARRAY(Elem, arraySize, this, Util::SystemAllocType::AllocInternalTemp);

        VK_ASSERT(pMem != nullptr); // "virtual stack overflow"

        return pMem;
    }

    template <typename Elem> void FreeArray(const Elem* pArray)
    {
        PAL_DELETE_ARRAY(pArray, this);
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(VirtualStackFrame);
};

// =====================================================================================================================
// Virtual stack frame manager class
class VirtualStackMgr
{
public:
    static Pal::Result Create(
        Instance*           pInstance,
        VirtualStackMgr**   ppVirtualStackMgr);

    Pal::Result Init() { return Pal::Result::Success; }
    void Destroy();

    Pal::Result AcquireAllocator(VirtualStackAllocator** ppAllocator);
    void ReleaseAllocator(VirtualStackAllocator* pAllocator);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(VirtualStackMgr);

    VirtualStackMgr(Instance* pInstance);

    typedef Util::IntrusiveList<VirtualStackAllocator> VirtualStackList;

    Instance* const         m_pInstance;        // Vulkan instance the virtual stack manager belongs to

    VirtualStackList        m_stackList;        // List of available virtual stack allocators

    Util::Mutex             m_lock;             // Lock protecting concurrent access to the manager
};

} // namespace vk

#endif /* __VIRTUAL_STACK_MGR_H */
