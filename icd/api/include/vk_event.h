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
 * @file  vk_event.h
 * @brief Functionality related to Vulkan event objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_EVENT_H__
#define __VK_EVENT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/internal_mem_mgr.h"

namespace Pal
{

class IGpuEvent;

} // namespace Pal

namespace vk
{

class Device;
class ApiEvent;

class Event final : public NonDispatchable<VkEvent, Event>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkEventCreateInfo*        pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkEvent*                        pEvent);

    VkResult GetStatus(void);

    VkResult Set(void);

    VkResult Reset(void);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VK_FORCEINLINE Pal::IGpuEvent* PalEvent(uint32_t deviceIdx) const
    {
        return m_pPalEvents[deviceIdx];
    }

    VK_FORCEINLINE uint32 GetSyncToken() const
    {
        return m_syncToken;
    }

    VK_FORCEINLINE void SetSyncToken(uint32 syncToken)
    {
        m_syncToken = syncToken;
    }

    VK_FORCEINLINE bool IsUseToken() const
    {
        return m_useToken;
    }

protected:
    Event(
        Device*                         pDevice,
        uint32_t                        numDeviceEvents,
        Pal::IGpuEvent**                pPalEvents,
        bool                            useToken);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Event);

    VkResult Initialize(
        Device* const      m_pDevice,
        uint32_t           numDeviceEvents,
        VkEventCreateFlags flags);

    union
    {
        Pal::IGpuEvent*    m_pPalEvents[MaxPalDevices];
        uint32             m_syncToken;
    };

    InternalMemory         m_internalGpuMem;

    // This flag is used to decide which path to use when setting and waiting event with CmdRelease/CmdAcquire.
    // if the flag is true, we will use sync tokens. Well, if the flag is false, we will use iGpuEvents.
    bool                   m_useToken;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyEvent(
    VkDevice                                    device,
    VkEvent                                     event,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetEventStatus(
    VkDevice                                    device,
    VkEvent                                     event);

VKAPI_ATTR VkResult VKAPI_CALL vkSetEvent(
    VkDevice                                    device,
    VkEvent                                     event);

VKAPI_ATTR VkResult VKAPI_CALL vkResetEvent(
    VkDevice                                    device,
    VkEvent                                     event);
} // namespace entry

} // namespace vk

#endif /* __VK_EVENT_H__ */
