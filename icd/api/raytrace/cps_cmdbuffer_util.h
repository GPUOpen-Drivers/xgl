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
 * @file  cps_cmdbuffer_util.h
 * @brief Contains declaration for CPS Command Buffer Utils.
 ***********************************************************************************************************************
 */

#pragma once
#ifndef __CPS_CMDBUFFER_UTIL_H__
#define __CPS_CMDBUFFER_UTIL_H__

#include "include/vk_utils.h"

namespace vk
{

class CpsCmdBufferUtil
{
public:
    CpsCmdBufferUtil(Device* pDevice);
    ~CpsCmdBufferUtil() {};

    void FreePatchCpsList(
        uint32_t deviceMask);

    void AddPatchCpsRequest(
        uint32_t                      deviceIdx,
        GpuRt::DispatchRaysConstants* pConstsMem,
        uint64_t                      bufSize);

    void ApplyPatchCpsRequests(
        uint32_t               deviceIdx,
        Device*                pDevice,
        const Pal::IGpuMemory& cpsMem) const;

    uint64 GetCpsMemSize() const { return m_maxCpsMemSize; }

    void SetCpsMemSize(uint64_t cpsMemSize) { m_maxCpsMemSize = cpsMemSize; }

private:
    uint64_t m_maxCpsMemSize;

    typedef Util::Vector<GpuRt::DispatchRaysConstants*, 1, PalAllocator> PatchCpsVector;
    PatchCpsVector m_patchCpsList[MaxPalDevices];
};

}

#endif
