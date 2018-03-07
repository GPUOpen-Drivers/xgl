/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_swapchain.h
 * @brief Contains declaration of Vulkan swap chain classes.
 ***********************************************************************************************************************
 */

#ifndef __VK_SWAPCHAIN_H__
#define __VK_SWAPCHAIN_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_device.h"
#include "include/vk_image.h"
#include "include/vk_utils.h"

#include "palQueue.h"
#include "palSwapChain.h"

namespace vk
{

// Forward declare Vulkan classes used in this file.
class FullscreenMgr;
class Fence;
class Image;
class Semaphore;

// =====================================================================================================================
// Implementation of the Vulkan swap chain object (VkSwapChainKHR).
class SwapChain : public NonDispatchable<VkSwapchainKHR, SwapChain>
{
public:
    struct Properties
    {
        DisplayableSurfaceInfo displayableInfo;
        Pal::PresentMode       imagePresentSupport; // Describes whether present images support fullscreen or
                                                    // just windowed (default).
        bool                   summedImage;         // The image needs a final copy.
        bool                   stereo;              // The swap chain is a stereo one
        uint32_t               imageCount;          // Number of images in the swap chain
        VkFormat               imageFormat;         // Image format
        VkImage*               images;
        VkDeviceMemory*        imageMemory;

        Surface*               pSurface;
        VkSurfaceFormatKHR     surfaceFormat;

        Surface*               pFullscreenSurface;
        VkSurfaceFormatKHR     fullscreenSurfaceFormat;
    };

    static VkResult Create(
        Device*                                 pDevice,
        const VkSwapchainCreateInfoKHR*         pCreateInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkSwapchainKHR*                         pSwapChain);

    VkResult Destroy(const VkAllocationCallbacks* pAllocator);

    VkResult AcquireNextImage(
        const VkStructHeader*   pAcquireInfo,
        uint32_t*               pImageIndex);

    VkResult GetSwapchainImagesKHR(
        uint32_t*       pCount,
        VkImage*        pSwapchainImages);

    VK_FORCEINLINE const Properties& GetProperties() const
        { return m_properties; }

    VK_FORCEINLINE const Image* GetPresentableImage(uint32_t imageIndex) const
        { return Image::ObjectFromHandle(m_properties.images[imageIndex]); }

    VK_FORCEINLINE const Memory* GetPresentableImageMemory(uint32_t imageIndex) const
        { return Memory::ObjectFromHandle(m_properties.imageMemory[imageIndex]); }

    VK_FORCEINLINE Pal::ISwapChain* PalSwapChain(uint32_t deviceIdx = DefaultDeviceIndex) const
        { return m_pPalSwapChain[deviceIdx]; }

    VK_INLINE const FullscreenMgr* GetFullscreenMgr() const
        { return m_pFullscreenMgr; }

    VK_INLINE FullscreenMgr* GetFullscreenMgr()
        { return m_pFullscreenMgr; }

    VK_INLINE uint32_t GetPresentCount() const
        { return m_presentCount; }

    VK_INLINE VkPresentModeKHR GetPresentMode() const
        { return m_presentMode; }

    VK_INLINE uint32_t GetAppOwnedImageCount() const
        { return m_appOwnedImageCount; }

    VkResult GetPresentInfo(
        uint32_t                   deviceIdx,
        uint32_t                   imageIndex,
        Pal::PresentSwapChainInfo* pPresentInfo);

    void PostPresent(
        const Pal::PresentSwapChainInfo& presentInfo,
        Pal::Result*                     pPresentResult);

    VkResult AcquireWin32FullscreenOwnership(
        Device*                       pDevice);

    VkResult ReleaseWin32FullscreenOwnership(
        Device*                       pDevice);

    void MarkAsDeprecated();

    Pal::Result PalSwapChainWaitIdle() const;
    void        PalSwapChainDestroy();

protected:

    SwapChain(
        Device*             pDevice,
        const Properties&   properties,
        FullscreenMgr*      pFullscreenMgr)
        :
        m_pDevice(pDevice),
        m_properties(properties),
        m_nextImage(0),
        m_pFullscreenMgr(pFullscreenMgr),
        m_appOwnedImageCount(0),
        m_presentCount(0),
        m_deprecated(false)
    {
    }

    Device*                 m_pDevice;
    const Properties        m_properties;
    uint32_t                m_nextImage;
    Pal::ISwapChain*        m_pPalSwapChain[MaxPalDevices];

    FullscreenMgr*          m_pFullscreenMgr;
    int32_t                 m_appOwnedImageCount;
    uint32_t                m_presentCount;
    VkPresentModeKHR        m_presentMode;
    bool                    m_deprecated;      // Indicates whether the swapchain has been used as
                                               // oldSwapChain when creating a new SwapChain.
};

// =====================================================================================================================
// This is a helper class that handles Implicit, Explicit and Explicit_Mixed Presentation modes
class FullscreenMgr
{
public:
    // These flags describe whether the current state of the screen tied to the swap chain is compatible with
    // exclusive mode.  If all of the flags are 0, the screen is compatible and we can attempt to enter exclusive
    // access mode and enable page flipping.  Otherwise, we should exit immediately or at our earliest convenience.
    union CompatibilityFlags
    {
        struct
        {
            uint32_t disabled             : 1;  // Disabled by panel or for other reasons (e.g. too many unexpected
                                                // failures)
            uint32_t screenChanged        : 1;  // Current screen that owns the window has changed
            uint32_t windowRectBad        : 1;  // Window rect doesn't cover the whole desktop
            uint32_t resolutionBad        : 1;  // Current screen resolution does not match swap chain extents
            uint32_t windowNotForeground  : 1;  // Swap chain window is not currently the foreground window
            uint32_t reserved             : 27; // Padding, should be set to zero
        };
        uint32_t u32All;
    };

    enum Mode
    {
        Implicit = 0,
        Explicit,
        Explicit_Mixed,
    };

    FullscreenMgr(
        Device*                         pDevice,
        FullscreenMgr::Mode             mode,
        const Pal::SwapChainCreateInfo& createInfo,
        Pal::IScreen*                   pScreen,
        uint32_t                        vidPnSourceId);

    ~FullscreenMgr();

    void PostAcquireImage(SwapChain* pSwapChain);

    void PostPresent(
        SwapChain*                       pSwapChain,
        const Pal::PresentSwapChainInfo& presentInfo,
        Pal::Result*                     pPresentResult);

    void PreImageCreate(
        Pal::PresentMode*                pImagePresentSupport,
        Pal::PresentableImageCreateInfo* pImageInfo);

    void PreSwapChainCreate(Pal::SwapChainCreateInfo* pCreateInfo);

    void PostImageCreate(const Image* pImage);

    void Destroy(const VkAllocationCallbacks* pAllocator);

    VkResult UpdatePresentInfo(
        SwapChain*                 pSwapChain,
        Pal::PresentSwapChainInfo* pPresentInfo);

    VK_INLINE CompatibilityFlags GetCompatibility() const
        { return m_compatFlags; }

    VK_INLINE bool HasExclusiveAccess() const
        { return m_exclusiveModeAcquired; }

    VK_INLINE const Pal::Extent2d& GetLastResolution() const
        { return m_lastResolution; }

    VK_INLINE uint32_t GetVidPnSourceId() const
        { return m_vidPnSourceId; }

    VK_INLINE Pal::IScreen* GetPalScreen() const
        { return m_pScreen; }

    VkResult SetHdrMetadata(
        Device*                    pDevice,
        const VkHdrMetadataEXT*    pMetadata);

    bool TryEnterExclusive(SwapChain* pSwapChain);
    bool TryExitExclusive(SwapChain* pSwapChain);
    bool UpdateExclusiveModeCompat(SwapChain* pSwapChain);

private:
    CompatibilityFlags EvaluateExclusiveModeCompat(const SwapChain* pSwapChain) const;

    void UpdateExclusiveMode(SwapChain* pSwapChain);
    void DisableFullscreenPresents();
    void FullscreenPresentEvent(bool success);
    VK_INLINE bool IsExclusiveModePossible() const { return (m_compatFlags.u32All == 0); }

    Device*             m_pDevice;                  // Device pointer
    CompatibilityFlags  m_compatFlags;              // Current exclusive access compatibility flags
    Pal::IScreen*       m_pScreen;                  // Screen that owns the window this swap chain was created with.
    const Image*        m_pImage;                   // Pointer to one of the presentable images
    uint32_t            m_exclusiveAccessFailCount; // Number of consecutive times we've failed acquiring exclusive
                                                    // access or failed a fullscreen present because Windows kicked us
                                                    // out.
    uint32_t            m_fullscreenPresentSuccessCount; // Number of consecutively successful fullscreen presents.

    Pal::ScreenColorCapabilities m_colorCaps;
    Pal::ScreenColorConfig       m_colorParams;
    Pal::ScreenColorConfig       m_windowedColorParams;

    Pal::Extent2d       m_lastResolution;
    uint32_t            m_vidPnSourceId;            // Video present source identifier
    Mode                m_mode;                     // Indicates the Presentation mode we are using
    bool                m_exclusiveModeAcquired;    // True if currently in exclusive access (fullscreen) mode
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    const VkAllocationCallbacks*                 pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    uint32_t*                                    pSwapchainImageCount,
    VkImage*                                     pSwapchainImages);

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    uint64_t                                     timeout,
    VkSemaphore                                  semaphore,
    VkFence                                      fence,
    uint32_t*                                    pImageIndex);

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHX(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHX*            pAcquireInfo,
    uint32_t*                                   pImageIndex);

#ifdef ICD_VULKAN_1_1
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHR(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHR*            pAcquireInfo,
    uint32_t*                                   pImageIndex);
#endif

VKAPI_ATTR void VKAPI_CALL vkSetHdrMetadataEXT(
    VkDevice                                    device,
    uint32_t                                    swapchainCount,
    const VkSwapchainKHR*                       pSwapchains,
    const VkHdrMetadataEXT*                     pMetadata);

}// entry
}// vk

#endif /* __VK_SWAPCHAIN_H__ */
