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
#ifndef __VK_SAMPLER_YCBCR_CONVERSION_H__
#define __VK_SAMPLER_YCBCR_CONVERSION_H__

#pragma once

#include "vkgcDefs.h"
#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"

#include "palUtil.h"

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;
class ApiSampler;

typedef struct
{
  uint32 xBitCount;
  uint32 yBitCount;
  uint32 zBitCount;
  uint32 wBitCount;
} BitDepth;

class SamplerYcbcrConversion final : public NonDispatchable<VkSamplerYcbcrConversion, SamplerYcbcrConversion>
{
public:
    static VkResult Create(
        Device*                                     pDevice,
        const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSamplerYcbcrConversion*                   pYcbcrConversion);

    void Destroy(
        Device*                                     pDevice,
        const VkAllocationCallbacks*                pAllocator);

    Vkgc::SamplerYCbCrConversionMetaData* GetMetaData() {return &m_metaData;}

    void SetExtent(uint32 width, uint32 height, uint32 depth);

protected:
    SamplerYcbcrConversion(const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const RuntimeSettings& settings);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SamplerYcbcrConversion);

    BitDepth GetYuvBitDepth(VkFormat format);

    static uint32_t MapSwizzle(
        VkComponentSwizzle    inputSwizzle,
        VkComponentSwizzle    defaultSwizzle);

    Vkgc::SamplerYCbCrConversionMetaData m_metaData;
    const RuntimeSettings&               m_settings;
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
