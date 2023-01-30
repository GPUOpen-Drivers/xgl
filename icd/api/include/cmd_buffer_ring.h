/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  cmd_buffer_ring.h
 * @brief Utility class for managing a ring of command buffers
 **************************************************************************************************
 */

#ifndef __CMD_BUFFER_RING_H__
#define __CMD_BUFFER_RING_H__

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_utils.h"
#include "settings/settings.h"

#include "palCmdBuffer.h"
#include "palDeque.h"

#pragma once

namespace vk
{

class Device;

// =====================================================================================================================
// State of a command buffer managed by the CmdBufferRing
struct CmdBufState
{
    Pal::ICmdBuffer*    pCmdBuf;     // Command buffer pointer
    Pal::IFence*        pFence;      // Fence that will be signaled when this fence's submit completes
};

// =====================================================================================================================
// Managed ring of command buffers to be acquired and submitted in FIFO order.
class CmdBufferRing
{
public:
    static CmdBufferRing* Create(
        const Device*              pDevice,
        Pal::EngineType            engineType,
        Pal::QueueType             queueType);

    void Destroy(
        const Device*              pDevice);

    CmdBufState* AcquireCmdBuffer(
        const Device*              pDevice,
        uint32                     deviceIdx);

    VkResult SubmitCmdBuffer(
        const Device*              pDevice,
        uint32                     deviceIdx,
        Pal::IQueue*               pPalQueue,
        const Pal::CmdBufInfo&     cmdBufInfo,
        CmdBufState*               pCmdBufState);

protected:
    CmdBufState* CreateCmdBufState(
        const Device*              pDevice,
        uint32                     deviceIdx);

    void DestroyCmdBufState(
        const Device*              pDevice,
        uint32                     deviceIdx,
        CmdBufState*               pCmdBufState);

    typedef Util::Deque<CmdBufState*, PalAllocator> CmdBufferDequeue;

    CmdBufferRing(
        CmdBufferDequeue* pCmdBufferRings[],
        Pal::EngineType   engineType,
        Pal::QueueType    queueType);

    ~CmdBufferRing() {}

    CmdBufferDequeue* m_pCmdBufferRings[MaxPalDevices];
    Pal::EngineType   m_engineType;
    Pal::QueueType    m_queueType;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBufferRing);
};

} //namespace vk

#endif /* __CMD_BUFFER_RING_H__ */
