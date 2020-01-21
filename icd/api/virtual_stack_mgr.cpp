/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  virtual_stack_mgr.cpp
 * @brief Virtual stack manager class implementation.
 **************************************************************************************************
 */

#include "include/virtual_stack_mgr.h"
#include "include/vk_instance.h"
#include "include/vk_conv.h"
#include "include/vk_utils.h"

#include "palIntrusiveListImpl.h"

namespace vk
{

constexpr size_t MaxVirtualStackSize = 256 * 1024;  // 256 kilobytes

// =====================================================================================================================
VirtualStackMgr::VirtualStackMgr(
    Instance* pInstance)
  : m_pInstance(pInstance)
{
}

// =====================================================================================================================
// Creates the virtual stack manager.
Pal::Result VirtualStackMgr::Create(
    Instance*           pInstance,
    VirtualStackMgr**   ppVirtualStackMgr)
{
    VK_ASSERT(pInstance != nullptr);

    // Allocate the virtual stack manager
    void* pMemory = pInstance->AllocMem(sizeof(VirtualStackMgr), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    VirtualStackMgr* pNewVirtualStackMgr = (pMemory != nullptr) ?
                                           VK_PLACEMENT_NEW(pMemory) VirtualStackMgr (pInstance) :
                                           nullptr;

    if (pNewVirtualStackMgr != nullptr)
    {
        // Initialize the virtual stack manager
        Pal::Result palResult = pNewVirtualStackMgr->Init();

        if (palResult == Pal::Result::Success)
        {
            // Return the created object
            *ppVirtualStackMgr = pNewVirtualStackMgr;
        }
        else
        {
            // Failed to initialize, free the object
            Util::Destructor(pNewVirtualStackMgr);
            pInstance->FreeMem(pNewVirtualStackMgr);
        }

        return palResult;
    }
    else
    {
        return Pal::Result::ErrorOutOfMemory;
    }
}

// =====================================================================================================================
// Initializes the virtual stack manager.
Pal::Result VirtualStackMgr::Init()
{
    // Initialize the lock
    return m_lock.Init();
}

// =====================================================================================================================
// Tears down the virtual stack manager.
void VirtualStackMgr::Destroy()
{
    // Release all virtual stack allocators
    while (m_stackList.IsEmpty() == false)
    {
        auto iter = m_stackList.Begin();

        VirtualStackAllocator* pAllocator = iter.Get();

        m_stackList.Erase(&iter);

        PAL_DELETE(pAllocator, m_pInstance->Allocator());
    }

    // Free the memory used by the object
    Util::Destructor(this);
    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
// Acquires a virtual stack allocator.
Pal::Result VirtualStackMgr::AcquireAllocator(
    VirtualStackAllocator** ppAllocator)
{
    Util::MutexAuto lock(&m_lock);

    Pal::Result palResult = Pal::Result::Success;

    // Reuse an existing allocator if possible; otherwise create a new one
    if (m_stackList.IsEmpty() == false)
    {
        auto iter = m_stackList.Begin();

        // Just return the first available stack allocator
        *ppAllocator = iter.Get();

        // Remove the selected stack allocator from the list of the available ones
        m_stackList.Erase(&iter);
    }
    else
    {
        // Create a new stack allocator
        VirtualStackAllocator* pAllocator = PAL_NEW(VirtualStackAllocator,
            m_pInstance->Allocator(), Util::AllocInternal) (MaxVirtualStackSize);

        if (pAllocator != nullptr)
        {
            // Initialize it
            palResult = pAllocator->Init();

            if (palResult == Pal::Result::Success)
            {
                // If the initialization is successful then return this object
                *ppAllocator = pAllocator;
            }
            else
            {
                // If initialization failed then free the allocator
                PAL_DELETE(pAllocator, m_pInstance->Allocator());
            }
        }
        else
        {
            // Failed to create the new stack allocator object, return appropriate error
            palResult = Pal::Result::ErrorOutOfMemory;
        }
    }

    return palResult;
}

// =====================================================================================================================
// Releases a virtual stack allocator.
void VirtualStackMgr::ReleaseAllocator(
    VirtualStackAllocator* pAllocator)
{
    Util::MutexAuto lock(&m_lock);

    VK_ASSERT(pAllocator != nullptr);

    // Simply put the allocator to the front of the list of available stack allocators
    m_stackList.PushFront(pAllocator->GetNode());
}

} // namespace vk
