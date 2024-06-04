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
* @file  bvh_batch_layer.cpp
* @brief Implementation of bvh batch layer.
***********************************************************************************************************************
*/

#if VKI_RAY_TRACING

#include <inttypes.h>

#include "bvh_batch_layer.h"
#include "vk_cmdbuffer.h"
#include "raytrace/ray_tracing_device.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
BvhBatchLayer::BvhBatchLayer(
    Device* pDevice)
    :
    m_pInstance(pDevice->VkInstance()),
    m_emptyStateCount(0),
    m_pEmptyStateStack()
{
}

// =====================================================================================================================
BvhBatchLayer::~BvhBatchLayer()
{
    for (uint32_t stateIdx = 0; stateIdx < m_emptyStateCount; ++stateIdx)
    {
        m_pEmptyStateStack[stateIdx]->DestroyState();
    }
}

// =====================================================================================================================
VkResult BvhBatchLayer::Init(
    Device* pDevice)
{
    VkResult result = VK_SUCCESS;

    if (pDevice->GetRuntimeSettings().batchBvhBuilds == BatchBvhModeImplicitAndLog)
    {
        const char* pRootDir = pDevice->PalDevice(DefaultDeviceIndex)->GetDebugFilePath();

        if (pRootDir != nullptr)
        {
            char absPath[1024] = {};
            Util::Snprintf(absPath, sizeof(absPath), "%s/%s", pRootDir, "BvhBatchLog.txt");

            if (result == VK_SUCCESS)
            {
                result = PalToVkResult(m_logFile.Open(absPath, Util::FileAccessMode::FileAccessAppend));
            }

            if (result == VK_SUCCESS)
            {
                result = PalToVkResult(m_logFile.Printf("|--------------BEGIN RUN--------------\n"));
            }
        }
        else
        {
            // AMD_DEBUG_DIR must be set for logging
            result = VK_ERROR_UNKNOWN;
        }
    }

    return result;
}

// =====================================================================================================================
VkResult BvhBatchLayer::CreateLayer(
    Device*         pDevice,
    BvhBatchLayer** ppLayer)
{
    VkResult               result   = VK_SUCCESS;
    BvhBatchLayer*         pLayer   = nullptr;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if ((settings.batchBvhBuilds == BatchBvhModeImplicit) || (settings.batchBvhBuilds == BatchBvhModeImplicitAndLog))
    {
        void* pMem = pDevice->VkInstance()->AllocMem(sizeof(BvhBatchLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMem != nullptr)
        {
            pLayer = VK_PLACEMENT_NEW(pMem) BvhBatchLayer(pDevice);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS)
    {
        result = pLayer->Init(pDevice);
    }

    if (result == VK_SUCCESS)
    {
        *ppLayer = pLayer;
    }

    return result;
}

// =====================================================================================================================
void BvhBatchLayer::DestroyLayer()
{
    m_logFile.Printf("|--------------END RUN--------------\n");
    m_logFile.Close();

    Instance* pInstance = VkInstance();
    Util::Destructor(this);
    pInstance->FreeMem(this);
}

// =====================================================================================================================
void BvhBatchLayer::VLog(
    const char* pFormat,
    va_list     argList)
{
    VK_ASSERT(LoggingEnabled());

    Util::MutexAuto lock(&m_mutex);

    Util::Result printResult = m_logFile.VPrintf(pFormat, argList);
    VK_ASSERT(printResult == Util::Result::Success);
}

// =====================================================================================================================
BvhBatchState* BvhBatchLayer::CreateState(
    CmdBuffer* pCmdBuffer)
{
    // Try to reuse a previously freed state
    BvhBatchState* pState = PopEmptyState();

    if (pState != nullptr)
    {
        pState->Log("Reusing a stashed BvhBatchState.\n");
    }
    else
    {
        // Allocate a new state if no previously freed states were available
        void* pMem = m_pInstance->AllocMem(sizeof(BvhBatchState), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        pState = (pMem != nullptr) ? (VK_PLACEMENT_NEW(pMem) BvhBatchState(this)) : nullptr;
    }

    // Link this state to the given cmd buffer
    pCmdBuffer->SetBvhBatchState(pState);

    VK_ASSERT(pState != nullptr);
    return pState;
}

// =====================================================================================================================
bool BvhBatchLayer::PushEmptyState(
    BvhBatchState* pState)
{
    bool success = false;

    Util::MutexAuto lock(&m_mutex);

    if (m_emptyStateCount < VK_ARRAY_SIZE(m_pEmptyStateStack))
    {
        m_pEmptyStateStack[m_emptyStateCount] = pState;
        m_emptyStateCount++;

        success = true;
    }

    return success;
}

// =====================================================================================================================
BvhBatchState* BvhBatchLayer::PopEmptyState()
{
    BvhBatchState* pState = nullptr;

    Util::MutexAuto lock(&m_mutex);

    if (m_emptyStateCount > 0)
    {
        m_emptyStateCount--;
        pState = m_pEmptyStateStack[m_emptyStateCount];
    }

    return pState;
}

// =====================================================================================================================
BvhBatchState::BvhBatchState(
    BvhBatchLayer* pLayer)
    :
    m_type(BvhBatchType::Undefined),
    m_pCmdBuffer(nullptr),
    m_pLayer(pLayer),
    m_geomInfos(pLayer->VkInstance()->Allocator()),
    m_rangeInfosOrMaxPrimCounts(pLayer->VkInstance()->Allocator()),
    m_indirectVirtAddrs(pLayer->VkInstance()->Allocator()),
    m_indirectStrides(pLayer->VkInstance()->Allocator()),
    m_infoCount(0),
    m_allocations(pLayer->VkInstance()->Allocator())
{
    Log("Allocating a new BvhBatchState.\n");
}

// =====================================================================================================================
BvhBatchState::~BvhBatchState()
{
}

// =====================================================================================================================
void BvhBatchState::Log(
    const char* pFormat,
    ...)
{
    if (m_pLayer->LoggingEnabled())
    {
        char prependedStr[21] = {};
        Util::Snprintf(prependedStr, sizeof(prependedStr), "|-- 0x%" PRIx64 " - ", this);

        va_list argList = {};
        m_pLayer->VLog(prependedStr, argList);

        va_start(argList, pFormat);
        m_pLayer->VLog(pFormat, argList);
        va_end(argList);
    }
}

// =====================================================================================================================
void BvhBatchState::DestroyState()
{
    Log("Freeing a BvhBatchState.\n");
    Util::Destructor(this);
    m_pLayer->VkInstance()->FreeMem(this);
}

// =====================================================================================================================
void BvhBatchState::Reset()
{
    for (auto pMem : m_allocations)
    {
        m_pLayer->VkInstance()->FreeMem(pMem);
    }

    m_type = BvhBatchType::Undefined;
    m_allocations.Clear();
    m_geomInfos.Clear();
    m_rangeInfosOrMaxPrimCounts.Clear();
    m_indirectVirtAddrs.Clear();
    m_indirectStrides.Clear();
    m_infoCount = 0;

    // Unlink this state from the cmd buffer
    m_pCmdBuffer->SetBvhBatchState(nullptr);
    m_pCmdBuffer = nullptr;

    // Try to stash this now empty state to be reused later
    if (m_pLayer->PushEmptyState(this))
    {
        Log("Stashing a BvhBatchState during reset.\n");
    }
    else
    {
        DestroyState();
    }
}

// =====================================================================================================================
template<BvhBatchType batchType>
bool BvhBatchState::EnqueueBvhBuild(
    CmdBuffer*                                             pCmdBuffer,
    uint32_t                                               infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*     pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos,
    const VkDeviceAddress*                                 pIndirectDeviceAddresses,
    const uint32_t*                                        pIndirectStrides,
    const uint32_t* const*                                 ppMaxPrimitiveCounts)
{
    static_assert(batchType != BvhBatchType::Undefined, "Invalid batch type provided to EnqueueBvhBuild via template.");

    // Ensure the batch type in the state matches
    if ((m_type != batchType) && (m_type != BvhBatchType::Undefined))
    {
        Flush();
    }

    // Determine how much memory the hard copy needs
    size_t memSize = GetHardCopyMemSize<batchType>(infoCount, pInfos);

    // Allocate memory for the hard copy
    void* pMem = m_pLayer->VkInstance()->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    // Hard copy given data
    if (pMem != nullptr)
    {
        if (m_infoCount == 0)
        {
            m_pCmdBuffer = pCmdBuffer;
        }
        else if (m_pCmdBuffer != pCmdBuffer)
        {
            // CmdBuffer pointer shouldn't change when pending infos are present
            VK_NEVER_CALLED();
            Flush();
        }

        Log("Enqueueing %u BVH build infos (batchType - %u).\n", infoCount, batchType);
        HardCopyBuildInfos<batchType>(
            infoCount,
            pInfos,
            ppBuildRangeInfos,
            pIndirectDeviceAddresses,
            pIndirectStrides,
            ppMaxPrimitiveCounts,
            pMem,
            memSize);
    }
    else
    {
        // Failed to allocate memory
        VK_NEVER_CALLED();
    }

    return (pMem != nullptr);
}

// =====================================================================================================================
void BvhBatchState::Flush()
{
    if (m_infoCount > 0)
    {
        BvhBatchLayer* pLayer = m_pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();

        VK_ASSERT(m_type != BvhBatchType::Undefined);

        if (m_type == BvhBatchType::Direct)
        {
            Log("Flushing a direct build batch (infoCount - %u).\n", m_infoCount);
            BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresKHR)(
                reinterpret_cast<VkCommandBuffer>(ApiCmdBuffer::FromObject(m_pCmdBuffer)),
                m_infoCount,
                m_geomInfos.Data(),
                reinterpret_cast<VkAccelerationStructureBuildRangeInfoKHR**>(m_rangeInfosOrMaxPrimCounts.Data()));
        }
        else
        {
            Log("Flushing an indirect build batch (infoCount - %u).\n", m_infoCount);
            BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresIndirectKHR)(
                reinterpret_cast<VkCommandBuffer>(ApiCmdBuffer::FromObject(m_pCmdBuffer)),
                m_infoCount,
                m_geomInfos.Data(),
                m_indirectVirtAddrs.Data(),
                m_indirectStrides.Data(),
                reinterpret_cast<uint32_t**>(m_rangeInfosOrMaxPrimCounts.Data()));
        }

        Reset();
    }
}

// =====================================================================================================================
void BvhBatchState::TryFlush(
    VkFlags64      srcStageMask)
{
    constexpr VkFlags64 TargetSrcStages =
        VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT |
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    if ((srcStageMask & TargetSrcStages) != 0)
    {
        Log("Flushing via barrier or event (srcStageMask - %llu).\n", srcStageMask);
        Flush();
    }
}

// =====================================================================================================================
void BvhBatchState::TryFlush(
    uint32_t                depInfoCount,
    const VkDependencyInfo* pDependencyInfos)
{
    VkFlags64 globalSrcMask = 0u;

    for (uint32_t i = 0; i < depInfoCount; ++i)
    {
        const auto& dependencyInfo = pDependencyInfos[i];

        for (uint32_t j = 0; j < dependencyInfo.memoryBarrierCount; j++)
        {
            globalSrcMask |= dependencyInfo.pMemoryBarriers[j].srcStageMask;
        }
        for (uint32_t j = 0; j < dependencyInfo.bufferMemoryBarrierCount; j++)
        {
            globalSrcMask |= dependencyInfo.pBufferMemoryBarriers[j].srcStageMask;
        }
        for (uint32_t j = 0; j < dependencyInfo.imageMemoryBarrierCount; j++)
        {
            globalSrcMask |= dependencyInfo.pImageMemoryBarriers[j].srcStageMask;
        }
    }

    TryFlush(globalSrcMask);
}

// =====================================================================================================================
template<BvhBatchType batchType>
size_t BvhBatchState::GetHardCopyMemSize(
    uint32_t                                           infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos)
{
    // Calculate total geometry structs and ptrs across all infos
    size_t totalGeomCount    = 0;
    size_t totalGeomPtrCount = 0;
    for (uint32_t infoIdx = 0; infoIdx < infoCount; ++infoIdx)
    {
        totalGeomCount += pInfos[infoIdx].geometryCount;

        if (pInfos[infoIdx].ppGeometries != nullptr)
        {
            totalGeomPtrCount += pInfos[infoIdx].geometryCount;
        }
    }

    // Memory size for pGeometries and ppGeometies
    size_t memSize =
        (totalGeomCount * sizeof(VkAccelerationStructureGeometryKHR)) +
        (totalGeomPtrCount * sizeof(void*));

    // Memory size for ppBuildRangeInfos or ppMaxPrimitiveCounts
    if (batchType == BvhBatchType::Direct)
    {
        memSize += (totalGeomCount * sizeof(VkAccelerationStructureBuildRangeInfoKHR));
    }
    else
    {
        memSize += (totalGeomCount * sizeof(uint32_t*));
    }

    // Report the memory size required
    return memSize;
}

// =====================================================================================================================
template<BvhBatchType batchType>
void BvhBatchState::HardCopyBuildInfos(
    uint32_t                                               infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*     pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos,
    const VkDeviceAddress*                                 pIndirectDeviceAddresses,
    const uint32_t*                                        pIndirectStrides,
    const uint32_t* const*                                 ppMaxPrimitiveCounts,
    void*                                                  pMem,
    size_t                                                 memSize)
{
    m_allocations.PushBack(pMem);

    for (uint32_t infoIdx = 0; infoIdx < infoCount; ++infoIdx)
    {
        VkAccelerationStructureBuildGeometryInfoKHR geomInfoDst = pInfos[infoIdx];

        // Per spec, pNext must be NULL
        VK_ASSERT(geomInfoDst.pNext == nullptr);

        const size_t geometrySize    = geomInfoDst.geometryCount * sizeof(VkAccelerationStructureGeometryKHR);
        const size_t geometryPtrSize = geomInfoDst.geometryCount * sizeof(void*);

        if (geomInfoDst.ppGeometries != nullptr)
        {
            // Array of Goemetry pointers
            VkAccelerationStructureGeometryKHR** ppGeometries =
                static_cast<VkAccelerationStructureGeometryKHR**>(pMem);

            // Geometry descs follow the pointers
            VkAccelerationStructureGeometryKHR* pGeometries =
                static_cast<VkAccelerationStructureGeometryKHR*>(Util::VoidPtrInc(pMem, geometryPtrSize));

            // Copy each geometry info and its new pointer into the internal allocation
            for (uint32 i = 0; i < geomInfoDst.geometryCount; i++)
            {
                pGeometries[i] = *geomInfoDst.ppGeometries[i];
                ppGeometries[i] = &pGeometries[i];
            }

            // Apply the local copy
            geomInfoDst.ppGeometries =
                static_cast<const VkAccelerationStructureGeometryKHR* const*>(pMem);

            // Increment the data pointer for the following copy
            pMem = Util::VoidPtrInc(pMem, geometrySize + geometryPtrSize);
        }
        else
        {
            // Copy original geometry info into the internal allocation
            memcpy(pMem, geomInfoDst.pGeometries, geometrySize);

            // Apply the local copy
            geomInfoDst.pGeometries =
                static_cast<const VkAccelerationStructureGeometryKHR*>(pMem);

            // Increment the data pointer for the following copy
            pMem = Util::VoidPtrInc(pMem, geometrySize);
        }

        m_type = batchType;
        m_geomInfos.PushBack(geomInfoDst);
        m_infoCount++;

        if (batchType == BvhBatchType::Direct)
        {
            // Copy BuildRangeInfos into internal allocation
            const size_t rangeInfoSize = geomInfoDst.geometryCount * sizeof(VkAccelerationStructureBuildRangeInfoKHR);
            memcpy(pMem, ppBuildRangeInfos[infoIdx], rangeInfoSize);

            m_rangeInfosOrMaxPrimCounts.PushBack(pMem);

            // Increment the data pointer for the following copy
            pMem = Util::VoidPtrInc(pMem, rangeInfoSize);
        }
        else
        {
            // Copy MaxPrimitiveCounts into internal allocation
            const size_t maxPrimCountsSize = geomInfoDst.geometryCount * sizeof(uint32_t);
            memcpy(pMem, ppMaxPrimitiveCounts[infoIdx], maxPrimCountsSize);

            m_rangeInfosOrMaxPrimCounts.PushBack(pMem);

            // Increment the data pointer for the following copy
            pMem = Util::VoidPtrInc(pMem, maxPrimCountsSize);

            m_indirectVirtAddrs.PushBack(pIndirectDeviceAddresses[infoIdx]);
            m_indirectStrides.PushBack(pIndirectStrides[infoIdx]);
        }
    }

    // Ensure that we did not overallocate nor underallocate
    VK_ASSERT((reinterpret_cast<size_t>(pMem) - reinterpret_cast<size_t>(m_allocations.Back())) == memSize);
}

namespace entry
{

namespace bvhBatchLayer
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer                                         commandBuffer,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos)
{
    bool           queued     = false;
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState == nullptr)
    {
        pState = pLayer->CreateState(pCmdBuffer);
    }

    if (pState != nullptr)
    {
        queued = pState->EnqueueBvhBuild<BvhBatchType::Direct>(
                     pCmdBuffer,
                     infoCount,
                     pInfos,
                     ppBuildRangeInfos,
                     nullptr,
                     nullptr,
                     nullptr);

        if (queued == false)
        {
            // State exists, but we were not able to enqueue. Flush any valid contents in the batch.
            pState->Flush();
        }
    }

    if (queued == false)
    {
        // We were not able to batch. Add directly to cmd buffer.
        BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresKHR)(
            commandBuffer,
            infoCount,
            pInfos,
            ppBuildRangeInfos);
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer                                    commandBuffer,
    uint32_t                                           infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkDeviceAddress*                             pIndirectDeviceAddresses,
    const uint32_t*                                    pIndirectStrides,
    const uint32_t* const*                             ppMaxPrimitiveCounts)
{
    bool           queued     = false;
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState == nullptr)
    {
        pState = pLayer->CreateState(pCmdBuffer);
    }

    if (pState != nullptr)
    {
        queued = pState->EnqueueBvhBuild<BvhBatchType::Indirect>(
                     pCmdBuffer,
                     infoCount,
                     pInfos,
                     nullptr,
                     pIndirectDeviceAddresses,
                     pIndirectStrides,
                     ppMaxPrimitiveCounts);

        if (queued == false)
        {
            // State exists, but we were not able to enqueue. Flush any valid contents in the batch.
            pState->Flush();
        }
    }

    if (queued == false)
    {
        // We were not able to batch. Add directly to cmd buffer.
        BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresIndirectKHR)(
            commandBuffer,
            infoCount,
            pInfos,
            pIndirectDeviceAddresses,
            pIndirectStrides,
            ppMaxPrimitiveCounts);
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer              commandBuffer,
    VkPipelineStageFlags         srcStageMask,
    VkPipelineStageFlags         dstStageMask,
    VkDependencyFlags            dependencyFlags,
    uint32_t                     memoryBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState != nullptr)
    {
        pState->TryFlush(srcStageMask);
    }

    BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdPipelineBarrier)(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        dependencyFlags,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(
    VkCommandBuffer            commandBuffer,
    const VkDependencyInfoKHR* pDependencyInfo)
{
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState != nullptr)
    {
        pState->TryFlush(1, pDependencyInfo);
    }

    BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdPipelineBarrier2)(commandBuffer, pDependencyInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents(
    VkCommandBuffer              commandBuffer,
    uint32_t                     eventCount,
    const VkEvent*               pEvents,
    VkPipelineStageFlags         srcStageMask,
    VkPipelineStageFlags         dstStageMask,
    uint32_t                     memoryBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState != nullptr)
    {
        pState->TryFlush(srcStageMask);
    }

    BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdWaitEvents)(
        commandBuffer,
        eventCount,
        pEvents,
        srcStageMask,
        dstStageMask,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents2(
    VkCommandBuffer            commandBuffer,
    uint32_t                   eventCount,
    const VkEvent*             pEvents,
    const VkDependencyInfoKHR* pDependencyInfos)
{
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState != nullptr)
    {
        pState->TryFlush(eventCount, pDependencyInfos);
    }

    BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkCmdWaitEvents2)(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(
    VkCommandBuffer commandBuffer)
{
    CmdBuffer*     pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    BvhBatchLayer* pLayer     = pCmdBuffer->VkDevice()->RayTrace()->GetBvhBatchLayer();
    BvhBatchState* pState     = pCmdBuffer->GetBvhBatchState();

    if (pState != nullptr)
    {
        pState->Log("Flushing via vkEndCommandBuffer\n");
        pState->Flush();
    }

    return BVH_BATCH_LAYER_CALL_NEXT_LAYER(vkEndCommandBuffer)(commandBuffer);
}

} // namespace bvhBatchLayer

} // namespace entry

// =====================================================================================================================
void BvhBatchLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdBuildAccelerationStructuresKHR);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdBuildAccelerationStructuresIndirectKHR);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdPipelineBarrier);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdPipelineBarrier2);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdWaitEvents);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkCmdWaitEvents2);
    BVH_BATCH_LAYER_OVERRIDE_ENTRY(vkEndCommandBuffer);
}

} // namespace vk

#endif
