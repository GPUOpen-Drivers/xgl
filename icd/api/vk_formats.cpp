/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vk_format.cpp
* @brief Contains implementation of several vk::Formats functions
***********************************************************************************************************************
*/
#include "include/vk_formats.h"
#include "include/vk_conv.h"
#include "include/vk_physical_device.h"
namespace vk
{
#if ( VKI_GPU_DECOMPRESS)
// =====================================================================================================================
// This function should only be called for astc formats
void Formats::GetAstcMappedInfo(
    VkFormat        format,
    AstcMappedInfo* pMapInfo)
{
    switch (format)
    {
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 4;
            pMapInfo->hScale = 4;
            break;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 4;
            pMapInfo->hScale = 4;
            break;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 5;
            pMapInfo->hScale = 4;
            break;
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 5;
            pMapInfo->hScale = 4;
            break;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 5;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 5;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 6;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 6;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 6;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 6;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 8;
            break;
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 8;
            pMapInfo->hScale = 8;
            break;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 5;
            break;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 6;
            break;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 8;
            break;
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 8;
            break;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 10;
            break;
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 10;
            pMapInfo->hScale = 10;
            break;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 12;
            pMapInfo->hScale = 10;
            break;
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 12;
            pMapInfo->hScale = 10;
            break;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
            pMapInfo->wScale = 12;
            pMapInfo->hScale = 12;
            break;
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            pMapInfo->format = VK_FORMAT_R8G8B8A8_SRGB;
            pMapInfo->wScale = 12;
            pMapInfo->hScale = 12;
            break;
        default:
            VK_ASSERT(IsASTCFormat(format));
            VK_NEVER_CALLED();
            break;
    }
}
#endif

// =====================================================================================================================
// Helper function to calculate image texels based on whether an image is compressed or not. Element is compatible to
// pal definition. For non-compressed format elements equal to texels. For compressed format elements are considered
// as compressed blocks.
VkExtent3D Formats::ElementsToTexels(
    VkFormat               format,
    const VkExtent3D&      extent,
    const RuntimeSettings& settings)
{
    VkExtent3D texels = {};
    const Pal::ChNumFormat palFormat = VkToPalFormat(format, settings).format;

    if (Pal::Formats::IsBlockCompressed(palFormat))
    {
        texels = PalToVkExtent3d(Pal::Formats::CompressedBlocksToTexels(palFormat,
                                                                        extent.width,
                                                                        extent.height,
                                                                        extent.depth));
    }
    else
    {
        texels = extent;
    }

    return texels;
}

// =====================================================================================================================
// Returns the number type of a particular Vulkan format.  This is necessary to be called instead of the PAL utility
// functions because we map certain Vulkan formats to "undefined" that we still technically expose through very limited
// ways, and we need to know this particular piece of information about those formats.
Pal::Formats::NumericSupportFlags Formats::GetNumberFormat(
    VkFormat               format,
    const RuntimeSettings& settings)
{
    using namespace Pal::Formats;

    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of the new ones we currently map to Undefined in "
        "VkToPalFormat, and return a number type for them below in the switch-case (this is rare).");

    NumericSupportFlags numType;
    const Pal::SwizzledFormat palFormat = VkToPalFormat(format, settings);

    if (palFormat.format != Pal::UndefinedSwizzledFormat.format)
    {
        const auto info = Pal::Formats::FormatInfoTable[static_cast<size_t>(palFormat.format)];

        numType = info.numericSupport;
    }
    else if (IsYuvFormat(format))
    {
        numType = NumericSupportFlags::Unorm;
    }
    else
    {
        switch (format)
        {
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_B8G8R8_UNORM:
            numType = NumericSupportFlags::Unorm;
            break;
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            numType = NumericSupportFlags::Snorm;
            break;
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_B8G8R8_USCALED:
            numType = NumericSupportFlags::Uscaled;
            break;
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            numType = NumericSupportFlags::Sscaled;
            break;
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_B8G8R8_UINT:
            numType = NumericSupportFlags::Uint;
            break;
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            numType = NumericSupportFlags::Sint;
            break;
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
            numType = NumericSupportFlags::Srgb;
            break;
        case VK_FORMAT_R16G16B16_SFLOAT:
        case VK_FORMAT_R64_SFLOAT:
        case VK_FORMAT_R64G64_SFLOAT:
        case VK_FORMAT_R64G64B64_SFLOAT:
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            numType = NumericSupportFlags::Float;
            break;
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            numType = NumericSupportFlags::Unorm;
            break;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            numType = NumericSupportFlags::DepthStencil;
            break;
        case VK_FORMAT_UNDEFINED:
            numType = NumericSupportFlags::Undefined;
            break;
        default:
            VK_NEVER_CALLED();
            numType = NumericSupportFlags::Undefined;
            break;
        }
    }

    return numType;
}

// =====================================================================================================================
// Individual planes of multi-planar formats are size-compatible with single-plane color formats if they occupy
// the same number of bits per texel block, and are compatible with those formats if they have the same block extent.
// See 34.1.1 Compatible Formats of Planes of Multi-Planar Formats
VkFormat Formats::GetCompatibleSinglePlaneFormat(VkFormat multiPlaneFormat, uint32_t planeIndex)
{
    VK_ASSERT(GetYuvPlaneCounts(multiPlaneFormat) > 1);
    VkFormat singlePlaneFormat = VK_FORMAT_UNDEFINED;

    if (planeIndex < GetYuvPlaneCounts(multiPlaneFormat))
    {
        // The conversion below is based on the table in 34.1.1.
        // Individual planes of a multi-planar format are in turn format compatible with the listed single plane
        // format's Format Compatability Classes (See 34.1.7).
        switch (multiPlaneFormat)
        {
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
            singlePlaneFormat = VK_FORMAT_R8_UNORM;
            break;
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
            singlePlaneFormat = VK_FORMAT_R10X6_UNORM_PACK16;
            break;
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
            singlePlaneFormat = VK_FORMAT_R12X4_UNORM_PACK16;
            break;
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
            singlePlaneFormat = VK_FORMAT_R16_UNORM;
            break;
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:
            singlePlaneFormat = (planeIndex == 0) ?
                                 VK_FORMAT_R8_UNORM :
                                 VK_FORMAT_R8G8_UNORM;
            break;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
            singlePlaneFormat = (planeIndex == 0) ?
                                 VK_FORMAT_R10X6_UNORM_PACK16 :
                                 VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
            break;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16:
            singlePlaneFormat = (planeIndex == 0) ?
                                 VK_FORMAT_R12X4_UNORM_PACK16 :
                                 VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
            break;
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM:
            singlePlaneFormat = (planeIndex == 0) ?
                                 VK_FORMAT_R16_UNORM :
                                 VK_FORMAT_R16G16_UNORM;
            break;
        default:
            break;
        }
    }

    return singlePlaneFormat;
}

// =====================================================================================================================
// Computes the extended feature set of a format when VK_IMAGE_CREATE_EXTENDED_USAGE_BIT is set
// NOTE: This function assumes the format that is passed in does not have
//       Pal::Formats::PropertyFlags::BitCountInaccurate set
VkFormatFeatureFlags Formats::GetExtendedFeatureFlags(
    const PhysicalDevice*  pPhysicalDevice,
    VkFormat               format,
    VkImageTiling          tiling,
    const RuntimeSettings& settings)
{
    VkFormatFeatureFlags extendedFeatures = 0;
    Pal::SwizzledFormat palFormat = VkToPalFormat(format, settings);

    uint32 bitsPerPixel = Pal::Formats::BitsPerPixel(palFormat.format);

    // The following tables are from the Format Compatibility Classes section of the Vulkan specification.
    static constexpr VkFormat Bpp8FormatClass[] =
    {
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_USCALED,
        VK_FORMAT_R8_SSCALED,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_SRGB
    };

    static constexpr VkFormat Bpp16FormatClass[] =
    {
        VK_FORMAT_R10X6_UNORM_PACK16,
        VK_FORMAT_R12X4_UNORM_PACK16,
        VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        VK_FORMAT_A4B4G4R4_UNORM_PACK16,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_USCALED,
        VK_FORMAT_R8G8_SSCALED,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_USCALED,
        VK_FORMAT_R16_SSCALED,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SFLOAT
    };

    static constexpr VkFormat Bpp24FormatClass[] =
    {
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R8G8B8_USCALED,
        VK_FORMAT_R8G8B8_SSCALED,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_R8G8B8_SINT,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_B8G8R8_SNORM,
        VK_FORMAT_B8G8R8_USCALED,
        VK_FORMAT_B8G8R8_SSCALED,
        VK_FORMAT_B8G8R8_UINT,
        VK_FORMAT_B8G8R8_SINT,
        VK_FORMAT_B8G8R8_SRGB
    };

    static constexpr VkFormat Bpp32FormatClass[] =
    {
        VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
        VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_USCALED,
        VK_FORMAT_R8G8B8A8_SSCALED,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SNORM,
        VK_FORMAT_B8G8R8A8_USCALED,
        VK_FORMAT_B8G8R8A8_SSCALED,
        VK_FORMAT_B8G8R8A8_UINT,
        VK_FORMAT_B8G8R8A8_SINT,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_USCALED_PACK32,
        VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_A2R10G10B10_SNORM_PACK32,
        VK_FORMAT_A2R10G10B10_USCALED_PACK32,
        VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
        VK_FORMAT_A2R10G10B10_UINT_PACK32,
        VK_FORMAT_A2R10G10B10_SINT_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        VK_FORMAT_A2B10G10R10_USCALED_PACK32,
        VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        VK_FORMAT_A2B10G10R10_SINT_PACK32,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_USCALED,
        VK_FORMAT_R16G16_SSCALED,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
    };

    static constexpr VkFormat Bpp48FormatClass[] =
    {
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R16G16B16_USCALED,
        VK_FORMAT_R16G16B16_SSCALED,
        VK_FORMAT_R16G16B16_UINT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16G16B16_SFLOAT
    };

    static constexpr VkFormat Bpp64FormatClass[] =
    {
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_USCALED,
        VK_FORMAT_R16G16B16A16_SSCALED,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R64_UINT,
        VK_FORMAT_R64_SINT,
        VK_FORMAT_R64_SFLOAT
    };

    static constexpr VkFormat Bpp96FormatClass[] =
    {
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R32G32B32_SINT,
        VK_FORMAT_R32G32B32_SFLOAT
    };

    static constexpr VkFormat Bpp128FormatClass[] =
    {
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R64G64_UINT,
        VK_FORMAT_R64G64_SINT,
        VK_FORMAT_R64G64_SFLOAT
    };

    static constexpr VkFormat Bpp192FormatClass[] =
    {
        VK_FORMAT_R64G64B64_UINT,
        VK_FORMAT_R64G64B64_SINT,
        VK_FORMAT_R64G64B64_SFLOAT
    };

    static constexpr VkFormat Bpp256FormatClass[] =
    {
        VK_FORMAT_R64G64B64A64_UINT,
        VK_FORMAT_R64G64B64A64_SINT,
        VK_FORMAT_R64G64B64A64_SFLOAT
    };

    // Depth images have no extended usage.
    // YUV single and multiplanar images by themselves have no extended usage. To compute extended usage
    // of a single plane of a multiplanar image call GetCompatibleSinglePlaneFormat and pass that format in.
    // BC images allow conversion between UNORM|SRGB but there shouldn't be any difference in features.
    bool noCompatibleExtendedUsage = Formats::IsDepthStencilFormat(format) ||
                                     Formats::IsYuvFormat(format) ||
                                     Pal::Formats::IsBlockCompressed(palFormat.format) ||
                                     (format == VK_FORMAT_UNDEFINED);

    if (noCompatibleExtendedUsage == false)
    {
        const VkFormat* pExtendedFormats = nullptr;
        uint32_t        extendedFormatCount = 0;

        switch (bitsPerPixel)
        {
        case 8:
            pExtendedFormats = Bpp8FormatClass;
            extendedFormatCount = sizeof(Bpp8FormatClass) / sizeof(VkFormat);
            break;
        case 16:
            pExtendedFormats = Bpp16FormatClass;
            extendedFormatCount = sizeof(Bpp16FormatClass) / sizeof(VkFormat);
            break;
        case 24:
            pExtendedFormats = Bpp24FormatClass;
            extendedFormatCount = sizeof(Bpp24FormatClass) / sizeof(VkFormat);
            break;
        case 32:
            pExtendedFormats = Bpp32FormatClass;
            extendedFormatCount = sizeof(Bpp32FormatClass) / sizeof(VkFormat);
            break;
        case 48:
            pExtendedFormats = Bpp48FormatClass;
            extendedFormatCount = sizeof(Bpp48FormatClass) / sizeof(VkFormat);
            break;
        case 64:
            pExtendedFormats = Bpp64FormatClass;
            extendedFormatCount = sizeof(Bpp64FormatClass) / sizeof(VkFormat);
            break;
        case 96:
            pExtendedFormats = Bpp96FormatClass;
            extendedFormatCount = sizeof(Bpp96FormatClass) / sizeof(VkFormat);
            break;
        case 128:
            pExtendedFormats = Bpp128FormatClass;
            extendedFormatCount = sizeof(Bpp128FormatClass) / sizeof(VkFormat);
            break;
        case 192:
            pExtendedFormats = Bpp192FormatClass;
            extendedFormatCount = sizeof(Bpp192FormatClass) / sizeof(VkFormat);
            break;
        case 256:
            pExtendedFormats = Bpp256FormatClass;
            extendedFormatCount = sizeof(Bpp256FormatClass) / sizeof(VkFormat);
            break;
        default:
            VK_ALERT_ALWAYS_MSG("Unknown Format Class");
        }

        if ((extendedFormatCount > 0 && pExtendedFormats != nullptr))
        {
            for (uint32_t i = 0; i < extendedFormatCount; ++i)
            {
                VkFormat extendedFormat = pExtendedFormats[i];

                VkFormatProperties extendedFormatProperties = {};

                VkResult result = pPhysicalDevice->GetFormatProperties(extendedFormat, &extendedFormatProperties);
                if (result != VK_ERROR_FORMAT_NOT_SUPPORTED)
                {
                    extendedFeatures |= (tiling == VK_IMAGE_TILING_OPTIMAL) ?
                                        extendedFormatProperties.optimalTilingFeatures :
                                        extendedFormatProperties.linearTilingFeatures;
                }

            }
        }
    }

    return extendedFeatures;
}

}
