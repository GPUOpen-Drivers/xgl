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
#if VKI_RAY_TRACING
#include "include/vk_deferred_operation.h"
#endif

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

#if VKI_GPU_DECOMPRESS
#include "imported/gputexdecoder/gpuTexDecoder.h"
#endif

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
class Device;
class ApiDevice;
class ApiQueue;
class Instance;
class OptLayer;
class PhysicalDevice;
class Queue;
class SqttMgr;
class SwapChain;
class ChillMgr;
class AsyncLayer;
#if VKI_GPU_DECOMPRESS
class GpuDecoderLayer;
#endif

#if VKI_RAY_TRACING
class RayTracingDevice;
#endif

// =====================================================================================================================
// Specifies properties for importing a semaphore, it's an encapsulation of VkImportSemaphoreFdInfoKHR and
// VkImportSemaphoreWin32HandleInfoKHR. Please refer to the vkspec for the defination of members.
struct ImportSemaphoreInfo
{
    VkExternalSemaphoreHandleTypeFlagBits handleType;
    Pal::OsExternalHandle                 handle;
    VkSemaphoreImportFlags                importFlags;
    bool                                  crossProcess;
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

    union DeviceFeatures
    {
        struct
        {
            uint32                robustBufferAccess                   : 1;
            uint32                sparseBinding                        : 1;
            // The state of enabled feature VK_EXT_scalar_block_layout.
            uint32                scalarBlockLayout                    : 1;
            // Attachment Fragment Shading Rate feature in VK_KHR_variable_rate_shading
            uint32                attachmentFragmentShadingRate        : 1;
            // The states of enabled feature DEVICE_COHERENT_MEMORY_FEATURES_AMD which is defined by
            // extensions VK_AMD_device_coherent_memory
            uint32                deviceCoherentMemory                 : 1;
            // The state of enabled features in VK_EXT_robustness2.
            uint32                robustBufferAccessExtended           : 1;
            uint32                robustImageAccessExtended            : 1;
            uint32                nullDescriptorExtended               : 1;
            // True if EXT_MEMORY_PRIORITY or EXT_PAGEABLE_DEVICE_LOCAL_MEMORY is enabled.
            uint32                appControlledMemPriority             : 1;
            uint32                mustWriteImmutableSamplers           : 1;
            uint32                strictImageSizeRequirements          : 1;
            uint32                dynamicPrimitiveTopologyUnrestricted : 1;
            uint32                graphicsPipelineLibrary              : 1;
            uint32                deviceMemoryReport                   : 1;
            uint32                deviceAddressBindingReport           : 1;
            // True if EXT_DEVICE_MEMORY_REPORT or EXT_DEVICE_ADDRESS_BINDING_REPORT is enabled.
            uint32                gpuMemoryEventHandler                : 1;
            uint32                assumeDynamicTopologyInLibs          : 1;
            uint32                reserved                             : 15;
        };

        uint32 u32All;
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
            uint32_t alignmentInDwords;
        } descriptorSizes;

        struct
        {
            size_t colorTargetView;
            size_t depthStencilView;
        } palSizes;

#if VKI_RAY_TRACING
        Pal::RayTracingIpLevel rayTracingIpLevel;
#endif
        uint32_t               timestampQueryPoolSlotSize;
        bool                   connectThroughThunderBolt;
    };

    static VkResult Create(
        PhysicalDevice*                             pPhysicalDevice,
        const VkDeviceCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        ApiDevice**                                 ppDevice);

    VkResult Destroy(const VkAllocationCallbacks*   pAllocator);

    VkResult WaitIdle(void);

    VkResult AllocMemory(
        const VkMemoryAllocateInfo*                 pAllocInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDeviceMemory*                             pMem);

    VkQueue GetQueue(
        Pal::EngineType                             engineType,
        Pal::QueueType                              queueType);

    void GetQueue(
        uint32_t                                    queueFamilyIndex,
        uint32_t                                    queueIndex,
        VkQueue*                                    pQueue);

    void GetQueue2(
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
        ApiQueue**                                  pQueues,
        const DeviceExtensions::Enabled&            enabled,
        const VkMemoryOverallocationBehaviorAMD     overallocationBehavior,
        bool                                        bufferDeviceAddressMultiDeviceEnabled,
        bool                                        pageableDeviceLocalMemory,
        const VkAllocationCallbacks*                pAllocator);

    void InitDispatchTable();

    VK_FORCEINLINE Instance* VkInstance() const
        { return m_pInstance; }

    VK_FORCEINLINE InternalMemMgr* MemMgr()
        { return &m_internalMemMgr; }

    VK_FORCEINLINE ShaderOptimizer* GetShaderOptimizer()
        { return &m_shaderOptimizer; }

    VK_FORCEINLINE const ShaderOptimizer* GetShaderOptimizer() const
        { return &m_shaderOptimizer; }

    VK_FORCEINLINE ResourceOptimizer* GetResourceOptimizer()
        { return &m_resourceOptimizer; }

    VK_FORCEINLINE const ResourceOptimizer* GetResourceOptimizer() const
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

    VK_FORCEINLINE const Pal::DeviceProperties& GetPalProperties() const
        { return VkPhysicalDevice(DefaultDeviceIndex)->PalProperties(); }

#if VKI_RAY_TRACING
    uint32_t GetMaxLdsForTargetOccupancy(float targetOccupancyPerSimd) const;
    uint32_t GetDefaultLdsSizePerThread(bool isIndirect) const;
    uint32_t GetDefaultLdsTraversalStackSize(bool isIndirect) const;
    uint32_t ClampLdsStackSizeFromThreadGroupSize(uint32_t ldsStackSize) const;
#endif

    Pal::QueueType GetQueueFamilyPalQueueType(
        uint32_t queueFamilyIndex) const;

    Pal::EngineType GetQueueFamilyPalEngineType(
        uint32_t queueFamilyIndex) const;

    uint32_t GetQueueFamilyPalImageLayoutFlag(
        uint32_t queueFamilyIndex) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalImageLayoutFlag(queueFamilyIndex);
    }

    uint32_t GetMemoryTypeMask() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMask();
    }

    uint32_t GetMemoryTypeMaskMatching(VkMemoryPropertyFlags flags) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskMatching(flags);
    }

    uint32_t GetMemoryTypeMaskForDescriptorBuffers() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskForDescriptorBuffers();
    }

    uint32_t GetMemoryTypeMaskForExternalSharing() const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryTypeMaskForExternalSharing();
    }

    bool GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkIndexBits) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetVkTypeIndexBitsFromPalHeap(heapIndex, pVkIndexBits);
    }

    Pal::GpuHeap GetPalHeapFromVkTypeIndex(uint32_t vkIndex) const
    {
        return VkPhysicalDevice(DefaultDeviceIndex)->GetPalHeapFromVkTypeIndex(vkIndex);
    }

    uint64_t TimestampFrequency() const
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
        const VkBindImageMemoryInfo* pBindInfos);

    const DeviceFeatures& GetEnabledFeatures() const
        { return m_enabledFeatures; }

    bool IsGlobalGpuVaEnabled() const
        { return m_useGlobalGpuVa; }

    Pal::PrtFeatureFlags GetPrtFeatures() const;

    Pal::Result AddMemReference(
        Pal::IDevice*       pPalDevice,
        Pal::IGpuMemory*    pPalMemory,
        bool                readOnly = false);

    void RemoveMemReference(
        Pal::IDevice*       pPalDevice,
        Pal::IGpuMemory*    pPalMemory);

    const RuntimeSettings& GetRuntimeSettings() const
        { return m_settings; }

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

    bool OverallocationRequestedForPalHeap(
        uint32_t palHeapIdx) const
    {
        return m_overallocationRequestedForPalHeap[palHeapIdx];
    }

    const InternalPipeline& GetTimestampQueryCopyPipeline() const
        { return m_timestampQueryCopyPipeline; }

#if VKI_RAY_TRACING
    InternalPipeline& GetInternalRayTracingPipeline()
    {
        return m_internalRayTracingPipeline;
    }

    const InternalPipeline& GetInternalAccelerationStructureQueryCopyPipeline() const
    {
        return m_accelerationStructureQueryCopyPipeline;
    }
#endif

    inline const Pal::IMsaaState* const * GetBltMsaaState(uint32_t imgSampleCount) const;

    bool IsExtensionEnabled(DeviceExtensions::ExtensionId id) const
        { return m_enabledExtensions.IsExtensionEnabled(id); }

    AppProfile GetAppProfile() const
        { return VkPhysicalDevice(DefaultDeviceIndex)->GetAppProfile(); }

    SqttMgr* GetSqttMgr()
        { return m_pSqttMgr; }

    OptLayer* GetAppOptLayer()
        { return m_pAppOptLayer; }

    BarrierFilterLayer* GetBarrierFilterLayer()
        { return m_pBarrierFilterLayer; }

#if VKI_GPU_DECOMPRESS
    GpuDecoderLayer* GetGpuDecoderLayer()
        { return m_pGpuDecoderLayer; }

    InternalPipeline& GetInternalTexDecodePipeline()
        {    return m_internalTexDecodePipeline; }
#endif

    Util::Mutex* GetMemoryMutex()
        { return &m_memoryMutex; }

    PipelineCompiler* GetCompiler(uint32_t idx) const
        { return m_perGpu[idx].pPhysicalDevice->GetCompiler(); }

    static const Pal::MsaaQuadSamplePattern* GetDefaultQuadSamplePattern(uint32_t sampleCount);
    static uint32_t GetDefaultSamplePatternIndex(uint32_t sampleCount);

    VkDeviceSize GetMemoryBaseAddrAlignment(uint32_t memoryTypes) const;

    RenderStateCache* GetRenderStateCache()
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

    bool IsAllocationSizeTrackingEnabled() const
        { return m_allocationSizeTracking; }

    bool UseComputeAsTransfer() const
        { return m_useComputeAsTransferQueue; }

    bool UseStridedCopyQueryResults() const
        { return (m_properties.timestampQueryPoolSlotSize == 32); }

    bool UseCompactDynamicDescriptors() const
    {
        return (
                !GetRuntimeSettings().enableRelocatableShaders &&
                !GetEnabledFeatures().robustBufferAccess);
    }

    bool MustWriteImmutableSamplers() const
        { return GetEnabledFeatures().mustWriteImmutableSamplers; }

    bool SupportDepthStencilResolve() const
    {
        return (IsExtensionEnabled(DeviceExtensions::KHR_DEPTH_STENCIL_RESOLVE) ||
                (VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion() >= VK_MAKE_API_VERSION(0, 1, 2, 0)) ||
                m_settings.forceResolveLayoutForDepthStencilTransferUsage);
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

#if VKI_RAY_TRACING
    VkResult CreateDeferredOperation(
        const VkAllocationCallbacks* pAllocator,
        VkDeferredOperationKHR*      pDeferredOperation);
#endif

#if VKI_RAY_TRACING
    RayTracingDevice* RayTrace() const { return m_pRayTrace; }

    VkResult CreateRayTracingPipelines(
        VkDeferredOperationKHR                      deferredOperation,
        VkPipelineCache                             pipelineCache,
        uint32_t                                    count,
        const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
        const VkAllocationCallbacks*                pAllocator,
        VkPipeline*                                 pPipelines);

    VkResult CopyAccelerationStructure(
        VkDeferredOperationKHR                    deferredOperation,
        const VkCopyAccelerationStructureInfoKHR* pInfo);

    VkResult CopyAccelerationStructureToMemory(
        VkDeferredOperationKHR                            deferredOperation,
        const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo);

     VkResult CopyMemoryToAccelerationStructure(
         VkDeferredOperationKHR                            deferredOperation,
         const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo);

    VkResult CreateAccelerationStructureKHR(
        const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkAccelerationStructureKHR*                 pAccelerationStructure);

    VkResult BuildAccelerationStructure(
        VkDeferredOperationKHR                                  deferredOperation,
        uint32_t                                                infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos);

    void GetDeviceAccelerationStructureCompatibility(
        const uint8_t*                                      pData,
        VkAccelerationStructureCompatibilityKHR*            pCompatibility);

    VkResult WriteAccelerationStructuresProperties(
        uint32_t                                    accelerationStructureCount,
        const VkAccelerationStructureKHR*           pAccelerationStructures,
        VkQueryType                                 queryType,
        size_t                                      dataSize,
        void*                                       pData,
        size_t                                      stride);

    void GetAccelerationStructureBuildSizesKHR(
        VkAccelerationStructureBuildTypeKHR                  buildType,
        const VkAccelerationStructureBuildGeometryInfoKHR*   pBuildInfo,
        const uint32_t*                                      pMaxPrimitiveCounts,
        VkAccelerationStructureBuildSizesInfoKHR*            pSizeInfo);

#endif

    VK_FORCEINLINE VkExtent2D GetMaxVrsShadingRate() const
    {
        return m_maxVrsShadingRate;
    }

    VK_FORCEINLINE PipelineBinningMode GetPipelineBinningMode() const
    {
        return m_pipelineBinningMode;
    }

    VK_FORCEINLINE void SetPipelineBinningMode(PipelineBinningMode mode)
    {
        m_pipelineBinningMode = mode;
    }

    size_t GetPrivateDataSize() const
    {
        return m_privateDataSize;
    }

    bool ReserveFastPrivateDataSlot(
        uint64*                         pIndex);

    void* AllocApiObject(
        const VkAllocationCallbacks*    pAllocator,
        const size_t                    totalObjectSize) const;

    void FreeApiObject(
        const VkAllocationCallbacks*    pAllocator,
        void*                           pMemory) const;

    void FreeUnreservedPrivateData(
        void*                           pMemory) const;

    Util::RWLock* GetPrivateDataRWLock()
    {
        return &m_privateDataRWLock;
    }

    VkResult SetDebugUtilsObjectName(const VkDebugUtilsObjectNameInfoEXT* pNameInfo);

    uint32_t GetBorderColorIndex(
        const float*             pBorderColor);

    void ReleaseBorderColorIndex(
        uint32_t                 pBorderColor);

    void ReserveBorderColorIndex(
        uint32                   borderColorIndex,
        const float*             pBorderColor);

    Pal::IBorderColorPalette* GetPalBorderColorPalette(uint32_t deviceIdx) const
    {
        return m_perGpu[deviceIdx].pPalBorderColorPalette;
    }

    VkResult CreateInternalComputePipeline(
        size_t                         codeByteSize,
        const uint8_t*                 pCode,
        uint32_t                       numUserDataNodes,
        Vkgc::ResourceMappingRootNode* pUserDataNodes,
        VkShaderModuleCreateFlags      internalShaderFlags,
        bool                           forceWave64,
        const VkSpecializationInfo*    pSpecializationInfo,
        InternalPipeline*              pInternalPipeline);

    Pal::TilingOptMode GetTilingOptMode() const;

    VkResult GetDeviceFaultInfoEXT(
        VkDeviceFaultCountsEXT* pFaultCounts,
        VkDeviceFaultInfoEXT*   pFaultInfo);

    const PipelineLayout* GetNullPipelineLayout() const { return m_pNullPipelineLayout; }

    template<typename CreateInfo>
    static PipelineCreateFlags GetPipelineCreateFlags(
        const CreateInfo* pCreateInfo);

    static BufferUsageFlagBits GetBufferUsageFlagBits(
        const VkBufferCreateInfo* pCreateInfo);

protected:
    Device(
        uint32_t                         deviceCount,
        PhysicalDevice**                 pPhysicalDevices,
        Pal::IDevice**                   pPalDevices,
        const VkDeviceCreateInfo*        pCreateInfo,
        const DeviceExtensions::Enabled& enabledExtensions,
        const VkPhysicalDeviceFeatures*  pFeatures,
        bool                             useComputeAsTransferQueue,
        uint32                           privateDataSlotRequestCount,
        size_t                           privateDataSize,
        const DeviceFeatures&            deviceFeatures);

    VkResult CreateInternalPipelines();

    void DestroyInternalPipeline(InternalPipeline* pPipeline);

    VkResult CreateBltMsaaStates();
    void DestroyInternalPipelines();
#if VKI_RAY_TRACING
    VkResult CreateRayTraceState();
#endif
    void InitSamplePatternPalette(Pal::SamplePatternPalette* pPalette) const;

    VkResult InitSwCompositing(uint32_t deviceIdx);

    VkResult CreateSharedPalCmdAllocator(
        );
    void     DestroySharedPalCmdAllocator();

    VkResult AllocBorderColorPalette();
    void     DestroyBorderColorPalette();

    Instance* const                     m_pInstance;
    const RuntimeSettings&              m_settings;

    uint32_t                            m_palDeviceCount;

    Properties                          m_properties;

    InternalMemMgr                      m_internalMemMgr;

    ShaderOptimizer                     m_shaderOptimizer;

    PipelineBinningMode                 m_pipelineBinningMode;

    ResourceOptimizer                   m_resourceOptimizer;

    RenderStateCache                    m_renderStateCache;

    ApiQueue*                           m_pQueues[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily];

    InternalPipeline                    m_timestampQueryCopyPipeline;

#if VKI_RAY_TRACING
    InternalPipeline                    m_internalRayTracingPipeline;
    InternalPipeline                    m_accelerationStructureQueryCopyPipeline;
#endif

    static const uint32_t BltMsaaStateCount = 4;

    Pal::IMsaaState*                    m_pBltMsaaState[BltMsaaStateCount][MaxPalDevices];

    const DeviceBarrierPolicy           m_barrierPolicy;           // Barrier policy to use for this device

    const DeviceExtensions::Enabled     m_enabledExtensions;       // Enabled device extensions
    DispatchTable                       m_dispatchTable;           // Device dispatch table
    SqttMgr*                            m_pSqttMgr;                // Manager for developer mode SQ thread tracing
    OptLayer*                           m_pAppOptLayer;            // State for an app-specific layer, otherwise null
    BarrierFilterLayer*                 m_pBarrierFilterLayer;     // State for enabling barrier filtering, otherwise
                                                                   // null

#if VKI_GPU_DECOMPRESS
    GpuDecoderLayer*                     m_pGpuDecoderLayer;
    InternalPipeline                     m_internalTexDecodePipeline;
#endif

    Util::Mutex                         m_memoryMutex;             // Shared mutex used occasionally by memory objects

    // The states of m_enabledFeatures are provided by application
    const DeviceFeatures                m_enabledFeatures;

    // Determines if the allocated memory size will be tracked (error will be thrown when
    // allocation exceeds threshold size)
    bool                                m_allocationSizeTracking;

    // Determines if overallocation requested specifically via extension
    bool                                m_overallocationRequestedForPalHeap[static_cast<uint32_t>(Pal::GpuHeap::GpuHeapCount)];

    // If set to true, will use a compute queue internally for transfers.
    bool                                m_useComputeAsTransferQueue;

    // If set to true, overrides compute queue to universal queue internally
    bool                                m_useUniversalAsComputeQueue;

    // The max VRS shading rate supported
    VkExtent2D                          m_maxVrsShadingRate;

    // If use global GpuVa's should be used in MGPU configurations
    bool                                m_useGlobalGpuVa;

#if VKI_RAY_TRACING
#endif

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

#if VKI_RAY_TRACING
    RayTracingDevice*    m_pRayTrace;
#endif

    // This is from device create info, VkDevicePrivateDataCreateInfoEXT
    uint32                              m_privateDataSlotRequestCount;
    volatile uint64                     m_nextPrivateDataSlot;
    size_t                              m_privateDataSize;
    Util::RWLock                        m_privateDataRWLock;

    InternalMemory                      m_memoryPalBorderColorPalette;
    bool*                               m_pBorderColorUsedIndexes;
    Util::Mutex                         m_borderColorMutex;

    bool                                m_retrievedFaultData;
    Pal::PageFaultStatus                m_pageFaultStatus;

    // Null pipeline layout for pipeline Library. We can use this if the pipeline library create info doesn't have
    // a pipeline layout specified.
    PipelineLayout* m_pNullPipelineLayout;

    // This goes last.  The memory for the rest of the array is calculated dynamically based on the number of GPUs in
    // use.
    PerGpuInfo              m_perGpu[1];

    // NOTE: Please don't add anything here. m_perGpu[1] must be the last.

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
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

#if VKI_RAY_TRACING
VKAPI_ATTR VkResult VKAPI_CALL vkCreateAccelerationStructureKHR(
    VkDevice                                    device,
    const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkAccelerationStructureKHR*                 pAccelerationStructure);

VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureKHR(
    VkDevice                                    device,
    VkAccelerationStructureKHR                  accelerationStructure,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkBuildAccelerationStructuresKHR(
    VkDevice                                                device,
    VkDeferredOperationKHR                                  deferredOperation,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos);

VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      deferredOperation,
    const VkCopyAccelerationStructureInfoKHR*   pInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureToMemoryKHR(
    VkDevice                                          device,
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkCopyMemoryToAccelerationStructureKHR(
    VkDevice                                          device,
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      deferredOperation,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines);

VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingShaderGroupHandlesKHR(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkWriteAccelerationStructuresPropertiesKHR(
    VkDevice                                    device,
    uint32_t                                    accelerationStructureCount,
    const VkAccelerationStructureKHR*           pAccelerationStructures,
    VkQueryType                                 queryType,
    size_t                                      dataSize,
    void*                                       pData,
    size_t                                      stride);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceAccelerationStructureCompatibilityKHR(
    VkDevice                                     device,
    const VkAccelerationStructureVersionInfoKHR* pVersionInfo,
    VkAccelerationStructureCompatibilityKHR*     pCompatibility);

VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetAccelerationStructureDeviceAddressKHR(
    VkDevice                                           device,
    const VkAccelerationStructureDeviceAddressInfoKHR* pInfo);

VKAPI_ATTR VkDeviceSize VKAPI_CALL vkGetRayTracingShaderGroupStackSizeKHR(
    VkDevice                                           device,
    VkPipeline                                         pipeline,
    uint32_t                                           group,
    VkShaderGroupShaderKHR                             groupShader);

VKAPI_ATTR void VKAPI_CALL vkGetAccelerationStructureBuildSizesKHR(
    VkDevice                                           device,
    VkAccelerationStructureBuildTypeKHR                buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo,
    const uint32_t*                                    pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR*          pSizeInfo);
#endif

#if VKI_RAY_TRACING
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDeferredOperationKHR(
    VkDevice                                    device,
    const VkAllocationCallbacks*                pAllocator,
    VkDeferredOperationKHR*                     pDeferredOperation);

VKAPI_ATTR void VKAPI_CALL vkDestroyDeferredOperationKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeferredOperationResultKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation);

VKAPI_ATTR uint32_t VKAPI_CALL vkGetDeferredOperationMaxConcurrencyKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation);

VKAPI_ATTR VkResult VKAPI_CALL vkDeferredOperationJoinKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation);
#endif

VKAPI_ATTR void VKAPI_CALL vkGetDeviceBufferMemoryRequirements(
    VkDevice                                    device,
    const VkDeviceBufferMemoryRequirementsKHR*  pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirements(
    VkDevice                                    device,
    const VkDeviceImageMemoryRequirementsKHR*   pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageSparseMemoryRequirements(
    VkDevice                                    device,
    const VkDeviceImageMemoryRequirementsKHR*   pInfo,
    uint32_t*                                   pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2*           pSparseMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    lineStippleFactor,
    uint16_t                                    lineStipplePattern);

VKAPI_ATTR void VKAPI_CALL vkSetDeviceMemoryPriorityEXT(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    float                                       priority);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceFaultInfoEXT(
    VkDevice                                    device,
    VkDeviceFaultCountsEXT*                     pFaultCounts,
    VkDeviceFaultInfoEXT*                       pFaultInfo);

} // namespace entry

} // namespace vk

#endif /* __VK_DEVICE_H__ */
