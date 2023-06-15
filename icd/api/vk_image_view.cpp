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

#include "include/vk_image_view.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_sampler_ycbcr_conversion.h"

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
    const bool               needsFmaskViewSrds,
    uint32_t                 numDevices)
    :
    m_pImage(pImage),
    m_viewFormat(viewFormat),
    m_subresRange(subresRange),
    m_zRange(zRange),
    m_needsFmaskViewSrds(needsFmaskViewSrds)
{
    memset(m_pColorTargetViews, 0, sizeof(m_pColorTargetViews));
    memset(m_pDepthStencilViews, 0, sizeof(m_pDepthStencilViews));

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
bool ImageView::IsMallNoAllocSnsrPolicySet(
    VkImageUsageFlags imageViewUsage,
    const RuntimeSettings& settings)
{
    bool isSet = false;

    // Bypass the Mall cache for color target resource if it is used as shader non-storage resource and the corresponding
    // panel setting is set
    if ((imageViewUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
        (TestAnyFlagSet(settings.mallNoAllocCtPolicy, MallNoAllocCtAsSnsr)))
    {
        isSet = true;
    }
    // Bypass the Mall cache for shader storage resource if it is used as shader non-storage resource and the corresponding
    // panel setting is set
    else if ((imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT) &&
             (TestAnyFlagSet(settings.mallNoAllocSsrPolicy, MallNoAllocSsrAsSnsr)))
    {
        isSet = true;
    }
    // Bypass the Mall cache for depth stencil resource if it is used as shader non-storage resource and the corresponding
    // panel setting is set
    else if ((imageViewUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
             (TestAnyFlagSet(settings.mallNoAllocDsPolicy, MallNoAllocDsAsSnsr)))
    {
        isSet = true;
    }
    // Bypass the Mall cache for color target/shader storage resource if it is used as shader non-storage resource and the
    // corresponding panel setting is set
    else if (((imageViewUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
              (imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT)) &&
             (TestAnyFlagSet(settings.mallNoAllocCtSsrPolicy, MallNoAllocCtSsrAsSnsr)))
    {
        isSet = true;
    }

    return isSet;
}

// =====================================================================================================================
void ImageView::BuildImageSrds(
    const Device*                pDevice,
    size_t                       srdSize,
    const Image*                 pImage,
    const Pal::SwizzledFormat    viewFormat,
    const Pal::SubresRange&      subresRange,
    const Pal::Range&            zRange,
    VkImageUsageFlags            imageViewUsage,
    float                        minLod,
    const VkImageViewCreateInfo* pCreateInfo,
    void*                        pSrdMemory)
{
    Pal::ImageViewInfo info = {};
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    const Pal::ImageCreateInfo& imageInfo = pImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

    info.viewType         = VkToPalImageViewType(pCreateInfo->viewType);
    info.swizzledFormat   = RemapFormatComponents(viewFormat,
                                                  subresRange,
                                                  pCreateInfo->components,
                                                  pDevice->PalDevice(DefaultDeviceIndex),
                                                  imageInfo.tiling);
    info.samplePatternIdx = Device::GetDefaultSamplePatternIndex(pImage->GetImageSamples());
    info.texOptLevel      = VkToPalTexFilterQuality(settings.vulkanTexFilterQuality);

    info.flags.u32All = 0;

    // When zRangeValid is 0, PAL still enables all depth slices on that subresource visible to the view
    // despite the arrayLayers count. However, zRangeValid must be set for a 2D image view of a 3D image.
    if ((imageInfo.imageType == Pal::ImageType::Tex3d) && (info.viewType == Pal::ImageViewType::Tex2d))
    {
        info.flags.zRangeValid = 1;
        info.zRange = zRange;
    }

    info.subresRange = subresRange;
    info.minLod      = minLod;

    // Restrict possible usages to only those supported by the image, e.g. no FMask based reads without MSAA.
    const ImageBarrierPolicy& barrierPolicy = pImage->GetBarrierPolicy();

    info.possibleLayouts.usages  = (barrierPolicy.GetSupportedLayoutUsageMask() &
                                    (Pal::LayoutShaderRead | Pal::LayoutShaderFmaskBasedRead));

    info.possibleLayouts.engines = barrierPolicy.GetPossibleLayoutEngineMasks();

    pDevice->GetResourceOptimizer()->OverrideImageViewCreateInfo(pImage->GetResourceKey(), &info);

    // Bypass Mall cache read/write if no alloc policy is set for SRDs. This global setting applies to every image view SRD
    // and takes precedence over other shader non-storage resource/shader storage resource settings
    if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocImageViewSrds))
    {
        info.flags.bypassMallRead = 1;
        info.flags.bypassMallWrite = 1;
    }

    // Create all possible SRD variants
    static_assert(SrdCount == 2, "More SRD types were added; need to create them below");

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        void* pReadOnlySrd = pSrdMemory;

        info.pImage = pImage->PalImage(deviceIdx);

        // Bypass Mall cache read/write if no alloc policy is set for shader non-storage resources
        if (!Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocImageViewSrds) &&
            IsMallNoAllocSnsrPolicySet(imageViewUsage, settings))
        {
            info.flags.bypassMallRead = 1;
            info.flags.bypassMallWrite = 1;
        }
        // Create a read-only SRD
        VK_ASSERT(Pal::Result::Success == pDevice->PalDevice(deviceIdx)->ValidateImageViewInfo(info));

        pDevice->PalDevice(deviceIdx)->CreateImageViewSrds(1, &info, pReadOnlySrd);

        void* pReadWriteSrd = Util::VoidPtrInc(pSrdMemory, srdSize);

        if (imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT)
        {
            const void* pNext = pCreateInfo->pNext;

            while (pNext != nullptr)
            {
                const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

                if (pHeader->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_SLICED_CREATE_INFO_EXT)
                {
                    const auto* pSlicedInfo = reinterpret_cast<const VkImageViewSlicedCreateInfoEXT*>(pHeader);

                    // Only update zRange here because this extension is specific to shader storage 3D image views.
                    // The zRange.extent was previously initialized, so that can be used for the subresource depth.
                    // The minimum is taken below for VK_REMAINING_3D_SLICES_EXT handling.
                    VK_ASSERT((imageInfo.imageType == Pal::ImageType::Tex3d) &&
                              (info.viewType       == Pal::ImageViewType::Tex3d));
                    VK_ASSERT(zRange.extent > 0);

                    info.zRange.offset = pSlicedInfo->sliceOffset;
                    info.zRange.extent = Util::Min(pSlicedInfo->sliceCount, (zRange.extent - pSlicedInfo->sliceOffset));

                    info.flags.zRangeValid = 1;
                    break;
                }

                pNext = pHeader->pNext;
            }

            if (!Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocImageViewSrds))
            {
                // If Mall No alloc flags were set for shader non-storage resources, clear the flags first so that they
                // are not set incorrectly by default for the 3rd SRD to be created in this pass
                if (IsMallNoAllocSnsrPolicySet(imageViewUsage, settings))
                {
                    info.flags.bypassMallRead = 0;
                    info.flags.bypassMallWrite = 0;
                }

                // Bypass the Mall cache for shader storage resources if the corresponding panel settings are set
                if (Util::TestAnyFlagSet(settings.mallNoAllocSsrPolicy, MallNoAllocSsrAlways) ||
                    Util::TestAnyFlagSet(settings.mallNoAllocSsrPolicy, MallNoAllocSsrAsSsr))
                {
                    info.flags.bypassMallRead  = 1;
                    info.flags.bypassMallWrite = 1;
                }
                // Bypass the Mall cache for render target/shader storage resources if it is used as shader storage resource
                // and the corresponding panel setting is set
                else if ((imageViewUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
                         Util::TestAnyFlagSet(settings.mallNoAllocCtSsrPolicy, MallNoAllocCtSsrAlways))
                {
                    info.flags.bypassMallRead  = 1;
                    info.flags.bypassMallWrite = 1;
                }
            }
            // Create a read-write storage SRD
            info.possibleLayouts.usages = info.possibleLayouts.usages | Pal::ImageLayoutUsageFlags::LayoutShaderWrite;

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
    VkImageUsageFlags         imageViewUsage,
    const Pal::SwizzledFormat viewFormat,
    const Pal::SubresRange&   subresRange,
    const Pal::Range&         zRange,
    void*                     pPalViewMemory,
    Pal::IColorTargetView**   pColorView,
    const RuntimeSettings&    settings)
{
    struct Pal::SubresId subresId;

    subresId.plane      = subresRange.startSubres.plane;
    subresId.mipLevel   = subresRange.startSubres.mipLevel;
    subresId.arraySlice = subresRange.startSubres.arraySlice;

    Pal::ColorTargetViewCreateInfo colorInfo = {};

    colorInfo.flags.imageVaLocked  = 1;
    colorInfo.imageInfo.pImage     = pPalImage;
    colorInfo.swizzledFormat       = viewFormat;
    colorInfo.imageInfo.baseSubRes = subresId;
    colorInfo.imageInfo.arraySize  = subresRange.numSlices;

    // Bypass Mall cache if no alloc policy is set for color target resource
    // Global resource settings always takes precedence over everything else
    if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocCt))
    {
        colorInfo.flags.bypassMall = 1;
    }
    else
    {
        // Bypass the Mall cache if resource view is color target and the corresponding panel settings are set
        if (Util::TestAnyFlagSet(settings.mallNoAllocCtPolicy, MallNoAllocCtAsCt) ||
            Util::TestAnyFlagSet(settings.mallNoAllocCtPolicy, MallNoAllocCtAlways))
        {
            colorInfo.flags.bypassMall = 1;
        }
        // Bypass the Mall cache for color target/shader storage resource if it is used as CT and the corresponding panel
        // setting is set
        else if ((imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT) &&
                 TestAnyFlagSet(settings.mallNoAllocCtSsrPolicy, MallNoAllocCtSsrAlways))
        {
            colorInfo.flags.bypassMall = 1;
        }
    }

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
    const RuntimeSettings&    settings)
{
    Pal::DepthStencilViewCreateInfo depthInfo = {};

    depthInfo.flags.imageVaLocked           = 1;
    depthInfo.flags.lowZplanePolyOffsetBits = 1;
    depthInfo.pImage                        = pPalImage;
    depthInfo.mipLevel                      = subresRange.startSubres.mipLevel;
    depthInfo.baseArraySlice                = subresRange.startSubres.arraySlice;
    depthInfo.arraySize                     = subresRange.numSlices;

    // Bypass Mall cache if no alloc policy is set for depth stencil resource
    // Global resource settings always takes precedence over everything else
    if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocDs))
    {
        depthInfo.flags.bypassMall = 1;
    }
    else
    {
        // Bypass the Mall cache if resource view is depth stencil and the corresponding panel settings are set
        if (Util::TestAnyFlagSet(settings.mallNoAllocDsPolicy, MallNoAllocDsAsDs) ||
            Util::TestAnyFlagSet(settings.mallNoAllocDsPolicy, MallNoAllocDsAlways))
        {
            depthInfo.flags.bypassMall = 1;
        }
        // Bypass the Mall cache for color target/shader storage resource if it is used as DS and the corresponding panel
        // setting is set
        else if ((imageViewUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
                 (imageViewUsage & VK_IMAGE_USAGE_STORAGE_BIT) &&
                 TestAnyFlagSet(settings.mallNoAllocCtSsrPolicy, MallNoAllocCtSsrAsDs))
        {
            depthInfo.flags.bypassMall = 1;
        }
    }

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
    VkImageView*                 pImageView)
{
    // Determine the amount of memory needed by all of the different kinds of views based on the image's declared
    // usage flags.
    const Image* const pImage = Image::ObjectFromHandle(pCreateInfo->image);
    const auto & gfxipProperties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties;
    const uint32_t numDevices    = pDevice->NumPalDevices();

    const Pal::IImage* pPalImage          = pImage->PalImage(DefaultDeviceIndex);
    const Pal::ImageCreateInfo& imageInfo = pPalImage->GetImageCreateInfo();
    bool needsFmaskViewSrds               = false;

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

    VkToPalSubresRange(pImage->GetFormat(),
                       subresRange,
                       pImage->GetMipLevels(),
                       pImage->GetArraySize(),
                       palRanges,
                       &palRangeCount,
                       pDevice->GetRuntimeSettings());

    const size_t   imageDescSize = gfxipProperties.srdSizes.imageView;
    const size_t   srdSize       = imageDescSize * palRangeCount;
    const size_t   fmaskDescSize = gfxipProperties.srdSizes.fmaskView;
    const size_t   apiSize       = sizeof(ImageView);

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
    VkImageUsageFlags        imageViewUsage = pImage->GetImageUsage();
    float                    minLod         = 0.0f;

    if ((pCreateInfo->subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) &&
        (pCreateInfo->subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT))
    {
        imageViewUsage = (pImage->GetImageStencilUsage() & pImage->GetImageUsage());
    }
    else if (pCreateInfo->subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        imageViewUsage = pImage->GetImageStencilUsage();
    }

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    VkFormat createInfoFormat = pCreateInfo->format;

    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);

    for (const auto* pHeader = static_cast<const VkStructHeader*>(pCreateInfo->pNext);
         pHeader != nullptr;
         pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
        {
            // The image view usage must be a subset of the usage of the image it is created from.  For uncompressed
            // views of compressed images or format compatible image views, VK_IMAGE_CREATE_EXTENDED_USAGE_BIT
            // allows the image to be created with usage flags that are not supported for the format the image is created
            // with but are supported for the format of the VkImageView.

            const auto* pUsageInfo = reinterpret_cast<const VkImageViewUsageCreateInfo*>(pHeader);
            VK_ASSERT((imageViewUsage | pUsageInfo->usage) == imageViewUsage);

            imageViewUsage = pUsageInfo->usage;
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
        {
            const auto* pSamplerYcbcrConversionInfo = reinterpret_cast<const VkSamplerYcbcrConversionInfo*>(pHeader);
            SamplerYcbcrConversion::ObjectFromHandle(pSamplerYcbcrConversionInfo->conversion)->SetExtent(
                imageInfo.extent.width, imageInfo.extent.height, imageInfo.arraySize);
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT:
        {
            const auto* pMinLodStruct = reinterpret_cast<const VkImageViewMinLodCreateInfoEXT*>(pHeader);

            minLod = pMinLodStruct->minLod;
        }
        default:
            // Skip any unknown extension structures
            break;
        }
    }

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

    void* pMemory = pDevice->AllocApiObject(pAllocator, totalSize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    Pal::Result result = Pal::Result::Success;

    // Get the view format (without component mapping)
    Pal::SwizzledFormat viewFormat = VkToPalFormat(createInfoFormat, pDevice->GetRuntimeSettings());

    VK_ASSERT(viewFormat.format != Pal::ChNumFormat::Undefined);

    // Build the PAL image view SRDs if needed
    if (srdSegmentSize > 0)
    {
        Pal::SwizzledFormat aspectFormat = VkToPalFormat(Formats::GetAspectFormat(createInfoFormat,
                                                         subresRange.aspectMask), pDevice->GetRuntimeSettings());

        VK_ASSERT(aspectFormat.format != Pal::ChNumFormat::Undefined);

        for (uint32 plane = 0; plane < palRangeCount; ++plane)
        {
            void* pSrdMemory = Util::VoidPtrInc(pMemory, srdSegmentOffset + (imageDescSize  * SrdCount * plane));

            BuildImageSrds(pDevice,
                           imageDescSize,
                           pImage,
                           aspectFormat,
                           palRanges[plane],
                           zRange,
                           imageViewUsage,
                           minLod,
                           pCreateInfo,
                           pSrdMemory);
        }
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
                                          imageViewUsage,
                                          viewFormat,
                                          palRanges[0],
                                          zRange,
                                          pPalMem,
                                          &pColorView[deviceIdx],
                                          pDevice->GetRuntimeSettings());
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

            result = BuildDepthStencilView(pDevice,
                                           pDevice->PalDevice(deviceIdx),
                                           pImage->PalImage(deviceIdx),
                                           pCreateInfo->viewType,
                                           imageViewUsage,
                                           viewFormat,
                                           palRanges[0],
                                           zRange,
                                           pPalMem,
                                           &pDsView[deviceIdx],
                                           pDevice->GetRuntimeSettings());
        }
    }

    if (result == Pal::Result::Success)
    {
        VK_PLACEMENT_NEW(pMemory) ImageView(
            (colorViewSegmentSize > 0) ? pColorView : nullptr,
            (depthViewSegmentSize > 0) ? pDsView    : nullptr,
            pImage,
            createInfoFormat,
            palRanges[0],
            zRange,
            needsFmaskViewSrds,
            numDevices);

        *pImageView = ImageView::HandleFromVoidPointer(pMemory);

        return VK_SUCCESS;
    }
    else
    {
        // NOTE: None of PAL SRDs, color target views, and DS views require any clean-up other than their
        // memory freed.
        pDevice->FreeApiObject(pAllocator, pMemory);

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

    pDevice->FreeApiObject(pAllocator, this);

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
