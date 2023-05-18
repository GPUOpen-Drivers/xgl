/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_query.cpp
 * @brief Contains implementation of Vulkan query pool.
 ***********************************************************************************************************************
 */

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_query.h"

#include "palAutoBuffer.h"
#include "palQueryPool.h"
#include "palSysUtil.h"

namespace vk
{

// =====================================================================================================================
// Creates a new query pool object.
VkResult QueryPool::Create(
    Device*                      pDevice,
    const VkQueryPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkQueryPool*                 pQueryPool)
{
    VkResult result;
    QueryPool* pObject = nullptr;

    if (pCreateInfo->queryType != VK_QUERY_TYPE_TIMESTAMP)
    {
#if VKI_RAY_TRACING
        if (IsAccelerationStructureQueryType(pCreateInfo->queryType))
        {
            result = AccelerationStructureQueryPool::Create(pDevice, pCreateInfo, pAllocator, &pObject);
        }
        else
#endif
        {
            result = PalQueryPool::Create(pDevice, pCreateInfo, pAllocator, &pObject);
        }
    }
    else
    {
        result = TimestampQueryPool::Create(pDevice, pCreateInfo, pAllocator, &pObject);
    }

    if (result == VK_SUCCESS)
    {
        *pQueryPool = QueryPool::HandleFromObject(pObject);
    }

    return result;
}

// =====================================================================================================================
// Creates a new query pool object (PAL query pool types).
VkResult PalQueryPool::Create(
    Device*                      pDevice,
    const VkQueryPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    QueryPool**                  ppQueryPool)
{
    VK_ASSERT(pCreateInfo->queryType != VK_QUERY_TYPE_TIMESTAMP);

    Pal::Result palResult;
    VkResult result = VK_SUCCESS;

    Pal::QueryPoolCreateInfo createInfo = {};
    Pal::QueryType           queryType  = Pal::QueryType::Occlusion;

    if (pCreateInfo->queryType == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)
    {
        queryType                = Pal::QueryType::StreamoutStats;
        createInfo.queryPoolType = Pal::QueryPoolType::StreamoutStats;
    }
    else if ((pCreateInfo->queryType == VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT) ||
             (pCreateInfo->queryType == VK_QUERY_TYPE_MESH_PRIMITIVES_GENERATED_EXT))
    {
        queryType                = Pal::QueryType::PipelineStats;
        createInfo.queryPoolType = Pal::QueryPoolType::PipelineStats;
    }

    if (VK_ENUM_IN_RANGE(pCreateInfo->queryType, VK_QUERY_TYPE))
    {
        queryType = VkToPalQueryType(pCreateInfo->queryType);
        createInfo.queryPoolType = VkToPalQueryPoolType(pCreateInfo->queryType);
    }

    createInfo.numSlots = pCreateInfo->queryCount;

    VkQueryPipelineStatisticFlags enabledStats = pCreateInfo->pipelineStatistics;

    if (pCreateInfo->queryType == VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT)
    {
        enabledStats |= VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
    }

    createInfo.enabledStats = VkToPalQueryPipelineStatsFlags(enabledStats);

    if (pCreateInfo->queryType == VK_QUERY_TYPE_MESH_PRIMITIVES_GENERATED_EXT)
    {
        createInfo.enabledStats |= Pal::QueryPipelineStatsMsPrimitives;
    }

    createInfo.flags.enableCpuAccess = true;

    const size_t palSize = pDevice->PalDevice(DefaultDeviceIndex)->GetQueryPoolSize(createInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    const size_t apiSize    = sizeof(PalQueryPool);
    const size_t size       = apiSize + (pDevice->NumPalDevices() * palSize);

    // Allocate enough system memory for the API query pool object and the PAL query pool object
    void* pSystemMem = pDevice->AllocApiObject(pAllocator, size);

    if (pSystemMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Create the PAL query pool
    Pal::IQueryPool* pPalQueryPools[MaxPalDevices] = {};

    void* pPalQueryPoolAddr = Util::VoidPtrInc(pSystemMem, apiSize);

    for (uint32_t deviceIdx = 0;
        (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        palResult = pDevice->PalDevice(deviceIdx)->CreateQueryPool(
            createInfo,
            Util::VoidPtrInc(pPalQueryPoolAddr, deviceIdx * palSize),
            &pPalQueryPools[deviceIdx]);

        result = PalToVkResult(palResult);
    }

    PalQueryPool* pObject = nullptr;

    if (result == VK_SUCCESS)
    {
        pObject = VK_PLACEMENT_NEW(pSystemMem) PalQueryPool(
            pDevice,
            pCreateInfo->queryType,
            queryType,
            pPalQueryPools);

        result = pObject->Initialize();
    }

    if (result == VK_SUCCESS)
    {
        *ppQueryPool = pObject;
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            // Something went wrong
            if (pPalQueryPools[deviceIdx] != nullptr)
            {
                pPalQueryPools[deviceIdx]->Destroy();
            }
        }

        // Call destructor
        Util::Destructor(pObject);

        // Failure in creating the PAL query pool object. Free system memory and return error.
        pDevice->FreeApiObject(pAllocator, pSystemMem);
    }

    return result;
}

// =====================================================================================================================
// Initialize query pool object (PAL query pools)
VkResult PalQueryPool::Initialize()
{
    VkResult result = VK_SUCCESS;

    // Allocate and bind GPU memory for the object
    const bool removeInvisibleHeap = true;
    const bool persistentMapped = true;

    result = m_pDevice->MemMgr()->AllocAndBindGpuMem(
        m_pDevice->NumPalDevices(),
        reinterpret_cast<Pal::IGpuMemoryBindable**>(m_pPalQueryPool),
        false,
        &m_internalMem,
        m_pDevice->GetPalDeviceMask(),
        removeInvisibleHeap,
        persistentMapped,
        VK_OBJECT_TYPE_QUERY_POOL,
        PalQueryPool::IntValueFromHandle(PalQueryPool::HandleFromObject(this)));

    return result;
}

// =====================================================================================================================
// Destroy query pool object (PAL query pools)
VkResult PalQueryPool::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Destroy the PAL objects
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalQueryPool[deviceIdx] != nullptr)
        {
            m_pPalQueryPool[deviceIdx]->Destroy();
        }
    }

    // Free internal GPU memory allocation used by the object
    pDevice->MemMgr()->FreeGpuMem(&m_internalMem);
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Get the results of a range of query slots (PAL query pools)
VkResult PalQueryPool::GetResults(
    Device*             pDevice,
    uint32_t            startQuery,
    uint32_t            queryCount,
    size_t              dataSize,
    void*               pData,
    VkDeviceSize        stride,
    VkQueryResultFlags  flags)
{
    VkResult result = VK_SUCCESS;

    if (queryCount > 0)
    {
        VK_ASSERT(queryCount * stride <= dataSize);

        void*              pQueryData           = pData;
        VkQueryResultFlags queryFlags           = flags;
        size_t             queryDataSize        = dataSize;
        VkDeviceSize       queryDataStride      = stride;

        const bool availability = flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

        // HW will returns two 64-bits integers for the query of transform feedback, they are written primitives and the
        // number of needed primitives. And if the flag VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set, an extra integer
        // which indicates the availability state needs to be written.
        const uint32_t numXfbQueryDataElems = availability ? 3 : 2;

        // Vulkan supports 32-bit unsigned integer values data of transform feedback query, but Pal supports 64-bit only.
        // So the query data is stored into pXfbQueryData first.
        uint64_t* pXfbQueryData = nullptr;

        if (m_queryType == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)
        {
            queryDataStride  = sizeof(uint64_t) * numXfbQueryDataElems;
            queryDataSize    = queryDataStride * queryCount;
            queryFlags      |= VK_QUERY_RESULT_64_BIT;

            pXfbQueryData    = static_cast<uint64_t*>(pDevice->VkInstance()->AllocMem(
                queryDataSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));
            if (pXfbQueryData == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            pQueryData       = pXfbQueryData;
        }

        if (result == VK_SUCCESS)
        {
            Pal::Result palResult = m_pPalQueryPool[DefaultDeviceIndex]->GetResults(
                VkToPalQueryResultFlags(queryFlags),
                m_palQueryType,
                startQuery,
                queryCount,
                m_internalMem.CpuAddr(DefaultDeviceIndex),
                &queryDataSize,
                pQueryData,
                static_cast<size_t>(queryDataStride));

            result = PalToVkResult(palResult);
        }

        if ((m_queryType == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT) &&
            ((result == VK_SUCCESS) || (result == VK_NOT_READY)))
        {
            stride = (stride == 0) ? queryDataStride : stride;

            for (size_t i = 0; i < queryCount; i++)
            {
                uint64_t* pXfbQueryElem = &pXfbQueryData[i * numXfbQueryDataElems];

                // The number of written primitives and the number of needed primitives are in reverse order in Pal.
                if ((flags & VK_QUERY_RESULT_64_BIT) == 0)
                {
                    uint32_t* pPrimitivesCount = static_cast<uint32_t*>(pData);

                    if ((result == VK_SUCCESS) || (flags & VK_QUERY_RESULT_PARTIAL_BIT))
                    {
                        pPrimitivesCount[0] = static_cast<uint32_t>(pXfbQueryElem[1]);
                        pPrimitivesCount[1] = static_cast<uint32_t>(pXfbQueryElem[0]);
                    }

                    if (availability)
                    {
                        // Set the availability state to the last slot.
                        pPrimitivesCount[2] = static_cast<uint32_t>(pXfbQueryElem[2]);
                    }
                }
                else
                {
                    uint64_t* pPrimitivesCount = static_cast<uint64_t*>(pData);

                    if ((result == VK_SUCCESS) || (flags & VK_QUERY_RESULT_PARTIAL_BIT))
                    {
                        pPrimitivesCount[0] = pXfbQueryElem[1];
                        pPrimitivesCount[1] = pXfbQueryElem[0];
                    }

                    if (availability)
                    {
                        // Set the availability state to the last slot.
                        pPrimitivesCount[2] = pXfbQueryElem[2];
                    }
                }

                pData = Util::VoidPtrInc(pData, static_cast<size_t>(stride));
            }

            pDevice->VkInstance()->FreeMem(pXfbQueryData);
        }
    }

    return result;
}

// =====================================================================================================================
// Reset Pal query pool from CPU
void PalQueryPool::Reset(
    Device*     pDevice,
    uint32_t    startQuery,
    uint32_t    queryCount)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalQueryPool[deviceIdx] != nullptr)
        {
            m_pPalQueryPool[deviceIdx]->Reset(startQuery, queryCount, nullptr);
        }
    }
}

// =====================================================================================================================
// Creates a new query pool object (Timestamp query pool).
VkResult TimestampQueryPool::Create(
    Device*                      pDevice,
    const VkQueryPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    QueryPool**                  ppQueryPool)
{
    VK_ASSERT(pCreateInfo->queryType == VK_QUERY_TYPE_TIMESTAMP);

    VkResult result = VK_SUCCESS;

    // Parse create info
    uint32_t entryCount;

    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);

    entryCount = pCreateInfo->queryCount;

    // Allocate system memory
    size_t apiSize   = sizeof(TimestampQueryPool);
    size_t viewSize  = pDevice->GetProperties().descriptorSizes.bufferView;
    size_t totalSize = apiSize + (viewSize * pDevice->NumPalDevices());
    void*  pMemory   = nullptr;
    const uint32_t slotSize = pDevice->GetProperties().timestampQueryPoolSlotSize;

    if (result == VK_SUCCESS)
    {
        pMemory = pDevice->AllocApiObject(pAllocator, totalSize);

        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    TimestampQueryPool* pObject = nullptr;

    if ((result == VK_SUCCESS) && (entryCount > 0))
    {
        // Construct the final pool object
        pObject = VK_PLACEMENT_NEW(pMemory) TimestampQueryPool(
            pDevice,
            pCreateInfo->queryType,
            entryCount);

        result = pObject->Initialize(
            pMemory,
            apiSize,
            viewSize,
            entryCount,
            slotSize);
    }

    if (result == VK_SUCCESS)
    {
        *ppQueryPool = pObject;
    }
    else
    {
        // Call destructor
        Util::Destructor(pObject);

        pDevice->FreeApiObject(pAllocator, pMemory);
    }

    return result;
}

// =====================================================================================================================
VkResult QueryPoolWithStorageView::Initialize(
    void*          pMemory,
    size_t         apiSize,
    size_t         viewSize,
    uint32_t       entryCount,
    const uint32_t slotSize)
{
    VkResult result = VK_SUCCESS;

    const VkDeviceSize poolSize = entryCount * slotSize;

    InternalMemCreateInfo createInfo = {};

    createInfo.pal.size               = poolSize;
    createInfo.pal.alignment          = slotSize;
    createInfo.pal.priority           = Pal::GpuMemPriority::Normal;
    createInfo.flags.persistentMapped = true;

    uint32_t allocMask = m_pDevice->GetPalDeviceMask();

    const bool sharedAllocation = (m_pDevice->NumPalDevices() > 1);
    if (sharedAllocation == false)
    {
        createInfo.pal.heapCount = 3;
        createInfo.pal.heaps[0]  = Pal::GpuHeapLocal;
        createInfo.pal.heaps[1]  = Pal::GpuHeapGartCacheable;
        createInfo.pal.heaps[2]  = Pal::GpuHeapGartUswc;
    }
    else
    {
        createInfo.pal.heapCount = 1;
        createInfo.pal.heaps[0]  = Pal::GpuHeapGartCacheable;

        createInfo.pal.flags.shareable = 1;
        allocMask = 1 << DefaultMemoryInstanceIdx;
    }

    result = m_pDevice->MemMgr()->AllocGpuMem(
        createInfo,
        &m_internalMem,
        allocMask,
        VK_OBJECT_TYPE_QUERY_POOL,
        QueryPoolWithStorageView::IntValueFromHandle(QueryPoolWithStorageView::HandleFromObject(this)));

    if (result == VK_SUCCESS)
    {
        // Construct an untyped bufferView or SSBO (UAV) typed RG32 buffer view into the memory. This
        // will be used by compute shaders performing vkCmdCopyQueryPoolResults.
        void* pViewMem = Util::VoidPtrInc(pMemory, apiSize);

        if (entryCount > 0)
        {
            Pal::BufferViewInfo bufferViewInfo = {};

            bufferViewInfo.range = m_internalMem.Size();

            if (m_pDevice->UseStridedCopyQueryResults())
            {
                bufferViewInfo.swizzledFormat = Pal::UndefinedSwizzledFormat;
                bufferViewInfo.stride = 0;

                for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
                {
                    bufferViewInfo.gpuAddr = m_internalMem.GpuVirtAddr(deviceIdx);

                    m_pStorageView[deviceIdx] = Util::VoidPtrInc(pMemory, apiSize + (viewSize * deviceIdx));

                    m_pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &bufferViewInfo, m_pStorageView[deviceIdx]);
                }
            }
            else
            {
                Pal::SwizzledFormat QueryCopyFormat = {
                    Pal::ChNumFormat::X32Y32_Uint,
                    Pal::ChannelMapping{{{
                        Pal::ChannelSwizzle::X,
                        Pal::ChannelSwizzle::Y,
                        Pal::ChannelSwizzle::Zero,
                        Pal::ChannelSwizzle::Zero
                    }}},
                };

#if VKI_RAY_TRACING
                Pal::SwizzledFormat QueryCopyFormat128 = {
                    Pal::ChNumFormat::X32Y32Z32W32_Uint,
                    Pal::ChannelMapping{{{
                        Pal::ChannelSwizzle::X,
                        Pal::ChannelSwizzle::Y,
                        Pal::ChannelSwizzle::Z,
                        Pal::ChannelSwizzle::W
                    }}},
                };
#endif

                if ((GetQueryType() == VK_QUERY_TYPE_TIMESTAMP)
#if VKI_RAY_TRACING
                    || (GetQueryType() == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR)
#endif
                    )
                {
                    VK_ASSERT(slotSize == 8);
                }
#if VKI_RAY_TRACING
                else if (IsAccelerationStructureSerializationType(GetQueryType()))
                {
                    VK_ASSERT(slotSize == 16);
                }
#endif

                // Stride should be 8 if query type is VK_QUERY_TYPE_TIMESTAMP
#if VKI_RAY_TRACING
                // or VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR
                // or VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR.
#endif
                // BufferView is used just to query first 32 or 64 bits of the corresponding structure
#if VKI_RAY_TRACING
                // although VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR
                // and VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR query type
                // slotSize is 16.
#endif

#if VKI_RAY_TRACING
                if(IsAccelerationStructureSerializationType(GetQueryType()))
                {
                    bufferViewInfo.stride = 16;
                    bufferViewInfo.swizzledFormat = QueryCopyFormat128;
                }
                else
#endif
                {
                    // Compute shader expects rg32ui if not strided copy.
                    bufferViewInfo.stride = 8;
                    bufferViewInfo.swizzledFormat = QueryCopyFormat;
                }

                for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
                {
                    bufferViewInfo.gpuAddr = m_internalMem.GpuVirtAddr(deviceIdx);

                    m_pStorageView[deviceIdx] = Util::VoidPtrInc(pMemory, apiSize + (viewSize * deviceIdx));

                    m_pDevice->PalDevice(deviceIdx)->CreateTypedBufferViewSrds(1, &bufferViewInfo, m_pStorageView[deviceIdx]);
                }
            }
        }
        else
        {
            memset(pViewMem, 0, m_pDevice->GetProperties().descriptorSizes.bufferView);
        }
    }

    return result;
}

// =====================================================================================================================
// Destroy query pool object (Timestamp query pools)
VkResult TimestampQueryPool::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Free internal GPU memory allocation used by the object
    pDevice->MemMgr()->FreeGpuMem(&m_internalMem);

    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Get the results of a range of query slots (Timestamp query pools)
VkResult TimestampQueryPool::GetResults(
    Device*             pDevice,
    uint32_t            startQuery,
    uint32_t            queryCount,
    size_t              dataSize,
    void*               pData,
    VkDeviceSize        stride,
    VkQueryResultFlags  flags)
{
    VkResult result = VK_SUCCESS;

    if (queryCount > 0)
    {
        const uint64_t* pSrcData = nullptr;

        // Map timestamp memory
        m_internalMem.Map(DefaultDeviceIndex, (void**)(&pSrcData));

        // This map should never fail
        VK_ASSERT(pData != nullptr);

        // Determine availability for all query slots, optionally waiting for them to become available
        bool allReady = true;

        // Determine number of bytes written per query slot
        const size_t queryValueSize = ((flags & VK_QUERY_RESULT_64_BIT) != 0) ? sizeof(uint64_t) : sizeof(uint32_t);
        const size_t querySlotSize  = queryValueSize * (((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0) ? 2 : 1);

        // Although the spec says that dataSize has to be large enough to contain the result of each query, which sort
        // of sounds like it makes it redundant, clamp the maximum number of queries written to the given dataSize
        // and take account of the supplied stride, since it's harmless to do.
        queryCount = Util::Min(queryCount,
                static_cast<uint32_t>(dataSize / Util::Max(querySlotSize, static_cast<size_t>(stride))));

        // Write results of each query slot
        for (uint32_t dstSlot = 0; dstSlot < queryCount; ++dstSlot)
        {
            const uint32_t srcSlotOffset = (dstSlot + startQuery) * GetSlotSize();

            // Pointer to this slot's timestamp counter value
            volatile const uint64_t* pTimestamp =
                reinterpret_cast<const uint64_t*>(Util::VoidPtrInc(pSrcData, srcSlotOffset));

            // Test if the timestamp query is available
            uint64_t value = *pTimestamp;
            bool ready     = (value != TimestampNotReady);

            // Wait until the timestamp query has become available
            if ((flags & VK_QUERY_RESULT_WAIT_BIT) != 0)
            {
                while (!ready)
                {
                    value = *pTimestamp;
                    ready = (value != TimestampNotReady);
                }
            }

            // Get a pointer to the start of this slot's data
            void* pSlotData = Util::VoidPtrInc(pData, static_cast<size_t>(dstSlot * stride));

            // Write the timestamp value + availability (only write the value if the timestamp was ready,
            // and only write availability if it was requested)
            if ((flags & VK_QUERY_RESULT_64_BIT) != 0)
            {
                uint64_t* pSlot = reinterpret_cast<uint64_t*>(pSlotData);

                if (ready)
                {
                    pSlot[0] = value;
                }

                if ((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0)
                {
                    pSlot[1] = static_cast<uint64_t>(ready);
                }
            }
            else
            {
                uint32_t* pSlot = reinterpret_cast<uint32_t*>(pSlotData);

                if (ready)
                {
                    pSlot[0] = static_cast<uint32_t>(value); // Note: 32-bit results are allowed to wrap
                }

                if ((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0)
                {
                    pSlot[1] = static_cast<uint32_t>(ready);
                }
            }

            // Track whether all requested queries were ready
            allReady &= ready;
        }

        // If at least one query was not available, we need to return VK_NOT_READY
        if (allReady == false)
        {
            result = VK_NOT_READY;
        }

        m_internalMem.Unmap(DefaultDeviceIndex);
    }

    return result;
}

// =====================================================================================================================
// Reset timestamp query pool from CPU
void TimestampQueryPool::Reset(
    Device*     pDevice,
    uint32_t    startQuery,
    uint32_t    queryCount)
{
    if (startQuery < m_entryCount)
    {
        queryCount = Util::Min(queryCount, m_entryCount - startQuery);

        // Query pool size needs to be reset in qwords.
        const uint32_t queryDataSize = (m_slotSize * queryCount) / sizeof(uint64_t);
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            void* pMappedAddr = nullptr;
            if (m_internalMem.Map(deviceIdx, &pMappedAddr) == Pal::Result::Success)
            {
                uint64_t* pQueryData = static_cast<uint64_t*>(Util::VoidPtrInc(pMappedAddr,
                                                                               (m_slotSize * startQuery)));

                for (uint32_t idx = 0; idx < queryDataSize; idx++)
                {
                    pQueryData[idx] = TimestampNotReady;
                }

                if (pMappedAddr != nullptr)
                {
                    m_internalMem.Unmap(deviceIdx);
                }
            }
        }
    }
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Creates a new query pool object (AccelerationStructure query pool).
VkResult AccelerationStructureQueryPool::Create(
    Device*                      pDevice,
    const VkQueryPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    QueryPool**                  ppQueryPool)
{
    VK_ASSERT(IsAccelerationStructureQueryType(pCreateInfo->queryType));

    VkResult result = VK_SUCCESS;

    // Parse create info
    uint32_t entryCount = pCreateInfo->queryCount;

    // Allocate system memory
    size_t apiSize   = sizeof(AccelerationStructureQueryPool);
    size_t viewSize  = pDevice->GetProperties().descriptorSizes.bufferView;
    size_t totalSize = apiSize + (viewSize * pDevice->NumPalDevices());
    void*  pMemory   = nullptr;

    const uint32_t slotSize = IsAccelerationStructureSerializationType(pCreateInfo->queryType) ?
                                sizeof(GpuRt::AccelStructPostBuildInfoSerializationDesc) :
                                sizeof(GpuRt::AccelStructPostBuildInfoCompactedSizeDesc);

    if (result == VK_SUCCESS)
    {
        pMemory = pDevice->AllocApiObject(pAllocator, totalSize);

        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    AccelerationStructureQueryPool* pObject = nullptr;

    if ((result == VK_SUCCESS) && (entryCount > 0))
    {
        pObject = VK_PLACEMENT_NEW(pMemory) AccelerationStructureQueryPool(
                pDevice,
                pCreateInfo->queryType,
                entryCount,
                slotSize);

        result = pObject->Initialize(pMemory,
                                     apiSize,
                                     viewSize,
                                     entryCount,
                                     slotSize);
    }

    if (result == VK_SUCCESS)
    {
        *ppQueryPool = pObject;
    }
    else
    {
        // Call destructor
        Util::Destructor(pObject);

        pDevice->FreeApiObject(pAllocator, pMemory);
    }

    return result;
}

// =====================================================================================================================
// Destroy query pool object (AccelerationStructure query pools)
VkResult AccelerationStructureQueryPool::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Free internal GPU memory allocation used by the object
    pDevice->MemMgr()->FreeGpuMem(&m_internalMem);

    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Get the results of a range of query slots (Acceleration Structure Query Pool) based on queryType
uint64_t AccelerationStructureQueryPool::GetAccelerationStructureQueryResults(
    VkQueryType     queryType,
    const uint64_t* pSrcData,
    uint32_t        srcSlotOffset) const
{
    uint64_t value = AccelerationStructureQueryNotReady;

    switch (static_cast<uint32_t>(m_queryType))
    {
        case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR:
        {
            volatile const GpuRt::AccelStructPostBuildInfoSerializationDesc*
                pAccelerationStructureSerializationDesc =
                reinterpret_cast<const GpuRt::AccelStructPostBuildInfoSerializationDesc*>
                (Util::VoidPtrInc(pSrcData, srcSlotOffset));

            value = pAccelerationStructureSerializationDesc->serializedSizeInBytes;
            break;
        }
        case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR:
        {
            volatile const GpuRt::AccelStructPostBuildInfoCompactedSizeDesc*
                pAccelStructPostBuildInfoCompactedSizeDesc =
                reinterpret_cast<const GpuRt::AccelStructPostBuildInfoCompactedSizeDesc*>
                (Util::VoidPtrInc(pSrcData, srcSlotOffset));

            value = pAccelStructPostBuildInfoCompactedSizeDesc->compactedSizeInBytes;
            break;
        }
        case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR:
        {
            // Pointer to this slot's AccelStructPostBuildInfoSerializationDesc counter value
            volatile const GpuRt::AccelStructPostBuildInfoSerializationDesc*
                pAccelerationStructureSerializationDesc =
                reinterpret_cast<const GpuRt::AccelStructPostBuildInfoSerializationDesc*>
                (Util::VoidPtrInc(pSrcData, srcSlotOffset));

            value = pAccelerationStructureSerializationDesc->numBottomLevelAccelerationStructurePointers;
            break;
        }
        case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR:
        {
            volatile const GpuRt::AccelStructPostBuildInfoCurrentSizeDesc*
                pAccelStructPostBuildInfoCurrentSizeDesc =
                reinterpret_cast<const GpuRt::AccelStructPostBuildInfoCurrentSizeDesc*>
                (Util::VoidPtrInc(pSrcData, srcSlotOffset));

            value = pAccelStructPostBuildInfoCurrentSizeDesc->currentSizeInBytes;
            break;
        }
        default:
            VK_NEVER_CALLED();
            break;
    }

    return value;
}

// =====================================================================================================================
// Get the results of a range of query slots (Acceleration Structure Query Pool)
// Each slot should will be a AccelStructPostBuildInfoSerializationDesc
VkResult AccelerationStructureQueryPool::GetResults(
    Device*             pDevice,
    uint32_t            startQuery,
    uint32_t            queryCount,
    size_t              dataSize,
    void*               pData,
    VkDeviceSize        stride,
    VkQueryResultFlags  flags)
{
    VkResult result = VK_SUCCESS;

     if (queryCount > 0)
     {
         const uint64_t* pSrcData = nullptr;

         // Map acceleration structure memory
         m_internalMem.Map(DefaultDeviceIndex, (void**)(&pSrcData));

         // This map should never fail
         VK_ASSERT(pData != nullptr);

         // Determine availability for all query slots, optionally waiting for them to become available
         bool allReady = true;

         // Determine number of bytes written per query slot
         const size_t queryValueSize = ((flags & VK_QUERY_RESULT_64_BIT) != 0) ? sizeof(uint64_t) : sizeof(uint32_t);
         const size_t querySlotSize = queryValueSize * (((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0) ? 2 : 1);

         // Although the spec says that dataSize has to be large enough to contain the result of each query, which sort
         // of sounds like it makes it redundant, clamp the maximum number of queries written to the given dataSize
         // and take account of the supplied stride, since it's harmless to do.
         queryCount = Util::Min(queryCount,
             static_cast<uint32_t>(dataSize / Util::Max(querySlotSize, static_cast<size_t>(stride))));

         // Write results of each query slot
         for (uint32_t dstSlot = 0; dstSlot < queryCount; ++dstSlot)
         {
             const uint32_t srcSlotOffset = (dstSlot + startQuery) * m_slotSize;

            uint64_t value = GetAccelerationStructureQueryResults(
                                m_queryType,
                                pSrcData,
                                srcSlotOffset);

             bool ready = (value != AccelerationStructureQueryNotReady);

             // Wait until the acceleration structure query is available
             if ((flags & VK_QUERY_RESULT_WAIT_BIT) != 0)
             {
                 while (!ready)
                 {
                     Util::SleepMs(0u);

                     value = GetAccelerationStructureQueryResults(
                         m_queryType,
                         pSrcData,
                         srcSlotOffset);

                     ready = (value != AccelerationStructureQueryNotReady);
                 }
             }

             // Get a pointer to the start of this slot's data
             void* pSlotData = Util::VoidPtrInc(pData, static_cast<size_t>(dstSlot * stride));

             // Write the AccelStructPostBuildInfoSerializationDesc (only write the value if ready,
             // and only write availability if it was requested)
             if ((flags & VK_QUERY_RESULT_64_BIT) != 0)
             {
                 uint64_t* pSlot = reinterpret_cast<uint64_t*>(pSlotData);

                 if (ready)
                 {
                     pSlot[0] = value;
                 }

                 if ((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0)
                 {
                     pSlot[1] = static_cast<uint64_t>(ready);
                 }
             }
             else
             {
                 uint32_t* pSlot = reinterpret_cast<uint32_t*>(pSlotData);

                 if (ready)
                 {
                     pSlot[0] = static_cast<uint32_t>(value); // Note: 32-bit results are allowed to wrap
                 }

                 if ((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0)
                 {
                     pSlot[1] = static_cast<uint32_t>(ready);
                 }
             }

             // Track whether all requested queries were ready
             allReady = allReady && ready;
         }

         // If at least one query was not available, we need to return VK_NOT_READY
         if (allReady == false)
         {
             result = VK_NOT_READY;
         }

         m_internalMem.Unmap(DefaultDeviceIndex);
     }

    return result;
}

// =====================================================================================================================
// Reset AccelerationStructure query pool from CPU
void AccelerationStructureQueryPool::Reset(
    Device*     pDevice,
    uint32_t    startQuery,
    uint32_t    queryCount)
{
     if (startQuery < m_entryCount)
     {
         queryCount = Util::Min(queryCount, m_entryCount - startQuery);

         // Query pool size needs to be reset in qwords.
         const uint32_t queryDataSize = (m_slotSize * queryCount) / sizeof(uint64_t);
         for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
         {
             void* pMappedAddr = nullptr;
             if (m_internalMem.Map(deviceIdx, &pMappedAddr) == Pal::Result::Success)
             {
                 uint64_t* pQueryData = static_cast<uint64_t*>(Util::VoidPtrInc(pMappedAddr,
                                                                               (m_slotSize * startQuery)));

                 for (uint32_t idx = 0; idx < queryDataSize; idx++)
                 {
                     pQueryData[idx] = AccelerationStructureQueryNotReady;
                 }

                 if (pMappedAddr != nullptr)
                 {
                     m_internalMem.Unmap(deviceIdx);
                 }
             }
         }
     }
}
#endif

namespace entry
{
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetQueryPoolResults(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount,
    size_t                                      dataSize,
    void*                                       pData,
    VkDeviceSize                                stride,
    VkQueryResultFlags                          flags)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    return QueryPool::ObjectFromHandle(queryPool)->GetResults(
        pDevice,
        firstQuery,
        queryCount,
        dataSize,
        pData,
        stride,
        flags);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyQueryPool(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    const VkAllocationCallbacks*                pAllocator)
{
    if (queryPool != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        QueryPool::ObjectFromHandle(queryPool)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkResetQueryPool(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    QueryPool::ObjectFromHandle(queryPool)->Reset(pDevice, firstQuery, queryCount);
}

} // namespace entry

} // namespace vk
