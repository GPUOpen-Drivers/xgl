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
/**
***********************************************************************************************************************
* @file  vk_format.cpp
* @brief Contains implementation of several vk::Formats functions
***********************************************************************************************************************
*/
#include "include/vk_formats.h"
#include "include/vk_conv.h"
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

}
