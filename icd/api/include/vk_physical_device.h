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
        const RuntimeSettings&  settings,
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

    VK_INLINE bool GetVkTypeIndexFromPalHeap(Pal::GpuHeap heapIndex, uint32_t* pVkIndex) const
    {
        VK_ASSERT(heapIndex < Pal::GpuHeapCount);
        VK_ASSERT(pVkIndex != nullptr);
        *pVkIndex = m_memoryPalHeapToVkIndex[static_cast<uint32_t>(heapIndex)];
        if (*pVkIndex >= Pal::GpuHeapCount)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    VK_INLINE Pal::GpuHeap GetPalHeapFromVkTypeIndex(uint32_t vkIndex) const
    {
        VK_ASSERT(vkIndex < VK_MEMORY_TYPE_NUM);
        return m_memoryVkIndexToPalHeap[vkIndex];
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

    void  GetPhysicalDeviceIDProperties(
        uint8_t*            pDeviceUUID,
        uint8_t*            pDriverUUID,
        uint8_t*            pDeviceLUID,
        uint32_t*           pDeviceNodeMask,
        VkBool32*           pDeviceLUIDValid) const;

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

    void GetPhysicalDeviceFeatures2(
        VkPhysicalDeviceFeatures2*                  pFeatures);

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
        VkSurfaceKHR        surface,
        T                   pSurfaceCapabilities) const;

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
        uint32_t*            pSurfaceFormatCount,
        VkSurfaceFormatKHR*  pSurfaceFormats) const;

    VkResult GetSurfaceFormats(
        Surface*             pSurface,
        uint32_t*            pSurfaceFormatCount,
        VkSurfaceFormat2KHR*  pSurfaceFormats) const;

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
        return m_settings;
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
        uint32_t subgroupSize = m_properties.gfxipProperties.shaderCore.wavefrontSize;

        return subgroupSize;
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

    static DeviceExtensions::Supported GetAvailableExtensions(
        const Instance*       pInstance,
        const PhysicalDevice* pPhysicalDevice);

    VK_INLINE const DeviceExtensions::Supported& GetSupportedExtensions() const
        { return m_supportedExtensions; }

    VK_INLINE bool IsExtensionSupported(DeviceExtensions::ExtensionId id) const
        { return m_supportedExtensions.IsExtensionSupported(id); }

    VK_INLINE bool IsExtensionSupported(InstanceExtensions::ExtensionId id) const
        { return VkInstance()->IsExtensionSupported(id); }

    uint32_t GetSupportedAPIVersion() const;

    VK_INLINE uint32_t GetEnabledAPIVersion() const
    {
#if VKI_SDK_1_0 == 0
        return Util::Min(GetSupportedAPIVersion(), VkInstance()->GetAPIVersion());
#else
        return VkInstance()->GetAPIVersion();
#endif
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
protected:
    PhysicalDevice(PhysicalDeviceManager* pPhysicalDeviceManager,
                   Pal::IDevice*          pPalDevice,
                   const RuntimeSettings& settings,
                   AppProfile             appProfile
                   );

    VkResult Initialize();
    void PopulateLimits();
    void PopulateExtensions();
    void PopulateGpaProperties();

    VK_FORCEINLINE bool IsPerChannelMinMaxFilteringSupported() const
    {
        return m_properties.gfxipProperties.flags.supportPerChannelMinMaxFilter;
    }

    PhysicalDeviceManager*           m_pPhysicalDeviceManager;
    Pal::IDevice*                    m_pPalDevice;
    Pal::DeviceProperties            m_properties;
    uint32_t                         m_memoryTypeMask;
    uint32_t                         m_memoryPalHeapToVkIndex[Pal::GpuHeapCount];
    Pal::GpuHeap                     m_memoryVkIndexToPalHeap[Pal::GpuHeapCount];
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    RuntimeSettings                  m_settings;
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

    const AppProfile                 m_appProfile;

    DeviceExtensions::Supported      m_supportedExtensions;

    // Device properties related to the VK_AMD_gpu_perf_api_interface extension
    PhysicalDeviceGpaProperties      m_gpaProps;

    PipelineCompiler                         m_compiler;
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

VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXcbPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    xcb_connection_t*                           connection,
    xcb_visualid_t                              visual_id);

VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    Display*                                    dpy,
    VisualID                                    visualID);
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

} // namespace entry

} // namespace vk

#endif /* __VK_PHYSICAL_DEVICE_H__ */
