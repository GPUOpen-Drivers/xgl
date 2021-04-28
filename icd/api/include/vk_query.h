/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
class DispatchableQueryPool;
class PalQueryPool;
class TimestampQueryPool;

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

    VK_INLINE VkQueryType GetQueryType() const
    {
        return m_queryType;
    }

    VK_INLINE const PalQueryPool* AsPalQueryPool() const;

    VK_INLINE const TimestampQueryPool* AsTimestampQueryPool() const;

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
class PalQueryPool : public QueryPool
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

// =====================================================================================================================
// Query pool class for VK_QUERY_TYPE_TIMESTAMP query pools
class TimestampQueryPool : public QueryPool
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

    VK_INLINE const InternalMemory& GetMemory() const
        { return m_internalMem; }

    VK_INLINE Pal::gpusize GetSlotOffset(uint32_t query) const
    {
        VK_ASSERT(query < m_entryCount);

        return m_internalMem.Offset() + query * m_slotSize;
    }

    VK_INLINE uint32_t GetSlotSize() const
        { return m_slotSize; }

    VK_INLINE const Pal::IGpuMemory& PalMemory(uint32_t deviceIdx) const
        { return *m_internalMem.PalMemory(deviceIdx); }

    VK_INLINE const void* GetStorageView(uint32_t deviceIdx) const
        { return m_pStorageView[deviceIdx]; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(TimestampQueryPool);

    TimestampQueryPool(
        Device* pDevice,
        VkQueryType           queryType,
        uint32_t              entryCount)
        :
        QueryPool(pDevice, queryType),
        m_entryCount(entryCount),
        m_slotSize(pDevice->GetProperties().timestampQueryPoolSlotSize),
        m_internalMem()
    {
    }

    VkResult Initialize(
        void*          pMemory,
        size_t         apiSize,
        size_t         viewSize,
        uint32_t       entryCount,
        const uint32_t slotSize);

    const uint32_t    m_entryCount;
    const uint32_t    m_slotSize;
    InternalMemory    m_internalMem;
    void*             m_pStorageView[MaxPalDevices];
};

// =====================================================================================================================
VK_INLINE const PalQueryPool* QueryPool::AsPalQueryPool() const
{
    VK_ASSERT(m_queryType != VK_QUERY_TYPE_TIMESTAMP);

    return static_cast<const PalQueryPool*>(this);
}

// =====================================================================================================================
VK_INLINE const TimestampQueryPool* QueryPool::AsTimestampQueryPool() const
{
    VK_ASSERT(m_queryType == VK_QUERY_TYPE_TIMESTAMP);

    return static_cast<const TimestampQueryPool*>(this);
}

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
