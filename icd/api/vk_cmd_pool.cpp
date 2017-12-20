/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 **************************************************************************************************
 * @file  vk_cmd_pool.cpp
 * @brief Implementation of Vulkan command buffer pool class.
 **************************************************************************************************
 */

#include "include/vk_cmd_pool.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_instance.h"
#include "include/vk_device.h"
#include "include/vk_conv.h"

#include "palHashSetImpl.h"
#include "palIntrusiveListImpl.h"

namespace vk
{

// =====================================================================================================================
CmdPool::CmdPool(
    Device*              pDevice,
    Pal::ICmdAllocator** pPalCmdAllocators,
    uint32_t             queueFamilyIndex,
    bool                 sharedCmdAllocator)
    :
    m_pDevice(pDevice),
    m_queueFamilyIndex(queueFamilyIndex),
    m_sharedCmdAllocator(sharedCmdAllocator),
    m_cmdBufferRegistry(32, pDevice->VkInstance()->Allocator()),
    m_totalEventMgrCount(0)
{
    memcpy(m_pPalCmdAllocators, pPalCmdAllocators, sizeof(pPalCmdAllocators[0]) * pDevice->NumPalDevices());
}

// =====================================================================================================================
// Initializes the command buffer pool object.
VkResult CmdPool::Init()
{
    Pal::Result palResult = m_cmdBufferRegistry.Init();

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult CmdPool::Create(
    Device*                        pDevice,
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkCommandPool*                 pCmdPool)
{
    const RuntimeSettings* pSettings = &pDevice->VkPhysicalDevice()->GetRuntimeSettings();

    void* pMemory = pDevice->AllocApiObject(sizeof(CmdPool), pAllocator);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkResult result = VK_SUCCESS;

    Pal::ICmdAllocator* pPalCmdAllocator[MaxPalDevices] = {};

    if (pSettings->useSharedCmdAllocator)
    {
        // Use the per-device shared CmdAllocator if the settings indicate so.
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            pPalCmdAllocator[deviceIdx] = pDevice->GetSharedCmdAllocator(deviceIdx);
        }
    }
    else
    {
        // Create a private CmdAllocator for this command buffer pool. As the application can only use a CmdPool
        // object in a single thread at any given time, we don't need a thread safe CmdAllocator.
        Pal::CmdAllocatorCreateInfo createInfo = { };

        createInfo.flags.threadSafe               = 1;
        createInfo.flags.autoMemoryReuse          = 1;
        createInfo.flags.disableBusyChunkTracking = 1;

        // Initialize command data chunk allocation size
        createInfo.allocInfo[Pal::CommandDataAlloc].allocHeap    = pSettings->cmdAllocatorDataHeap;
        createInfo.allocInfo[Pal::CommandDataAlloc].allocSize    = pSettings->cmdAllocatorDataAllocSize;
        createInfo.allocInfo[Pal::CommandDataAlloc].suballocSize = pSettings->cmdAllocatorDataSubAllocSize;

        // Initialize embedded data chunk allocation size
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocHeap    = pSettings->cmdAllocatorEmbeddedHeap;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocSize    = pSettings->cmdAllocatorEmbeddedAllocSize;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].suballocSize = pSettings->cmdAllocatorEmbeddedSubAllocSize;

        Pal::Result  palResult     = Pal::Result::Success;
        const size_t allocatorSize = pDevice->PalDevice()->GetCmdAllocatorSize(createInfo, &palResult);

        if (palResult == Pal::Result::Success)
        {
            void* pAllocatorMem = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                allocatorSize * pDevice->NumPalDevices(),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

            if (pAllocatorMem != NULL)
            {
                for (uint32_t deviceIdx = 0;
                    (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                    deviceIdx++)
                {
                    palResult = pDevice->PalDevice(deviceIdx)->CreateCmdAllocator(
                        createInfo,
                        pAllocatorMem,
                        &pPalCmdAllocator[deviceIdx]);
                }

                result = PalToVkResult(palResult);

                if (result != VK_SUCCESS)
                {
                    pAllocator->pfnFree(pAllocator->pUserData, pAllocatorMem);
                }
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
        else
        {
            result = PalToVkResult(palResult);
        }
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) CmdPool(
            pDevice,
            pPalCmdAllocator,
            pCreateInfo->queueFamilyIndex,
            pSettings->useSharedCmdAllocator);

        VkCommandPool handle = CmdPool::HandleFromVoidPointer(pMemory);
        CmdPool* pApiCmdPool = CmdPool::ObjectFromHandle(handle);

        result = pApiCmdPool->Init();

        if (result == VK_SUCCESS)
        {
            *pCmdPool = handle;
        }
        else
        {
            pApiCmdPool->Destroy(pDevice, pAllocator);
        }
    }
    else
    {
        pAllocator->pfnFree(pAllocator->pUserData, pMemory);
    }

    return result;
}

// =====================================================================================================================
// Destroy a command buffer pool object
VkResult CmdPool::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // When a command pool is destroyed, all command buffers allocated from the pool are implicitly freed and
    // become invalid.
    while (m_cmdBufferRegistry.GetNumEntries() > 0)
    {
        CmdBuffer* pCmdBuf = m_cmdBufferRegistry.Begin().Get()->key;

        pCmdBuf->Destroy();
    }

    // If we don't use a shared CmdAllocator then we have to destroy our own one.
    if (m_sharedCmdAllocator == false)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            m_pPalCmdAllocators[deviceIdx]->Destroy();
        }

        pAllocator->pfnFree(pAllocator->pUserData, m_pPalCmdAllocators[DefaultDeviceIndex]);
    }

    DestroyGpuEventMgrs();

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
void CmdPool::DestroyGpuEventMgrs()
{
    while (m_freeEventMgrs.IsEmpty() == false)
    {
        VK_ASSERT(m_totalEventMgrCount > 0);

        m_totalEventMgrCount--;

        GpuEventMgr::List::Iter it = m_freeEventMgrs.Begin();
        GpuEventMgr* pEventMgr = it.Get();
        m_freeEventMgrs.Erase(&it);

        pEventMgr->Destroy();
        m_pDevice->VkInstance()->FreeMem(pEventMgr);
    }

    VK_ASSERT(m_totalEventMgrCount == 0);
}

// =====================================================================================================================
VkResult CmdPool::PalCmdAllocatorReset()
{
    Pal::Result result = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
        deviceIdx++)
    {
        result = m_pPalCmdAllocators[deviceIdx]->Reset();
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Reset a command buffer pool object
VkResult CmdPool::Reset(VkCommandPoolResetFlags flags)
{
    VkResult result = VK_SUCCESS;

    // There's currently no way to tell to the PAL CmdAllocator that it should release the actual allocations used
    // by the pool, it always just marks the allocations unused, so we currently ignore the
    // VK_CMD_POOL_RESET_RELEASE_RESOURCES flag if present.
    VK_IGNORE(flags & VK_CMD_POOL_RESET_RELEASE_RESOURCES);

    // We first have to reset all the command buffers that use this pool (PAL doesn't do this automatically).
    for (auto it = m_cmdBufferRegistry.Begin(); (it.Get() != nullptr) && (result == VK_SUCCESS); it.Next())
    {
        // Per-spec we always have to do a command buffer reset that also releases the used resources.

        result = it.Get()->key->Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }

    if (result == VK_SUCCESS)
    {
        // After resetting the registered command buffers, reset the pool itself but only if we use per-pool
        // CmdAllocator objects, not a single shared one.
        if (m_sharedCmdAllocator == false)
        {
            result = PalCmdAllocatorReset();
        }
    }

    return result;
}

// =====================================================================================================================
// Register a command buffer with this pool. Used to reset the command buffers at pool reset time.
Pal::Result CmdPool::RegisterCmdBuffer(CmdBuffer* pCmdBuffer)
{
    return m_cmdBufferRegistry.Insert(pCmdBuffer);
}

// =====================================================================================================================
// Unregister a command buffer from this pool.
void CmdPool::UnregisterCmdBuffer(CmdBuffer* pCmdBuffer)
{
    m_cmdBufferRegistry.Erase(pCmdBuffer);
}

// =====================================================================================================================
GpuEventMgr* CmdPool::AcquireGpuEventMgr()
{
    GpuEventMgr* pEventMgr = nullptr;

    if (!m_freeEventMgrs.IsEmpty())
    {
        GpuEventMgr::List::Iter it = m_freeEventMgrs.Begin();

        pEventMgr = it.Get();

        m_freeEventMgrs.Erase(&it);
    }

    if (pEventMgr == nullptr)
    {
        void* pMemory = m_pDevice->VkInstance()->AllocMem(
            sizeof(GpuEventMgr),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMemory != nullptr)
        {
            pEventMgr = VK_PLACEMENT_NEW(pMemory) GpuEventMgr(m_pDevice);

            m_totalEventMgrCount++;
        }
    }

    return pEventMgr;
}

// =====================================================================================================================
void CmdPool::ReleaseGpuEventMgr(GpuEventMgr* pGpuEventMgr)
{
    VK_ASSERT(pGpuEventMgr->ListNode()->InList() == false);

    pGpuEventMgr->ResetEvents();

    m_freeEventMgrs.PushBack(pGpuEventMgr->ListNode());
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    const VkAllocationCallbacks*                pAllocator)
{
    if (commandPool != VK_NULL_HANDLE)
    {
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        CmdPool::ObjectFromHandle(commandPool)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolResetFlags                     flags)
{
    return CmdPool::ObjectFromHandle(commandPool)->Reset(flags);
}

} // namespace entry

} // namespace vk
