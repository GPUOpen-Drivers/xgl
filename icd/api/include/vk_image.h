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
        uint32_t                                concurrentQueueFlags,
        VkDeviceMemory*                         pDeviceMemory);

    VkSharingMode GetSharingMode() const
    {
        return m_sharingMode;
    }

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
        uint32_t*                               pNumRequirements,
        VkSparseImageMemoryRequirements*        pSparseMemoryRequirements);

    VK_FORCEINLINE Pal::IImage* PalImage(int32_t idx = DefaultDeviceIndex) const
       { return m_pPalImages[idx]; }

    VK_FORCEINLINE uint8_t GetMemoryInstanceIdx(uint32_t idx) const
       { return m_multiInstanceIndices[idx]; }

    VK_FORCEINLINE VkFormat GetFormat() const
        { return m_format; }

    VkImageUsageFlags GetImageUsage() const
        { return m_imageUsage; }

    uint32_t GetSupportedLayouts() const
        { return m_layoutUsageMask; }

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

    VK_INLINE Pal::ImageLayout GetTransferLayout(
        VkImageLayout    layout,
        const CmdBuffer* pCmdBuffer,
        uint32_t         queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) const;

    Pal::ImageLayout GetAttachmentLayout(
        const RPImageLayout& layout,
        Pal::ImageAspect     aspect,
        const CmdBuffer*     pCmdBuffer
        ) const;

    Pal::ImageLayout GetLayoutFromUsage(
        uint32_t         palLayoutUsages,
        const CmdBuffer* pCmdBuffer,
        uint32_t         queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) const;

    bool DedicatedMemoryRequired() const { return m_internalFlags.dedicatedRequired; }

    VK_INLINE const ImageBarrierPolicy& GetBarrierPolicy() const
        { return m_barrierPolicy; }

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
            uint32_t reserved               : 24;
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
        VkSharingMode               sharingMode,
        uint32_t                    concurrentQueueFlags,
        ImageFlags                  internalFlags,
        Pal::PresentMode*           pPresentMode = nullptr);

    void CalcBarrierUsage(
        VkImageUsageFlags usage,
        Pal::PresentMode* pPresentMode);

    void CalcMemoryPriority();

    VK_INLINE uint32_t GetLayoutEngines(
        const CmdBuffer* pCmdBuffer,
        uint32_t         queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) const;

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

    uint32_t                m_layoutUsageMask;              // This is the maximum set of supported layouts
                                                            // calculated from enabled image usage.
    ImageBarrierPolicy      m_barrierPolicy;                // Barrier policy to use for this image

    Pal::IGpuMemory* m_pSampleLocationsMetaDataMemory[MaxPalDevices]; // Bound image memory

    Pal::gpusize     m_sampleLocationsMetaDataOffset[MaxPalDevices];  // Offset into image memory for depth/stencil
                                                                      // sample locations meta data

    VkSharingMode            m_sharingMode;                // Image sharing mode.

    uint32_t                 m_concurrentQueueFlags;       // Image layout queue flags in case of concurrent
                                                           // sharing mode.

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

// =====================================================================================================================
// This function converts a given VkImageLayout and queue family index to a compatible PAL ImageLayout.  For more
// information about the queue family and pCmdBuffer requirements, see the lower level GetLayoutFromUsage().
VK_INLINE Pal::ImageLayout Image::GetTransferLayout(
    VkImageLayout    layout,
    const CmdBuffer* pCmdBuffer,
    uint32_t         queueFamilyIndex
    ) const
{
    uint32_t palLayoutUsages[MaxRangePerAttachment];

    VkToPalImageLayoutUsages(layout, m_format, palLayoutUsages);

    // Under no condition should the layout argument specify separate layouts for depth and stencil subresources at this
    // point, since only the below layouts are permitted for transfer queues.  This provided layout is applicable to
    // whatever aspect(s) are specified using VkImageAspectFlags for the transfer.  That being said, if an aspect isn't
    // specified for the transfer, it's current layout may very well not match that of the aspect that is.
    VK_ASSERT((palLayoutUsages[1] == 0) ||
              (palLayoutUsages[1] == palLayoutUsages[0]));
    VK_ASSERT((layout == VK_IMAGE_LAYOUT_GENERAL) ||
              (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
              (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
              (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

    return GetLayoutFromUsage(palLayoutUsages[0], pCmdBuffer, queueFamilyIndex);
}

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
