/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

    void* pMemory = pDevice->AllocApiObject(
        pAllocator,
        sizeof(SamplerYcbcrConversion));

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) SamplerYcbcrConversion(pCreateInfo, pDevice->GetRuntimeSettings());

        *pYcbcrConversion = SamplerYcbcrConversion::HandleFromVoidPointer(pMemory);
    }

    return result;
}

// =====================================================================================================================
void SamplerYcbcrConversion::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);
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
        yuvFormatInfo = Pal::Formats::FormatInfoTable[static_cast<uint32_t>(VkToPalFormat(format, m_settings).format)];
        BitDepth bitDepth;
        bitDepth.xBitCount = yuvFormatInfo.bitCount[0];
        bitDepth.yBitCount = yuvFormatInfo.bitCount[1];
        bitDepth.zBitCount = yuvFormatInfo.bitCount[2];
        bitDepth.wBitCount = yuvFormatInfo.bitCount[3];
        return bitDepth;
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
    const VkSamplerYcbcrConversionCreateInfo* pCreateInfo,
    const RuntimeSettings&                    settings)
    :
	m_settings(settings)
{
    VkFormat createInfoFormat = pCreateInfo->format;

    Pal::SwizzledFormat palFormat = VkToPalFormat(createInfoFormat, m_settings);

    if ((createInfoFormat == VK_FORMAT_B5G5R5A1_UNORM_PACK16) ||
        (createInfoFormat == VK_FORMAT_R5G5B5A1_UNORM_PACK16))
    {
        m_metaData.word0.bitDepth.channelBitsR = GetYuvBitDepth(createInfoFormat).wBitCount;
        m_metaData.word0.bitDepth.channelBitsG = GetYuvBitDepth(createInfoFormat).zBitCount;
        m_metaData.word0.bitDepth.channelBitsB = GetYuvBitDepth(createInfoFormat).yBitCount;
    }
    else
    {
        m_metaData.word0.bitDepth.channelBitsR = GetYuvBitDepth(createInfoFormat).xBitCount;
        m_metaData.word0.bitDepth.channelBitsG = GetYuvBitDepth(createInfoFormat).yBitCount;
        m_metaData.word0.bitDepth.channelBitsB = GetYuvBitDepth(createInfoFormat).zBitCount;
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

    m_metaData.word1.planes        = Formats::GetYuvPlaneCounts(createInfoFormat);
    m_metaData.word1.xSubSampled   = Formats::IsYuvXChromaSubsampled(createInfoFormat);
    m_metaData.word1.ySubSampled   = Formats::IsYuvYChromaSubsampled(createInfoFormat);

    Pal::Formats::FormatInfo yuvFormatInfo = Pal::Formats::FormatInfoTable[static_cast<uint32_t>(palFormat.format)];
    m_metaData.word2.bitCounts.xBitCount = yuvFormatInfo.bitCount[0];
    m_metaData.word2.bitCounts.yBitCount = yuvFormatInfo.bitCount[1];
    m_metaData.word2.bitCounts.zBitCount = yuvFormatInfo.bitCount[2];
    m_metaData.word2.bitCounts.wBitCount = yuvFormatInfo.bitCount[3];

    m_metaData.word4.lumaWidth  = 0;
    m_metaData.word4.lumaHeight = 0;
    m_metaData.word5.lumaDepth  = 0;
}

// =====================================================================================================================
void SamplerYcbcrConversion::SetExtent(
    uint32 width,
    uint32 height,
    uint32 depth)
{
    m_metaData.word4.lumaWidth  = width;
    m_metaData.word4.lumaHeight = height;
    m_metaData.word5.lumaDepth  = depth;
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
