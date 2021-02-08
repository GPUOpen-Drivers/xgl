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
#include "include/vk_private_data_slot.h"
#include "include/vk_queue.h"

#include "include/app_shader_optimizer.h"
#include "include/app_resource_optimizer.h"

#include "include/internal_mem_mgr.h"
#include "include/log.h"
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
};

namespace Util
{
class VirtualLinearAllocator;
};

namespace Llpc
{
class ICompiler;
};

namespace Vkgc
{
struct ResourceMappingRootNode;
}

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
class AsyncLayer;

// =====================================================================================================================
// Specifies properties for importing a semaphore, it's an encapsulation of VkImportSemaphoreFdInfoKHR and
// VkImportSemaphoreWin32HandleInfoKHR. Please refer to the vkspec for the defination of members.
struct ImportSemaphoreInfo
{
    VkExternalSemaphoreHandleTypeFlagBits handleType;
    Pal::OsExternalHandle                 handle;
    VkSemaphoreImportFlags                importFlags;
};

// =====================================================================================================================
class Device
{
public:
    // Represent features in VK_EXT_robustness2
    struct ExtendedRobustness
    {
        bool robustBufferAccess;
        bool robustImageAccess;
        bool nullDescriptor;
    };

    struct DeviceFeatures
    {
        VkBool32                robustBufferAccess;
        VkBool32                sparseBinding;
        // The state of enabled feature VK_EXT_scalar_block_layout.
        VkBool32                scalarBlockLayout;
        // Attachment Fragment Shading Rate feature in VK_KHR_variable_rate_shading
        VkBool32                attachmentFragmentShadingRate;
        // The states of enabled feature DEVICE_COHERENT_MEMORY_FEATURES_AMD which is defined by
        // extensions VK_AMD_device_coherent_memory
        VkBool32                deviceCoherentMemory;
        // The state of enabled features in VK_EXT_robustness2.
        ExtendedRobustness      extendedRobustness;
    };

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
            uint32_t bvh;
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

    VkResult GetQueue2(
        const VkDeviceQueueInfo2*                   pQueueInfo,
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
        const VkRenderPassCreateInfo2*              pCreateInfo,
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

    VkResult CreateSamplerYcbcrConversion(
        const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSamplerYcbcrConversion*                   pYcbcrConversion);

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

    VkResult GetSemaphoreCounterValue(
        VkSemaphore                                 semaphore,
        uint64_t*                                   pValue);

    VkResult WaitSemaphores(
        const VkSemaphoreWaitInfo*                  pWaitInfo,
        uint64_t                                    timeout);

    VkResult SignalSemaphore(
        VkSemaphore                                 semaphore,
        uint64_t                                    value);

    VkResult CreateSwapchain(
        const VkSwapchainCreateInfoKHR*             pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSwapchainKHR*                             pSwapChain);

    VkResult ImportSemaphore(
        VkSemaphore                semaphore,
        const ImportSemaphoreInfo& importInfo);

    VkResult Initialize(
        PhysicalDevice*                             pPhysicalDevice,
        DispatchableQueue**                         pQueues,
        const DeviceExtensions::Enabled&            enabled,
        const VkMemoryOverallocationBehaviorAMD     overallocationBehavior,
        const bool                                  deviceCoherentMemoryEnabled,
        const bool                                  attachmentFragmentShadingRate,
        bool                                        scalarBlockLayoutEnabled,
        const ExtendedRobustness&                   extendedRobustnessEnabled);

    void InitDispatchTable();

    VK_FORCEINLINE Instance* VkInstance() const
        { return m_pInstance; }

    VK_FORCEINLINE InternalMemMgr* MemMgr()
        { return &m_internalMemMgr; }

    VK_FORCEINLINE ShaderOptimizer* GetShaderOptimizer()
        { return &m_shaderOptimizer; }

    VK_FORCEINLINE ResourceOptimizer* GetResourceOptimizer()
        { return &m_resourceOptimizer; }

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

    VK_INLINE uint32_t GetMemoryTypeMaskMatching(VkMemoryPropertyFlags flags) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskMatching(flags);
    }

    VK_INLINE uint32_t GetMemoryTypeMaskForExternalSharing() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskForExternalSharing();
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

    VK_INLINE uint64_t TimestampFrequency() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().timestampFrequency;
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

    VK_INLINE const DeviceFeatures& GetEnabledFeatures() const
        { return m_enabledFeatures; }

    Pal::PrtFeatureFlags GetPrtFeatures() const;

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

    VK_INLINE bool ShouldAddRemoteBackupHeap(
        uint32_t deviceIdx,
        uint32_t memoryTypeIdx,
        uint32_t palHeapIdx) const
    {
        return (m_perGpu[deviceIdx].pPhysicalDevice->ShouldAddRemoteBackupHeap(memoryTypeIdx) ||
                m_overallocationRequestedForPalHeap[palHeapIdx]);
    }

    VK_INLINE const InternalPipeline& GetTimestampQueryCopyPipeline() const
        { return m_timestampQueryCopyPipeline; }

    VK_INLINE InternalPipeline& GetInternalRayTracingPipeline()
    {
        return m_internalRayTracingPipeline;
    }

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

    VK_INLINE AsyncLayer* GetAsyncLayer()
        { return m_pAsyncLayer; }

    VK_INLINE Util::Mutex* GetMemoryMutex()
        { return &m_memoryMutex; }

    VK_INLINE PipelineCompiler* GetCompiler(uint32_t idx) const
        { return m_perGpu[idx].pPhysicalDevice->GetCompiler(); }

    static const Pal::MsaaQuadSamplePattern* GetDefaultQuadSamplePattern(uint32_t sampleCount);
    static uint32_t GetDefaultSamplePatternIndex(uint32_t sampleCount);

    VkDeviceSize GetMemoryBaseAddrAlignment(uint32_t memoryTypes) const;

    VK_INLINE RenderStateCache* GetRenderStateCache()
        { return &m_renderStateCache; }

    uint32_t GetPinnedSystemMemoryTypes() const;

    uint32_t GetPinnedHostMappedForeignMemoryTypes() const;

    uint32_t GetExternalHostMemoryTypes(
        VkExternalMemoryHandleTypeFlagBits handleType,
        const void*                        pExternalPtr) const;

    VkResult GetCalibratedTimestamps(
        uint32_t                            timestampCount,
        const VkCalibratedTimestampInfoEXT* pTimestampInfos,
        uint64_t*                           pTimestamps,
        uint64_t*                           pMaxDeviation);

    VK_FORCEINLINE const DispatchTable& GetDispatchTable() const
        { return m_dispatchTable; }

    VK_FORCEINLINE const EntryPoints& GetEntryPoints() const
        { return m_dispatchTable.GetEntryPoints(); }

    VK_FORCEINLINE const DeviceBarrierPolicy& GetBarrierPolicy() const
        { return m_barrierPolicy; }

    VK_INLINE bool IsAllocationSizeTrackingEnabled() const
        { return m_allocationSizeTracking; }

    VK_INLINE bool UseStridedCopyQueryResults() const
        { return (m_properties.timestampQueryPoolSlotSize == 32); }

    VK_INLINE bool UseCompactDynamicDescriptors() const
        { return !GetRuntimeSettings().enableRelocatableShaders && !GetEnabledFeatures().robustBufferAccess;}

    VK_INLINE bool SupportDepthStencilResolve() const
    {
        return (IsExtensionEnabled(DeviceExtensions::KHR_DEPTH_STENCIL_RESOLVE) ||
                (VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion() >= VK_MAKE_VERSION(1, 2, 0)));
    }

    Pal::IQueue* PerformSwCompositing(
        uint32_t         deviceIdx,
        uint32_t         presentationDeviceIdx,
        Pal::ICmdBuffer* pCommandBuffer,
        Pal::QueueType   cmdBufferQueueType,
        const Queue*     pQueue);

    VkResult SwCompositingNotifyFlipMetadata(
        Pal::IQueue*            pPresentQueue,
        const Pal::CmdBufInfo&  cmdBufInfo);

    bool BigSW60Supported() const;

    void UpdateFeatureSettings();

    VK_FORCEINLINE VkExtent2D GetMaxVrsShadingRate() const
    {
        return m_maxVrsShadingRate;
    }

    VK_INLINE size_t GetPrivateDataSize() const
    {
        return m_privateDataSize;
    }

    bool ReserveFastPrivateDataSlot(
        uint64*                         pIndex);

    void* AllocApiObject(
        const VkAllocationCallbacks*    pAllocator,
        const size_t                    totalObjectSize);

    void FreeApiObject(
        const VkAllocationCallbacks*    pAllocator,
        void*                           pMemory);

    void FreeUnreservedPrivateData(
        void*                           pMemory) const;

    VK_INLINE Util::RWLock* GetPrivateDataRWLock()
    {
        return &m_privateDataRWLock;
    }

    VkResult SetDebugUtilsObjectName(const VkDebugUtilsObjectNameInfoEXT* pNameInfo);

    uint32_t GetBorderColorIndex(
        const float*             pBorderColor);

    void ReleaseBorderColorIndex(
        uint32_t                 pBorderColor);

    VK_INLINE Pal::IBorderColorPalette* GetPalBorderColorPalette(uint32_t deviceIdx) const
    {
        return m_perGpu[deviceIdx].pPalBorderColorPalette;
    }

    VkResult CreateInternalComputePipeline(
        size_t                         codeByteSize,
        const uint8_t*                 pCode,
        uint32_t                       numUserDataNodes,
        Vkgc::ResourceMappingRootNode* pUserDataNodes,
        VkShaderModuleCreateFlags      flags,
        bool                           forceWave64,
        const VkSpecializationInfo*    pSpecializationInfo,
        InternalPipeline*              pInternalPipeline);

protected:
    Device(
        uint32_t                         deviceCount,
        PhysicalDevice**                 pPhysicalDevices,
        Pal::IDevice**                   pPalDevices,
        const DeviceBarrierPolicy&       barrierPolicy,
        const DeviceExtensions::Enabled& enabledExtensions,
        const VkPhysicalDeviceFeatures*  pFeatures,
        bool                             useComputeAsTransferQueue,
        uint32                           privateDataSlotRequestCount,
        size_t                           privateDataSize);

    VkResult CreateInternalPipelines();

    void DestroyInternalPipeline(InternalPipeline* pPipeline);

    VkResult CreateBltMsaaStates();
    void DestroyInternalPipelines();
    void InitSamplePatternPalette(Pal::SamplePatternPalette* pPalette) const;

    VkResult InitSwCompositing(uint32_t deviceIdx);

    VkResult AllocBorderColorPalette();

    void DestroyBorderColorPalette();

    Instance* const                     m_pInstance;
    const RuntimeSettings&              m_settings;

    uint32_t                            m_palDeviceCount;

    Properties                          m_properties;

    InternalMemMgr                      m_internalMemMgr;

    ShaderOptimizer                     m_shaderOptimizer;

    ResourceOptimizer                   m_resourceOptimizer;

    RenderStateCache                    m_renderStateCache;

    DispatchableQueue*                  m_pQueues[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily];

    InternalPipeline                    m_timestampQueryCopyPipeline;

    InternalPipeline                    m_internalRayTracingPipeline;

    static const uint32_t BltMsaaStateCount = 4;

    Pal::IMsaaState*                    m_pBltMsaaState[BltMsaaStateCount][MaxPalDevices];

    const DeviceBarrierPolicy           m_barrierPolicy;           // Barrier policy to use for this device

    const DeviceExtensions::Enabled     m_enabledExtensions;       // Enabled device extensions
    DispatchTable                       m_dispatchTable;           // Device dispatch table
    SqttMgr*                            m_pSqttMgr;                // Manager for developer mode SQ thread tracing
    AsyncLayer*                         m_pAsyncLayer;             // State for async compiler layer, otherwise null
    OptLayer*                           m_pAppOptLayer;            // State for an app-specific layer, otherwise null
    BarrierFilterLayer*                 m_pBarrierFilterLayer;     // State for enabling barrier filtering, otherwise
                                                                   // null

    Util::Mutex                         m_memoryMutex;             // Shared mutex used occasionally by memory objects

    // The states of m_enabledFeatures are provided by application
    DeviceFeatures                      m_enabledFeatures;

    // The count of allocations that has been created from the logical device.
    uint32_t                            m_allocatedCount;

    // The maximum allocations that can be created from the logical device
    uint32_t                            m_maxAllocations;

    // Determines if the allocated memory size will be tracked (error will be thrown when
    // allocation exceeds threshold size)
    bool                                m_allocationSizeTracking;

    // Determines if overallocation requested specifically via extension
    bool                                m_overallocationRequestedForPalHeap[static_cast<uint32_t>(Pal::GpuHeap::GpuHeapCount)];

    // If set to true, will use a compute queue internally for transfers.
    bool                                m_useComputeAsTransferQueue;

    VkExtent2D                          m_maxVrsShadingRate;

    struct PerGpuInfo
    {
        PhysicalDevice*           pPhysicalDevice;
        Pal::IDevice*             pPalDevice;
        Pal::ICmdAllocator*       pSharedPalCmdAllocator;

        void*                     pSwCompositingMemory;    // Internal memory for the below PAL objects (master and slave)
        Pal::IQueue*              pSwCompositingQueue;     // Internal present queue (master) or transfer queue (slave)
        Pal::IQueueSemaphore*     pSwCompositingSemaphore; // Internal semaphore (master and slave)
        Pal::ICmdBuffer*          pSwCompositingCmdBuffer; // Internal dummy command buffer for flip metadata (master)
        Pal::IBorderColorPalette* pPalBorderColorPalette;  // Pal border color palette for custom border color.

    };

    // Compute size required for the object.  One copy of PerGpuInfo is included in the object and we need
    // to add space for any additional GPUs.
    static size_t ObjectSize(size_t baseClassSize, uint32_t numDevices)
    {
        return baseClassSize + ((numDevices - 1) * sizeof(PerGpuInfo));
    }

    // This is from device create info, VkDevicePrivateDataCreateInfoEXT
    uint32                              m_privateDataSlotRequestCount;
    volatile uint64                     m_nextPrivateDataSlot;
    size_t                              m_privateDataSize;
    Util::RWLock                        m_privateDataRWLock;

    InternalMemory                      m_memoryPalBorderColorPalette;
    bool*                               m_pBorderColorUsedIndexes;
    Util::Mutex                         m_borderColorMutex;

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

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2(
    VkDevice                                    device,
    const VkRenderPassCreateInfo2*              pCreateInfo,
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

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFdKHR(
    VkDevice                                    device,
    const VkImportSemaphoreFdInfoKHR*           pImportSemaphoreFdInfo);
#endif

VKAPI_ATTR VkResult VKAPI_CALL vkSetGpaDeviceClockModeAMD(
    VkDevice                                    device,
    VkGpaDeviceClockModeInfoAMD*                pInfo);

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupport(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    VkDescriptorSetLayoutSupport*               pSupport);

VKAPI_ATTR VkResult VKAPI_CALL vkGetCalibratedTimestampsEXT(
    VkDevice                                    device,
    uint32_t                                    timestampCount,
    const VkCalibratedTimestampInfoEXT*         pTimestampInfos,
    uint64_t*                                   pTimestamps,
    uint64_t*                                   pMaxDeviation);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreCounterValue(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    uint64_t*                                   pValue);

VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(
    VkDevice                                    device,
    const VkSemaphoreWaitInfo*                  pWaitInfo,
    uint64_t                                    timeout);

VKAPI_ATTR VkResult VKAPI_CALL vkSignalSemaphore(
    VkDevice                                    device,
    const VkSemaphoreSignalInfo*                pSignalInfo);

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

VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    lineStippleFactor,
    uint16_t                                    lineStipplePattern);

} // namespace entry

} // namespace vk

#endif /* __VK_DEVICE_H__ */
