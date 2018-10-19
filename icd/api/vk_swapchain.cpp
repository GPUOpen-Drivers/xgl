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

// These WSI header files should be included before vk_wsi_swapchain.h
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
SwapChain::SwapChain(
    Device*             pDevice,
    const Properties&   properties,
    VkPresentModeKHR    presentMode,
    FullscreenMgr*      pFullscreenMgr,
    SwCompositor*       pSwCompositor,
    Pal::ISwapChain*    pPalSwapChain[])
    :
    m_pDevice(pDevice),
    m_properties(properties),
    m_nextImage(0),
    m_pFullscreenMgr(pFullscreenMgr),
    m_pSwCompositor(pSwCompositor),
    m_appOwnedImageCount(0),
    m_presentCount(0),
    m_presentMode(presentMode),
    m_deprecated(false)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        m_pPalSwapChain[deviceIdx] = pPalSwapChain[deviceIdx];
    }
}

// =====================================================================================================================
// Creates a new Vulkan API swap chain object
VkResult SwapChain::Create(
    Device*                                 pDevice,
    const VkSwapchainCreateInfoKHR*         pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkSwapchainKHR*                         pSwapChain)
{
    VkResult result = VK_SUCCESS;

    const VkDeviceGroupSwapchainCreateInfoKHR*  pDeviceGroupExt = nullptr;

    Properties properties = {};

    union
    {
        const VkStructHeader*                      pHeader;
        const VkSwapchainCreateInfoKHR*            pVkSwapchainCreateInfoKHR;
        const VkDeviceGroupSwapchainCreateInfoKHR* pVkDeviceGroupSwapchainCreateInfoKHR;
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

                properties.imageCreateInfo.swizzledFormat     = VkToPalFormat(pVkSwapchainCreateInfoKHR->imageFormat);
                properties.imageCreateInfo.flags.stereo       = properties.stereo;
                properties.imageCreateInfo.flags.peerWritable = (pDevice->NumPalDevices() > 1) ? 1 : 0;

                VkFormatProperties formatProperties;
                pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetFormatProperties(pVkSwapchainCreateInfoKHR->imageFormat,
                                                                 &formatProperties);
                VkImageUsageFlags imageUsage = pVkSwapchainCreateInfoKHR->imageUsage;
                imageUsage &=
                    VkFormatFeatureFlagsToImageUsageFlags(formatProperties.optimalTilingFeatures);

                properties.imageCreateInfo.usage    = VkToPalImageUsageFlags(imageUsage,
                                                                             pVkSwapchainCreateInfoKHR->imageFormat,
                                                                             1,
                                                                             (VkImageUsageFlags)(0),
                                                                             (VkImageUsageFlags)(0));
                properties.imageCreateInfo.extent   = VkToPalExtent2d(pVkSwapchainCreateInfoKHR->imageExtent);
                properties.imageCreateInfo.hDisplay = properties.displayableInfo.displayHandle;
                properties.imageCreateInfo.hWindow  = properties.displayableInfo.windowHandle;

            }
            break;

        case VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR:
            pDeviceGroupExt = pVkDeviceGroupSwapchainCreateInfoKHR;
            properties.summedImage = (pDeviceGroupExt->modes & VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR) != 0;
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
    swapChainCreateInfo.imageSwizzledFormat = properties.imageCreateInfo.swizzledFormat;
    swapChainCreateInfo.imageExtent         = VkToPalExtent2d(pCreateInfo->imageExtent);
    swapChainCreateInfo.imageUsageFlags     = VkToPalImageUsageFlags(pCreateInfo->imageUsage,
                                                                     pCreateInfo->imageFormat,
                                                                     1,
                                                                     (VkImageUsageFlags)(0),
                                                                     (VkImageUsageFlags)(0));
    swapChainCreateInfo.preTransform        = Pal::SurfaceTransformNone;
    swapChainCreateInfo.imageArraySize      = 1;
    swapChainCreateInfo.swapChainMode       = VkToPalSwapChainMode(pCreateInfo->presentMode);

    if (properties.displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
        swapChainCreateInfo.pScreen = properties.displayableInfo.pScreen;
#endif
    }

    // Allocate system memory for objects
    const size_t    vkSwapChainSize  = sizeof(SwapChain);
    size_t          palSwapChainSize = 0;

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        palSwapChainSize += pDevice->PalDevice(deviceIdx)->GetSwapChainSize(swapChainCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);
    }

    size_t          imageArraySize   = sizeof(VkImage) * swapImageCount;
    size_t          memoryArraySize  = sizeof(VkDeviceMemory) * swapImageCount;
    size_t          objSize          = vkSwapChainSize +
                                       palSwapChainSize +
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

        offset += pPalDevice->GetSwapChainSize(swapChainCreateInfo, &palResult);
    }
    VK_ASSERT((palSwapChainSize + vkSwapChainSize) == offset);
    result = PalToVkResult(palResult);

    if (result == VK_SUCCESS)
    {
        properties.imageCreateInfo.pSwapChain = pPalSwapChain[DefaultDeviceIndex];
    }

    // Allocate memory for the fullscreen manager if it's enabled.  We need to create it first before the
    // swap chain because it needs to have a say in how presentable images are created.
    Pal::IScreen*  pScreen        = nullptr;
    FullscreenMgr* pFullscreenMgr = nullptr;
    SwCompositor*  pSwCompositor  = nullptr;

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
                pScreen,
                props.hDisplay,
                swapChainCreateInfo.hWindow,
                props.vidPnSourceId);
        }
    }

    if (pFullscreenMgr != nullptr)
    {
        // Update the image create info to make them compatible with optional fullscreen presents.
        pFullscreenMgr->PreImageCreate(
            &properties.imagePresentSupport,
            &properties.imageCreateInfo);
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
                &properties.imageCreateInfo,
                pAllocator,
                pCreateInfo->imageUsage,
                properties.imagePresentSupport,
                &properties.images[properties.imageCount],
                pCreateInfo->imageFormat,
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
        // Initialize the fullscreen manager after presentable image creation
        VkImage anyImage = (properties.imageCount > 0) ? properties.images[0] : VK_NULL_HANDLE;

        pFullscreenMgr->PostImageCreate(Image::ObjectFromHandle(anyImage));

        // Only create a SW compositor for MGPU if fullscreen doesn't support HW compositing
        if (pDevice->NumPalDevices() > 1)
        {
            Pal::GetPrimaryInfoInput primaryInfoInput = {};

            primaryInfoInput.vidPnSrcId     = pFullscreenMgr->GetVidPnSourceId();
            primaryInfoInput.width          = properties.imageCreateInfo.extent.width;
            primaryInfoInput.height         = properties.imageCreateInfo.extent.height;
            primaryInfoInput.swizzledFormat = properties.imageCreateInfo.swizzledFormat;

            Pal::GetPrimaryInfoOutput primaryInfoOutput = {};

            pDevice->PalDevice(DefaultDeviceIndex)->GetPrimaryInfo(primaryInfoInput, &primaryInfoOutput);

            if ((primaryInfoOutput.flags.dvoHwMode | primaryInfoOutput.flags.xdmaHwMode) == 0)
            {
                VK_ASSERT(primaryInfoOutput.flags.swMode);

                pSwCompositor = SwCompositor::Create(pDevice,
                                                     pAllocator,
                                                     properties,
                                                     pDevice->GetRuntimeSettings().useSdmaCompositingBlt);
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) SwapChain(pDevice,
                                            properties,
                                            pCreateInfo->presentMode,
                                            pFullscreenMgr,
                                            pSwCompositor,
                                            pPalSwapChain);

        *pSwapChain = SwapChain::HandleFromVoidPointer(pMemory);

        SwapChain* pObject = SwapChain::ObjectFromHandle(*pSwapChain);

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

    if (m_pSwCompositor != nullptr)
    {
        m_pSwCompositor->Destroy(m_pDevice, pAllocator);
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
// Destroy all of the PAL swap chains
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
        const VkAcquireNextImageInfoKHR*  pVkAcquireNextImageInfoKHR;
    };

    uint32_t presentationDeviceIdx = DefaultDeviceIndex;

    for (pHeader = pAcquireInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR:
        {
            semaphore = pVkAcquireNextImageInfoKHR->semaphore;
            fence     = pVkAcquireNextImageInfoKHR->fence;
            timeout   = pVkAcquireNextImageInfoKHR->timeout;

            Util::BitMaskScanForward(&presentationDeviceIdx, pVkAcquireNextImageInfoKHR->deviceMask);

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
            acquireInfo.pSemaphore = (pSemaphore != nullptr) ? pSemaphore->PalSemaphore(DefaultDeviceIndex) : nullptr;
            acquireInfo.pFence     = (pFence != nullptr) ? pFence->PalFence(presentationDeviceIdx) : nullptr;

            if (pFence != nullptr)
            {
                pFence->SetActiveDevice(presentationDeviceIdx);
            }

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
// Performs fullscreen ownership transitions as well as MGPU software composition when necessary prior to a present
// being enqueued on a particular queue using a particular image.
// Returns the presentation device index in case the swapchain/device properties can't perform HW composition.
Pal::IQueue* SwapChain::PrePresent(
    uint32_t                   deviceIdx,
    uint32_t                   imageIndex,
    Pal::PresentSwapChainInfo* pPresentInfo,
    const Queue*               pPresentQueue)
{
    // Get swap chain properties
    pPresentInfo->pSwapChain  = PalSwapChain(deviceIdx);
    pPresentInfo->pSrcImage   = GetPresentableImage(imageIndex)->PalImage(deviceIdx);
    pPresentInfo->presentMode = m_properties.imagePresentSupport;
    pPresentInfo->imageIndex  = imageIndex;

    // Let the fullscreen manager override some of this present information in case it has enabled
    // fullscreen presents.
    if (m_pFullscreenMgr != nullptr)
    {
        m_pFullscreenMgr->UpdatePresentInfo(this, pPresentInfo);
    }

    // The presentation queue will be unchanged unless SW composition is needed.
    Pal::IQueue* pPalQueue = pPresentQueue->PalQueue(deviceIdx);

    if (m_pSwCompositor != nullptr)
    {
        // If the original software compositor doesn't support this presentation queue, the internal queue SDMA
        // approach must be used instead.
        if ((m_pDevice->GetRuntimeSettings().useSdmaCompositingBlt == false) &&
            (m_pSwCompositor->GetQueueType() != m_pDevice->GetQueueFamilyPalQueueType(pPresentQueue->GetFamilyIndex())))
        {
            const VkAllocationCallbacks* pAllocCallbacks = m_pDevice->VkInstance()->GetAllocCallbacks();

            m_pSwCompositor->Destroy(m_pDevice, pAllocCallbacks);
            m_pSwCompositor = SwCompositor::Create(m_pDevice, pAllocCallbacks, m_properties, true);
        }

        if (m_pSwCompositor != nullptr)
        {
            pPalQueue = m_pSwCompositor->DoSwCompositing(m_pDevice,
                                                         deviceIdx,
                                                         imageIndex,
                                                         pPresentInfo,
                                                         pPresentQueue);
        }
    }

    return pPalQueue;
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
    Pal::IScreen*                   pScreen,
    Pal::OsDisplayHandle            hDisplay,
    Pal::OsWindowHandle             hWindow,
    uint32_t                        vidPnSourceId)
    :
    m_pDevice(pDevice),
    m_exclusiveModeFlags(),
    m_pScreen(pScreen),
    m_exclusiveAccessFailCount(0),
    m_fullscreenPresentSuccessCount(0),
    m_hDisplay(hDisplay),
    m_hWindow(hWindow),
    m_vidPnSourceId(vidPnSourceId),
    m_mode(mode)
{
    VK_ASSERT(m_pScreen != nullptr);

    pScreen->GetColorCapabilities(&m_colorCaps);
}

// =====================================================================================================================
// Attempt to enter exclusive access mode for the screen associated with this swap chain.  If in exclusive mode already
// do nothing or exit exclusive mode if fullscreen compatibility is lost.
bool FullscreenMgr::TryEnterExclusive(
    SwapChain* pSwapChain)
{
    // If we are not perma-disabled
    if (m_exclusiveModeFlags.disabled == 0)
    {
        Pal::Result result = Pal::Result::Success;

        // In explicit mode, we allow the fullscreen manager to acquire fullscreen ownership, regardless of
        // size changes or lost window focus.
        if (m_mode != Explicit)
        {
            VK_ASSERT(m_pImage != nullptr);

            const auto& imageInfo = m_pImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

            Pal::Extent2d imageExtent;

            imageExtent.width  = imageInfo.extent.width;
            imageExtent.height = imageInfo.extent.height;

            // Update current exclusive access compatibility
            result = m_pScreen->IsImplicitFullscreenOwnershipSafe(m_hDisplay, m_hWindow, imageExtent);

        }

        // Exit exclusive access mode if no longer compatible or try to enter (or simply remain in) if we are currently
        // compatible
        if (m_exclusiveModeFlags.acquired && (result != Pal::Result::Success))
        {
            TryExitExclusive(pSwapChain);
        }
        else if ((m_exclusiveModeFlags.acquired == 0) && (result == Pal::Result::Success))
        {
            if (m_pScreen != nullptr && m_pImage != nullptr)
            {
                result = pSwapChain->PalSwapChainWaitIdle();

                if (result == Pal::Result::Success)
                {
                    const SwapChain::Properties&props = pSwapChain->GetProperties();

                    result = m_pScreen->TakeFullscreenOwnership(*m_pImage->PalImage(DefaultDeviceIndex));

                    // NOTE: ErrorFullscreenUnavailable means according to PAL, we already have exclusive access.
                    if (result == Pal::Result::Success || result == Pal::Result::ErrorFullscreenUnavailable)
                    {
                        m_exclusiveModeFlags.acquired = 1;

                        if (m_mode != Implicit)
                        {
                            m_colorParams.format     = VkToPalFormat(props.fullscreenSurfaceFormat.format).format;
                            m_colorParams.colorSpace = VkToPalScreenSpace(props.fullscreenSurfaceFormat);
                            m_colorParams.u32All     = 0;

                            m_pScreen->SetColorConfiguration(&m_colorParams);
                        }
                    }
                }

                // If we fail to get exclusive access, increment a counter.
                if (m_exclusiveModeFlags.acquired == 0)
                {
                    FullscreenPresentEvent(false);
                }
            }
        }
    }
    else
    {
        VK_ASSERT(m_exclusiveModeFlags.acquired == 0);
    }

    return m_exclusiveModeFlags.acquired;
}

// =====================================================================================================================
// Make the screen of the swap chain window screen exit exclusive access mode.
bool FullscreenMgr::TryExitExclusive(
    SwapChain* pSwapChain)
{
    if (pSwapChain != nullptr)
    {
        pSwapChain->PalSwapChainWaitIdle();
    }

    // if we acquired full screen ownership before with this fullscreenmanager.
    if ((m_pScreen != nullptr) && m_exclusiveModeFlags.acquired)
    {
        Pal::Result palResult = m_pScreen->ReleaseFullscreenOwnership();
        VK_ASSERT(palResult == Pal::Result::Success);
    }

    m_exclusiveModeFlags.acquired = 0;

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

    // max already in nits
    palGamut.maxLuminance             = static_cast<uint32_t>(pMetadata->maxLuminance);

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

        VkResult result = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetSurfacePresentModes(
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
        Pal::IScreen* pScreen = pDevice->VkInstance()->FindScreen(
            pDevice->PalDevice(DefaultDeviceIndex),
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
    if (m_exclusiveModeFlags.disabled == 0)
    {
        // If we found that screen, then make the images compatible with fullscreen presents to that monitor.  Note that
        // this does not make them incompatible with windowed blit presents -- it just chooses a displayable tiling
        // configuration.
        VK_ASSERT(m_pScreen != nullptr);

        if ((pImageInfo->extent.width > 0) && (pImageInfo->extent.height > 0))
        {
            *pImagePresentSupport        = Pal::PresentMode::Fullscreen;
            pImageInfo->flags.fullscreen = 1;
            pImageInfo->pScreen          = m_pScreen;
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
    const auto& settings = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();

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
    m_exclusiveModeFlags.disabled = 1;

    TryExitExclusive(nullptr);
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
            VK_ASSERT(m_exclusiveModeFlags.disabled == 0);

            FullscreenPresentEvent(true);
        }
    }
    else if (m_exclusiveModeFlags.disabled == 0)
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

            VK_ASSERT(m_exclusiveModeFlags.acquired == 0);

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
    if (m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings().backgroundFullscreenIgnorePresentErrors)
    {
        *pPresentResult = Pal::Result::Success;
    }
}

// =====================================================================================================================
// This function potentially overrides normal swap chain present info by replacing a windowed present with a page-
// flipped fullscreen present.
//
// This can only happen if the screen is currently compatible with fullscreen presents and we have successfully
// acquired exclusive access to the screen.
void FullscreenMgr::UpdatePresentInfo(
    SwapChain*                 pSwapChain,
    Pal::PresentSwapChainInfo* pPresentInfo)
{
    // Try to enter (or remain in) exclusive access mode on this swap chain's screen for this present
    TryEnterExclusive(pSwapChain);

    switch(m_mode)
    {
    case Implicit:
        if (m_exclusiveModeFlags.disabled == 0)
        {
            // If we've successfully entered exclusive mode, switch to fullscreen presents
            if (m_exclusiveModeFlags.acquired)
            {
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
                m_exclusiveModeFlags.acquired ? Pal::PresentMode::Fullscreen : Pal::PresentMode::Windowed;
        }
        break;

      default:
        VK_NOT_IMPLEMENTED;
        break;
    };
}

// =====================================================================================================================
FullscreenMgr::~FullscreenMgr()
{

}

// =====================================================================================================================
// Construct the software compositor object
SwCompositor::SwCompositor(
    const Device*     pDevice,
    uint32_t          presentationDeviceIdx,
    uint32_t          imageCount,
    Pal::QueueType    queueType,
    Pal::IImage**     ppBltImages[],
    Pal::IGpuMemory** ppBltMemory[],
    Pal::ICmdBuffer** ppBltCmdBuffers[])
    :
    m_presentationDeviceIdx(presentationDeviceIdx),
    m_imageCount(imageCount),
    m_queueType(queueType)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        m_ppBltImages[deviceIdx]     = ppBltImages[deviceIdx];
        m_ppBltMemory[deviceIdx]     = ppBltMemory[deviceIdx];
        m_ppBltCmdBuffers[deviceIdx] = ppBltCmdBuffers[deviceIdx];

        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            m_ppBltImages[deviceIdx][i]     = nullptr;
            m_ppBltMemory[deviceIdx][i]     = nullptr;
            m_ppBltCmdBuffers[deviceIdx][i] = nullptr;
        }
    }
}

// =====================================================================================================================
// One time setup for this swapchain/device combination.  Creates intermediate images and command buffers to perform the
// composition BLTs to the presentation device.
SwCompositor* SwCompositor::Create(
    const Device*                pDevice,
    const VkAllocationCallbacks* pAllocator,
    const SwapChain::Properties& properties,
    bool                         useSdmaCompositingBlt)
{
    VkResult      result  = VK_SUCCESS;
    SwCompositor* pObject = nullptr;

    // Find the device that can perform the actual present
    uint32_t presentationDeviceIdx = DefaultDeviceIndex;

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (pDevice->VkPhysicalDevice(deviceIdx)->PalProperties().attachedScreenCount > 0)
        {
            presentationDeviceIdx = deviceIdx;
            break;
        }
    }

    Pal::IDevice* pPalDevice        = pDevice->PalDevice(presentationDeviceIdx);
    size_t        palImageSize      = 0;
    size_t        palMemorySize     = 0;
    size_t        palPeerImageSize  = 0;
    size_t        palPeerMemorySize = 0;
    size_t        palCmdBufferSize  = 0;
    Pal::Result   palResult;

    pPalDevice->GetPresentableImageSizes(properties.imageCreateInfo, &palImageSize, &palMemorySize, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    Pal::PeerImageOpenInfo peerInfo = {};
    peerInfo.pOriginalImage = Image::ObjectFromHandle(properties.images[0])->PalImage(DefaultDeviceIndex);

    pPalDevice->GetPeerImageSizes(peerInfo, &palPeerImageSize, &palPeerMemorySize, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    Pal::CmdBufferCreateInfo cmdBufCreateInfo = {};

    if (useSdmaCompositingBlt)
    {
        cmdBufCreateInfo.queueType  = Pal::QueueType::QueueTypeDma;
        cmdBufCreateInfo.engineType = Pal::EngineType::EngineTypeDma;
    }
    else
    {
        cmdBufCreateInfo.queueType  = Pal::QueueType::QueueTypeUniversal;
        cmdBufCreateInfo.engineType = Pal::EngineType::EngineTypeUniversal;
    }

    palCmdBufferSize = pPalDevice->GetCmdBufferSize(cmdBufCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Total size for: 1. this object
    //                 2. pBltImages, pBltMemory, pBltCmdBuffers for all images and devices
    //                 3. the intermediate images for the presentation device
    //                 4. the peer images for all of the other devices
    size_t imageArraysOffset       = sizeof(SwCompositor);
    size_t presentableDeviceOffset = imageArraysOffset +
                                     ((sizeof(Pal::IImage*) + sizeof(Pal::IGpuMemory*) + sizeof(Pal::ICmdBuffer*)) *
                                      properties.imageCount * pDevice->NumPalDevices());
    size_t otherDevicesOffset      = presentableDeviceOffset + ((palImageSize + palMemorySize) * properties.imageCount);
    size_t totalSize               = otherDevicesOffset +
                                     ((palPeerImageSize + palPeerMemorySize + palCmdBufferSize) *
                                         properties.imageCount * (pDevice->NumPalDevices() - 1));

    void* pMemory = pDevice->VkInstance()->AllocMem(totalSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        // Setup the image count array pointers for all devices
        Pal::IImage**     ppBltImages[MaxPalDevices];
        Pal::IGpuMemory** ppBltMemory[MaxPalDevices];
        Pal::ICmdBuffer** ppBltCmdBuffers[MaxPalDevices];

        void* pNextImageArrays = Util::VoidPtrInc(pMemory, imageArraysOffset);

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
        {
            ppBltImages[deviceIdx]     = static_cast<Pal::IImage**>(pNextImageArrays);
            ppBltMemory[deviceIdx]     = static_cast<Pal::IGpuMemory**>(Util::VoidPtrInc(ppBltImages[deviceIdx],
                                            (sizeof(Pal::IImage*) * properties.imageCount)));
            ppBltCmdBuffers[deviceIdx] = static_cast<Pal::ICmdBuffer**>(Util::VoidPtrInc(ppBltMemory[deviceIdx],
                                            (sizeof(Pal::IGpuMemory*) * properties.imageCount)));
            pNextImageArrays           = Util::VoidPtrInc(ppBltCmdBuffers[deviceIdx],
                                            (sizeof(Pal::ICmdBuffer*) * properties.imageCount));
        }

        // Construct the object after setting up the member array bases
        pObject = VK_PLACEMENT_NEW(pMemory) SwCompositor(
            pDevice,
            presentationDeviceIdx,
            properties.imageCount,
            cmdBufCreateInfo.queueType,
            ppBltImages,
            ppBltMemory,
            ppBltCmdBuffers);

        // Setup for the intermediate destination images for the presentation device
        void* pImageMemory  = Util::VoidPtrInc(pMemory, presentableDeviceOffset);
        void* pMemoryMemory = Util::VoidPtrInc(pImageMemory, (palImageSize * properties.imageCount));

        for (uint32_t i = 0; i < properties.imageCount; ++i)
        {
            palResult = pPalDevice->CreatePresentableImage(
                properties.imageCreateInfo,
                pImageMemory,
                pMemoryMemory,
                &ppBltImages[presentationDeviceIdx][i],
                &ppBltMemory[presentationDeviceIdx][i]);

            pImageMemory  = Util::VoidPtrInc(pImageMemory, palImageSize);
            pMemoryMemory = Util::VoidPtrInc(pMemoryMemory, palMemorySize);

            // Clean up and break if any error is encountered
            if (palResult != Pal::Result::Success)
            {
                result = PalToVkError(palResult);

                pObject->Destroy(pDevice, pAllocator);
                pObject = nullptr;

                break;
            }
        }

        // Next, setup the peer copies to the intermediate destinations
        void* pPeerImageMemory  = Util::VoidPtrInc(pMemory, otherDevicesOffset);
        void* pPeerMemoryMemory = Util::VoidPtrInc(pPeerImageMemory, (palPeerImageSize * properties.imageCount));
        void* pCmdBufferMemory  = Util::VoidPtrInc(pPeerMemoryMemory, (palPeerMemorySize * properties.imageCount));

        // Composition BLT common setup.  Only the destination image varies.
        Pal::ImageLayout     srcLayout = { Pal::LayoutCopySrc, cmdBufCreateInfo.engineType };
        Pal::ImageLayout     dstLayout = { Pal::LayoutCopyDst, cmdBufCreateInfo.engineType };
        Pal::ImageCopyRegion region    = {};

        region.extent    = peerInfo.pOriginalImage->GetImageCreateInfo().extent;
        region.numSlices = 1;

        region.srcSubres.aspect     = Pal::ImageAspect::Color;
        region.srcSubres.arraySlice = 0;
        region.srcSubres.mipLevel   = 0;

        region.dstSubres.aspect     = Pal::ImageAspect::Color;
        region.dstSubres.arraySlice = 0;
        region.dstSubres.mipLevel   = 0;

        Pal::CmdBufferBuildInfo buildInfo = {};

        for (uint32_t deviceIdx = 0; (deviceIdx < pDevice->NumPalDevices()) && (result == VK_SUCCESS); ++deviceIdx)
        {
            // The presentation device image setup was performed above
            if (deviceIdx == presentationDeviceIdx)
            {
                continue;
            }

            pPalDevice = pDevice->PalDevice(deviceIdx);

            cmdBufCreateInfo.pCmdAllocator = pDevice->GetSharedCmdAllocator(deviceIdx);

            VK_ASSERT(palCmdBufferSize == pPalDevice->GetCmdBufferSize(cmdBufCreateInfo, nullptr));

            // Create/open all of the peer images/memory together and generate the BLT commands on first use
            for (uint32_t i = 0; i < properties.imageCount; ++i)
            {
                peerInfo.pOriginalImage = ppBltImages[presentationDeviceIdx][i];

                size_t assertPalImageSize;
                size_t assertPalMemorySize;
                pPalDevice->GetPeerImageSizes(peerInfo, &assertPalImageSize, &assertPalMemorySize, nullptr);
                VK_ASSERT((assertPalImageSize == palPeerImageSize) && (assertPalMemorySize == assertPalMemorySize));

                palResult = pPalDevice->OpenPeerImage(peerInfo,
                    pPeerImageMemory,
                    pPeerMemoryMemory,
                    &ppBltImages[deviceIdx][i],
                    &ppBltMemory[deviceIdx][i]);

                pPeerImageMemory  = Util::VoidPtrInc(pPeerImageMemory, palPeerImageSize);
                pPeerMemoryMemory = Util::VoidPtrInc(pPeerMemoryMemory, palPeerMemorySize);

                if (palResult == Pal::Result::Success)
                {
                    palResult = pPalDevice->CreateCmdBuffer(
                        cmdBufCreateInfo,
                        pCmdBufferMemory,
                        &ppBltCmdBuffers[deviceIdx][i]);

                    pCmdBufferMemory = Util::VoidPtrInc(pCmdBufferMemory, palCmdBufferSize);

                    // Generate the BLT to the appropriate peer destination image
                    if (palResult == Pal::Result::Success)
                    {
                        ppBltCmdBuffers[deviceIdx][i]->Begin(buildInfo);

                        ppBltCmdBuffers[deviceIdx][i]->CmdCopyImage(
                            *Image::ObjectFromHandle(properties.images[i])->PalImage(deviceIdx),
                            srcLayout,
                            *ppBltImages[deviceIdx][i],
                            dstLayout,
                            1,
                            &region,
                            0);

                        ppBltCmdBuffers[deviceIdx][i]->End();
                    }
                }

                // Clean up and break if any error is encountered
                if (palResult != Pal::Result::Success)
                {
                    result = PalToVkError(palResult);

                    pObject->Destroy(pDevice, pAllocator);
                    pObject = nullptr;

                    break;
                }
            }
        }
    }

    return pObject;
}

// =====================================================================================================================
// Destroy the software compositor object
void SwCompositor::Destroy(
    const Device*                pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            if (m_ppBltMemory[deviceIdx][i] != nullptr)
            {
                m_ppBltMemory[deviceIdx][i]->Destroy();
                m_ppBltMemory[deviceIdx][i] = nullptr;
            }

            if (m_ppBltImages[deviceIdx][i] != nullptr)
            {
                m_ppBltImages[deviceIdx][i]->Destroy();
                m_ppBltImages[deviceIdx][i] = nullptr;
            }

            if (m_ppBltCmdBuffers[deviceIdx][i] != nullptr)
            {
                m_ppBltCmdBuffers[deviceIdx][i]->Destroy();
                m_ppBltCmdBuffers[deviceIdx][i] = nullptr;
            }
        }
    }

    this->~SwCompositor();

    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Perform the software compositing BLT if this is the non presentable device and returns the queue for the present.
Pal::IQueue* SwCompositor::DoSwCompositing(
    Device*                    pDevice,
    uint32_t                   deviceIdx,
    uint32_t                   imageIndex,
    Pal::PresentSwapChainInfo* pPresentInfo,
    const Queue*               pPresentQueue)
{
    Pal::IQueue* pPalQueue = pPresentQueue->PalQueue(deviceIdx);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 441
    // SW compositing uses separate queues for present, so notify the original PAL queue first to prevent developer mode
    // tracking information and the overlay from being dropped.
    pPresentInfo->flags.notifyOnly = 1;
    pPalQueue->PresentSwapChain(*pPresentInfo);
    pPresentInfo->flags.notifyOnly = 0;
#endif

    pPalQueue = pDevice->PerformSwCompositing(deviceIdx,
                                              m_presentationDeviceIdx,
                                              m_ppBltCmdBuffers[deviceIdx][imageIndex],
                                              m_queueType,
                                              pPresentQueue);

    if (pPalQueue != nullptr)
    {
        if (deviceIdx != m_presentationDeviceIdx)
        {
            // Update the present info to use the intermediate image as the source on a presentable device.
            pPresentInfo->pSrcImage = m_ppBltImages[m_presentationDeviceIdx][imageIndex];
        }
    }
    else
    {
        // Give up if any errors were encountered, and reset to the original presentation queue.
        pPalQueue = pPresentQueue->PalQueue(deviceIdx);
        VK_ASSERT(false);
    }

    return pPalQueue;
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

    const VkAcquireNextImageInfoKHR acquireInfo =
    {
        VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
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
        const VkAcquireNextImageInfoKHR*  pAcquireInfoKHR;
    };

    pAcquireInfoKHR = &acquireInfo;

    return SwapChain::ObjectFromHandle(swapchain)->AcquireNextImage(pHeader, pImageIndex);
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
