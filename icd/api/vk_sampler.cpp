/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_object.h"
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
            const VkStructHeader*                      pInfo;
            const VkSamplerYcbcrConversionInfo*        pYcbcrConversionInfo;
            const VkSamplerReductionModeCreateInfoEXT* pReductionModeCreateInfo;
        };

        pInfo = static_cast<const VkStructHeader*>(pCreateInfo->pNext);

        while (pInfo != nullptr)
        {
            switch (static_cast<uint32_t>(pInfo->sType))
            {
            case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
                hasher.Update(pYcbcrConversionInfo->sType);
                SamplerYcbcrConversionMetaData* pSamplerYcbcrConversionMetaData;
                pSamplerYcbcrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(pYcbcrConversionInfo->conversion)->GetMetaData();
                hasher.Update(pSamplerYcbcrConversionMetaData->word0.u32All);
                hasher.Update(pSamplerYcbcrConversionMetaData->word1.u32All);
                hasher.Update(pSamplerYcbcrConversionMetaData->word2.u32All);
                hasher.Update(pSamplerYcbcrConversionMetaData->word3.u32All);

                break;
            case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT:
                hasher.Update(pReductionModeCreateInfo->sType);
                hasher.Update(pReductionModeCreateInfo->reductionMode);

                break;
            default:
                break;
            }

            pInfo = pInfo->pNext;
        }
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t* const>(&hash));

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
    uint64_t         apiHash     = BuildApiHash(pCreateInfo);
    Pal::SamplerInfo samplerInfo = {};
    samplerInfo.filterMode       = Pal::TexFilterMode::Blend;  // Initialize "legacy" behavior
    SamplerYcbcrConversionMetaData* pSamplerYcbcrConversionMetaData;

    union
    {
        const VkStructHeader*                      pHeader;
        const VkSamplerCreateInfo*                 pSamplerInfo;
        const VkSamplerReductionModeCreateInfoEXT* pVkSamplerReductionModeCreateInfoEXT;
        const VkSamplerYcbcrConversionInfo*        pVkSamplerYcbcrConversionInfo;
    };

    // Parse the creation info.
    for (pSamplerInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO:
            samplerInfo.filter   = VkToPalTexFilter(pSamplerInfo->anisotropyEnable,
                                                    pSamplerInfo->magFilter,
                                                    pSamplerInfo->minFilter,
                                                    pSamplerInfo->mipmapMode);
            samplerInfo.addressU = VkToPalTexAddressMode(pSamplerInfo->addressModeU);
            samplerInfo.addressV = VkToPalTexAddressMode(pSamplerInfo->addressModeV);
            samplerInfo.addressW = VkToPalTexAddressMode(pSamplerInfo->addressModeW);

            samplerInfo.mipLodBias      = pSamplerInfo->mipLodBias;

            samplerInfo.maxAnisotropy   = static_cast<uint32_t>(pSamplerInfo->maxAnisotropy);
            samplerInfo.compareFunc     = VkToPalCompareFunc(pSamplerInfo->compareOp);
            samplerInfo.minLod          = pSamplerInfo->minLod;
            samplerInfo.maxLod          = pSamplerInfo->maxLod;
            samplerInfo.borderColorType = VkToPalBorderColorType(pSamplerInfo->borderColor);
            samplerInfo.borderColorPaletteIndex = 0;

            switch (pDevice->GetRuntimeSettings().preciseAnisoMode)
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

            samplerInfo.flags.useAnisoThreshold        = (pDevice->GetRuntimeSettings().useAnisoThreshold == true);
            samplerInfo.anisoThreshold                 = pDevice->GetRuntimeSettings().anisoThreshold;
            samplerInfo.flags.unnormalizedCoords       = (pSamplerInfo->unnormalizedCoordinates == VK_TRUE) ? 1 : 0;
            samplerInfo.flags.prtBlendZeroMode         = 0;
            samplerInfo.flags.seamlessCubeMapFiltering = 1;
            break;
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT:
            samplerInfo.filterMode = VkToPalTexFilterMode(pVkSamplerReductionModeCreateInfoEXT->reductionMode);
            break;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
            pSamplerYcbcrConversionMetaData = SamplerYcbcrConversion::ObjectFromHandle(pVkSamplerYcbcrConversionInfo->conversion)->GetMetaData();
            pSamplerYcbcrConversionMetaData->word1.lumaFilter = samplerInfo.filter.minification;
            break;

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    // Figure out how big a sampler SRD is. This is not the most efficient way of doing
    // things, so we could cache the SRD size.
    Pal::DeviceProperties props;
    pDevice->PalDevice(DefaultDeviceIndex)->GetProperties(&props);

    const uint32_t apiSize = sizeof(Sampler);
    const uint32_t palSize = props.gfxipProperties.srdSizes.sampler;

    // Allocate system memory. Construct the sampler in memory and then wrap a Vulkan
    // object around it.
    void* pMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        apiSize + palSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Create one sampler srd which can be used by any device in the group
    pDevice->PalDevice(DefaultDeviceIndex)->CreateSamplerSrds(
            1,
            &samplerInfo,
            Util::VoidPtrInc(pMemory, apiSize));

    VK_PLACEMENT_NEW (pMemory) Sampler(apiHash);

    *pSampler = Sampler::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// ====================================================================================================================
// Destroy a sampler object
VkResult Sampler::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

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
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Sampler::ObjectFromHandle(sampler)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
