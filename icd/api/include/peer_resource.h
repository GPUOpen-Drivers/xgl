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
 * @file  peer_resource.h
 * @brief Classes to manage Multi-GPU resource sharing
 ***********************************************************************************************************************
 */
#ifndef __PEER_RESOURCE_H__
#define __PEER_RESOURCE_H__

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "palImage.h"

#define ENABLE_P2P_GENERIC_ACCESS 0
// This feature is currently disabled due to concerns that applications
// will either misuse or have an inability to achieve maximum pci-e throughput.

namespace vk
{
class Device;

// =====================================================================================================================
class PeerMemory
{
public:
    PeerMemory(
        Device*           pDevice,
        Pal::IGpuMemory** pGpuMemories,
        uint32_t          palObjectSize);

    Pal::IGpuMemory* AllocatePeerMemory(
        Pal::IDevice*   pLocalDevice,
        uint32_t        localIdx,
        uint32_t        remoteIdx);

    Pal::IGpuMemory* GetPeerMemory(
        uint32_t        localIdx,
        uint32_t        remoteIdx) const
    {
        return m_ppGpuMemory[localIdx][remoteIdx];
    }

    void Destroy(Device* pDevice);

    static uint32_t GetMemoryRequirements(
        Device*     pDevice,
        bool        multiInstanceHeap,
        uint32_t    allocationMask,
        uint32_t    palMemSize);

    uint32_t ObjSize() const
    {
        return m_palObjSize;
    }

protected:
    Pal::IGpuMemory*   m_ppGpuMemory[MaxPalDevices][MaxPalDevices];
    // Gpu memory is located in the 2D array as follows
    //                              | REAL | PEER | PEER | PEER |
    //                              | PEER | REAL | PEER | PEER |
    //                              | PEER | PEER | REAL | PEER |
    //                              | PEER | PEER | PEER | REAL |

    uint32_t           m_palObjSize;
    uint32_t           m_allocationOffset;
};

} //namespace vk

#endif //__PEER_RESOURCE_H__
