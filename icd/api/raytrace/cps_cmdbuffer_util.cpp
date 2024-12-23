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
 * @file  cps_cmdbuffer_util.cpp
 * @brief Contains implementation of CPS Command Buffer Utils.
 ***********************************************************************************************************************
 */

#include "include/vk_device.h"
#include "raytrace/ray_tracing_device.h"
#include "raytrace/cps_cmdbuffer_util.h"

#include "palVectorImpl.h"
#include "palListImpl.h"

namespace vk
{

// =====================================================================================================================
CpsCmdBufferUtil::CpsCmdBufferUtil(
    Device* pDevice)
    :
    m_maxCpsMemSize(0),
    m_patchCpsList{
        pDevice->VkInstance()->Allocator()
#if VKI_BUILD_MAX_NUM_GPUS > 1
        , pDevice->VkInstance()->Allocator()
        , pDevice->VkInstance()->Allocator()
        , pDevice->VkInstance()->Allocator()
#endif
    }
{

}

// =====================================================================================================================
void CpsCmdBufferUtil::FreePatchCpsList(
    uint32_t deviceMask)
{
    if (m_maxCpsMemSize > 0)
    {
        // Clear the patch cps list
        utils::IterateMask deviceGroup(deviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            m_patchCpsList[deviceIdx].Clear();
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CpsCmdBufferUtil::AddPatchCpsRequest(
    uint32_t                      deviceIdx,
    GpuRt::DispatchRaysConstants* pConstsMem,
    uint64_t                      bufSize)
{
    VK_ASSERT(pConstsMem != nullptr);
    m_maxCpsMemSize = Util::Max(m_maxCpsMemSize, bufSize);
    Pal::Result result = m_patchCpsList[deviceIdx].PushBack(pConstsMem);
    VK_ASSERT(result == Pal::Result::Success);
}

// =====================================================================================================================
// Fill bufVa to each patch request (call this at execute time).
void CpsCmdBufferUtil::ApplyPatchCpsRequests(
    uint32_t               deviceIdx,
    Device*                pDevice,
    const Pal::IGpuMemory& cpsMem) const
{
    const uint32 patchCpsCount = m_patchCpsList[deviceIdx].NumElements();
    for (uint32 i = 0; i < patchCpsCount; ++i)
    {
        GpuRt::DispatchRaysConstants* pConstsMem = m_patchCpsList[deviceIdx].At(i);

        pDevice->RayTrace()->GpuRt(deviceIdx)->PatchDispatchRaysConstants(
            pConstsMem,
            cpsMem.Desc().gpuVirtAddr,
            m_maxCpsMemSize);
    }
}

}
