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
#ifndef __VK_SAMPLER_H__
#define __VK_SAMPLER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "include/vk_sampler_ycbcr_conversion.h"

#include "palUtil.h"

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;
class ApiSampler;

class Sampler final : public NonDispatchable<VkSampler, Sampler>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkSamplerCreateInfo*      pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSampler*                      pSampler);

    VK_FORCEINLINE const void* Descriptor() const
    {
        return static_cast<const void*>(this + 1);
    }

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    uint64_t GetApiHash() const
        { return m_apiHash; }

    bool IsYCbCrSampler() const
        { return m_isYCbCrSampler; }

    uint32_t GetMultiPlaneCount() const
    {
        return m_multiPlaneCount;
    }

    uint32_t GetBorderColorPaletteIndex() const
    {
        return m_borderColorPaletteIndex;
    }

    Vkgc::SamplerYCbCrConversionMetaData* GetYCbCrConversionMetaData()
    {
        return m_pYcbcrConversionMetaData;
    }

    bool IsYCbCrConversionMetaDataUpdated(const Vkgc::SamplerYCbCrConversionMetaData* pMetaData)
    {
        return ((m_pYcbcrConversionMetaData->word4.u32All != pMetaData->word4.u32All) ||
                (m_pYcbcrConversionMetaData->word5.u32All != pMetaData->word5.u32All));
    }

protected:

    struct SamplerExtStructs
    {
        const VkSamplerReductionModeCreateInfo*                   pSamplerReductionModeCreateInfo;
        const VkSamplerYcbcrConversionInfo*                       pSamplerYcbcrConversionInfo;
        const VkSamplerCustomBorderColorCreateInfoEXT*            pSamplerCustomBorderColorCreateInfoEXT;
        const VkSamplerBorderColorComponentMappingCreateInfoEXT*  pSamplerBorderColorComponentMappingCreateInfoEXT;
        const VkOpaqueCaptureDescriptorDataCreateInfoEXT*         pOpaqueCaptureDescriptorDataCreateInfoEXT;
    };

    Sampler(
        uint64_t                              apiHash,
        bool                                  isYCbCrSampler,
        uint32_t                              multiPlaneCount,
        uint32_t                              borderColorPaletteIndex,
        Vkgc::SamplerYCbCrConversionMetaData* pYcbcrConversionMetaData)
        :
        m_apiHash(apiHash),
        m_isYCbCrSampler(isYCbCrSampler),
        m_multiPlaneCount(multiPlaneCount),
        m_borderColorPaletteIndex(borderColorPaletteIndex),
        m_pYcbcrConversionMetaData(pYcbcrConversionMetaData)
    {
    }

    static uint64_t BuildApiHash(
        const VkSamplerCreateInfo* pCreateInfo,
        const SamplerExtStructs&   extStructs);

    const uint64_t                        m_apiHash;
    const bool                            m_isYCbCrSampler;
    const uint32_t                        m_multiPlaneCount;
    const uint32_t                        m_borderColorPaletteIndex;
    Vkgc::SamplerYCbCrConversionMetaData* m_pYcbcrConversionMetaData;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Sampler);

    static void HandleExtensionStructs(
        const VkSamplerCreateInfo* pCreateInfo,
        SamplerExtStructs*         pExtStructs);

};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySampler(
    VkDevice                                    device,
    VkSampler                                   sampler,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_SAMPLER_H__ */
