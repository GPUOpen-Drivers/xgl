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
 ***********************************************************************************************************************
 * @file  vk_event.cpp
 * @brief Contains implementation of Vulkan event objects.
 ***********************************************************************************************************************
 */

#include "include/gpu_event_mgr.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_event.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"

#include "palGpuEvent.h"

namespace vk
{

// =====================================================================================================================
// Create a new event object. This is the implementation of vkCreateEvent.
VkResult Event::Create(
    Device*                         pDevice,
    const VkEventCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkEvent*                        pEvent)
{
    const size_t numDeviceEvents = pDevice->NumPalDevices();

    Pal::IGpuEvent* pPalGpuEvents[MaxPalDevices] = {};
    Pal::Result palResult = Pal::Result::Success;

    // Technically, we should vet the pCreateInfo structure here, but the only defined
    // field in it right now is reserved, so we gain nothing except protecting broken
    // applications from themselves. A debug layer should do that.

    // Allocate enough system memory for the dispatchable event object and the
    // PAL event object.

    const size_t apiSize = sizeof(Event);

    const Pal::GpuEventCreateInfo eventCreateInfo = {};
    const size_t palSize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuEventSize(eventCreateInfo, nullptr);

    void* pSystemMem = pAllocator->pfnAllocation(
                        pAllocator->pUserData,
                        apiSize + (palSize * numDeviceEvents),
                        VK_DEFAULT_MEM_ALIGN,
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    // Bail on allocation failure
    if (pSystemMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    void* pPalMem = Util::VoidPtrInc(pSystemMem, apiSize);

    for (uint32_t deviceIdx = 0;
        (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        VK_ASSERT(palSize == pDevice->PalDevice(deviceIdx)->GetGpuEventSize(eventCreateInfo, nullptr));

        // Create the PAL object.
        palResult = pDevice->PalDevice(deviceIdx)->CreateGpuEvent(eventCreateInfo,
            Util::VoidPtrInc(pPalMem, palSize * deviceIdx),
            &pPalGpuEvents[deviceIdx]);
    }

    VkResult result =  PalToVkResult(palResult);

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) Event(pDevice, pDevice->NumPalDevices(), pPalGpuEvents);

        *pEvent = Event::HandleFromVoidPointer(pSystemMem);
    }
    else
    {
        // Something went wrong

        for (uint32_t deviceIdx = 0; (deviceIdx < pDevice->NumPalDevices()); deviceIdx++)
        {
            if (pPalGpuEvents[deviceIdx] != nullptr)
            {
                pPalGpuEvents[deviceIdx]->Destroy();
            }
        }

        // PAL event construction failed. Free system memory and return.
        pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);
    }

    return result;
}

// =====================================================================================================================
// Signal an event object
VkResult Event::Set(void)
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_numDeviceEvents) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        palResult = PalEvent(deviceIdx)->Set();
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Reset an event object
VkResult Event::Reset(void)
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_numDeviceEvents) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        palResult = PalEvent(deviceIdx)->Reset();
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Get the current status of an event object
VkResult Event::GetStatus(void)
{
    Pal::Result palStatus = PalEvent(DefaultDeviceIndex)->GetStatus();

    for (uint32_t deviceIdx = 1; deviceIdx < m_numDeviceEvents; deviceIdx++)
    {
        if (PalEvent(deviceIdx)->GetStatus() != palStatus)
        {
            palStatus = Pal::Result::ErrorUnknown;
            break;
        }
    }

    return PalToVkResult(palStatus);
}

// =====================================================================================================================
// Destroy event object
VkResult Event::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Destroy the PAL object.
    for (uint32_t deviceIdx = 0; (deviceIdx < m_numDeviceEvents); deviceIdx++)
    {
        PalEvent(deviceIdx)->Destroy();
    }

    // Call my own destructor
    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

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
