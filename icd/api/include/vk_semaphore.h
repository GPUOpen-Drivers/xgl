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

class Semaphore final : public NonDispatchable<VkSemaphore, Semaphore>
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
        uint32_t*                       pSemaphoreCount);

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

    void SetTemporarySemaphore(
        Pal::IQueueSemaphore* pPalImportedSemaphore[],
        uint32_t              semaphoreCount,
        Pal::OsExternalHandle importedHandle);

    void SetSemaphore(
        Pal::IQueueSemaphore* pPalImportedSemaphore[],
        uint32_t              semaphoreCount,
        Pal::OsExternalHandle importedHandle);

    void DestroySemaphore(
        const Device*           pDevice);

    void DestroyTemporarySemaphore(
        const Device*           pDevice);

    void Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult GetShareHandle(
        Device*                                     device,
        VkExternalSemaphoreHandleTypeFlagBits       handleType,
        Pal::OsExternalHandle*                      pHandle);

    VK_FORCEINLINE Pal::IQueueSemaphore* PalSemaphore(uint32_t deviceIdx) const
    {
#if defined(__unix__)
        return m_useTempSemaphore ? m_pPalTemporarySemaphores[deviceIdx] : m_pPalSemaphores[deviceIdx];
#else
        return m_useTempSemaphore ? m_pPalTemporarySemaphores[0] : m_pPalSemaphores[0];
#endif
    }

    VK_FORCEINLINE Pal::OsExternalHandle GetHandle() const
    {
        return (m_useTempSemaphore) ? m_sharedSemaphoreTempHandle : m_sharedSemaphoreHandle;
    }

    VK_FORCEINLINE void RestoreSemaphore()
    {
        if (m_useTempSemaphore)
        {
            m_useTempSemaphore = false;
        }
    }

    VK_FORCEINLINE bool IsTimelineSemaphore() const
    {
        return m_palCreateInfo.flags.timeline;
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Semaphore);

    Semaphore(
        Pal::IQueueSemaphore*                pPalSemaphore[],
        uint32_t                             semaphoreCount,
        const Pal::QueueSemaphoreCreateInfo& palCreateInfo,
        Pal::OsExternalHandle                sharedSemaphorehandle)
        :
        m_palCreateInfo(palCreateInfo),
        m_useTempSemaphore(false),
        m_sharedSemaphoreHandle(sharedSemaphorehandle),
        m_sharedSemaphoreTempHandle(0)
    {
        for (uint32_t i = 0; i < semaphoreCount; i++)
        {
            m_pPalSemaphores[i] = pPalSemaphore[i];
        }
        for (uint32_t i = semaphoreCount; i < MaxPalDevices; i++)
        {
            m_pPalSemaphores[i] = nullptr;
        }

        memset(m_pPalTemporarySemaphores, 0, sizeof(m_pPalTemporarySemaphores));
    }

    Pal::QueueSemaphoreCreateInfo   m_palCreateInfo;

    Pal::IQueueSemaphore*           m_pPalSemaphores[MaxPalDevices];
    // Temporary-completion semaphore special for swapchain
    // which will be associated with a signaled semaphore
    // in AcquireNextImage.
    Pal::IQueueSemaphore*           m_pPalTemporarySemaphores[MaxPalDevices];
    // m_useTempSemaphore indicates whether temporary Semaphore is in use.
    bool                            m_useTempSemaphore;

    // For now the m_sharedSemaphoreHandle and m_sharedSemaphoreTempHandle are only used by Windows driver to cache the
    // semaphore's handle when the semaphore object is creating.
    Pal::OsExternalHandle           m_sharedSemaphoreHandle;
    Pal::OsExternalHandle           m_sharedSemaphoreTempHandle;

};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    const VkAllocationCallbacks*                pAllocator);

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFdKHR(
    VkDevice                                    device,
    const VkSemaphoreGetFdInfoKHR*              pGetFdInfo,
    int*                                        pFd);
#endif
} // namespace entry

} // namespace vk

#endif /* __VK_SEMAPHORE_H__ */
