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

    VkResult ImportSemaphore(
        Device*                                    pDevice,
        VkExternalSemaphoreHandleTypeFlagsKHR      handleType,
        const Pal::OsExternalHandle                handle,
        VkSemaphoreImportFlagsKHR                  importFlags);

    VK_FORCEINLINE Pal::IQueueSemaphore* PalSemaphore() const
    {
        return m_pPalSemaphore;
    }

    VK_FORCEINLINE Pal::IQueueSemaphore* PalTemporarySemaphore() const
    {
        return m_pPalTemporarySemaphore;
    }

    VK_FORCEINLINE void SetPalTemporarySemaphore(Pal::IQueueSemaphore* pPalTemporarySemaphore)
    {
        m_pPalTemporarySemaphore = pPalTemporarySemaphore;
    }

    VkResult Destroy(
        const Device*                   pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult GetShareHandle(
        Device*                                     device,
        VkExternalSemaphoreHandleTypeFlagBitsKHR    handleType,
        Pal::OsExternalHandle*                      pHandle);

private:
    Semaphore(Pal::IQueueSemaphore* pPalSemaphore)
        :
        m_pPalSemaphore(pPalSemaphore),
        m_pPalTemporarySemaphore(nullptr)
    {

    }

    Pal::IQueueSemaphore*       m_pPalSemaphore;
    Pal::IQueueSemaphore*       m_pPalTemporarySemaphore;     // Temporary-completion semaphore special for swapchain
                                                              // which will be associated with a signaled semaphore
                                                              // in AcquireNextImage.
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
