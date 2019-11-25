/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef __VK_SAMPLER_YCBCR_CONVERSION_H__
#define __VK_SAMPLER_YCBCR_CONVERSION_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"

#include "palUtil.h"

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;
class DispatchableSampler;

typedef struct
{
    union
    {
        struct
        {                                            // e.g R12X4G12X4_UNORM_2PACK16
            uint32_t channelBitsR               : 5; // channelBitsR = 12
            uint32_t channelBitsG               : 5; // channelBitsG = 12
            uint32_t channelBitsB               : 5; // channelBitsB =  0
            uint32_t                            :17;
        } bitDepth;
        struct
        {
            uint32_t                            :15; // VkComponentSwizzle, e.g
            uint32_t swizzleR                   : 3; // swizzleR = VK_COMPONENT_SWIZZLE_R(3)
            uint32_t swizzleG                   : 3; // swizzleG = VK_COMPONENT_SWIZZLE_G(4)
            uint32_t swizzleB                   : 3; // swizzleB = VK_COMPONENT_SWIZZLE_B(5)
            uint32_t swizzleA                   : 3; // swizzleA = VK_COMPONENT_SWIZZLE_A(6)
            uint32_t                            : 5;
        } componentMapping;
        struct
        {
            uint32_t                            :27;
            uint32_t ycbcrModel                 : 3; // RGB_IDENTITY(0), ycbcr_identity(1),
                                                     // _709(2),_601(3),_2020(4)
            uint32_t ycbcrRange                 : 1; // ITU_FULL(0), ITU_NARROW(0)
            uint32_t forceExplicitReconstruct   : 1; // Disable(0), Enable(1)
        };
        uint32_t u32All;
    } word0;
    union
    {
        struct
        {
            uint32_t planes                     : 2; // Number of planes, normally from 1 to 3
            uint32_t lumaFilter                 : 1; // FILTER_NEAREST(0) or FILTER_LINEAR(1)
            uint32_t chromaFilter               : 1; // FILTER_NEAREST(0) or FILTER_LINEAR(1)
            uint32_t xChromaOffset              : 1; // COSITED_EVEN(0) or MIDPOINT(1)
            uint32_t yChromaOffset              : 1; // COSITED_EVEN(0) or MIDPOINT(1)
            uint32_t xSubSampled                : 1; // true(1) or false(0)
            uint32_t ySubSampled                : 1; // true(1) or false(0)
            uint32_t tileOptimal                : 1; // true(1) or false(0)
            uint32_t dstSelXYZW                 :12;
            uint32_t undefined                  :11;
        };
        uint32_t u32All;
    } word1;
    union
    {
        // For yuv formats, bitCount may not equal to bitDepth, where bitCount >= bitDepth
        struct
        {
            uint32_t xBitCount                  : 6;
            uint32_t yBitCount                  : 6;
            uint32_t zBitCount                  : 6;
            uint32_t wBitCount                  : 6;
            uint32_t undefined                  : 8;
        } bitCount; // Pal Bit Counts
        uint32_t u32All;
    } word2;
    union
    {
        struct
        {
            uint32_t sqImgRsrcWord1             : 32;
        };
        uint32_t u32All;
    } word3;
} SamplerYcbcrConversionMetaData;

typedef struct
{
  uint32 xBitCount;
  uint32 yBitCount;
  uint32 zBitCount;
  uint32 wBitCount;
} BitDepth;

class SamplerYcbcrConversion : public NonDispatchable<VkSamplerYcbcrConversion, SamplerYcbcrConversion>
{
public:
    static VkResult Create(
        Device*                                     pDevice,
        const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSamplerYcbcrConversion*                   pYcbcrConversion);

    void Destroy(
        const Device*                               pDevice,
        const VkAllocationCallbacks*                pAllocator);

    SamplerYcbcrConversionMetaData* GetMetaData() {return &m_metaData;}

protected:
    SamplerYcbcrConversion(const VkSamplerYcbcrConversionCreateInfo* pCreateInfo);

private:
    uint32_t GetSqImgRsrcWord1(VkFormat format);

    BitDepth GetYuvBitDepth(VkFormat format);

    uint32_t GetDstSelXYZW(VkFormat format);

    SamplerYcbcrConversionMetaData m_metaData;
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversion(
    VkDevice                                    device,
    VkSamplerYcbcrConversion                    ycbcrConversion,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_SAMPLER_YCBCR_CONVERSION_H__ */
