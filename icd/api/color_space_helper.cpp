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
* @file  color_space_helper.cpp
* @brief Helper class to convert Pal to Vulkan API data formats
***********************************************************************************************************************
*/

#include "include/color_space_helper.h"
#include "include/vk_utils.h"

#include "palFormatInfo.h"
#include "palScreen.h"

namespace vk
{

using namespace Pal;

typedef ColorSpaceHelper::FmtSupport FmtSupport;

struct LookupDefines
{
    Pal::ScreenColorSpace        mask;
    VkColorSpaceKHR              colorSpace;
    ColorSpaceHelper::FmtSupport fmtSupported;
};

const LookupDefines colorspaceLookup[] =
{
    { Pal::ScreenColorSpace::CsSrgb,         VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       FmtSupport::Fmt_8bpc         },
    { Pal::ScreenColorSpace::CsBt709,        VK_COLOR_SPACE_BT709_NONLINEAR_EXT,      FmtSupport::Fmt_All          },
    { Pal::ScreenColorSpace::TfHlg,          VK_COLOR_SPACE_HDR10_HLG_EXT,            FmtSupport::Fmt_KnownHDR     },
    { Pal::ScreenColorSpace::TfPq2084,       VK_COLOR_SPACE_HDR10_ST2084_EXT,         FmtSupport::Fmt_10bpc        },
    { Pal::ScreenColorSpace::TfDolbyVision,  VK_COLOR_SPACE_DOLBYVISION_EXT,          FmtSupport::Fmt_8bpc_unorm   },
    { Pal::ScreenColorSpace::CsBt2020,       VK_COLOR_SPACE_BT2020_LINEAR_EXT,        FmtSupport::Fmt_10bpc        },
    { Pal::ScreenColorSpace::CsAdobe,        VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT,      FmtSupport::Fmt_All          },
    { Pal::ScreenColorSpace::CsDciP3,        VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT,     FmtSupport::Fmt_All          },
    { Pal::ScreenColorSpace::CsScrgb,        VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, FmtSupport::Fmt_16bpc_sfloat },
    { Pal::ScreenColorSpace::CsUserDefined,  VK_COLOR_SPACE_PASS_THROUGH_EXT,         FmtSupport::Fmt_All          },
};

static const uint32_t colorspaceLookupSize = sizeof(colorspaceLookup) / sizeof(colorspaceLookup[0]);

// =====================================================================================================================
// Returns all the formats and color spaces corresponding to a Pal::ScreenColorSpace bitfield
VkResult ColorSpaceHelper::GetSupportedFormats(
    Pal::ScreenColorSpace   palColorSpaceMask,
    uint32_t*               pFormatCount,
    Fmts*                   pFormats)
{
    uint32_t       numSupportedFormats = 0;
    const uint32_t maxToWrite = (pFormats != nullptr) ? *pFormatCount : 0;

    for (uint32_t i = 0; i < colorspaceLookupSize; i++)
    {
        if (colorspaceLookup[i].mask == (colorspaceLookup[i].mask & palColorSpaceMask))
        {
            if (numSupportedFormats < maxToWrite)
            {
                pFormats[numSupportedFormats].colorSpace   = colorspaceLookup[i].colorSpace;
                pFormats[numSupportedFormats].fmtSupported = colorspaceLookup[i].fmtSupported;
            }
            ++numSupportedFormats;
        }
    }

    VkResult result;

    if ((pFormats != nullptr) && (numSupportedFormats > maxToWrite))
    {
        *pFormatCount = maxToWrite;
        result        = VK_INCOMPLETE;
    }
    else
    {
        *pFormatCount = numSupportedFormats;
        result        = VK_SUCCESS;
    }

    return result;
}

// =====================================================================================================================
// Returns FmtSupport from Pal::ChNumFormat input
ColorSpaceHelper::FmtSupport ColorSpaceHelper::GetBitFormat(Pal::ChNumFormat palFormat)
{
    const uint32_t bitCount = Pal::Formats::MaxComponentBitCount(palFormat);

    FmtSupport  fmt = Fmt_Undefined;

    switch (bitCount)
    {
        case 0:
            break;
        case 4:
            fmt = Fmt_4bpc;
            break;
        case 5:
            fmt = Fmt_6bpc;
            break;
        case 6:
            fmt = Fmt_6bpc;
            break;
        case 8:
            if (Pal::Formats::IsSrgb(palFormat))
            {
                fmt = Fmt_8bpc_srgb;
            }
            else
            {
                VK_ASSERT(Pal::Formats::IsUnorm(palFormat));
                fmt = Fmt_8bpc_unorm;
            }
            break;
        case 9:
            fmt = Fmt_9bpc;
            break;
        case 10:
            fmt = Fmt_10bpc;
            break;
        case 11:
            fmt = Fmt_10bpc;
            break;
        case 12:
            fmt = Fmt_12bpc;
            break;
        case 16:
            if (Pal::Formats::IsFloat(palFormat))
            {
                fmt = Fmt_16bpc_sfloat;
            }
            else
            {
                VK_ASSERT(Pal::Formats::IsUnorm(palFormat));
                fmt = Fmt_16bpc_unorm;
            }
            break;
        case 32:
            fmt = Fmt_32bpc;
            break;
        default:
            VK_NOT_IMPLEMENTED;
            break;
    };
    return fmt;
}

} //namespace vk

