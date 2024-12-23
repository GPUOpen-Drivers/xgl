/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  cps_global_memory.h
 * @brief Contains declaration of CPS Global Memory.
 ***********************************************************************************************************************
 */

#pragma once
#ifndef __CPS_GLOBAL_MEMORY_H__
#define __CPS_GLOBAL_MEMORY_H__

#include "internal_mem_mgr.h"

#include "palGpuMemory.h"
#include "palFence.h"
#include "palList.h"

namespace vk
{

struct CpsMemTracker
{
    InternalMemory* pMem;
    Pal::IFence*    pFences[MaxPalDevices];
};

class CpsGlobalMemory
{
public:
    CpsGlobalMemory(Device* pDevice);
    ~CpsGlobalMemory();

    void FreeRetiredCpsStackMem();

    Pal::Result AllocateCpsStackMem(
        uint32_t      deviceIdx,
        uint64_t      size,
        Pal::IFence** pFences);

    const Pal::IGpuMemory& GetPalMemory(uint32_t deviceIdx) const
    {
        VK_ASSERT(deviceIdx < MaxPalDevices);
        return *m_pCpsGlobalMem->PalMemory(deviceIdx);
    }

private:
    Device*                                  m_pDevice;
    InternalMemory*                          m_pCpsGlobalMem;
    Util::List<CpsMemTracker, PalAllocator>  m_cpsMemDestroyList;

};

}
#endif
