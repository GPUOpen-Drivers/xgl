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
#include "palUuid.h"

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
class ApiPhysicalDevice;
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
// Shader stage bit that represents all graphics stages
static const uint32 ShaderStageAllGraphics = VK_SHADER_STAGE_TASK_BIT_EXT |
                                             VK_SHADER_STAGE_MESH_BIT_EXT |
                                             VK_SHADER_STAGE_ALL_GRAPHICS;

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

    void GetDeviceProperties(VkPhysicalDeviceProperties* pProperties) const;

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

    uint32_t GetMemoryTypeMask() const
    {
        return m_memoryTypeMask;
    }

    uint32_t GetMemoryTypeMaskMatching(VkMemoryPropertyFlags flags) const;

    uint32_t GetMemoryTypeMaskForExternalSharing() const
    {
        return m_memoryTypeMaskForExternalSharing;
    }

    uint32_t GetMemoryTypeMaskForDescriptorBuffers() const
    {
        return m_memoryTypeMaskForDescriptorBuffers;
    }

    bool GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkIndexBits) const
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

    Pal::GpuHeap GetPalHeapFromVkTypeIndex(uint32_t vkIndex) const
    {
        VK_ASSERT(vkIndex < m_memoryProperties.memoryTypeCount);
        return m_memoryVkIndexToPalHeap[vkIndex];
    }

    Pal::GpuHeap GetPalHeapFromVkHeapIndex(uint32_t heapIndex) const
    {
        VK_ASSERT(heapIndex < m_memoryProperties.memoryHeapCount);
        return m_heapVkToPal[heapIndex];
    }

    bool GetVkHeapIndexFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkHeapIndex) const
    {
        VK_ASSERT(heapIndex < Pal::GpuHeapCount);

        *pVkHeapIndex = m_memoryPalHeapToVkHeap[heapIndex];

        return *pVkHeapIndex != Pal::GpuHeapCount;
    }

    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
    {
        return m_memoryProperties;
    }

    Pal::QueueType GetQueueFamilyPalQueueType(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palQueueType;
    }

    uint32_t GetQueueFamilyIndexByPalQueueType(
        Pal::QueueType queueType) const
    {
        uint32_t index = 0;

        for (uint32_t i = 0; i < Queue::MaxQueueFamilies; i++)
        {
            if (m_queueFamilies[i].palQueueType == queueType)
            {
                index = i;
                break;
            }
        }

        return index;
    }

    Pal::EngineType GetQueueFamilyPalEngineType(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palEngineType;
    }

    uint32_t GetCompQueueEngineIndex(
        const uint32_t queueIndex) const
    {
        return m_compQueueEnginesNdx[queueIndex];
    }

    uint32_t GetUniversalQueueEngineIndex(
        const uint32_t queueIndex) const
    {
        return m_universalQueueEnginesNdx[queueIndex];
    }

    uint32_t GetQueueFamilyPalImageLayoutFlag(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].palImageLayoutFlag;
    }

    VkShaderStageFlags GetValidShaderStages(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].validShaderStages;
    }

    const VkQueueFamilyProperties& GetQueueFamilyProperties(
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

    size_t GetFeatures(VkPhysicalDeviceFeatures* pFeatures) const;

    VkResult GetFormatProperties(
        VkFormat            format,
        VkFormatProperties* pFormatProperties) const
    {
        uint32_t formatIndex = Formats::GetIndex(format);

        *pFormatProperties = m_formatFeaturesTable[formatIndex];

        return VK_SUCCESS;
    }

    VkResult GetExtendedFormatProperties(
        VkFormat                format,
        VkFormatProperties3KHR* pFormatProperties) const;

    bool FormatSupportsMsaa(VkFormat format) const
    {
        uint32_t formatIndex = Formats::GetIndex(format);

        return Util::WideBitfieldIsSet(m_formatFeatureMsaaTarget, formatIndex);
    }

    void GetPhysicalDeviceIDProperties(
        uint8_t*                 pDeviceUUID,
        uint8_t*                 pDriverUUID,
        uint8_t*                 pDeviceLUID,
        uint32_t*                pDeviceNodeMask,
        VkBool32*                pDeviceLUIDValid) const;

    void GetPhysicalDeviceMaintenance3Properties(
        uint32_t*                pMaxPerSetDescriptors,
        VkDeviceSize*            pMaxMemoryAllocationSize) const;

    void GetPhysicalDeviceMultiviewProperties(
        uint32_t*                pMaxMultiviewViewCount,
        uint32_t*                pMaxMultiviewInstanceIndex) const;

    void GetPhysicalDevicePointClippingProperties(
        VkPointClippingBehavior* pPointClippingBehavior) const;

    void GetPhysicalDeviceProtectedMemoryProperties(
        VkBool32*                pProtectedNoFault) const;

    void GetPhysicalDeviceSubgroupProperties(
        uint32_t*                pSubgroupSize,
        VkShaderStageFlags*      pSupportedStages,
        VkSubgroupFeatureFlags*  pSupportedOperations,
        VkBool32*                pQuadOperationsInAllStages) const;

    void GetPhysicalDeviceSubgroupSizeControlProperties(
        uint32_t*           pMinSubgroupSize,
        uint32_t*           pMaxSubgroupSize,
        uint32_t*           pMaxComputeWorkgroupSubgroups,
        VkShaderStageFlags* pQuadOperationsInAllStages) const;

    void GetPhysicalDeviceUniformBlockProperties(
        uint32_t* pMaxInlineUniformBlockSize,
        uint32_t* pMaxPerStageDescriptorInlineUniformBlocks,
        uint32_t* pMaxPerStageDescriptorUpdateAfterBindInlineUniformBlocks,
        uint32_t* pMaxDescriptorSetInlineUniformBlocks,
        uint32_t* pMaxDescriptorSetUpdateAfterBindInlineUniformBlocks) const;

    void GetPhysicalDeviceDotProduct8Properties(
        VkBool32* pIntegerDotProduct8BitUnsignedAccelerated,
        VkBool32* pIntegerDotProduct8BitSignedAccelerated,
        VkBool32* pIntegerDotProduct8BitMixedSignednessAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating8BitUnsignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating8BitSignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated) const;

    void GetPhysicalDeviceDotProduct4x8Properties(
        VkBool32* pIntegerDotProduct4x8BitPackedUnsignedAccelerated,
        VkBool32* pIntegerDotProduct4x8BitPackedSignedAccelerated,
        VkBool32* pIntegerDotProduct4x8BitPackedMixedSignednessAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated) const;

    void GetPhysicalDeviceDotProduct16Properties(
        VkBool32* pIntegerDotProduct16BitUnsignedAccelerated,
        VkBool32* pIntegerDotProduct16BitSignedAccelerated,
        VkBool32* pIntegerDotProduct16BitMixedSignednessAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating16BitUnsignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating16BitSignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated) const;

    void GetPhysicalDeviceDotProduct32Properties(
        VkBool32* pIntegerDotProduct32BitUnsignedAccelerated,
        VkBool32* pIntegerDotProduct32BitSignedAccelerated,
        VkBool32* pIntegerDotProduct32BitMixedSignednessAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating32BitUnsignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating32BitSignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated) const;

    void GetPhysicalDeviceDotProduct64Properties(
        VkBool32* pIntegerDotProduct64BitUnsignedAccelerated,
        VkBool32* pIntegerDotProduct64BitSignedAccelerated,
        VkBool32* pIntegerDotProduct64BitMixedSignednessAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating64BitUnsignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating64BitSignedAccelerated,
        VkBool32* pIntegerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated) const;

    void GetPhysicalDeviceTexelBufferAlignmentProperties(
        VkDeviceSize* pStorageTexelBufferOffsetAlignmentBytes,
        VkBool32*     pStorageTexelBufferOffsetSingleTexelAlignment,
        VkDeviceSize* pUniformTexelBufferOffsetAlignmentBytes,
        VkBool32*     pUniformTexelBufferOffsetSingleTexelAlignment) const;

    void GetDevicePropertiesMaxBufferSize(
        VkDeviceSize* pMaxBufferSize) const;

    void GetPhysicalDeviceDriverProperties(
        VkDriverId*              pDriverID,
        char*                    pDriverName,
        char*                    pDriverInfo,
        VkConformanceVersion*    pConformanceVersion) const;

    template<typename T>
    void GetPhysicalDeviceFloatControlsProperties(
        T                        pFloatControlsProperties) const;

    template<typename T>
    void GetPhysicalDeviceDescriptorIndexingProperties(
        T                        pDescriptorIndexingProperties) const;

    void GetPhysicalDeviceDepthStencilResolveProperties(
        VkResolveModeFlags*      pSupportedDepthResolveModes,
        VkResolveModeFlags*      pSupportedStencilResolveModes,
        VkBool32*                pIndependentResolveNone,
        VkBool32*                pIndependentResolve) const;

    void GetPhysicalDeviceSamplerFilterMinmaxProperties(
        VkBool32*                pFilterMinmaxSingleComponentFormats,
        VkBool32*                pFilterMinmaxImageComponentMapping) const;

    void GetPhysicalDeviceTimelineSemaphoreProperties(
        uint64_t*                pMaxTimelineSemaphoreValueDifference) const;

    VkResult GetExternalMemoryProperties(
        bool                               isSparse,
        bool                               isImageUsage,
        VkExternalMemoryHandleTypeFlagBits handleType,
        VkExternalMemoryProperties*        pExternalMemoryProperties) const;

#if defined(__unix__)
template <typename ModifierPropertiesList_T>
    VkResult GetDrmFormatModifierPropertiesList(
        VkFormat                 format,
        ModifierPropertiesList_T pPropertiesList) const;
#endif

    VkResult GetImageFormatProperties(
        VkFormat                 format,
        VkImageType              type,
        VkImageTiling            tiling,
        VkImageUsageFlags        usage,
        VkImageCreateFlags       flags,
#if defined(__unix__)
        uint64                   modifier,
#endif
        VkImageFormatProperties* pImageFormatProperties) const;

    void GetSparseImageFormatProperties(
        VkFormat                                        format,
        VkImageType                                     type,
        VkSampleCountFlagBits                           samples,
        VkImageUsageFlags                               usage,
        VkImageTiling                                   tiling,
        uint32_t*                                       pPropertyCount,
        utils::ArrayView<VkSparseImageFormatProperties> properties) const;

    void GetPhysicalDevice16BitStorageFeatures(
        VkBool32* pStorageBuffer16BitAccess,
        VkBool32* pUniformAndStorageBuffer16BitAccess,
        VkBool32* pStoragePushConstant16,
        VkBool32* pStorageInputOutput16) const;

    void GetPhysicalDeviceMultiviewFeatures(
        VkBool32* pMultiview,
        VkBool32* pMultiviewGeometryShader,
        VkBool32* pMultiviewTessellationShader) const;

    void GetPhysicalDeviceVariablePointerFeatures(
        VkBool32* pVariablePointersStorageBuffer,
        VkBool32* pVariablePointers) const;

    void GetPhysicalDeviceProtectedMemoryFeatures(
        VkBool32* pProtectedMemory) const;

    void GetPhysicalDeviceSamplerYcbcrConversionFeatures(
        VkBool32* pSamplerYcbcrConversion) const;

    void GetPhysicalDeviceShaderDrawParameterFeatures(
        VkBool32* pShaderDrawParameters) const;

    void GetPhysicalDevice8BitStorageFeatures(
        VkBool32* pStorageBuffer8BitAccess,
        VkBool32* pUniformAndStorageBuffer8BitAccess,
        VkBool32* pStoragePushConstant8) const;

    void GetPhysicalDeviceShaderAtomicInt64Features(
        VkBool32* pShaderBufferInt64Atomics,
        VkBool32* pShaderSharedInt64Atomics) const;

    void GetPhysicalDeviceFloat16Int8Features(
        VkBool32* pShaderFloat16,
        VkBool32* pShaderInt8) const;

    void GetPhysicalDeviceMutableDescriptorTypeFeatures(
        VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT* pMutableDescriptorTypeFeatures) const;

    template<typename T>
    void GetPhysicalDeviceDescriptorIndexingFeatures(
        T         pDescriptorIndexingFeatures) const;

    void GetPhysicalDeviceScalarBlockLayoutFeatures(
        VkBool32* pScalarBlockLayout) const;

    void GetPhysicalDeviceImagelessFramebufferFeatures(
        VkBool32* pImagelessFramebuffer) const;

    void GetPhysicalDeviceUniformBufferStandardLayoutFeatures(
        VkBool32* pUniformBufferStandardLayout) const;

    void GetPhysicalDeviceSubgroupExtendedTypesFeatures(
        VkBool32* pShaderSubgroupExtendedTypes) const;

    void GetPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
        VkBool32* pSeparateDepthStencilLayouts) const;

    void GetPhysicalDeviceHostQueryResetFeatures(
        VkBool32* pHostQueryReset) const;

    void GetPhysicalDeviceTimelineSemaphoreFeatures(
        VkBool32* pTimelineSemaphore) const;

    void GetPhysicalDeviceBufferAddressFeatures(
        VkBool32* pBufferDeviceAddress,
        VkBool32* pBufferDeviceAddressCaptureReplay,
        VkBool32* pBufferDeviceAddressMultiDevice) const;

    void GetPhysicalDeviceVulkanMemoryModelFeatures(
        VkBool32* pVulkanMemoryModel,
        VkBool32* pVulkanMemoryModelDeviceScope,
        VkBool32* pVulkanMemoryModelAvailabilityVisibilityChains) const;

    VkResult GetPhysicalDeviceCalibrateableTimeDomainsEXT(
        uint32_t*                           pTimeDomainCount,
        VkTimeDomainEXT*                    pTimeDomains);

    VkResult GetPhysicalDeviceToolPropertiesEXT(
        uint32_t*                           pToolCount,
        VkPhysicalDeviceToolPropertiesEXT*  pToolProperties);

    void GetExternalBufferProperties(
        const VkPhysicalDeviceExternalBufferInfo*   pExternalBufferInfo,
        VkExternalBufferProperties*                 pExternalBufferProperties);

    void GetSparseImageFormatProperties2(
        const VkPhysicalDeviceSparseImageFormatInfo2*   pFormatInfo,
        uint32_t*                                       pPropertyCount,
        VkSparseImageFormatProperties2*                 pProperties);

    size_t GetFeatures2(
        VkStructHeaderNonConst*                     pFeatures,
        bool                                        updateFeatures) const;

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

    VK_FORCEINLINE bool IsComputeEngineSupported() const
    {
        return (m_properties.engineProperties[Pal::EngineTypeCompute].engineCount != 0);
    }

    const RuntimeSettings& GetRuntimeSettings() const
    {
        return m_pSettingsLoader->GetSettings();
    }

    VulkanSettingsLoader* GetSettingsLoader() const
    {
        return m_pSettingsLoader;
    }

    const VkPhysicalDeviceLimits& GetLimits() const
    {
        return m_limits;
    }

    uint32_t GetVrHighPrioritySubEngineIndex() const
    {
        return m_vrHighPrioritySubEngineIndex;
    }

    uint32_t GetRtCuHighComputeSubEngineIndex() const
    {
        return m_RtCuHighComputeSubEngineIndex;
    }

    uint32_t GetTunnelComputeSubEngineIndex() const
    {
        return m_tunnelComputeSubEngineIndex;
    }

    uint32_t GetTunnelPrioritySupport() const
    {
        return m_tunnelPriorities;
    }

    uint32_t GetSubgroupSize() const
    {
        uint32_t subgroupSize = m_properties.gfxipProperties.shaderCore.maxWavefrontSize;

        const RuntimeSettings& settings = GetRuntimeSettings();
        if (settings.subgroupSize != 0)
        {
            subgroupSize = settings.subgroupSize;
        }
        return subgroupSize;
    }

    bool IsPrtSupportedOnDmaEngine() const
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

    uint32_t GetPipelineCacheExpectedEntryCount();
    void DecreasePipelineCacheCount();

    uint32_t GetNumberOfSupportedShadingRates(
        uint32 supportedVrsRates) const;

    VkResult GetFragmentShadingRates(
        uint32*                                 pFragmentShadingRateCount,
        VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates);

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

#if VKI_RAY_TRACING
    bool HwSupportsRayTracing() const;
#endif

    static DeviceExtensions::Supported GetAvailableExtensions(
        const Instance*       pInstance,
        const PhysicalDevice* pPhysicalDevice);

    const DeviceExtensions::Supported& GetSupportedExtensions() const
        { return m_supportedExtensions; }

    const DeviceExtensions::Supported& GetAllowedExtensions() const
        { return m_allowedExtensions; }

    const DeviceExtensions::Supported& GetIgnoredExtensions() const
        { return m_ignoredExtensions; }

    bool IsExtensionSupported(DeviceExtensions::ExtensionId id) const
        { return m_supportedExtensions.IsExtensionSupported(id); }

    bool IsExtensionSupported(InstanceExtensions::ExtensionId id) const
        { return VkInstance()->IsExtensionSupported(id); }

    uint32_t GetSupportedAPIVersion() const;

    uint32_t GetEnabledAPIVersion() const
    {
        return Util::Min(GetSupportedAPIVersion(), VkInstance()->GetAPIVersion());
    }

    AppProfile GetAppProfile() const
        { return m_appProfile; }

    const PhysicalDeviceGpaProperties& GetGpaProperties() const
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

    bool IsOverrideHeapChoiceToLocalWithinBudget(Pal::gpusize size) const;

    Util::IPlatformKey* GetPlatformKey() const { return m_pPlatformKey; }

    Pal::WorkstationStereoMode GetWorkstationStereoMode() const { return m_workstationStereoMode; }

    // Only Active and Passive stereo are supported at the moment.
    bool IsWorkstationStereoEnabled() const;

    bool IsAutoStereoEnabled() const;

protected:
    PhysicalDevice(PhysicalDeviceManager* pPhysicalDeviceManager,
                   Pal::IDevice*          pPalDevice,
                   VulkanSettingsLoader*  pSettingsLoader,
                   AppProfile             appProfile);

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
    uint32_t                         m_memoryTypeMaskForExternalSharing;
    uint32_t                         m_memoryPalHeapToVkIndexBits[Pal::GpuHeapCount];
    uint32_t                         m_memoryPalHeapToVkHeap[Pal::GpuHeapCount];
    Pal::GpuHeap                     m_memoryVkIndexToPalHeap[VK_MAX_MEMORY_TYPES];
    Pal::GpuHeap                     m_heapVkToPal[VkMemoryHeapNum];
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    uint32_t                         m_memoryTypeMaskForDescriptorBuffers;

    VulkanSettingsLoader*            m_pSettingsLoader;
    VkPhysicalDeviceLimits           m_limits;
    VkSampleCountFlags               m_sampleLocationSampleCounts;
    VkFormatProperties               m_formatFeaturesTable[VK_SUPPORTED_FORMAT_COUNT];
    uint32_t                         m_formatFeatureMsaaTarget[Util::RoundUpQuotient(
                                                                    static_cast<uint32_t>(VK_SUPPORTED_FORMAT_COUNT),
                                                                    static_cast<uint32_t>(sizeof(uint32_t) << 3))];
    uint32_t                         m_vrHighPrioritySubEngineIndex;
    uint32_t                         m_RtCuHighComputeSubEngineIndex;
    uint32_t                         m_tunnelComputeSubEngineIndex;
    uint32_t                         m_tunnelPriorities;
    uint32_t                         m_queueFamilyCount;

    // Record pipeline caches count created on this device. Note this may be dropped once there isn't any test creating
    // excessive pipeline caches.
    volatile uint32_t                m_pipelineCacheCount;

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
    bool                             m_eqaaSupported;

    DeviceExtensions::Supported      m_supportedExtensions;
    DeviceExtensions::Supported      m_allowedExtensions;
    DeviceExtensions::Supported      m_ignoredExtensions;

    // Device properties related to the VK_AMD_gpu_perf_api_interface extension
    PhysicalDeviceGpaProperties      m_gpaProps;

    PipelineCompiler                 m_compiler;
    Pal::WorkstationStereoMode       m_workstationStereoMode;

    struct
    {
        Util::Mutex  trackerMutex;                                    // Mutex for memory usage tracking
        Pal::gpusize allocatedMemorySize[Pal::GpuHeap::GpuHeapCount]; // Number of bytes allocated per heap
        Pal::gpusize totalMemorySize[Pal::GpuHeap::GpuHeapCount];     // The total memory (in bytes) per heap
    } m_memoryUsageTracker;

    Util::Uuid::Uuid                 m_pipelineCacheUUID;

    Util::IPlatformKey*              m_pPlatformKey;             // Platform identifying key

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PhysicalDevice);
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

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceToolProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pToolCount,
    VkPhysicalDeviceToolPropertiesEXT*          pToolProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceFragmentShadingRatesKHR(
    VkPhysicalDevice                        physicalDevice,
    uint32*                                 pFragmentShadingRateCount,
    VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates);

} // namespace entry

} // namespace vk

#endif /* __VK_PHYSICAL_DEVICE_H__ */
