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
 * @file  vk_event.cpp
 * @brief Contains implementation of Vulkan event objects.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_event.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"

#include "palGpuEvent.h"

namespace vk
{

// =====================================================================================================================
// This is the implementation of constructor of event.
Event::Event(
    Device*          pDevice,
    uint32_t         numDeviceEvents,
    Pal::IGpuEvent** pPalEvents,
    bool             useToken)
    :
    m_internalGpuMem(),
    m_useToken(useToken)
{
    if (useToken)
    {
        m_syncToken = 0;
    }
    else
    {
        memcpy(m_pPalEvents, pPalEvents, sizeof(Pal::IGpuEvent*) * numDeviceEvents);
    }
}

// =====================================================================================================================
// Create a new event object. This is the implementation of vkCreateEvent.
VkResult Event::Create(
    Device*                         pDevice,
    const VkEventCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkEvent*                        pEvent)
{
    const uint32_t numDeviceEvents               = pDevice->NumPalDevices();

    Pal::IGpuEvent* pPalGpuEvents[MaxPalDevices] = {};
    InternalMemory  internalGpuMem               = {};
    VkResult result                              = VK_SUCCESS;
    Pal::Result palResult                        = Pal::Result::Success;
    bool useToken                                = false;

    Pal::DeviceProperties info;
    pDevice->PalDevice(DefaultDeviceIndex)->GetProperties(&info);

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    // If supportReleaseAcquireInterface is true, the ASIC provides new barrier interface CmdReleaseThenAcquire()
    // designed for Acquire/Release-based driver. This flag is currently enabled for gfx9 and above.
    // If supportSplitReleaseAcquire is true, the ASIC provides split CmdRelease() and CmdAcquire() to express barrier,
    // and CmdReleaseThenAcquire() is still valid. This flag is currently enabled for gfx10 and above.
    bool useSplitReleaseAcquire = info.gfxipProperties.flags.supportReleaseAcquireInterface &&
                                  info.gfxipProperties.flags.supportSplitReleaseAcquire &&
                                  settings.useAcquireReleaseInterface;

    if (useSplitReleaseAcquire && settings.syncTokenEnabled &&
        ((pCreateInfo->flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR) != 0))
    {
        useToken = true;
    }

    // we need to allocate enough system memory for the api objects
    const size_t apiSize = sizeof(Event);

    Pal::GpuEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.flags.gpuAccessOnly = ((pCreateInfo->flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR) != 0) ? 1 : 0;

    const size_t palSize = useToken ?
        0 : pDevice->PalDevice(DefaultDeviceIndex)->GetGpuEventSize(eventCreateInfo, nullptr);

    void* pSystemMem = pDevice->AllocApiObject(
        pAllocator,
        apiSize + (palSize * numDeviceEvents));

    // Bail on allocation failure
    if (pSystemMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // we need to allocate memory for the pal event objects if we aren't using tokens.
    if (useToken == false)
    {
        void* pPalMem = Util::VoidPtrInc(pSystemMem, apiSize);

        for (uint32_t deviceIdx = 0;
            (deviceIdx < numDeviceEvents) && (palResult == Pal::Result::Success);
            deviceIdx++)
        {
            VK_ASSERT(palSize == pDevice->PalDevice(deviceIdx)->GetGpuEventSize(eventCreateInfo, nullptr));

            // Create the PAL object.
            palResult = pDevice->PalDevice(deviceIdx)->CreateGpuEvent(eventCreateInfo,
                Util::VoidPtrInc(pPalMem, palSize * deviceIdx),
                &pPalGpuEvents[deviceIdx]);
        }

        result = PalToVkResult(palResult);
    }

    Event* pObject = nullptr;

    if (result == VK_SUCCESS)
    {
        pObject = VK_PLACEMENT_NEW(pSystemMem) Event(pDevice, numDeviceEvents, pPalGpuEvents, useToken);

        *pEvent = Event::HandleFromVoidPointer(pSystemMem);

        result = pObject->Initialize(
            pDevice,
            numDeviceEvents,
            pCreateInfo->flags);
    }

    if (result != VK_SUCCESS)
    {
        if (useToken == false)
        {
            // Something went wrong
            for (uint32_t deviceIdx = 0; (deviceIdx < numDeviceEvents); deviceIdx++)
            {
                if (pPalGpuEvents[deviceIdx] != nullptr)
                {
                    pPalGpuEvents[deviceIdx]->Destroy();
                }
            }
            pDevice->MemMgr()->FreeGpuMem(&internalGpuMem);
        }

        // Call destructor
        Util::Destructor(pObject);

        // PAL event construction failed. Free system memory and return.
        pDevice->FreeApiObject(pAllocator, pSystemMem);
    }

    return result;
}

// =====================================================================================================================
// Initialize event object
VkResult Event::Initialize(
    Device* const      pDevice,
    uint32_t           numDeviceEvents,
    VkEventCreateFlags flags)
{
    VkResult result       = VK_SUCCESS;
    Pal::Result palResult = Pal::Result::Success;

    Pal::GpuMemoryRequirements gpuMemReqs = {};
    m_pPalEvents[0]->GetGpuMemoryRequirements(&gpuMemReqs);

    InternalMemCreateInfo allocInfo  = {};
    allocInfo.pal.size               = gpuMemReqs.size;
    allocInfo.pal.alignment          = gpuMemReqs.alignment;
    allocInfo.pal.priority           = Pal::GpuMemPriority::Normal;
    allocInfo.pal.flags.shareable    = (numDeviceEvents > 1) ? 1 : 0;
    allocInfo.pal.flags.cpuInvisible = (gpuMemReqs.flags.cpuAccess ? 0 : 1);

    InternalSubAllocPool pool = InternalPoolCpuCacheableGpuUncached;

    if (((flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR) != 0) &&
        (numDeviceEvents == 1))
    {
        pool = InternalPoolGpuAccess;
    }

    pDevice->MemMgr()->GetCommonPool(pool, &allocInfo);

    result = pDevice->MemMgr()->AllocGpuMem(
        allocInfo,
        &m_internalGpuMem,
        1,
        VK_OBJECT_TYPE_EVENT,
        Event::IntValueFromHandle(Event::HandleFromObject(this)));

    if (result == VK_SUCCESS)
    {
        for (uint32_t deviceIdx = 0;
            (deviceIdx < numDeviceEvents) && (palResult == Pal::Result::Success);
            deviceIdx++)
        {
            palResult = m_pPalEvents[deviceIdx]->BindGpuMemory(m_internalGpuMem.PalMemory(deviceIdx),
                m_internalGpuMem.Offset());
        }
        result = PalToVkResult(palResult);
    }

    return result;
}

// =====================================================================================================================
// Signal an event object
VkResult Event::Set(void)
{
    const Pal::Result palResult = PalEvent(DefaultDeviceIndex)->Set();

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Reset an event object
VkResult Event::Reset(void)
{
    const Pal::Result palResult = PalEvent(DefaultDeviceIndex)->Reset();

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Get the current status of an event object
VkResult Event::GetStatus(void)
{
    const Pal::Result palStatus = PalEvent(DefaultDeviceIndex)->GetStatus();

    return PalToVkResult(palStatus);
}

// =====================================================================================================================
// Destroy event object
VkResult Event::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    const uint32_t numDeviceEvents  = pDevice->NumPalDevices();

    // Destroy the PAL object if the event isn't gpu-only.
    if (m_useToken == false)
    {
        for (uint32_t deviceIdx = 0; (deviceIdx < numDeviceEvents); deviceIdx++)
        {
            PalEvent(deviceIdx)->Destroy();
        }
        pDevice->MemMgr()->FreeGpuMem(&m_internalGpuMem);
    }

    // Call my own destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyEvent(
    VkDevice                                    device,
    VkEvent                                     event,
    const VkAllocationCallbacks*                pAllocator)
{
    if (event != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Event::ObjectFromHandle(event)->Destroy(pDevice, pAllocCB);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetEventStatus(
    VkDevice                                    device,
    VkEvent                                     event)
{
    return Event::ObjectFromHandle(event)->GetStatus();
}

VKAPI_ATTR VkResult VKAPI_CALL vkSetEvent(
    VkDevice                                    device,
    VkEvent                                     event)
{
    return Event::ObjectFromHandle(event)->Set();
}

VKAPI_ATTR VkResult VKAPI_CALL vkResetEvent(
    VkDevice                                    device,
    VkEvent                                     event)
{
    return Event::ObjectFromHandle(event)->Reset();
}

} // namespace entry

} // namespace vk
