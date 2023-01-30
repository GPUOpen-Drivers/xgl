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
 ***********************************************************************************************************************
 * @file  cmd_buffer_ring.cpp
 * @brief Contains implementation of the CmdBufferRing
 ***********************************************************************************************************************
 */

#include "include/cmd_buffer_ring.h"
#include "include/vk_device.h"
#include "include/vk_queue.h"

#include "palDequeImpl.h"

namespace vk
{

// =====================================================================================================================
CmdBufferRing::CmdBufferRing(
    CmdBufferDequeue* pCmdBufferRings[],
    Pal::EngineType   engineType,
    Pal::QueueType    queueType)
    :
    m_engineType(engineType),
    m_queueType(queueType)
{
    for (uint32 deviceIdx = 0; deviceIdx < MaxPalDevices; ++deviceIdx)
    {
        m_pCmdBufferRings[deviceIdx] = pCmdBufferRings[deviceIdx];
    }
}

// =====================================================================================================================
CmdBufferRing* CmdBufferRing::Create(
    const Device*                    pDevice,
    Pal::EngineType                  engineType,
    Pal::QueueType                   queueType)
{
    void* pMemory = pDevice->VkInstance()->AllocMem((sizeof(CmdBufferRing) +
                                                     (sizeof(CmdBufferDequeue) * pDevice->NumPalDevices())),
                                                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory != nullptr)
    {
        CmdBufferDequeue* pCmdBufferDequeues[MaxPalDevices] = {};

        for (uint32 deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
        {
            pCmdBufferDequeues[deviceIdx] = VK_PLACEMENT_NEW(Util::VoidPtrInc(pMemory,
                                                                             (sizeof(CmdBufferRing) +
                                                                              sizeof(CmdBufferDequeue) * deviceIdx)))
                                                            CmdBufferDequeue(pDevice->VkInstance()->Allocator());
        }

        VK_PLACEMENT_NEW(pMemory) CmdBufferRing(pCmdBufferDequeues, engineType, queueType);
    }

    return static_cast<CmdBufferRing*>(pMemory);
}

// =====================================================================================================================
// Destroys a ring buffer and frees any memory associated with it
void CmdBufferRing::Destroy(
    const Device*                    pDevice)
{
    // Destroy the command buffer rings
    for (uint32 deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (m_pCmdBufferRings[deviceIdx] != nullptr)
        {
            while (m_pCmdBufferRings[deviceIdx]->NumElements() > 0)
            {
                CmdBufState* pCmdBufState = nullptr;
                m_pCmdBufferRings[deviceIdx]->PopFront(&pCmdBufState);
                DestroyCmdBufState(pDevice, deviceIdx, pCmdBufState);
            }

            Util::Destructor(m_pCmdBufferRings[deviceIdx]);
        }
    }

    this->~CmdBufferRing();
    pDevice->VkInstance()->FreeMem(this);
}

// =====================================================================================================================
// Initializes the command buffer state
CmdBufState* CmdBufferRing::CreateCmdBufState(
    const Device*                    pDevice,
    uint32                           deviceIdx)
{
    CmdBufState* pCmdBufState = nullptr;

    Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

    Pal::CmdBufferCreateInfo cmdBufInfo = {};

    cmdBufInfo.queueType     = m_queueType;
    cmdBufInfo.engineType    = m_engineType;
    cmdBufInfo.pCmdAllocator = pDevice->GetSharedCmdAllocator(deviceIdx);

    Pal::FenceCreateInfo fenceInfo = {};

    size_t cmdBufSize = 0;
    size_t fenceSize = 0;

    Pal::Result result;

    cmdBufSize = pPalDevice->GetCmdBufferSize(cmdBufInfo, &result);

    if (result == Pal::Result::Success)
    {
        fenceSize = pPalDevice->GetFenceSize(&result);
    }

    size_t totalSize = sizeof(CmdBufState) + cmdBufSize + fenceSize;

    void* pStorage = nullptr;

    if (result == Pal::Result::Success)
    {
        pStorage = pDevice->VkInstance()->AllocMem(totalSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    }

    if (pStorage != nullptr)
    {
        pCmdBufState = static_cast<CmdBufState*>(pStorage);
        pStorage = Util::VoidPtrInc(pStorage, sizeof(CmdBufState));

        void* pCmdBufStorage = pStorage;
        pStorage = Util::VoidPtrInc(pStorage, cmdBufSize);

        void* pFenceStorage = pStorage;
        pStorage = Util::VoidPtrInc(pStorage, fenceSize);

        if (result == Pal::Result::Success)
        {
            result = pPalDevice->CreateCmdBuffer(cmdBufInfo, pCmdBufStorage, &pCmdBufState->pCmdBuf);
        }

        if (result == Pal::Result::Success)
        {
            result = pPalDevice->CreateFence(fenceInfo, pFenceStorage, &pCmdBufState->pFence);
        }

        VK_ASSERT(Util::VoidPtrInc(pCmdBufState, totalSize) == pStorage);

        if (result != Pal::Result::Success)
        {
            DestroyCmdBufState(pDevice, deviceIdx, pCmdBufState);
            pCmdBufState = nullptr;
        }
    }

    return pCmdBufState;
}

// =====================================================================================================================
// Destroys a command buffer state and frees any memory associated with it
void CmdBufferRing::DestroyCmdBufState(
    const Device*                    pDevice,
    uint32                           deviceIdx,
    CmdBufState*                     pCmdBufState)
{
    // Wait to finish in case still in flight
    if (pCmdBufState->pFence->GetStatus() == Pal::Result::NotReady)
    {
        pDevice->PalDevice(deviceIdx)->WaitForFences(1, &pCmdBufState->pFence, true, ~0ULL);
    }

    // Destroy Fence
    if (pCmdBufState->pFence != nullptr)
    {
        pCmdBufState->pFence->Destroy();
    }

    // Destroy CmdBuf
    if (pCmdBufState->pCmdBuf != nullptr)
    {
        pCmdBufState->pCmdBuf->Destroy();
    }

    // Free all system memory
    pDevice->VkInstance()->FreeMem(pCmdBufState);
}

// =====================================================================================================================
// Gets a new command buffer from a ring buffer, the cmd of which can be redefined with new command data
CmdBufState* CmdBufferRing::AcquireCmdBuffer(
    const Device*                    pDevice,
    uint32                           deviceIdx)
{
    CmdBufState* pCmdBufState = nullptr;

    if (m_pCmdBufferRings[deviceIdx] != nullptr)
    {
        // Create a new command buffer if the least recently used one is still busy.
        if ((m_pCmdBufferRings[deviceIdx]->NumElements() == 0) ||
            (m_pCmdBufferRings[deviceIdx]->Front()->pFence->GetStatus() == Pal::Result::NotReady))
        {
            pCmdBufState = CreateCmdBufState(pDevice, deviceIdx);
        }
        else
        {
            m_pCmdBufferRings[deviceIdx]->PopFront(&pCmdBufState);
        }

        // Immediately push this command buffer onto the back of the deque to avoid leaking memory.
        if (pCmdBufState != nullptr)
        {
            Pal::Result result = m_pCmdBufferRings[deviceIdx]->PushBack(pCmdBufState);

            if (result != Pal::Result::Success)
            {
                // We failed to push this command buffer onto the deque. To avoid leaking memory we must delete it.
                DestroyCmdBufState(pDevice, deviceIdx, pCmdBufState);
                pCmdBufState = nullptr;
            }
            else
            {
                Pal::CmdBufferBuildInfo buildInfo = {};

                buildInfo.flags.optimizeOneTimeSubmit = 1;

                result = pCmdBufState->pCmdBuf->Reset(pDevice->GetSharedCmdAllocator(deviceIdx), true);

                if (result == Pal::Result::Success)
                {
                    result = pCmdBufState->pCmdBuf->Begin(buildInfo);
                }

                if (result != Pal::Result::Success)
                {
                    pCmdBufState = nullptr;
                }
            }
        }
    }

    return pCmdBufState;
}

// =====================================================================================================================
// Submit commands to the provided queue
VkResult CmdBufferRing::SubmitCmdBuffer(
    const Device*                    pDevice,
    uint32                           deviceIdx,
    Pal::IQueue*                     pPalQueue,
    const Pal::CmdBufInfo&           cmdBufInfo,
    CmdBufState*                     pCmdBufState)
{
    Pal::Result result = pCmdBufState->pCmdBuf->End();

    if (result == Pal::Result::Success)
    {
        result = pDevice->PalDevice(deviceIdx)->ResetFences(1, &pCmdBufState->pFence);

        // Submit the command buffer
        if (result == Pal::Result::Success)
        {
            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount  = 1;
            perSubQueueInfo.ppCmdBuffers    = &pCmdBufState->pCmdBuf;
            perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

            Pal::SubmitInfo palSubmitInfo = {};

            VK_ASSERT(cmdBufInfo.isValid == 1);

            palSubmitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
            palSubmitInfo.perSubQueueInfoCount = 1;
            palSubmitInfo.ppFences             = &pCmdBufState->pFence;
            palSubmitInfo.fenceCount           = 1;

            result = pPalQueue->Submit(palSubmitInfo);
        }
    }

    return PalToVkResult(result);
}

} //namespace vk
