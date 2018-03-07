/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcFragColorExport.h
 * @brief LLPC header file: contains declaration of class Llpc::FragColorExport.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcInternal.h"
#include "llpcIntrinsDefs.h"

namespace Llpc
{

class Context;

// Enumerates the source selection of each color channel in a color attachment format.
enum class ChannelSwizzle : uint8_t
{
    Zero = 0,       // Always 0 (ignore source)
    One,            // Always 1 (ignore source)
    X,              // X channel from the source
    Y,              // Y channel from the source
    Z,              // Z channel from the source
    W,              // W channel from the source
};

// Enumerates the presence of each color channel in a color attachment format.
enum ChannelMask : uint8_t
{
    X = 1,          // X channel is present
    Y = 2,          // Y channel is present
    Z = 4,          // Z channel is present
    W = 8,          // W channel is present
};

// Enumerates component setting of color format. This is a "helper" enum used in the CB's algorithm for deriving
// an ideal shader export format.
enum class CompSetting : uint32_t
{
    Invalid,            // Invalid
    OneCompRed,         // Red
    OneCompAlpha,       // Alpha
    TwoCompAlphaRed,    // Alpha, red
    TwoCompGreenRed     // Green, red
};

// Represents fragment color format info corresponding to color attachment format (VkFormat).
struct ColorFormatInfo
{
    VkFormat        format;         // Color attachment format
    ColorNumFormat  nfmt;           // Numeric format of fragment color
    ColorDataFormat dfmt;           // Data format of fragment color
    uint32_t        numChannels;    // Valid number of channels
    uint32_t        bitCount[4];    // Number of bits for each channel

    union
    {
        struct
        {
            ChannelSwizzle r;           // Red component swizzle
            ChannelSwizzle g;           // Green component swizzle
            ChannelSwizzle b;           // Blue component swizzle
            ChannelSwizzle a;           // Alpha component swizzle
        };
        ChannelSwizzle     rgba[4];     // All four swizzles packed into one array
    } channelSwizzle;

    uint8_t    channelMask;    // Mask indicating which channel is valid
};

// =====================================================================================================================
// Represents the manager of fragment color export operations.
class FragColorExport
{
public:
    FragColorExport(llvm::Module* pModule);

    llvm::Value* Run(llvm::Value* pOutput, uint32_t location, llvm::Instruction* pInsertPos);

private:
    LLPC_DISALLOW_DEFAULT_CTOR(FragColorExport);
    LLPC_DISALLOW_COPY_AND_ASSIGN(FragColorExport);

    ExportFormat ComputeExportFormat(llvm::Type* pOutputTy, uint32_t location) const;
    CompSetting ComputeCompSetting(VkFormat format) const;
    ColorSwap ComputeColorSwap(VkFormat format) const;

    static const ColorFormatInfo* GetColorFormatInfo(VkFormat format);

    // Checks whether numeric format of the specified color attachment format is as expected
    bool IsUnorm(VkFormat format) const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_UNORM); }
    bool IsSnorm(VkFormat format) const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_SNORM); }
    bool IsFloat(VkFormat format) const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_FLOAT); }
    bool IsUint(VkFormat format)  const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_UINT);  }
    bool IsSint(VkFormat format)  const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_SINT);  }
    bool IsSrgb(VkFormat format)  const { return (GetColorFormatInfo(format)->nfmt == COLOR_NUM_FORMAT_SRGB);  }

    bool HasAlpha(VkFormat format) const;

    uint32_t GetMaxComponentBitCount(VkFormat format) const;

    llvm::Value* ConvertToFloat(llvm::Value* pValue, bool signedness, llvm::Instruction* pInsertPos) const;
    llvm::Value* ConvertToInt(llvm::Value* pValue, bool signedness, llvm::Instruction* pInsertPos) const;

    // -----------------------------------------------------------------------------------------------------------------

    llvm::Module*   m_pModule;          // LLVM module
    Context*        m_pContext;         // LLPC context

    static const ColorFormatInfo    m_colorFormatInfo[]; // Info table of fragment color format

    const GraphicsPipelineBuildInfo* pPipelineInfo;   // Graphics pipeline build info
};

} // Llpc
