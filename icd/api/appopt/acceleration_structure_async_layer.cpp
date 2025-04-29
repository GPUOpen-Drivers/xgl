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
#include "acceleration_structure_async_layer.h"

#include "raytrace/ray_tracing_device.h"
#include "raytrace/vk_acceleration_structure.h"
#include "vk_conv.h"
#include "vk_queue.h"
#include "vk_semaphore.h"
#include "palVectorImpl.h"
#include "palHashSetImpl.h"

namespace vk
{
// =====================================================================================================================
VkResult AccelStructAsyncBuildLayer::CreateLayer(
    Device* pDevice,
    AccelStructAsyncBuildLayer** ppLayer)
{
    VkResult result = VK_SUCCESS;
    AccelStructAsyncBuildLayer* pLayer = nullptr;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    void* pMem = pDevice->VkInstance()->AllocMem(sizeof(AccelStructAsyncBuildLayer),
                                                 VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pMem != nullptr)
    {
        pLayer = VK_PLACEMENT_NEW(pMem) AccelStructAsyncBuildLayer(pDevice);

        result = pLayer->Init(pDevice);

        if (result != VK_SUCCESS)
        {
            pLayer->Destroy();
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        *ppLayer = pLayer;
    }

    return result;
}

// =====================================================================================================================
AccelStructAsyncBuildLayer::AccelStructAsyncBuildLayer(
    Device* pDevice)
    :
    m_currentBuildCounter(0),
    m_currentTimelineValue(0),
    m_buildCommandBuffers(),
    m_dependentCommandBuffers(MaxNumBuilds * 2, pDevice->VkInstance()->GetPrivateAllocator()),
    m_commandPool(VK_NULL_HANDLE),
    m_commandBuffers(),
    m_asyncComputeQueue(VK_NULL_HANDLE),
    m_buildsInFlight(false),
    m_semaphore(nullptr),
    m_pDevice(pDevice),
    m_pInstance(pDevice->VkInstance())
{
}

// =====================================================================================================================
VkResult AccelStructAsyncBuildLayer::Init(
    Device* pDevice)
{
    VkResult result = VK_SUCCESS;
    uint32_t queueCount = Queue::MaxQueueFamilies;
    VkQueueFamilyProperties queueProps[Queue::MaxQueueFamilies] = {};

    uint32_t queueFamilyIndex = queueCount;
    result = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyProperties(&queueCount, queueProps);

    VK_ASSERT(result == VK_SUCCESS);
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        if (((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) &&
            ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
        {
            queueFamilyIndex = i;
            break;
        }
    }

    result = Queue::Create(pDevice, m_pInstance->GetAllocCallbacks(), 0, queueFamilyIndex,
                           0, VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT, 0, true, &m_asyncComputeQueue);

    if (result == VK_SUCCESS)
    {
        const VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndex
        };
        result = pDevice->CreateCommandPool(&commandPoolCreateInfo, m_pInstance->GetAllocCallbacks(), &m_commandPool);
    }

    if (result == VK_SUCCESS)
    {
        const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = MaxNumBuilds
        };

        result = pDevice->AllocateCommandBuffers(&commandBufferAllocateInfo, m_commandBuffers);
    }

    if (result == VK_SUCCESS)
    {
        Pal::QueueSemaphoreCreateInfo semaphoreCreateInfo {};
        semaphoreCreateInfo.flags.timeline = 1;

        Pal::Result palResult;
        const size_t palSemaphoreSize = pDevice->PalDevice(DefaultDeviceIndex)->GetQueueSemaphoreSize(
            semaphoreCreateInfo, &palResult);

        result = PalToVkResult(palResult);
        if (result == VK_SUCCESS)
        {
            void* pSemaphoreMem = pDevice->VkInstance()->AllocMem(
                palSemaphoreSize,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

            if (pSemaphoreMem != nullptr)
            {
                result = PalToVkResult(pDevice->PalDevice(DefaultDeviceIndex)->CreateQueueSemaphore(
                    semaphoreCreateInfo, pSemaphoreMem, &m_semaphore));
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }
    return result;
}

// =====================================================================================================================
void AccelStructAsyncBuildLayer::Destroy()
{
    if (m_semaphore != nullptr)
    {
        m_semaphore->Destroy();
        m_pInstance->FreeMem(m_semaphore);
    }
    for (uint32_t i = 0; i < MaxNumBuilds; ++i)
    {
        if (m_commandBuffers[i] != VK_NULL_HANDLE)
        {
            ApiCmdBuffer::ObjectFromHandle(m_commandBuffers[i])->Destroy();
        }
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        CmdPool::ObjectFromHandle(m_commandPool)->Destroy(m_pDevice, m_pInstance->GetAllocCallbacks());
    }
    if (m_asyncComputeQueue != VK_NULL_HANDLE)
    {
        ApiQueue::ObjectFromHandle(m_asyncComputeQueue)->Destroy(m_pDevice, m_pInstance->GetAllocCallbacks());
    }
    Util::Destructor(this);
    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
// Register the commandBuffer as a command buffer that contained a TLAS build
// Sets up a new async queue command buffer, unless the buffer had already been set up
uint32_t AccelStructAsyncBuildLayer::AddBuildBuffer(
    VkCommandBuffer commandBuffer)
{
    for (uint32_t i = 0; i < MaxNumBuilds; ++i)
    {
        if (m_buildCommandBuffers[i] == commandBuffer)
        {
            return i;
        }
    }

    uint32_t buildCounter = 0;
    {
        Util::MutexAuto lock(&m_buildMutex);
        m_currentBuildCounter = (m_currentBuildCounter + 1) % uint32_t(MaxNumBuilds);
        buildCounter = m_currentBuildCounter;
    }
    m_buildCommandBuffers[buildCounter] = commandBuffer;

    CmdBuffer* pAsyncCommandBuffer = ApiCmdBuffer::ObjectFromHandle(m_commandBuffers[buildCounter]);
    pAsyncCommandBuffer->Reset({});
    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    pAsyncCommandBuffer->Begin(&beginInfo);

    return buildCounter;
}

// =====================================================================================================================
// Register the commandBuffer as a command buffer that uses a TLAS
void AccelStructAsyncBuildLayer::AddDependentBuffer(
    VkCommandBuffer commandBuffer)
{
    Util::MutexAuto lock(&m_buildMutex);
    m_dependentCommandBuffers.Insert(commandBuffer);
}

// =====================================================================================================================
// Unregister the commandBuffer as a command buffer that uses a TLAS (after submission)
void AccelStructAsyncBuildLayer::RemoveDependentBuffer(
    VkCommandBuffer commandBuffer)
{
    m_dependentCommandBuffers.Erase(commandBuffer);
}

// =====================================================================================================================
// Returns the resource index of the command buffer, if it is a build buffer
Util::Optional<uint32_t> AccelStructAsyncBuildLayer::GetBuildBufferId(
    VkCommandBuffer commandBuffer)
{
    Util::Optional<uint32_t> result;
    for (uint32_t i = 0; i < MaxNumBuilds; ++i)
    {
        if (m_buildCommandBuffers[i] == commandBuffer)
        {
            result = i;
        }
    }
    return result;
}

// =====================================================================================================================
// Returns whether the buffer is a depdendent buffer or not
bool AccelStructAsyncBuildLayer::IsDependentBuffer(
    VkCommandBuffer commandBuffer)
{
    return m_dependentCommandBuffers.Contains(commandBuffer);
}

// =====================================================================================================================
VkCommandBuffer AccelStructAsyncBuildLayer::GetAsyncBuildBuffer(
    uint32_t id)
{
    return m_commandBuffers[id];
}

// =====================================================================================================================
// Small helpers for dealing with VkSubmitInfo and VkSubmitInfo2
static uint32_t GetCommandBufferCount(const VkSubmitInfo* pSubmit)
{
    return pSubmit->commandBufferCount;
}

static uint32_t GetCommandBufferCount(const VkSubmitInfo2* pSubmit)
{
    return pSubmit->commandBufferInfoCount;
}

static void SetCommandBufferCount(VkSubmitInfo* pSubmit, uint32_t count)
{
    pSubmit->commandBufferCount = count;
}

static void SetCommandBufferCount(VkSubmitInfo2* pSubmit, uint32_t count)
{
    pSubmit->commandBufferInfoCount = count;
}

static VkCommandBuffer GetCommandBuffer(const VkSubmitInfo* pSubmit, uint32_t index)
{
    return pSubmit->pCommandBuffers[index];
}

static VkCommandBuffer GetCommandBuffer(const VkSubmitInfo2* pSubmit, uint32_t index)
{
    return pSubmit->pCommandBufferInfos[index].commandBuffer;
}

static void IncrementCommandBuffer(VkSubmitInfo* pSubmit, uint32_t count)
{
    pSubmit->pCommandBuffers += count;
}

static void IncrementCommandBuffer(VkSubmitInfo2* pSubmit, uint32_t count)
{
    pSubmit->pCommandBufferInfos += count;
}

// =====================================================================================================================
template<typename SubmitInfoType>
VkResult AccelStructAsyncBuildLayer::SubmitBuildBuffer(
    VkQueue queue,
    uint32_t submitCount,
    SubmitInfoType* pSubmits,
    VkFence fence,
    uint32_t buildSubmitIndex,
    uint32_t buildCommandBufferIndex,
    uint32_t buildResourceIndex)
{
    VkResult result =  VK_SUCCESS;

    constexpr bool IsSynchronization2 = std::is_same<SubmitInfoType, VkSubmitInfo2>::value;

    const bool lastCommandBuffer =
        buildCommandBufferIndex == (GetCommandBufferCount(&pSubmits[buildCommandBufferIndex]) - 1);

    const uint32_t preBuildCount = buildSubmitIndex + 1;
    // If the build command buffer isn't the last buffer in its submit, we'll need to split one VkSubmitInfo
    const uint32_t postBuildCount = lastCommandBuffer ? submitCount - preBuildCount : submitCount - preBuildCount + 1;

    // Split the submitInfo with the build
    const uint32_t oldBuildCommandBufferIndex = GetCommandBufferCount(&pSubmits[buildSubmitIndex]);
    SetCommandBufferCount(&pSubmits[buildSubmitIndex], buildCommandBufferIndex + 1);

    SubmitInfoType* pPostSubmits = pSubmits + (submitCount - postBuildCount);

    const VkCommandBuffer buildCommandBuffer = m_commandBuffers[buildResourceIndex];

    result = ApiCmdBuffer::ObjectFromHandle(buildCommandBuffer)->End();

    if (result == VK_SUCCESS)
    {
        // Make sure this submit is destined for a universal queue
        if (ApiQueue::ObjectFromHandle(queue)->PalQueue(DefaultDeviceIndex)->Type() == Pal::QueueTypeUniversal)
        {
            // Submit everything up to and including the build first
            if (preBuildCount > 0)
            {
                if constexpr (IsSynchronization2)
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                        queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
                }
                else
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                        queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
                }
            }
            SetCommandBufferCount(&pSubmits[buildCommandBufferIndex], oldBuildCommandBufferIndex);

            const uint64_t timelineBase = m_currentTimelineValue += 2;

            result = PalToVkResult(
                ApiQueue::ObjectFromHandle(queue)->PalQueue(DefaultDeviceIndex)->SignalQueueSemaphore(m_semaphore,
                                                                                                      timelineBase));

            if (result == VK_SUCCESS)
            {
                // Submit everything left
                if (postBuildCount > 0)
                {
                    if (lastCommandBuffer == false)
                    {
                        IncrementCommandBuffer(&pPostSubmits[0], buildCommandBufferIndex + 1);
                        SetCommandBufferCount(
                            &pPostSubmits[0], GetCommandBufferCount(&pPostSubmits[0]) - (buildCommandBufferIndex + 1));
                    }
                    if constexpr (IsSynchronization2)
                    {
                        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                            queue, postBuildCount, pPostSubmits, fence);
                    }
                    else
                    {
                        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                            queue, postBuildCount, pPostSubmits, fence);
                    }
                }
            }

            // Now submit the async buffer to the compute queue

            const auto pComputeQueue = ApiQueue::ObjectFromHandle(m_asyncComputeQueue);

            if (result == VK_SUCCESS)
            {
                result = PalToVkResult(
                    pComputeQueue->PalQueue(DefaultDeviceIndex)->WaitQueueSemaphore(m_semaphore, timelineBase));
            }

            if (result == VK_SUCCESS)
            {
                m_buildCommandBuffers[buildResourceIndex] = VK_NULL_HANDLE;
                result = SubmitAsyncBuffer<SubmitInfoType>(m_asyncComputeQueue,
                                                           buildCommandBuffer,
                                                           postBuildCount == 0 ? fence : VK_NULL_HANDLE);
            }

            if (result == VK_SUCCESS)
            {
                result = PalToVkResult(
                    pComputeQueue->PalQueue(DefaultDeviceIndex)->SignalQueueSemaphore(m_semaphore, timelineBase + 1));
                m_buildsInFlight = true;
            }
        }
        else
        {
            // For non-universal queues, just pass the buffers through
            if (preBuildCount > 0)
            {
                if constexpr (IsSynchronization2)
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                        queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
                }
                else
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                        queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
                }
            }
            SetCommandBufferCount(&pSubmits[buildCommandBufferIndex], oldBuildCommandBufferIndex);

            if (result == VK_SUCCESS)
            {
                m_buildCommandBuffers[buildResourceIndex] = VK_NULL_HANDLE;
                result = SubmitAsyncBuffer<SubmitInfoType>(queue,
                                                           buildCommandBuffer,
                                                           postBuildCount == 0 ? fence : VK_NULL_HANDLE);
            }

            if (result == VK_SUCCESS)
            {
                // Submit everything left
                if (postBuildCount > 0)
                {
                    if (lastCommandBuffer == false)
                    {
                        IncrementCommandBuffer(&pPostSubmits[0], buildCommandBufferIndex + 1);
                        SetCommandBufferCount(
                            &pPostSubmits[0], GetCommandBufferCount(&pPostSubmits[0]) - (buildCommandBufferIndex + 1));
                    }
                    if constexpr (IsSynchronization2)
                    {
                        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                            queue, postBuildCount, pPostSubmits, fence);
                    }
                    else
                    {
                        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                            queue, postBuildCount, pPostSubmits, fence);
                    }
                }
            }
        }
    }
    return result;
}

// =====================================================================================================================
template<typename SubmitInfoType>
VkResult AccelStructAsyncBuildLayer::SubmitDependentBuffer(
    VkQueue queue,
    uint32_t submitCount,
    SubmitInfoType* pSubmits,
    VkFence fence,
    uint32_t dependentSubmitIndex,
    uint32_t dependentCommandBufferIndex)
{
    VkResult result =  VK_SUCCESS;

    constexpr bool IsSynchronization2 = std::is_same<SubmitInfoType, VkSubmitInfo2>::value;

    if ((ApiQueue::ObjectFromHandle(queue)->PalQueue(DefaultDeviceIndex)->Type() == Pal::QueueTypeUniversal) &&
        m_buildsInFlight)
    {
        const bool firstCommandBuffer = dependentCommandBufferIndex == 0;

        // If the dependent command buffer isn't the first buffer in its submit, we'll need to split one VkSubmitInfo
        const uint32_t preBuildCount = firstCommandBuffer ? dependentSubmitIndex : dependentSubmitIndex + 1;
        const uint32_t postBuildCount = submitCount - preBuildCount;

        SubmitInfoType* pPostSubmits = pSubmits + (submitCount - postBuildCount);

        const uint32_t oldDependentCommandBufferIndex = GetCommandBufferCount(&pSubmits[dependentSubmitIndex]);
        SetCommandBufferCount(&pSubmits[dependentSubmitIndex], dependentCommandBufferIndex);

        // Submit everything before the dependent buffer (if there are any)
        if (preBuildCount > 0)
        {
            if constexpr (IsSynchronization2)
            {
                result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                    queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
            }
            else
            {
                result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                    queue, preBuildCount, pSubmits, VK_NULL_HANDLE);
            }
        }
        SetCommandBufferCount(&pSubmits[dependentCommandBufferIndex], oldDependentCommandBufferIndex);

        result = PalToVkResult(
            ApiQueue::ObjectFromHandle(queue)->PalQueue(DefaultDeviceIndex)->WaitQueueSemaphore(m_semaphore,
                                                                               m_currentTimelineValue + 1));

        if (result == VK_SUCCESS)
        {
            // Submit the dependent buffer and the rest
            if (postBuildCount > 0)
            {
                if (firstCommandBuffer == false)
                {
                    IncrementCommandBuffer(&pPostSubmits[0], dependentCommandBufferIndex);
                    SetCommandBufferCount(&pPostSubmits[0],
                                          GetCommandBufferCount(&pPostSubmits[0]) - dependentCommandBufferIndex);
                }
                if constexpr (IsSynchronization2)
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(
                        queue, postBuildCount, pPostSubmits, fence);
                }
                else
                {
                    result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(
                        queue, postBuildCount, pPostSubmits, fence);
                }
            }
        }
        m_buildsInFlight = false;
    }
    else
    {
        // Just pass through the submit, we don't need to do anything
        if constexpr (IsSynchronization2)
        {
            result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(queue, submitCount, pSubmits, fence);
        }
        else
        {
            result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(queue, submitCount, pSubmits, fence);
        }
    }

    return result;
}

// =====================================================================================================================
template<typename SubmitInfoType>
VkResult AccelStructAsyncBuildLayer::SubmitAsyncBuffer(
    VkQueue queue,
    VkCommandBuffer asyncBuffer,
    VkFence fence)
{
    constexpr bool IsSynchronization2 = std::is_same<SubmitInfoType, VkSubmitInfo2>::value;
    VkResult result = VK_SUCCESS;

    if constexpr (IsSynchronization2)
    {
        VkCommandBufferSubmitInfo bufferSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = asyncBuffer,
            .deviceMask = 0,
        };
        VkSubmitInfo2 buildSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &bufferSubmitInfo,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos = nullptr
        };

        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(queue, 1, &buildSubmitInfo, fence);
    }
    else
    {
        SubmitInfoType buildSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &asyncBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(queue, 1, &buildSubmitInfo, fence);
    }
    return result;
}

// =====================================================================================================================
// Determines if the submission contains any build or dependent buffers, and submits them appropriately
template<typename SubmitInfoType>
VkResult AccelStructAsyncBuildLayer::SubmitBuffers(
    VkQueue queue,
    uint32_t submitCount,
    const SubmitInfoType* pSubmits,
    VkFence fence)
{
    VkResult result = VK_SUCCESS;

    constexpr bool IsSynchronization2 = std::is_same<SubmitInfoType, VkSubmitInfo2>::value;

    uint32_t buildSubmitIndex = 0;
    uint32_t dependentSubmitIndex = 0;
    uint32_t buildCommandBufferIndex = 0;
    uint32_t dependentCommandBufferIndex = 0;
    uint32_t buildResourceIndex = 0;
    bool buildBufferFound = false;
    bool dependentBufferFound = false;

    for (uint32_t i = 0; i < submitCount; ++i)
    {
        for (uint32_t j = 0; j < GetCommandBufferCount(&pSubmits[i]); ++j)
        {
            const VkCommandBuffer commandBuffer = GetCommandBuffer(&pSubmits[i], j);
            const Util::Optional<uint32_t> buildId = GetBuildBufferId(commandBuffer);
            if (buildId.HasValue())
            {
                buildResourceIndex = buildId.Value();
                buildSubmitIndex = i;
                buildCommandBufferIndex = j;
                buildBufferFound = true;
            }
            if (IsDependentBuffer(commandBuffer))
            {
                // Only need to find the first dependent buffer
                if (dependentBufferFound == false)
                {
                    dependentSubmitIndex = i;
                    dependentCommandBufferIndex = j;
                    dependentBufferFound = true;
                }
                RemoveDependentBuffer(commandBuffer);
            }
        }
    }
    if (buildBufferFound || dependentBufferFound)
    {
        Util::Vector<SubmitInfoType, 8, PalAllocator> submitsCopy{m_pInstance->Allocator()};
        submitsCopy.Reserve(submitCount);
        for (uint32_t i = 0; i < submitCount; ++i)
        {
            submitsCopy.PushBack(pSubmits[i]);
        }

        if (buildBufferFound && !dependentBufferFound)
        {
            result = SubmitBuildBuffer(queue, submitsCopy.NumElements(), submitsCopy.Data(),
                                       fence, buildSubmitIndex, buildCommandBufferIndex, buildResourceIndex);
        }
        else if ((buildBufferFound == false) && dependentBufferFound)
        {
            result = SubmitDependentBuffer(queue, submitsCopy.NumElements(), submitsCopy.Data(),
                                           fence, dependentSubmitIndex, dependentCommandBufferIndex);
        }
        else if (buildBufferFound && dependentBufferFound)
        {
            if (buildSubmitIndex < dependentSubmitIndex)
            {
                // Build is before dependent, not in the same submit
                uint32_t buildSubmits = buildSubmitIndex + 1;
                result = SubmitBuildBuffer(queue, buildSubmits, submitsCopy.Data(), VK_NULL_HANDLE,
                                           buildSubmitIndex, buildCommandBufferIndex, buildResourceIndex);
                if (result == VK_SUCCESS)
                {
                    result = SubmitDependentBuffer(queue, submitsCopy.NumElements() - buildSubmits,
                                                   &submitsCopy[buildSubmits], fence, dependentSubmitIndex,
                                                   dependentCommandBufferIndex);
                }
            }
            else if ((buildSubmitIndex == dependentSubmitIndex) &&
                (buildCommandBufferIndex < dependentCommandBufferIndex))
            {
                // Build is before dependent, in the same submit
                // First submit everything up to and including the build command buffer
                uint32_t buildCommandBuffers = buildCommandBufferIndex + 1;
                SetCommandBufferCount(&submitsCopy[buildSubmitIndex], buildCommandBuffers);
                result = SubmitBuildBuffer(queue, buildSubmitIndex + 1, submitsCopy.Data(), VK_NULL_HANDLE,
                                           buildSubmitIndex, buildCommandBufferIndex, buildResourceIndex);

                if (result == VK_SUCCESS)
                {
                    // Now submit the remainder of pSubmits[buildSubmitIndex] as well as any pSubmits after
                    IncrementCommandBuffer(&submitsCopy[buildSubmitIndex], buildCommandBuffers);
                    SetCommandBufferCount(&submitsCopy[buildSubmitIndex],
                                          GetCommandBufferCount(&pSubmits[buildSubmitIndex]) - buildCommandBuffers);
                    result = SubmitDependentBuffer(queue, submitsCopy.NumElements() - buildSubmitIndex,
                                                   &submitsCopy[buildSubmitIndex], fence, dependentSubmitIndex,
                                                   dependentCommandBufferIndex - buildCommandBuffers);
                }
            }
            else if (dependentSubmitIndex < buildSubmitIndex)
            {
                // Dependent is before build, not in the same submit
                // We'll just submit everything up to (but excluding) the build buffer first
                uint32_t dependentSubmits = buildSubmitIndex;
                result = SubmitDependentBuffer(queue, dependentSubmits, submitsCopy.Data(), VK_NULL_HANDLE,
                                               dependentSubmitIndex, dependentCommandBufferIndex);
                if (result == VK_SUCCESS)
                {
                    result = SubmitBuildBuffer(queue, submitsCopy.NumElements() - dependentSubmits,
                                               &submitsCopy[dependentSubmits], fence, buildSubmitIndex,
                                               buildCommandBufferIndex, buildResourceIndex);
                }
            }
            else if ((dependentSubmitIndex == buildSubmitIndex) &&
                (dependentCommandBufferIndex < buildCommandBufferIndex))
            {
                // Dependent is before build, in the same submit
                // First submit everything up to and excluding the build command buffer
                uint32_t dependentCommandBuffers = buildCommandBufferIndex;
                SetCommandBufferCount(&submitsCopy[dependentSubmitIndex], buildCommandBufferIndex);
                result = SubmitDependentBuffer(queue, dependentSubmitIndex + 1, submitsCopy.Data(), VK_NULL_HANDLE,
                                               dependentSubmitIndex, dependentCommandBufferIndex);

                if (result == VK_SUCCESS)
                {
                    // Now submit the remainder of pSubmits[dependentSubmitIndex] as well as any pSubmits after
                    IncrementCommandBuffer(&submitsCopy[buildSubmitIndex],  dependentCommandBuffers);
                    SetCommandBufferCount(&submitsCopy[dependentSubmitIndex], GetCommandBufferCount(
                        &pSubmits[dependentSubmitIndex]) - dependentSubmitIndex);
                    result = SubmitBuildBuffer(queue, submitsCopy.NumElements() - dependentSubmitIndex,
                                               &submitsCopy[dependentSubmitIndex], fence, buildSubmitIndex,
                                               buildCommandBufferIndex - dependentCommandBuffers, buildResourceIndex);
                }
            }
            else
            {
                //build and dep are same command buffer (not currently handled)
                VK_NOT_IMPLEMENTED;
                result = VK_ERROR_UNKNOWN;
            }
        }
    }
    else
    {
        if constexpr (IsSynchronization2)
        {
            result = GetNextLayer()->GetEntryPoints().vkQueueSubmit2(queue, submitCount, pSubmits, fence);
        }
        else
        {
            result = GetNextLayer()->GetEntryPoints().vkQueueSubmit(queue, submitCount, pSubmits, fence);
        }
    }

    return result;
}

// =====================================================================================================================
namespace entry
{

namespace accelerationStructureAsyncBuildLayer
{
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    AccelStructAsyncBuildLayer* pLayer = pCmdBuffer->VkDevice()->RayTrace()->GetAccelStructAsyncBuildLayer();
    pLayer->AddDependentBuffer(commandBuffer);
    pLayer->GetNextLayer()->GetEntryPoints().vkCmdTraceRaysKHR(
         commandBuffer,
         pRaygenShaderBindingTable,
         pMissShaderBindingTable,
         pHitShaderBindingTable,
         pCallableShaderBindingTable,
         width,
         height,
         depth);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer originalCommandBuffer,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
    VkCommandBuffer commandBuffer = originalCommandBuffer;

    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    AccelStructAsyncBuildLayer* pLayer = pCmdBuffer->VkDevice()->RayTrace()->GetAccelStructAsyncBuildLayer();

    for (uint32_t i = 0; i < infoCount; ++i)
    {
        if (pInfos[i].type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
        {
            uint32_t id = pLayer->AddBuildBuffer(commandBuffer);
            commandBuffer = pLayer->GetAsyncBuildBuffer(id);
            break;
        }
    }

    pLayer->GetNextLayer()->GetEntryPoints().vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        infoCount,
        pInfos,
        ppBuildRangeInfos);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo *pSubmits,
    VkFence fence)
{
    Queue* pQueue = ApiQueue::ObjectFromHandle(queue);
    AccelStructAsyncBuildLayer* pLayer = pQueue->VkDevice()->RayTrace()->GetAccelStructAsyncBuildLayer();
    return pLayer->SubmitBuffers(queue, submitCount, pSubmits, fence);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(
    VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo2 *pSubmits,
    VkFence fence)
{
    Queue* pQueue = ApiQueue::ObjectFromHandle(queue);
    AccelStructAsyncBuildLayer* pLayer = pQueue->VkDevice()->RayTrace()->GetAccelStructAsyncBuildLayer();
    return pLayer->SubmitBuffers(queue, submitCount, pSubmits, fence);
}

} // accelerationStructureAsyncBuildLayer
} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define AS_ASYNC_BUILD_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::accelerationStructureAsyncBuildLayer::func_name

#define AS_ASYNC_BUILD_OVERRIDE_ENTRY(entry_name) AS_ASYNC_BUILD_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void AccelStructAsyncBuildLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    AS_ASYNC_BUILD_OVERRIDE_ENTRY(vkCmdBuildAccelerationStructuresKHR);
    AS_ASYNC_BUILD_OVERRIDE_ENTRY(vkCmdTraceRaysKHR);
    AS_ASYNC_BUILD_OVERRIDE_ENTRY(vkQueueSubmit);
    AS_ASYNC_BUILD_OVERRIDE_ENTRY(vkQueueSubmit2);
}

} // namespace vk
