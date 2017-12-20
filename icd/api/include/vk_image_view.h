/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

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

class ImageView : public NonDispatchable<VkImageView, ImageView>
{
public:
    typedef VkImageView ApiType;

    static VkResult Create(
        Device*                      pDevice,
        const VkImageViewCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        uint32_t                     viewFlags,
        VkImageView*                 pImageView);

    VkResult Destroy(
        Device*                      pDevice,
        const VkAllocationCallbacks* pAllocator);

    VK_INLINE const void* Descriptor(VkImageLayout layout, uint32_t deviceIdx, size_t srdSize) const;

    VK_INLINE const Pal::IColorTargetView* PalColorTargetView(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pColorTargetViews[idx];
    }

    VK_INLINE const Pal::IDepthStencilView* PalDepthStencilView(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pDepthStencilViews[idx];
    }

    VK_INLINE const Image* GetImage() const
        { return m_pImage; }

    VK_INLINE const VkFormat GetViewFormat() const
        { return m_viewFormat; }

    VK_INLINE const Pal::Range GetZRange() const
        { return m_zRange; }

    VK_INLINE void GetFrameBufferAttachmentSubresRange(Pal::ImageAspect aspect, Pal::SubresRange* pRange) const;

    VK_INLINE bool NeedsFmaskViewSrds() const
        { return m_needsFmaskViewSrds; }

protected:
    static VK_INLINE void BuildImageSrds(
        const Device*                pDevice,
        size_t                       srdSize,
        const Image*                 pImage,
        const Pal::SwizzledFormat    viewFormat,
        const Pal::SubresRange&      subresRange,
        VkImageUsageFlags            imageViewUsage,
        float                        minLod,
        const VkImageViewCreateInfo* pCreateInfo,
        void*                        pSrdMemory);

    static VK_INLINE void BuildFmaskViewSrds(
        const Device*                pDevice,
        size_t                       fmaskDescSize,
        const Image*                 pImage,
        const Pal::SubresRange&      subresRange,
        const VkImageViewCreateInfo* pCreateInfo,
        void*                        pFmaskMemory);

    static VK_INLINE Pal::Result BuildColorTargetView(
        const Pal::IDevice*       pPalDevice,
        const Pal::IImage*        pPalImage,
        VkImageViewType           viewType,
        const Pal::SwizzledFormat viewFormat,
        const Pal::SubresRange&   subresRange,
        const Pal::Range&         zRange,
        void*                     pPalViewMemory,
        Pal::IColorTargetView**   pColorView);

    static VK_INLINE Pal::Result BuildDepthStencilView(
        const Pal::IDevice*       pPalDevice,
        const Pal::IImage*        pPalImage,
        VkImageViewType           viewType,
        const Pal::SwizzledFormat viewFormat,
        const Pal::SubresRange&   subresRange,
        const Pal::Range&         zRange,
        uint32_t                  flags,
        void*                     pPalViewMemory,
        Pal::IDepthStencilView**  pDepthStencilView);

    // Types of supported SRD contained within this view (chosen based on layout)
    enum SrdIndexType
    {
        SrdReadOnly = 0, // SRD compatible for read-only shader ops
        SrdWritable = 1, // SRD compatible with writable shader ops
        SrdCount
    };

    VK_INLINE ImageView(
        Pal::IColorTargetView**  pColorTargetView,
        Pal::IDepthStencilView** pDepthStencilView,
        const Image*             pImage,
        VkFormat                 viewFormat,
        const Pal::SubresRange&  subresRange,
        const Pal::Range&        zRange,
        const bool               needsFmaskViewSrds);

    const Image*        m_pImage;
    const VkFormat      m_viewFormat;
    Pal::SubresRange    m_subresRange;
    Pal::Range          m_zRange;      // Needed for views of 3D textures.  Overloading Pal::SubresRange arraySlice
                                       // and numSlices like the Vulkan API does disrupts PAL subresource indexing

    const bool          m_needsFmaskViewSrds;

    Pal::IColorTargetView*  m_pColorTargetViews[MaxPalDevices];
    Pal::IDepthStencilView* m_pDepthStencilViews[MaxPalDevices];
};

// =====================================================================================================================
// Returns an SRD pointer that is compatible with the given VkImageLayout.  This is expected to be the layout of
// the image at the time the shader accesses this SRD data.
const void* ImageView::Descriptor(
    VkImageLayout layout,
    uint32_t      deviceIdx,
    size_t        srdSize) const
{
    VK_ASSERT((m_pImage->GetSupportedLayouts() & (Pal::LayoutShaderRead | Pal::LayoutShaderWrite)) != 0);

    static_assert(
        SrdReadOnly == 0 &&
        SrdWritable == 1 &&
        SrdCount    == 2,
        "SRD data order mismatch");

    size_t srdOffset = (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ? 0 : srdSize;

    srdOffset += deviceIdx * (SrdCount * srdSize);

    return Util::VoidPtrInc(this, sizeof(*this) + srdOffset);
}

// =====================================================================================================================
// Returns the subresource range of the given aspect from this view for a frame buffer attachment
void ImageView::GetFrameBufferAttachmentSubresRange(
    Pal::ImageAspect  aspect,
    Pal::SubresRange* pRange
    ) const
{
    *pRange = m_subresRange;

    pRange->numMips = (pRange->numMips > 1) ? 1 : pRange->numMips;

    pRange->startSubres.aspect = aspect;
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
