/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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

/**
 ***********************************************************************************************************************
 * @file  llpcFragColorExport.cpp
 * @brief LLPC source file: contains implementation of class Llpc::FragColorExport.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-frag-color-export"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llpcContext.h"
#include "llpcDebug.h"
#include "llpcFragColorExport.h"
#include "llpcIntrinsDefs.h"

using namespace llvm;

namespace Llpc
{

#define COLOR_FORMAT_UNDEFINED(_format) \
{ \
    _format, \
    COLOR_NUM_FORMAT_FLOAT, \
    COLOR_DATA_FORMAT_INVALID, \
    1, \
    {  8,  0,  0,  0, }, \
    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One }, \
    ChannelMask::X, \
}

// Initializes info table of vertex format map
const ColorFormatInfo FragColorExport::m_colorFormatInfo[] =
{
    // VK_FORMAT_UNDEFINED = 0
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_UNDEFINED),
    // VK_FORMAT_R4G4_UNORM_PACK8 = 1
    {
        VK_FORMAT_R4G4_UNORM_PACK8,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_INVALID,
        2,
        {  4,  4,  0,  0, },
        { ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R4G4B4A4_UNORM_PACK16 = 2
    {
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_4_4_4_4,
        4,
        {  4,  4,  4,  4, },
        { ChannelSwizzle::W, ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B4G4R4A4_UNORM_PACK16 = 3
    {
        VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_4_4_4_4,
        4,
        {  4,  4,  4,  4, },
        { ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W, ChannelSwizzle::X },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R5G6B5_UNORM_PACK16 = 4
    {
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_5_6_5,
        3,
        {  5,  6,  5,  0, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_B5G6R5_UNORM_PACK16 = 5
    {
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_5_6_5,
        3,
        {  5,  6,  5,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_R5G5B5A1_UNORM_PACK16 = 6
    {
        VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_5_5_5_1,
        4,
        {  1,  5,  5,  5, },
        { ChannelSwizzle::W, ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B5G5R5A1_UNORM_PACK16 = 7
    {
        VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_5_5_5_1,
        4,
        {  1,  5,  5,  5, },
        { ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W, ChannelSwizzle::X },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8
    {
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_1_5_5_5,
        4,
        {  5,  5,  5,  1, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8_UNORM = 9
    {
        VK_FORMAT_R8_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_8,
        1,
        {  8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_SNORM = 10
    {
        VK_FORMAT_R8_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_8,
        1,
        {  8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_USCALED = 11
    {
        VK_FORMAT_R8_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_8,
        1,
        {  8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_SSCALED = 12
    {
        VK_FORMAT_R8_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_8,
        1,
        {  8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_UINT = 13
    {
    VK_FORMAT_R8_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8,
        1,
        { 8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_SINT = 14
    {
        VK_FORMAT_R8_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_8,
        1,
        { 8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8_SRGB = 15
    {
        VK_FORMAT_R8_SRGB,
        COLOR_NUM_FORMAT_SRGB,
        COLOR_DATA_FORMAT_8,
        1,
        {  8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R8G8_UNORM = 16
    {
        VK_FORMAT_R8G8_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_SNORM = 17
    {
        VK_FORMAT_R8G8_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_USCALED = 18
    {
        VK_FORMAT_R8G8_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_SSCALED = 19
    {
        VK_FORMAT_R8G8_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_UINT = 20
    {
        VK_FORMAT_R8G8_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_SINT = 21
    {
        VK_FORMAT_R8G8_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8_SRGB = 22
    {
        VK_FORMAT_R8G8_SRGB,
        COLOR_NUM_FORMAT_SRGB,
        COLOR_DATA_FORMAT_8_8,
        2,
        {  8,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R8G8B8_UNORM = 23
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_UNORM),
    // VK_FORMAT_R8G8B8_SNORM = 24
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_SNORM),
    // VK_FORMAT_R8G8B8_USCALED = 25
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_USCALED),
    // VK_FORMAT_R8G8B8_SSCALED = 26
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_SSCALED),
    // VK_FORMAT_R8G8B8_UINT = 27
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_UINT),
    // VK_FORMAT_R8G8B8_SINT = 28
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_SINT),
    // VK_FORMAT_R8G8B8_SRGB = 29
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R8G8B8_SRGB),
    // VK_FORMAT_B8G8R8_UNORM = 30
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_UNORM),
    // VK_FORMAT_B8G8R8_SNORM = 31
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_SNORM),
    // VK_FORMAT_B8G8R8_USCALED = 32
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_USCALED),
    // VK_FORMAT_B8G8R8_SSCALED = 33
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_SSCALED),
    // VK_FORMAT_B8G8R8_UINT = 34
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_UINT),
    // VK_FORMAT_B8G8R8_SINT = 35
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_SINT),
    // VK_FORMAT_B8G8R8_SRGB = 36
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_B8G8R8_SRGB),
    // VK_FORMAT_R8G8B8A8_UNORM = 37
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_SNORM = 38
    {
        VK_FORMAT_R8G8B8A8_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_USCALED = 39
    {
        VK_FORMAT_R8G8B8A8_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_SSCALED = 40
    {
        VK_FORMAT_R8G8B8A8_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_UINT = 41
    {
        VK_FORMAT_R8G8B8A8_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_SINT = 42
    {
        VK_FORMAT_R8G8B8A8_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R8G8B8A8_SRGB = 43
    {
        VK_FORMAT_R8G8B8A8_SRGB,
        COLOR_NUM_FORMAT_SRGB,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_UNORM = 44
    {
        VK_FORMAT_B8G8R8A8_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_SNORM = 45
    {
        VK_FORMAT_B8G8R8A8_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_USCALED = 46
    {
        VK_FORMAT_B8G8R8A8_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_SSCALED = 47
    {
        VK_FORMAT_B8G8R8A8_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_UINT = 48
    {
        VK_FORMAT_B8G8R8A8_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_SINT = 49
    {
        VK_FORMAT_B8G8R8A8_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_B8G8R8A8_SRGB = 50
    {
        VK_FORMAT_B8G8R8A8_SRGB,
        COLOR_NUM_FORMAT_SRGB,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_UNORM_PACK32 = 51
    {
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_SNORM_PACK32 = 52
    {
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_USCALED_PACK32 = 53
    {
        VK_FORMAT_A8B8G8R8_USCALED_PACK32,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_SSCALED_PACK32 = 54
    {
        VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_UINT_PACK32 = 55
    {
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_SINT_PACK32 = 56
    {
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A8B8G8R8_SRGB_PACK32 = 57
    {
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        COLOR_NUM_FORMAT_SRGB,
        COLOR_DATA_FORMAT_8_8_8_8,
        4,
        {  8,  8,  8,  8, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_UNORM_PACK32 = 58
    {
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_SNORM_PACK32 = 59
    {
        VK_FORMAT_A2R10G10B10_SNORM_PACK32,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_USCALED_PACK32 = 60
    {
        VK_FORMAT_A2R10G10B10_USCALED_PACK32,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_SSCALED_PACK32 = 61
    {
        VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_UINT_PACK32 = 62
    {
        VK_FORMAT_A2R10G10B10_UINT_PACK32,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2R10G10B10_SINT_PACK32 = 63
    {
        VK_FORMAT_A2R10G10B10_SINT_PACK32,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64
    {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2B10G10R10_SNORM_PACK32 = 65
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_A2B10G10R10_SNORM_PACK32),
    // VK_FORMAT_A2B10G10R10_USCALED_PACK32 = 66
    {
        VK_FORMAT_A2B10G10R10_USCALED_PACK32,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2B10G10R10_SSCALED_PACK32 = 67
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_A2B10G10R10_SSCALED_PACK32),
    // VK_FORMAT_A2B10G10R10_UINT_PACK32 = 68
    {
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_2_10_10_10,
        4,
        { 10, 10, 10,  2, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_A2B10G10R10_SINT_PACK32 = 69
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_A2B10G10R10_SINT_PACK32),
    // VK_FORMAT_R16_UNORM = 70
    {
        VK_FORMAT_R16_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_SNORM = 71
    {
        VK_FORMAT_R16_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_USCALED = 72
    {
        VK_FORMAT_R16_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_SSCALED = 73
    {
        VK_FORMAT_R16_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_UINT = 74
    {
        VK_FORMAT_R16_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_SINT = 75
    {
        VK_FORMAT_R16_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16_SFLOAT = 76
    {
        VK_FORMAT_R16_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R16G16_UNORM = 77
    {
        VK_FORMAT_R16G16_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_SNORM = 78
    {
        VK_FORMAT_R16G16_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_USCALED = 79
    {
        VK_FORMAT_R16G16_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_SSCALED = 80
    {
        VK_FORMAT_R16G16_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_UINT = 81
    {
        VK_FORMAT_R16G16_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_SINT = 82
    {
        VK_FORMAT_R16G16_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16_SFLOAT = 83
    {
        VK_FORMAT_R16G16_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_16_16,
        2,
        { 16, 16,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R16G16B16_UNORM = 84
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_UNORM),
    // VK_FORMAT_R16G16B16_SNORM = 85
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_SNORM),
    // VK_FORMAT_R16G16B16_USCALED = 86
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_USCALED),
    // VK_FORMAT_R16G16B16_SSCALED = 87
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_SSCALED),
    // VK_FORMAT_R16G16B16_UINT = 88
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_UINT),
    // VK_FORMAT_R16G16B16_SINT = 89
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_SINT),
    // VK_FORMAT_R16G16B16_SFLOAT = 90
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R16G16B16_SFLOAT),
    // VK_FORMAT_R16G16B16A16_UNORM = 91
    {
        VK_FORMAT_R16G16B16A16_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_SNORM = 92
    {
        VK_FORMAT_R16G16B16A16_SNORM,
        COLOR_NUM_FORMAT_SNORM,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_USCALED = 93
    {
        VK_FORMAT_R16G16B16A16_USCALED,
        COLOR_NUM_FORMAT_USCALED,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_SSCALED = 94
    {
        VK_FORMAT_R16G16B16A16_SSCALED,
        COLOR_NUM_FORMAT_SSCALED,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_UINT = 95
    {
        VK_FORMAT_R16G16B16A16_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_SINT = 96
    {
        VK_FORMAT_R16G16B16A16_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R16G16B16A16_SFLOAT = 97
    {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_16_16_16_16,
        4,
        { 16, 16, 16, 16, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R32_UINT = 98
    {
        VK_FORMAT_R32_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_32,
        1,
        { 32,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R32_SINT = 99
    {
        VK_FORMAT_R32_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_32,
        1,
        { 32,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R32_SFLOAT = 100
    {
        VK_FORMAT_R32_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_32,
        1,
        { 32,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_R32G32_UINT = 101
    {
        VK_FORMAT_R32G32_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_32_32,
        2,
        { 32, 32,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R32G32_SINT = 102
    {
        VK_FORMAT_R32G32_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_32_32,
        2,
        { 32, 32,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R32G32_SFLOAT = 103
    {
        VK_FORMAT_R32G32_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_32_32,
        2,
        { 32, 32,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_R32G32B32_UINT = 104
    {
        VK_FORMAT_R32G32B32_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_INVALID,
        3,
        { 32, 32, 32,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_R32G32B32_SINT = 105
    {
        VK_FORMAT_R32G32B32_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_INVALID,
        3,
        { 32, 32, 32,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_R32G32B32_SFLOAT = 106
    {
        VK_FORMAT_R32G32B32_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_INVALID,
        3,
        { 32, 32, 32,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_R32G32B32A32_UINT = 107
    {
        VK_FORMAT_R32G32B32A32_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_32_32_32_32,
        3,
        { 32, 32, 32, 32, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R32G32B32A32_SINT = 108
    {
        VK_FORMAT_R32G32B32A32_SINT,
        COLOR_NUM_FORMAT_SINT,
        COLOR_DATA_FORMAT_32_32_32_32,
        3,
        { 32, 32, 32, 32, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R32G32B32A32_SFLOAT = 109
    {
        VK_FORMAT_R32G32B32A32_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_32_32_32_32,
        3,
        { 32, 32, 32, 32, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z | ChannelMask::W,
    },
    // VK_FORMAT_R64_UINT = 110
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64_UINT),
    // VK_FORMAT_R64_SINT = 111
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64_SINT),
    // VK_FORMAT_R64_SFLOAT = 112
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64_SFLOAT),
    // VK_FORMAT_R64G64_UINT = 113
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64_UINT),
    // VK_FORMAT_R64G64_SINT = 114
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64_SINT),
    // VK_FORMAT_R64G64_SFLOAT = 115
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64_SFLOAT),
    // VK_FORMAT_R64G64B64_UINT = 116
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64_UINT),
    // VK_FORMAT_R64G64B64_SINT = 117
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64_SINT),
    // VK_FORMAT_R64G64B64_SFLOAT = 118
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64_SFLOAT),
    // VK_FORMAT_R64G64B64A64_UINT = 119
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64A64_UINT),
    // VK_FORMAT_R64G64B64A64_SINT = 120
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64A64_SINT),
    // VK_FORMAT_R64G64B64A64_SFLOAT = 121
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_R64G64B64A64_SFLOAT),
    // VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122
    {
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_10_11_11,
        3,
        { 11, 11, 10,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123
    {
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_INVALID,
        4,
        {  9,  9,  9,  5, },
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y | ChannelMask::Z,
    },
    // VK_FORMAT_D16_UNORM = 124
    {
        VK_FORMAT_D16_UNORM,
        COLOR_NUM_FORMAT_UNORM,
        COLOR_DATA_FORMAT_16,
        1,
        { 16,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_X8_D24_UNORM_PACK32 = 125
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_X8_D24_UNORM_PACK32),
    // VK_FORMAT_D32_SFLOAT = 126
    {
        VK_FORMAT_D32_SFLOAT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_32,
        1,
        { 32,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_S8_UINT = 127
    {
        VK_FORMAT_S8_UINT,
        COLOR_NUM_FORMAT_UINT,
        COLOR_DATA_FORMAT_8,
        1,
        { 8,  0,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X,
    },
    // VK_FORMAT_D16_UNORM_S8_UINT = 128
    {
        VK_FORMAT_D16_UNORM_S8_UINT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_INVALID,
        2,
        { 16,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_D24_UNORM_S8_UINT = 129
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_D24_UNORM_S8_UINT),
    // VK_FORMAT_D32_SFLOAT_S8_UINT = 130
    {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        COLOR_NUM_FORMAT_FLOAT,
        COLOR_DATA_FORMAT_INVALID,
        2,
        { 32,  8,  0,  0, },
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
        ChannelMask::X | ChannelMask::Y,
    },
    // VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC1_RGB_UNORM_BLOCK),
    // VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC1_RGB_SRGB_BLOCK),
    // VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC1_RGBA_UNORM_BLOCK),
    // VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC1_RGBA_SRGB_BLOCK),
    // VK_FORMAT_BC2_UNORM_BLOCK = 135
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC2_UNORM_BLOCK),
    // VK_FORMAT_BC2_SRGB_BLOCK = 136
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC2_SRGB_BLOCK),
    // VK_FORMAT_BC3_UNORM_BLOCK = 137
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC3_UNORM_BLOCK),
    // VK_FORMAT_BC3_SRGB_BLOCK = 138
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC3_SRGB_BLOCK),
    // VK_FORMAT_BC4_UNORM_BLOCK = 139
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC4_UNORM_BLOCK),
    // VK_FORMAT_BC4_SNORM_BLOCK = 140
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC4_SNORM_BLOCK),
    // VK_FORMAT_BC5_UNORM_BLOCK = 141
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC5_UNORM_BLOCK),
    // VK_FORMAT_BC5_SNORM_BLOCK = 142
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC5_SNORM_BLOCK),
    // VK_FORMAT_BC6H_UFLOAT_BLOCK = 143
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC6H_UFLOAT_BLOCK),
    // VK_FORMAT_BC6H_SFLOAT_BLOCK = 144
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC6H_SFLOAT_BLOCK),
    // VK_FORMAT_BC7_UNORM_BLOCK = 145
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC7_UNORM_BLOCK),
    // VK_FORMAT_BC7_SRGB_BLOCK = 146
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_BC7_SRGB_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK),
    // VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK),
    // VK_FORMAT_EAC_R11_UNORM_BLOCK = 153
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_EAC_R11_UNORM_BLOCK),
    // VK_FORMAT_EAC_R11_SNORM_BLOCK = 154
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_EAC_R11_SNORM_BLOCK),
    // VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_EAC_R11G11_UNORM_BLOCK),
    // VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_EAC_R11G11_SNORM_BLOCK),
    // VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_4x4_UNORM_BLOCK),
    // VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_4x4_SRGB_BLOCK),
    // vK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_5x4_UNORM_BLOCK),
    // VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_5x4_SRGB_BLOCK),
    // VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_5x5_UNORM_BLOCK),
    // VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_5x5_SRGB_BLOCK),
    // VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_6x5_UNORM_BLOCK),
    // VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_6x5_SRGB_BLOCK),
    // VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_6x6_UNORM_BLOCK),
    // VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_6x6_SRGB_BLOCK),
    // VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x5_UNORM_BLOCK),
    // VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x5_SRGB_BLOCK),
    // VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x6_UNORM_BLOCK),
    // VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x6_SRGB_BLOCK),
    // VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x8_UNORM_BLOCK),
    // VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_8x8_SRGB_BLOCK),
    // VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x5_UNORM_BLOCK),
    // VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x5_SRGB_BLOCK),
    // VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x6_UNORM_BLOCK),
    // VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x6_SRGB_BLOCK),
    // VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x8_UNORM_BLOCK),
    // VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x8_SRGB_BLOCK),
    // VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x10_UNORM_BLOCK),
    // VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_10x10_SRGB_BLOCK),
    // VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_12x10_UNORM_BLOCK),
    // VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_12x10_SRGB_BLOCK),
    // VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_12x12_UNORM_BLOCK),
    // VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184
    COLOR_FORMAT_UNDEFINED(VK_FORMAT_ASTC_12x12_SRGB_BLOCK),
};

// =====================================================================================================================
FragColorExport::FragColorExport(
    Module* pModule) // [in] LLVM module
    :
    m_pModule(pModule),
    m_pContext(static_cast<Context*>(&pModule->getContext())),
    pPipelineInfo(static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo()))
{
    LLPC_ASSERT(GetShaderStageFromModule(m_pModule) == ShaderStageFragment); // Must be fragment shader
}

// =====================================================================================================================
// Executes fragment color export operations based on the specified output type and its location.
Value* FragColorExport::Run(
    Value*       pOutput,       // [in] Fragment color output
    uint32_t     location,      // Location of fragment color output
    Instruction* pInsertPos)    // [in] Where to insert fragment color export instructions
{
    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment);

    Type* pOutputTy = pOutput->getType();

    auto expFmt = ComputeExportFormat(pOutputTy, location); // TODO: Support dual source blend.

    pResUsage->inOutUsage.fs.expFmts[location] = expFmt;
    if (expFmt == EXP_FORMAT_ZERO)
    {
        // Clear channel mask if shader export format is ZERO
        pResUsage->inOutUsage.fs.cbShaderMask &= ~(0xF << (4 * location));
    }

    const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();

    auto pCompTy = pOutputTy->isVectorTy() ? pOutputTy->getVectorElementType() : pOutputTy;
    uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() : 1;

    Value* comps[4] = { nullptr };
    if (compCount == 1)
    {
        comps[0] = pOutput;
    }
    else
    {
        for (uint32_t i = 0; i < compCount; ++i)
        {
            comps[i] = ExtractElementInst::Create(pOutput,
                                                  ConstantInt::get(m_pContext->Int32Ty(), i),
                                                  "",
                                                  pInsertPos);
        }
    }

    bool comprExp = false;
    bool needPack = false;

    std::vector<Value*> args;

    const auto pUndefFloat     = UndefValue::get(m_pContext->FloatTy());
    const auto pUndefFloat16   = UndefValue::get(m_pContext->Float16Ty());
    const auto pUndefFloat16x2 = UndefValue::get(m_pContext->Float16x2Ty());

    switch (expFmt)
    {
    case EXP_FORMAT_ZERO:
        {
            break;
        }
    case EXP_FORMAT_32_R:
        {
            compCount = 1;
            comps[0] = ConvertToFloat(comps[0], pInsertPos);
            comps[1] = pUndefFloat;
            comps[2] = pUndefFloat;
            comps[3] = pUndefFloat;
            break;
        }
    case EXP_FORMAT_32_GR:
        {
            if (compCount >= 2)
            {
                compCount = 2;
                comps[0] = ConvertToFloat(comps[0], pInsertPos);
                comps[1] = ConvertToFloat(comps[1], pInsertPos);
                comps[2] = pUndefFloat;
                comps[3] = pUndefFloat;
            }
            else
            {
                compCount = 1;
                comps[0] = ConvertToFloat(comps[0], pInsertPos);
                comps[1] = pUndefFloat;
                comps[2] = pUndefFloat;
                comps[3] = pUndefFloat;
            }
            break;
        }
    case EXP_FORMAT_32_AR:
        {
            if (compCount == 4)
            {
                compCount = 2;
                comps[0] = ConvertToFloat(comps[0], pInsertPos);
                comps[1] = ConvertToFloat(comps[3], pInsertPos);
                comps[2] = pUndefFloat;
                comps[3] = pUndefFloat;
            }
            else
            {
                compCount = 1;
                comps[0] = ConvertToFloat(comps[0], pInsertPos);
                comps[1] = pUndefFloat;
                comps[2] = pUndefFloat;
                comps[3] = pUndefFloat;
            }
            break;
        }
    case EXP_FORMAT_32_ABGR:
       {
            for (uint32_t i = 0; i < compCount; ++i)
            {
                comps[i] = ConvertToFloat(comps[i], pInsertPos);
            }

            for (uint32_t i = compCount; i < 4; ++i)
            {
                comps[i] = pUndefFloat;
            }
            break;
        }
    case EXP_FORMAT_FP16_ABGR:
        {
            comprExp = true;

            if (bitWidth == 16)
            {
                needPack = true;

                if (pCompTy->isIntegerTy())
                {
                    // Cast i16 to float16
                    for (uint32_t i = 0; i < compCount; ++i)
                    {
                        // %comp = bitcast i16 %comp to half
                        comps[i] = new BitCastInst(comps[i], m_pContext->Float16Ty(), "", pInsertPos);
                    }
                }

                for (uint32_t i = compCount; i < 4; ++i)
                {
                    comps[i] = pUndefFloat16;
                }
            }
            else
            {
                if (pCompTy->isIntegerTy())
                {
                    // Cast i32 to float
                    for (uint32_t i = 0; i < compCount; ++i)
                    {
                        // %comp = bitcast i32 %comp to float
                        comps[i] = new BitCastInst(comps[i], m_pContext->FloatTy(), "", pInsertPos);
                    }
                }

                for (uint32_t i = compCount; i < 4; ++i)
                {
                    comps[i] = pUndefFloat;
                }

                std::vector<Attribute::AttrKind> attribs;
                attribs.push_back(Attribute::ReadNone);

                // Do packing
                args.clear();
                args.push_back(comps[0]);
                args.push_back(comps[1]);
                comps[0] = EmitCall(m_pModule,
                                    "llvm.amdgcn.cvt.pkrtz",
                                    m_pContext->Float16x2Ty(),
                                    args,
                                    attribs,
                                    pInsertPos);

                if (compCount > 2)
                {
                    args.clear();
                    args.push_back(comps[2]);
                    args.push_back(comps[3]);
                    comps[1] = EmitCall(m_pModule,
                                        "llvm.amdgcn.cvt.pkrtz",
                                        m_pContext->Float16x2Ty(),
                                        args,
                                        attribs,
                                        pInsertPos);
                }
                else
                {
                    comps[1] = pUndefFloat16x2;
                }
            }

            break;
        }
    case EXP_FORMAT_UNORM16_ABGR:
    case EXP_FORMAT_SNORM16_ABGR:
        {
            comprExp = true;
            needPack = true;

            for (uint32_t i = 0; i < compCount; ++i)
            {
                // Convert the components to float value if necessary
                comps[i] = ConvertToFloat(comps[i], pInsertPos);

                if (expFmt == EXP_FORMAT_UNORM16_ABGR)
                {
                    // int(round(clamp(c, 0.0, 1.0) * 65535.0))

                    // %comp = @llvm.amdgcn.fmed3.f32(float %comp, float 0.0, float 1.0)
                    args.clear();
                    args.push_back(comps[i]);
                    args.push_back(ConstantFP::get(m_pContext->FloatTy(), 0.0));
                    args.push_back(ConstantFP::get(m_pContext->FloatTy(), 1.0));
                    comps[i] = EmitCall(m_pModule,
                                        "llvm.amdgcn.fmed3.f32",
                                        m_pContext->FloatTy(),
                                        args,
                                        NoAttrib,
                                        pInsertPos);

                    // %comp = fmul float %comp, 65535.0
                    comps[i] = BinaryOperator::Create(BinaryOperator::FMul,
                                                      comps[i],
                                                      ConstantFP::get(m_pContext->FloatTy(), 65535.0),
                                                      "",
                                                      pInsertPos);

                    // %comp = fadd float %comp, 0.5
                    comps[i] = BinaryOperator::Create(BinaryOperator::FAdd,
                                                      comps[i],
                                                      ConstantFP::get(m_pContext->FloatTy(), 0.5),
                                                      "",
                                                      pInsertPos);

                    // %comp = fptoui float %comp to i32
                    comps[i] = new FPToUIInst(comps[i], m_pContext->Int32Ty(), "", pInsertPos);

                    // %comp = trunc i32 %comp to i16
                    comps[i] = new TruncInst(comps[i], m_pContext->Int16Ty(), "", pInsertPos);

                    // %comp = bitcast i16 %comp to half
                    comps[i] = new BitCastInst(comps[i], m_pContext->Float16Ty(), "", pInsertPos);
                }
                else
                {
                    LLPC_ASSERT(expFmt == EXP_FORMAT_SNORM16_ABGR);

                    // int(round(clamp(c, -1.0, 1.0) * 32767.0))

                    // %comp = @llvm.amdgcn.fmed3.f32(float %comp, float -1.0, float 1.0)
                    args.clear();
                    args.push_back(comps[i]);
                    args.push_back(ConstantFP::get(m_pContext->FloatTy(), -1.0));
                    args.push_back(ConstantFP::get(m_pContext->FloatTy(), 1.0));
                    comps[i] = EmitCall(m_pModule,
                                        "llvm.amdgcn.fmed3.f32",
                                        m_pContext->FloatTy(),
                                        args,
                                        NoAttrib,
                                        pInsertPos);

                    // %comp = fmul float %comp, 32767.0
                    comps[i] = BinaryOperator::Create(BinaryOperator::FMul,
                                                      comps[i],
                                                      ConstantFP::get(m_pContext->FloatTy(), 32767.0),
                                                      "",
                                                      pInsertPos);

                    // %cond = fcmp oge float %36, 0.0
                    auto pCond = new FCmpInst(pInsertPos,
                                              FCmpInst::FCMP_OGE,
                                              comps[i],
                                              ConstantFP::get(m_pContext->FloatTy(), 0.0),
                                              "");

                    // %select = select i1 %cond, float 0.5, float -0.5
                    auto pSelect = SelectInst::Create(pCond,
                                                      ConstantFP::get(m_pContext->FloatTy(), 0.5),
                                                      ConstantFP::get(m_pContext->FloatTy(), -0.5),
                                                      "",
                                                      pInsertPos);

                    // %comp = fadd float %comp, %select
                    comps[i] = BinaryOperator::Create(BinaryOperator::FAdd,
                                                      comps[i],
                                                      pSelect,
                                                      "",
                                                      pInsertPos);

                    // %comp = fptosi float %comp to i32
                    comps[i] = new FPToSIInst(comps[i], m_pContext->Int32Ty(), "", pInsertPos);

                    // %comp = trunc i32 %comp to i16
                    comps[i] = new TruncInst(comps[i], m_pContext->Int16Ty(), "", pInsertPos);

                    // %comp = bitcast i16 %comp to half
                    comps[i] = new BitCastInst(comps[i], m_pContext->Float16Ty(), "", pInsertPos);
                }
            }

            for (uint32_t i = compCount; i < 4; ++i)
            {
                comps[i] = pUndefFloat16;
            }

            break;
        }
    case EXP_FORMAT_UINT16_ABGR:
    case EXP_FORMAT_SINT16_ABGR:
        {
            comprExp = true;
            needPack = true;

            for (uint32_t i = 0; i < compCount; ++i)
            {
                // Convert the components to int value if necessary
                comps[i] = ConvertToInt(comps[i], pInsertPos);

                if (expFmt == EXP_FORMAT_UINT16_ABGR)
                {
                    // clamp(c, 0, 65535)

                    // %cond = icmp ult i32 %comp, 65535
                    auto pCond = new ICmpInst(pInsertPos,
                                              ICmpInst::ICMP_ULT,
                                              comps[i],
                                              ConstantInt::get(m_pContext->Int32Ty(), 65535),
                                              "");

                    // %comp = select i1 %cond, i32 %comp, i32 65535
                    comps[i] = SelectInst::Create(pCond,
                                                  comps[i],
                                                  ConstantInt::get(m_pContext->Int32Ty(), 65535),
                                                  "",
                                                  pInsertPos);

                    // %comp = trunc i32 %comp to i16
                    comps[i] = new TruncInst(comps[i], m_pContext->Int16Ty(), "", pInsertPos);

                    // %comp = bitcast i16 %comp to half
                    comps[i] = new BitCastInst(comps[i], m_pContext->Float16Ty(), "", pInsertPos);
                }
                else
                {
                    LLPC_ASSERT(expFmt == EXP_FORMAT_SINT16_ABGR);

                    // clamp(c, -32768, 32767)

                    // %cond = icmp slt i32 %comp, 32767
                    auto pCond = new ICmpInst(pInsertPos,
                                              ICmpInst::ICMP_SLT,
                                              comps[i],
                                              ConstantInt::get(m_pContext->Int32Ty(), 32767),
                                              "");

                    // %comp = select i1 %cond, i32 %comp, i32 32767
                    comps[i] = SelectInst::Create(pCond,
                                                  comps[i],
                                                  ConstantInt::get(m_pContext->Int32Ty(), 32767),
                                                  "",
                                                  pInsertPos);

                    // %cond = icmp sgt i32 %comp, -32768
                    pCond = new ICmpInst(pInsertPos,
                                              ICmpInst::ICMP_SGT,
                                              comps[i],
                                              ConstantInt::get(m_pContext->Int32Ty(), -32768),
                                              "");

                    // %comp = select i1 %cond, i32 %comp, i32 -32768
                    comps[i] = SelectInst::Create(pCond,
                                                  comps[i],
                                                  ConstantInt::get(m_pContext->Int32Ty(), -32768),
                                                  "",
                                                  pInsertPos);

                    // %comp = trunc i32 %comp to i16
                    comps[i] = new TruncInst(comps[i], m_pContext->Int16Ty(), "", pInsertPos);

                    // %comp = bitcast i16 %comp to half
                    comps[i] = new BitCastInst(comps[i], m_pContext->Float16Ty(), "", pInsertPos);
                }
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    Value* pExport = nullptr;

    if (expFmt == EXP_FORMAT_ZERO)
    {
        // Do nothing
    }
    else if (comprExp)
    {
        // 16-bit export (compressed)
        if (needPack)
        {
            // Do packing

            // %comp[0] = insertelement <2 x half> undef, half %comp[0], i32 0
            comps[0] = InsertElementInst::Create(pUndefFloat16x2,
                                                 comps[0],
                                                 ConstantInt::get(m_pContext->Int32Ty(), 0),
                                                 "",
                                                 pInsertPos);

            // %comp[0] = insertelement <2 x half> %comp[0], half %comp[1], i32 1
            comps[0] = InsertElementInst::Create(comps[0],
                                                 comps[1],
                                                 ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                 "",
                                                 pInsertPos);

            if (compCount > 2)
            {
                // %comp[1] = insertelement <2 x half> undef, half %comp[2], i32 0
                comps[1] = InsertElementInst::Create(pUndefFloat16x2,
                                                     comps[2],
                                                     ConstantInt::get(m_pContext->Int32Ty(), 0),
                                                     "",
                                                     pInsertPos);

                // %comp[1] = insertelement <2 x half> %comp[1], half %comp[3], i32 1
                comps[1] = InsertElementInst::Create(comps[1],
                                                     comps[3],
                                                     ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                     "",
                                                     pInsertPos);
            }
            else
            {
                comps[1] = pUndefFloat16x2;
            }
        }

        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_MRT_0 + location)); // tgt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), (compCount > 2) ? 0xF : 0x3)); // en
        args.push_back(comps[0]);                                                             // src0
        args.push_back(comps[1]);                                                             // src1
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                        // done
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                         // vm

        pExport = EmitCall(m_pModule, "llvm.amdgcn.exp.compr.v2f16", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }
    else
    {
        // 32-bit export
        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_MRT_0 + location)); // tgt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), (1 << compCount) - 1));        // en
        args.push_back(comps[0]);                                                             // src0
        args.push_back(comps[1]);                                                             // src1
        args.push_back(comps[2]);                                                             // src2
        args.push_back(comps[3]);                                                             // src3
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                        // done
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                         // vm

        pExport = EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }

    return pExport;
}

// =====================================================================================================================
// Determines the shader export format for a particular fragment color output. Value should be used to do programming
// for SPI_SHADER_COL_FORMAT.
ExportFormat FragColorExport::ComputeExportFormat(
    Type*    pOutputTy,  // [in] Type of fragment data output
    uint32_t location    // Location of fragment data output
    ) const
{
    const auto pCbState = &pPipelineInfo->cbState;
    const auto pTarget = &pCbState->target[location];

    const VkFormat format = pTarget->format;

    const bool blendEnabled = pTarget->blendEnable;
    const bool blendSrcAlphaToColor = pTarget->blendSrcAlphaToColor;

    const bool shaderExportsAlpha = (pOutputTy->isVectorTy() && (pOutputTy->getVectorNumElements() == 4));

    // NOTE: Alpha-to-coverage only cares at the output from target #0.
    const bool enableAlphaToCoverage = (pCbState->alphaToCoverageEnable && (location == 0));

    const bool isUnorm = IsUnorm(format);
    const bool isSnorm = IsSnorm(format);
    const bool isFloat = IsFloat(format);
    const bool isUint  = IsUint(format);
    const bool isSint  = IsSint(format);
    const bool isSrgb  = IsSrgb(format);

    const uint32_t maxCompBitCount = GetMaxComponentBitCount(format);

    const bool hasAlpha = HasAlpha(pTarget->format);
    const bool alphaExport = (shaderExportsAlpha &&
                              (hasAlpha || pTarget->blendSrcAlphaToColor || enableAlphaToCoverage));

    const CompSetting compSetting = ComputeCompSetting(format);

    // Start by assuming EXP_FORMAT_ZERO (no exports)
    ExportFormat expFmt = EXP_FORMAT_ZERO;

    GfxIpVersion gfxIp = m_pContext->GetGfxIpVersion();

    bool waCbNoLt16BitIntClamp = false;
    if ((gfxIp.major == 6) || (gfxIp.major == 7))
    {
        // NOTE: Gfx6 and part of gfx7 hardware, the CB does not properly clamp its input if the shader
        // export format is "UINT16" or "SINT16" and the CB format is less than 16 bits per channel.
        waCbNoLt16BitIntClamp = false;
    }

    bool gfx8RbPlusEnable = false;
    if ((gfxIp.major == 8) || (gfxIp.minor == 1))
    {
        gfx8RbPlusEnable = true;
    }

    if (format == VK_FORMAT_UNDEFINED)
    {
        expFmt = EXP_FORMAT_ZERO;
    }
    else if ((compSetting == CompSetting::OneCompRed) &&
        (alphaExport == false)                   &&
        (isSrgb == false)                        &&
        ((gfx8RbPlusEnable == false) || (maxCompBitCount == 32)))
    {
        // NOTE: When Rb+ is enabled, "R8 UNORM" and "R16 UNORM" shouldn't use "EXP_FORMAT_32_R", instead
        // "EXP_FORMAT_FP16_ABGR" and "EXP_FORMAT_UNORM16_ABGR" should be used for 2X exporting performance.
        expFmt = EXP_FORMAT_32_R;
    }
    else if (((isUnorm || isSnorm) && (maxCompBitCount <= 10)) ||
             (isFloat && (maxCompBitCount <= 16)) ||
             (isSrgb && (maxCompBitCount == 8)))
    {
        expFmt = EXP_FORMAT_FP16_ABGR;
    }
    else if (isSint &&
             ((maxCompBitCount == 16) || ((waCbNoLt16BitIntClamp == false) && (maxCompBitCount < 16))) &&
             (enableAlphaToCoverage == false))
    {
        // NOTE: On some hardware, the CB will not properly clamp its input if the shader export format is "UINT16"
        // "SINT16" and the CB format is less than 16 bits per channel. On such hardware, the workaround is picking
        // an appropriate 32-bit export format. If this workaround isn't necessary, then we can choose this higher
        // performance 16-bit export format in this case.
        expFmt = EXP_FORMAT_SINT16_ABGR;
    }
    else if (isSnorm && (maxCompBitCount == 16) && (blendEnabled == false))
    {
        expFmt = EXP_FORMAT_SNORM16_ABGR;
    }
    else if (isUint &&
             ((maxCompBitCount == 16) || ((waCbNoLt16BitIntClamp == false) && (maxCompBitCount < 16))) &&
             (enableAlphaToCoverage == false))
    {
        // NOTE: On some hardware, the CB will not properly clamp its input if the shader export format is "UINT16"
        // "SINT16" and the CB format is less than 16 bits per channel. On such hardware, the workaround is picking
        // an appropriate 32-bit export format. If this workaround isn't necessary, then we can choose this higher
        // performance 16-bit export format in this case.
        expFmt = EXP_FORMAT_UINT16_ABGR;
    }
    else if (isUnorm && (maxCompBitCount == 16) && (blendEnabled == false))
    {
        expFmt = EXP_FORMAT_UNORM16_ABGR;
    }
    else if (((isUint || isSint) ||
              (isFloat && (maxCompBitCount > 16)) ||
              ((isUnorm || isSnorm) && (maxCompBitCount == 16)))  &&
             ((compSetting == CompSetting::OneCompRed) ||
              (compSetting == CompSetting::OneCompAlpha) ||
              (compSetting == CompSetting::TwoCompAlphaRed)))
    {
        expFmt = EXP_FORMAT_32_AR;
    }
    else if (((isUint || isSint) ||
              (isFloat && (maxCompBitCount > 16)) ||
              ((isUnorm || isSnorm) && (maxCompBitCount == 16)))  &&
             (compSetting == CompSetting::TwoCompGreenRed) && (alphaExport == false))
    {
        expFmt = EXP_FORMAT_32_GR;
    }
    else if (((isUnorm || isSnorm) && (maxCompBitCount == 16)) ||
             (isUint || isSint) ||
             (isFloat && (maxCompBitCount >  16)))
    {
        expFmt = EXP_FORMAT_32_ABGR;
    }

    return expFmt;
}

// =====================================================================================================================
// This is the helper function for the algorithm to determine the shader export format.
CompSetting FragColorExport::ComputeCompSetting(
    VkFormat format // Color attachment color
    ) const
{
    CompSetting compSetting = CompSetting::Invalid;

    const ColorSwap colorSwap = ComputeColorSwap(format);
    const uint32_t dfmt = GetColorFormatInfo(format)->dfmt;

    switch (dfmt)
    {
    case COLOR_DATA_FORMAT_8:
    case COLOR_DATA_FORMAT_16:
    case COLOR_DATA_FORMAT_32:
        if (colorSwap == COLOR_SWAP_STD)
        {
            compSetting = CompSetting::OneCompRed;
        }
        else if (colorSwap == COLOR_SWAP_ALT_REV)
        {
            compSetting = CompSetting::OneCompAlpha;
        }
        break;
    case COLOR_DATA_FORMAT_8_8:
    case COLOR_DATA_FORMAT_16_16:
    case COLOR_DATA_FORMAT_32_32:
        if ((colorSwap == COLOR_SWAP_STD) || (colorSwap == COLOR_SWAP_STD_REV))
        {
            compSetting = CompSetting::TwoCompGreenRed;
        }
        else if ((colorSwap == COLOR_SWAP_ALT) || (colorSwap == COLOR_SWAP_ALT_REV))
        {
            compSetting = CompSetting::TwoCompAlphaRed;
        }
        break;
    default:
        compSetting = CompSetting::Invalid;
        break;
    }

    return compSetting;
}

// =====================================================================================================================
// Determines the CB component swap mode according to color attachment format.
ColorSwap FragColorExport::ComputeColorSwap(
    VkFormat format // Color attachment format
    ) const
{
    ColorSwap colorSwap = COLOR_SWAP_STD;

    const uint32_t numChannels = GetColorFormatInfo(format)->numChannels;
    const auto& swizzle = GetColorFormatInfo(format)->channelSwizzle;

    if (numChannels == 1)
    {
        if (swizzle.r == ChannelSwizzle::X)
        {
            colorSwap = COLOR_SWAP_STD;
        }
        else if (swizzle.a == ChannelSwizzle::X)
        {
            colorSwap = COLOR_SWAP_ALT_REV;
        }
        else
        {
            LLPC_NEVER_CALLED();
        }
    }
    else if (numChannels == 2)
    {
        if ((swizzle.r == ChannelSwizzle::X) && (swizzle.g == ChannelSwizzle::Y))
        {
            colorSwap = COLOR_SWAP_STD;
        }
        else if ((swizzle.r == ChannelSwizzle::X) && (swizzle.a == ChannelSwizzle::Y))
        {
            colorSwap = COLOR_SWAP_ALT;
        }
        else if ((swizzle.g == ChannelSwizzle::X) && (swizzle.r == ChannelSwizzle::Y))
        {
            colorSwap = COLOR_SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) && (swizzle.r == ChannelSwizzle::Y))
        {
            colorSwap = COLOR_SWAP_ALT_REV;
        }
        else
        {
            LLPC_NEVER_CALLED();
        }
    }
    else if (numChannels == 3)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z))
        {
            colorSwap = COLOR_SWAP_STD;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.a == ChannelSwizzle::Z))
        {
            colorSwap = COLOR_SWAP_ALT;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            colorSwap = COLOR_SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            colorSwap = COLOR_SWAP_ALT_REV;
        }
        else
        {
            LLPC_NEVER_CALLED();
        }
    }
    else if (numChannels == 4)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z) &&
            ((swizzle.a == ChannelSwizzle::W) || (swizzle.a == ChannelSwizzle::One)))
        {
            colorSwap = COLOR_SWAP_STD;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z) &&
                 ((swizzle.a == ChannelSwizzle::W) || (swizzle.a == ChannelSwizzle::One)))
        {
            colorSwap = COLOR_SWAP_ALT;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z) &&
                 (swizzle.r == ChannelSwizzle::W))
        {
            colorSwap = COLOR_SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z) &&
                 (swizzle.b == ChannelSwizzle::W))
        {
            colorSwap = COLOR_SWAP_ALT_REV;
        }
        else
        {
            LLPC_NEVER_CALLED();
        }
    }
    else
    {
        LLPC_NEVER_CALLED();
    }

    return colorSwap;
}

// =====================================================================================================================
// Gets info from table according to color attachment format.
const ColorFormatInfo* FragColorExport::GetColorFormatInfo(
    VkFormat format) // Color attachment format
{
    LLPC_ASSERT(format < VK_FORMAT_RANGE_SIZE);

    const ColorFormatInfo* pFormatInfo = &m_colorFormatInfo[format];
    LLPC_ASSERT(pFormatInfo->format == format);

    return pFormatInfo;
}

// =====================================================================================================================
// Checks whether the alpha channel is present in the specified color attachment format.
bool FragColorExport::HasAlpha(
    VkFormat format // Color attachment foramt
    ) const
{
    const auto mask = GetColorFormatInfo(format)->channelMask;
    const auto& swizzle = GetColorFormatInfo(format)->channelSwizzle;

    return (((mask & ChannelMask::W) != 0) ||
            ((swizzle.a != ChannelSwizzle::Zero) && (swizzle.a != ChannelSwizzle::One)));
}

// =====================================================================================================================
// Gets the maximum bit-count of any component in specified color attachment format.
uint32_t FragColorExport::GetMaxComponentBitCount(
    VkFormat format // Color attachment foramt
    ) const
{
    auto& bitCount = GetColorFormatInfo(format)->bitCount;
    return std::max(std::max(bitCount[0], bitCount[1]), std::max(bitCount[2], bitCount[3]));
}

// =====================================================================================================================
// Converts an output component value to its floating-point representation. This function is a "helper" in computing
// the export value based on shader export format.
Value* FragColorExport::ConvertToFloat(
    Value*       pValue,    // [in] Output component value
    Instruction* pInsertPos // [in] Where to insert conversion instructions
    ) const
{
    Type* pValueTy = pValue->getType();
    LLPC_ASSERT(pValueTy->isFloatingPointTy() || pValueTy->isIntegerTy()); // Should be floating-point/integer scalar

    const uint32_t bitWidth = pValueTy->getScalarSizeInBits();
    if (bitWidth == 16)
    {
        if (pValueTy->isFloatingPointTy())
        {
            // %value = bicast half %value to i16
            pValue = new BitCastInst(pValue, m_pContext->Int16Ty(), "", pInsertPos);
        }

        // %value = @llvm.convert.from.fp16.f32(i16 %value)
        std::vector<Value*> args;
        args.push_back(pValue);
        pValue = EmitCall(m_pModule, "llvm.convert.from.fp16.f32", m_pContext->FloatTy(), args, NoAttrib, pInsertPos);
    }
    else
    {
        LLPC_ASSERT(bitWidth == 32); // The valid bit width is 16 or 32
        if (pValueTy->isIntegerTy())
        {
            // %value = bitcast i32 %value to float
            pValue = new BitCastInst(pValue, m_pContext->FloatTy(), "", pInsertPos);
        }
    }

    return pValue;
}

// =====================================================================================================================
// Converts an output component value to its integer representation. This function is a "helper" in computing the
// export value based on shader export format.
Value* FragColorExport::ConvertToInt(
    Value*       pValue,    // [in] Output component value
    Instruction* pInsertPos // [in] Where to insert conversion instructions
    ) const
{
    Type* pValueTy = pValue->getType();
    LLPC_ASSERT(pValueTy->isFloatingPointTy() || pValueTy->isIntegerTy()); // Should be floating-point/integer scalar

    const uint32_t bitWidth = pValueTy->getScalarSizeInBits();
    if (bitWidth == 16)
    {
        if (pValueTy->isFloatingPointTy())
        {
            // %value = bicast half %value to i16
            pValue = new BitCastInst(pValue, m_pContext->Int16Ty(), "", pInsertPos);
        }

        // %value = @llvm.convert.from.fp16.f32(i16 %value)
        std::vector<Value*> args;
        args.push_back(pValue);
        pValue = EmitCall(m_pModule, "llvm.convert.from.fp16.f32", m_pContext->FloatTy(), args, NoAttrib, pInsertPos);

        // %value = bitcast float %value to i32
        pValue = new BitCastInst(pValue, m_pContext->Int32Ty(), "", pInsertPos);
    }
    else
    {
        LLPC_ASSERT(bitWidth == 32); // The valid bit width is 16 or 32
        if (pValueTy->isFloatingPointTy())
        {
            // %value = bitcast float %value to i32
            pValue = new BitCastInst(pValue, m_pContext->Int32Ty(), "", pInsertPos);
        }
    }

    return pValue;
}

} // Llpc
