/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifdef ICD_BUILD_APPPROFILE
        AppProfile              appProfile,
#endif
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
        const VkPhysicalDeviceExternalSemaphoreInfoKHR* pExternalSemaphoreInfo,
        VkExternalSemaphorePropertiesKHR*               pExternalSemaphoreProperties);

    void GetExternalFenceProperties(
        const VkPhysicalDeviceExternalFenceInfoKHR* pExternalFenceInfo,
        VkExternalFencePropertiesKHR*               pExternalFenceProperties);

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

    VK_INLINE const VkQueueFamilyProperties& GetQueueFamilyProperties(
        uint32_t queueFamilyIndex) const
    {
        return m_queueFamilies[queueFamilyIndex].properties;
    }

    VkResult GetQueueFamilyProperties(
        uint32_t*                pCount,
        VkQueueFamilyProperties* pQueueProperties) const;

    VkResult GetQueueFamilyProperties(
        uint32_t*                    pCount,
        VkQueueFamilyProperties2KHR* pQueueProperties) const;

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

    VkResult GetBufferExternalMemoryProperties(
        VkExternalMemoryHandleTypeFlagBitsKHR   handleType,
        VkExternalMemoryPropertiesKHR*          pExternalMemoryProperties) const;

    VkResult GetImageExternalMemoryProperties(
        VkFormat                                format,
        VkImageType                             type,
        VkImageTiling                           tiling,
        VkImageUsageFlags                       usage,
        VkImageCreateFlags                      flags,
        VkExternalMemoryHandleTypeFlagBitsKHR   handleType,
        VkExternalMemoryPropertiesKHR*          pExternalMemoryProperties) const;

    VkResult GetImageFormatProperties(
        VkFormat                 format,
        VkImageType              type,
        VkImageTiling            tiling,
        VkImageUsageFlags        usage,
        VkImageCreateFlags       flags,
        VkImageFormatProperties* pImageFormatProperties) const;

    void GetSparseImageFormatProperties(
        VkFormat                        format,
        VkImageType                     type,
        VkSampleCountFlagBits           samples,
        VkImageUsageFlags               usage,
        VkImageTiling                   tiling,
        uint32_t*                       pPropertyCount,
        VkSparseImageFormatProperties*  pProperties) const;

    void GetExternalBufferProperties(
        const VkPhysicalDeviceExternalBufferInfoKHR*    pExternalBufferInfo,
        VkExternalBufferPropertiesKHR*                  pExternalBufferProperties);

    void GetSparseImageFormatProperties2(
        const VkPhysicalDeviceSparseImageFormatInfo2KHR*    pFormatInfo,
        uint32_t*                                           pPropertyCount,
        VkSparseImageFormatProperties2KHR*                  pProperties);

    void GetFeatures2(
        VkPhysicalDeviceFeatures2KHR*               pFeatures);

    void GetMemoryProperties2(
        VkPhysicalDeviceMemoryProperties2KHR*  pMemoryProperties);

    void GetFormatProperties2(
        VkFormat                                    format,
        VkFormatProperties2KHR*                     pFormatProperties);

    void GetDeviceProperties2(
        VkPhysicalDeviceProperties2KHR*             pProperties);

    void GetPhysicalDeviceFeatures2(
        VkPhysicalDeviceFeatures2KHR*               pFeatures);

    VkResult GetImageFormatProperties2(
        const VkPhysicalDeviceImageFormatInfo2KHR*  pImageFormatInfo,
        VkImageFormatProperties2KHR*                pImageFormatProperties);

    void GetDeviceMultisampleProperties(
        VkSampleCountFlagBits                       samples,
        VkMultisamplePropertiesEXT*                 pMultisampleProperties);

    bool QueueSupportsPresents(
        uint32_t queueFamilyIndex) const;

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

    VkResult EnumerateExtensionProperties(
        const char*                 pLayerName,
        uint32_t*                   pPropertyCount,
        VkExtensionProperties*      pProperties) const;

    static DeviceExtensions::Supported GetAvailableExtensions(
        const Instance*       pInstance,
        const PhysicalDevice* pPhysicalDevice);

    VK_INLINE const DeviceExtensions::Supported& GetSupportedExtensions() const
        { return m_supportedExtensions; }

    VK_INLINE bool IsExtensionSupported(DeviceExtensions::ExtensionId id) const
        { return m_supportedExtensions.IsExtensionSupported(id); }

    VK_INLINE bool IsExtensionSupported(InstanceExtensions::ExtensionId id) const
        { return VkInstance()->IsExtensionSupported(id); }

#ifdef ICD_BUILD_APPPROFILE
    VK_INLINE AppProfile GetAppProfile() const
        { return m_appProfile; }
#endif

    VK_INLINE const PhysicalDeviceGpaProperties& GetGpaProperties() const
        { return m_gpaProps; }

    void LateInitialize();

protected:
    PhysicalDevice(PhysicalDeviceManager* pPhysicalDeviceManager,
                   Pal::IDevice*          pPalDevice,
                   const RuntimeSettings& settings
#ifdef ICD_BUILD_APPPROFILE
                   ,
                   AppProfile             appProfile
#endif
                   );

    VkResult Initialize();
    void PopulateLimits();
    void PopulateExtensions();
    void PopulateGpaProperties();

    PhysicalDeviceManager*           m_pPhysicalDeviceManager;
    Pal::IDevice*                    m_pPalDevice;
    Pal::DeviceProperties            m_properties;
    uint32_t                         m_memoryTypeMask;
    uint32_t                         m_memoryPalHeapToVkIndex[Pal::GpuHeapCount];
    Pal::GpuHeap                     m_memoryVkIndexToPalHeap[Pal::GpuHeapCount];
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    RuntimeSettings                  m_settings;
    VkPhysicalDeviceLimits           m_limits;
    VkFormatProperties               m_formatFeaturesTable[VK_SUPPORTED_FORMAT_COUNT];
    uint16_t                         m_formatFeatureMsaaTarget[(VK_SUPPORTED_FORMAT_COUNT + (sizeof(uint16_t) << 3) - 1) /
                                                               (sizeof(uint16_t) << 3)];
    uint32_t                         m_queueFamilyCount;
    struct
    {
        Pal::QueueType               palQueueType;
        Pal::EngineType              palEngineType;
        uint32_t                     palImageLayoutFlag;
        VkQueueFamilyProperties      properties;
    } m_queueFamilies[Queue::MaxQueueFamilies];

#ifdef ICD_BUILD_APPPROFILE
    const AppProfile                 m_appProfile;
#endif

    DeviceExtensions::Supported      m_supportedExtensions;

    // Device properties related to the VK_AMD_gpu_perf_api_interface extension
    PhysicalDeviceGpaProperties      m_gpaProps;
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

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures2KHR*               pFeatures);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties2KHR*             pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties2KHR*                     pFormatProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2KHR*  pImageFormatInfo,
    VkImageFormatProperties2KHR*                pImageFormatProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2KHR*                pQueueFamilyProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties2KHR*       pMemoryProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2KHR(
    VkPhysicalDevice                                    physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2KHR*    pFormatInfo,
    uint32_t*                                           pPropertyCount,
    VkSparseImageFormatProperties2KHR*                  pProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalBufferPropertiesKHR(
    VkPhysicalDevice                                physicalDevice,
    const VkPhysicalDeviceExternalBufferInfoKHR*    pExternalBufferInfo,
    VkExternalBufferPropertiesKHR*                  pExternalBufferProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMultisamplePropertiesEXT(
    VkPhysicalDevice                            physicalDevice,
    VkSampleCountFlagBits                       samples,
    VkMultisamplePropertiesEXT*                 pMultisampleProperties);

VKAPI_ATTR void VKAPI_CALL vkTrimCommandPoolKHR(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolTrimFlagsKHR                   flags);

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

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHX(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfoKHR* pExternalSemaphoreInfo,
    VkExternalSemaphorePropertiesKHR*               pExternalSemaphoreProperties);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFencePropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceExternalFenceInfoKHR* pExternalFenceInfo,
    VkExternalFencePropertiesKHR*               pExternalFenceProperties);

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

} // namespace entry

} // namespace vk

#endif /* __VK_PHYSICAL_DEVICE_H__ */
