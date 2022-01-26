/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_instance.h"
#include "include/vk_physical_device.h"
#include "include/vk_sampler.h"
#include "include/vk_sampler_ycbcr_conversion.h"

#include "palDevice.h"
#include "palMetroHash.h"

namespace vk
{

// =====================================================================================================================
// Generates the API hash using the contents of the VkSamplerCreateInfo struct
uint64_t Sampler::BuildApiHash(
    const VkSamplerCreateInfo* pCreateInfo)
{
    Util::MetroHash64 hasher;

    hasher.Update(pCreateInfo->flags);
    hasher.Update(pCreateInfo->magFilter);
    hasher.Update(pCreateInfo->minFilter);
    hasher.Update(pCreateInfo->mipmapMode);
    hasher.Update(pCreateInfo->addressModeU);
    hasher.Update(pCreateInfo->addressModeV);
    hasher.Update(pCreateInfo->addressModeW);
    hasher.Update(pCreateInfo->mipLodBias);
    hasher.Update(pCreateInfo->anisotropyEnable);
    hasher.Update(pCreateInfo->maxAnisotropy);
    hasher.Update(pCreateInfo->compareEnable);
    hasher.Update(pCreateInfo->compareOp);
    hasher.Update(pCreateInfo->minLod);
    hasher.Update(pCreateInfo->maxLod);
    hasher.Update(pCreateInfo->borderColor);
    hasher.Update(pCreateInfo->unnormalizedCoordinates);

    if (pCreateInfo->pNext != nullptr)
    {
        union
        {
            const VkStructHeader*                                       pInfo;
            const VkSamplerYcbcrConversionInfo*                         pYCbCrConversionInfo;
            const VkSamplerReductionModeCreateInfo*                     pReductionModeCreateInfo;
            const VkSamplerCustomBorderColorCreateInfoEXT*              pVkSamplerCustomBorderColorCreateInfoEXT;
            const VkSamplerBorderColorComponentMappingCreateInfoEXT*    pVkSamplerBorderColorComponentMappingCreateInfoEXT;
        };

        pInfo = static_cast<const VkStructHeader*>(pCreateInfo->pNext);

        while (pInfo != nullptr)
        {
            switch (static_cast<uint32_t>(pInfo->sType))
            {
            case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
                hasher.Update(pYCbCrConversionInfo->sType);
                Vkgc::SamplerYCbCrConversionMetaData* pSamplerYCbCrConversionMetaData;
                pSamplerYCbCrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(pYCbCrConversionInfo->conversion)->GetMetaData();
                hasher.Update(pSamplerYCbCrConversionMetaData->word0.u32All);
                hasher.Update(pSamplerYCbCrConversionMetaData->word1.u32All);
                hasher.Update(pSamplerYCbCrConversionMetaData->word2.u32All);
                hasher.Update(pSamplerYCbCrConversionMetaData->word3.u32All);
                hasher.Update(pSamplerYCbCrConversionMetaData->word4.u32All);
                hasher.Update(pSamplerYCbCrConversionMetaData->word5.u32All);

                break;
            case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
                hasher.Update(pReductionModeCreateInfo->sType);
                hasher.Update(pReductionModeCreateInfo->reductionMode);

                break;
            case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
                hasher.Update(pVkSamplerCustomBorderColorCreateInfoEXT->sType);
                hasher.Update(pVkSamplerCustomBorderColorCreateInfoEXT->customBorderColor);
                hasher.Update(pVkSamplerCustomBorderColorCreateInfoEXT->format);
                break;
            case VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT:
                hasher.Update(pVkSamplerBorderColorComponentMappingCreateInfoEXT->sType);
                hasher.Update(pVkSamplerBorderColorComponentMappingCreateInfoEXT->components);
                hasher.Update(pVkSamplerBorderColorComponentMappingCreateInfoEXT->srgb);
                break;
            default:
                break;
            }

            pInfo = pInfo->pNext;
        }
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t*>(&hash));

    return hash;
}

// =====================================================================================================================
// Create a new sampler object
VkResult Sampler::Create(
    Device*                         pDevice,
    const VkSamplerCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkSampler*                      pSampler)
{
    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

    uint64           apiHash     = BuildApiHash(pCreateInfo);
    Pal::SamplerInfo samplerInfo = {};
    samplerInfo.filterMode       = Pal::TexFilterMode::Blend;  // Initialize "legacy" behavior
    Vkgc::SamplerYCbCrConversionMetaData* pSamplerYCbCrConversionMetaData = nullptr;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    samplerInfo.filter     = VkToPalTexFilter(pCreateInfo->anisotropyEnable,
                                              pCreateInfo->magFilter,
                                              pCreateInfo->minFilter,
                                              pCreateInfo->mipmapMode);
    samplerInfo.addressU   = VkToPalTexAddressMode(pCreateInfo->addressModeU);
    samplerInfo.addressV   = VkToPalTexAddressMode(pCreateInfo->addressModeV);
    samplerInfo.addressW   = VkToPalTexAddressMode(pCreateInfo->addressModeW);
    samplerInfo.mipLodBias = pCreateInfo->mipLodBias;

    samplerInfo.maxAnisotropy           = static_cast<uint32_t>(pCreateInfo->maxAnisotropy);
    samplerInfo.compareFunc             = (pCreateInfo->compareEnable == VK_FALSE) ?
                                              Pal::CompareFunc::Never : VkToPalCompareFunc(pCreateInfo->compareOp);
    samplerInfo.minLod                  = pCreateInfo->minLod;
    samplerInfo.maxLod                  = pCreateInfo->maxLod;
    samplerInfo.borderColorType         = VkToPalBorderColorType(pCreateInfo->borderColor);
    samplerInfo.borderColorPaletteIndex = MaxBorderColorPaletteSize;

    switch (settings.preciseAnisoMode)
    {
    case EnablePreciseAniso:
        samplerInfo.flags.preciseAniso = 1;
        break;
    case DisablePreciseAnisoAll:
        samplerInfo.flags.preciseAniso = 0;
        break;
    case DisablePreciseAnisoAfOnly:
        samplerInfo.flags.preciseAniso = (pCreateInfo->anisotropyEnable == VK_FALSE) ? 1 : 0;
        break;
    default:
        break;
    }

    // disableSingleMipAnisoOverride=1 ensure properly sampling with single mipmap level and anisotropic filtering.
    samplerInfo.flags.disableSingleMipAnisoOverride = 1;

    samplerInfo.flags.useAnisoThreshold             = (settings.useAnisoThreshold == true) ? 1 : 0;
    samplerInfo.anisoThreshold                      = settings.anisoThreshold;
    samplerInfo.perfMip                             = settings.samplerPerfMip;
    samplerInfo.flags.unnormalizedCoords            = (pCreateInfo->unnormalizedCoordinates == VK_TRUE) ? 1 : 0;
    samplerInfo.flags.prtBlendZeroMode              = 0;
    samplerInfo.flags.seamlessCubeMapFiltering      = 1;
    samplerInfo.flags.truncateCoords                = ((pCreateInfo->magFilter == VK_FILTER_NEAREST) &&
                                                       (pCreateInfo->minFilter == VK_FILTER_NEAREST) &&
                                                       (samplerInfo.compareFunc == Pal::CompareFunc::Never))
                                                      ? 1 : 0;

    // Parse the creation info.
    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT:
        {
            const auto* pExtInfo = static_cast<const VkSamplerReductionModeCreateInfo*>(pNext);
            samplerInfo.filterMode = VkToPalTexFilterMode(pExtInfo->reductionMode);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
        {
            const auto* pExtInfo = static_cast<const VkSamplerYcbcrConversionInfo*>(pNext);
            pSamplerYCbCrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(pExtInfo->conversion)->GetMetaData();
            pSamplerYCbCrConversionMetaData->word1.lumaFilter = samplerInfo.filter.minification;

            if (pSamplerYCbCrConversionMetaData->word0.forceExplicitReconstruct)
            {
                samplerInfo.flags.truncateCoords = 0;
            }
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
        {
            if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_CUSTOM_BORDER_COLOR))
            {
                const auto* pExtInfo = static_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext);
                VK_ASSERT(samplerInfo.borderColorType == Pal::BorderColorType::PaletteIndex);
                samplerInfo.borderColorPaletteIndex = pDevice->GetBorderColorIndex(pExtInfo->customBorderColor.float32);

                if (samplerInfo.borderColorPaletteIndex == MaxBorderColorPaletteSize)
                {
                    samplerInfo.borderColorType = Pal::BorderColorType::TransparentBlack;
                    VK_ASSERT(!"Limit has been reached");
                }
            }
            else
            {
                samplerInfo.borderColorType = Pal::BorderColorType::TransparentBlack;
                VK_ASSERT(!"Extension is not enabled");
            }
            break;
        }
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }

    // Figure out how big a sampler SRD is. This is not the most efficient way of doing
    // things, so we could cache the SRD size.
    Pal::DeviceProperties props;
    pDevice->PalDevice(DefaultDeviceIndex)->GetProperties(&props);

    const uint32 apiSize = sizeof(Sampler);
    const uint32 palSize = props.gfxipProperties.srdSizes.sampler;

    const uint32 yCbCrMetaDataSize = (pSamplerYCbCrConversionMetaData == nullptr) ?
                                        0 : sizeof(Vkgc::SamplerYCbCrConversionMetaData);

    // Allocate system memory. Construct the sampler in memory and then wrap a Vulkan
    // object around it.
    void* pMemory = pDevice->AllocApiObject(
        pAllocator,
        apiSize + palSize + yCbCrMetaDataSize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Create one sampler srd which can be used by any device in the group
    pDevice->PalDevice(DefaultDeviceIndex)->CreateSamplerSrds(
            1,
            &samplerInfo,
            Util::VoidPtrInc(pMemory, apiSize));

    if (pSamplerYCbCrConversionMetaData != nullptr)
    {
        memcpy(Util::VoidPtrInc(pMemory, apiSize + palSize), pSamplerYCbCrConversionMetaData, yCbCrMetaDataSize);
    }

    uint32_t multiPlaneCount = pSamplerYCbCrConversionMetaData != nullptr ? pSamplerYCbCrConversionMetaData->word1.planes : 1;

    VK_PLACEMENT_NEW (pMemory) Sampler(apiHash,
                                       (pSamplerYCbCrConversionMetaData != nullptr),
                                       multiPlaneCount,
                                       samplerInfo.borderColorPaletteIndex,
                                       pSamplerYCbCrConversionMetaData);

    *pSampler = Sampler::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// ====================================================================================================================
// Destroy a sampler object
VkResult Sampler::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    if (m_borderColorPaletteIndex != MaxBorderColorPaletteSize)
    {
        pDevice->ReleaseBorderColorIndex(m_borderColorPaletteIndex);
    }

    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroySampler(
    VkDevice                                    device,
    VkSampler                                   sampler,
    const VkAllocationCallbacks*                pAllocator)
{
    if (sampler != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Sampler::ObjectFromHandle(sampler)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
