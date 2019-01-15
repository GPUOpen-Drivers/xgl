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
/**
 **************************************************************************************************
 * @file  vk_cmd_pool.h
 * @brief Declaration of Vulkan command buffer pool class.
 **************************************************************************************************
 */

#ifndef __VK_CMD_POOL_H__
#define __VK_CMD_POOL_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"
#include "include/vk_alloccb.h"

#include "include/gpu_event_mgr.h"

#include "palCmdAllocator.h"
#include "palHashSet.h"

namespace vk
{

class Device;
class CmdBuffer;

// =====================================================================================================================
// A Vulkan command buffer pool
class CmdPool : public NonDispatchable<VkCommandPool, CmdPool>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkCommandPoolCreateInfo*  pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkCommandPool*                  pCmdPool);

    VkResult Init();

    VkResult Destroy(
        const Device*                   pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult Reset(VkCommandPoolResetFlags flags);

    Pal::ICmdAllocator* PalCmdAllocator(int32_t idx)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(m_pDevice->NumPalDevices())));
        return m_pPalCmdAllocators[idx];
    }

    Pal::Result RegisterCmdBuffer(CmdBuffer* pCmdBuffer);

    void UnregisterCmdBuffer(CmdBuffer* pCmdBuffer);

    GpuEventMgr* AcquireGpuEventMgr();
    void ReleaseGpuEventMgr(GpuEventMgr* pGpuEventMgr);

    VkResult PalCmdAllocatorReset();

    VK_INLINE uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }

protected:
    CmdPool(
        Device*              pDevice,
        Pal::ICmdAllocator** pPalCmdAllocators,
        uint32_t             queueFamilyIndex,
        bool                 sharedCmdAllocator);

    void DestroyGpuEventMgrs();

    Device*             m_pDevice;
    Pal::ICmdAllocator* m_pPalCmdAllocators[MaxPalDevices];
    const uint32_t      m_queueFamilyIndex;
    const bool          m_sharedCmdAllocator;

    Util::HashSet<CmdBuffer*, PalAllocator> m_cmdBufferRegistry;

    Util::IntrusiveList<GpuEventMgr> m_freeEventMgrs;
    uint32_t                         m_totalEventMgrCount;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolResetFlags                     flags);
} // namespace entry

} // namespace vk

#endif /* __VK_CMD_POOL_H__ */
