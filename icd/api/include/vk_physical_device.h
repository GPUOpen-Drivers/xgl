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
 * @file  vk_physical_device.h
 * @brief Definition of physical device class for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_PHYSICAL_DEVICE_H__
#define __VK_PHYSICAL_DEVICE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_physical_device_manager.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "include/vk_extensions.h"
#include "include/vk_formats.h"
#include "include/vk_queue.h"
#include "include/pipeline_compiler.h"
#include "settings/settings.h"

#include "palDevice.h"
#include "palInlineFuncs.h"
#include "palQueue.h"

namespace Pal
{

enum WsiPlatform : uint32;
// Forward declare PAL classes used in this file.
class IDevice;

} // namespace Pal

namespace Util
{
class IPlatformKey;
} // namespace Util

namespace vk
{

// Forward declare Vulkan classes used in this file.
class DispatchablePhysicalDevice;
class Surface;

// =====================================================================================================================
// Relevant window system information decoded from a VkSurfaceKHR
struct DisplayableSurfaceInfo
{
    VkIcdWsiPlatform     icdPlatform;
    Pal::OsDisplayHandle displayHandle;
    Pal::OsWindowHandle  windowHandle;
    Pal::WsiPlatform     palPlatform;
    VkExtent2D           surfaceExtent;
    Pal::IScreen*        pScreen;
};

// =====================================================================================================================
// Properties relevant for the VK_AMD_gpu_perf_api_interface extension
struct PhysicalDeviceGpaProperties
{
    VkPhysicalDeviceGpaPropertiesAMD properties;
    VkPhysicalDeviceGpaFeaturesAMD   features;
    Pal::PerfExperimentProperties    palProps;
};

// =====================================================================================================================
// Represents the Vulkan view of physical device. All Vulkan functions on the VkPhysicalDevice land in
// this class. The class wraps a PAL IDevice and punts most functionality down to the next layer.
class PhysicalDevice
{
public:
    typedef VkPhysicalDevice ApiType;

    static VkResult UnpackDisplayableSurface(
        Surface*                pSurface,
        DisplayableSurfaceInfo* pSurfaceInfo);

    static VkResult Create(
        PhysicalDeviceManager*  pPhysicalDeviceManager,
        Pal::IDevice*           pPalDevice,
        VulkanSettingsLoader*   pSettingsLoader,
        AppProfile              appProfile,
        VkPhysicalDevice*       pPhysicalDevice);

    VkResult Destroy(void);

    VkResult CreateDevice(
        const VkDeviceCreateInfo*       pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkDevice*                       pDevice);

    VkResult GetDeviceProperties(VkPhysicalDeviceProperties* pProperties) const;

    void GetDeviceGpaProperties(
        VkPhysicalDeviceGpaPropertiesAMD* pGpaProperties) const;

    void GetExternalSemaphoreProperties(
        const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo,
        VkExternalSemaphoreProperties*               pExternalSemaphoreProperties);

    void GetExternalFenceProperties(
        const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo,
        VkExternalFenceProperties*               pExternalFenceProperties);

    void PopulateQueueFamilies();
    void PopulateFormatProperties();

    VK_INLINE uint32_t GetMemoryTypeMask() const
    {
        return m_memoryTypeMask;
    }

    VK_INLINE uint32_t GetMemoryTypeForAttachmentImage() const
    {
        return m_memoryVkIndexAttachmentImage;
    }

    VK_INLINE bool GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkIndexBits) const
    {
        VK_ASSERT(heapIndex < Pal::GpuHeapCount);
        VK_ASSERT(pVkIndexBits != nullptr);
        *pVkIndexBits = m_memoryPalHeapToVkIndexBits[static_cast<uint32_t>(heapIndex)];
        if (*pVkIndexBits == 0)
        {
            return false;
        }
        else
        {
            VK_ASSERT(((*pVkIndexBits) & ~((1ULL << VK_MAX_MEMORY_TYPES) - 1)) == 0);
            return true;
        }
    }

    VK_INLINE Pal::GpuHeap GetPalHeapFromVkTypeIndex(uint32_t vkIndex) const
    {
        VK_ASSERT(vkIndex < m_memoryProperties.memoryTypeCount);
        return m_memoryVkIndexToPalHeap[vkIndex];
    }

    VK_INLINE Pal::GpuHeap GetPalHeapFromVkHeapIndex(uint32_t heapIndex) const
    {
        VK_ASSERT(heapIndex < m_memoryProperties.memoryHeapCount);
        return m_heapVkToPal[heapIndex];
    }

    VK_INLINE bool GetVkHeapIndexFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkHeapIndex) const
    {
        VK_ASSERT(heapIndex < Pal::GpuHeapCount);

        *pVkHeapIndex = m_memoryPalHeapToVkHeap[heapIndex];

        return *pVkHeapIndex != Pal::GpuHeapCount;
    }

    VK_INLINE const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
    {
        return m_memoryProperties;
    }

    VK_INLINE Pal::QueueType GetQueueFamilyPalQueueType(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palQueueType;
    }

    VK_INLINE Pal::EngineType GetQueueFamilyPalEngineType(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palEngineType;
    }

    VK_INLINE uint32_t GetCompQueueEngineIndex(
        const uint32_t queueIndex) const
    {
        return m_compQueueEnginesNdx[queueIndex];
    }

    VK_INLINE uint32_t GetUniversalQueueEngineIndex(
        const uint32_t queueIndex) const
    {
        return m_universalQueueEnginesNdx[queueIndex];
    }

    VK_INLINE uint32_t GetQueueFamilyPalImageLayoutFlag(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palImageLayoutFlag;
    }

    VK_INLINE const VkShaderStageFlags GetValidShaderStages(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].validShaderStages;
    }

    VK_INLINE const VkQueueFamilyProperties& GetQueueFamilyProperties(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].properties;
    }

    VkResult GetQueueFamilyProperties(
        uint32_t*                pCount,
        VkQueueFamilyProperties* pQueueProperties) const;

    VkResult GetQueueFamilyProperties(
        uint32_t*                 pCount,
        VkQueueFamilyProperties2* pQueueProperties) const;

    VkResult GetFeatures(VkPhysicalDeviceFeatures* pFeatures) const;

    VK_INLINE VkResult GetFormatProperties(
        VkFormat            format,
        VkFormatProperties* pFormatProperties) const
    {
        uint32_t formatIndex = Formats::GetIndex(format);

        *pFormatProperties = m_formatFeaturesTable[formatIndex];

        return VK_SUCCESS;
    }

    VK_INLINE bool FormatSupportsMsaa(VkFormat format) const
    {
        uint32_t formatIndex = Formats::GetIndex(format);

        return Util::WideBitfieldIsSet(m_formatFeatureMsaaTarget, formatIndex);
    }

    VK_INLINE void GetPhysicalDeviceIDProperties(
        uint8_t*                 pDeviceUUID,
        uint8_t*                 pDriverUUID,
        uint8_t*                 pDeviceLUID,
        uint32_t*                pDeviceNodeMask,
        VkBool32*                pDeviceLUIDValid) const;

    VK_INLINE void GetPhysicalDeviceMaintenance3Properties(
        uint32_t*                pMaxPerSetDescriptors,
        VkDeviceSize*            pMaxMemoryAllocationSize) const;

    VK_INLINE void GetPhysicalDeviceMultiviewProperties(
        uint32_t*                pMaxMultiviewViewCount,
        uint32_t*                pMaxMultiviewInstanceIndex) const;

    VK_INLINE void GetPhysicalDevicePointClippingProperties(
        VkPointClippingBehavior* pPointClippingBehavior) const;

    VK_INLINE void GetPhysicalDeviceProtectedMemoryProperties(
        VkBool32*                pProtectedNoFault) const;

    VK_INLINE void GetPhysicalDeviceSubgroupProperties(
        uint32_t*                pSubgroupSize,
        VkShaderStageFlags*      pSupportedStages,
        VkSubgroupFeatureFlags*  pSupportedOperations,
        VkBool32*                pQuadOperationsInAllStages) const;

    VK_INLINE void GetPhysicalDeviceDriverProperties(
        VkDriverIdKHR*           pDriverID,
        char*                    pDriverName,
        char*                    pDriverInfo,
        VkConformanceVersionKHR* pConformanceVersion) const;

    template<typename T>
    VK_INLINE void GetPhysicalDeviceFloatControlsProperties(
        T                        pFloatControlsProperties) const;

    template<typename T>
    VK_INLINE void GetPhysicalDeviceDescriptorIndexingProperties(
        T                        pDescriptorIndexingProperties) const;

    VK_INLINE void GetPhysicalDeviceDepthStencilResolveProperties(
        VkResolveModeFlagsKHR*   pSupportedDepthResolveModes,
        VkResolveModeFlagsKHR*   pSupportedStencilResolveModes,
        VkBool32*                pIndependentResolveNone,
        VkBool32*                pIndependentResolve) const;

    VK_INLINE void GetPhysicalDeviceSamplerFilterMinmaxProperties(
        VkBool32*                pFilterMinmaxSingleComponentFormats,
        VkBool32*                pFilterMinmaxImageComponentMapping) const;

    VK_INLINE void GetPhysicalDeviceTimelineSemaphoreProperties(
        uint64_t*                pMaxTimelineSemaphoreValueDifference) const;

    VkResult GetExternalMemoryProperties(
        bool                               isSparse,
        VkExternalMemoryHandleTypeFlagBits handleType,
        VkExternalMemoryProperties*        pExternalMemoryProperties) const;

    VkResult GetImageFormatProperties(
        VkFormat                 format,
        VkImageType              type,
        VkImageTiling            tiling,
        VkImageUsageFlags        usage,
        VkImageCreateFlags       flags,
        VkImageFormatProperties* pImageFormatProperties) const;

    void GetSparseImageFormatProperties(
        VkFormat                                        format,
        VkImageType                                     type,
        VkSampleCountFlagBits                           samples,
        VkImageUsageFlags                               usage,
        VkImageTiling                                   tiling,
        uint32_t*                                       pPropertyCount,
        utils::ArrayView<VkSparseImageFormatProperties> properties) const;

    VK_INLINE void GetPhysicalDevice16BitStorageFeatures(
        VkBool32* pStorageBuffer16BitAccess,
        VkBool32* pUniformAndStorageBuffer16BitAccess,
        VkBool32* pStoragePushConstant16,
        VkBool32* pStorageInputOutput16) const;

    VK_INLINE void GetPhysicalDeviceMultiviewFeatures(
        VkBool32* pMultiview,
        VkBool32* pMultiviewGeometryShader,
        VkBool32* pMultiviewTessellationShader) const;

    VK_INLINE void GetPhysicalDeviceVariablePointerFeatures(
        VkBool32* pVariablePointersStorageBuffer,
        VkBool32* pVariablePointers) const;

    VK_INLINE void GetPhysicalDeviceProtectedMemoryFeatures(
        VkBool32* pProtectedMemory) const;

    VK_INLINE void GetPhysicalDeviceSamplerYcbcrConversionFeatures(
        VkBool32* pSamplerYcbcrConversion) const;

    VK_INLINE void GetPhysicalDeviceShaderDrawParameterFeatures(
        VkBool32* pShaderDrawParameters) const;

    VK_INLINE void GetPhysicalDevice8BitStorageFeatures(
        VkBool32* pStorageBuffer8BitAccess,
        VkBool32* pUniformAndStorageBuffer8BitAccess,
        VkBool32* pStoragePushConstant8) const;

    VK_INLINE void GetPhysicalDeviceShaderAtomicInt64Features(
        VkBool32* pShaderBufferInt64Atomics,
        VkBool32* pShaderSharedInt64Atomics) const;

    VK_INLINE void GetPhysicalDeviceFloat16Int8Features(
        VkBool32* pShaderFloat16,
        VkBool32* pShaderInt8) const;

    template<typename T>
    VK_INLINE void GetPhysicalDeviceDescriptorIndexingFeatures(
        T         pDescriptorIndexingFeatures) const;

    VK_INLINE void GetPhysicalDeviceScalarBlockLayoutFeatures(
        VkBool32* pScalarBlockLayout) const;

    VK_INLINE void GetPhysicalDeviceImagelessFramebufferFeatures(
        VkBool32* pImagelessFramebuffer) const;

    VK_INLINE void GetPhysicalDeviceUniformBufferStandardLayoutFeatures(
        VkBool32* pUniformBufferStandardLayout) const;

    VK_INLINE void GetPhysicalDeviceSubgroupExtendedTypesFeatures(
        VkBool32* pShaderSubgroupExtendedTypes) const;

    VK_INLINE void GetPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
        VkBool32* pSeparateDepthStencilLayouts) const;

    VK_INLINE void GetPhysicalDeviceHostQueryResetFeatures(
        VkBool32* pHostQueryReset) const;

    VK_INLINE void GetPhysicalDeviceTimelineSemaphoreFeatures(
        VkBool32* pTimelineSemaphore) const;

    VK_INLINE void GetPhysicalDeviceBufferAddressFeatures(
        VkBool32* pBufferDeviceAddress,
        VkBool32* pBufferDeviceAddressCaptureReplay,
        VkBool32* pBufferDeviceAddressMultiDevice) const;

    VK_INLINE void GetPhysicalDeviceVulkanMemoryModelFeatures(
        VkBool32* pVulkanMemoryModel,
        VkBool32* pVulkanMemoryModelDeviceScope,
        VkBool32* pVulkanMemoryModelAvailabilityVisibilityChains) const;

    VkResult GetPhysicalDeviceCalibrateableTimeDomainsEXT(
        uint32_t*                           pTimeDomainCount,
        VkTimeDomainEXT*                    pTimeDomains);

    void GetExternalBufferProperties(
        const VkPhysicalDeviceExternalBufferInfo*   pExternalBufferInfo,
        VkExternalBufferProperties*                 pExternalBufferProperties);

    void GetSparseImageFormatProperties2(
        const VkPhysicalDeviceSparseImageFormatInfo2*   pFormatInfo,
        uint32_t*                                       pPropertyCount,
        VkSparseImageFormatProperties2*                 pProperties);

    void GetFeatures2(
        VkStructHeaderNonConst*                     pFeatures) const;

    void GetMemoryProperties2(
        VkPhysicalDeviceMemoryProperties2*          pMemoryProperties);

    void GetFormatProperties2(
        VkFormat                                    format,
        VkFormatProperties2*                        pFormatProperties);

    void GetDeviceProperties2(
        VkPhysicalDeviceProperties2*                pProperties);

    VkResult GetImageFormatProperties2(
        const VkPhysicalDeviceImageFormatInfo2*     pImageFormatInfo,
        VkImageFormatProperties2*                   pImageFormatProperties);

    void GetDeviceMultisampleProperties(
        VkSampleCountFlagBits                       samples,
        VkMultisamplePropertiesEXT*                 pMultisampleProperties);

    bool QueueSupportsPresents(
        uint32_t         queueFamilyIndex,
        VkIcdWsiPlatform platform) const;

    template< typename T >
    VkResult GetSurfaceCapabilities(
        VkSurfaceKHR         surface,
        Pal::OsDisplayHandle displayHandle,
        T                    pSurfaceCapabilities) const;

    VkResult GetSurfaceCapabilities2KHR(
        const VkPhysicalDeviceSurfaceInfo2KHR*  pSurfaceInfo,
        VkSurfaceCapabilities2KHR*              pSurfaceCapabilities) const;

    VkResult GetSurfacePresentModes(
        const DisplayableSurfaceInfo& displayable,
        Pal::PresentMode              presentType,
        uint32_t*                     pPresentModeCount,
        VkPresentModeKHR*             pPresentModes) const;

    VkResult GetSurfaceFormats(
        Surface*             pSurface,
        Pal::OsDisplayHandle osDisplayHandle,
        uint32_t*            pSurfaceFormatCount,
        VkSurfaceFormatKHR*  pSurfaceFormats) const;

    VkResult GetSurfaceFormats(
        Surface*             pSurface,
        Pal::OsDisplayHandle osDisplayHandle,
        uint32_t*            pSurfaceFormatCount,
        VkSurfaceFormat2KHR* pSurfaceFormats) const;

    VkResult GetPhysicalDevicePresentRectangles(
        VkSurfaceKHR                                surface,
        uint32_t*                                   pRectCount,
        VkRect2D*                                   pRects);

    VkBool32 DeterminePresentationSupported(
        Pal::OsDisplayHandle     hDisplay,
        VkIcdWsiPlatform         platform,
        int64_t                  visualId,
        uint32_t                 queueFamilyIndex);

    VK_FORCEINLINE PhysicalDeviceManager* Manager() const
    {
        VK_ASSERT(m_pPhysicalDeviceManager != nullptr);
        return m_pPhysicalDeviceManager;
    }

    VK_FORCEINLINE Instance* VkInstance() const
    {
        VK_ASSERT(m_pPhysicalDeviceManager != nullptr);
        return m_pPhysicalDeviceManager->VkInstance();
    }

    VK_FORCEINLINE Pal::IDevice* PalDevice() const
    {
        VK_ASSERT(m_pPalDevice != nullptr);
        return m_pPalDevice;
    }

    VK_FORCEINLINE const Pal::DeviceProperties& PalProperties() const
    {
        return m_properties;
    }

    VK_FORCEINLINE Pal::PrtFeatureFlags GetPrtFeatures() const
    {
        return m_properties.imageProperties.prtFeatures;
    }

    VK_FORCEINLINE bool IsVirtualRemappingSupported() const
    {
        return m_properties.gpuMemoryProperties.flags.virtualRemappingSupport;
    }

    VK_INLINE const RuntimeSettings& GetRuntimeSettings() const
    {
        return m_pSettingsLoader->GetSettings();
    }

    VK_INLINE VulkanSettingsLoader* GetSettingsLoader() const
    {
        return m_pSettingsLoader;
    }

    VK_INLINE const VkPhysicalDeviceLimits& GetLimits() const
    {
        return m_limits;
    }

    VK_INLINE uint32_t GetVrHighPrioritySubEngineIndex() const
    {
        return m_vrHighPrioritySubEngineIndex;
    }

    VK_INLINE uint32_t GetRtCuHighComputeSubEngineIndex() const
    {
        return m_RtCuHighComputeSubEngineIndex;
    }

    VK_INLINE uint32_t GetSubgroupSize() const
    {
        uint32_t subgroupSize = m_properties.gfxipProperties.shaderCore.maxWavefrontSize;

        const RuntimeSettings& settings = GetRuntimeSettings();
        if (settings.subgroupSize != 0)
        {
            subgroupSize = settings.subgroupSize;
        }
        return subgroupSize;
    }

    VK_INLINE bool IsPrtSupportedOnDmaEngine() const
    {
        return m_prtOnDmaSupported;
    }

    VkResult GetDisplayProperties(
        uint32_t*                                   pPropertyCount,
        utils::ArrayView<VkDisplayPropertiesKHR>    properties);

    VkResult GetDisplayPlaneProperties(
        uint32_t*                                       pPropertyCount,
        utils::ArrayView<VkDisplayPlanePropertiesKHR>   properties);

    VkResult GetDisplayPlaneSupportedDisplays(
        uint32_t                                    planeIndex,
        uint32_t*                                   pDisplayCount,
        VkDisplayKHR*                               pDisplays);

    VkResult GetDisplayModeProperties(
        VkDisplayKHR                                    display,
        uint32_t*                                       pPropertyCount,
        utils::ArrayView<VkDisplayModePropertiesKHR>    properties);

    VkResult CreateDisplayMode(
            VkDisplayKHR                                display,
            const VkDisplayModeCreateInfoKHR*           pCreateInfo,
            const VkAllocationCallbacks*                pAllocator,
            VkDisplayModeKHR*                           pMode);

    VkResult GetDisplayPlaneCapabilities(
            VkDisplayModeKHR                            mode,
            uint32_t                                    planeIndex,
            VkDisplayPlaneCapabilitiesKHR*              pCapabilities);

    VkResult EnumerateExtensionProperties(
        const char*                 pLayerName,
        uint32_t*                   pPropertyCount,
        VkExtensionProperties*      pProperties) const;

    VkResult GetSurfaceCapabilities2EXT(
        VkSurfaceKHR                surface,
        VkSurfaceCapabilities2EXT*  pSurfaceCapabilitiesExt) const;

    void GetMemoryBudgetProperties(
        VkPhysicalDeviceMemoryBudgetPropertiesEXT* pMemBudgetProps);

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    VkResult AcquireXlibDisplay(
        Display*        dpy,
        VkDisplayKHR    display);

    VkResult GetRandROutputDisplay(
        Display*        dpy,
        uint32_t        randrOutput,
        VkDisplayKHR*   pDisplay);
#endif

    VkResult ReleaseDisplay(VkDisplayKHR display);
#endif

    static DeviceExtensions::Supported GetAvailableExtensions(
        const Instance*       pInstance,
        const PhysicalDevice* pPhysicalDevice);

    VK_INLINE const DeviceExtensions::Supported& GetSupportedExtensions() const
        { return m_supportedExtensions; }

    VK_INLINE const DeviceExtensions::Supported& GetAllowedExtensions() const
        { return m_allowedExtensions; }

    VK_INLINE bool IsExtensionSupported(DeviceExtensions::ExtensionId id) const
        { return m_supportedExtensions.IsExtensionSupported(id); }

    VK_INLINE bool IsExtensionSupported(InstanceExtensions::ExtensionId id) const
        { return VkInstance()->IsExtensionSupported(id); }

    uint32_t GetSupportedAPIVersion() const;

    VK_INLINE uint32_t GetEnabledAPIVersion() const
    {
        return Util::Min(GetSupportedAPIVersion(), VkInstance()->GetAPIVersion());
    }

    VK_INLINE AppProfile GetAppProfile() const
        { return m_appProfile; }

    VK_INLINE const PhysicalDeviceGpaProperties& GetGpaProperties() const
        { return m_gpaProps; }

    void LateInitialize();

    VK_FORCEINLINE PipelineCompiler* GetCompiler()
    {
        return &m_compiler;
    }

    VkResult TryIncreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     heapIdx);

    void IncreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     heapIdx);

    void DecreaseAllocatedMemorySize(
        Pal::gpusize allocationSize,
        uint32_t     heapIdx);

    VK_INLINE bool ShouldAddRemoteBackupHeap(uint32_t vkIndex) const
        { return m_memoryVkIndexAddRemoteBackupHeap[vkIndex]; }

    Util::IPlatformKey* GetPlatformKey() const { return m_pPlatformKey; }

protected:
    PhysicalDevice(PhysicalDeviceManager* pPhysicalDeviceManager,
                   Pal::IDevice*          pPalDevice,
                   VulkanSettingsLoader*  pSettingsLoader,
                   AppProfile             appProfile
                   );

    VkResult Initialize();
    void PopulateLimits();
    void PopulateExtensions();
    void PopulateGpaProperties();

    void InitializePlatformKey(const RuntimeSettings& settings);

    VK_FORCEINLINE bool IsPerChannelMinMaxFilteringSupported() const
    {
        return m_properties.gfxipProperties.flags.supportPerChannelMinMaxFilter;
    }

    PhysicalDeviceManager*           m_pPhysicalDeviceManager;
    Pal::IDevice*                    m_pPalDevice;
    Pal::DeviceProperties            m_properties;

    uint32_t                         m_memoryTypeMask;
    uint32_t                         m_memoryVkIndexAttachmentImage;
    bool                             m_memoryVkIndexAddRemoteBackupHeap[VK_MAX_MEMORY_TYPES];
    uint32_t                         m_memoryPalHeapToVkIndexBits[Pal::GpuHeapCount];
    uint32_t                         m_memoryPalHeapToVkHeap[Pal::GpuHeapCount];
    Pal::GpuHeap                     m_memoryVkIndexToPalHeap[VK_MAX_MEMORY_TYPES];
    Pal::GpuHeap                     m_heapVkToPal[VkMemoryHeapNum];
    VkPhysicalDeviceMemoryProperties m_memoryProperties;

    VulkanSettingsLoader*            m_pSettingsLoader;
    VkPhysicalDeviceLimits           m_limits;
    VkSampleCountFlags               m_sampleLocationSampleCounts;
    VkFormatProperties               m_formatFeaturesTable[VK_SUPPORTED_FORMAT_COUNT];
    uint32_t                         m_formatFeatureMsaaTarget[Util::RoundUpQuotient(
                                                                    static_cast<uint32_t>(VK_SUPPORTED_FORMAT_COUNT),
                                                                    static_cast<uint32_t>(sizeof(uint32_t) << 3))];
    uint32_t                         m_vrHighPrioritySubEngineIndex;
    uint32_t                         m_RtCuHighComputeSubEngineIndex;
    uint32_t                         m_queueFamilyCount;
    struct
    {
        Pal::QueueType               palQueueType;
        Pal::EngineType              palEngineType;
        VkShaderStageFlags           validShaderStages;
        uint32_t                     palImageLayoutFlag;
        VkQueueFamilyProperties      properties;
    } m_queueFamilies[Queue::MaxQueueFamilies];

    // List of indices for compute engines that aren't exclusive.
    uint32_t m_compQueueEnginesNdx[Queue::MaxQueuesPerFamily];

    // List of indices for universal engines that aren't exclusive.
    uint32_t m_universalQueueEnginesNdx[Queue::MaxQueuesPerFamily];

    const AppProfile                 m_appProfile;
    bool                             m_prtOnDmaSupported;

    DeviceExtensions::Supported      m_supportedExtensions;
    DeviceExtensions::Supported      m_allowedExtensions;

    // Device properties related to the VK_AMD_gpu_perf_api_interface extension
    PhysicalDeviceGpaProperties      m_gpaProps;

    PipelineCompiler                 m_compiler;

    struct
    {
        Util::Mutex  trackerMutex;                                    // Mutex for memory usage tracking
        Pal::gpusize allocatedMemorySize[Pal::GpuHeap::GpuHeapCount]; // Number of bytes allocated per heap
        Pal::gpusize totalMemorySize[Pal::GpuHeap::GpuHeapCount];     // The total memory (in bytes) per heap
    } m_memoryUsageTracker;

    uint8_t                          m_pipelineCacheUUID[VK_UUID_SIZE];

    Util::IPlatformKey*              m_pPlatformKey;             // Platform identifying key
};

VK_DEFINE_DISPATCHABLE(PhysicalDevice);

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice                            physicalDevice,
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    VkSurfaceKHR                                surface,
    VkBool32*                                   pSupported);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkSampleCountFlagBits                       samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pPropertyCount,
    VkSparseImageFormatProperties*              pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures2*                  pFeatures);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties2*                pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties2*                        pFormatProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2*     pImageFormatInfo,
    VkImageFormatProperties2*                   pImageFormatProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2*                   pQueueFamilyProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties2*          pMemoryProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice                                    physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2*       pFormatInfo,
    uint32_t*                                           pPropertyCount,
    VkSparseImageFormatProperties2*                     pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice                                physicalDevice,
    const VkPhysicalDeviceExternalBufferInfo*       pExternalBufferInfo,
    VkExternalBufferProperties*                     pExternalBufferProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMultisamplePropertiesEXT(
    VkPhysicalDevice                            physicalDevice,
    VkSampleCountFlagBits                       samples,
    VkMultisamplePropertiesEXT*                 pMultisampleProperties);

VKAPI_ATTR void VKAPI_CALL vkTrimCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolTrimFlags                      flags);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilitiesKHR*                   pSurfaceCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceSurfaceInfo2KHR*      pSurfaceInfo,
    VkSurfaceCapabilities2KHR*                  pSurfaceCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceSurfaceInfo2KHR*      pSurfaceInfo,
    uint32_t*                                   pSurfaceFormatCount,
    VkSurfaceFormat2KHR*                        pSurfaceFormats);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pPresentModeCount,
    VkPresentModeKHR*                           pPresentModes);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pSurfaceFormatCount,
    VkSurfaceFormatKHR*                         pSurfaceFormats);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo*    pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties*                  pExternalSemaphoreProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFenceProperties(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo*    pExternalFenceInfo,
    VkExternalFenceProperties*                  pExternalFenceProperties);

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXcbPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    xcb_connection_t*                           connection,
    xcb_visualid_t                              visual_id);
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    Display*                                    dpy,
    VisualID                                    visualID);
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceWaylandPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    struct wl_display*                          display);
#endif

#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireXlibDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    Display*                                    dpy,
    VkDisplayKHR                                display);

VKAPI_ATTR VkResult VKAPI_CALL vkGetRandROutputDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    Display*                                    dpy,
    RROutput                                    rrOutput,
    VkDisplayKHR*                               pDisplay);
#endif

VKAPI_ATTR VkResult VKAPI_CALL vkReleaseDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display);
#endif

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPropertiesKHR*                     pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPlanePropertiesKHR*                pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneSupportedDisplaysKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    planeIndex,
    uint32_t*                                   pDisplayCount,
    VkDisplayKHR*                               pDisplays);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayModePropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    uint32_t*                                   pPropertyCount,
    VkDisplayModePropertiesKHR*                 pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDisplayModeKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    const VkDisplayModeCreateInfoKHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDisplayModeKHR*                           pMode);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneCapabilitiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayModeKHR                            mode,
    uint32_t                                    planeIndex,
    VkDisplayPlaneCapabilitiesKHR*              pCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDisplayPlaneSurfaceKHR(
    VkInstance                                  instance,
    const VkDisplaySurfaceCreateInfoKHR*        pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayProperties2KHR*                    pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPlaneProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPlaneProperties2KHR*               pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayModeProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    uint32_t*                                   pPropertyCount,
    VkDisplayModeProperties2KHR*                pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneCapabilities2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkDisplayPlaneInfo2KHR*               pDisplayPlaneInfo,
    VkDisplayPlaneCapabilities2KHR*             pCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2EXT(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilities2EXT*                  pSurfaceCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pTimeDomainCount,
    VkTimeDomainEXT*                            pTimeDomains);

} // namespace entry

} // namespace vk

#endif /* __VK_PHYSICAL_DEVICE_H__ */
