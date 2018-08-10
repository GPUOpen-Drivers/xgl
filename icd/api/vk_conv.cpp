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
 * @file  vk_conv.cpp
 * @brief Contains lookup table definitions for Vulkan conversion functions.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"

// The helper macros below are used exclusively by the format conversion table to map VkFormats to PAL SwizzledFormats.
#define PalFmt_Undefined Pal::UndefinedSwizzledFormat
#define PalFmtX          Pal::ChannelSwizzle::X
#define PalFmtY          Pal::ChannelSwizzle::Y
#define PalFmtZ          Pal::ChannelSwizzle::Z
#define PalFmtW          Pal::ChannelSwizzle::W
#define PalFmt0          Pal::ChannelSwizzle::Zero
#define PalFmt1          Pal::ChannelSwizzle::One

// For VK_FORMAT_R{r}G{g}B{b}A{a}_{numfmt}_PACKx:
#define PalFmt_RGBA_PACK(r, g, b, a, numfmt) \
    PalFmt(Pal::ChNumFormat::X##a##Y##b##Z##g##W##r##_##numfmt, PalFmtW, PalFmtZ, PalFmtY, PalFmtX)

// For VK_FORMAT_R{r}G{g}B{b}_{numfmt}_PACKx:
#define PalFmt_RGB_PACK(r, g, b, numfmt) \
    PalFmt(Pal::ChNumFormat::X##b##Y##g##Z##r##_##numfmt, PalFmtZ, PalFmtY, PalFmtX, PalFmt1)

// For VK_FORMAT_R{r}G{g}_{numfmt}_PACKx:
#define PalFmt_RG_PACK(r, g, numfmt) \
    PalFmt(Pal::ChNumFormat::X##g##Y##r##_##numfmt, PalFmtY, PalFmtX, PalFmt0, PalFmt1)

// For VK_FORMAT_A{a}R{r}G{g}B{b}_{numfmt}_PACKx:
#define PalFmt_ARGB_PACK(a, r, g, b, numfmt) \
    PalFmt(Pal::ChNumFormat::X##b##Y##g##Z##r##W##a##_##numfmt, PalFmtZ, PalFmtY, PalFmtX, PalFmtW)

// For VK_FORMAT_R{r}_{numfmt}:
#define PalFmt_R(r, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##_##numfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_R{r}G{g}_{numfmt}:
#define PalFmt_RG(r, g, numfmt) \
     PalFmt(Pal::ChNumFormat::X##r##Y##g##_##numfmt, PalFmtX, PalFmtY, PalFmt0, PalFmt1)

// For VK_FORMAT_R{r}G{g}B{b}_{numfmt}:
#define PalFmt_RGB(r, g, b, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_R{r}G{g}B{b}A{a}_{numfmt}:
#define PalFmt_RGBA(r, g, b, a, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##W##a##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_B{b}G{g}R{r}_{numfmt}_PACKx:
#define PalFmt_BGR_PACK(b, g, r, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_E{e}B{b}G{g}R{r}_{numfmt}_PACKx:
#define PalFmt_EBGR_PACK(e, b, g, r, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##E##e##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_D{d}_{numfmt}:
#define PalFmt_D(d, numfmt) \
    PalFmt(Pal::ChNumFormat::X##d##_##numfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_S{s}_{numfmt}:
#define PalFmt_S(s, numfmt) \
    PalFmt(Pal::ChNumFormat::X##s##_##numfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_D{d}_{numfmt}_S{s}_{numfmt}:
#define PalFmt_DS(d, dnumfmt, s, snumfmt) \
    PalFmt(Pal::ChNumFormat::D##d##_##dnumfmt##_S##s##_##snumfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_BC1_RGB_{numfmt}_BLOCK:
#define PalFmt_BC1_RGB(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc1_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_BC1_RGBA_{numfmt}_BLOCK:
#define PalFmt_BC1_RGBA(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc1_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_BC2_{numfmt}_BLOCK:
#define PalFmt_BC2(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc2_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_BC3_{numfmt}_BLOCK:
#define PalFmt_BC3(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc3_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_BC4_{numfmt}_BLOCK:
#define PalFmt_BC4(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc4_##numfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_BC5_{numfmt}_BLOCK:
#define PalFmt_BC5(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc5_##numfmt, PalFmtX, PalFmtY, PalFmt0, PalFmt1)

// For VK_FORMAT_BC6H_{numfmt}_BLOCK:
#define PalFmt_BC6H(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc6_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_BC7_{numfmt}_BLOCK:
#define PalFmt_BC7(numfmt) \
    PalFmt(Pal::ChNumFormat::Bc7_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_ETC2_R{r}G{g}B{b}_{numfmt}_BLOCK:
#define PalFmt_ETC2_RGB(r, g, b, numfmt) \
    PalFmt(Pal::ChNumFormat::Etc2##X##r##Y##g##Z##b##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_ETC2_R{r}G{g}B{b}A{a}_{numfmt}_BLOCK:
#define PalFmt_ETC2_RGBA(r, g, b, a, numfmt) \
    PalFmt(Pal::ChNumFormat::Etc2##X##r##Y##g##Z##b##W##a##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_EAC_R{r}_{numfmt}_BLOCK:
#define PalFmt_EAC_R(r, numfmt) \
    PalFmt(Pal::ChNumFormat::Etc2##X##r##_##numfmt, PalFmtX, PalFmt0, PalFmt0, PalFmt1)

// For VK_FORMAT_EAC_R{r}G{g}_{numfmt}_BLOCK:
#define PalFmt_EAC_RG(r, g, numfmt) \
    PalFmt(Pal::ChNumFormat::Etc2##X##r##Y##g##_##numfmt, PalFmtX, PalFmtY, PalFmt0, PalFmt1)

// For VK_FORMAT_ASTC_{w}x{h}_{numfmt}_BLOCK:
#define PalFmt_ASTC(w, h, numfmt) \
    PalFmt(Pal::ChNumFormat::AstcLdr##w##x##h##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

// For VK_FORMAT_B{b}G{g}R{r}A{a}_{numfmt}_PACKn:
#define PalFmt_BGRA_PACK(b, g, r, a, numfmt) \
    PalFmt(Pal::ChNumFormat::X##a##Y##r##Z##g##W##b##_##numfmt, PalFmtY, PalFmtZ, PalFmtW, PalFmtX)

// For VK_FORMAT_B{b}G{g}R{r}_{numfmt}_PACKn:
#define PalFmt_BGR_PACK(b, g, r, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmt1)

// For VK_FORMAT_B{b}G{g}R{r}A{a}_{numfmt}:
#define PalFmt_BGRA(b, g, r, a, numfmt) \
    PalFmt(Pal::ChNumFormat::X##b##Y##g##Z##r##W##a##_##numfmt, PalFmtZ, PalFmtY, PalFmtX, PalFmtW)

// For VK_FORMAT_A{a}B{b}G{g}R{r}_{numfmt}_PACKn:
#define PalFmt_ABGR_PACK(a, b, g, r, numfmt) \
    PalFmt(Pal::ChNumFormat::X##r##Y##g##Z##b##W##a##_##numfmt, PalFmtX, PalFmtY, PalFmtZ, PalFmtW)

namespace vk
{

// =====================================================================================================================
VK_TO_PAL_TABLE_X(  FORMAT, Format,                               SwizzledFormat,
// =====================================================================================================================
VK_TO_PAL_STRUC_X(  FORMAT_UNDEFINED,                             PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R4G4_UNORM_PACK8,                      PalFmt_RG_PACK(4, 4, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R4G4B4A4_UNORM_PACK16,                 PalFmt_RGBA_PACK(4, 4, 4, 4, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R5G6B5_UNORM_PACK16,                   PalFmt_RGB_PACK(5, 6, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R5G5B5A1_UNORM_PACK16,                 PalFmt_RGBA_PACK(5, 5, 5, 1, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8_UNORM,                              PalFmt_R(8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8_SNORM,                              PalFmt_R(8, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8_USCALED,                            PalFmt_R(8, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8_SSCALED,                            PalFmt_R(8, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8_UINT,                               PalFmt_R(8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R8_SINT,                               PalFmt_R(8, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R8_SRGB,                               PalFmt_R(8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_UNORM,                            PalFmt_RG(8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_SNORM,                            PalFmt_RG(8, 8, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_USCALED,                          PalFmt_RG(8, 8, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_SSCALED,                          PalFmt_RG(8, 8, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_UINT,                             PalFmt_RG(8, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_SINT,                             PalFmt_RG(8, 8, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8_SRGB,                             PalFmt_RG(8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_UNORM,                          PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_SNORM,                          PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_USCALED,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_SSCALED,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_UINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_SINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8_SRGB,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_UNORM,                        PalFmt_RGBA(8, 8, 8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_SNORM,                        PalFmt_RGBA(8, 8, 8, 8, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_USCALED,                      PalFmt_RGBA(8, 8, 8, 8, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_SSCALED,                      PalFmt_RGBA(8, 8, 8, 8, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_UINT,                         PalFmt_RGBA(8, 8, 8, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_SINT,                         PalFmt_RGBA(8, 8, 8, 8, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R8G8B8A8_SRGB,                         PalFmt_RGBA(8, 8, 8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_UNORM_PACK32,              PalFmt_ARGB_PACK(2, 10, 10, 10, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_SNORM_PACK32,              PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_USCALED_PACK32,            PalFmt_ARGB_PACK(2, 10, 10, 10, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_SSCALED_PACK32,            PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_UINT_PACK32,               PalFmt_ARGB_PACK(2, 10, 10, 10, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_A2R10G10B10_SINT_PACK32,               PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16_UNORM,                             PalFmt_R(16, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16_SNORM,                             PalFmt_R(16, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16_USCALED,                           PalFmt_R(16, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16_SSCALED,                           PalFmt_R(16, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16_UINT,                              PalFmt_R(16, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R16_SINT,                              PalFmt_R(16, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R16_SFLOAT,                            PalFmt_R(16, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_UNORM,                          PalFmt_RG(16, 16, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_SNORM,                          PalFmt_RG(16, 16, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_USCALED,                        PalFmt_RG(16, 16, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_SSCALED,                        PalFmt_RG(16, 16, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_UINT,                           PalFmt_RG(16, 16, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_SINT,                           PalFmt_RG(16, 16, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16_SFLOAT,                         PalFmt_RG(16, 16, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_UNORM,                       PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_SNORM,                       PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_USCALED,                     PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_SSCALED,                     PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_UINT,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_SINT,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16_SFLOAT,                      PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_UNORM,                    PalFmt_RGBA(16, 16, 16, 16, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_SNORM,                    PalFmt_RGBA(16, 16, 16, 16, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_USCALED,                  PalFmt_RGBA(16, 16, 16, 16, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_SSCALED,                  PalFmt_RGBA(16, 16, 16, 16, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_UINT,                     PalFmt_RGBA(16, 16, 16, 16, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_SINT,                     PalFmt_RGBA(16, 16, 16, 16, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R16G16B16A16_SFLOAT,                   PalFmt_RGBA(16, 16, 16, 16, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R32_UINT,                              PalFmt_R(32, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R32_SINT,                              PalFmt_R(32, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R32_SFLOAT,                            PalFmt_R(32, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32_UINT,                           PalFmt_RG(32, 32, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32_SINT,                           PalFmt_RG(32, 32, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32_SFLOAT,                         PalFmt_RG(32, 32, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32_UINT,                        PalFmt_RGB(32, 32, 32, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32_SINT,                        PalFmt_RGB(32, 32, 32, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32_SFLOAT,                      PalFmt_RGB(32, 32, 32, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32A32_UINT,                     PalFmt_RGBA(32, 32, 32, 32, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32A32_SINT,                     PalFmt_RGBA(32, 32, 32, 32, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_R32G32B32A32_SFLOAT,                   PalFmt_RGBA(32, 32, 32, 32, Float))
VK_TO_PAL_STRUC_X(  FORMAT_R64_SFLOAT,                            PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64_SFLOAT,                         PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64_SFLOAT,                      PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64A64_SFLOAT,                   PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64_UINT,                              PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64_UINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64_UINT,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64A64_UINT,                     PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64_SINT,                              PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64_SINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64_SINT,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_R64G64B64A64_SINT,                     PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B10G11R11_UFLOAT_PACK32,               PalFmt_BGR_PACK(10, 11, 11, Float))
VK_TO_PAL_STRUC_X(  FORMAT_E5B9G9R9_UFLOAT_PACK32,                PalFmt_EBGR_PACK(5, 9, 9, 9, Float))
VK_TO_PAL_STRUC_X(  FORMAT_D16_UNORM,                             PalFmt_D(16, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_X8_D24_UNORM_PACK32,                   PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_D32_SFLOAT,                            PalFmt_D(32, Float))
VK_TO_PAL_STRUC_X(  FORMAT_S8_UINT,                               PalFmt_S(8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_D16_UNORM_S8_UINT,                     PalFmt_DS(16, Unorm, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_D24_UNORM_S8_UINT,                     PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_D32_SFLOAT_S8_UINT,                    PalFmt_DS(32, Float, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_BC1_RGB_UNORM_BLOCK,                   PalFmt_BC1_RGB(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC1_RGB_SRGB_BLOCK,                    PalFmt_BC1_RGB(Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_BC1_RGBA_UNORM_BLOCK,                  PalFmt_BC1_RGBA(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC1_RGBA_SRGB_BLOCK,                   PalFmt_BC1_RGBA(Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_BC2_UNORM_BLOCK,                       PalFmt_BC2(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC2_SRGB_BLOCK,                        PalFmt_BC2(Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_BC3_UNORM_BLOCK,                       PalFmt_BC3(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC3_SRGB_BLOCK,                        PalFmt_BC3(Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_BC4_UNORM_BLOCK,                       PalFmt_BC4(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC4_SNORM_BLOCK,                       PalFmt_BC4(Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC5_UNORM_BLOCK,                       PalFmt_BC5(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC5_SNORM_BLOCK,                       PalFmt_BC5(Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC6H_UFLOAT_BLOCK,                     PalFmt_BC6H(Ufloat))
VK_TO_PAL_STRUC_X(  FORMAT_BC6H_SFLOAT_BLOCK,                     PalFmt_BC6H(Sfloat))
VK_TO_PAL_STRUC_X(  FORMAT_BC7_UNORM_BLOCK,                       PalFmt_BC7(Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_BC7_SRGB_BLOCK,                        PalFmt_BC7(Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8_UNORM_BLOCK,               PalFmt_ETC2_RGB(8, 8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8_SRGB_BLOCK,                PalFmt_ETC2_RGB(8, 8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,             PalFmt_ETC2_RGBA(8, 8, 8, 1, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,              PalFmt_ETC2_RGBA(8, 8, 8, 1, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,             PalFmt_ETC2_RGBA(8, 8, 8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,              PalFmt_ETC2_RGBA(8, 8, 8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_EAC_R11_UNORM_BLOCK,                   PalFmt_EAC_R(11, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_EAC_R11_SNORM_BLOCK,                   PalFmt_EAC_R(11, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_EAC_R11G11_UNORM_BLOCK,                PalFmt_EAC_RG(11, 11, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_EAC_R11G11_SNORM_BLOCK,                PalFmt_EAC_RG(11, 11, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_4x4_UNORM_BLOCK,                  PalFmt_ASTC(4, 4, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_4x4_SRGB_BLOCK,                   PalFmt_ASTC(4, 4, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_5x4_UNORM_BLOCK,                  PalFmt_ASTC(5, 4, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_5x4_SRGB_BLOCK,                   PalFmt_ASTC(5, 4, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_5x5_UNORM_BLOCK,                  PalFmt_ASTC(5, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_5x5_SRGB_BLOCK,                   PalFmt_ASTC(5, 5, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_6x5_UNORM_BLOCK,                  PalFmt_ASTC(6, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_6x5_SRGB_BLOCK,                   PalFmt_ASTC(6, 5, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_6x6_UNORM_BLOCK,                  PalFmt_ASTC(6, 6, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_6x6_SRGB_BLOCK,                   PalFmt_ASTC(6, 6, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x5_UNORM_BLOCK,                  PalFmt_ASTC(8, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x5_SRGB_BLOCK,                   PalFmt_ASTC(8, 5, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x6_UNORM_BLOCK,                  PalFmt_ASTC(8, 6, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x6_SRGB_BLOCK,                   PalFmt_ASTC(8, 6, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x8_UNORM_BLOCK,                  PalFmt_ASTC(8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_8x8_SRGB_BLOCK,                   PalFmt_ASTC(8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x5_UNORM_BLOCK,                 PalFmt_ASTC(10, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x5_SRGB_BLOCK,                  PalFmt_ASTC(10, 5, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x6_UNORM_BLOCK,                 PalFmt_ASTC(10, 6, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x6_SRGB_BLOCK,                  PalFmt_ASTC(10, 6, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x8_UNORM_BLOCK,                 PalFmt_ASTC(10, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x8_SRGB_BLOCK,                  PalFmt_ASTC(10, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x10_UNORM_BLOCK,                PalFmt_ASTC(10, 10, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_10x10_SRGB_BLOCK,                 PalFmt_ASTC(10, 10, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_12x10_UNORM_BLOCK,                PalFmt_ASTC(12, 10, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_12x10_SRGB_BLOCK,                 PalFmt_ASTC(12, 10, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_12x12_UNORM_BLOCK,                PalFmt_ASTC(12, 12, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_ASTC_12x12_SRGB_BLOCK,                 PalFmt_ASTC(12, 12, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_B4G4R4A4_UNORM_PACK16,                 PalFmt_BGRA_PACK(4, 4, 4, 4, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_B5G5R5A1_UNORM_PACK16,                 PalFmt_BGRA_PACK(5, 5, 5, 1, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_B5G6R5_UNORM_PACK16,                   PalFmt_BGR_PACK(5, 6, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_UNORM,                          PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_SNORM,                          PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_USCALED,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_SSCALED,                        PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_UINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_SINT,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8_SRGB,                           PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_UNORM,                        PalFmt_BGRA(8, 8, 8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_SNORM,                        PalFmt_BGRA(8, 8, 8, 8, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_USCALED,                      PalFmt_BGRA(8, 8, 8, 8, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_SSCALED,                      PalFmt_BGRA(8, 8, 8, 8, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_UINT,                         PalFmt_BGRA(8, 8, 8, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_SINT,                         PalFmt_BGRA(8, 8, 8, 8, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_B8G8R8A8_SRGB,                         PalFmt_BGRA(8, 8, 8, 8, Srgb))
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_UNORM_PACK32,              PalFmt_ABGR_PACK(2, 10, 10, 10, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_SNORM_PACK32,              PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_USCALED_PACK32,            PalFmt_ABGR_PACK(2, 10, 10, 10, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_SSCALED_PACK32,            PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_UINT_PACK32,               PalFmt_ABGR_PACK(2, 10, 10, 10, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_A2B10G10R10_SINT_PACK32,               PalFmt_Undefined)
VK_TO_PAL_STRUC_X(  FORMAT_A1R5G5B5_UNORM_PACK16,                 PalFmt_ARGB_PACK(1, 5, 5, 5, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_UNORM_PACK32,                 PalFmt_ABGR_PACK(8, 8, 8, 8, Unorm))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_SNORM_PACK32,                 PalFmt_ABGR_PACK(8, 8, 8, 8, Snorm))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_USCALED_PACK32,               PalFmt_ABGR_PACK(8, 8, 8, 8, Uscaled))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_SSCALED_PACK32,               PalFmt_ABGR_PACK(8, 8, 8, 8, Sscaled))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_UINT_PACK32,                  PalFmt_ABGR_PACK(8, 8, 8, 8, Uint))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_SINT_PACK32,                  PalFmt_ABGR_PACK(8, 8, 8, 8, Sint))
VK_TO_PAL_STRUC_X(  FORMAT_A8B8G8R8_SRGB_PACK32,                  PalFmt_ABGR_PACK(8, 8, 8, 8, Srgb))
)

// =====================================================================================================================
// Macro to provide the storage of lookup tables needed by non-identity conversions
#define VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX(srcType, dstType, convertFunc) \
    VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX_WITH_SUFFIX(srcType, dstType, convertFunc, )

#define VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX_AMD(srcType, dstType, convertFunc) \
    VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX_WITH_SUFFIX(srcType, dstType, convertFunc, _AMD)

#define VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX_WITH_SUFFIX(srcType, dstType, convertFunc, suffix) \
    namespace convert \
    { \
        dstType VkToPal##convertFunc##LookupTableStorage[VK_##srcType##_END_RANGE##suffix + 1]; \
        const dstType* VkToPal##convertFunc##LookupTable = InitVkToPal##convertFunc##LookupTable(); \
        VK_DBG_DECL(bool VkToPal##convertFunc##Valid[VK_##srcType##_END_RANGE##suffix + 1]); \
    }

// =====================================================================================================================
// Macro to provide the storage of lookup tables needed by non-identity conversions
#define VK_TO_PAL_DECL_LOOKUP_TABLE(srcType, dstType) \
    VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX(srcType, Pal::dstType, dstType)

#define VK_TO_PAL_DECL_LOOKUP_TABLE_AMD(srcType, dstType) \
    VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX_AMD(srcType, Pal::dstType, dstType)

// =====================================================================================================================
// Storage declarations of lookup tables used by non-identity conversions
VK_TO_PAL_DECL_LOOKUP_TABLE(PRIMITIVE_TOPOLOGY,             PrimitiveTopology                                          )
VK_TO_PAL_DECL_LOOKUP_TABLE(FORMAT,                         SwizzledFormat                                             )
VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX(PRIMITIVE_TOPOLOGY,     PalPrimTypeAdjacency, PrimTypeAdjacency                    )
VK_TO_PAL_DECL_LOOKUP_TABLE_COMPLEX(QUERY_TYPE,             PalQueryTypePool,     QueryTypePool                        )
VK_TO_PAL_DECL_LOOKUP_TABLE(INDEX_TYPE,                     IndexType                                                  )
VK_TO_PAL_DECL_LOOKUP_TABLE(BORDER_COLOR,                   BorderColorType                                            )
VK_TO_PAL_DECL_LOOKUP_TABLE(IMAGE_VIEW_TYPE,                ImageViewType                                              )
VK_TO_PAL_DECL_LOOKUP_TABLE(LOGIC_OP,                       LogicOp                                                    )
VK_TO_PAL_DECL_LOOKUP_TABLE(SAMPLER_ADDRESS_MODE,           TexAddressMode                                             )
VK_TO_PAL_DECL_LOOKUP_TABLE(POLYGON_MODE,                   FillMode                                                   )
VK_TO_PAL_DECL_LOOKUP_TABLE(IMAGE_TILING,                   ImageTiling                                                )
VK_TO_PAL_DECL_LOOKUP_TABLE(COMPONENT_SWIZZLE,              ChannelSwizzle                                             )
VK_TO_PAL_DECL_LOOKUP_TABLE(PIPELINE_BIND_POINT,            PipelineBindPoint                                          )

// =====================================================================================================================
// Converts a PAL::Result value to an equivalent string name
const char* PalResultName(
    Pal::Result result)
{
    const char* resultName = nullptr;

    switch (result)
    {
    case Pal::Result::TooManyFlippableAllocations:
        resultName = "TooManyFlippableAllocations";
        break;

    case Pal::Result::PresentOccluded:
        resultName = "PresentOccluded";
        break;

    case Pal::Result::Unsupported:
        resultName = "Unsupported";
        break;

    case Pal::Result::NotReady:
        resultName = "NotReady";
        break;

    case Pal::Result::Timeout:
        resultName = "Timeout";
        break;

    case Pal::Result::ErrorFenceNeverSubmitted:
        resultName = "ErrorFenceNeverSubmitted";
        break;

    case Pal::Result::EventSet:
        resultName = "EventSet";
        break;

    case Pal::Result::EventReset:
        resultName = "EventReset";
        break;

    case Pal::Result::ErrorInitializationFailed:
        resultName = "ErrorInitializationFailed";
        break;

    case Pal::Result::ErrorOutOfMemory:
        resultName = "ErrorOutOfMemory";
        break;

    case Pal::Result::ErrorOutOfGpuMemory:
        resultName = "ErrorOutOfGpuMemory";
        break;

    case Pal::Result::ErrorDeviceLost:
        resultName = "ErrorDeviceLost";
        break;

    case Pal::Result::ErrorIncompatibleLibrary:
        resultName = "ErrorIncompatibleLibrary";
        break;

    case Pal::Result::ErrorGpuMemoryMapFailed:
        resultName = "ErrorGpuMemoryMapFailed";
        break;

    case Pal::Result::ErrorNotMappable:
        resultName = "ErrorNotMappable";
        break;

    case Pal::Result::ErrorUnknown:
        resultName = "ErrorUnknown";
        break;

    case Pal::Result::ErrorUnavailable:
        resultName = "ErrorUnavailable";
        break;

    case Pal::Result::ErrorInvalidPointer:
        resultName = "ErrorInvalidPointer";
        break;

    case Pal::Result::ErrorInvalidValue:
        resultName = "ErrorInvalidValue";
        break;

    case Pal::Result::ErrorInvalidOrdinal:
        resultName = "ErrorInvalidOrdinal";
        break;

    case Pal::Result::ErrorInvalidMemorySize:
        resultName = "ErrorInvalidMemorySize";
        break;

    case Pal::Result::ErrorInvalidFlags:
        resultName = "ErrorInvalidFlags";
        break;

    case Pal::Result::ErrorInvalidAlignment:
        resultName = "ErrorInvalidAlignment";
        break;

    case Pal::Result::ErrorInvalidFormat:
        resultName = "ErrorInvalidFormat";
        break;

    case Pal::Result::ErrorInvalidImage:
        resultName = "ErrorInvalidImage";
        break;

    case Pal::Result::ErrorInvalidDescriptorSetData:
        resultName = "ErrorInvalidDescriptorSetData";
        break;

    case Pal::Result::ErrorInvalidQueueType:
        resultName = "ErrorInvalidQueueType";
        break;

    case Pal::Result::ErrorUnsupportedShaderIlVersion:
        resultName = "ErrorUnsupportedShaderIlVersion";
        break;

    case Pal::Result::ErrorBadShaderCode:
        resultName = "ErrorBadShaderCode";
        break;

    case Pal::Result::ErrorBadPipelineData:
        resultName = "ErrorBadPipelineData";
        break;

    case Pal::Result::ErrorGpuMemoryUnmapFailed:
        resultName = "ErrorGpuMemoryUnmapFailed";
        break;

    case Pal::Result::ErrorIncompatibleDevice:
        resultName = "ErrorIncompatibleDevice";
        break;

    case Pal::Result::ErrorBuildingCommandBuffer:
        resultName = "ErrorBuildingCommandBuffer";
        break;

    case Pal::Result::ErrorGpuMemoryNotBound:
        resultName = "ErrorGpuMemoryNotBound";
        break;

    case Pal::Result::ErrorImageNotShaderAccessible:
        resultName = "ErrorImageNotShaderAccessible";
        break;

    case Pal::Result::ErrorInvalidUsageForFormat:
        resultName = "ErrorInvalidUsageForFormat";
        break;

    case Pal::Result::ErrorFormatIncompatibleWithImageUsage:
        resultName = "ErrorFormatIncompatibleWithImageUsage";
        break;

    case Pal::Result::ErrorThreadGroupTooBig:
        resultName = "ErrorThreadGroupTooBig";
        break;

    case Pal::Result::ErrorInvalidMsaaMipLevels:
        resultName = "ErrorInvalidMsaaMipLevels";
        break;

    case Pal::Result::ErrorInvalidSampleCount:
        resultName = "ErrorInvalidSampleCount";
        break;

    case Pal::Result::ErrorInvalidImageArraySize:
        resultName = "ErrorInvalidImageArraySize";
        break;

    case Pal::Result::ErrorInvalid3dImageArraySize:
        resultName = "ErrorInvalid3dImageArraySize";
        break;

    case Pal::Result::ErrorInvalidImageWidth:
        resultName = "ErrorInvalidImageWidth";
        break;

    case Pal::Result::ErrorInvalidImageHeight:
        resultName = "ErrorInvalidImageHeight";
        break;

    case Pal::Result::ErrorInvalidImageDepth:
        resultName = "ErrorInvalidImageDepth";
        break;

    case Pal::Result::ErrorInvalidMipCount:
        resultName = "ErrorInvalidMipCount";
        break;

    case Pal::Result::ErrorInvalidBaseMipLevel:
        resultName = "ErrorInvalidBaseMipLevel";
        break;

    case Pal::Result::ErrorInvalidViewArraySize:
        resultName = "ErrorInvalidViewArraySize";
        break;

    case Pal::Result::ErrorInvalidViewBaseSlice:
        resultName = "ErrorInvalidViewBaseSlice";
        break;

    case Pal::Result::ErrorInsufficientImageArraySize:
        resultName = "ErrorInsufficientImageArraySize";
        break;

    case Pal::Result::ErrorCubemapNonSquareFaceSize:
        resultName = "ErrorCubemapNonSquareFaceSize";
        break;

    case Pal::Result::ErrorInvalidImageTargetUsage:
        resultName = "ErrorInvalidImageTargetUsage";
        break;

    case Pal::Result::ErrorMissingDepthStencilUsage:
        resultName = "ErrorMissingDepthStencilUsage";
        break;

    case Pal::Result::ErrorInvalidColorTargetType:
        resultName = "ErrorInvalidColorTargetType";
        break;

    case Pal::Result::ErrorInvalidDepthTargetType:
        resultName = "ErrorInvalidDepthTargetType";
        break;

    case Pal::Result::ErrorInvalidMsaaType:
        resultName = "ErrorInvalidMsaaType";
        break;

    case Pal::Result::ErrorInvalidCompressedImageType:
        resultName = "ErrorInvalidCompressedImageType";
        break;

    case Pal::Result::ErrorImageAspectUnavailable:
        resultName = "ErrorImageAspectUnavailable";
        break;

    case Pal::Result::ErrorInvalidFormatSwizzle:
        resultName = "ErrorInvalidFormatSwizzle";
        break;

    case Pal::Result::ErrorViewTypeIncompatibleWithImageType:
        resultName = "ErrorViewTypeIncompatibleWithImageType";
        break;

    case Pal::Result::ErrorCubemapIncompatibleWithMsaa:
        resultName = "ErrorCubemapIncompatibleWithMsaa";
        break;

    case Pal::Result::ErrorInvalidMsaaFormat:
        resultName = "ErrorInvalidMsaaFormat";
        break;

    case Pal::Result::ErrorFormatIncompatibleWithImageFormat:
        resultName = "ErrorFormatIncompatibleWithImageFormat";
        break;

    case Pal::Result::ErrorFormatIncompatibleWithImageAspect:
        resultName = "ErrorFormatIncompatibleWithImageAspect";
        break;

    case Pal::Result::ErrorFullscreenUnavailable:
        resultName = "ErrorFullscreenUnavailable";
        break;

    case Pal::Result::ErrorScreenRemoved:
        resultName = "ErrorScreenRemoved";
        break;

    case Pal::Result::ErrorIncompatibleScreenMode:
        resultName = "ErrorIncompatibleScreenMode";
        break;

    case Pal::Result::ErrorMultiDevicePresentFailed:
        resultName = "ErrorMultiDevicePresentFailed";
        break;

    case Pal::Result::ErrorWindowedPresentUnavailable:
        resultName = "ErrorWindowedPresentUnavailable";
        break;

    case Pal::Result::ErrorInvalidResolution:
        resultName = "ErrorInvalidResolution";
        break;

    case Pal::Result::ErrorInvalidObjectType:
        resultName = "ErrorInvalidObjectType";
        break;

    case Pal::Result::ErrorTooManyMemoryReferences:
        resultName = "ErrorTooManyMemoryReferences";
        break;

    case Pal::Result::ErrorNotShareable:
        resultName = "ErrorNotShareable";
        break;

    case Pal::Result::ErrorImageFmaskUnavailable:
        resultName = "ErrorImageFmaskUnavailable";
        break;

    case Pal::Result::ErrorPrivateScreenRemoved:
        resultName = "ErrorPrivateScreenRemoved";
        break;

    case Pal::Result::ErrorPrivateScreenUsed:
        resultName = "ErrorPrivateScreenUsed";
        break;

    case Pal::Result::ErrorTooManyPrivateDisplayImages:
        resultName = "ErrorTooManyPrivateDisplayImages";
        break;

    case Pal::Result::ErrorPrivateScreenNotEnabled:
        resultName = "ErrorPrivateScreenNotEnabled";
        break;

    default:
        VK_NOT_IMPLEMENTED;
        resultName = "??";
        break;
    }

    return resultName;
}

// =====================================================================================================================
// Converts a VkResult value to an equivalent string name
const char* VkResultName(
    VkResult result)
{
    const char* errName = nullptr;

    switch (result)
    {
    case VkResult::VK_SUCCESS:
        errName = "VK_SUCCESS";
        break;

    case VkResult::VK_NOT_READY:
        errName = "VK_NOT_READY";
        break;

    case VkResult::VK_TIMEOUT:
        errName = "VK_TIMEOUT";
        break;

    case VkResult::VK_EVENT_SET:
        errName = "VK_EVENT_SET";
        break;

    case VkResult::VK_EVENT_RESET:
        errName = "VK_EVENT_RESET";
        break;

    case VkResult::VK_INCOMPLETE:
        errName = "VK_INCOMPLETE";
        break;

    case VkResult::VK_ERROR_OUT_OF_HOST_MEMORY:
        errName = "VK_ERROR_OUT_OF_HOST_MEMORY";
        break;

    case VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY:
        errName = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        break;

    case VkResult::VK_ERROR_INITIALIZATION_FAILED:
        errName = "VK_ERROR_INITIALIZATION_FAILED";
        break;

    case VkResult::VK_ERROR_DEVICE_LOST:
        errName = "VK_ERROR_DEVICE_LOST";
        break;

    case VkResult::VK_ERROR_MEMORY_MAP_FAILED:
        errName = "VK_ERROR_MEMORY_MAP_FAILED";
        break;

    case VkResult::VK_ERROR_LAYER_NOT_PRESENT:
        errName = "VK_ERROR_LAYER_NOT_PRESENT";
        break;

    case VkResult::VK_ERROR_EXTENSION_NOT_PRESENT:
        errName = "VK_ERROR_EXTENSION_NOT_PRESENT";
        break;

    case VkResult::VK_ERROR_FEATURE_NOT_PRESENT:
        errName = "VK_ERROR_FEATURE_NOT_PRESENT";
        break;

    case VkResult::VK_ERROR_INCOMPATIBLE_DRIVER:
        errName = "VK_ERROR_INCOMPATIBLE_DRIVER";
        break;

    case VkResult::VK_ERROR_TOO_MANY_OBJECTS:
        errName = "VK_ERROR_TOO_MANY_OBJECTS";
        break;

    case VkResult::VK_ERROR_FORMAT_NOT_SUPPORTED:
        errName = "VK_ERROR_FORMAT_NOT_SUPPORTED";
        break;

    case VkResult::VK_ERROR_FRAGMENTED_POOL:
        errName = "VK_ERROR_FRAGMENTED_POOL";
        break;

    case VkResult::VK_ERROR_OUT_OF_POOL_MEMORY:
        errName = "VK_ERROR_OUT_OF_POOL_MEMORY";
        break;

    case VkResult::VK_ERROR_INVALID_EXTERNAL_HANDLE:
        errName = "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        break;

    case VkResult::VK_ERROR_SURFACE_LOST_KHR:
        errName = "VK_ERROR_SURFACE_LOST_KHR";
        break;

    case VkResult::VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        errName = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        break;

    case VkResult::VK_SUBOPTIMAL_KHR:
        errName = "VK_SUBOPTIMAL_KHR";
        break;

    case VkResult::VK_ERROR_OUT_OF_DATE_KHR:
        errName = "VK_ERROR_OUT_OF_DATE_KHR";
        break;

    case VkResult::VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        errName = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        break;

    case VkResult::VK_ERROR_VALIDATION_FAILED_EXT:
        errName = "VK_ERROR_VALIDATION_FAILED_EXT";
        break;

    case VkResult::VK_ERROR_INVALID_SHADER_NV:
        errName = "VK_ERROR_INVALID_SHADER_NV";
        break;

    case VkResult::VK_ERROR_NOT_PERMITTED_EXT:
        errName = "VK_ERROR_NOT_PERMITTED_EXT";
        break;

    default:
        VK_NOT_IMPLEMENTED;
        errName = "??";
        break;
    };

    return errName;
}

// =====================================================================================================================
// Converts a non-Success PAL result to an equivalent VK error
VkResult PalToVkError(
    Pal::Result result)
{
    VK_ASSERT(result != Pal::Result::Success);

    VkResult vkResult = VK_SUCCESS;

    switch (result)
    {
    // These PAL error codes currently aren't handled specially and they indicate success otherwise
    case Pal::Result::TooManyFlippableAllocations:
    case Pal::Result::PresentOccluded:
        vkResult = VK_SUCCESS;
        break;

    case Pal::Result::Unsupported:
        vkResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
        break;

    case Pal::Result::NotReady:
        vkResult = VK_NOT_READY;
        break;

    case Pal::Result::Timeout:
    case Pal::Result::ErrorFenceNeverSubmitted:
        vkResult = VK_TIMEOUT;
        break;

    case Pal::Result::EventSet:
        vkResult = VK_EVENT_SET;
        break;

    case Pal::Result::EventReset:
        vkResult = VK_EVENT_RESET;
        break;

    case Pal::Result::ErrorInitializationFailed:
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        break;

    case Pal::Result::ErrorOutOfMemory:
        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        break;

    case Pal::Result::ErrorOutOfGpuMemory:
        vkResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        break;

    case Pal::Result::ErrorDeviceLost:
        vkResult = VK_ERROR_DEVICE_LOST;
        break;

    case Pal::Result::ErrorIncompatibleLibrary:
        vkResult = VK_ERROR_INCOMPATIBLE_DRIVER;
        break;

    case Pal::Result::ErrorGpuMemoryMapFailed:
        vkResult = VK_ERROR_MEMORY_MAP_FAILED;
        break;

    case Pal::Result::ErrorNotMappable:
        vkResult = VK_ERROR_MEMORY_MAP_FAILED;
        break;

    case Pal::Result::ErrorUnknown:
    case Pal::Result::ErrorUnavailable:
    case Pal::Result::ErrorInvalidPointer:
    case Pal::Result::ErrorInvalidValue:
    case Pal::Result::ErrorInvalidOrdinal:
    case Pal::Result::ErrorInvalidMemorySize:
    case Pal::Result::ErrorInvalidFlags:
    case Pal::Result::ErrorInvalidAlignment:
    case Pal::Result::ErrorInvalidFormat:
    case Pal::Result::ErrorInvalidImage:
    case Pal::Result::ErrorInvalidDescriptorSetData:
    case Pal::Result::ErrorInvalidQueueType:
    case Pal::Result::ErrorUnsupportedShaderIlVersion:
    case Pal::Result::ErrorBadShaderCode:
    case Pal::Result::ErrorBadPipelineData:
    case Pal::Result::ErrorGpuMemoryUnmapFailed:
    case Pal::Result::ErrorIncompatibleDevice:
    case Pal::Result::ErrorBuildingCommandBuffer:
    case Pal::Result::ErrorGpuMemoryNotBound:
    case Pal::Result::ErrorImageNotShaderAccessible:
    case Pal::Result::ErrorInvalidUsageForFormat:
    case Pal::Result::ErrorFormatIncompatibleWithImageUsage:
    case Pal::Result::ErrorThreadGroupTooBig:
    case Pal::Result::ErrorInvalidMsaaMipLevels:
    case Pal::Result::ErrorInvalidSampleCount:
    case Pal::Result::ErrorInvalidImageArraySize:
    case Pal::Result::ErrorInvalid3dImageArraySize:
    case Pal::Result::ErrorInvalidImageWidth:
    case Pal::Result::ErrorInvalidImageHeight:
    case Pal::Result::ErrorInvalidImageDepth:
    case Pal::Result::ErrorInvalidMipCount:
    case Pal::Result::ErrorInvalidBaseMipLevel:
    case Pal::Result::ErrorInvalidViewArraySize:
    case Pal::Result::ErrorInvalidViewBaseSlice:
    case Pal::Result::ErrorInsufficientImageArraySize:
    case Pal::Result::ErrorCubemapNonSquareFaceSize:
    case Pal::Result::ErrorInvalidImageTargetUsage:
    case Pal::Result::ErrorMissingDepthStencilUsage:
    case Pal::Result::ErrorInvalidColorTargetType:
    case Pal::Result::ErrorInvalidDepthTargetType:
    case Pal::Result::ErrorInvalidMsaaType:
    case Pal::Result::ErrorInvalidCompressedImageType:
    case Pal::Result::ErrorImageAspectUnavailable:
    case Pal::Result::ErrorInvalidFormatSwizzle:
    case Pal::Result::ErrorViewTypeIncompatibleWithImageType:
    case Pal::Result::ErrorCubemapIncompatibleWithMsaa:
    case Pal::Result::ErrorInvalidMsaaFormat:
    case Pal::Result::ErrorFormatIncompatibleWithImageFormat:
    case Pal::Result::ErrorFormatIncompatibleWithImageAspect:
    case Pal::Result::ErrorFullscreenUnavailable:
    case Pal::Result::ErrorScreenRemoved:
    case Pal::Result::ErrorIncompatibleScreenMode:
    case Pal::Result::ErrorMultiDevicePresentFailed:
    case Pal::Result::ErrorWindowedPresentUnavailable:
    case Pal::Result::ErrorInvalidResolution:
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        break;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    case Pal::Result::ErrorIncompatibleDisplayMode:
        vkResult = VK_ERROR_OUT_OF_DATE_KHR;
        break;
#endif
    case Pal::Result::ErrorInvalidObjectType:
        // This is only generated by RemapVirtualMemoryPages currently which is only used
        // internally by us, thus should never be triggered, fall through to the default path.
    case Pal::Result::ErrorTooManyMemoryReferences:
        // The memory reference list is managed by the API layer thus this error should
        // never get to the client, fall through to the default path.
    case Pal::Result::ErrorNotShareable:
        // This is only used for cross-GPU sharing which the API layer doesn't support yet.
        // Fall through to the default path.
    case Pal::Result::ErrorImageFmaskUnavailable:
        // Fmask based reads will be handled by the API layer thus this error should
        // never get to the client, fall through to the default path.
    case Pal::Result::ErrorPrivateScreenRemoved:
    case Pal::Result::ErrorPrivateScreenUsed:
    case Pal::Result::ErrorTooManyPrivateDisplayImages:
    case Pal::Result::ErrorPrivateScreenNotEnabled:
        // There's no private screen support yet. Fall through to the default path.
    default:
        VK_NOT_IMPLEMENTED;
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        break;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    const char* palErrorName = PalResultName(result);
    const char* vkErrorName = VkResultName(vkResult);
    PAL_DPINFO("Vulkan error: %s(%d), from Pal error: Pal::Result::%s(%d)", vkErrorName, vkResult, palErrorName, result);
#endif

    return vkResult;
}

} // namespace vk
