/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_formats.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_sampler_ycbcr_conversion.h"

#include "palDevice.h"

namespace vk
{

// =====================================================================================================================
VkResult SamplerYcbcrConversion::Create(
    Device*                                     pDevice,
    const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSamplerYcbcrConversion*                   pYcbcrConversion)
{
    VkResult result = VK_SUCCESS;

    void* pMemory = pAllocator->pfnAllocation(pAllocator->pUserData,
                                              sizeof(SamplerYcbcrConversion),
                                              VK_DEFAULT_MEM_ALIGN,
                                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) SamplerYcbcrConversion(pCreateInfo);

        *pYcbcrConversion = SamplerYcbcrConversion::HandleFromVoidPointer(pMemory);
    }

    return result;
}

// =====================================================================================================================
void SamplerYcbcrConversion::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Returns the bit depth if the given format is a yuv format.
BitDepth SamplerYcbcrConversion::GetYuvBitDepth(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        return { 8, 8, 8, 0 };
    case VK_FORMAT_R10X6_UNORM_PACK16:
        return { 10, 0, 0, 0 };
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        return { 10, 10, 0, 0 };
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        return { 10, 10, 10, 10 };
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        return { 10, 10, 10, 0 };
    case VK_FORMAT_R12X4_UNORM_PACK16:
        return { 12, 0, 0, 0 };
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        return { 12, 12, 0, 0 };
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        return { 12, 12, 12, 12 };
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        return { 16, 16, 16, 0 };
    default:
        Pal::Formats::FormatInfo yuvFormatInfo;
        yuvFormatInfo = Pal::Formats::FormatInfoTable[static_cast<uint32_t>(VkToPalFormat(format).format)];
        BitDepth bitDepth;
        bitDepth.xBitCount = yuvFormatInfo.bitCount[0];
        bitDepth.yBitCount = yuvFormatInfo.bitCount[1];
        bitDepth.zBitCount = yuvFormatInfo.bitCount[2];
        bitDepth.wBitCount = yuvFormatInfo.bitCount[3];
        return bitDepth;
    }
}

// =====================================================================================================================
// Returns the constructed DST_SEL_* value for yuv conersion usage.
//     struct SQ_IMG_RSRC_WORD3 {
//         unsigned int DST_SEL_X  : 3;
//         unsigned int DST_SEL_Y  : 3;
//         unsigned int DST_SEL_Z  : 3;
//         unsigned int DST_SEL_W  : 3;
//         unsigned int BASE_LEVEL : 4;
//         unsigned int LAST_LEVEL : 4;
//         unsigned int SW_MODE    : 5;
//         unsigned int            : 3;
//         unsigned int TYPE       : 4;
//     } bits, bitfields;
// E.g VK_FORMAT_B8G8R8G8_422_UNORM,
//              ____ ____ ____ ____ ____ ____ ____ ____ ___
// (HEX)       |   0|   0|   0|   0|   0|   F|   2|   E|   |
// (BIN)       |0000|0000|0000|0000|0000|1111|0010|1110|   |
//             |                        //   ||   \\   \\  |
// (BIN)       |                     |111| |100| |101| |110|
// (DST_SEL_*) |                     | W | | Z | | Y | | X |
// (DEC)       |                     | 7 | | 4 | | 5 | | 6 |
// (SQ_SEL_*)  |                     | W | | X | | Y | | Z |
// (RGBA)      |                     | A | | B | | G | | R |
// (Y'CbCr)    |                     | A | | Cb| | Y'| | Cr|
// Then all the yuv formats are forced to be output in
// the order of XYZ <-> RGB(VYU).
uint32_t SamplerYcbcrConversion::GetDstSelXYZW(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        return 0x105;
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        return 0x305;
    case VK_FORMAT_G8B8G8R8_422_UNORM:
        return 0x977;
    case VK_FORMAT_B8G8R8G8_422_UNORM:
        return 0xF2E;
    default:
        return 0x000;
    }
}

// =====================================================================================================================
// Returns the constructed SqImgRsrcWord1 value for yuv conersion usage, only be availble for CbCr(plane).
//     struct SQ_IMG_RSRC_WORD1 {
//         unsigned int BASE_ADDRESS_HI :  8;
//         unsigned int MIN_LOD         : 12;
//         unsigned int DATA_FORMAT     :  6;
//         unsigned int NUM_FORMAT      :  4;
//         unsigned int NV              :  1;
//         unsigned int META_DIRECT     :  1;
//     } bits, bitfields;
// E.g VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
//              ____ ____ ____ ____ ____ ____ ____ ____
// (HEX)       |   0|   0|   3|   0|   0|   0|   0|   0|
// (BIN)       |0000|0000|0011|0000|0000|0000|0000|0000|
//             |       |000011|                        |
//             |          ||                           |
//             |     |IMG_DATA_FORMAT_8_8|             |
uint32_t SamplerYcbcrConversion::GetSqImgRsrcWord1(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        return 0x00300000;
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        return 0x00500000;
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
        return 0x00A00000;
    default:
        return 0x00100000;
    }
}

// =====================================================================================================================
// Returns the mapped swizzle which eliminates identity case and adjust the value of enum to
// match llpc YCbCr sampler setting, where: Zero = 0, One = 1, R = 4, G = 5, B = 6, A = 7
uint32_t SamplerYcbcrConversion::MapSwizzle(
    VkComponentSwizzle inputSwizzle,
    VkComponentSwizzle defaultSwizzle)
{
    switch (inputSwizzle)
    {
    case VK_COMPONENT_SWIZZLE_IDENTITY:
        return (defaultSwizzle + 1);
    case VK_COMPONENT_SWIZZLE_ZERO:
        return 0;
    case VK_COMPONENT_SWIZZLE_ONE:
        return 1;
    case VK_COMPONENT_SWIZZLE_R:
    case VK_COMPONENT_SWIZZLE_G:
    case VK_COMPONENT_SWIZZLE_B:
    case VK_COMPONENT_SWIZZLE_A:
        return (inputSwizzle + 1);
    default:
        VK_NEVER_CALLED();
        return (inputSwizzle + 1);
    }
}

// =====================================================================================================================
// Assign Ycbcr Conversion MetaData during construction
SamplerYcbcrConversion::SamplerYcbcrConversion(
    const VkSamplerYcbcrConversionCreateInfo* pCreateInfo)
{
    VK_ASSERT(pCreateInfo->pNext == nullptr);

    const Pal::SwizzledFormat palFormat = VkToPalFormat(pCreateInfo->format);

    if ((pCreateInfo->format == VK_FORMAT_B5G5R5A1_UNORM_PACK16) ||
        (pCreateInfo->format == VK_FORMAT_R5G5B5A1_UNORM_PACK16))
    {
        m_metaData.word0.bitDepth.channelBitsR = GetYuvBitDepth(pCreateInfo->format).wBitCount;
        m_metaData.word0.bitDepth.channelBitsG = GetYuvBitDepth(pCreateInfo->format).zBitCount;
        m_metaData.word0.bitDepth.channelBitsB = GetYuvBitDepth(pCreateInfo->format).yBitCount;
    }
    else
    {
        m_metaData.word0.bitDepth.channelBitsR = GetYuvBitDepth(pCreateInfo->format).xBitCount;
        m_metaData.word0.bitDepth.channelBitsG = GetYuvBitDepth(pCreateInfo->format).yBitCount;
        m_metaData.word0.bitDepth.channelBitsB = GetYuvBitDepth(pCreateInfo->format).zBitCount;
    }

    m_metaData.word0.componentMapping.swizzleR = MapSwizzle(pCreateInfo->components.r, VK_COMPONENT_SWIZZLE_R);
    m_metaData.word0.componentMapping.swizzleG = MapSwizzle(pCreateInfo->components.g, VK_COMPONENT_SWIZZLE_G);
    m_metaData.word0.componentMapping.swizzleB = MapSwizzle(pCreateInfo->components.b, VK_COMPONENT_SWIZZLE_B);
    m_metaData.word0.componentMapping.swizzleA = MapSwizzle(pCreateInfo->components.a, VK_COMPONENT_SWIZZLE_A);
    m_metaData.word0.yCbCrModel                = pCreateInfo->ycbcrModel;
    m_metaData.word0.yCbCrRange                = pCreateInfo->ycbcrRange;
    m_metaData.word0.forceExplicitReconstruct  = pCreateInfo->forceExplicitReconstruction;

    m_metaData.word1.chromaFilter  = pCreateInfo->chromaFilter;
    m_metaData.word1.xChromaOffset = pCreateInfo->xChromaOffset;
    m_metaData.word1.yChromaOffset = pCreateInfo->yChromaOffset;
    m_metaData.word1.dstSelXYZW    = GetDstSelXYZW(pCreateInfo->format);
    m_metaData.word1.planes        = Formats::GetYuvPlaneCounts(pCreateInfo->format);
    m_metaData.word1.xSubSampled   = Formats::IsYuvXChromaSubsampled(pCreateInfo->format);
    m_metaData.word1.ySubSampled   = Formats::IsYuvYChromaSubsampled(pCreateInfo->format);
    m_metaData.word1.tileOptimal   = Formats::IsYuvTileOptimal(pCreateInfo->format);

    Pal::Formats::FormatInfo yuvFormatInfo = Pal::Formats::FormatInfoTable[static_cast<uint32_t>(palFormat.format)];
    m_metaData.word2.bitCounts.xBitCount = yuvFormatInfo.bitCount[0];
    m_metaData.word2.bitCounts.yBitCount = yuvFormatInfo.bitCount[1];
    m_metaData.word2.bitCounts.zBitCount = yuvFormatInfo.bitCount[2];
    m_metaData.word2.bitCounts.wBitCount = yuvFormatInfo.bitCount[3];

    m_metaData.word3.sqImgRsrcWord1 = GetSqImgRsrcWord1(pCreateInfo->format);
}

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversion(
    VkDevice                                    device,
    VkSamplerYcbcrConversion                    ycbcrConversion,
    const VkAllocationCallbacks*                pAllocator)
{
    if (ycbcrConversion != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        SamplerYcbcrConversion::ObjectFromHandle(ycbcrConversion)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
