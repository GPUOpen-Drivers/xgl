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

#include "include/vk_image_view.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"

#include "palColorTargetView.h"
#include "palDepthStencilView.h"
#include "palFormatInfo.h"

namespace vk
{

// =====================================================================================================================
ImageView::ImageView(
    Pal::IColorTargetView**  pColorTargetView,
    Pal::IDepthStencilView** pDepthStencilView,
    const Image*             pImage,
    VkFormat                 viewFormat,
    const Pal::SubresRange&  subresRange,
    const Pal::Range&        zRange,
    const bool               needsFmaskViewSrds)
    :
    m_pImage(pImage),
    m_viewFormat(viewFormat),
    m_subresRange(subresRange),
    m_zRange(zRange),
    m_needsFmaskViewSrds(needsFmaskViewSrds)
{
    memset(m_pColorTargetViews, 0, sizeof(m_pColorTargetViews));
    memset(m_pDepthStencilViews, 0, sizeof(m_pDepthStencilViews));

    const uint32_t numDevices = pImage->VkDevice()->NumPalDevices();
    if (pColorTargetView != nullptr)
    {
        memcpy(m_pColorTargetViews, pColorTargetView, sizeof(pColorTargetView[0]) * numDevices);
    }

    if (pDepthStencilView != nullptr)
    {
        memcpy(m_pDepthStencilViews, pDepthStencilView, sizeof(pDepthStencilView[0]) * numDevices);
    }
}

// =====================================================================================================================
void ImageView::BuildImageSrds(
    const Device*                pDevice,
    size_t                       srdSize,
    const Image*                 pImage,
    const Pal::SwizzledFormat    viewFormat,
    const Pal::SubresRange&      subresRange,
    VkImageUsageFlags            imageViewUsage,
    float                        minLod,
    const VkImageViewCreateInfo* pCreateInfo,
    void*                        pSrdMemory)
{
    Pal::ImageViewInfo info = {};

    info.viewType         = VkToPalImageViewType(pCreateInfo->viewType);
    info.swizzledFormat   = RemapFormatComponents(viewFormat, pCreateInfo->components);
    info.samplePatternIdx = Device::GetDefaultSamplePatternIndex(pImage->GetImageSamples());
    info.texOptLevel      = VkToPalTexFilterQuality(pDevice->GetRuntimeSettings().vulkanTexFilterQuality);

    // NOTE: Unlike for color views, we don't have to mess with the subresource range for 3D views.
    // When zRangeValid is 0, PAL still enables all depth slices on that subresource visible to the view
    // despite the arrayLayers count.

    info.subresRange  = subresRange;
    info.flags.u32All = 0;
    info.minLod       = minLod;

    // Create all possible SRD variants
    static_assert(SrdCount == 2, "More SRD types were added; need to create them below");

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        void* pReadOnlySrd = pSrdMemory;

        info.pImage = pImage->PalImage(deviceIdx);

        VK_ASSERT(Pal::Result::Success == pDevice->PalDevice(deviceIdx)->ValidateImageViewInfo(info));

        pDevice->PalDevice(deviceIdx)->CreateImageViewSrds(1, &info, pReadOnlySrd);

        void* pReadWriteSrd = Util::VoidPtrInc(pSrdMemory, srdSize);

        if (imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT)
        {
            info.flags.shaderWritable = 1;

            VK_ASSERT(Pal::Result::Success == pDevice->PalDevice(deviceIdx)->ValidateImageViewInfo(info));

            pDevice->PalDevice(deviceIdx)->CreateImageViewSrds(1, &info, pReadWriteSrd);
        }
        else
        {
            memcpy(pReadWriteSrd, pReadOnlySrd, srdSize);
        }

        pSrdMemory = Util::VoidPtrInc(pSrdMemory, srdSize * SrdCount);
    }
}

// =====================================================================================================================
void ImageView::BuildFmaskViewSrds(
    const Device*                pDevice,
    size_t                       fmaskDescSize,
    const Image*                 pImage,
    const Pal::SubresRange&      subresRange,
    const VkImageViewCreateInfo* pCreateInfo,
    void*                        pFmaskMemory)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        Pal::FmaskViewInfo fmaskViewInfo = {};

        fmaskViewInfo.pImage         = pImage->PalImage(deviceIdx);
        fmaskViewInfo.baseArraySlice = subresRange.startSubres.arraySlice;
        fmaskViewInfo.arraySize      = subresRange.numSlices;

        // Zero-initialize FMASK descriptor memory
        memset(pFmaskMemory, 0, fmaskDescSize);

        VK_ASSERT((pImage->PalImage(deviceIdx)->GetImageCreateInfo().usageFlags.shaderRead != 0) &&
            (pImage->PalImage(deviceIdx)->GetImageCreateInfo().usageFlags.depthStencil == 0));

        // Create FMASK shader resource descriptor
        pDevice->PalDevice(deviceIdx)->CreateFmaskViewSrds(1, &fmaskViewInfo, pFmaskMemory);

        pFmaskMemory = Util::VoidPtrInc(pFmaskMemory, fmaskDescSize);
    }
}

// =====================================================================================================================
Pal::Result ImageView::BuildColorTargetView(
    const Pal::IDevice*       pPalDevice,
    const Pal::IImage*        pPalImage,
    VkImageViewType           viewType,
    const Pal::SwizzledFormat viewFormat,
    const Pal::SubresRange&   subresRange,
    const Pal::Range&         zRange,
    void*                     pPalViewMemory,
    Pal::IColorTargetView**   pColorView)
{
    struct Pal::SubresId subresId;

    subresId.aspect     = Pal::ImageAspect::Color;
    subresId.mipLevel   = subresRange.startSubres.mipLevel;
    subresId.arraySlice = subresRange.startSubres.arraySlice;

    Pal::ColorTargetViewCreateInfo colorInfo = {};

    colorInfo.flags.imageVaLocked  = 1;
    colorInfo.imageInfo.pImage     = pPalImage;
    colorInfo.swizzledFormat       = viewFormat;
    colorInfo.imageInfo.baseSubRes = subresId;
    colorInfo.imageInfo.arraySize  = subresRange.numSlices;

    if (pPalImage->GetImageCreateInfo().imageType == Pal::ImageType::Tex3d)
    {
        colorInfo.flags.zRangeValid = 1;
        colorInfo.zRange            = zRange;
    }

    Pal::Result result = pPalDevice->CreateColorTargetView(colorInfo, pPalViewMemory, pColorView);

    return result;
}

// =====================================================================================================================
Pal::Result ImageView::BuildDepthStencilView(
    const Pal::IDevice*       pPalDevice,
    const Pal::IImage*        pPalImage,
    VkImageViewType           viewType,
    const Pal::SwizzledFormat viewFormat,
    const Pal::SubresRange&   subresRange,
    const Pal::Range&         zRange,
    uint32_t                  viewFlags,
    void*                     pPalViewMemory,
    Pal::IDepthStencilView**  pDepthStencilView)
{
    Pal::DepthStencilViewCreateInfo depthInfo = {};

    depthInfo.flags.imageVaLocked = 1;
    depthInfo.pImage              = pPalImage;
    depthInfo.mipLevel            = subresRange.startSubres.mipLevel;
    depthInfo.baseArraySlice      = subresRange.startSubres.arraySlice;
    depthInfo.arraySize           = subresRange.numSlices;

    if (pPalImage->GetImageCreateInfo().imageType  == Pal::ImageType::Tex3d)
    {
        depthInfo.baseArraySlice = zRange.offset;
        depthInfo.arraySize      = zRange.extent;
    }

    Pal::Result result = pPalDevice->CreateDepthStencilView(
        depthInfo,
        pPalViewMemory,
        pDepthStencilView);

    return result;
}

// =====================================================================================================================
// Create a new Vulkan Image View object
VkResult ImageView::Create(
    Device*                      pDevice,
    const VkImageViewCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    uint32_t                     viewFlags,
    VkImageView*                 pImageView)
{
    // Determine the amount of memory needed by all of the different kinds of views based on the image's declared
    // usage flags.
    const Image* const pImage = Image::ObjectFromHandle(pCreateInfo->image);

    const size_t numDevices    = pDevice->NumPalDevices();
    const size_t srdSize       = pDevice->VkPhysicalDevice()->PalProperties().gfxipProperties.srdSizes.imageView;
    const size_t fmaskDescSize = pDevice->VkPhysicalDevice()->PalProperties().gfxipProperties.srdSizes.fmaskView;
    const size_t apiSize       = sizeof(ImageView);

    size_t totalSize              = apiSize; // Starting point
    size_t srdSegmentOffset       = 0;
    size_t srdSegmentSize         = 0;
    size_t fmaskSegmentOffset     = 0;
    size_t fmaskSegmentSize       = 0;
    size_t colorViewSegmentOffset = 0;
    size_t colorViewSegmentSize   = 0;
    size_t depthViewSegmentOffset = 0;
    size_t depthViewSegmentSize   = 0;

    // Creation arguments that may be overridden by extensions below
    VkImageUsageFlags imageViewUsage = pImage->GetImageUsage();
    float             minLod         = 0.0f;

    union
    {
        const VkStructHeader*                pHeader;
        const VkImageViewUsageCreateInfo*    pUsageInfo;
    };

    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);

    for (pHeader = static_cast<const VkStructHeader*>(pCreateInfo->pNext); pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
            // The image view usage must be a subset of the usage of the image it is created from.  For uncompressed
            // views of compressed images or format compatible image views, VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR
            // allows the image to be created with usage flags that are not supported for the format the image is created
            // with but are supported for the format of the VkImageView.
            VK_ASSERT((imageViewUsage | pUsageInfo->usage) == imageViewUsage);

            imageViewUsage = pUsageInfo->usage;
            break;
        default:
            // Skip any unknown extension structures
            break;
        }
    }

    const Pal::IImage* pPalImage          = pImage->PalImage();
    const Pal::ImageCreateInfo& imageInfo = pPalImage->GetImageCreateInfo();
    bool needsFmaskViewSrds               = false;

    // NOTE: The SRDs must be the first "segment" of data after the API because the GetDescriptor() functions
    // assumes this.
    if (imageViewUsage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
    {
        // Image views having both DEPTH_BIT and STENCIL_BIT specified in the aspectMask cannot be used as a sampled
        // image view, only as attachment, so check the condition before trying to generate any SRDs for the view.
        //
        // Also note that, for 2D array compatible 3D images, SRDs should only be created for 3D image views. Trying
        // to use atomic/load/store ops against 2D and 2D array image views created from such images is illegal from the API
        // PoV, and triggers an assertion failure in PAL.
        const VkImageAspectFlags combinedDsView = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        if ((pCreateInfo->subresourceRange.aspectMask & combinedDsView) != combinedDsView        &&
            ( !pImage->Is2dArrayCompatible() ||
              (pImage->Is2dArrayCompatible() && pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_3D)) )
        {
            srdSegmentOffset = totalSize;
            srdSegmentSize   = srdSize * SrdCount;
            totalSize        = srdSegmentOffset + (srdSegmentSize * numDevices);
        }

        // Check if FMASK based MSAA read is enabled. If enabled, add fmask descriptor size to totalSize.
        const Pal::ImageMemoryLayout& memoryLayout = pPalImage->GetMemoryLayout();
        if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead &&
            (pImage->GetImageSamples() > VK_SAMPLE_COUNT_1_BIT)    &&
            (imageInfo.usageFlags.shaderRead   != 0)               &&
            (imageInfo.usageFlags.depthStencil == 0)               &&
            ((memoryLayout.metadataSize + memoryLayout.metadataHeaderSize) > 0)) // Has metadata
        {
            needsFmaskViewSrds = true;

            fmaskSegmentOffset = totalSize;
            fmaskSegmentSize   = fmaskDescSize;
            totalSize          = fmaskSegmentOffset + (fmaskSegmentSize * numDevices);
        }
    }

    if (imageViewUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        colorViewSegmentOffset = totalSize;
        colorViewSegmentSize   = pDevice->GetProperties().palSizes.colorTargetView;
        totalSize              = colorViewSegmentOffset + (colorViewSegmentSize * numDevices);
    }

    if (imageViewUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        depthViewSegmentOffset = totalSize;
        depthViewSegmentSize   = pDevice->GetProperties().palSizes.depthStencilView;
        totalSize              = depthViewSegmentOffset + (depthViewSegmentSize * numDevices);
    }

    void* pMemory = pDevice->AllocApiObject(totalSize, pAllocator);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // When the image type is a 3D texture, a single level-layer 3D texture subresource describes all depth slices of
    // that texture.  This is implied by Table 8 of the spec, where the description for shader reads from a 3D texture
    // of arbitrary depth through VK_IMAGE_VIEW_TYPE_3D requires that the PAL subresource range be set to
    // arraySlice = 0, numSlices = 1.  However, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT permits rendering to 3D
    // slices as 2D by specifying a baseArrayLayer >= 0 and layerCount >= 1 in VkImageSubresourceRange, which doesn't
    // directly map to the PAL subresource range anymore.  Separate this information from the subresource range and
    // have the view keep track of a 3D texture zRange for attachment operations like clears.
    VkImageSubresourceRange subresRange = pCreateInfo->subresourceRange;
    Pal::Range              zRange;

    if (imageInfo.imageType == Pal::ImageType::Tex3d)
    {
        const uint32_t subresDepth = Util::Max(1U, imageInfo.extent.depth >> subresRange.baseMipLevel);

        if (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_3D)
        {
            zRange.offset = 0;
            zRange.extent = subresDepth;
        }
        else
        {
            VK_ASSERT(pImage->Is2dArrayCompatible());
            VK_ASSERT(subresRange.layerCount <= subresDepth);
            VK_ASSERT((pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_2D) ||
                      (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY));

            zRange.offset = subresRange.baseArrayLayer;
            zRange.extent = subresRange.layerCount;
        }

        subresRange.baseArrayLayer = 0;
        subresRange.layerCount     = 1;
    }
    else
    {
        zRange.offset = 0;
        zRange.extent = 1;
    }

    // We may need multiple entries here for images with multiple planes but we're only actually going to
    // use the first one.
    Pal::SubresRange palRanges[MaxPalAspectsPerMask];
    uint32_t palRangeCount = 0;

    VkToPalSubresRange(VkToPalFormat(pImage->GetFormat()).format,
                       subresRange,
                       pImage->GetMipLevels(),
                       pImage->GetArraySize(),
                       palRanges,
                       &palRangeCount);

    Pal::Result result = Pal::Result::Success;

    // Get the view format (without component mapping)
    Pal::SwizzledFormat viewFormat = VkToPalFormat(pCreateInfo->format);

    VK_ASSERT(viewFormat.format != Pal::ChNumFormat::Undefined);

    // Build the PAL image view SRDs if needed
    if (srdSegmentSize > 0)
    {
        void* pSrdMemory = Util::VoidPtrInc(pMemory, srdSegmentOffset);

        Pal::SwizzledFormat aspectFormat = VkToPalFormat(Formats::GetAspectFormat(pCreateInfo->format,
            subresRange.aspectMask));

        VK_ASSERT(aspectFormat.format != Pal::ChNumFormat::Undefined);

        BuildImageSrds(pDevice,
                       srdSize,
                       pImage,
                       aspectFormat,
                       palRanges[0],
                       imageViewUsage,
                       minLod,
                       pCreateInfo,
                       pSrdMemory);
    }

    //Build Fmask View SRDS if needed
    if (fmaskSegmentSize > 0)
    {
        void *pFmaskMem = Util::VoidPtrInc(pMemory, fmaskSegmentOffset);

        BuildFmaskViewSrds(pDevice, fmaskSegmentSize, pImage, palRanges[0], pCreateInfo, pFmaskMem);
    }

    Pal::IColorTargetView* pColorView[MaxPalDevices] = {};

    // Build the color target view if needed
    if ((colorViewSegmentSize > 0) && (result == Pal::Result::Success))
    {
        VK_ASSERT(pImage->GetBarrierPolicy().GetSupportedLayoutUsageMask() & Pal::LayoutColorTarget);

        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            void* pPalMem = Util::VoidPtrInc(pMemory,
                                colorViewSegmentOffset + (colorViewSegmentSize * deviceIdx));

            result = BuildColorTargetView(pDevice->PalDevice(deviceIdx),
                                          pImage->PalImage(deviceIdx),
                                          pCreateInfo->viewType,
                                          viewFormat,
                                          palRanges[0],
                                          zRange,
                                          pPalMem,
                                          &pColorView[deviceIdx]);
         }
    }

    Pal::IDepthStencilView* pDsView[MaxPalDevices] = {};

    // Build the depth/stencil view if needed
    if ((depthViewSegmentSize > 0) && (result == Pal::Result::Success))
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            void* pPalMem = Util::VoidPtrInc(pMemory,
                            depthViewSegmentOffset + (depthViewSegmentSize * deviceIdx));

            result = BuildDepthStencilView(pDevice->PalDevice(deviceIdx),
                                           pImage->PalImage(deviceIdx),
                                           pCreateInfo->viewType,
                                           viewFormat,
                                           palRanges[0],
                                           zRange,
                                           viewFlags,
                                           pPalMem,
                                           &pDsView[deviceIdx]);
        }
    }

    if (result == Pal::Result::Success)
    {
        VK_PLACEMENT_NEW(pMemory) ImageView(
            (colorViewSegmentSize > 0) ? pColorView : nullptr,
            (depthViewSegmentSize > 0) ? pDsView    : nullptr,
            pImage,
            pCreateInfo->format,
            palRanges[0],
            zRange,
            needsFmaskViewSrds);

        *pImageView = ImageView::HandleFromVoidPointer(pMemory);

        return VK_SUCCESS;
    }
    else
    {
        // NOTE: None of PAL SRDs, color target views, and DS views require any clean-up other than their
        // memory freed.
        pAllocator->pfnFree(pAllocator->pUserData, pMemory);

        return PalToVkResult(result);
    }
}

// ===============================================================================================
// Destroy an image view object
VkResult ImageView::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(
    VkDevice                                    device,
    VkImageView                                 imageView,
    const VkAllocationCallbacks*                pAllocator)
{
    if (imageView != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        ImageView::ObjectFromHandle(imageView)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
