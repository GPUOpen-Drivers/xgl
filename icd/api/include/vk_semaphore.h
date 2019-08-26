/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_SEMAPHORE_H__
#define __VK_SEMAPHORE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"

#include "palQueueSemaphore.h"

namespace Pal
{

class IQueueSemaphore;

}

namespace vk
{

class Device;

class Semaphore : public NonDispatchable<VkSemaphore, Semaphore>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkSemaphoreCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSemaphore*                    pSemaphore);

    static VkResult PopulateInDeviceGroup(
        Device*                         pDevice,
        Pal::IQueueSemaphore*           pPalSemaphores[MaxPalDevices],
        int32_t*                        pSemaphoreCount);

    VkResult ImportSemaphore(
        Device*                    pDevice,
        const ImportSemaphoreInfo& importInfo);

    VkResult GetSemaphoreCounterValue(
        Device*                   pDevice,
        Semaphore*                pSemaphore,
        uint64_t*                 pValue);

    VkResult WaitSemaphoreValue(
        Device*                 pDevice,
        Semaphore*              pSemaphore,
        uint64_t                value,
        uint64_t                timeout);

    VkResult SignalSemaphoreValue(
        Device*                 pDevice,
        Semaphore*              pSemaphore,
        uint64_t                value);

    VK_FORCEINLINE Pal::IQueueSemaphore* PalSemaphore(uint32_t deviceIdx) const
    {
        return m_pPalSemaphores[deviceIdx];
    }

    VK_FORCEINLINE Pal::IQueueSemaphore* PalTemporarySemaphore(uint32_t deviceIdx) const
    {
        return m_pPalTemporarySemaphores[deviceIdx];
    }

    VK_FORCEINLINE void ClearTemporarySemaphore()
    {
        memset(m_pPalTemporarySemaphores, 0, sizeof(m_pPalTemporarySemaphores));

    }

    VK_FORCEINLINE void SetTemporarySemaphore(
        Pal::IQueueSemaphore* pPalTemporarySemaphore[],
        int32_t               semaphoreCount,
        Pal::OsExternalHandle tempHandle)
    {
        for (int32_t i = 0; i < semaphoreCount; i++)
        {
            m_pPalTemporarySemaphores[i] = pPalTemporarySemaphore[i];
        }
        for (uint32_t i = semaphoreCount; i < MaxPalDevices; i++)
        {
            m_pPalTemporarySemaphores[i] = nullptr;
        }
        m_sharedSemaphoreTempHandle = tempHandle;
    }

    VK_FORCEINLINE Pal::OsExternalHandle GetHandle() const
    {
        return (m_sharedSemaphoreTempHandle == 0) ? m_sharedSemaphoreHandle : m_sharedSemaphoreTempHandle;
    }

    VkResult Destroy(
        const Device*                   pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult GetShareHandle(
        Device*                                     device,
        VkExternalSemaphoreHandleTypeFlagBits       handleType,
        Pal::OsExternalHandle*                      pHandle);

    VK_FORCEINLINE bool IsTimelineSemaphore() const
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        return m_palCreateInfo.flags.timeline;
#else
        return false;
#endif
    }

private:
    Semaphore(
        Pal::IQueueSemaphore*                pPalSemaphore[],
        int32_t                              semaphoreCount,
        const Pal::QueueSemaphoreCreateInfo& palCreateInfo,
        Pal::OsExternalHandle                sharedSemaphorehandle)
        :
        m_sharedSemaphoreHandle(sharedSemaphorehandle),
        m_sharedSemaphoreTempHandle(0),
        m_palCreateInfo(palCreateInfo)
    {
        for (int32_t i = 0; i < semaphoreCount; i++)
        {
            m_pPalSemaphores[i] = pPalSemaphore[i];
        }
        for (uint32_t i = semaphoreCount; i < MaxPalDevices; i++)
        {
            m_pPalSemaphores[i] = nullptr;
        }

        ClearTemporarySemaphore();
    }

    Pal::IQueueSemaphore*           m_pPalSemaphores[MaxPalDevices];

    // Temporary-completion semaphore special for swapchain
    // which will be associated with a signaled semaphore
    // in AcquireNextImage.
    Pal::IQueueSemaphore*           m_pPalTemporarySemaphores[MaxPalDevices];

    // For now the m_sharedSemaphoreHandle and m_sharedSemaphoreTempHandle are only used by Windows driver to cache the
    // semaphore's handle when the semaphore object is creating.
    Pal::OsExternalHandle           m_sharedSemaphoreHandle;
    Pal::OsExternalHandle           m_sharedSemaphoreTempHandle;
    Pal::QueueSemaphoreCreateInfo   m_palCreateInfo;
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFdKHR(
    VkDevice                                    device,
    const VkSemaphoreGetFdInfoKHR*              pGetFdInfo,
    int*                                        pFd);
} // namespace entry

} // namespace vk

#endif /* __VK_SEMAPHORE_H__ */
