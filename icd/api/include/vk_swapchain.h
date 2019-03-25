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
class SwCompositor;

// =====================================================================================================================
// Implementation of the Vulkan swap chain object (VkSwapChainKHR).
class SwapChain : public NonDispatchable<VkSwapchainKHR, SwapChain>
{
public:
    struct Properties
    {
        DisplayableSurfaceInfo          displayableInfo;
        Pal::PresentableImageCreateInfo imageCreateInfo;
        Pal::PresentMode                imagePresentSupport;   // Describes whether present images support fullscreen or
                                                               // just windowed (default).
        union
        {
            uint32_t                    u32All;
            struct
            {
                uint32_t                summedImage   : 1;     // The image needs a final copy.
                uint32_t                stereo        : 1;     // The swap chain is a stereo one
                uint32_t                hwCompositing : 1;     // If true, only uses SW compositing for windowed mode AFR
                uint32_t                reserved      : 29;
            };
        } flags;
        uint32_t                        presentationDeviceIdx; // The physical device that created the PAL swap chain
        uint32_t                        imageCount;            // Number of images in the swap chain
        VkImage*                        images;
        VkDeviceMemory*                 imageMemory;

        Surface*                        pSurface;
        VkSurfaceFormatKHR              surfaceFormat;

        Surface*                        pFullscreenSurface;
        VkSurfaceFormatKHR              fullscreenSurfaceFormat;

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

    VK_FORCEINLINE Memory* GetPresentableImageMemory(uint32_t imageIndex) const
        { return Memory::ObjectFromHandle(m_properties.imageMemory[imageIndex]); }

    VK_FORCEINLINE Pal::ISwapChain* PalSwapChain() const
        { return m_pPalSwapChain; }

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

    VK_INLINE bool IsHwCompositingSupported() const
        { return (m_properties.flags.hwCompositing == 1); }

    Pal::IQueue* PrePresent(
        uint32_t                   deviceIdx,
        uint32_t                   imageIndex,
        Pal::PresentSwapChainInfo* pPresentInfo,
        const Queue*               pQueue);

    void PostPresent(
        const Pal::PresentSwapChainInfo& presentInfo,
        Pal::Result*                     pPresentResult);

    void MarkAsDeprecated();

protected:

    SwapChain(
        Device*             pDevice,
        const Properties&   properties,
        VkPresentModeKHR    presentMode,
        FullscreenMgr*      pFullscreenMgr,
        Pal::ISwapChain*    pPalSwapChain);

    void InitSwCompositor(Pal::QueueType presentQueueType);

    Device*                 m_pDevice;
    const Properties        m_properties;
    uint32_t                m_nextImage;
    Pal::ISwapChain*        m_pPalSwapChain;

    FullscreenMgr*          m_pFullscreenMgr;
    SwCompositor*           m_pSwCompositor;
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
    union ExclusiveModeFlags
    {
        uint32_t u32All;
        struct
        {
            uint32_t acquired             : 1;  // True if currently in exclusive access (fullscreen) mode
            uint32_t disabled             : 1;  // Disabled by panel or for other reasons (e.g. too many unexpected
                                                // failures)
            uint32_t reserved             : 30; // Reserved for future use
        };
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
        Pal::IScreen*                   pScreen,
        Pal::OsDisplayHandle            hDisplay,
        Pal::OsWindowHandle             hWindow,
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

    void PostImageCreate(const Image* pImage);

    void Destroy(const VkAllocationCallbacks* pAllocator);

    void UpdatePresentInfo(
        SwapChain*                 pSwapChain,
        Pal::PresentSwapChainInfo* pPresentInfo);

    VK_INLINE ExclusiveModeFlags GetExclusiveModeFlags() const
        { return m_exclusiveModeFlags; }

    VK_INLINE uint32_t GetVidPnSourceId() const
        { return m_vidPnSourceId; }

    VK_INLINE Pal::IScreen* GetPalScreen() const
        { return m_pScreen; }

    VkResult SetHdrMetadata(
        Device*                    pDevice,
        const VkHdrMetadataEXT*    pMetadata);

    bool TryEnterExclusive(SwapChain* pSwapChain);
    bool TryExitExclusive(SwapChain* pSwapChain);

private:
    void DisableFullscreenPresents();
    void FullscreenPresentEvent(bool success);

    Device*             m_pDevice;                  // Device pointer
    ExclusiveModeFlags  m_exclusiveModeFlags;       // Flags describing the current exclusive mode state
    Pal::IScreen*       m_pScreen;                  // Screen that owns the window this swap chain was created with.
    const Image*        m_pImage;                   // Pointer to one of the presentable images
    uint32_t            m_exclusiveAccessFailCount; // Number of consecutive times we've failed acquiring exclusive
                                                    // access or failed a fullscreen present because Windows kicked us
                                                    // out.
    uint32_t            m_fullscreenPresentSuccessCount; // Number of consecutively successful fullscreen presents.

    Pal::ScreenColorCapabilities m_colorCaps;
    Pal::ScreenColorConfig       m_colorParams;
    Pal::ScreenColorConfig       m_windowedColorParams;
    Pal::OsDisplayHandle         m_hDisplay;        // The monitor of the IScreen from swap chain creation
    Pal::OsWindowHandle          m_hWindow;         // The window of the swap chain

    uint32_t            m_vidPnSourceId;            // Video present source identifier
    Mode                m_mode;                     // Indicates the Presentation mode we are using
};

// =====================================================================================================================
// This is a helper class that handles software compositing
class SwCompositor
{
public:
    static SwCompositor* Create(
        const Device*                pDevice,
        const VkAllocationCallbacks* pAllocator,
        const SwapChain::Properties& properties,
        bool                         useSdmaCompositingBlt);

    void Destroy(const Device* pDevice, const VkAllocationCallbacks* pAllocator);

    Pal::QueueType GetQueueType() const { return m_queueType; }

    Pal::IQueue* DoSwCompositing(
        Device*                    pDevice,
        uint32_t                   deviceIdx,
        uint32_t                   imageIndex,
        Pal::PresentSwapChainInfo* pPresentInfo,
        const Queue*               pPresentQueue);

protected:
    SwCompositor(
        const Device*     pDevice,
        uint32_t          presentationDeviceIdx,
        uint32_t          imageCount,
        Pal::QueueType    queueType,
        Pal::IImage**     ppBltImages[],
        Pal::IGpuMemory** ppBltMemory[],
        Pal::ICmdBuffer** ppBltCmdBuffers[]);

    ~SwCompositor() {}

    uint32_t          m_presentationDeviceIdx;          // The physical device that performs the actual present
    uint32_t          m_imageCount;                     // The number of images in the swapchain
    Pal::QueueType    m_queueType;                      // The queue type that the command buffers are compatible with
    Pal::IImage**     m_ppBltImages[MaxPalDevices];     // Array of intermediate images (master) or peer images (slave)
    Pal::IGpuMemory** m_ppBltMemory[MaxPalDevices];     // Array of intermediate memory (master) or peer memory (slave)
    Pal::ICmdBuffer** m_ppBltCmdBuffers[MaxPalDevices]; // Array of copy to peer image command buffers (slave-only)
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

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHR(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHR*            pAcquireInfo,
    uint32_t*                                   pImageIndex);

VKAPI_ATTR void VKAPI_CALL vkSetHdrMetadataEXT(
    VkDevice                                    device,
    uint32_t                                    swapchainCount,
    const VkSwapchainKHR*                       pSwapchains,
    const VkHdrMetadataEXT*                     pMetadata);

}// entry
}// vk

#endif /* __VK_SWAPCHAIN_H__ */
