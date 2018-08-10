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
    VK_INLINE static bool HasDepth(VkFormat format);
    VK_INLINE static bool HasStencil(VkFormat format);
    VK_INLINE static VkFormat GetAspectFormat(VkFormat format, VkImageAspectFlags aspectMask);

    VK_INLINE static uint32_t GetIndex(VkFormat format);
    VK_INLINE static VkFormat FromIndex(uint32_t index);

    static VkExtent3D ElementsToTexels(VkFormat format, const VkExtent3D& extent);
    static Pal::Formats::NumericSupportFlags GetNumberFormat(VkFormat format);
};

// Number of formats supported by the driver.
#define VK_SUPPORTED_FORMAT_COUNT   VK_FORMAT_RANGE_SIZE

// =====================================================================================================================
// Get a linear index for a format (used to address tables indirectly indexed by formats).
uint32_t Formats::GetIndex(VkFormat format)
{
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT))
    {
        // Core format
        return static_cast<uint32_t>(format);
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
