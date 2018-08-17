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
 * @file  vk_image.h
 * @brief Image object related functionality for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_IMAGE_H__
#define __VK_IMAGE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_conv.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_memory.h"
#include "include/vk_physical_device.h"
#include "include/vk_cmdbuffer.h"

#include "include/barrier_policy.h"

#include "palCmdBuffer.h"
#include "palQueue.h"

namespace Pal
{

class  IImage;
struct ImageLayout;
struct ImageCreateInfo;

}

namespace vk
{

class CmdBuffer;
class Device;
class DispatchableImage;
class PeerImages;
struct RPImageLayout;
class SwapChain;

class Image : public NonDispatchable<VkImage, Image>
{
public:
    typedef VkImage ApiType;

    static VkResult Create(
        Device*                                 pDevice,
        const VkImageCreateInfo*                pCreateInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkImage*                                pImage);

    static VkResult CreateImageInternal(
        Device*                         pDevice,
        Pal::ImageCreateInfo*           pPalCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        Pal::IImage**                   pPalImage);

    static VkResult CreatePresentableImage(
        Device*                                 pDevice,
        const Pal::PresentableImageCreateInfo*  pCreateInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkImageUsageFlags                       imageUsageFlags,
        Pal::PresentMode                        presentMode,
        VkImage*                                pImage,
        VkFormat                                pImageFormat,
        VkSharingMode                           sharingMode,
        uint32_t                                queueFamilyIndexCount,
        const uint32_t*                         pQueueFamilyIndices,
        VkDeviceMemory*                         pDeviceMemory);

    VkResult Destroy(
        const Device*                   pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult GetMemoryRequirements(
        VkMemoryRequirements* pMemoryRequirements);

    VkResult BindMemory(
        VkDeviceMemory     mem,
        VkDeviceSize       memOffset,
        uint32_t           deviceIndexCount,
        const uint32_t*    pDeviceIndices,
        uint32_t           rectCount,
        const VkRect2D*    pRects);

    VkResult BindSwapchainMemory(
        uint32_t           swapChainImageIndex,
        SwapChain*         pSwapchain,
        uint32_t           deviceIndexCount,
        const uint32_t*    pDeviceIndices,
        uint32_t           rectCount,
        const VkRect2D*    pRects);

    VkResult GetSubresourceLayout(
        const VkImageSubresource*               pSubresource,
        VkSubresourceLayout*                    pLayout) const;

    void GetSparseMemoryRequirements(
        uint32_t*                                           pNumRequirements,
        utils::ArrayView<VkSparseImageMemoryRequirements>   sparseMemoryRequirements);

    VK_FORCEINLINE Pal::IImage* PalImage(int32_t idx = DefaultDeviceIndex) const
       { return m_pPalImages[idx]; }

    VK_FORCEINLINE uint8_t GetMemoryInstanceIdx(uint32_t idx) const
       { return m_multiInstanceIndices[idx]; }

    VK_FORCEINLINE VkFormat GetFormat() const
        { return m_format; }

    VkImageUsageFlags GetImageUsage() const
        { return m_imageUsage; }

    uint32_t GetMipLevels() const
        { return m_mipLevels; }

    uint32_t GetArraySize() const
        { return m_arraySize; }

    Pal::IGpuMemory* GetSampleLocationsMetaDataMemory(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pSampleLocationsMetaDataMemory[idx];
    }

    Pal::gpusize GetSampleLocationsMetaDataOffset(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_sampleLocationsMetaDataOffset[idx];
    }

    void SetSampleLocationsMetaDataMemory(Pal::IGpuMemory* pSampleLocationsMetaDataMemory, int32_t idx = DefaultDeviceIndex)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        m_pSampleLocationsMetaDataMemory[idx] = pSampleLocationsMetaDataMemory;
    }

    void SetSampleLocationsMetaDataOffset(Pal::gpusize sampleLocationsMetaDataOffset, int32_t idx = DefaultDeviceIndex)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        m_sampleLocationsMetaDataOffset[idx] = sampleLocationsMetaDataOffset;
    }

    bool IsPresentable() const
        { return m_pSwapChain != nullptr; }

    VK_FORCEINLINE SwapChain* GetSwapChain() const
    {
        VK_ASSERT(m_pSwapChain != nullptr);
        return m_pSwapChain;
    }

    // We have to treat the image sparse if any of these flags are set
    static const VkImageCreateFlags SparseEnablingFlags =
        VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
        VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

    bool IsSparse() const
        { return (m_flags & SparseEnablingFlags) != 0; }

    bool Is2dArrayCompatible() const
        { return (m_flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) != 0; }

    bool IsSampleLocationsCompatibleDepth() const
    {
        return (m_flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT) != 0;
    }

    Device* VkDevice() const { return m_pDevice; }

    VK_FORCEINLINE Pal::IGpuMemory* PalMemory(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalMemory[idx];
    }

    const VkExtent3D& GetTileSize() const
        { return m_tileSize; }

    uint32_t GetImageSamples() const { return static_cast<uint32_t>(m_imageSamples); }

    Pal::ImageLayout GetAttachmentLayout(
        const RPImageLayout& layout,
        Pal::ImageAspect     aspect,
        const CmdBuffer*     pCmdBuffer
        ) const;

    bool DedicatedMemoryRequired() const { return m_internalFlags.dedicatedRequired; }

    VK_FORCEINLINE const ImageBarrierPolicy& GetBarrierPolicy() const
        { return m_barrierPolicy; }

    // Returns true if the image has a color format.
    VK_FORCEINLINE bool IsColorFormat() const
        { return m_internalFlags.isColorFormat == 1; }

    // Returns true if the image has a depth, stencil, or depth-stencil format.
    VK_FORCEINLINE bool IsDepthStencilFormat() const
        { return (m_internalFlags.hasDepth | m_internalFlags.hasStencil) == 1; }

    // Returns true if the image has depth components.
    VK_FORCEINLINE bool HasDepth() const
        { return m_internalFlags.hasDepth == 1; }

    // Returns true if the image has stencil components.
    VK_FORCEINLINE bool HasStencil() const
        { return m_internalFlags.hasStencil == 1; }

    // Returns true if the image has depth and stencil components.
    VK_FORCEINLINE bool HasDepthAndStencil() const
        { return (m_internalFlags.hasDepth & m_internalFlags.hasStencil) == 1; }

private:
    // SwapChain object needs to be able to instantiate API image objects for presentable images
    friend class SwapChain;

    union ImageFlags
    {
        struct
        {
            uint32_t internalMemBound       : 1;  // If this image has an internal memory bound. The bound memory
                                                  // should be destroyed when this image is destroyed.
            uint32_t boundToSwapchainMemory : 1;  // Pal::IImage objects are not destroyed by Image::Destroy,
                                                  // this case is handled by Swapchain::Destroy instead.
            uint32_t dedicatedRequired      : 1;  // Indicates if a dedicated memory is required.
            uint32_t externallyShareable    : 1;  // True if the backing memory of this image may be shared externally.
            uint32_t boundToExternalMemory  : 1;  // If true, indicates the image is bound to an external memory, and
                                                  //   the m_pPalMemory is a pointer to an external Pal image (does
                                                  //   not include backing pinned system memory case).
            uint32_t androidPresentable     : 1;  // True if this image is created as Android presentable image.
            uint32_t externalPinnedHost     : 1;  // True if image backing memory is compatible with pinned sysmem.
            uint32_t externalD3DHandle      : 1;  // True if image is backed by a D3D11 image
            uint32_t isColorFormat          : 1;  // True if the image has a color format
            uint32_t hasDepth               : 1;  // True if the image has depth components
            uint32_t hasStencil             : 1;  // True if the image has stencil components
            uint32_t reserved               : 21;
        };
        uint32_t     u32All;
    };

    Image(
        Device*                     pDevice,
        VkImageCreateFlags          flags,
        Pal::IImage**               pPalImage,
        Pal::IGpuMemory**           pPalMemory,
        const ImageBarrierPolicy&   barrierPolicy,
        VkExtent3D                  tileSize,
        uint32_t                    mipLevels,
        uint32_t                    arraySize,
        VkFormat                    imageFormat,
        VkSampleCountFlagBits       imageSamples,
        VkImageTiling               imageTiling,
        VkImageUsageFlags           usage,
        ImageFlags                  internalFlags);

    void CalcMemoryPriority();

    // This function is used to register presentable images with swap chains
    VK_FORCEINLINE void RegisterPresentableImageWithSwapChain(SwapChain* pSwapChain)
    {
        // Registration is only allowed to happen once
        VK_ASSERT(m_pSwapChain == nullptr);
        m_pSwapChain = pSwapChain;
    }

    Device* const           m_pDevice;
    VkImageCreateFlags      m_flags;

    Memory*                 m_pMemory;

    Pal::IImage*            m_pPalImages[MaxPalDevices];    // Each device in the group can own an instance of the image

    Pal::IGpuMemory*        m_pPalMemory[MaxPalDevices];    // Virtual-only memory object used for sparse images

    uint8_t                 m_multiInstanceIndices[MaxPalDevices]; // The memory instance each image is bound to

    VkExtent3D              m_tileSize;                     // Cached sparse image block dimensions (tile size)
                                                            // for sparse images

    uint32_t                m_mipLevels;                    // This is the amount of mip levels contained in the image.
                                                            // We need this to support VK_WHOLE_SIZE during
                                                            // memory barrier creation

    uint32_t                m_arraySize;                    // This is the amount of array slices contained
                                                            // in the image
                                                            // we need this to support VK_WHOLE_SIZE during
                                                            // memory barrier creation

    VkFormat                m_format;                       // The image format is needed for handling copy
                                                            // operations for compressed formats appropriately

    VkSampleCountFlagBits   m_imageSamples;                 // Number of samples in the image

    VkImageTiling           m_imageTiling;                  // Image tiling mode

    VkImageUsageFlags       m_imageUsage;                   // Bitmask describing the intended image usage

    ImageBarrierPolicy      m_barrierPolicy;                // Barrier policy to use for this image

    Pal::IGpuMemory* m_pSampleLocationsMetaDataMemory[MaxPalDevices]; // Bound image memory

    Pal::gpusize     m_sampleLocationsMetaDataOffset[MaxPalDevices];  // Offset into image memory for depth/stencil
                                                                      // sample locations meta data

    SwapChain*               m_pSwapChain;                 // If this image is a presentable image this tells
                                                           // which swap chain the image belongs to

    ImageFlags               m_internalFlags;              // Flags describing the properties of this image

    VkDeviceSize        m_baseAddrOffset;       // Offset from the beginning of the bound memory range (i.e. after
                                                // the app offset) to the start of image data.  This is generally zero,
                                                // but sometimes may reflect padding required to align the image's
                                                // base address to harsher alignment requirements.

    // Minimum priority assigned to any VkMemory object that this image is bound to.
    MemoryPriority m_priority;
};

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset);

VKAPI_ATTR void VKAPI_CALL vkDestroyImage(
    VkDevice                                    device,
    VkImage                                     image,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements*                       pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    uint32_t*                                   pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements*            pSparseMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout);

VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2(
    VkDevice                                    device,
    const VkImageMemoryRequirementsInfo2*       pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements2(
    VkDevice                                    device,
    const VkImageSparseMemoryRequirementsInfo2* pInfo,
    uint32_t*                                   pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2*           pSparseMemoryRequirements);

} // namespace entry

} // namespace vk

#endif /*__VK_IMAGE_H__*/
