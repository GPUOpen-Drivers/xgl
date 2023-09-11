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
* @file  color_space_helper.h
* @brief Helper class to convert Pal to Vulkan API data formats
***********************************************************************************************************************
*/

#ifndef __COLOR_SPACE_HELPER_H__
#define __COLOR_SPACE_HELPER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "palScreen.h"

namespace vk
{

// =====================================================================================================================
class ColorSpaceHelper
{
public:

    enum FmtSupport : uint32_t
    {
        Fmt_Undefined    = 0x0000,

        Fmt_4bpc         = 0x0001,
        Fmt_5bpc         = 0x0002,
        Fmt_6bpc         = 0x0004,
        Fmt_8bpc_srgb    = 0x0008,
        Fmt_8bpc_unorm   = 0x0010,
        Fmt_9bpc         = 0x0020,
        Fmt_10bpc        = 0x0040,
        Fmt_11bpc        = 0x0080,
        Fmt_12bpc        = 0x0100,
        Fmt_16bpc_unorm  = 0x0200,
        Fmt_16bpc_sfloat = 0x0400,
        Fmt_32bpc        = 0x0800,

        Fmt_8bpc     = Fmt_8bpc_srgb   | Fmt_8bpc_unorm,
        Fmt_16bpc    = Fmt_16bpc_unorm | Fmt_16bpc_sfloat,
        Fmt_KnownHDR = Fmt_10bpc | Fmt_11bpc | Fmt_12bpc | Fmt_16bpc,
        Fmt_All      = Fmt_4bpc  | Fmt_5bpc  | Fmt_6bpc  | Fmt_8bpc  | Fmt_KnownHDR | Fmt_32bpc,

        Fmt_FreeSync2 = Fmt_10bpc | Fmt_16bpc,
    };

    struct Fmts
    {
        VkColorSpaceKHR     colorSpace;
        FmtSupport          fmtSupported;
    };

    static VkResult GetSupportedFormats(
        Pal::ScreenColorSpace   palColorSpaceMask,
        uint32_t*               pFormatCount,
        Fmts*                   pFormats);

    static bool IsFormatColorSpaceCompatible(
        Pal::ChNumFormat        palFormat,
        FmtSupport              bitSupport)
    {
        return (GetBitFormat(palFormat) & bitSupport) != 0;
    }

    static bool IsFmtHdr(const Fmts& format)
    {
        return  Util::TestAnyFlagSet(FmtSupport::Fmt_KnownHDR, format.fmtSupported);
    }

    static bool IsColorSpaceHdr(VkColorSpaceKHR colorSpace)
    {
        return (colorSpace != VK_COLORSPACE_SRGB_NONLINEAR_KHR);
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorSpaceHelper);

    static FmtSupport GetBitFormat(Pal::ChNumFormat palFormat);
};

} //namespace vk

#endif
