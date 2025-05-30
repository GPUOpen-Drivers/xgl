/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const VkSamplerCreateInfo* pCreateInfo,
    const SamplerExtStructs&   extStructs)
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

    if (extStructs.pSamplerYcbcrConversionInfo != nullptr)
    {
        hasher.Update(extStructs.pSamplerYcbcrConversionInfo->sType);
        Vkgc::SamplerYCbCrConversionMetaData* pSamplerYCbCrConversionMetaData;
        pSamplerYCbCrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(
            extStructs.pSamplerYcbcrConversionInfo->conversion)->GetMetaData();
        hasher.Update(pSamplerYCbCrConversionMetaData->word0.u32All);
        hasher.Update(pSamplerYCbCrConversionMetaData->word1.u32All);
        hasher.Update(pSamplerYCbCrConversionMetaData->word2.u32All);
        hasher.Update(pSamplerYCbCrConversionMetaData->word3.u32All);
        hasher.Update(pSamplerYCbCrConversionMetaData->word4.u32All);
        hasher.Update(pSamplerYCbCrConversionMetaData->word5.u32All);
    }

    if (extStructs.pSamplerReductionModeCreateInfo != nullptr)
    {
        hasher.Update(extStructs.pSamplerReductionModeCreateInfo->sType);
        hasher.Update(extStructs.pSamplerReductionModeCreateInfo->reductionMode);
    }

    if (extStructs.pSamplerCustomBorderColorCreateInfoEXT != nullptr)
    {
        hasher.Update(extStructs.pSamplerCustomBorderColorCreateInfoEXT->sType);
        hasher.Update(extStructs.pSamplerCustomBorderColorCreateInfoEXT->customBorderColor);
        hasher.Update(extStructs.pSamplerCustomBorderColorCreateInfoEXT->format);
    }

    if (extStructs.pSamplerBorderColorComponentMappingCreateInfoEXT != nullptr)
    {
        hasher.Update(extStructs.pSamplerBorderColorComponentMappingCreateInfoEXT->sType);
        hasher.Update(extStructs.pSamplerBorderColorComponentMappingCreateInfoEXT->components);
        hasher.Update(extStructs.pSamplerBorderColorComponentMappingCreateInfoEXT->srgb);
    }

    if (extStructs.pOpaqueCaptureDescriptorDataCreateInfoEXT != nullptr)
    {
        hasher.Update(extStructs.pOpaqueCaptureDescriptorDataCreateInfoEXT->sType);
        const uint32* pPaletteIndex = static_cast<
            const uint32*>(extStructs.pOpaqueCaptureDescriptorDataCreateInfoEXT->opaqueCaptureDescriptorData);

        hasher.Update(*pPaletteIndex);
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

    SamplerExtStructs extStructs = {};
    Pal::SamplerInfo samplerInfo = {};

    // Convert sampler create info to pal sampler info.
    ConvertSamplerCreateInfo(pDevice, pCreateInfo, &samplerInfo, &extStructs);

    // Handle custom border color
    const bool extCustomBorderColor = pDevice->IsExtensionEnabled(DeviceExtensions::EXT_CUSTOM_BORDER_COLOR);

    if (extCustomBorderColor && (extStructs.pOpaqueCaptureDescriptorDataCreateInfoEXT != nullptr))
    {
        if (extStructs.pSamplerCustomBorderColorCreateInfoEXT != nullptr)
        {
            const uint32* pPaletteIndex = static_cast<
                const uint32*>(extStructs.pOpaqueCaptureDescriptorDataCreateInfoEXT->opaqueCaptureDescriptorData);

            pDevice->ReserveBorderColorIndex(
                *pPaletteIndex,
                extStructs.pSamplerCustomBorderColorCreateInfoEXT->customBorderColor.float32);

            samplerInfo.borderColorPaletteIndex = *pPaletteIndex;
        }
    }
    else if (extStructs.pSamplerCustomBorderColorCreateInfoEXT != nullptr)
    {
        if (extCustomBorderColor)
        {
            VK_ASSERT(samplerInfo.borderColorType == Pal::BorderColorType::PaletteIndex);

            samplerInfo.borderColorPaletteIndex = pDevice->GetBorderColorIndex(
                extStructs.pSamplerCustomBorderColorCreateInfoEXT->customBorderColor.float32);

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
    }

    // Figure out how big a sampler SRD is. This is not the most efficient way of doing
    // things, so we could cache the SRD size.
    const Pal::DeviceProperties& props = pDevice->GetPalProperties();

    const uint32 apiSize = sizeof(Sampler);
    const uint32 palSize = props.gfxipProperties.srdSizes.sampler;

    Vkgc::SamplerYCbCrConversionMetaData* pSamplerYCbCrConversionMetaData = nullptr;

    if (extStructs.pSamplerYcbcrConversionInfo != nullptr)
    {
        pSamplerYCbCrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(
                                            extStructs.pSamplerYcbcrConversionInfo->conversion)->GetMetaData();
    }

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

    VK_PLACEMENT_NEW (pMemory) Sampler(BuildApiHash(pCreateInfo, extStructs),
                                       (pSamplerYCbCrConversionMetaData != nullptr),
                                       multiPlaneCount,
                                       samplerInfo.borderColorPaletteIndex,
                                       pSamplerYCbCrConversionMetaData);

    *pSampler = Sampler::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// ====================================================================================================================
void Sampler::BuildSrd(
    const Device*                         pDevice,
    const VkSamplerCreateInfo*            pCreateInfo,
    uint32_t                              borderColorIndex,
    void*                                 pOut)
{
    SamplerExtStructs extStructs = {};
    Pal::SamplerInfo samplerInfo = {};

    ConvertSamplerCreateInfo(pDevice, pCreateInfo, &samplerInfo, &extStructs);

    samplerInfo.borderColorPaletteIndex = borderColorIndex;

    if (borderColorIndex == MaxBorderColorPaletteSize)
    {
        samplerInfo.borderColorType = Pal::BorderColorType::TransparentBlack;
    }

    Vkgc::SamplerYCbCrConversionMetaData* pSamplerYCbCrConversionMetaData = nullptr;

    if (extStructs.pSamplerYcbcrConversionInfo != nullptr)
    {
        pSamplerYCbCrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(
                                            extStructs.pSamplerYcbcrConversionInfo->conversion)->GetMetaData();
    }

    const uint32 yCbCrMetaDataSize = (pSamplerYCbCrConversionMetaData == nullptr) ?
                                        0 : sizeof(Vkgc::SamplerYCbCrConversionMetaData);

    pDevice->PalDevice(DefaultDeviceIndex)->CreateSamplerSrds(
            1,
            &samplerInfo,
            pOut);

    const uint32 palSize = pDevice->GetPalProperties().gfxipProperties.srdSizes.sampler;

    if (pSamplerYCbCrConversionMetaData != nullptr)
    {
        memcpy(Util::VoidPtrInc(pOut, palSize), pSamplerYCbCrConversionMetaData, yCbCrMetaDataSize);
    }
}

// ====================================================================================================================
// Convert sampler create info to pal sampler info.
void Sampler::ConvertSamplerCreateInfo(
    const Device*              pDevice,
    const VkSamplerCreateInfo* pCreateInfo,
    Pal::SamplerInfo*          pPalSamplerInfo,
    SamplerExtStructs*         pExtStructs)
{
    pPalSamplerInfo->filterMode     = Pal::TexFilterMode::Blend;  // Initialize "legacy" behavior

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    const VkBool32 anisotropyEnable = (settings.forceDisableAnisoFilter == false) ?
        pCreateInfo->anisotropyEnable : VK_FALSE;

    pPalSamplerInfo->filter     = VkToPalTexFilter(anisotropyEnable,
                                            pCreateInfo->magFilter,
                                            pCreateInfo->minFilter,
                                            pCreateInfo->mipmapMode);
    pPalSamplerInfo->addressU   = VkToPalTexAddressMode(pCreateInfo->addressModeU);
    pPalSamplerInfo->addressV   = VkToPalTexAddressMode(pCreateInfo->addressModeV);
    pPalSamplerInfo->addressW   = VkToPalTexAddressMode(pCreateInfo->addressModeW);
    pPalSamplerInfo->mipLodBias = pCreateInfo->mipLodBias;

    pPalSamplerInfo->maxAnisotropy           = static_cast<uint32_t>(pCreateInfo->maxAnisotropy);
    pPalSamplerInfo->compareFunc             = (pCreateInfo->compareEnable == VK_FALSE) ?
                                            Pal::CompareFunc::Never : VkToPalCompareFunc(pCreateInfo->compareOp);
    pPalSamplerInfo->minLod                  = pCreateInfo->minLod;
    pPalSamplerInfo->maxLod                  = pCreateInfo->maxLod;
    pPalSamplerInfo->borderColorType         = VkToPalBorderColorType(pCreateInfo->borderColor);
    pPalSamplerInfo->borderColorPaletteIndex = MaxBorderColorPaletteSize;

    switch (settings.preciseAnisoMode)
    {
    case EnablePreciseAniso:
        pPalSamplerInfo->flags.preciseAniso = 1;
        break;
    case DisablePreciseAnisoAll:
        pPalSamplerInfo->flags.preciseAniso = 0;
        break;
    case DisablePreciseAnisoAfOnly:
        pPalSamplerInfo->flags.preciseAniso = (anisotropyEnable == VK_FALSE) ? 1 : 0;
        break;
    default:
        break;
    }

    // disableSingleMipAnisoOverride=1 ensure properly sampling with single mipmap level and anisotropic filtering.
    pPalSamplerInfo->flags.disableSingleMipAnisoOverride = settings.disableSingleMipAnisoOverride ? 1 : 0;

    pPalSamplerInfo->flags.useAnisoThreshold        = (settings.useAnisoThreshold == true) ? 1 : 0;
    pPalSamplerInfo->anisoThreshold                 = settings.anisoThreshold;
    pPalSamplerInfo->perfMip                        = settings.samplerPerfMip;
    pPalSamplerInfo->flags.unnormalizedCoords       = (pCreateInfo->unnormalizedCoordinates == VK_TRUE) ? 1 : 0;
    pPalSamplerInfo->flags.prtBlendZeroMode         = 0;
    pPalSamplerInfo->flags.seamlessCubeMapFiltering =
        (pCreateInfo->flags & VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT) ? 0 : 1;
    pPalSamplerInfo->flags.truncateCoords           = ((pCreateInfo->magFilter == VK_FILTER_NEAREST) &&
                                                (pCreateInfo->minFilter == VK_FILTER_NEAREST) &&
                                                (pPalSamplerInfo->compareFunc == Pal::CompareFunc::Never))
                                                ? 1 : 0;

    HandleExtensionStructs(pCreateInfo, pExtStructs);

    if (pExtStructs->pSamplerYcbcrConversionInfo != nullptr)
    {
        Vkgc::SamplerYCbCrConversionMetaData* pYCbCrMetaData = SamplerYcbcrConversion::ObjectFromHandle(
            pExtStructs->pSamplerYcbcrConversionInfo->conversion)->GetMetaData();
        pYCbCrMetaData->word1.lumaFilter = pPalSamplerInfo->filter.minification;

        if (pYCbCrMetaData->word0.forceExplicitReconstruct)
        {
            pPalSamplerInfo->flags.truncateCoords = 0;
        }
    }

    if (pExtStructs->pSamplerReductionModeCreateInfo != nullptr)
    {
        pPalSamplerInfo->filterMode = VkToPalTexFilterMode(pExtStructs->pSamplerReductionModeCreateInfo->reductionMode);
    }
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

// ====================================================================================================================
void Sampler::HandleExtensionStructs(
    const VkSamplerCreateInfo*  pCreateInfo,
    SamplerExtStructs*          pExtStructs)
{
    // Parse the creation info.
    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT:
        {
            pExtStructs->pSamplerReductionModeCreateInfo = static_cast<const VkSamplerReductionModeCreateInfo*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
        {
            pExtStructs->pSamplerYcbcrConversionInfo = static_cast<const VkSamplerYcbcrConversionInfo*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
        {
            pExtStructs->pSamplerCustomBorderColorCreateInfoEXT = static_cast<
                const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT:
        {
            pExtStructs->pSamplerBorderColorComponentMappingCreateInfoEXT = static_cast<
                const VkSamplerBorderColorComponentMappingCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT:
        {
            pExtStructs->pOpaqueCaptureDescriptorDataCreateInfoEXT = static_cast<
                const VkOpaqueCaptureDescriptorDataCreateInfoEXT*>(pNext);
            break;
        }
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }
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
