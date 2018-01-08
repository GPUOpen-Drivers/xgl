/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  peer_resource.cpp
 * @brief Classes to manage Multi-GPU resource sharing
 ***********************************************************************************************************************
 */
#include "include/vk_device.h"
#include "include/peer_resource.h"

#include "palGpuMemory.h"
#include "palImage.h"
#include "palSysUtil.h"

namespace vk
{

// =====================================================================================================================
PeerMemory::PeerMemory(
    Device*           pDevice,
    Pal::IGpuMemory** pGpuMemories,
    uint32_t          palObjSize)
:
    m_palObjSize(palObjSize),
    m_allocationOffset(sizeof(*this))
{
    memset(m_ppGpuMemory, 0, sizeof(m_ppGpuMemory));

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        // Real allocations are placed on the 'diagonal line' of this 2d array
        if (pGpuMemories[deviceIdx] != nullptr)
        {
            m_ppGpuMemory[deviceIdx][deviceIdx] = pGpuMemories[deviceIdx];
        }
    }
}

// =====================================================================================================================
void PeerMemory::Destroy(Device* pDevice)
{
    const uint32_t numDevices = pDevice->NumPalDevices();

    for (uint32_t i = 0; i < numDevices; i++)
    {
        // Real memory allocations are owned externally there is no need to destroy them yet
        m_ppGpuMemory[i][i] = nullptr;
    }

    for (uint32_t i = 0; i < numDevices; i++)
    {
        for (uint32_t j = 0; j < numDevices; j++)
        {
            Pal::IGpuMemory* pPalMemory = m_ppGpuMemory[i][j];
            if (pPalMemory != nullptr)
            {
                // Destroy any peer memory allocations now
                pDevice->PalDevice(i)->RemoveGpuMemoryReferences(1, &pPalMemory, nullptr);
                pPalMemory->Destroy();
            }
        }
    }
}

// =====================================================================================================================
// Allocates a remote view of an existing GPU allocation. If the exact mapping already exists use that.
Pal::IGpuMemory* PeerMemory::AllocatePeerMemory(
    Pal::IDevice*       pLocalDevice,
    uint32_t            localIdx,
    uint32_t            remoteIdx)
{
    if (m_ppGpuMemory[localIdx][remoteIdx] != nullptr)
    {
        // return the previously created mapping
        return m_ppGpuMemory[localIdx][remoteIdx];
    }

    // Create a new peer view from a real Gpu allocation.

    Pal::Result palResult;
    Pal::PeerGpuMemoryOpenInfo peerInfo = {};

    // Real memory allocations are placed on the 'diagonal' part of the 2d matrix. Therefore
    // we reference using a single input 'remoteIdx'.
    peerInfo.pOriginalMem = m_ppGpuMemory[remoteIdx][remoteIdx];
    VK_ASSERT(peerInfo.pOriginalMem != nullptr);

    VK_ASSERT(m_palObjSize == pLocalDevice->GetPeerGpuMemorySize(peerInfo, &palResult));
    VK_ASSERT(palResult == Pal::Result::Success);

    Pal::IGpuMemory* pGpuMemory = nullptr;
    palResult = pLocalDevice->OpenPeerGpuMemory(
        peerInfo,
        Util::VoidPtrInc(static_cast<void*>(this), m_allocationOffset),
        &pGpuMemory);
    VK_ASSERT(palResult == Pal::Result::Success);

    m_allocationOffset += m_palObjSize;

    m_ppGpuMemory[localIdx][remoteIdx] = pGpuMemory;

    Pal::GpuMemoryRef ref = {};
    ref.pGpuMemory = pGpuMemory;
    palResult = pLocalDevice->AddGpuMemoryReferences(1, &ref, nullptr, Pal::GpuMemoryRefCantTrim);
    VK_ASSERT(palResult == Pal::Result::Success);

    return m_ppGpuMemory[localIdx][remoteIdx];
}

// =====================================================================================================================
// Computes the maximum amount of system memory needed by the PeerMemory object
uint32_t PeerMemory::GetMemoryRequirements(
    Device*     pDevice,
    bool        multiInstanceHeap,
    uint32_t    allocationMask,
    uint32_t    palMemSize)
{
    const uint32_t numDevices = pDevice->NumPalDevices();
    if ((numDevices == 1) ||( multiInstanceHeap == false))
    {
        // Do not allocate if we are running single Gpu or we allocated remote (system) memory.
        return 0;
    }

    // Compute the maximum number of peer objects that might be allocated for accessing a single multi-instance resource
    // across all devices in the group. This works by presuming that any device might need access to any another
    // device's instance of the resource. We use the number of 'real' allocated objects via allocationmask. Each set bit
    // relates to a real allocation and we multiply this value by the numDevices-1, because the owning device doesn't
    // require a peer mapping of its own resource.
    const uint32_t maxPeerAllocations = (numDevices - 1) * Util::CountSetBits(allocationMask);

    return sizeof(PeerMemory) + (palMemSize * maxPeerAllocations);
}

} //namespace vk
