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
 * @file  vk_query.h
 * @brief Declaration of classes related to Vulkan query pool.
 ***********************************************************************************************************************
 */

#ifndef __VK_QUERY_H__
#define __VK_QUERY_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/internal_mem_mgr.h"

#include "palQueryPool.h"

namespace vk
{

class CmdBuffer;
class Device;
class ApiQueryPool;
class PalQueryPool;
class QueryPoolWithStorageView;
class TimestampQueryPool;
#if VKI_RAY_TRACING
class AccelerationStructureQueryPool;
#endif

// =====================================================================================================================
// Base class for all Vulkan query pools.  VkQueryPool handles map to this base class pointer.
class QueryPool : public NonDispatchable<VkQueryPool, QueryPool>
{
public:
    typedef VkQueryPool ApiType;

    static VkResult Create(
        Device*                         pDevice,
        const VkQueryPoolCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkQueryPool*                    pQueryPool);

    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) = 0;

    virtual VkResult GetResults(
        Device*             pDevice,
        uint32_t            startQuery,
        uint32_t            queryCount,
        size_t              dataSize,
        void*               pData,
        VkDeviceSize        stride,
        VkQueryResultFlags  flags) = 0;

    virtual void Reset(
        Device*     pDevice,
        uint32_t    startQuery,
        uint32_t    queryCount) = 0;

    VkQueryType GetQueryType() const
    {
        return m_queryType;
    }

    inline const PalQueryPool* AsPalQueryPool() const;
    inline const QueryPoolWithStorageView* AsQueryPoolWithStorageView() const;
    inline const TimestampQueryPool* AsTimestampQueryPool() const;

#if VKI_RAY_TRACING
    inline const AccelerationStructureQueryPool* AsAccelerationStructureQueryPool() const;
#endif

protected:
    QueryPool(
        Device*     pDevice,
        VkQueryType queryType)
        :
        m_pDevice(pDevice),
        m_queryType(queryType)
    {
    }

    virtual ~QueryPool() { }

    Device* const     m_pDevice;
    const VkQueryType m_queryType;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(QueryPool);
};

// =====================================================================================================================
// Vulkan query pools that are not VK_QUERY_TYPE_TIMESTAMP pools
class PalQueryPool final : public QueryPool
{
public:
    VK_FORCEINLINE Pal::QueryType PalQueryType() const
    {
        return m_palQueryType;
    }

    VK_FORCEINLINE Pal::IQueryPool* PalPool(uint32_t deviceIdx) const
    {
        return m_pPalQueryPool[deviceIdx];
    }

    static VkResult Create(
        Device*                         pDevice,
        const VkQueryPoolCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        QueryPool**                     ppQueryPool);

    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    virtual VkResult GetResults(
        Device*             pDevice,
        uint32_t            startQuery,
        uint32_t            queryCount,
        size_t              dataSize,
        void*               pData,
        VkDeviceSize        stride,
        VkQueryResultFlags  flags) override;

    virtual void Reset(
        Device*     pDevice,
        uint32_t    startQuery,
        uint32_t    queryCount) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PalQueryPool);

    PalQueryPool(
        Device*           pDevice,
        VkQueryType       queryType,
        Pal::QueryType    palQueryType,
        Pal::IQueryPool** ppPalQueryPools)
        :
        QueryPool(pDevice, queryType),
        m_palQueryType(palQueryType),
        m_internalMem()
    {
        memcpy(m_pPalQueryPool, ppPalQueryPools, sizeof(m_pPalQueryPool));
    }

    VkResult Initialize();

    const Pal::QueryType    m_palQueryType;
    Pal::IQueryPool*        m_pPalQueryPool[MaxPalDevices];
    InternalMemory          m_internalMem;
};

class QueryPoolWithStorageView : public QueryPool
{
public:
    const void* GetStorageView(uint32_t deviceIdx) const
        { return m_pStorageView[deviceIdx]; }

protected:
        PAL_DISALLOW_COPY_AND_ASSIGN(QueryPoolWithStorageView);

        VkResult Initialize(
            void*           pMemory,
            size_t          apiSize,
            size_t          viewSize,
            uint32_t        entryCount,
            const uint32_t  slotSize);

        const uint32_t m_entryCount;
        const uint32_t m_slotSize;
        InternalMemory m_internalMem;

        QueryPoolWithStorageView(
            Device*           pDevice,
            VkQueryType       queryType,
            uint32_t          entryCount,
            uint32_t          slotSize)
            :
            QueryPool(pDevice, queryType),
            m_entryCount(entryCount),
            m_slotSize(slotSize),
            m_internalMem()
        {
        }

private:
        void* m_pStorageView[MaxPalDevices];
};

#if VKI_RAY_TRACING
// =====================================================================================================================
inline bool IsAccelerationStructureQueryType(VkQueryType queryType)
{
    return ((queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR) ||
            (queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR) ||
            (queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR) ||
            (queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR));
}

// =====================================================================================================================
inline bool IsAccelerationStructureSerializationType(VkQueryType queryType)
{
    return ((queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR) ||
            (queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR));
}

// =====================================================================================================================
// Query pool class for VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR query pools
class AccelerationStructureQueryPool final : public QueryPoolWithStorageView
{
public:
    static constexpr uint32_t AccelerationStructureQueryNotReady = UINT32_MAX;

    VK_FORCEINLINE Pal::gpusize GpuVirtAddr(uint32_t deviceIdx) const
    {
        return m_internalMem.GpuVirtAddr(deviceIdx);
    }

    static VkResult Create(
        Device*                         pDevice,
        const VkQueryPoolCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        QueryPool**                     ppQueryPool);

    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    virtual VkResult GetResults(
        Device*             pDevice,
        uint32_t            startQuery,
        uint32_t            queryCount,
        size_t              dataSize,
        void*               pData,
        VkDeviceSize        stride,
        VkQueryResultFlags  flags) override;

    virtual void Reset(
        Device*     pDevice,
        uint32_t    startQuery,
        uint32_t    queryCount) override;

    uint32_t GetSlotSize() const
    {
        return m_slotSize;
    }

    uint64_t GetAccelerationStructureQueryResults(
        VkQueryType     queryType,
        const uint64_t* pSrcData,
        uint32_t        srcSlotOffset) const;

    Pal::gpusize GetSlotOffset(uint32_t query) const
    {
        VK_ASSERT(query < m_entryCount);

        return m_internalMem.Offset() + query * m_slotSize;
    }

    const Pal::IGpuMemory& PalMemory(uint32_t deviceIdx) const
    {
        return *m_internalMem.PalMemory(deviceIdx);
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(AccelerationStructureQueryPool);

    AccelerationStructureQueryPool(
        Device*           pDevice,
        VkQueryType       queryType,
        uint32_t          entryCount,
        uint32_t          slotSize)
        :
        QueryPoolWithStorageView(pDevice, queryType, entryCount, slotSize)
    {
    }
};
#endif

// =====================================================================================================================
// Query pool class for VK_QUERY_TYPE_TIMESTAMP query pools
class TimestampQueryPool final : public QueryPoolWithStorageView
{
public:
    static constexpr uint32_t TimestampNotReadyChunk = UINT32_MAX;
    // +------------------------+------------------------+
    // | TimestampNotReadyChunk | TimestampNotReadyChunk |
    // |------------------------+------------------------|
    // |                TimestampNotReady                |
    // +-------------------------------------------------+
    static constexpr uint64_t TimestampNotReady = (uint64_t(TimestampNotReadyChunk) << 32) + TimestampNotReadyChunk;

    static VkResult Create(
        Device*                         pDevice,
        const VkQueryPoolCreateInfo*    pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        QueryPool**                     ppQueryPool);

    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    virtual VkResult GetResults(
        Device*             pDevice,
        uint32_t            startQuery,
        uint32_t            queryCount,
        size_t              dataSize,
        void*               pData,
        VkDeviceSize        stride,
        VkQueryResultFlags  flags) override;

    virtual void Reset(
        Device*     pDevice,
        uint32_t    startQuery,
        uint32_t    queryCount) override;

    const InternalMemory& GetMemory() const
        { return m_internalMem; }

    Pal::gpusize GetSlotOffset(uint32_t query) const
    {
        VK_ASSERT(query < m_entryCount);

        return m_internalMem.Offset() + query * m_slotSize;
    }

    uint32_t GetSlotSize() const
        { return m_slotSize; }

    const Pal::IGpuMemory& PalMemory(uint32_t deviceIdx) const
        { return *m_internalMem.PalMemory(deviceIdx); }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(TimestampQueryPool);

    TimestampQueryPool(
        Device* pDevice,
        VkQueryType           queryType,
        uint32_t              entryCount)
        :
        QueryPoolWithStorageView(pDevice, queryType, entryCount, pDevice->GetProperties().timestampQueryPoolSlotSize)
    {
    }
};

// =====================================================================================================================
inline const PalQueryPool* QueryPool::AsPalQueryPool() const
{
    VK_ASSERT(m_queryType != VK_QUERY_TYPE_TIMESTAMP);

#if VKI_RAY_TRACING
    VK_ASSERT(IsAccelerationStructureQueryType(m_queryType) == false);
#endif

    return static_cast<const PalQueryPool*>(this);
}

// =====================================================================================================================
inline const TimestampQueryPool* QueryPool::AsTimestampQueryPool() const
{
    VK_ASSERT(m_queryType == VK_QUERY_TYPE_TIMESTAMP);

    return static_cast<const TimestampQueryPool*>(this);
}

inline const QueryPoolWithStorageView* QueryPool::AsQueryPoolWithStorageView() const
{
    if ((m_queryType != VK_QUERY_TYPE_TIMESTAMP)
#if VKI_RAY_TRACING
         && (IsAccelerationStructureQueryType(m_queryType) == false)
#endif
       )
        {
            VK_ASSERT(false);
        }

    return static_cast<const QueryPoolWithStorageView*>(this);
}

#if VKI_RAY_TRACING
// =====================================================================================================================
inline const AccelerationStructureQueryPool* QueryPool::AsAccelerationStructureQueryPool() const
{
    VK_ASSERT(IsAccelerationStructureQueryType(m_queryType));

    return static_cast<const AccelerationStructureQueryPool*>(this);
}
#endif

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkGetQueryPoolResults(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount,
    size_t                                      dataSize,
    void*                                       pData,
    VkDeviceSize                                stride,
    VkQueryResultFlags                          flags);

VKAPI_ATTR void VKAPI_CALL vkDestroyQueryPool(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkResetQueryPool(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount);

} // namespace entry

} // namespace vk

#endif
