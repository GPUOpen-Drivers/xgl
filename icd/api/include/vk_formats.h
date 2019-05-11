/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_formats.h
 * @brief Format-related utility functions.
 ***********************************************************************************************************************
 */

#ifndef __VK_FORMATS_H__
#define __VK_FORMATS_H__

#pragma once

#include "include/vk_utils.h"
#include "include/khronos/vulkan.h"

namespace vk
{

// =====================================================================================================================
// Container for storing compile-time meta-information about Vulkan formats.
//
// NOTE: This class should not store any information that is unknown at compile-time (e.g. which formats are renderable
// on the current HW) -- put such information in PhysicalDevice or Device.
struct Formats
{
    VK_INLINE static bool IsColorFormat(VkFormat format);
    VK_INLINE static bool IsDepthStencilFormat(VkFormat format);
    VK_INLINE static bool IsBcCompressedFormat(VkFormat format);
    VK_INLINE static bool IsYuvFormat(VkFormat format);
    VK_INLINE static bool IsYuvPlanar(VkFormat format);
    VK_INLINE static bool IsYuvPacked(VkFormat format);
    VK_INLINE static bool HasDepth(VkFormat format);
    VK_INLINE static bool HasStencil(VkFormat format);
    VK_INLINE static VkFormat GetAspectFormat(VkFormat format, VkImageAspectFlags aspectMask);

    VK_INLINE static uint32_t GetIndex(VkFormat format);
    VK_INLINE static VkFormat FromIndex(uint32_t index);

    static VkExtent3D ElementsToTexels(VkFormat format, const VkExtent3D& extent);
    static Pal::Formats::NumericSupportFlags GetNumberFormat(VkFormat format);
};

#define VK_YUV_FORMAT_START VK_FORMAT_G8B8G8R8_422_UNORM
#define VK_YUV_FORMAT_END VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM

// Number of formats supported by the driver.
#define VK_SUPPORTED_FORMAT_COUNT     (VK_FORMAT_RANGE_SIZE + (VK_YUV_FORMAT_END - VK_YUV_FORMAT_START + 1))

// =====================================================================================================================
// Get a linear index for a format (used to address tables indirectly indexed by formats).
uint32_t Formats::GetIndex(VkFormat format)
{
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT))
    {
        // Core format
        return static_cast<uint32_t>(format);
    }
    else if ((format >= VK_YUV_FORMAT_START) && (format <= VK_YUV_FORMAT_END))
    {
        return VK_FORMAT_RANGE_SIZE + (format - VK_YUV_FORMAT_START);
    }
    else
    {
        VK_ALERT(!"Unexpected format");
        return 0;
    }
}

// =====================================================================================================================
// Get format corresponding to lienar index (inverse of Formats::GetFormatIndex).
VkFormat Formats::FromIndex(uint32_t index)
{
    if (index < VK_FORMAT_RANGE_SIZE)
    {
        // Core format
        return static_cast<VkFormat>(index);
    }
    else if ((index >= VK_FORMAT_RANGE_SIZE) &&
             (index <= (VK_FORMAT_RANGE_SIZE + VK_YUV_FORMAT_END - VK_YUV_FORMAT_START)))
    {
        return static_cast<VkFormat>(VK_YUV_FORMAT_START + index - VK_FORMAT_RANGE_SIZE);
    }
    else
    {
        VK_ASSERT(!"Unexpected format index");
        return VK_FORMAT_MAX_ENUM;
    }
}

// =====================================================================================================================
// Returns true if the given format has a depth component.
bool Formats::HasDepth(VkFormat format)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are depth-stencil");

    bool hasDepth;

    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        hasDepth = true;
        break;
    default:
        hasDepth = false;
    }

    return hasDepth;
}

// =====================================================================================================================
// Returns true if the given format has a stencil component.
bool Formats::HasStencil(VkFormat format)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are depth-stencil");

    bool hasStencil;

    switch (format)
    {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        hasStencil = true;
        break;
    default:
        hasStencil = false;
    }

    return hasStencil;
}

// =====================================================================================================================
// Returns true if the given format is a core Vulkan color format.
bool Formats::IsColorFormat(VkFormat format)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are color");

    return ((format >= VK_FORMAT_R4G4_UNORM_PACK8)    && (format <= VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)) ||
           ((format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK) && (format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK));
}

// =====================================================================================================================
// Returns true if the given format is a depth or stencil format.
bool Formats::IsDepthStencilFormat(VkFormat format)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are depth-stencil");
    return (format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT_S8_UINT);
}

// =====================================================================================================================
// Returns true if the given format is a BC block-compressed format.
bool Formats::IsBcCompressedFormat(VkFormat format)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are BC block-compressed");
    return (format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && format <= VK_FORMAT_BC7_SRGB_BLOCK);
}

// =====================================================================================================================
// Returns true if the given format is a yuv format.
bool Formats::IsYuvFormat(VkFormat format)
{
    return (format >= VK_YUV_FORMAT_START && format <= VK_YUV_FORMAT_END);
}

// =====================================================================================================================
// Returns true if the given format is a yuv format.
bool Formats::IsYuvPlanar(VkFormat format)
{
    return (VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM                  == format) ||
           (VK_FORMAT_G8_B8R8_2PLANE_420_UNORM                   == format) ||
           (VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM                  == format) ||
           (VK_FORMAT_G8_B8R8_2PLANE_422_UNORM                   == format) ||
           (VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM                  == format) ||
           (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16  == format) ||
           (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16  == format) ||
           (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16  == format) ||
           (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16  == format) ||
           (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 == format) ||
           (VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM               == format) ||
           (VK_FORMAT_G16_B16R16_2PLANE_420_UNORM                == format) ||
           (VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM               == format) ||
           (VK_FORMAT_G16_B16R16_2PLANE_422_UNORM                == format) ||
           (VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM               == format);
}

// =====================================================================================================================
// Returns true if the given format is a yuv format.
bool Formats::IsYuvPacked(VkFormat format)
{
    return (VK_FORMAT_G8B8G8R8_422_UNORM                     == format) ||
           (VK_FORMAT_B8G8R8G8_422_UNORM                     == format) ||
           (VK_FORMAT_R10X6_UNORM_PACK16                     == format) ||
           (VK_FORMAT_R10X6G10X6_UNORM_2PACK16               == format) ||
           (VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16     == format) ||
           (VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 == format) ||
           (VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 == format) ||
           (VK_FORMAT_R12X4_UNORM_PACK16                     == format) ||
           (VK_FORMAT_R12X4G12X4_UNORM_2PACK16               == format) ||
           (VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16     == format) ||
           (VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 == format) ||
           (VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 == format) ||
           (VK_FORMAT_G16B16G16R16_422_UNORM                 == format) ||
           (VK_FORMAT_B16G16R16G16_422_UNORM                 == format);
}

// =====================================================================================================================
// Given a format and an aspect mask, returns the sub-format for multi-aspect formats.  For example for D16_S8, the
// depth sub-format is R16 and the stencil sub-format is R8.
//
// For single aspect images, the original format is returned.
VkFormat Formats::GetAspectFormat(VkFormat format, VkImageAspectFlags aspectMask)
{
    static_assert(VK_FORMAT_RANGE_SIZE == 185,
        "Number of formats changed.  Double check whether any of them are depth-stencil");

    VkFormat subFormat;

    if (aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        // Convert only if no stencil is specified
        switch (format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D16_UNORM_S8_UINT:
            subFormat = VK_FORMAT_D16_UNORM;
            break;
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            subFormat = VK_FORMAT_D32_SFLOAT;
            break;
        default:
            subFormat = format;
            break;
        }
    }
    else if (aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        // Convert only if no depth is specified
        switch (format)
        {
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            subFormat = VK_FORMAT_S8_UINT;
            break;
        default:
            subFormat = format;
            break;
        }
    }
    else
    {
        subFormat = format;
    }

    return subFormat;
}

} // namespace vk

#endif /* __VK_FORMATS_H__ */
