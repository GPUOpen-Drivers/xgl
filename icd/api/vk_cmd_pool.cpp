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
 * @file  vk_cmd_pool.cpp
 * @brief Implementation of Vulkan command buffer pool class.
 **************************************************************************************************
 */

#include "include/vk_cmd_pool.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_instance.h"
#include "include/vk_device.h"
#include "include/vk_conv.h"

#include "palFile.h"
#include "palHashSetImpl.h"
#include "palIntrusiveListImpl.h"
#include "palVectorImpl.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

namespace vk
{

// =====================================================================================================================
CmdPool::CmdPool(
    Device*                      pDevice,
    Pal::ICmdAllocator**         pPalCmdAllocators,
    const VkAllocationCallbacks* pAllocator,
    uint32_t                     queueFamilyIndex,
    VkCommandPoolCreateFlags     flags,
    bool                         sharedCmdAllocator)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_queueFamilyIndex(queueFamilyIndex),
    m_cmdBufferRegistry(32, pDevice->VkInstance()->Allocator()),
    m_cmdBuffersAlreadyBegun(32, pDevice->VkInstance()->Allocator())
{
    m_flags.u32All = 0;

    if (flags & VK_COMMAND_POOL_CREATE_PROTECTED_BIT)
    {
        m_flags.isProtected = true;
    }
    if (flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    {
        m_flags.isResetCmdBuffer = true;
    }

    m_flags.sharedCmdAllocator = sharedCmdAllocator;

    memcpy(m_pPalCmdAllocators, pPalCmdAllocators, sizeof(pPalCmdAllocators[0]) * pDevice->NumPalDevices());
}

// =====================================================================================================================
// Initializes the command buffer pool object.
VkResult CmdPool::Init()
{
    Pal::Result palResult = m_cmdBufferRegistry.Init();

    if (palResult == Pal::Result::Success)
    {
        palResult = m_cmdBuffersAlreadyBegun.Init();
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult CmdPool::Create(
    Device*                        pDevice,
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkCommandPool*                 pCmdPool)
{
    const RuntimeSettings* pSettings = &pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();

    void* pMemory   = nullptr;
    VkResult result = VK_SUCCESS;

    Pal::ICmdAllocator* pPalCmdAllocator[MaxPalDevices] = {};

    if (pSettings->useSharedCmdAllocator)
    {
        // Use the per-device shared CmdAllocator if the settings indicate so.
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            pPalCmdAllocator[deviceIdx] = pDevice->GetSharedCmdAllocator(deviceIdx);
        }

        pMemory = pDevice->AllocApiObject(pAllocator, sizeof(CmdPool));

        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    else
    {
        // Create a private CmdAllocator for this command buffer pool. As the application can only use a CmdPool
        // object in a single thread at any given time, we don't need a thread safe CmdAllocator.
        Pal::CmdAllocatorCreateInfo createInfo = { };

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

        // Initialize large embedded data chunk allocation size
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocHeap    = pSettings->cmdAllocatorEmbeddedHeap;
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocSize    =
            pSettings->cmdAllocatorLargeEmbeddedAllocSize;
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].suballocSize =
            pSettings->cmdAllocatorLargeEmbeddedSubAllocSize;

        // Initialize GPU scratch memory chunk allocation size
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocHeap    = pSettings->cmdAllocatorScratchHeap;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocSize    = pSettings->cmdAllocatorScratchAllocSize;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].suballocSize = pSettings->cmdAllocatorScratchSubAllocSize;

        Pal::Result  palResult     = Pal::Result::Success;
        const size_t allocatorSize = pDevice->PalDevice(DefaultDeviceIndex)->GetCmdAllocatorSize(createInfo, &palResult);

        if (palResult == Pal::Result::Success)
        {
            size_t apiSize = sizeof(CmdPool);
            size_t palSize = allocatorSize * pDevice->NumPalDevices();

            pMemory = pDevice->AllocApiObject(pAllocator, apiSize + palSize);

            if (pMemory != NULL)
            {
                void* pAllocatorMem = Util::VoidPtrInc(pMemory, apiSize);

                for (uint32_t deviceIdx = 0;
                    (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                    deviceIdx++)
                {
                    palResult = pDevice->PalDevice(deviceIdx)->CreateCmdAllocator(
                        createInfo,
                        Util::VoidPtrInc(pAllocatorMem, allocatorSize * deviceIdx),
                        &pPalCmdAllocator[deviceIdx]);
                }

                result = PalToVkResult(palResult);

                if (result != VK_SUCCESS)
                {
                    pDevice->FreeApiObject(pAllocator, pMemory);
                    pMemory = nullptr;
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
            pAllocator,
            pCreateInfo->queueFamilyIndex,
            pCreateInfo->flags,
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

    return result;
}

// =====================================================================================================================
// Destroy a command buffer pool object
VkResult CmdPool::Destroy(
    Device*                         pDevice,
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
    if (m_flags.sharedCmdAllocator == 0)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            m_pPalCmdAllocators[deviceIdx]->Destroy();
        }
    }

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Resets the PAL command allocators
VkResult CmdPool::ResetCmdAllocator(
    bool releaseResources)
{
    Pal::Result result = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
        deviceIdx++)
    {
        result = m_pPalCmdAllocators[deviceIdx]->Reset(releaseResources);
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Reset a command buffer pool object
VkResult CmdPool::Reset(
    VkCommandPoolResetFlags flags)
{
    VkResult result = VK_SUCCESS;

    m_cmdPoolResetInProgress = true;

    // Reset all command buffers in the pool when individual command buffer reset is selected for this pool.  Otherwise,
    // only reset the command buffers that were begun and not already reset (PAL doesn't do this automatically).
    if (IsResetCmdBuffer())
    {
        for (auto it = m_cmdBufferRegistry.Begin(); (it.Get() != nullptr) && (result == VK_SUCCESS); it.Next())
        {
            // Per-spec we always have to do a command buffer reset that also releases the used resources.
            result = it.Get()->key->Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        }
    }
    else
    {
        for (auto it = m_cmdBuffersAlreadyBegun.Begin(); (it.Get() != nullptr) && (result == VK_SUCCESS); it.Next())
        {
            // Per-spec we always have to do a command buffer reset that also releases the used resources.
            result = it.Get()->key->Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        }

        // Clear the set of command buffers to reset. Only done if all the buffers were reset successfully so it is
        // possible that after an error this set will contain already reset command buffers. This is fine because we
        // can reset command buffers twice.
        if ((result == VK_SUCCESS) && (m_cmdBuffersAlreadyBegun.GetNumEntries() > 0))
        {
            m_cmdBuffersAlreadyBegun.Reset();
        }
    }

    if (result == VK_SUCCESS)
    {
        // After resetting the registered command buffers, reset the pool itself but only if we use per-pool
        // CmdAllocator objects, not a single shared one.
        if (m_flags.sharedCmdAllocator == 0)
        {
            const bool releaseResources = ((flags & VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != 0);

            result = ResetCmdAllocator(releaseResources);
        }
    }

    m_cmdPoolResetInProgress = false;

    return result;
}

// =====================================================================================================================
void CmdPool::Trim()
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        m_pPalCmdAllocators[deviceIdx]->Trim(((1 << Pal::CmdAllocatorTypeCount) - 1), 0);
    }
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
    UnmarkCmdBufBegun(pCmdBuffer);
    m_cmdBufferRegistry.Erase(pCmdBuffer);
}

// =====================================================================================================================
// Adds command buffer to the set of command buffers needing explicit reset when this cmd pool is reset.
Pal::Result CmdPool::MarkCmdBufBegun(
    CmdBuffer* pCmdBuffer)
{
    Pal::Result result = Pal::Result::Success;

    if (IsResetCmdBuffer() == false)
    {
        result = m_cmdBuffersAlreadyBegun.Insert(pCmdBuffer);
    }

    return result;
}

// =====================================================================================================================
// Removes command buffer from the set of command buffers needint explicit reset when this cmd pool is reset.
void CmdPool::UnmarkCmdBufBegun(
    CmdBuffer* pCmdBuffer)
{
    // Skip erasing individual command buffers during command pool reset as the command pool reset will instead reset
    // the entire HashSet all at once after all individual command buffer resets are completed.
    if ((IsResetCmdBuffer() == false) && (m_cmdPoolResetInProgress == false))
    {
        m_cmdBuffersAlreadyBegun.Erase(pCmdBuffer);
    }
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
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
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

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkTrimCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolTrimFlags                      flags)
{
    CmdPool::ObjectFromHandle(commandPool)->Trim();
}

} // namespace entry

} // namespace vk
