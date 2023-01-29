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
 * @file  vk_fence.h
 * @brief Functionality related to Vulkan fence objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_FENCE_H__
#define __VK_FENCE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_defines.h"

namespace Pal
{

class IFence;
struct FenceOpenInfo;
};

namespace vk
{

class Device;

class Fence final : public NonDispatchable<VkFence, Fence>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkFenceCreateInfo*        pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkFence*                        pFence);

    VkResult GetStatus(void);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

#if defined(__unix__)
    VkResult ImportFenceFd(
        Device*                         pDevice,
        const VkImportFenceFdInfoKHR*   pImportFenceFdInfo);

    VkResult GetFenceFd(
        Device*                         pDevice,
        const VkFenceGetFdInfoKHR*      pGetFdInfo,
        int*                            pFd);
#endif

    VkResult RestoreFence(const Device* pDevice);

    uint32_t GetActiveDeviceMask() const
        { return m_activeDeviceMask; }

    void ClearActiveDeviceMask()
        { m_activeDeviceMask = 0; }

    void SetActiveDevice(uint32_t deviceIdx)
        { m_activeDeviceMask |= (1 << deviceIdx); }

    VK_FORCEINLINE Pal::IFence* PalFence(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));

        return m_flags.isPermanence ? m_pPalFences[idx] : m_pPalTemporaryFences;
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Fence);

    Fence(uint32_t      numGroupedFences,
          Pal::IFence** pPalFences,
          bool          canBeInherited)
    :
    m_activeDeviceMask(0),
    m_groupedFenceCount(numGroupedFences),
    m_pPalTemporaryFences(nullptr)
    {
        memcpy(m_pPalFences, pPalFences, sizeof(pPalFences[0]) * numGroupedFences);
        m_flags.value          = 0;
        m_flags.isPermanence   = 1;
        m_flags.canBeInherited = canBeInherited;
    }

    uint32_t     m_activeDeviceMask;
    uint32_t     m_groupedFenceCount;
    Pal::IFence* m_pPalFences[MaxPalDevices];
    Pal::IFence* m_pPalTemporaryFences;

    union
    {
        struct
        {
            uint32_t isPermanence   : 1;
            uint32_t isOpened       : 1;
            uint32_t isReference    : 1;
            uint32_t canBeInherited : 1;
            uint32_t reserved       : 28;
        };
        uint32_t value;
    } m_flags;
};

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(
    VkDevice                                    device,
    VkFence                                     fence);

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(
    VkDevice                                    device,
    VkFence                                     fence,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFenceProperties(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo*    pExternalFenceInfo,
    VkExternalFenceProperties*                  pExternalFenceProperties);

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkImportFenceFdKHR(
    VkDevice                                    device,
    const VkImportFenceFdInfoKHR*               pImportFenceFdInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceFdKHR(
    VkDevice                                    device,
    const VkFenceGetFdInfoKHR*                  pGetFdInfo,
    int*                                        pFd);
#endif

} // namespace entry

} // namespace vk

#endif /* __VK_FENCE_H__ */
