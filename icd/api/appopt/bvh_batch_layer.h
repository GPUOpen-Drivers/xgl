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
* @file  bvh_batch_layer.h
* @brief Declaration of bvh batch layer
***********************************************************************************************************************
*/

#if VKI_RAY_TRACING
#ifndef __BVH_BATCH_LAYER_H
#define __BVH_BATCH_LAYER_H

#pragma once
#include "opt_layer.h"
#include "vk_alloccb.h"
#include "vk_cmdbuffer.h"
#include "palVector.h"
#include "palMutex.h"
#include "palFile.h"

namespace vk
{

enum class BvhBatchType : uint32
{
    Undefined,
    Direct,
    Indirect
};

class BvhBatchLayer;

class BvhBatchState
{
public:
    BvhBatchState(BvhBatchLayer* pLayer);
    ~BvhBatchState();

    void Log(const char* pFormat, ...);

    template<BvhBatchType batchType>
    bool EnqueueBvhBuild(
        CmdBuffer*                                             pCmdBuffer,
        uint32_t                                               infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR*     pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos,
        const VkDeviceAddress*                                 pIndirectDeviceAddresses,
        const uint32_t*                                        pIndirectStrides,
        const uint32_t* const*                                 ppMaxPrimitiveCounts);

    void Reset();
    void Flush();
    void TryFlush(VkFlags64 srcStageMask);
    void TryFlush(uint32_t depInfoCount, const VkDependencyInfo* pDependencyInfos);
    void DestroyState();

private:
    template<BvhBatchType batchType>
    size_t GetHardCopyMemSize(
        uint32_t                                           infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR* pInfos);

    template<BvhBatchType batchType>
    void HardCopyBuildInfos(
        uint32_t                                               infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR*     pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos,
        const VkDeviceAddress*                                 pIndirectDeviceAddresses,
        const uint32_t*                                        pIndirectStrides,
        const uint32_t* const*                                 ppMaxPrimitiveCounts,
        void*                                                  pMem,
        size_t                                                 memSize);

    typedef Util::Vector<VkAccelerationStructureBuildGeometryInfoKHR, 16, PalAllocator> GeometryInfoList;
    typedef Util::Vector<void*, 16, PalAllocator> VoidPtrList;
    typedef Util::Vector<VkDeviceAddress, 16, PalAllocator> VirtAddrList;
    typedef Util::Vector<uint32_t, 16, PalAllocator> StrideList;

    BvhBatchType     m_type;
    CmdBuffer*       m_pCmdBuffer;
    BvhBatchLayer*   m_pLayer;
    GeometryInfoList m_geomInfos;
    VoidPtrList      m_rangeInfosOrMaxPrimCounts;
    VirtAddrList     m_indirectVirtAddrs;
    StrideList       m_indirectStrides;
    uint32_t         m_infoCount;
    VoidPtrList      m_allocations;
};

class BvhBatchLayer final : public OptLayer
{
public:
    ~BvhBatchLayer();

    static VkResult CreateLayer(Device* pDevice, BvhBatchLayer** ppLayer);
    void DestroyLayer();

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

    void VLog(const char* pFormat, va_list argList);

    BvhBatchState* CreateState(CmdBuffer* pCmdBuffer);
    bool PushEmptyState(BvhBatchState* pState);
    BvhBatchState* PopEmptyState();

    Instance* VkInstance() { return m_pInstance; }
    bool LoggingEnabled() { return m_logFile.IsOpen(); }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(BvhBatchLayer);

    BvhBatchLayer(Device* pDevice);

    VkResult Init(Device* pDevice);

    Instance*      m_pInstance;
    Util::Mutex    m_mutex;

    uint32_t       m_emptyStateCount;
    BvhBatchState* m_pEmptyStateStack[16];

    Util::File     m_logFile;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define BVH_BATCH_LAYER_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::bvhBatchLayer::func_name;

#define BVH_BATCH_LAYER_OVERRIDE_ENTRY(entry_name) BVH_BATCH_LAYER_OVERRIDE_ALIAS(entry_name, entry_name)

#define BVH_BATCH_LAYER_CALL_NEXT_LAYER(entry_name) \
    pLayer->GetNextLayer()->GetEntryPoints().entry_name

} // namespace vk

#endif /* __BVH_BATCH_LAYER_H */
#endif /* VKI_RAY_TRACING */
