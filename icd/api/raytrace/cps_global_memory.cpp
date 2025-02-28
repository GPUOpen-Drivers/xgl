/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  cps_global_memory.cpp
 * @brief Implementation of CPS global memory management for ray tracing.
 **************************************************************************************************
 */

#include "include/vk_device.h"
#include "raytrace/ray_tracing_device.h"
#include "raytrace/cps_global_memory.h"

#include "palVectorImpl.h"
#include "palListImpl.h"

namespace vk
{

// =====================================================================================================================
CpsGlobalMemory::CpsGlobalMemory(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pCpsGlobalMem(nullptr),
    m_cpsMemDestroyList(pDevice->VkInstance()->Allocator())
{

}

// =====================================================================================================================
CpsGlobalMemory::~CpsGlobalMemory()
{
    FreeRetiredCpsStackMem();

    VK_ASSERT(m_cpsMemDestroyList.NumElements() == 0);

    if (m_pCpsGlobalMem != nullptr)
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_pCpsGlobalMem);
        Util::Destructor(m_pCpsGlobalMem);
        m_pDevice->VkInstance()->FreeMem(m_pCpsGlobalMem);
        m_pCpsGlobalMem = nullptr;
    }
}

// =====================================================================================================================
// Check the allocations in m_cpsMemDestroyList, free the retired ones.
void CpsGlobalMemory::FreeRetiredCpsStackMem()
{
    auto cpsMemDestroyListIter = m_cpsMemDestroyList.Begin();
    while (cpsMemDestroyListIter != m_cpsMemDestroyList.End())
    {
        CpsMemTracker* pRetiredEntry = cpsMemDestroyListIter.Get();

        // Free retired CPS Fences
        bool freeCpsMemory = true;
        utils::IterateMask deviceGroup(m_pDevice->GetPalDeviceMask());
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            if (pRetiredEntry->pFences[deviceIdx]->GetStatus() == Pal::Result::Success)
            {
                pRetiredEntry->pFences[deviceIdx]->Destroy();
                m_pDevice->VkInstance()->FreeMem(pRetiredEntry->pFences[deviceIdx]);
            }
            else
            {
                freeCpsMemory = false;
            }
        } while (deviceGroup.IterateNext());

        if (freeCpsMemory)
        {
            // Free retired CPS Memory
            m_pDevice->MemMgr()->FreeGpuMem(pRetiredEntry->pMem);

            Util::Destructor(pRetiredEntry->pMem);
            m_pDevice->VkInstance()->FreeMem(pRetiredEntry->pMem);

            // Implicitly preceed the iterator to next node.
            m_cpsMemDestroyList.Erase(&cpsMemDestroyListIter);
        }
        else
        {
            cpsMemDestroyListIter.Next();
        }
    }
}

// =====================================================================================================================
// Allocate Cps global memory.
// - Allocate if it does not exist.
// - Reallocate m_pCpsGlobalMem from X To Y if its size is not big enough. X is put into m_cpsMemDestroyList to be freed
//   later. A fence is generated and passed in the submission to Pal. When it is signaled, X is freed. Note it is
//   signaled when the first cmdbuf switching to Y is done, so not optimal regarding memory footprint. Ideally it can be
//   signalled when X is retired, but that means every submission referencing X has to signal an extra IFence even
//   m_pCpsGlobalMem stays unchanged. The reason is we dont know if the next submission will require a bigger cps stack
//   memory.
Pal::Result CpsGlobalMemory::AllocateCpsStackMem(
    uint32_t      allocDeviceMask,
    uint64_t      size,
    Pal::IFence** pFences)
{
    VK_ASSERT(m_pDevice->GetRuntimeSettings().cpsFlags & CpsFlagStackInGlobalMem);

    Pal::Result palResult = Pal::Result::Success;

    if ((m_pCpsGlobalMem == nullptr) ||
        (m_pCpsGlobalMem->Size() < size))
    {
        InternalMemory* pCpsVidMem = nullptr;
        void* pMem = m_pDevice->VkInstance()->AllocMem(sizeof(InternalMemory),
                                                       VK_DEFAULT_MEM_ALIGN,
                                                       VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
        if (pMem == nullptr)
        {
            palResult = Pal::Result::ErrorOutOfMemory;
        }

        if (palResult == Pal::Result::Success)
        {
            pCpsVidMem = VK_PLACEMENT_NEW(pMem) InternalMemory;

            InternalMemCreateInfo allocInfo = {};
            allocInfo.pal.size              = size;
            allocInfo.pal.alignment         = VK_DEFAULT_MEM_ALIGN;
            allocInfo.pal.priority          = Pal::GpuMemPriority::Normal;
            m_pDevice->MemMgr()->GetCommonPool(InternalPoolGpuAccess, &allocInfo);

            VkResult result = m_pDevice->MemMgr()->AllocGpuMem(
                allocInfo,
                pCpsVidMem,
                allocDeviceMask,
                VK_OBJECT_TYPE_QUEUE,
                ApiDevice::IntValueFromHandle(ApiDevice::FromObject(m_pDevice)));

            if (result != VK_SUCCESS)
            {
                palResult = Pal::Result::ErrorOutOfMemory;
            }
            else if (m_pCpsGlobalMem == nullptr) // first alloc
            {
                m_pCpsGlobalMem = pCpsVidMem;
            }
            else if (pCpsVidMem != nullptr)
            {
                const size_t palFenceSize = m_pDevice->PalDevice(DefaultDeviceIndex)->GetFenceSize(&palResult);
                VK_ASSERT(palResult == Pal::Result::Success);

                void* pPalMemory = m_pDevice->VkInstance()->AllocMem(
                    (palFenceSize * MaxPalDevices),
                    VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

                utils::IterateMask deviceGroup(m_pDevice->GetPalDeviceMask());
                do
                {
                    const uint32_t deviceIdx  = deviceGroup.Index();
                    Pal::IDevice* pPalDevice  = m_pDevice->PalDevice(deviceIdx);

                    Pal::FenceCreateInfo fenceInfo = {};
                    fenceInfo.flags.signaled = 0;

                    if (pPalMemory != nullptr)
                    {
                        palResult = pPalDevice->CreateFence(
                            fenceInfo,
                            Util::VoidPtrInc(pPalMemory, (deviceIdx * palFenceSize)),
                            &pFences[deviceIdx]);

                        VK_ASSERT(palResult == Pal::Result::Success);
                    }
                    else
                    {
                        palResult = Pal::Result::ErrorOutOfMemory;
                    }

                } while (deviceGroup.IterateNext() &&
                        (palResult == Pal::Result::Success));

                VK_ASSERT(palResult == Pal::Result::Success);

                CpsMemTracker tracker = { m_pCpsGlobalMem, *pFences };
                m_cpsMemDestroyList.PushBack(tracker);
                m_pCpsGlobalMem = pCpsVidMem;
            }
        }

        // Initialize CPS Memory
        if (palResult == Pal::Result::Success)
        {
            utils::IterateMask deviceGroup(m_pDevice->GetPalDeviceMask());
            do
            {
                const uint32_t deviceIdx  = deviceGroup.Index();
                GpuRt::IDevice* pRtDevice = m_pDevice->RayTrace()->GpuRt(deviceIdx);

                palResult = pRtDevice->InitializeCpsMemory(
                    *m_pCpsGlobalMem->PalMemory(deviceIdx),
                    size);

            } while (deviceGroup.IterateNext() &&
                    (palResult == Pal::Result::Success));
        }
    }

    return palResult;
}

}
