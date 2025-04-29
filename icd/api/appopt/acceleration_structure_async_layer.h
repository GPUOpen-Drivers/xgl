/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef __ACCELERATION_STRUCTURE_ASYNC_LAYER_H__
#define __ACCELERATION_STRUCTURE_ASYNC_LAYER_H__

#pragma once

#include "opt_layer.h"
#include "palHashSet.h"
#include "palMutex.h"
#include "palOptional.h"
#include "palQueueSemaphore.h"
#include "vk_cmdbuffer.h"

namespace vk
{
// =====================================================================================================================
class AccelStructAsyncBuildLayer final : public OptLayer
{
public:
    virtual ~AccelStructAsyncBuildLayer() {}

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

    static VkResult CreateLayer(Device* pDevice, AccelStructAsyncBuildLayer** ppLayer);
    void Destroy();

    Instance* VkInstance() { return m_pInstance; }

    uint32_t AddBuildBuffer(VkCommandBuffer buffer);
    Util::Optional<uint32_t> GetBuildBufferId(VkCommandBuffer buffer);
    void AddDependentBuffer(VkCommandBuffer buffer);
    void RemoveDependentBuffer(VkCommandBuffer buffer);
    bool IsDependentBuffer(VkCommandBuffer buffer);

    VkCommandBuffer GetAsyncBuildBuffer(uint32_t id);

    template<typename SubmitInfoType>
    VkResult SubmitBuffers(VkQueue queue, uint32_t submitCount, const SubmitInfoType* pSubmits, VkFence fence);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(AccelStructAsyncBuildLayer);

    AccelStructAsyncBuildLayer(Device* pDevice);
    VkResult Init(Device* pDevice);

    template<typename SubmitInfoType>
    VkResult SubmitBuildBuffer(VkQueue queue, uint32_t submitCount, SubmitInfoType* pSubmits, VkFence fence,
                               uint32_t buildSubmitIndex, uint32_t buildCommandBufferIndex, uint32_t buildResourceIndex);
    template<typename SubmitInfoType>
    VkResult SubmitDependentBuffer(VkQueue queue, uint32_t submitCount, SubmitInfoType* pSubmits, VkFence fence,
                               uint32_t dependentSubmitIndex, uint32_t dependentCommandBufferIndex);
    template<typename SubmitInfoType>
    VkResult SubmitAsyncBuffer(VkQueue queue, VkCommandBuffer asyncBuffer, VkFence fence);

    static constexpr uint32_t MaxBuildsPerFrame = 2;
    static constexpr uint32_t MaxFramesInFlight = 3;
    static constexpr uint32_t MaxNumBuilds = MaxBuildsPerFrame * MaxFramesInFlight;

    uint32_t m_currentBuildCounter;  // Keeps track of which CommandBuffer/Semaphore to use for this build
    uint64_t m_currentTimelineValue; // Keeps track of the timeline semaphore value
    Util::Mutex m_buildMutex;

    VkCommandBuffer m_buildCommandBuffers[MaxNumBuilds];
    Util::HashSet<VkCommandBuffer, PalAllocator> m_dependentCommandBuffers;

    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffers[MaxNumBuilds];

    VkQueue m_asyncComputeQueue;
    bool    m_buildsInFlight; // Whether there are any builds in flight on the async compute queue.

    Pal::IQueueSemaphore* m_semaphore;

    Device* m_pDevice;
    Instance* m_pInstance;
};

}; // namespace vk

#endif /* __ACCELERATION_STRUCTURE_ASYNC_LAYER_H__ */
