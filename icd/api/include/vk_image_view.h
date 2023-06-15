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

#ifndef __VK_IMAGE_VIEW_H__
#define __VK_IMAGE_VIEW_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "include/vk_image.h"

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;

class ImageView final : public NonDispatchable<VkImageView, ImageView>
{
public:
    typedef VkImageView ApiType;

    // Types of supported SRD contained within this view (chosen based on descriptor type)
    enum SrdIndexType
    {
        SrdReadOnly  = 0, // SRD compatible for read-only shader ops
        SrdReadWrite = 1, // SRD compatible with storage read-write shader ops
        SrdCount
    };

    static VkResult Create(
        Device*                      pDevice,
        const VkImageViewCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkImageView*                 pImageView);

    VkResult Destroy(
        Device*                      pDevice,
        const VkAllocationCallbacks* pAllocator);

    inline const void* Descriptor(
        uint32_t      deviceIdx,
        bool          isShaderStorageDesc,
        size_t        srdSize) const;

    const Pal::IColorTargetView* PalColorTargetView(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pColorTargetViews[idx];
    }

    const Pal::IDepthStencilView* PalDepthStencilView(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pDepthStencilViews[idx];
    }

    const Image* GetImage() const
        { return m_pImage; }

    VkFormat GetViewFormat() const
        { return m_viewFormat; }

    const Pal::SubresRange GetSubresRange() const
        { return m_subresRange; }

    const Pal::Range GetZRange() const
        { return m_zRange; }

    inline void GetFrameBufferAttachmentSubresRange(Pal::SubresRange* pRange) const;

    bool NeedsFmaskViewSrds() const
        { return m_needsFmaskViewSrds; }

protected:
    static void BuildImageSrds(
        const Device*                pDevice,
        size_t                       srdSize,
        const Image*                 pImage,
        const Pal::SwizzledFormat    viewFormat,
        const Pal::SubresRange&      subresRange,
        const Pal::Range&            zRange,
        VkImageUsageFlags            imageViewUsage,
        float                        minLod,
        const VkImageViewCreateInfo* pCreateInfo,
        void*                        pSrdMemory);

    static void BuildFmaskViewSrds(
        const Device*                pDevice,
        size_t                       fmaskDescSize,
        const Image*                 pImage,
        const Pal::SubresRange&      subresRange,
        const VkImageViewCreateInfo* pCreateInfo,
        void*                        pFmaskMemory);

    static Pal::Result BuildColorTargetView(
        const Pal::IDevice*       pPalDevice,
        const Pal::IImage*        pPalImage,
        VkImageViewType           viewType,
        VkImageUsageFlags         imageViewUsage,
        const Pal::SwizzledFormat viewFormat,
        const Pal::SubresRange&   subresRange,
        const Pal::Range&         zRange,
        void*                     pPalViewMemory,
        Pal::IColorTargetView**   pColorView,
        const RuntimeSettings&    settings);

    static Pal::Result BuildDepthStencilView(
        const Device*             pDevice,
        const Pal::IDevice*       pPalDevice,
        const Pal::IImage*        pPalImage,
        VkImageViewType           viewType,
        VkImageUsageFlags         imageViewUsage,
        const Pal::SwizzledFormat viewFormat,
        const Pal::SubresRange&   subresRange,
        const Pal::Range&         zRange,
        void*                     pPalViewMemory,
        Pal::IDepthStencilView**  pDepthStencilView,
        const RuntimeSettings&    settings);

    ImageView(
        Pal::IColorTargetView**  pColorTargetView,
        Pal::IDepthStencilView** pDepthStencilView,
        const Image*             pImage,
        VkFormat                 viewFormat,
        const Pal::SubresRange&  subresRange,
        const Pal::Range&        zRange,
        const bool               needsFmaskViewSrds,
        uint32_t                 numDevices);

    static bool IsMallNoAllocSnsrPolicySet(
        VkImageUsageFlags      imageViewUsage,
        const RuntimeSettings& settings);

    const Image*        m_pImage;
    const VkFormat      m_viewFormat;
    Pal::SubresRange    m_subresRange;
    Pal::Range          m_zRange;      // Needed for views of 3D textures.  Overloading Pal::SubresRange arraySlice
                                       // and numSlices like the Vulkan API does disrupts PAL subresource indexing

    const bool          m_needsFmaskViewSrds;

    Pal::IColorTargetView*  m_pColorTargetViews[MaxPalDevices];
    Pal::IDepthStencilView* m_pDepthStencilViews[MaxPalDevices];

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ImageView);
};

// =====================================================================================================================
// Returns an SRD pointer that is compatible with the descriptor type. The layout is expected to be the layout of
// the image at the time the shader accesses this SRD data.
const void* ImageView::Descriptor(
    uint32_t      deviceIdx,
    bool          isShaderStorageDesc,
    size_t        srdSize) const
{
    VK_ASSERT((m_pImage->GetBarrierPolicy().GetSupportedLayoutUsageMask() &
               (Pal::LayoutShaderRead | Pal::LayoutShaderFmaskBasedRead | Pal::LayoutShaderWrite)) != 0);

    static_assert(
        SrdReadOnly  == 0 &&
        SrdReadWrite == 1 &&
        SrdCount     == 2,
        "SRD data order mismatch");

    // Set srdOffset to 0 by default for SRDs. This includes those with descType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    // as well as layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL i.e. SrdReadOnly
    size_t srdOffset = (!isShaderStorageDesc) ? 0 : srdSize;

    srdOffset += deviceIdx * (SrdCount * srdSize);

    return Util::VoidPtrInc(this, sizeof(*this) + srdOffset);
}

// =====================================================================================================================
// Returns the subresource range of the given aspect from this view for a frame buffer attachment
void ImageView::GetFrameBufferAttachmentSubresRange(
    Pal::SubresRange* pRange
    ) const
{
    *pRange = m_subresRange;

    pRange->numMips = (pRange->numMips > 1) ? 1 : pRange->numMips;
}

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(
    VkDevice                                    device,
    VkImageView                                 imageView,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk
#endif
 /* __VK_IMAGE_VIEW_H__ */
