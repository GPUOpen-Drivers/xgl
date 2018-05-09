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
 * @file  vk_swapchain.cpp
 * @brief Contains implementation of Vulkan swap chain classes.
 ***********************************************************************************************************************
 */

// These WSI header files should be included before vk_wsi_swapchina.h
#include <xcb/xcb.h>
#include "include/vk_conv.h"
#include "include/vk_display.h"
#include "include/vk_display_manager.h"
#include "include/vk_fence.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_queue.h"
#include "include/vk_semaphore.h"
#include "include/vk_surface.h"
#include "include/vk_swapchain.h"
#include "include/vk_utils.h"
#include "include/khronos/vk_icd.h"

#include "palQueueSemaphore.h"
#include "palSwapChain.h"
#include "palAutoBuffer.h"

#include <stdio.h>

namespace vk
{

static bool EnableFullScreen(
    const Device*                   pDevice,
    const SwapChain::Properties&    swapChainProps,
    FullscreenMgr::Mode             mode,
    const VkSwapchainCreateInfoKHR& createInfo,
    Pal::IScreen**                  ppScreen);

// =====================================================================================================================
// Creates a new Vulkan API swap chain object
VkResult SwapChain::Create(
    Device*                                 pDevice,
    const VkSwapchainCreateInfoKHR*         pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkSwapchainKHR*                         pSwapChain)
{
    VkResult result = VK_SUCCESS;

    const VkDeviceGroupSwapchainCreateInfoKHX*  pDeviceGroupExt = nullptr;

    Pal::PresentableImageCreateInfo imageCreateInfo = {};
    Properties properties = {};

    union
    {
        const VkStructHeader*                      pHeader;
        const VkSwapchainCreateInfoKHR*            pVkSwapchainCreateInfoKHR;
        const VkDeviceGroupSwapchainCreateInfoKHX* pVkDeviceGroupSwapchainCreateInfoKHX;
    };

    for (pVkSwapchainCreateInfoKHR = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR:
            {
                VK_ASSERT(pCreateInfo == pVkSwapchainCreateInfoKHR);

                Surface* pSurface = Surface::ObjectFromHandle(pVkSwapchainCreateInfoKHR->surface);

                if (pSurface->IsExplicitFullscreenSurface() == false)
                {
                    properties.pSurface      = pSurface;
                    properties.surfaceFormat = { pVkSwapchainCreateInfoKHR->imageFormat,
                                                 pVkSwapchainCreateInfoKHR->imageColorSpace };
                }
                else
                {
                    properties.pFullscreenSurface      = pSurface;
                    properties.fullscreenSurfaceFormat = { pVkSwapchainCreateInfoKHR->imageFormat,
                                                           pVkSwapchainCreateInfoKHR->imageColorSpace };
                }

                result = PhysicalDevice::UnpackDisplayableSurface(pSurface, &properties.displayableInfo);

                if (pDevice->VkInstance()->GetProperties().supportExplicitPresentMode)
                {
                    properties.imagePresentSupport = Pal::PresentMode::Windowed;
                }
                else
                {
                    // According to the design, when explicitPresentModes is not supported by platform,
                    // the present mode set by client is just a hint.
                    // The fullscreen present mode is always a preferred mode but platform would make the final call.
                    // To be fixed! The dota2 1080p + ultra mode noticed a performance drop.
                    // Dislabe the flip mode for now.
                    if (pDevice->GetRuntimeSettings().useFlipHint)
                    {
                        properties.imagePresentSupport = Pal::PresentMode::Fullscreen;
                    }
                    else
                    {
                        properties.imagePresentSupport = Pal::PresentMode::Windowed;
                    }
                }
                // The swap chain is stereo if imageArraySize is 2
                properties.stereo = (pVkSwapchainCreateInfoKHR->imageArrayLayers == 2) ? 1 : 0;

                // Store the image format
                properties.imageFormat  = pVkSwapchainCreateInfoKHR->imageFormat;

                imageCreateInfo.swizzledFormat     = VkToPalFormat(pVkSwapchainCreateInfoKHR->imageFormat);
                imageCreateInfo.flags.stereo       = properties.stereo;
                imageCreateInfo.flags.peerWritable = (pDevice->NumPalDevices() > 1) ? 1 : 0;

                VkFormatProperties formatProperties;
                pDevice->VkPhysicalDevice()->GetFormatProperties(pVkSwapchainCreateInfoKHR->imageFormat,
                                                                 &formatProperties);
                VkImageUsageFlags imageUsage = pVkSwapchainCreateInfoKHR->imageUsage;
                imageUsage &=
                    VkFormatFeatureFlagsToImageUsageFlags(formatProperties.optimalTilingFeatures);

                imageCreateInfo.usage              = VkToPalImageUsageFlags(imageUsage,
                                                                        pVkSwapchainCreateInfoKHR->imageFormat,
                                                                        1,
                                                                        (VkImageUsageFlags)(0),
                                                                        (VkImageUsageFlags)(0));
                imageCreateInfo.extent         = VkToPalExtent2d(pVkSwapchainCreateInfoKHR->imageExtent);
                imageCreateInfo.hDisplay       = properties.displayableInfo.displayHandle;
                imageCreateInfo.hWindow        = properties.displayableInfo.windowHandle;

            }
            break;

        case VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHX:
            pDeviceGroupExt = pVkDeviceGroupSwapchainCreateInfoKHX;
            properties.summedImage = (pDeviceGroupExt->modes & VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHX) != 0;
            break;
        default:
            // Skip any unknown extension structures
            break;
        }
    }

    if (pCreateInfo == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (result != VK_SUCCESS)
    {
        return result;
    }

    // Create PAL presentable images
    Pal::Result              palResult = Pal::Result::Success;
    Pal::ISwapChain*         pPalSwapChain[MaxPalDevices] = {};
    Pal::SwapChainCreateInfo swapChainCreateInfo          = {};
    const uint32_t           swapImageCount               = pCreateInfo->minImageCount;

    // PAL already support swap chain object, so swap chain object is created first and then the presentable image.
    swapChainCreateInfo.hDisplay            = properties.displayableInfo.displayHandle;
    swapChainCreateInfo.hWindow             = properties.displayableInfo.windowHandle;
    swapChainCreateInfo.wsiPlatform         = properties.displayableInfo.palPlatform;
    swapChainCreateInfo.imageCount          = pCreateInfo->minImageCount;
    swapChainCreateInfo.imageSwizzledFormat = VkToPalFormat(properties.imageFormat);
    swapChainCreateInfo.imageExtent         = VkToPalExtent2d(pCreateInfo->imageExtent);
    swapChainCreateInfo.imageUsageFlags     = VkToPalImageUsageFlags(pCreateInfo->imageUsage,
                                                                     pCreateInfo->imageFormat,
                                                                     1,
                                                                     (VkImageUsageFlags)(0),
                                                                     (VkImageUsageFlags)(0));
    swapChainCreateInfo.preTransform        = Pal::SurfaceTransformNone;
    swapChainCreateInfo.imageArraySize      = 1;
    swapChainCreateInfo.swapChainMode       = VkToPalSwapChainMode(pCreateInfo->presentMode);

    // Allocate system memory for objects
    const size_t    vkSwapChainSize  = sizeof(SwapChain);
    size_t          palSwapChainSize = pDevice->PalDevice()->GetSwapChainSize(swapChainCreateInfo, &palResult);
    size_t          imageArraySize   = sizeof(VkImage) * swapImageCount;
    size_t          memoryArraySize  = sizeof(VkDeviceMemory) * swapImageCount;
    size_t          objSize          = vkSwapChainSize +
                                       (palSwapChainSize * pDevice->NumPalDevices()) +
                                       imageArraySize + memoryArraySize;
    void*           pMemory          = pDevice->AllocApiObject(objSize, pAllocator);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    size_t offset = vkSwapChainSize;

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

        palResult = pPalDevice->CreateSwapChain(
            swapChainCreateInfo,
            Util::VoidPtrInc(pMemory, offset),
            &pPalSwapChain[deviceIdx]);

        size_t deviceSwapChainSize = pPalDevice->GetSwapChainSize(swapChainCreateInfo, &palResult);

        VK_ASSERT(palSwapChainSize >= deviceSwapChainSize);

        offset += palSwapChainSize;
    }
    result = PalToVkResult(palResult);

    if (result == VK_SUCCESS)
    {
        imageCreateInfo.pSwapChain = pPalSwapChain[DefaultDeviceIndex];
    }

    // Allocate memory for the fullscreen manager if it's enabled.  We need to create it first before the
    // swap chain because it needs to have a say in how presentable images are created.
    Pal::IScreen*  pScreen        = nullptr;
    FullscreenMgr* pFullscreenMgr = nullptr;

    // Figure out the mode the FullscreenMgr should be working in
    const FullscreenMgr::Mode mode =
            ((properties.pFullscreenSurface != nullptr) && (properties.pSurface != nullptr)) ?
                                                          FullscreenMgr::Explicit_Mixed :
            ((properties.pFullscreenSurface != nullptr) ? FullscreenMgr::Explicit :
                                                          FullscreenMgr::Implicit);

    if (EnableFullScreen(pDevice, properties, mode, *pCreateInfo, &pScreen))
    {
        void* pFullscreenStorage = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(FullscreenMgr),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pFullscreenStorage != nullptr)
        {
            Pal::ScreenProperties props = {};
            palResult = pScreen->GetProperties(&props);
            VK_ASSERT(palResult == Pal::Result::Success);

            // Construct the manager
            pFullscreenMgr = VK_PLACEMENT_NEW(pFullscreenStorage) FullscreenMgr(
                pDevice,
                mode,
                swapChainCreateInfo,
                pScreen,
                props.vidPnSourceId);
        }
    }

    if (pFullscreenMgr != nullptr)
    {
        // Update the image create info to make them compatible with optional fullscreen presents.
        pFullscreenMgr->PreImageCreate(
            &properties.imagePresentSupport,
            &imageCreateInfo);
    }

    properties.images      = static_cast<VkImage*>(Util::VoidPtrInc(pMemory, offset));
    offset += imageArraySize;

    properties.imageMemory = static_cast<VkDeviceMemory*>(Util::VoidPtrInc(pMemory, offset));
    offset += memoryArraySize;

    VK_ASSERT(offset == objSize);

    // Initialize sharing mode to concurrent and use all available queue's flag for the image layout.
    VkSharingMode sharingMode     = VK_SHARING_MODE_CONCURRENT;

    sharingMode = pCreateInfo->imageSharingMode;

    for (properties.imageCount = 0; properties.imageCount < swapImageCount; ++properties.imageCount)
    {
        if (result == VK_SUCCESS)
        {
            // Create presentable image
            result = Image::CreatePresentableImage(
                pDevice,
                &imageCreateInfo,
                pAllocator,
                pCreateInfo->imageUsage,
                properties.imagePresentSupport,
                &properties.images[properties.imageCount],
                properties.imageFormat,
                sharingMode,
                pCreateInfo->queueFamilyIndexCount,
                pCreateInfo->pQueueFamilyIndices,
                &properties.imageMemory[properties.imageCount]);
        }

        if (result == VK_SUCCESS)
        {
            palResult = Pal::Result::Success;

            // Add memory references to presentable image memory
            for (uint32_t deviceIdx = 0;
                (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                 deviceIdx++)
            {
                palResult = pDevice->AddMemReference(
                    pDevice->PalDevice(deviceIdx),
                    Memory::ObjectFromHandle(properties.imageMemory[properties.imageCount])->PalMemory(deviceIdx),
                    false);
            }

            result = PalToVkResult(palResult);
        }

        if (result != VK_SUCCESS)
        {
            break;
        }
    }

    if (pFullscreenMgr != nullptr)
    {
        VkImage anyImage = (properties.imageCount > 0) ? properties.images[0] : VK_NULL_HANDLE;

        pFullscreenMgr->PostImageCreate(Image::ObjectFromHandle(anyImage));
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) SwapChain(pDevice, properties, pFullscreenMgr);

        *pSwapChain = SwapChain::HandleFromVoidPointer(pMemory);

        SwapChain* pObject = SwapChain::ObjectFromHandle(*pSwapChain);

        memcpy(pObject->m_pPalSwapChain, pPalSwapChain, sizeof(pPalSwapChain));
        pObject->m_presentMode = pCreateInfo->presentMode;

        for (uint32_t i = 0; i < properties.imageCount; ++i)
        {
            // Register presentable images with the swap chain
            Image::ObjectFromHandle(properties.images[i])->RegisterPresentableImageWithSwapChain(pObject);
        }
    }
    else
    {
        if (pFullscreenMgr != nullptr)
        {
            pFullscreenMgr->Destroy(pAllocator);
        }

        // Delete already created images and image memory.
        for (uint32_t i = 0; i < properties.imageCount; ++i)
        {
            Memory::ObjectFromHandle(properties.imageMemory[i])->Free(pDevice, pAllocator);
            Image::ObjectFromHandle(properties.images[i])->Destroy(pDevice, pAllocator);
        }

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            if (pPalSwapChain[deviceIdx] != nullptr)
            {
                pPalSwapChain[deviceIdx]->Destroy();
            }
        }

        // Delete allocated memory
        pAllocator->pfnFree(pAllocator->pUserData, pMemory);
    }

    // the old swapchain should be flaged as deprecated no matter whether the new swapchain is created successfully.
    if (pCreateInfo->oldSwapchain != 0)
    {
        SwapChain::ObjectFromHandle(pCreateInfo->oldSwapchain)->MarkAsDeprecated();
    }

    return result;
}

// =====================================================================================================================
// Destroy Vulkan swap chain.
VkResult SwapChain::Destroy(const VkAllocationCallbacks* pAllocator)
{
    // Make sure the swapchain is idle and safe to be destroyed.
    PalSwapChainWaitIdle();

    if (m_pFullscreenMgr != nullptr)
    {
        m_pFullscreenMgr->Destroy(pAllocator);
    }

    for (uint32_t i = 0; i < m_properties.imageCount; ++i)
    {
        // Remove memory references to presentable image memory and destroy the images and image memory.
        Memory::ObjectFromHandle(m_properties.imageMemory[i])->Free(m_pDevice, pAllocator);
        Image::ObjectFromHandle(m_properties.images[i])->Destroy(m_pDevice, pAllocator);
    }

    PalSwapChainDestroy();

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Wait for all devices to be idle
// =====================================================================================================================
Pal::Result SwapChain::PalSwapChainWaitIdle() const
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
         (deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
         deviceIdx++)
    {
        if (m_pPalSwapChain[deviceIdx] != nullptr)
        {
            palResult = m_pPalSwapChain[deviceIdx]->WaitIdle();
        }
    }

    return palResult;
}

// =====================================================================================================================
// Wait for all devices to be idle
// =====================================================================================================================
void SwapChain::PalSwapChainDestroy()
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalSwapChain[deviceIdx] != nullptr)
        {
            m_pPalSwapChain[deviceIdx]->Destroy();
            m_pPalSwapChain[deviceIdx] = nullptr;
        }
    }
}

// =====================================================================================================================
// Implementation of vkGetSwapChainInfoWSI. Early error checks and switch out to sub-functions.
// =====================================================================================================================
// Acquires the next presentable swap image.
VkResult SwapChain::AcquireNextImage(
    const VkStructHeader*            pAcquireInfo,
    uint32_t*                        pImageIndex)
{
    VkFence     fence     = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    uint64_t    timeout   = UINT64_MAX;

    union
    {
        const VkStructHeader*             pHeader;
        const VkAcquireNextImageInfoKHX*  pVkAcquireNextImageInfoKHX;
    };

    // KHX structure has the same definition as KHR or Vulkan 1.1 core structures
    static_assert(VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR == VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHX,
                  "Mismatched KHR and KHX structure defines");

    uint32_t presentationDeviceIdx = DefaultDeviceIndex;

    for (pHeader = pAcquireInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHX:
        {
            semaphore = pVkAcquireNextImageInfoKHX->semaphore;
            fence     = pVkAcquireNextImageInfoKHX->fence;
            timeout   = pVkAcquireNextImageInfoKHX->timeout;

            Util::BitMaskScanForward(&presentationDeviceIdx, pVkAcquireNextImageInfoKHX->deviceMask);

            break;
        }

        default:
            break;
        };
    }

    if (pAcquireInfo == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    Pal::AcquireNextImageInfo acquireInfo = {};
    VkResult                  result = VK_SUCCESS;

    // SwapChain should not return any image if it was marked as deprecated before.
    if (m_deprecated == false)
    {
        Semaphore* pSemaphore = Semaphore::ObjectFromHandle(semaphore);
        Fence*     pFence     = Fence::ObjectFromHandle(fence);

        if (result == VK_SUCCESS)
        {
            acquireInfo.timeout    = timeout;
            acquireInfo.pSemaphore = (pSemaphore != nullptr) ? pSemaphore->PalSemaphore() : nullptr;
            acquireInfo.pFence     = (pFence != nullptr) ? pFence->PalFence(presentationDeviceIdx) : nullptr;

            result = PalToVkResult(m_pPalSwapChain[presentationDeviceIdx]->AcquireNextImage(acquireInfo, pImageIndex));
        }

        if (result == VK_SUCCESS)
        {
            m_appOwnedImageCount++;
        }
    }
    else
    {
        // it is not stated explicitly in the spec. Therefore VK_ERROR_OUT_OF_DATE_KHR is used to here.
        result = VK_ERROR_OUT_OF_DATE_KHR;
    }

    if ((timeout == 0) && (result == VK_TIMEOUT))
    {
        result = VK_NOT_READY;
    }

    return result;
}

// =====================================================================================================================
// Called after a present operation on the given queue using this swap chain.
void SwapChain::PostPresent(
    const Pal::PresentSwapChainInfo& presentInfo,
    Pal::Result*                     pPresentResult)
{
    if (m_pFullscreenMgr != nullptr)
    {
        m_pFullscreenMgr->PostPresent(this, presentInfo, pPresentResult);
    }

    m_appOwnedImageCount--;
    m_presentCount++;
}

// =====================================================================================================================
// Gets an array of presentable images associated with the swapchain.
VkResult SwapChain::GetSwapchainImagesKHR(
    uint32_t* pCount,
    VkImage*  pSwapchainImages)
{
    VkResult result = VK_SUCCESS;

    if (pSwapchainImages == nullptr)
    {
        *pCount = m_properties.imageCount;
    }
    else
    {
        const uint32_t numImagesToStore = Util::Min(*pCount, m_properties.imageCount);

        for (uint32_t i = 0; i < numImagesToStore; i++)
        {
            pSwapchainImages[i] = m_properties.images[i];
        }

        if (numImagesToStore < m_properties.imageCount)
        {
            result = VK_INCOMPLETE;
        }

        *pCount = numImagesToStore;
    }

    return result;
}

// =====================================================================================================================
// Fills present information for a present being enqueued on a particular queue using a particular image.
VkResult SwapChain::GetPresentInfo(
    uint32_t                   deviceIdx,
    uint32_t                   imageIndex,
    Pal::PresentSwapChainInfo* pPresentInfo)
{
    // Get swap chain properties
    pPresentInfo->pSwapChain  = PalSwapChain(deviceIdx);
    pPresentInfo->pSrcImage   = GetPresentableImage(imageIndex)->PalImage(deviceIdx);
    pPresentInfo->presentMode = m_properties.imagePresentSupport;
    pPresentInfo->imageIndex  = imageIndex;

    VkResult result = VK_SUCCESS;

    // Let the fullscreen manager override some of this present information in case it has enabled
    // fullscreen presents.
    if ((m_pFullscreenMgr != nullptr))
    {
        result = m_pFullscreenMgr->UpdatePresentInfo(this, pPresentInfo);
    }

    return result;
}

// =====================================================================================================================
void SwapChain::MarkAsDeprecated()
{
    m_deprecated = true;
}

// =====================================================================================================================
VkResult SwapChain::AcquireWin32FullscreenOwnership(
    Device*     pDevice)
{
    VK_ASSERT(m_pFullscreenMgr != nullptr);

    return m_pFullscreenMgr->TryEnterExclusive(this) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
VkResult SwapChain::ReleaseWin32FullscreenOwnership(
    Device*     pDevice)
{
    VK_ASSERT(m_pFullscreenMgr != nullptr);

    return m_pFullscreenMgr->TryExitExclusive(this) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
FullscreenMgr::FullscreenMgr(
    Device*                         pDevice,
    FullscreenMgr::Mode             mode,
    const Pal::SwapChainCreateInfo& createInfo,
    Pal::IScreen*                   pScreen,
    uint32_t                        vidPnSourceId)
    :
    m_pDevice(pDevice),
    m_pScreen(pScreen),
    m_vidPnSourceId(vidPnSourceId),
    m_mode(mode),
    m_exclusiveModeAcquired(false)
{
    VK_ASSERT(m_pScreen != nullptr);

    m_compatFlags.u32All            = 0;
    m_exclusiveAccessFailCount      = 0;
    m_fullscreenPresentSuccessCount = 0;
    m_lastResolution.width          = 0;
    m_lastResolution.height         = 0;

    pScreen->GetColorCapabilities(&m_colorCaps);
}

// =====================================================================================================================
// Returns the current display resolution of the given screen.
static Pal::Result GetScreenResolution(
    Pal::IScreen*  pScreen,
    Pal::Extent2d* pResolution)
{
    // This code needs to live inside PAL.
    Pal::Result result = Pal::Result::ErrorUnknown;

    pResolution->width  = 0;
    pResolution->height = 0;

    VK_NOT_IMPLEMENTED;

    return result;
}

// =====================================================================================================================
// Returns the current window rectangle in desktop coordinates
static Pal::Result GetWindowRectangle(
    Pal::OsWindowHandle windowHandle,
    Pal::Rect*          pRect)
{
    // This code probably needs to live inside PAL.
    Pal::Result result = Pal::Result::ErrorUnknown;

    pRect->offset.x      = 0;
    pRect->offset.y      = 0;
    pRect->extent.width  = 0;
    pRect->extent.height = 0;

    VK_NOT_IMPLEMENTED;

    return result;
}

// =====================================================================================================================
// Attempt to enter exclusive access mode for the screen associated with this swap chain.
bool FullscreenMgr::TryEnterExclusive(
    SwapChain* pSwapChain)
{
    CompatibilityFlags compatFlags = EvaluateExclusiveModeCompat(pSwapChain);

    if (compatFlags.u32All != 0)
    {
        if (m_exclusiveModeAcquired)
        {
            TryExitExclusive(pSwapChain);
        }

        return false;
    }

    if (m_pScreen != nullptr && m_pImage != nullptr)
    {
        if (m_exclusiveModeAcquired == false)
        {
            Pal::Result result = pSwapChain->PalSwapChainWaitIdle();

            if (result == Pal::Result::Success)
            {
                const SwapChain::Properties&props = pSwapChain->GetProperties();

                result = m_pScreen->TakeFullscreenOwnership(*m_pImage->PalImage());

                // NOTE: ErrorFullscreenUnavailable means according to PAL, we already have exclusive access.
                if (result == Pal::Result::Success || result == Pal::Result::ErrorFullscreenUnavailable)
                {
                    m_exclusiveModeAcquired = true;

                    if (m_mode != Implicit)
                    {
                        m_colorParams.format     = VkToPalFormat(props.fullscreenSurfaceFormat.format).format;
                        m_colorParams.colorSpace = vk::convert::ScreenColorSpace(props.fullscreenSurfaceFormat.colorSpace);
                        m_colorParams.u32All     = 0;

                        m_pScreen->SetColorConfiguration(&m_colorParams);
                    }
                }
            }

            // If we fail to get exclusive access, increment a counter.
            if (m_exclusiveModeAcquired == false)
            {
                FullscreenPresentEvent(false);
            }
        }
    }

    return m_exclusiveModeAcquired;
}

// =====================================================================================================================
// Make the screen of the swap chain window screen exit exclusive access mode.
bool FullscreenMgr::TryExitExclusive(
    SwapChain* pSwapChain)
{
    if (pSwapChain != nullptr)
    {
        pSwapChain->PalSwapChainWaitIdle();

        const SwapChain::Properties& props = pSwapChain->GetProperties();
        PhysicalDeviceManager* pPhyicalDeviceManager = m_pDevice->VkPhysicalDevice()->Manager();
    }

    // if we acquired full screen ownership before with this fullscreenmanager.
    if ((m_pScreen != nullptr) && m_exclusiveModeAcquired)
    {
        Pal::Result palResult = m_pScreen->ReleaseFullscreenOwnership();
        VK_ASSERT(palResult == Pal::Result::Success);
    }

    m_exclusiveModeAcquired = false;

    return true;
}

// =====================================================================================================================
VkResult FullscreenMgr::SetHdrMetadata(
    Device*                    pDevice,
    const VkHdrMetadataEXT*    pMetadata)
{
    Pal::ColorGamut& palGamut = m_colorParams.userDefinedColorGamut;

    auto ConvertUnits = [] (float input) { return static_cast<uint32_t>(static_cast<double>(input) * 10000.0); };

    palGamut.chromaticityRedX         = ConvertUnits(pMetadata->displayPrimaryRed.x);
    palGamut.chromaticityRedY         = ConvertUnits(pMetadata->displayPrimaryRed.y);
    palGamut.chromaticityGreenX       = ConvertUnits(pMetadata->displayPrimaryGreen.x);
    palGamut.chromaticityGreenY       = ConvertUnits(pMetadata->displayPrimaryGreen.y);
    palGamut.chromaticityBlueX        = ConvertUnits(pMetadata->displayPrimaryBlue.x);
    palGamut.chromaticityBlueY        = ConvertUnits(pMetadata->displayPrimaryBlue.y);
    palGamut.chromaticityWhitePointX  = ConvertUnits(pMetadata->whitePoint.x);
    palGamut.chromaticityWhitePointY  = ConvertUnits(pMetadata->whitePoint.y);
    palGamut.minLuminance             = ConvertUnits(pMetadata->minLuminance);
    palGamut.maxLuminance             = ConvertUnits(pMetadata->maxLuminance);

    // TODO: I don't know if average luminance is important, but VK_EXT_hdr_metadata does not currently expose it.
    // ie. palGamut.avgLuminance = ConvertUnits(pMetadata->avgLuminance);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Checks if the current runtime settings allow implicit fullscreen to be enabled.
static bool SettingsEnableImplicitFullscreen(
    const Device&                   device,
    const VkSwapchainCreateInfoKHR& createInfo)
{
    const uint32_t flags = device.GetRuntimeSettings().backgroundFullscreenPresent;
    bool enabled = false;

    if (flags != 0)
    {
        enabled = true;

        const VkPresentModeKHR pmode = createInfo.presentMode;

        enabled = false;
    }

    return enabled;
}

// =====================================================================================================================
// Based on panel settings in comparison with the current OS / swapchain configuration as well as other criteria,
// figures out if implicit fullscreen can and should be enabled for this swapchain.
static bool EnableFullScreen(
    const Device*                   pDevice,
    const SwapChain::Properties&    swapchainProps,
    FullscreenMgr::Mode             mode,
    const VkSwapchainCreateInfoKHR& createInfo,
    Pal::IScreen**                  ppScreen)
{
     bool enabled = SettingsEnableImplicitFullscreen(*pDevice, createInfo) || (mode != FullscreenMgr::Implicit);

    // Test whether the given present mode is compatible with full screen presents
    if (enabled)
    {
        // Get all supported fullscreen present modes (the separate count variable here is in fact needed because
        // otherwise GCC complains about a non-integral variable size).
        constexpr size_t SwapChainCount = static_cast<size_t>(Pal::SwapChainMode::Count);

        VkPresentModeKHR presentModes[SwapChainCount] = {};
        uint32_t modeCount = VK_ARRAY_SIZE(presentModes);

        VkResult result = pDevice->VkPhysicalDevice()->GetSurfacePresentModes(
            swapchainProps.displayableInfo, Pal::PresentMode::Fullscreen, &modeCount, presentModes);

        VK_ASSERT(result != VK_INCOMPLETE);

        if (result == VK_SUCCESS)
        {
            // Find whether the requested present mode is one of the supported ones
            enabled = false;

            for (uint32_t i = 0; i < modeCount; ++i)
            {
                if (presentModes[i] == createInfo.presentMode)
                {
                    enabled = true;

                    break;
                }
            }
        }
        else
        {
            enabled = false;
        }
    }

    // Try to find which monitor is associated with the given window handle.  We need this information to initialize
    // fullscreen
    if (enabled)
    {
        // TODO SWDEV-120359 - We need to enumerate the correct Pal device.
        Pal::IScreen* pScreen = pDevice->VkInstance()->FindScreen(pDevice->PalDevice(),
            swapchainProps.displayableInfo.windowHandle,
            (mode == FullscreenMgr::Explicit) ? swapchainProps.pFullscreenSurface->GetOSDisplayHandle() :
                                                swapchainProps.pSurface->GetOSDisplayHandle());

        if (pScreen != nullptr)
        {
            *ppScreen = pScreen;
        }
        else
        {
            enabled = false;
        }
    }

    return enabled;
}

// =====================================================================================================================
// This function should be called by the swap chain creation logic before the presentable images are created.  It will
// edit their create info to be fullscreen compatible.
void FullscreenMgr::PreImageCreate(
    Pal::PresentMode*                pImagePresentSupport,
    Pal::PresentableImageCreateInfo* pImageInfo)
{
    if (m_compatFlags.disabled == 0)
    {
        // If we found that screen, then make the images compatible with fullscreen presents to that monitor.  Note that
        // this does not make them incompatible with windowed blit presents -- it just chooses a displayable tiling
        // configuration.
        VK_ASSERT(m_pScreen != nullptr);

        if ((pImageInfo->extent.width > 0) && (pImageInfo->extent.height > 0))
        {
            *pImagePresentSupport = Pal::PresentMode::Fullscreen;
            pImageInfo->flags.fullscreen = 1;
            pImageInfo->pScreen = m_pScreen;
        }
    }
}

// =====================================================================================================================
// Call this function after the presentable images have been created.
void FullscreenMgr::PostImageCreate(
    const Image* pImage)
{
    m_pImage = pImage;

    if (m_pImage == nullptr)
    {
        DisableFullscreenPresents();
    }
}

// =====================================================================================================================
// Called when we either attempted to do a fullscreen present or enter exclusive mode.  This is used to track success
// and failure statistics and disable the logic in the case of unexpected OS behavior that may cause aberrant display
// flickering.
void FullscreenMgr::FullscreenPresentEvent(bool success)
{
    const auto& settings = m_pDevice->VkPhysicalDevice()->GetRuntimeSettings();

    if (success)
    {
        m_fullscreenPresentSuccessCount++;

        // Need this many consecutive successful fullscreen presents before we consider resetting the failure
        // count.  This is used to prevent a "ping-pong"-ing situation where, e.g. every third present somehow
        // fails in fullscreen.
        if (m_fullscreenPresentSuccessCount >= settings.backgroundFullscreenSuccessResetCount)
        {
            m_exclusiveAccessFailCount      = 0;
            m_fullscreenPresentSuccessCount = 0;
        }
    }
    else
    {
        // After a certain number of failures, permanently disable fullscreen presents.
        m_exclusiveAccessFailCount++;
        m_fullscreenPresentSuccessCount = 0;

        if (m_exclusiveAccessFailCount >= settings.backgroundFullscreenFailureDisableCount)
        {
            DisableFullscreenPresents();
        }
    }
}

// =====================================================================================================================
// Called when the owning swap chain is being destroyed
void FullscreenMgr::Destroy(const VkAllocationCallbacks* pAllocator)
{
    // The swap chain is going down.  We need to force exiting fullscreen exclusive mode no matter what.
    TryExitExclusive(nullptr);

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Permanently disables any attempt to do fullscreen presents using this swapchain.
void FullscreenMgr::DisableFullscreenPresents()
{
    // Clear all failure flags and disable fullscreen presents.
    m_compatFlags.u32All   = 0;
    m_compatFlags.disabled = 1;

    TryExitExclusive(nullptr);
}

// =====================================================================================================================
// This function enters/exits exclusive access mode on this swap chain's screen when possible.
void FullscreenMgr::UpdateExclusiveMode(
    SwapChain* pSwapChain)
{
    // If we are not perma-disabled
    if (m_compatFlags.disabled == 0)
    {
        // Update current exclusive access compatibility
        m_compatFlags = EvaluateExclusiveModeCompat(pSwapChain);

        // Try to enter exclusive access mode if we are currently compatible with it and vice versa.
        if (IsExclusiveModePossible() != m_exclusiveModeAcquired)
        {
            if (m_exclusiveModeAcquired == false)
            {
                TryEnterExclusive(pSwapChain);
            }
            else
            {
                TryExitExclusive(pSwapChain);
            }
        }
    }
    else
    {
        VK_ASSERT(m_exclusiveModeAcquired == false);
    }
}

// =====================================================================================================================
// Called after a present operation on the given queue using the swap chain
void FullscreenMgr::PostPresent(
    SwapChain*                       pSwapChain,
    const Pal::PresentSwapChainInfo& presentInfo,
    Pal::Result*                     pPresentResult)
{
    if (*pPresentResult == Pal::Result::Success)
    {
        // If we succeeded on a fullscreen present, we should reset the consecutive fullscreen failure count
        if (presentInfo.presentMode == Pal::PresentMode::Fullscreen)
        {
            VK_ASSERT(IsExclusiveModePossible());

            FullscreenPresentEvent(true);
        }
    }
    else if (m_compatFlags.disabled == 0)
    {
        // If we failed a fullscreen present for whatever reason, increment the failure counter
        if (presentInfo.presentMode == Pal::PresentMode::Fullscreen)
        {
            FullscreenPresentEvent(false);
        }

        // If we think we are in fullscreen exclusive mode, but the Present function corrects us, update our
        // internal state.  The Present we just did is lost, but at least the next Present won't be.
        if (*pPresentResult == Pal::Result::ErrorFullscreenUnavailable)
        {
            // Exit fullscreen exclusive mode immediately.  This should also put PAL's internal state back in sync
            // with the monitor's actual state, in case it's out of sync as well.
            TryExitExclusive(pSwapChain);

            VK_ASSERT(m_exclusiveModeAcquired == false);

            *pPresentResult = Pal::Result::Success;
        }
    }
    else
    {
        VK_ASSERT(presentInfo.presentMode != Pal::PresentMode::Fullscreen);
    }

    // There are cases under extreme alt-tabbing when DWM may return a null shared window handle (the windowed
    // blit destination surface).  This will then subsequently cause PAL to fail that windowed present.
    //
    // I think this happens if the app tries to present either while we are in the process of abandoning
    // exclusive access or very shortly before it.  It seems we can just ignore those errors and not affect
    // things as they only affect attempts to windowed present during an exclusive access switch.
    if ((presentInfo.presentMode != Pal::PresentMode::Fullscreen) &&
        (*pPresentResult == Pal::Result::ErrorUnknown))
    {
        *pPresentResult = Pal::Result::Success;
    }

    // Hide any present error if we have disabled them via panel
    if (m_pDevice->VkPhysicalDevice()->GetRuntimeSettings().backgroundFullscreenIgnorePresentErrors)
    {
        *pPresentResult = Pal::Result::Success;
    }
}

// =====================================================================================================================
// This function evaluates compatibility flags for whether it's safe to take exclusive access of the screen without
// the user being (more or less) able to tell the difference.  This is only possible if multiple conditions are all
// true.
FullscreenMgr::CompatibilityFlags FullscreenMgr::EvaluateExclusiveModeCompat(
    const SwapChain* pSwapChain) const
{
    CompatibilityFlags compatFlags = {};

    if (m_mode == Explicit)
    {
        // In explicit mode, we allow the fullscreen manager to acquire fullscreen ownership, regardless of
        // size changes or lost window focus.
        return compatFlags;
    }

    Pal::OsWindowHandle  windowHandle  = pSwapChain->GetProperties().displayableInfo.windowHandle;
    Pal::OsDisplayHandle displayHandle = pSwapChain->GetProperties().displayableInfo.displayHandle;

    VK_ASSERT(m_pScreen != nullptr);

    constexpr Pal::OsDisplayHandle unknownHandle = 0;

    Pal::IScreen* pScreen = m_pDevice->VkInstance()->FindScreen(
                m_pDevice->PalDevice(), windowHandle, displayHandle);

    if (pScreen != m_pScreen)
    {
        compatFlags.screenChanged = 1;
    }

    VK_ASSERT(m_pImage != nullptr);

    const auto& imageInfo = m_pImage->PalImage()->GetImageCreateInfo();

    Pal::Extent2d lastResolution = {};

    if (compatFlags.screenChanged == 0)
    {
        compatFlags.resolutionBad = 1;
        compatFlags.windowRectBad = 1;

        // Get the current resolution of the screen
        Pal::Result resolutionResult = GetScreenResolution(m_pScreen, &lastResolution);

        if (resolutionResult == Pal::Result::Success)
        {
            // Test that the current screen resolution matches current swap chain extents
            if ((lastResolution.width == imageInfo.extent.width) &&
                (lastResolution.height == imageInfo.extent.height))
            {
                compatFlags.resolutionBad = 0;
            }

            // Test that the current window rectangle (in desktop coordinates) covers the whole monitor resolution
            Pal::Rect windowRect = {};

            if (GetWindowRectangle(windowHandle, &windowRect) == Pal::Result::Success)
            {
                if ((windowRect.offset.x == 0) &&
                    (windowRect.offset.y == 0) &&
                    (windowRect.extent.width  == lastResolution.width) &&
                    (windowRect.extent.height == lastResolution.height))
                {
                    compatFlags.windowRectBad = 0;
                }
            }
        }
    }

    if (compatFlags.u32All == 0)
    {
        compatFlags.windowNotForeground = 1;
    }

    return compatFlags;
}

// =====================================================================================================================
// This function evaluates compatibility flags for whether it's safe to take exclusive access of the screen without
// the user being (more or less) able to tell the difference.  This is only possible if multiple conditions are all
// true.
bool FullscreenMgr::UpdateExclusiveModeCompat(
    SwapChain* pSwapChain)
{
    if (m_compatFlags.disabled == 0)
    {
        m_compatFlags = EvaluateExclusiveModeCompat(pSwapChain);
    }

    return m_compatFlags.u32All == 0;
}

// =====================================================================================================================
// This function potentially overrides normal swap chain present info by replacing a windowed present with a page-
// flipped fullscreen present.
//
// This can only happen if the screen is currently compatible with fullscreen presents and we have successfully
// acquired exclusive access to the screen.
VkResult FullscreenMgr::UpdatePresentInfo(
    SwapChain*                 pSwapChain,
    Pal::PresentSwapChainInfo* pPresentInfo)
{
    VkResult result = VK_SUCCESS;

    if (m_compatFlags.disabled == 0)
    {
        // Try to get exclusive access if things are compatible
        UpdateExclusiveMode(pSwapChain);
    }

    switch(m_mode)
    {
    case Implicit:
        if (m_compatFlags.disabled == 0)
        {
            // If we've successfully entered exclusive mode, switch to fullscreen presents
            if (m_exclusiveModeAcquired)
            {
                VK_ASSERT(IsExclusiveModePossible());

                pPresentInfo->presentMode = Pal::PresentMode::Fullscreen;
            }
            else
            {
                pPresentInfo->presentMode = Pal::PresentMode::Windowed;
            }
        }
        else
        {
            // set the presentMode to windoed if fullscreen is disabled
            pPresentInfo->presentMode = Pal::PresentMode::Windowed;
        }
        break;

    case Explicit:
        pPresentInfo->presentMode = Pal::PresentMode::Fullscreen;
        break;

    case Explicit_Mixed:
        {
            pPresentInfo->presentMode =
                m_exclusiveModeAcquired ? Pal::PresentMode::Fullscreen : Pal::PresentMode::Windowed;
        }
        break;

      default:
        VK_NOT_IMPLEMENTED;
        break;
    };

    return result;
}

// =====================================================================================================================
FullscreenMgr::~FullscreenMgr()
{

}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice                                    device,
    VkSwapchainKHR                              swapchain,
    const VkAllocationCallbacks*                pAllocator)
{
    if (swapchain != VK_NULL_HANDLE)
    {
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        SwapChain::ObjectFromHandle(swapchain)->Destroy(pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    uint32_t*                                    pSwapchainImageCount,
    VkImage*                                     pSwapchainImages)
{
    return SwapChain::ObjectFromHandle(swapchain)->GetSwapchainImagesKHR(pSwapchainImageCount, pSwapchainImages);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    uint64_t                                     timeout,
    VkSemaphore                                  semaphore,
    VkFence                                      fence,
    uint32_t*                                    pImageIndex)
{
    constexpr uint32_t deviceMask = 1;

    const VkAcquireNextImageInfoKHX acquireInfo =
    {
        VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHX,
        nullptr,
        swapchain,
        timeout,
        semaphore,
        fence,
        deviceMask
    };

    union
    {
        const VkStructHeader*             pHeader;
        const VkAcquireNextImageInfoKHX*  pAcquireInfoKHX;
    };

    pAcquireInfoKHX = &acquireInfo;

    return SwapChain::ObjectFromHandle(swapchain)->AcquireNextImage(pHeader, pImageIndex);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHX(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHX*            pAcquireInfo,
    uint32_t*                                   pImageIndex)
{
    union
    {
        const VkStructHeader*             pHeader;
        const VkAcquireNextImageInfoKHX*  pAcquireInfoKHX;
    };

    pAcquireInfoKHX = pAcquireInfo;

    return SwapChain::ObjectFromHandle(pAcquireInfo->swapchain)->AcquireNextImage(pHeader, pImageIndex);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHR(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHR*            pAcquireInfo,
    uint32_t*                                   pImageIndex)
{
    union
    {
        const VkStructHeader*             pHeader;
        const VkAcquireNextImageInfoKHR*  pAcquireInfoKHR;
    };

    pAcquireInfoKHR = pAcquireInfo;

    return SwapChain::ObjectFromHandle(pAcquireInfo->swapchain)->AcquireNextImage(pHeader, pImageIndex);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkSetHdrMetadataEXT(
    VkDevice                                    device,
    uint32_t                                    swapchainCount,
    const VkSwapchainKHR*                       pSwapchains,
    const VkHdrMetadataEXT*                     pMetadata)
{
    VkResult result = VK_SUCCESS;

    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    for (uint32_t swapChainIndex = 0; (swapChainIndex < swapchainCount) && (result == VK_SUCCESS); swapChainIndex++)
    {
        result = SwapChain::ObjectFromHandle(
                    pSwapchains[swapChainIndex])->GetFullscreenMgr()->SetHdrMetadata(pDevice, pMetadata);
    }

    VK_ASSERT(result == VK_SUCCESS);
}

} // namespace entry

} // namespace vk
