/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_device.h
 * @brief Contains declaration of Vulkan device classes.
 ***********************************************************************************************************************
 */

#ifndef __VK_DEVICE_H__
#define __VK_DEVICE_H__

#pragma once

#include "include/khronos/vulkan.h"

#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"

#include "include/app_shader_optimizer.h"

#include "include/internal_mem_mgr.h"
#include "include/render_state_cache.h"
#include "include/virtual_stack_mgr.h"
#include "include/barrier_policy.h"

#include "palDevice.h"
#include "palImage.h"
#include "palList.h"
#include "palHashMap.h"
#include "palPipeline.h"

namespace Pal
{
class IPipeline;
struct ResourceMappingNode;
};

namespace Util
{
class VirtualLinearAllocator;
};

namespace Llpc
{
class ICompiler;
struct ResourceMappingNode;
};

namespace vk
{

// Forward declarations of Vulkan classes used in this file.
class BarrierFilterLayer;
class Buffer;
struct CmdBufGpuMem;
class Device;
class DispatchableDevice;
class DispatchableQueue;
class Instance;
class OptLayer;
class PhysicalDevice;
class Queue;
class SqttMgr;
class SwapChain;
class ChillMgr;

// =====================================================================================================================
// Specifies properties for importing a semaphore, it's an encapsulation of VkImportSemaphoreFdInfoKHR and
// VkImportSemaphoreWin32HandleInfoKHR. Please refer to the vkspec for the defination of members.
struct ImportSemaphoreInfo
{
    VkExternalSemaphoreHandleTypeFlags  handleType;
    Pal::OsExternalHandle               handle;
    VkSemaphoreImportFlags              importFlags;
};

// =====================================================================================================================
class Device
{
public:
    // Pipelines used for internal operations, e.g. certain resource copies
    struct InternalPipeline
    {
        InternalPipeline();

        uint32_t  userDataNodeOffsets[16];
        Pal::IPipeline*  pPipeline[MaxPalDevices];
    };
    static const uint32_t MaxInternalPipelineUserNodeCount = 16;

    typedef VkDevice ApiType;

    struct Properties
    {
        VkDeviceSize virtualMemAllocGranularity;
        VkDeviceSize virtualMemPageSize;

        struct
        {
            uint32_t bufferView;
            uint32_t imageView;
            uint32_t fmaskView;
            uint32_t sampler;
            uint32_t combinedImageSampler;
            uint32_t alignment;
        } descriptorSizes;

        struct
        {
            size_t colorTargetView;
            size_t depthStencilView;
        } palSizes;

        uint32_t timestampQueryPoolSlotSize;

        bool connectThroughThunderBolt;
    };

    static VkResult Create(
        PhysicalDevice*                             pPhysicalDevice,
        const VkDeviceCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        DispatchableDevice**                        ppDevice);

    VkResult Destroy(const VkAllocationCallbacks*   pAllocator);

    VkResult WaitIdle(void);

    VkResult AllocMemory(
        const VkMemoryAllocateInfo*                 pAllocInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDeviceMemory*                             pMem);

    VkResult GetQueue(
        uint32_t                                    queueFamilyIndex,
        uint32_t                                    queueIndex,
        VkQueue*                                    pQueue);

    VkResult CreateEvent(
        const VkEventCreateInfo*                    pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkEvent*                                    pEvent);

    VkResult CreateFence(
        const VkFenceCreateInfo*                    pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkFence*                                    pFence);

    VkResult WaitForFences(
        uint32_t                                    fenceCount,
        const VkFence*                              pFences,
        VkBool32                                    waitAll,
        uint64_t                                    timeout);

    VkResult ResetFences(
        uint32_t                                    fenceCount,
        const VkFence*                              pFences);

    VkResult CreateDescriptorSetLayout(
        const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDescriptorSetLayout*                      pSetLayout);

    VkResult CreateDescriptorUpdateTemplate(
        const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDescriptorUpdateTemplate*                 pDescriptorUpdateTemplate);

    VkResult CreatePipelineLayout(
        const VkPipelineLayoutCreateInfo*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkPipelineLayout*                           pPipelineLayout);

    VkResult AllocateCommandBuffers(
        const VkCommandBufferAllocateInfo*          pAllocateInfo,
        VkCommandBuffer*                            pCommandBuffers);

    VkResult CreateFramebuffer(
        const VkFramebufferCreateInfo*              pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkFramebuffer*                              pFramebuffer);

    VkResult CreateRenderPass(
        const VkRenderPassCreateInfo*               pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkRenderPass*                               pRenderPass);

    VkResult CreateRenderPass2(
        const VkRenderPassCreateInfo2KHR*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkRenderPass*                               pRenderPass);

    VkResult CreateSemaphore(
        const VkSemaphoreCreateInfo*                pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSemaphore*                                pSemaphore);

    VkResult CreateQueryPool(
        const VkQueryPoolCreateInfo*                pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkQueryPool*                                pQueryPool);

    VkResult CreateBuffer(
        const VkBufferCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkBuffer*                                   pBuffer);

    VkResult CreateBufferView(
        const VkBufferViewCreateInfo*               pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkBufferView*                               pView);

    VkResult CreateImage(
        const VkImageCreateInfo*                    pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkImage*                                    pImage);

    VkResult CreateImageView(
        const VkImageViewCreateInfo*                pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkImageView*                                pView);

    VkResult CreateGraphicsPipelines(
        VkPipelineCache                             pipelineCache,
        uint32_t                                    count,
        const VkGraphicsPipelineCreateInfo*         pCreateInfos,
        const VkAllocationCallbacks*                pAllocator,
        VkPipeline*                                 pPipelines);

    VkResult CreateComputePipelines(
        VkPipelineCache                             pipelineCache,
        uint32_t                                    count,
        const VkComputePipelineCreateInfo*          pCreateInfos,
        const VkAllocationCallbacks*                pAllocator,
        VkPipeline*                                 pPipelines);

    VkResult CreateSampler(
        const VkSamplerCreateInfo*                  pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSampler*                                  pSampler);

   VkResult CreateCommandPool(
        const VkCommandPoolCreateInfo*              pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkCommandPool*                              pCmdPool);

    VkResult CreateShaderModule(
        const VkShaderModuleCreateInfo*             pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkShaderModule*                             pShaderModule);

    VkResult CreatePipelineCache(
        const VkPipelineCacheCreateInfo*            pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkPipelineCache*                            pPipelineCache);

    VkResult CreateSwapchain(
        const VkSwapchainCreateInfoKHR*             pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSwapchainKHR*                             pSwapChain);

    VkResult ImportSemaphore(
        VkSemaphore                semaphore,
        const ImportSemaphoreInfo& importInfo);

    VkResult Initialize(
        DispatchableQueue**                         pQueues,
        const DeviceExtensions::Enabled&            enabled,
        const VkMemoryOverallocationBehaviorAMD     overallocationBehavior,
        const bool                                  deviceCoherentMemoryEnabled);

    void InitDispatchTable();

    VK_FORCEINLINE Instance* VkInstance() const
        { return m_pInstance; }

    VK_FORCEINLINE InternalMemMgr* MemMgr()
        { return &m_internalMemMgr; }

    VK_FORCEINLINE ShaderOptimizer* GetShaderOptimizer()
        { return &m_shaderOptimizer; }

    VK_FORCEINLINE bool IsMultiGpu() const
        { return m_palDeviceCount > 1; }

    VK_FORCEINLINE uint32_t      NumPalDevices() const
        { return m_palDeviceCount; }

    VK_FORCEINLINE uint32_t      GetPalDeviceMask() const
    {
        return (1 << m_palDeviceCount) - 1;
    }

    VK_FORCEINLINE Pal::IDevice* PalDevice(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(m_palDeviceCount)));
        return m_perGpu[idx].pPalDevice;
    }

    VK_FORCEINLINE PhysicalDevice* VkPhysicalDevice(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(m_palDeviceCount)));
        return m_perGpu[idx].pPhysicalDevice;
    }

    VK_FORCEINLINE Pal::ICmdAllocator* GetSharedCmdAllocator(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(m_palDeviceCount)));
        return m_perGpu[idx].pSharedPalCmdAllocator;
    }

    VK_FORCEINLINE const Properties& GetProperties() const
        { return m_properties; }

    Pal::QueueType GetQueueFamilyPalQueueType(
        uint32_t queueFamilyIndex) const;

    Pal::EngineType GetQueueFamilyPalEngineType(
        uint32_t queueFamilyIndex) const;

    VK_INLINE uint32_t GetQueueFamilyPalImageLayoutFlag(
        uint32_t queueFamilyIndex) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalImageLayoutFlag(queueFamilyIndex);
    }

    VK_INLINE uint32_t GetMemoryTypeMask() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMask();
    }

    VK_INLINE bool GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkIndexBits) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetVkTypeIndexBitsFromPalHeap(heapIndex, pVkIndexBits);
    }

    VK_INLINE Pal::GpuHeap GetPalHeapFromVkTypeIndex(uint32_t vkIndex) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetPalHeapFromVkTypeIndex(vkIndex);
    }

    VK_INLINE uint32_t GetUmdFpsCapFrameRate() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().osProperties.umdFpsCapFrameRate;
    }

    void GetDeviceGroupPeerMemoryFeatures(
        uint32_t                  heapIndex,
        uint32_t                  localDeviceIndex,
        uint32_t                  remoteDeviceIndex,
        VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) const;

    VkResult GetDeviceGroupPresentCapabilities(
        VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) const;

    VkResult GetDeviceGroupSurfacePresentModes(
        VkSurfaceKHR                      surface,
        VkDeviceGroupPresentModeFlagsKHR* pModes) const;

    VkResult BindBufferMemory(
        uint32_t                      bindInfoCount,
        const VkBindBufferMemoryInfo* pBindInfos) const;

    VkResult BindImageMemory(
        uint32_t                     bindInfoCount,
        const VkBindImageMemoryInfo* pBindInfos) const;

    VK_INLINE const VkPhysicalDeviceFeatures& GetEnabledFeatures() const
        { return m_enabledFeatures;}

    bool IsVirtualRemappingSupported() const;

    Pal::PrtFeatureFlags GetPrtFeatures() const;
    Pal::gpusize GetVirtualAllocAlignment() const;

    VK_INLINE void* AllocApiObject(
        size_t                       size,
        const VkAllocationCallbacks* pAllocator) const
    {
        return pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    size,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    }

    Pal::Result AddMemReference(
        Pal::IDevice*       pPalDevice,
        Pal::IGpuMemory*    pPalMemory,
        bool                readOnly = false);

    void RemoveMemReference(
        Pal::IDevice*       pPalDevice,
        Pal::IGpuMemory*    pPalMemory);

    VK_INLINE const RuntimeSettings& GetRuntimeSettings() const
        { return m_settings; }

    // return too many objects if the allocation count will exceed max limit.
    // There is a potential improvement by using atomic inc/dec.
    // That require us to limit the max allocation to some value less than UINT_MAX
    // to avoid the overflow.
    VK_INLINE VkResult IncreaseAllocationCount()
    {
        VkResult vkResult = VK_SUCCESS;
        Util::MutexAuto lock(&m_memoryMutex);

        if (m_allocatedCount < m_maxAllocations)
        {
            m_allocatedCount ++;
        }
        else
        {
            vkResult = VK_ERROR_TOO_MANY_OBJECTS;
        }
        return vkResult;
    }

    VK_INLINE void DecreaseAllocationCount()
    {
        Util::MutexAuto lock(&m_memoryMutex);
        m_allocatedCount --;
    }

    VkResult TryIncreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     deviceMask,
        uint32_t     heapIdx);

    void IncreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     deviceMask,
        uint32_t     heapIdx);

    void DecreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     deviceMask,
        uint32_t     heapIdx);

    VK_INLINE const InternalPipeline& GetTimestampQueryCopyPipeline() const
        { return m_timestampQueryCopyPipeline; }

    VK_INLINE const Pal::IMsaaState* const * GetBltMsaaState(uint32_t imgSampleCount) const;

    VK_INLINE bool IsExtensionEnabled(DeviceExtensions::ExtensionId id) const
        { return m_enabledExtensions.IsExtensionEnabled(id); }

    VK_INLINE AppProfile GetAppProfile() const
        { return VkPhysicalDevice(DefaultDeviceIndex)->GetAppProfile(); }

    VK_INLINE SqttMgr* GetSqttMgr()
        { return m_pSqttMgr; }

    VK_INLINE OptLayer* GetAppOptLayer()
        { return m_pAppOptLayer; }

    VK_INLINE BarrierFilterLayer* GetBarrierFilterLayer()
        { return m_pBarrierFilterLayer; }

    VK_INLINE Util::Mutex* GetMemoryMutex()
        { return &m_memoryMutex; }

    VK_INLINE Util::Mutex* GetTimerQueueMutex()
        { return &m_timerQueueMutex; }

    VK_INLINE PipelineCompiler* GetCompiler(uint32_t idx) const
        { return m_perGpu[idx].pPhysicalDevice->GetCompiler(); }

    static const Pal::MsaaQuadSamplePattern* GetDefaultQuadSamplePattern(uint32_t sampleCount);
    static uint32_t GetDefaultSamplePatternIndex(uint32_t sampleCount);

    VkDeviceSize GetMemoryBaseAddrAlignment(uint32_t memoryTypes) const;

    VK_INLINE RenderStateCache* GetRenderStateCache()
        { return &m_renderStateCache; }

    uint32_t GetPinnedSystemMemoryTypes() const;

    uint32_t GetExternalHostMemoryTypes(
        VkExternalMemoryHandleTypeFlagBits handleType,
        const void*                        pExternalPtr) const;

    VK_FORCEINLINE const DispatchTable& GetDispatchTable() const
        { return m_dispatchTable; }

    VK_FORCEINLINE const EntryPoints& GetEntryPoints() const
        { return m_dispatchTable.GetEntryPoints(); }

    VK_FORCEINLINE const DeviceBarrierPolicy& GetBarrierPolicy() const
        { return m_barrierPolicy; }

    VK_INLINE const bool IsAllocationSizeTrackingEnabled() const
        { return m_allocationSizeTracking; }

    VK_INLINE bool UseStridedCopyQueryResults() const
        { return (m_properties.timestampQueryPoolSlotSize == 32); }

    VK_INLINE const bool IsDeviceCoherentMemoryEnabled() const
        { return m_deviceCoherentMemoryEnabled; }

    Pal::IQueue* PerformSwCompositing(
        uint32_t         deviceIdx,
        uint32_t         presentationDeviceIdx,
        Pal::ICmdBuffer* pCommandBuffer,
        Pal::QueueType   cmdBufferQueueType,
        const Queue*     pQueue);

protected:
    Device(
        uint32_t                         deviceCount,
        PhysicalDevice**                 pPhysicalDevices,
        Pal::IDevice**                   pPalDevices,
        const DeviceBarrierPolicy&       barrierPolicy,
        const DeviceExtensions::Enabled& enabledExtensions,
        const VkPhysicalDeviceFeatures*  pFeatures,
        bool                             useComputeAsTransferQueue);

    VkResult CreateInternalComputePipeline(
        size_t                           codeByteSize,
        const uint8_t*                   pCode,
        uint32_t                         numUserDataNodes,
        const Llpc::ResourceMappingNode* pUserDataNodes,
        InternalPipeline*                pInternalPipeline);

    VkResult CreateInternalPipelines();

    void DestroyInternalPipeline(InternalPipeline* pPipeline);

    VkResult CreateBltMsaaStates();
    void DestroyInternalPipelines();

    void InitSamplePatternPalette(Pal::SamplePatternPalette* pPalette) const;

    VkResult InitSwCompositing(uint32_t deviceIdx);

    Instance* const                     m_pInstance;
    const RuntimeSettings&              m_settings;

    uint32_t                            m_palDeviceCount;

    Properties                          m_properties;

    InternalMemMgr                      m_internalMemMgr;

    ShaderOptimizer                     m_shaderOptimizer;

    RenderStateCache                    m_renderStateCache;

    DispatchableQueue*                  m_pQueues[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily];

    InternalPipeline                    m_timestampQueryCopyPipeline;

    static const uint32_t BltMsaaStateCount = 4;

    Pal::IMsaaState*                    m_pBltMsaaState[BltMsaaStateCount][MaxPalDevices];

    const DeviceBarrierPolicy           m_barrierPolicy;        // Barrier policy to use for this device

    const DeviceExtensions::Enabled     m_enabledExtensions;    // Enabled device extensions
    DispatchTable                       m_dispatchTable;        // Device dispatch table
    SqttMgr*                            m_pSqttMgr;             // Manager for developer mode SQ thread tracing
    OptLayer*                           m_pAppOptLayer;         // State for an app-specific layer, otherwise null
    BarrierFilterLayer*                 m_pBarrierFilterLayer;  // State for enabling barrier filtering, otherwise null
    Util::Mutex                         m_memoryMutex;          // Shared mutex used occasionally by memory objects
    Util::Mutex                         m_timerQueueMutex;      // Shared mutex used occasionally by timer queue objects

    // The states of m_enabledFeatures are provided by application
    VkPhysicalDeviceFeatures            m_enabledFeatures;

    // The states of enabled feature DEVICE_COHERENT_MEMORY_FEATURES_AMD which is defined by
    // extensions VK_AMD_device_coherent_memory
    bool                                m_deviceCoherentMemoryEnabled;

    // The count of allocations that has been created from the logical device.
    uint32_t                            m_allocatedCount;

    // The maximum allocations that can be created from the logical device
    uint32_t                            m_maxAllocations;

    // Determines if the allocated memory size will be tracked (error will be thrown when
    // allocation exceeds threshold size)
    bool                                m_allocationSizeTracking;

    // If set to true, will use a compute queue internally for transfers.
    bool                                m_useComputeAsTransferQueue;

    struct PerGpuInfo
    {
        PhysicalDevice*        pPhysicalDevice;
        Pal::IDevice*          pPalDevice;
        Pal::ICmdAllocator*    pSharedPalCmdAllocator;

        void*                  pSwCompositingMemory;    // Internal memory for the below PAL objects (master and slave)
        Pal::IQueue*           pSwCompositingQueue;     // Internal present queue (master) or transfer queue (slave)
        Pal::IQueueSemaphore*  pSwCompositingSemaphore; // Internal semaphore (master and slave)
    };

    // Compute size required for the object.  One copy of PerGpuInfo is included in the object and we need
    // to add space for any additional GPUs.
    static size_t ObjectSize(size_t baseClassSize, uint32_t numDevices)
    {
        return baseClassSize + ((numDevices - 1) * sizeof(PerGpuInfo));
    }

    // This goes last.  The memory for the rest of the array is calculated dynamically based on the number of GPUs in
    // use.
    PerGpuInfo              m_perGpu[1];
};

// =====================================================================================================================
const Pal::IMsaaState* const * Device::GetBltMsaaState(
    uint32_t imgSampleCount
    ) const
{
    uint32_t i = Util::Log2(imgSampleCount);

    if (i < BltMsaaStateCount)
    {
        return &m_pBltMsaaState[i][0];
    }
    else
    {
        return nullptr;
    }
}

VK_DEFINE_DISPATCHABLE(Device);

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(
    VkDevice                                    device,
    const VkFenceCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkFence*                                    pFence);

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences,
    VkBool32                                    waitAll,
    uint64_t                                    timeout);

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue2(
    VkDevice                                    device,
    const VkDeviceQueueInfo2*                   pQueueInfo,
    VkQueue*                                    pQueue);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(
    VkDevice                                    device,
    const VkSemaphoreCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSemaphore*                                pSemaphore);

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice                                    device,
    const VkMemoryAllocateInfo*                 pAllocateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDeviceMemory*                             pMemory);

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice                                    device,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(
    VkDevice                                    device);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateEvent(
    VkDevice                                    device,
    const VkEventCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkEvent*                                    pEvent);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateQueryPool(
    VkDevice                                    device,
    const VkQueryPoolCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkQueryPool*                                pQueryPool);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorSetLayout*                      pSetLayout);

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice                                    device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineLayout*                           pPipelineLayout);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice                                    device,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorPool*                           pDescriptorPool);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(
    VkDevice                                    device,
    const VkFramebufferCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkFramebuffer*                              pFramebuffer);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice                                    device,
    const VkRenderPassCreateInfo*               pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkRenderPass*                               pRenderPass);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(
    VkDevice                                    device,
    const VkRenderPassCreateInfo2KHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkRenderPass*                               pRenderPass);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(
    VkDevice                                    device,
    const VkBufferCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBuffer*                                   pBuffer);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferView(
    VkDevice                                    device,
    const VkBufferViewCreateInfo*               pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBufferView*                               pView);

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice                                    device,
    const VkCommandBufferAllocateInfo*          pAllocateInfo,
    VkCommandBuffer*                            pCommandBuffers);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice                                    device,
    const VkCommandPoolCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkCommandPool*                              pCommandPool);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImage*                                    pImage);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImageView*                                pView);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkShaderModule*                             pShaderModule);

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineCache(
    VkDevice                                    device,
    const VkPipelineCacheCreateInfo*            pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineCache*                            pPipelineCache);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(
    VkDevice                                    device,
    const VkSamplerCreateInfo*                  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSampler*                                  pSampler);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSamplerYcbcrConversion(
    VkDevice                                    device,
    const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSamplerYcbcrConversion*                   pYcbcrConversion);

VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversion(
    VkDevice                                    device,
    VkSamplerYcbcrConversion                    ycbcrConversion,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice                                    device,
    const VkSwapchainCreateInfoKHR*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSwapchainKHR*                             pSwapchain);

VKAPI_ATTR void VKAPI_CALL vkGetRenderAreaGranularity(
    VkDevice                                    device,
    VkRenderPass                                renderPass,
    VkExtent2D*                                 pGranularity);

VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2(
    VkDevice                                    device,
    uint32_t                                    bindInfoCount,
    const VkBindBufferMemoryInfo*               pBindInfos);

VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2(
    VkDevice                                    device,
    uint32_t                                    bindInfoCount,
    const VkBindImageMemoryInfo*                pBindInfos);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplate(
    VkDevice                                        device,
    const VkDescriptorUpdateTemplateCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkDescriptorUpdateTemplate*                     pDescriptorUpdateTemplate);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceGroupPeerMemoryFeatures(
    VkDevice                                    device,
    uint32_t                                    heapIndex,
    uint32_t                                    localDeviceIndex,
    uint32_t                                    remoteDeviceIndex,
    VkPeerMemoryFeatureFlags*                   pPeerMemoryFeatures);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupPresentCapabilitiesKHR(
    VkDevice                                    device,
    VkDeviceGroupPresentCapabilitiesKHR*        pDeviceGroupPresentCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupSurfacePresentModesKHR(
    VkDevice                                    device,
    VkSurfaceKHR                                surface,
    VkDeviceGroupPresentModeFlagsKHR*           pModes);

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectTagInfoEXT*        pTagInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectNameInfoEXT*       pNameInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFdKHR(
    VkDevice                                    device,
    const VkImportSemaphoreFdInfoKHR*           pImportSemaphoreFdInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkSetGpaDeviceClockModeAMD(
    VkDevice                                    device,
    VkGpaDeviceClockModeInfoAMD*                pInfo);

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupport(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    VkDescriptorSetLayoutSupport*               pSupport);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryHostPointerPropertiesEXT(
    VkDevice                                    device,
    VkExternalMemoryHandleTypeFlagBits          handleType,
    const void*                                 pHostPointer,
    VkMemoryHostPointerPropertiesEXT*           pMemoryHostPointerProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectNameInfoEXT*        pNameInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectTagEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectTagInfoEXT*         pTagInfo);

} // namespace entry

} // namespace vk

#endif /* __VK_DEVICE_H__ */
