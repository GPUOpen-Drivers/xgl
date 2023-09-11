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
/**
 ***********************************************************************************************************************
 * @file  vk_image.cpp
 * @brief Contains implementation of Vulkan image object.
 ***********************************************************************************************************************
 */

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_formats.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_swapchain.h"
#include "include/vk_queue.h"

#include "palGpuMemory.h"
#include "palImage.h"
#include "palAutoBuffer.h"

#if defined(__unix__)
#include "drm_fourcc.h"
#endif

namespace vk
{

// =====================================================================================================================
// This funtion updates the image sharing mode if suitable forceImageSharingMode setting is applied.
static void UpdateImageSharingMode(
    uint32_t       sharingModeSetting,
    const bool     isColorAttachment,
    VkSharingMode* pImageSharingMode)
{
    if ((sharingModeSetting == ForceImageSharingModeExclusive) ||
        ((sharingModeSetting == ForceImageSharingModeExclusiveForNonColorAttachments) &&
         (isColorAttachment == false)))
    {
        *pImageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
}

// =====================================================================================================================
// Given a runtime priority setting value, this function updates the given priority/offset pair if the setting's
// priority is higher level.
static void UpgradeToHigherPriority(
    uint32_t        prioritySetting,
    MemoryPriority* pPriority)
{
    MemoryPriority newPriority = MemoryPriority::FromSetting(prioritySetting);

    if (*pPriority < newPriority)
    {
        *pPriority = newPriority;
    }
}

// =====================================================================================================================
// Generates a ResourceOptimizerKey object using the contents of the VkImageCreateInfo struct
void Image::BuildResourceKey(
    const VkImageCreateInfo* pCreateInfo,
    ResourceOptimizerKey*    pResourceKey,
    const RuntimeSettings&   settings)
{
    Util::MetroHash64 hasher;
    VkSharingMode     imageSharingMode  = pCreateInfo->sharingMode;
    const bool        isColorAttachment = (pCreateInfo->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    hasher.Update(pCreateInfo->flags);
    hasher.Update(pCreateInfo->imageType);
    hasher.Update(pCreateInfo->format);
    hasher.Update(pCreateInfo->extent.depth);
    hasher.Update(pCreateInfo->mipLevels);
    hasher.Update(pCreateInfo->arrayLayers);
    hasher.Update(pCreateInfo->samples);
    hasher.Update(pCreateInfo->tiling);
    hasher.Update(pCreateInfo->usage);

    // We don't want resource keys based on runtime settings (here, forceImageSharingMode), in general
    // for app profiles. Temporarily, modify the hash only for apps Which have DCC optimizations added.
    if (settings.modifyResourceKeyForAppProfile)
    {
        UpdateImageSharingMode(settings.forceImageSharingMode, isColorAttachment, &imageSharingMode);
    }

    hasher.Update(imageSharingMode);
    hasher.Update(pCreateInfo->queueFamilyIndexCount);
    hasher.Update(pCreateInfo->initialLayout);

    if (pCreateInfo->pQueueFamilyIndices != nullptr)
    {
        hasher.Update(
            reinterpret_cast<const uint8_t*>(pCreateInfo->pQueueFamilyIndices),
            pCreateInfo->queueFamilyIndexCount * sizeof(uint32_t));
    }

    hasher.Finalize(reinterpret_cast<uint8_t*>(&pResourceKey->apiHash));

    pResourceKey->width = pCreateInfo->extent.width;
    pResourceKey->height = pCreateInfo->extent.height;
}

// =====================================================================================================================
// Computes the priority level of this image based on its usage.
void Image::CalcMemoryPriority(
    const Device* pDevice)
{
    const auto& settings = pDevice->GetRuntimeSettings();

    m_priority = MemoryPriority::FromSetting(settings.memoryPriorityDefault);

    if (pDevice->GetEnabledFeatures().appControlledMemPriority == false)
    {
        UpgradeToHigherPriority(settings.memoryPriorityImageAny, &m_priority);

        if (GetBarrierPolicy().GetSupportedLayoutUsageMask() &
            (Pal::LayoutShaderRead | Pal::LayoutShaderFmaskBasedRead))
        {
            UpgradeToHigherPriority(settings.memoryPriorityImageShaderRead, &m_priority);
        }

        if (GetBarrierPolicy().GetSupportedLayoutUsageMask() & Pal::LayoutShaderWrite)
        {
            UpgradeToHigherPriority(settings.memoryPriorityImageShaderWrite, &m_priority);
        }

        if (GetBarrierPolicy().GetSupportedLayoutUsageMask() & Pal::LayoutColorTarget)
        {
            UpgradeToHigherPriority(settings.memoryPriorityImageColorTarget, &m_priority);
        }

        if (GetBarrierPolicy().GetSupportedLayoutUsageMask() & Pal::LayoutDepthStencilTarget)
        {
            UpgradeToHigherPriority(settings.memoryPriorityImageDepthStencil, &m_priority);
        }
    }
}

// =====================================================================================================================
Image::Image(
    Device*                      pDevice,
    const VkImageCreateInfo*     pCreateInfo,
    Pal::IImage**                pPalImages,
    Pal::IGpuMemory**            pPalMemory,
    VkSharingMode                sharingMode,
    uint32_t                     extraLayoutUsages,
    VkExtent3D                   tileSize,
    uint32_t                     mipLevels,
    uint32_t                     arraySize,
    VkFormat                     imageFormat,
    VkImageUsageFlags            stencilUsage,
    ImageFlags                   internalFlags,
    const ResourceOptimizerKey&  resourceKey)
    :
    m_mipLevels(mipLevels),
    m_arraySize(arraySize),
    m_format(imageFormat),
    m_srgbFormat(VK_FORMAT_UNDEFINED),
    m_imageSamples(pCreateInfo->samples),
    m_imageUsage(pCreateInfo->usage),
    m_imageType(pCreateInfo->imageType),
    m_imageStencilUsage(stencilUsage),
    m_tileSize(tileSize),
    m_barrierPolicy(
        pDevice,
        pCreateInfo->usage | stencilUsage,
        sharingMode,
        pCreateInfo->queueFamilyIndexCount,
        pCreateInfo->pQueueFamilyIndices,
        pCreateInfo->samples > VK_SAMPLE_COUNT_1_BIT,
        imageFormat,
        extraLayoutUsages),
    m_pSwapChain(nullptr),
    m_ResourceKey(resourceKey),
    m_memoryRequirements{}
{
    m_internalFlags.u32All = internalFlags.u32All;

    for (uint32_t devIdx = 0; devIdx < pDevice->NumPalDevices(); devIdx++)
    {
        m_perGpu[devIdx].pPalImage      = pPalImages[devIdx];
        m_perGpu[devIdx].pPalMemory     = (pPalMemory != nullptr) ? pPalMemory[devIdx] : nullptr;
        m_perGpu[devIdx].baseAddrOffset = 0;
    }

    CalcMemoryPriority(pDevice);
}

// =====================================================================================================================
void Image::ConvertImageCreateInfo(
    Device*                      pDevice,
    const VkImageCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    const ImageExtStructs&       extStructs,
    const ResourceOptimizerKey&  resourceKey,
    Pal::ImageCreateInfo*        pPalCreateInfo)
{
    VkImageUsageFlags            imageUsage       = pCreateInfo->usage;
    const RuntimeSettings&       settings         = pDevice->GetRuntimeSettings();
    const Pal::DeviceProperties& palProperties    = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
    VkFormat                     createInfoFormat = GetCreateInfoFormat(pCreateInfo, extStructs);

    // VK_IMAGE_CREATE_EXTENDED_USAGE_BIT indicates that the image can be created with usage flags that are not
    // supported for the format the image is created with but are supported for at least one format a VkImageView
    // created from the image can have.  For PAL, restrict the usage to only those supported for this format and set
    // formatChangeSrd and formatChangeTgt flags to handle the other usages.  This image will still contain the superset
    // of the usages to makes sure barriers properly handle each.
    if ((pCreateInfo->flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0)
    {
        Pal::MergedFormatPropertiesTable fmtProperties = {};
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalDevice()->GetFormatProperties(&fmtProperties);

        const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(createInfoFormat, settings);

        const size_t formatIdx = static_cast<size_t>(swizzledFormat.format);
        const size_t tilingIdx = ((pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR) ? Pal::IsLinear : Pal::IsNonLinear);

        const VkFormatFeatureFlags flags = PalToVkFormatFeatureFlags(fmtProperties.features[formatIdx][tilingIdx]);
        imageUsage &= VkFormatFeatureFlagsToImageUsageFlags(flags);
    }

    memset(pPalCreateInfo, 0, sizeof(*pPalCreateInfo));

    pPalCreateInfo->extent.width     = pCreateInfo->extent.width;
    pPalCreateInfo->extent.height    = pCreateInfo->extent.height;

    pPalCreateInfo->extent.depth      = pCreateInfo->extent.depth;
    pPalCreateInfo->imageType         = VkToPalImageType(pCreateInfo->imageType);
    pPalCreateInfo->swizzledFormat    = VkToPalFormat(createInfoFormat, settings);
    pPalCreateInfo->mipLevels         = pCreateInfo->mipLevels;
    pPalCreateInfo->arraySize         = pCreateInfo->arrayLayers;
    pPalCreateInfo->samples           = pCreateInfo->samples;
    pPalCreateInfo->fragments         = pCreateInfo->samples;
    pPalCreateInfo->tiling            = VkToPalImageTiling(pCreateInfo->tiling);
    pPalCreateInfo->tilingOptMode     = pDevice->GetTilingOptMode();
    pPalCreateInfo->imageMemoryBudget = settings.imageMemoryBudget;

    if ((pCreateInfo->imageType == VK_IMAGE_TYPE_3D) &&
        (pCreateInfo->usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT)))
    {
        pPalCreateInfo->tilingPreference = settings.imageTilingPreference3dGpuWritable;
    }
    else
    {
        pPalCreateInfo->tilingPreference = settings.imageTilingPreference;
    }

    pPalCreateInfo->flags.u32All     = VkToPalImageCreateFlags(pCreateInfo->flags, createInfoFormat, imageUsage);
    pPalCreateInfo->usageFlags       = VkToPalImageUsageFlags(
                                         imageUsage,
                                         pCreateInfo->samples,
                                         (VkImageUsageFlags)(settings.optImgMaskToApplyShaderReadUsageForTransferSrc),
                                         (VkImageUsageFlags)(settings.optImgMaskToApplyShaderWriteUsageForTransferDst));

    if (((pCreateInfo->flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0) && (settings.ignoreMutableFlag == false))
    {
        // Set viewFormatCount to Pal::AllCompatibleFormats to indicate that all compatible formats can be used for
        // image views created from the image. This gets overridden later if VK_KHR_image_format_list is used.
        pPalCreateInfo->viewFormatCount = Pal::AllCompatibleFormats;
    }

    // Vulkan allows individual subresources to be transitioned from uninitialized layout which means we
    // have to set this bit for PAL to be able to support this.  This may have performance implications
    // regarding DCC.
    pPalCreateInfo->flags.perSubresInit = 1;

    if (extStructs.pExternalMemoryImageCreateInfo != nullptr)
    {
        pPalCreateInfo->flags.invariant        = 1;
        pPalCreateInfo->flags.optimalShareable = 1;
    }

    if (((pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) == 0)            &&
        ((pCreateInfo->flags & VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT) != 0) &&
        (pCreateInfo->mipLevels > 1)                                                  &&
        Pal::Formats::IsBlockCompressed(pPalCreateInfo->swizzledFormat.format)        &&
        (palProperties.gfxLevel > Pal::GfxIpLevel::GfxIp9)                            &&
        (pCreateInfo->imageType == VK_IMAGE_TYPE_3D))
    {
        pPalCreateInfo->flags.view3dAs2dArray = 1;
    }

    ExternalMemoryFlags externalFlags;
    externalFlags.u32All = 0;

    GetExternalMemoryFlags(pDevice,
                           extStructs,
                           ((pCreateInfo->flags & SparseEnablingFlags) != 0),
                           &externalFlags);

    // When the VK image is shareable, the depthStencil PAL usage flag must be set in order for the underlying
    // surface to be depth/stencil (and not color). Otherwise, the image cannot be shared with OpenGL. Core
    // OpenGL does not allow for texture usage to be specified, thus all textures with a depth/stencil aspect
    // result in depth/stencil surfaces.
    if (Formats::IsDepthStencilFormat(createInfoFormat) &&
        externalFlags.externallyShareable &&
        (externalFlags.externalD3DHandle == false))
    {
        pPalCreateInfo->usageFlags.depthStencil = true;
    }

    // It indicates the stencil aspect will be read by shader, so it is only meaningful if the image contains the
    // stencil aspect. The setting of stencilShaderRead will be overridden, if
    // VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT exists.
    bool stencilShaderRead = pPalCreateInfo->usageFlags.shaderRead | pPalCreateInfo->usageFlags.resolveSrc;

    if (extStructs.pImageStencilUsageCreateInfo != nullptr)
    {
        Pal::ImageUsageFlags usageFlags = VkToPalImageUsageFlags(
                                    extStructs.pImageStencilUsageCreateInfo->stencilUsage,
                                    pCreateInfo->samples,
                                    (VkImageUsageFlags)(settings.optImgMaskToApplyShaderReadUsageForTransferSrc),
                                    (VkImageUsageFlags)(settings.optImgMaskToApplyShaderWriteUsageForTransferDst));

        pPalCreateInfo->usageFlags.u32All |= usageFlags.u32All;
    }

    // Configure the noStencilShaderRead:
    // 1. Set noStencilShaderRead = false by default, this indicates the stencil can be read by shader.
    // 2. Overwrite noStencilShaderRead according to the stencilUsage.
    // 3. Set noStencilShaderRead = true according to application profile.

    pPalCreateInfo->usageFlags.noStencilShaderRead = false;

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SEPARATE_STENCIL_USAGE))
    {
        pPalCreateInfo->usageFlags.noStencilShaderRead = (stencilShaderRead == false);
    }

    // Disable Stencil read according to the application profile during the creation of an MSAA depth stencil target.
    if ((pCreateInfo->samples > VK_SAMPLE_COUNT_1_BIT)                            &&
        ((pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) &&
        settings.disableMsaaStencilShaderRead)
    {
        pPalCreateInfo->usageFlags.noStencilShaderRead = true;
    }

    if ((extStructs.pImageFormatListCreateInfo != nullptr) &&
        (extStructs.pImageFormatListCreateInfo->viewFormatCount > 0))
    {
        Pal::SwizzledFormat* pPalFormatList = static_cast<Pal::SwizzledFormat*>(pAllocator->pfnAllocation(
            pAllocator->pUserData,
            (extStructs.pImageFormatListCreateInfo->viewFormatCount * sizeof(Pal::SwizzledFormat)),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));

        VK_ASSERT(pPalFormatList != nullptr);

        if (pPalFormatList != nullptr)
        {
            pPalCreateInfo->viewFormatCount = 0;
            pPalCreateInfo->pViewFormats    = pPalFormatList;

            for (uint32_t i = 0; i < extStructs.pImageFormatListCreateInfo->viewFormatCount; ++i)
            {
                // Skip any entries that specify the same format as the base format of the image as the PAL interface
                // expects that to be excluded from the list.
                if (VkToPalFormat(extStructs.pImageFormatListCreateInfo->pViewFormats[i], settings).format !=
                    VkToPalFormat(createInfoFormat, settings).format)
                {
                    pPalFormatList[pPalCreateInfo->viewFormatCount++] =
                        VkToPalFormat(extStructs.pImageFormatListCreateInfo->pViewFormats[i], settings);
                }
            }
        }
    }

    // Enable fullCopyDstOnly for MSAA color image with usage of transfer dst to maximize the texture copy performance.
    if ((pCreateInfo->samples > VK_SAMPLE_COUNT_1_BIT)                    &&
        ((pCreateInfo->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) &&
        ((pCreateInfo->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)     &&
        settings.enableFullCopyDstOnly)
    {
        pPalCreateInfo->flags.fullCopyDstOnly = 1;
    }

    if (pDevice->GetEnabledFeatures().attachmentFragmentShadingRate)
    {
        // Any depth buffer could potentially be used while VRS is active.
        if ((pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            pPalCreateInfo->usageFlags.vrsDepth = 1;
        }

        if ((pCreateInfo->usage & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR) != 0)
        {
            pPalCreateInfo->usageFlags.vrsRateImage = 1;
        }
    }

    pPalCreateInfo->metadataMode         = Pal::MetadataMode::Default;
    pPalCreateInfo->metadataTcCompatMode = Pal::MetadataTcCompatMode::Default;

    // Don't force DCC to be enabled for performance reasons unless the image is larger than the minimum size set for
    // compression, another performance optimization.
    // Don't force DCC to be enabled for shader write image on pre-gfx10 ASICs as DCC is unsupported in shader write.
    const Pal::GfxIpLevel gfxLevel = palProperties.gfxLevel;
    if (((pPalCreateInfo->extent.width * pPalCreateInfo->extent.height) >
         (settings.disableSmallSurfColorCompressionSize * settings.disableSmallSurfColorCompressionSize)) &&
        (Formats::IsColorFormat(createInfoFormat)) &&
        ((gfxLevel > Pal::GfxIpLevel::GfxIp9) || (pPalCreateInfo->usageFlags.shaderWrite == false)))
    {
        const uint32_t forceEnableDccMask = settings.forceEnableDcc;

        const uint32_t bpp         = Pal::Formats::BitsPerPixel(pPalCreateInfo->swizzledFormat.format);
        const bool isShaderStorage = (pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT);

        if (isShaderStorage &&
            ((forceEnableDccMask & ForceDccDefault) == 0) &&
            ((forceEnableDccMask & ForceDisableDcc) == 0))
        {
            const bool isColorAttachment = (pCreateInfo->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

            const bool is2DShaderStorageImage = (pCreateInfo->imageType & VK_IMAGE_TYPE_2D);
            const bool is3DShaderStorageImage = (pCreateInfo->imageType & VK_IMAGE_TYPE_3D);

            // Enable DCC beyond what PAL does by default for color attachments
            const bool shouldForceDccForCA = Util::TestAnyFlagSet(forceEnableDccMask, ForceDccForColorAttachments) &&
                                             isColorAttachment;
            const bool shouldForceDccForNonCAShaderStorage =
                Util::TestAnyFlagSet(forceEnableDccMask, ForceDccForNonColorAttachmentShaderStorage) &&
                (!isColorAttachment);

            const bool shouldForceDccFor2D = Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor2DShaderStorage) &&
                                             is2DShaderStorageImage;
            const bool shouldForceDccFor3D = Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor3DShaderStorage) &&
                                             is3DShaderStorageImage;

            const bool shouldForceDccFor32Bpp =
                Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor32BppShaderStorage) && (bpp >= 32) && (bpp < 64);

            const bool shouldForceDccFor64Bpp =
                Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor64BppShaderStorage) && (bpp >= 64);

            const bool shouldForceDccForAllBpp =
            ((Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor32BppShaderStorage) == false) &&
                (Util::TestAnyFlagSet(forceEnableDccMask, ForceDccFor64BppShaderStorage) == false));

            // To force enable shader storage DCC, at least one of 2D/3D and one of CA/non-CA need to be set
            if ((shouldForceDccFor2D || shouldForceDccFor3D) &&
                (shouldForceDccForCA || shouldForceDccForNonCAShaderStorage) &&
                (shouldForceDccFor32Bpp || shouldForceDccFor64Bpp || shouldForceDccForAllBpp))
            {
                pPalCreateInfo->metadataMode = Pal::MetadataMode::ForceEnabled;
            }
        }

        // This setting should only really be used for Vega20.
        // Turn DCC on/off for identified cases where memory bandwidth is not the bottleneck to improve latency.
        // PAL may do this implicitly, so specify force enabled instead of default.
        if (settings.dccBitsPerPixelThreshold != UINT_MAX)
        {
            pPalCreateInfo->metadataMode = (bpp < settings.dccBitsPerPixelThreshold) ?
                Pal::MetadataMode::Disabled : Pal::MetadataMode::ForceEnabled;
        }
    }

    // a. If devs don't enable the extension: can keep DCC enabled for UAVs with mips
    // b. If dev enables the extension: keep DCC enabled for UAVs with <= 4 mips
    // c. Can app-detect un-disable DCC for cases where we know devs don't store to multiple mips
    if ((gfxLevel == Pal::GfxIpLevel::GfxIp10_1) &&
        pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_IMAGE_LOAD_STORE_LOD) &&
        (pCreateInfo->mipLevels > 4) && (pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT))
    {
        pPalCreateInfo->metadataMode = Pal::MetadataMode::Disabled;
    }

    // If DCC was disabled above, still attempt to use Fmask.
    if ((pPalCreateInfo->samples > 1) && pPalCreateInfo->usageFlags.colorTarget &&
        (pPalCreateInfo->metadataMode == Pal::MetadataMode::Disabled))
    {
        pPalCreateInfo->metadataMode = Pal::MetadataMode::FmaskOnly;
    }

    // Disable TC compatible reads in order to maximize texture fetch performance.
    if ((pCreateInfo->samples > VK_SAMPLE_COUNT_1_BIT)                            &&
        ((pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) &&
        (settings.disableHtileBasedMsaaRead))
    {
        pPalCreateInfo->metadataTcCompatMode = Pal::MetadataTcCompatMode::Disabled;
    }

    // We must not use any metadata if sparse aliasing is enabled or
    // settings.forceEnableDcc is equal to ForceDisableDcc.
    if ((pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT) ||
        ((settings.forceEnableDcc & ForceDisableDcc) != 0))
    {
        pPalCreateInfo->metadataMode = Pal::MetadataMode::Disabled;
    }

    // Disable metadata for sharable images if the ForceDisableDccForSharedImages
    // flag is set in settings.forceEnableDcc
    if (externalFlags.externallyShareable &&
        ((settings.forceEnableDcc & ForceDisableDccForSharedImages) != 0))
    {
        pPalCreateInfo->metadataMode = Pal::MetadataMode::Disabled;
    }

    // Disable metadata for avoiding corruption if one image is sampled and rendered
    // in the same draw.
    if ((pCreateInfo->usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT) != 0)
    {
        pPalCreateInfo->metadataMode = Pal::MetadataMode::Disabled;
    }

    // Apply per application (or run-time) options
    pDevice->GetResourceOptimizer()->OverrideImageCreateInfo(resourceKey, pPalCreateInfo);

#if defined(__unix__)
    pPalCreateInfo->modifier = DRM_FORMAT_MOD_INVALID;

    if (pCreateInfo->tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
    {
        if ((extStructs.pModifierListCreateInfo != nullptr) &&
            (extStructs.pModifierListCreateInfo->drmFormatModifierCount > 0))
        {
            VK_ASSERT(extStructs.pModifierExplicitCreateInfo == nullptr);
        }
        else if ((extStructs.pModifierExplicitCreateInfo != nullptr)                       &&
                 (extStructs.pModifierExplicitCreateInfo->drmFormatModifierPlaneCount > 0) &&
                 (extStructs.pModifierExplicitCreateInfo->drmFormatModifier != DRM_FORMAT_MOD_INVALID))
        {
            VK_ASSERT(extStructs.pModifierListCreateInfo == nullptr);

            // Check if the explicit modifier is in the available list.
            GetPreferableModifier(pDevice,
                                  pAllocator,
                                  createInfoFormat,
                                  1,
                                  &extStructs.pModifierExplicitCreateInfo->drmFormatModifier,
                                  pPalCreateInfo);

            if (pPalCreateInfo->flags.hasModifier != 0)
            {
                pPalCreateInfo->modifierPlaneCount =
                    extStructs.pModifierExplicitCreateInfo->drmFormatModifierPlaneCount;

                for (uint32 i = 0; i < extStructs.pModifierExplicitCreateInfo->drmFormatModifierPlaneCount; i++)
                {
                    pPalCreateInfo->modifierMemoryPlaneOffset[i] =
                        extStructs.pModifierExplicitCreateInfo->pPlaneLayouts[i].offset;
                }
            }
        }
        else
        {
            VK_NEVER_CALLED();
        }
    }
    else
    {
        VK_ASSERT((extStructs.pModifierListCreateInfo == nullptr) &&
                  (extStructs.pModifierExplicitCreateInfo == nullptr));
    }
#endif
}

// =====================================================================================================================
// Creates virtual memory allocation for sparse images.
static VkResult InitSparseVirtualMemory(
    Device*                         pDevice,
    const VkImageCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    Pal::IImage*                    pPalImage[MaxPalDevices],
    Pal::IGpuMemory*                pSparseMemory[MaxPalDevices],
    VkExtent3D*                     pSparseTileSize)
{
    VkResult result = VK_SUCCESS;

    Pal::GpuMemoryCreateInfo sparseMemCreateInfo = {};
    Pal::GpuMemoryRequirements palReqs = {};

    pPalImage[DefaultDeviceIndex]->GetGpuMemoryRequirements(&palReqs);

    // We need virtual remapping support for all sparse resources
    VK_ASSERT(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsVirtualRemappingSupported());

    const VkDeviceSize sparseAllocGranularity = pDevice->GetProperties().virtualMemAllocGranularity;

    sparseMemCreateInfo.flags.globalGpuVa  = pDevice->IsGlobalGpuVaEnabled();
    sparseMemCreateInfo.flags.virtualAlloc = 1;
    sparseMemCreateInfo.flags.cpuInvisible = (palReqs.flags.cpuAccess ? 0 : 1);
    sparseMemCreateInfo.alignment          = Util::RoundUpToMultiple(sparseAllocGranularity, palReqs.alignment);
    sparseMemCreateInfo.size               = Util::RoundUpToMultiple(palReqs.size, sparseMemCreateInfo.alignment);
    sparseMemCreateInfo.heapCount          = 0;
    sparseMemCreateInfo.heapAccess         = Pal::GpuHeapAccess::GpuHeapAccessExplicit;

#if defined(__unix__)
    sparseMemCreateInfo.flags.initializeToZero = pDevice->GetRuntimeSettings().initializeVramToZero;
#endif

    // Virtual resource should return 0 on unmapped read if residencyNonResidentStrict is set.
    if (pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
    {
        sparseMemCreateInfo.virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
    }

    // If it's a sparse image we should also cache sparse image block dimensions (tile size) to
    // optimize sparse binding update, keeping in mind that each supported aspect (color, depth,
    // stencil) is permitted to use different granularity
    uint32_t                      propertyCount = 1;
    VkSparseImageFormatProperties sparseFormatProperties;

    pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetSparseImageFormatProperties(
        pCreateInfo->format,
        pCreateInfo->imageType,
        pCreateInfo->samples,
        pCreateInfo->usage,
        pCreateInfo->tiling,
        &propertyCount,
        utils::ArrayView<VkSparseImageFormatProperties>(&sparseFormatProperties));

    *pSparseTileSize = sparseFormatProperties.imageGranularity;

    Pal::Result palResult;

    size_t palMemSize = pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemorySize(sparseMemCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    void* pPalMemoryObj = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            (palMemSize * pDevice->NumPalDevices()),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pPalMemoryObj != nullptr)
    {
        size_t palMemOffset = 0;

        for (uint32_t deviceIdx = 0;
            (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
            deviceIdx++)
        {
            if (deviceIdx != DefaultDeviceIndex)
            {
                Pal::GpuMemoryRequirements deviceReqs = {};
                pPalImage[deviceIdx]->GetGpuMemoryRequirements(&deviceReqs);
                VK_ASSERT(memcmp(&palReqs, &deviceReqs, sizeof(deviceReqs)) == 0);

                VK_ASSERT(palMemSize ==
                          pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(sparseMemCreateInfo, &palResult));
                VK_ASSERT(palResult == Pal::Result::Success);
            }

            palResult = pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
                sparseMemCreateInfo,
                Util::VoidPtrInc(pPalMemoryObj, palMemOffset),
                &pSparseMemory[deviceIdx]);

            if (palResult == Pal::Result::Success)
            {
                palResult = pPalImage[deviceIdx]->BindGpuMemory(pSparseMemory[deviceIdx], 0);
            }

            palMemOffset += palMemSize;
        }

        result = PalToVkResult(palResult);
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
// Create a new PAL image object (internal function)
VkResult Image::CreateImageInternal(
    Device*                         pDevice,
    Pal::ImageCreateInfo*           pPalCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    Pal::IImage**                   pPalImage)
{
    VkResult     result = VkResult::VK_SUCCESS;
    void*        pMemory = nullptr;
    Pal::Result  palResult = Pal::Result::Success;

    // Calculate required system memory size
    const size_t palImgSize = pDevice->PalDevice(DefaultDeviceIndex)->GetImageSize(*pPalCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    // Allocate system memory for objects
    pMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        palImgSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    // Create PAL image
    if (pMemory != nullptr)
    {
        void* pPalImgAddr = Util::VoidPtrInc(pMemory, 0);

        palResult = pDevice->PalDevice(DefaultDeviceIndex)->CreateImage(
            *pPalCreateInfo,
            Util::VoidPtrInc(pPalImgAddr, 0),
            pPalImage);

        if (palResult != Pal::Result::Success)
        {
            // Failure in creating the PAL image object. Free system memory and return error.
            pAllocator->pfnFree(pAllocator->pUserData, pMemory);
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
// Traverse the VkImageCreateInfo's pNext chain and save pointers to the structures provided for processing later
void Image::HandleExtensionStructs(
    const VkImageCreateInfo* pCreateInfo,
    ImageExtStructs*         pExtStructs)
{
    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
        {
            pExtStructs->pExternalMemoryImageCreateInfo = static_cast<const VkExternalMemoryImageCreateInfo*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR:
        {
            // Nothing to do. BindSwapchainMemory has access to the swapchain and reinitializes based on it.
            // Some of that could be pulled here, but validation is needed to be sure the same swapchain is provided or
            // else reinitialization would be required anyway.
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
        {
            pExtStructs->pImageFormatListCreateInfo = static_cast<const VkImageFormatListCreateInfo*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
        {
            pExtStructs->pImageStencilUsageCreateInfo = static_cast<const VkImageStencilUsageCreateInfo*>(pNext);
            break;
        }
#if defined(__unix__)
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:
        {
            pExtStructs->pModifierListCreateInfo =
                static_cast<const VkImageDrmFormatModifierListCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:
        {
            pExtStructs->pModifierExplicitCreateInfo =
                static_cast<const VkImageDrmFormatModifierExplicitCreateInfoEXT*>(pNext);
            break;
        }
#endif
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
// Set ImageFlags flags during image creation
void Image::SetCommonFlags(
    const VkImageCreateInfo* pCreateInfo,
    const VkFormat           imageFormat,
    ImageFlags*              pImageFlags)
{
    // Set hasDepth and hasStencil flags based on the image's format.
    if (Formats::IsColorFormat(imageFormat))
    {
        pImageFlags->isColorFormat = 1;
    }
    if (Formats::HasDepth(imageFormat))
    {
        pImageFlags->hasDepth = 1;
    }
    if (Formats::HasStencil(imageFormat))
    {
        pImageFlags->hasStencil = 1;
    }
    if (Formats::IsYuvFormat(imageFormat))
    {
        pImageFlags->isYuvFormat = 1;
    }
    if (pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
    {
        pImageFlags->sparseBinding = 1;
    }
    if (pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
    {
        pImageFlags->sparseResidency = 1;
    }
    if (pCreateInfo->flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT)
    {
        pImageFlags->is2DArrayCompat = 1;
    }
    if (pCreateInfo->flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT)
    {
        pImageFlags->sampleLocsCompatDepth = 1;
    }
}

// =====================================================================================================================
// Get the image format from the VkImageCreateInfo
VkFormat Image::GetCreateInfoFormat(
    const VkImageCreateInfo* pCreateInfo,
    const ImageExtStructs&   extStructs)
{
    VkFormat format = pCreateInfo->format;

    return format;
}

// =====================================================================================================================
// Get the external memory flags from VkExternalMemoryImageCreateInfo, chained off the image create info's pNext
void Image::GetExternalMemoryFlags(
    const Device*          pDevice,
    const ImageExtStructs& extStructs,
    const bool             isSparse,
    ExternalMemoryFlags*   pFlags)
{
    if (extStructs.pExternalMemoryImageCreateInfo != nullptr)
    {
        VkExternalMemoryProperties externalMemoryProperties = {};

        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetExternalMemoryProperties(
            isSparse,
            true,
            static_cast<VkExternalMemoryHandleTypeFlagBitsKHR>(extStructs.pExternalMemoryImageCreateInfo->handleTypes),
            &externalMemoryProperties);

        if (externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
        {
            pFlags->dedicatedRequired = true;
        }

        if (externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT |
                                                               VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        {
            pFlags->externallyShareable = true;

            if ((extStructs.pExternalMemoryImageCreateInfo->handleTypes &
                (VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT     |
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT |
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT        |
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT)) != 0)
            {
                pFlags->externalD3DHandle = true;
            }

            if ((extStructs.pExternalMemoryImageCreateInfo->handleTypes &
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT) != 0)
            {
                pFlags->externalPinnedHost = true;
            }
        }
    }
}

// =====================================================================================================================
// Create a new image object
VkResult Image::Create(
    Device*                         pDevice,
    const VkImageCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkImage*                        pImage)
{
    ImageExtStructs        extStructs    = {};
    Pal::ImageCreateInfo   palCreateInfo = {};
    const RuntimeSettings& settings      = pDevice->GetRuntimeSettings();
    ResourceOptimizerKey   resourceKey;

    BuildResourceKey(pCreateInfo, &resourceKey, settings);

    HandleExtensionStructs(pCreateInfo, &extStructs);

    ConvertImageCreateInfo(pDevice, pCreateInfo, pAllocator, extStructs, resourceKey, &palCreateInfo);

    VkFormat               createInfoFormat      = GetCreateInfoFormat(pCreateInfo, extStructs);
    VkSharingMode          imageSharingMode      = pCreateInfo->sharingMode;
    const uint32_t         numDevices            = pDevice->NumPalDevices();
    const bool             isSparse              = (pCreateInfo->flags & SparseEnablingFlags) != 0;
    VkResult               result                = VK_SUCCESS;
    ImageFlags             imageFlags;

    imageFlags.u32All = 0;

    VkImageUsageFlags stencilUsage = pCreateInfo->usage;

#if defined(__unix__)
    result = ValidateModifierInfo(pDevice, createInfoFormat, extStructs, &palCreateInfo);
#endif

    if ((pCreateInfo->flags & VK_IMAGE_CREATE_PROTECTED_BIT) != 0)
    {
        imageFlags.isProtected = true;
    }

    imageFlags.internalMemBound = isSparse;

    ExternalMemoryFlags externalFlags;
    externalFlags.u32All = 0;

    GetExternalMemoryFlags(pDevice, extStructs, isSparse, &externalFlags);

    imageFlags.dedicatedRequired   = externalFlags.dedicatedRequired;
    imageFlags.externallyShareable = externalFlags.externallyShareable;
    imageFlags.externalD3DHandle   = externalFlags.externalD3DHandle;
    imageFlags.externalPinnedHost  = externalFlags.externalPinnedHost;

    if (extStructs.pImageStencilUsageCreateInfo != nullptr)
    {
        stencilUsage = extStructs.pImageStencilUsageCreateInfo->stencilUsage;
    }

    // If flags contains VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, imageType must be VK_IMAGE_TYPE_3D
    VK_ASSERT(((pCreateInfo->flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) == 0) ||
              (pCreateInfo->imageType == VK_IMAGE_TYPE_3D));

    // Fail image creation if the sample count is not supported based on the setting
    if ((result == VK_SUCCESS) && (settings.limitSampleCounts & pCreateInfo->samples) == 0)
    {
        result = VK_ERROR_UNKNOWN;
    }

    // Override image sharing mode if suitable settings are applied
    UpdateImageSharingMode(
        settings.forceImageSharingMode,
        (pCreateInfo->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        &imageSharingMode);

    SetCommonFlags(pCreateInfo, createInfoFormat, &imageFlags);

    // Calculate required system memory size
    const size_t apiSize   = ObjectSize(pDevice);
    size_t       totalSize = apiSize;
    void*        pMemory   = nullptr;
    Pal::Result  palResult = Pal::Result::Success;

    const size_t palImgSize = pDevice->PalDevice(DefaultDeviceIndex)->GetImageSize(palCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    if (result == VK_SUCCESS)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
        {
            VK_ASSERT(palImgSize == pDevice->PalDevice(deviceIdx)->GetImageSize(palCreateInfo, &palResult));
            VK_ASSERT(palResult  == Pal::Result::Success);
        }

        totalSize += (palImgSize * numDevices);

        // Allocate system memory for objects
        if (result == VK_SUCCESS)
        {
            pMemory = pDevice->AllocApiObject(pAllocator, totalSize);

            if (pMemory == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }

    // Create PAL images
    Pal::IImage* pPalImages[MaxPalDevices] = {};
    void*        pPalImgAddr  = Util::VoidPtrInc(pMemory, apiSize);
    size_t       palImgOffset = 0;

    if (result == VK_SUCCESS)
    {
        for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < pDevice->NumPalDevices()); deviceIdx++)
        {
#if defined(__unix__)
            if (extStructs.pModifierListCreateInfo != nullptr)
            {
                result = CreateImageWithModifierList(
                    pDevice,
                    pAllocator,
                    createInfoFormat,
                    extStructs,
                    Util::VoidPtrInc(pPalImgAddr, palImgOffset),
                    deviceIdx,
                    &palCreateInfo,
                    &pPalImages[deviceIdx]);
            }
            else
#endif
            {
                palResult = pDevice->PalDevice(deviceIdx)->CreateImage(
                    palCreateInfo,
                    Util::VoidPtrInc(pPalImgAddr, palImgOffset),
                    &pPalImages[deviceIdx]);
                VK_ASSERT(palResult == Pal::Result::Success);

                if (palResult != Pal::Result::Success)
                {
                    result = VK_ERROR_INITIALIZATION_FAILED;
                }
            }

            palImgOffset += palImgSize;
        }
    }

    // Create PAL memory if needed.  For sparse images we have to create a virtual-only memory object and bind it to
    // the image.  This will be used to configure the sparse mapping of the image to actual physical memory.
    //
    // NOTE: We cannot glue this object to the memory block we've already allocated and stored in
    //       pMemory, as the value returned by GetGpuMemorySize() depends on memCreateInfo.size,
    //       which means we need a working PAL Image instance before we can find out how much memory
    //       we actually need to allocate for the mem object.
    Pal::IGpuMemory* pSparseMemory[MaxPalDevices]  = {};
    VkExtent3D sparseTileSize                      = {};

    if ((result == VK_SUCCESS) && isSparse)
    {
        result = InitSparseVirtualMemory(pDevice, pCreateInfo, pAllocator, pPalImages, pSparseMemory, &sparseTileSize);
    }

    if (result == VK_SUCCESS)
    {
        // Construct API image object.
        VK_PLACEMENT_NEW (pMemory) Image(
            pDevice,
            pCreateInfo,
            pPalImages,
            pSparseMemory,
            imageSharingMode,
            0,
            sparseTileSize,
            palCreateInfo.mipLevels,
            palCreateInfo.arraySize,
            createInfoFormat,
            stencilUsage,
            imageFlags,
            resourceKey);

        *pImage = Image::HandleFromVoidPointer(pMemory);
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
        {
            if (pSparseMemory[deviceIdx] != nullptr)
            {
                pSparseMemory[deviceIdx]->Destroy();
            }

            if (pPalImages[deviceIdx] != nullptr)
            {
                pPalImages[deviceIdx]->Destroy();
            }
        }

        // Failure in creating the PAL image object. Free system memory and return error.
        pDevice->FreeApiObject(pAllocator, pMemory);
    }

    if (palCreateInfo.pViewFormats != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, const_cast<Pal::SwizzledFormat*>(palCreateInfo.pViewFormats));
    }

    return result;
}

// =====================================================================================================================
// Create a new image object
VkResult Image::CreatePresentableImage(
    Device*                                 pDevice,
    const Pal::PresentableImageCreateInfo*  pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkImageUsageFlags                       imageUsageFlags,
    Pal::PresentMode                        presentMode,
    VkImage*                                pImage,
    VkFormat                                imageFormat,
    VkSharingMode                           sharingMode,
    uint32_t                                queueFamilyIndexCount,
    const uint32_t*                         pQueueFamilyIndices,
    VkDeviceMemory*                         pDeviceMemory)
{
    Pal::Result palResult;

    Memory* pMemory = nullptr;

    // Allocate system memory for objects
    const uint32_t numDevices         = pDevice->NumPalDevices();
    const uint32_t allocateDeviceMask = pDevice->GetPalDeviceMask();

    size_t palImgSize = 0;
    size_t palMemSize = 0;

    pDevice->PalDevice(DefaultDeviceIndex)->GetPresentableImageSizes(*pCreateInfo, &palImgSize, &palMemSize, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

#if DEBUG
    for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
    {
        size_t imgSize = 0;
        size_t memSize = 0;

        // Validate Pal::IImage and Pal::IGpuMemory across devices.
        pDevice->PalDevice(deviceIdx)->GetPresentableImageSizes(*pCreateInfo, &imgSize, &memSize, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);
        VK_ASSERT(imgSize == palImgSize);
        VK_ASSERT(memSize == palMemSize);
    }
#endif

    void* pImgObjMemory = pDevice->AllocApiObject(
        pAllocator,
        ObjectSize(pDevice) + (palImgSize * numDevices));

    if (pImgObjMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    void* pMemObjMemory = pDevice->AllocApiObject(
        pAllocator,
        sizeof(Memory) + (palMemSize * numDevices));

    if (pMemObjMemory == nullptr)
    {
        pDevice->FreeApiObject(pAllocator, pImgObjMemory);

        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Create the PAL image
    Pal::IImage*        pPalImage[MaxPalDevices]  = {};
    Pal::IGpuMemory*    pPalMemory[MaxPalDevices] = {};

    Pal::Result result = Pal::Result::Success;

    size_t palImgOffset = ObjectSize(pDevice);
    size_t palMemOffset = sizeof(Memory);

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

        result = pPalDevice->CreatePresentableImage(
            *pCreateInfo,
            Util::VoidPtrInc(pImgObjMemory, palImgOffset),
            Util::VoidPtrInc(pMemObjMemory, palMemOffset),
            &pPalImage[deviceIdx],
            &pPalMemory[deviceIdx]);

        palImgOffset += palImgSize;
        palMemOffset += palMemSize;

        // We assert that preferredHeap crossing device group shall be same, actually, shall be LocalInvisible.
        VK_ASSERT((pPalMemory[DefaultDeviceIndex] == nullptr) ||
                  (((pPalMemory[deviceIdx]->Desc().heapCount > 0) &&
                    (pPalMemory[DefaultDeviceIndex]->Desc().heapCount > 0)) &&
                   (pPalMemory[deviceIdx]->Desc().heaps[0] == pPalMemory[DefaultDeviceIndex]->Desc().heaps[0])));
    }

    // from PAL, toomanyflippableAllocation is a warning, instead of a failure. the allocate should be success.
    // but when they warn us, future flippable image allocation may fail based on OS.
    if ((result == Pal::Result::Success) || (result == Pal::Result::TooManyFlippableAllocations))
    {
        // Presentable images are never sparse so tile size doesn't matter
        constexpr VkExtent3D dummyTileSize = {};

        // Default presentable images to a single mip and arraySize
        const uint32_t miplevels = 1;
        const uint32_t arraySize = pCreateInfo->flags.stereo ? 2 : 1;

        ImageFlags imageFlags;

        imageFlags.u32All            = 0;
        imageFlags.internalMemBound  = false;
        imageFlags.dedicatedRequired = true;

        uint32_t presentLayoutUsage = GetPresentLayoutUsage(presentMode);

        // stencil usage will be treated same as usage if no separate stencil usage is specified.
        VkImageUsageFlags stencilUsageFlags = imageUsageFlags;

        ResourceOptimizerKey resourceKey;
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.extent = { pCreateInfo->extent.width, pCreateInfo->extent.height, 1};
        imageCreateInfo.mipLevels = miplevels;
        imageCreateInfo.arrayLayers = arraySize;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = imageUsageFlags;
        imageCreateInfo.sharingMode = sharingMode;
        imageCreateInfo.queueFamilyIndexCount = queueFamilyIndexCount;
        imageCreateInfo.pQueueFamilyIndices = pQueueFamilyIndices;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        BuildResourceKey(&imageCreateInfo, &resourceKey, pDevice->GetRuntimeSettings());

        SetCommonFlags(&imageCreateInfo, imageFormat, &imageFlags);

        // Construct API image object.
        VK_PLACEMENT_NEW (pImgObjMemory) Image(
            pDevice,
            &imageCreateInfo,
            pPalImage,
            nullptr,
            sharingMode,
            presentLayoutUsage,
            dummyTileSize,
            miplevels,
            arraySize,
            imageFormat,
            stencilUsageFlags,
            imageFlags,
            resourceKey);

        *pImage = Image::HandleFromVoidPointer(pImgObjMemory);

        // Presentable image memory shall be multiInstance on multi-device configuration.
        const bool multiInstance = (pDevice->NumPalDevices() > 1);
        pMemory = VK_PLACEMENT_NEW(pMemObjMemory) Memory(pDevice, pPalMemory, multiInstance);

        *pDeviceMemory = Memory::HandleFromObject(pMemory);

        return VK_SUCCESS;
    }
    else
    {
        pDevice->FreeApiObject(pAllocator, pImgObjMemory);
        pDevice->FreeApiObject(pAllocator, pMemObjMemory);
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Destroy image object
VkResult Image::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_perGpu[deviceIdx].pPalImage != nullptr)
        {
            bool skipDestroy = m_internalFlags.boundToSwapchainMemory ||
                               (m_internalFlags.boundToExternalMemory && (deviceIdx == DefaultDeviceIndex));

            if (skipDestroy == false)
            {
                m_perGpu[deviceIdx].pPalImage->Destroy();
            }
        }

        if ((m_perGpu[deviceIdx].pPalMemory != nullptr) && (m_internalFlags.internalMemBound != 0))
        {
            pDevice->RemoveMemReference(pDevice->PalDevice(deviceIdx), m_perGpu[deviceIdx].pPalMemory);
            m_perGpu[deviceIdx].pPalMemory->Destroy();
        }
    }

    if (IsSparse())
    {
        // Free the system memory allocated by InitSparseVirtualMemory
        pAllocator->pfnFree(pAllocator->pUserData, m_perGpu[0].pPalMemory);
    }

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// This function calculates any required internal padding due to mismatching alignment requirements between a VkImage
// and a possible VkMemory host.  All VkMemory hosts have rather large base address alignment requirements to account
// for most images' requirements, but some images have very extreme alignment requirements (several MB), and it is
// wasteful to pad every VkMemory object to those exotic requirements.
//
// Instead, this function returns a sufficient amount of extra size padding required for a particular image to account
// for an extra offset to align the base address at bind-time.
static VkDeviceSize CalcBaseAddrSizePadding(
    const Device&               device,
    const VkMemoryRequirements& memReqs)
{
    VkDeviceSize extraPadding = 0;

    // Calculate the smallest base address alignment of any VkMemory created using one of the compatible memory
    // types.
    VkDeviceSize minBaseAlignment = device.GetMemoryBaseAddrAlignment(memReqs.memoryTypeBits);

    // If the base address alignment requirements of the image exceed the base address alignment requirements of
    // the memory object, we need to pad the size of the image by the difference so that we can align the base
    // address at bind-time using an offset.
    if (memReqs.alignment > minBaseAlignment)
    {
        extraPadding += (memReqs.alignment - minBaseAlignment);
    }

    return extraPadding;
}

// =====================================================================================================================
void GenerateBindIndices(
    uint32_t                numDevices,
    uint8_t*                pBindIndices,
    uint32_t                deviceIndexCount,
    const uint32_t*         pDeviceIndices,
    uint32_t                rectCount,
    const VkRect2D*         pRects,
    bool                    multiInstanceHeap)
{
    memset(pBindIndices, InvalidPalDeviceMask, sizeof(pBindIndices[0]) * numDevices);

    VK_ASSERT(rectCount == 0);
                                // We have not exposed VK_IMAGE_CREATE_BIND_SFR_BIT so rectCount must be zero.
    if (deviceIndexCount != 0)
    {
        // Binding Indices were supplied. There must be a bind index for each device in the device group.
        VK_ASSERT(deviceIndexCount == numDevices);

        for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
        {
            pBindIndices[deviceIdx] = pDeviceIndices[deviceIdx];
        }
    }
    else
    {
        // Apply default binding, considering whether we are binding a multi-instance heap.
        for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
        {
            pBindIndices[deviceIdx] = multiInstanceHeap ? deviceIdx : DefaultDeviceIndex;
        }
    }
}

// =====================================================================================================================
// Binds memory to this image.
VkResult Image::BindMemory(
    Device*            pDevice,
    VkDeviceMemory     mem,
    VkDeviceSize       memOffset,
    uint32_t           deviceIndexCount,
    const uint32_t*    pDeviceIndices,
    uint32_t           rectCount,
    const VkRect2D*    pRects)
{
    Memory* pMemory = (mem != VK_NULL_HANDLE) ? Memory::ObjectFromHandle(mem) : nullptr;

    // If this memory has been bound on the image, do nothing.
    if (m_perGpu[DefaultDeviceIndex].pPalImage == pMemory->GetExternalPalImage())
    {
        return VK_SUCCESS;
    }

    if (m_internalFlags.externallyShareable && (pMemory->GetExternalPalImage() != nullptr))
    {
        // For MGPU, the external sharing resource only uses the first PAL image.
        m_perGpu[DefaultDeviceIndex].pPalImage->Destroy();
        m_perGpu[DefaultDeviceIndex].pPalImage = pMemory->GetExternalPalImage();
        m_internalFlags.boundToExternalMemory = 1;
    }

    VkMemoryRequirements reqs = GetMemoryRequirements();

    Pal::Result result = Pal::Result::Success;

    const uint32_t numDevices = pDevice->NumPalDevices();

    uint8_t bindIndices[MaxPalDevices];
    GenerateBindIndices(numDevices,
        bindIndices,
        deviceIndexCount,
        pDeviceIndices,
        rectCount,
        pRects,
        ((pMemory == nullptr) ? false : pMemory->IsMultiInstance()));

    for (uint32_t localDeviceIdx = 0; localDeviceIdx < numDevices; localDeviceIdx++)
    {
        const uint32_t sourceMemInst = bindIndices[localDeviceIdx];

        Pal::IImage*     pPalImage      = m_perGpu[localDeviceIdx].pPalImage;
        Pal::IGpuMemory* pGpuMem        = nullptr;
        Pal::gpusize     baseAddrOffset = 0;

        if (pMemory != nullptr)
        {
            pGpuMem = pMemory->PalMemory(localDeviceIdx, sourceMemInst);

            // The bind offset within the memory should already be pre-aligned
            VK_ASSERT(Util::IsPow2Aligned(memOffset, reqs.alignment));

            VkDeviceSize baseGpuAddr = pGpuMem->Desc().gpuVirtAddr;

            // If the base address of the VkMemory is not already aligned
            if ((Util::IsPow2Aligned(baseGpuAddr, reqs.alignment) == false) &&
                (m_internalFlags.externalD3DHandle == false))
            {
                // This should only happen in situations where the image's alignment is extremely larger than
                // the VkMemory object.
                VK_ASSERT(pGpuMem->Desc().alignment < reqs.alignment);

                // Calculate the necessary offset to make the base address align to the image's requirements.
                baseAddrOffset = Util::Pow2Align(baseGpuAddr, reqs.alignment) - baseGpuAddr;

                // Verify that we allocated sufficient padding to account for this offset
                VK_ASSERT(baseAddrOffset <= CalcBaseAddrSizePadding(*pDevice, reqs));
            }

            // After applying any necessary base address offset, the full GPU address should be aligned
            VK_ASSERT(Util::IsPow2Aligned(baseGpuAddr + baseAddrOffset + memOffset, reqs.alignment));

            if (pDevice->GetEnabledFeatures().appControlledMemPriority == false)
            {
                pMemory->ElevatePriority(m_priority);
            }
        }

        result = pPalImage->BindGpuMemory(pGpuMem, baseAddrOffset + memOffset);

        if (result == Pal::Result::Success)
        {
            // Record the private base address offset.  This is necessary for things like subresource layout
            // calculation for linear images.
            m_perGpu[localDeviceIdx].baseAddrOffset = baseAddrOffset;
        }
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Binds to Gpu memory already allocated to a swapchain object
VkResult Image::BindSwapchainMemory(
    const Device*      pDevice,
    uint32_t           swapChainImageIndex,
    SwapChain*         pSwapchain,
    uint32_t           deviceIndexCount,
    const uint32_t*    pDeviceIndices,
    uint32_t           rectCount,
    const VkRect2D*    pRects)
{
    const uint32_t numDevices = pDevice->NumPalDevices();

    // We need to destroy the unbound pal image objects because the swap chain image we are about to bind probably
    // has different compression capabilities.
    for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
    {
        m_perGpu[deviceIdx].pPalImage->Destroy();
    }

    // Ensure we do not later destroy the Pal image objects that we bind in this function.
    m_internalFlags.boundToSwapchainMemory = 1;

    const SwapChain::Properties& properties = pSwapchain->GetProperties();

    m_pSwapChain = pSwapchain;

    Memory* pMemory = Memory::ObjectFromHandle(properties.imageMemory[swapChainImageIndex]);

    Image*  pSwapchainImage    = Image::ObjectFromHandle(properties.images[swapChainImageIndex]);
    Memory* pSwapchainImageMem = Memory::ObjectFromHandle(properties.imageMemory[swapChainImageIndex]);

    Util::Destructor(&m_barrierPolicy);

    uint32_t presentLayoutUsage = GetPresentLayoutUsage(properties.imagePresentSupport);

    // Create new barrier policy from the swapchain image.
    VK_PLACEMENT_NEW (&m_barrierPolicy) ImageBarrierPolicy(
        pDevice,
        properties.usage,
        properties.sharingMode,
        properties.queueFamilyIndexCount,
        properties.pQueueFamilyIndices,
        false, // presentable images are never multisampled
        properties.format,
        presentLayoutUsage);

    uint8_t bindIndices[MaxPalDevices];
    GenerateBindIndices(numDevices,
                        bindIndices,
                        deviceIndexCount,
                        pDeviceIndices,
                        rectCount,
                        pRects,
                        ((pMemory == nullptr) ? false : pMemory->IsMultiInstance()));

    for (uint32_t localDeviceIdx = 0; localDeviceIdx < numDevices; localDeviceIdx++)
    {
        const uint32_t sourceMemInst = bindIndices[localDeviceIdx];

        if (localDeviceIdx == sourceMemInst)
        {
            m_perGpu[localDeviceIdx].pPalImage = pSwapchainImage->PalImage(localDeviceIdx);
        }
        else
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(localDeviceIdx);
            Pal::IImage* pPalImage   = pSwapchainImage->PalImage(localDeviceIdx);

            Pal::PeerImageOpenInfo peerInfo = {};
            peerInfo.pOriginalImage = pPalImage;

            Pal::IGpuMemory* pGpuMemory = pMemory->PalMemory(localDeviceIdx, sourceMemInst);

            void* pImageMem = m_perGpu[localDeviceIdx].pPalImage;

            Pal::Result palResult = pPalDevice->OpenPeerImage(
                peerInfo, pImageMem, nullptr, &m_perGpu[localDeviceIdx].pPalImage, &pGpuMemory);

            VK_ASSERT(palResult == Pal::Result::Success);
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Implementation of vkGetImageSubresourceLayout
VkResult Image::GetSubresourceLayout(
    const Device*             pDevice,
    const VkImageSubresource* pSubresource,
    VkSubresourceLayout*      pLayout
    ) const
{
    // Request the subresource information from PAL
    Pal::SubresLayout palLayout = {};
    Pal::SubresId palSubResId = {};

    palSubResId.plane = VkToPalImagePlaneSingle(m_format,
        pSubresource->aspectMask, pDevice->GetRuntimeSettings());

    palSubResId.mipLevel   = pSubresource->mipLevel;
    palSubResId.arraySlice = pSubresource->arrayLayer;

    Pal::Result palResult = PalImage(DefaultDeviceIndex)->GetSubresourceLayout(palSubResId, &palLayout);

    if (palResult != Pal::Result::Success)
    {
        return PalToVkResult(palResult);
    }

    const Pal::ImageCreateInfo& createInfo = PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

    for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        // If this is triggered, memoryBaseAddrAlignment should be raised to the alignment of this image for MGPU.
        VK_ASSERT(m_perGpu[DefaultDeviceIndex].baseAddrOffset == m_perGpu[deviceIdx].baseAddrOffset);
    }

    pLayout->offset     = m_perGpu[DefaultDeviceIndex].baseAddrOffset + palLayout.offset;
    pLayout->size       = palLayout.size;
    pLayout->rowPitch   = palLayout.rowPitch;
    pLayout->arrayPitch = (createInfo.arraySize > 1)    ? palLayout.depthPitch : 0;
    pLayout->depthPitch = (createInfo.extent.depth > 1) ? palLayout.depthPitch : 0;

#if defined(__unix__)
    // If the image’s tiling is VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
    // then the aspectMask member of VkImageSubresource must be one of VK_IMAGE_ASPECT_MEMORY_PLANE_i_BIT_EXT.
    if (PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.hasModifier != 0)
    {
        VK_ASSERT((pSubresource->aspectMask >= VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT) &&
                  (pSubresource->aspectMask <= VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT));

        VK_ASSERT((pSubresource->arrayLayer == 0) && (pSubresource->mipLevel == 0));

        // Planar yuv with Dcc is not supported when using drm format modifier.
        if (Formats::GetYuvPlaneCounts(m_format) == 1)
        {
            // For non planar yuv with Dcc, memory plane layout is:
            // image without Dcc:
            // MEMORY_PLANE_0
            // main surface

            // image with gfx Dcc:
            // MEMORY_PLANE_0           MEMORY_PLANE_1
            // main surface             Dcc surface

            // image with gfx Dcc and dcn Dcc:
            // MEMORY_PLANE_0           MEMORY_PLANE_1           MEMORY_PLANE_2
            // main surface             displayable Dcc surface  Dcc surface

            // The main surface layout is already obtained from GetSubresourceLayout().
            if (pSubresource->aspectMask != VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT)
            {
                palResult = PalImage(DefaultDeviceIndex)->GetModifierSubresourceLayout(palSubResId.plane,
                                                                                       &palLayout);
            }

            pLayout->offset     = palLayout.offset;
            pLayout->size       = palLayout.size;
            pLayout->rowPitch   = palLayout.rowPitch;
            pLayout->arrayPitch = 0;
            pLayout->depthPitch = 0;

            return PalToVkResult(palResult);
        }
    }
#endif

    return VK_SUCCESS;
}

// =====================================================================================================================
// Implementation of vkGetImageSparseMemoryRequirements
void Image::GetSparseMemoryRequirements(
    Device*                                             pDevice,
    uint32_t*                                           pNumRequirements,
    utils::ArrayView<VkSparseImageMemoryRequirements>   sparseMemoryRequirements)
{
    // TODO: Use PAL interface for probing aspect availability, once it is introduced.
    uint32_t               usedAspectsCount    = 0;
    const bool             isSparse            = PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.prt;
    bool                   needsMetadataAspect = false;
    Pal::Result            palResult;
    const PhysicalDevice*  physDevice          = pDevice->VkPhysicalDevice(DefaultDeviceIndex);

    // Count the number of aspects
    struct
    {
        uint32_t              planePal;
        VkImageAspectFlagBits aspectVk;
        bool                  available;
    } aspects[] =
    {
        {0, VK_IMAGE_ASPECT_COLOR_BIT,   IsColorFormat()},
        {0, VK_IMAGE_ASPECT_DEPTH_BIT,   HasDepth()},
        {1, VK_IMAGE_ASPECT_STENCIL_BIT, HasStencil()}
    };
    uint32_t supportedAspectsCount = sizeof(aspects) / sizeof(aspects[0]);

    const Pal::ImageMemoryLayout& memoryLayout = PalImage(DefaultDeviceIndex)->GetMemoryLayout();

    for (uint32_t nAspect = 0; nAspect < supportedAspectsCount; ++nAspect)
    {
        if (aspects[nAspect].available)
        {
            ++usedAspectsCount;
        }
    }

    if (memoryLayout.metadataSize != 0)
    {
        /* Also include metadata aspect */
        needsMetadataAspect = true;

        ++usedAspectsCount;
    }

    if (isSparse && *pNumRequirements == 0)
    {
        *pNumRequirements = usedAspectsCount;
    }
    else if (isSparse && (sparseMemoryRequirements.IsNull() == false) && (*pNumRequirements >= 1))
    {
        const uint32_t       aspectsToReportCount = Util::Min(*pNumRequirements, usedAspectsCount);
        uint32_t             reportedAspectsCount = 0;

        // Get the memory layout of the sparse image

        for (uint32_t nAspect = 0; nAspect < supportedAspectsCount; ++(int&)nAspect)
        {
            const auto&                      currentAspect       = aspects[nAspect];
            Pal::SubresLayout                miptailLayouts[2]   = {};
            VkSparseImageMemoryRequirements* pCurrentRequirement = &sparseMemoryRequirements[reportedAspectsCount];

            // Is this aspect actually available?
            if (!currentAspect.available)
            {
                continue;
            }
            else
            {
                ++reportedAspectsCount;
            }

            // Get the first two miptail's layout information (if available) to be able to determine the miptail offset
            // and the stride between layers, if applicable
            uint32_t miptailLayoutCount = 0;

            if (memoryLayout.prtMinPackedLod < m_mipLevels)
            {
                miptailLayoutCount = Util::Min(m_arraySize, 2u);

                for (uint32_t i = 0; i < miptailLayoutCount; ++i)
                {
                    const Pal::SubresId subresourceId =
                    {
                        currentAspect.planePal,
                        memoryLayout.prtMinPackedLod,
                        i  /* arraySlice */
                    };

                    palResult = PalImage(DefaultDeviceIndex)->GetSubresourceLayout(subresourceId, &miptailLayouts[i]);
                    VK_ASSERT(palResult == Pal::Result::Success);
                }
            }

            pCurrentRequirement->formatProperties.aspectMask = currentAspect.aspectVk;

            pCurrentRequirement->formatProperties.imageGranularity.width  = m_tileSize.width;
            pCurrentRequirement->formatProperties.imageGranularity.height = m_tileSize.height;
            pCurrentRequirement->formatProperties.imageGranularity.depth  = m_tileSize.depth;

            // NOTE: For formats like D16S8, PAL reports support for 8x8 tile sizes on some HW . The spec
            //       recommends to use standard sparse image block shapes if only supported though, and
            //       since all of these are divisible by 8x8, we are going to stick to standard tile sizes.
            //
            //       We may want to revisit this in the future if ISVs request for better granularity.
            VK_ASSERT((m_tileSize.width  % memoryLayout.prtTileWidth)  == 0 &&
                      (m_tileSize.height % memoryLayout.prtTileHeight) == 0 &&
                      (m_tileSize.depth  % memoryLayout.prtTileDepth)  == 0);

            pCurrentRequirement->formatProperties.flags = 0;

            // If per-layer miptail isn't supported then set SINGLE_MIPTAIL_BIT
            if ((physDevice->GetPrtFeatures() & Pal::PrtFeaturePerSliceMipTail) == 0)
            {
                pCurrentRequirement->formatProperties.flags |= VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
            }

            // If unaligned mip size isn't supported then set ALIGNED_MIP_SIZE_BIT
            if ((physDevice->GetPrtFeatures() & Pal::PrtFeatureUnalignedMipSize) == 0)
            {
                pCurrentRequirement->formatProperties.flags |= VK_SPARSE_IMAGE_FORMAT_ALIGNED_MIP_SIZE_BIT;
            }

            pCurrentRequirement->imageMipTailFirstLod = memoryLayout.prtMinPackedLod;
            const auto mipTailSize                    = memoryLayout.prtMipTailTileCount *
                                                        physDevice->PalProperties().imageProperties.prtTileSize;

            // If PAL reports alignment > size, then we have no choice but to increase the size to match.
            pCurrentRequirement->imageMipTailSize = Util::RoundUpToMultiple(mipTailSize,
                                                        physDevice->PalProperties().imageProperties.prtTileSize);

            // for per-slice-miptail, the miptail should only take one tile and the base address is tile aligned.
            // for single-miptail, the offset of first in-miptail mip level of slice 0 refers to the offset of miptail.
            pCurrentRequirement->imageMipTailOffset   = Util::RoundDownToMultiple(miptailLayouts[0].offset,
                                                        physDevice->PalProperties().imageProperties.prtTileSize);

            pCurrentRequirement->imageMipTailStride   = (miptailLayoutCount > 1)
                                                      ? miptailLayouts[1].offset - miptailLayouts[0].offset : 0;
        }

        if (needsMetadataAspect && reportedAspectsCount < *pNumRequirements)
        {
            VkSparseImageMemoryRequirements* pCurrentRequirement = &sparseMemoryRequirements[reportedAspectsCount];

            pCurrentRequirement->formatProperties.aspectMask       = VK_IMAGE_ASPECT_METADATA_BIT;
            pCurrentRequirement->formatProperties.flags            = VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
            pCurrentRequirement->formatProperties.imageGranularity = {0};
            pCurrentRequirement->imageMipTailFirstLod              = 0;
            pCurrentRequirement->imageMipTailSize                  = Util::RoundUpToMultiple(memoryLayout.metadataSize,
                                                                                             physDevice->PalProperties().imageProperties.prtTileSize);
            pCurrentRequirement->imageMipTailOffset                = memoryLayout.metadataOffset;
            pCurrentRequirement->imageMipTailStride                = 0;

            ++reportedAspectsCount;
        }

        // Report the actual sparse memory requirements reported
        *pNumRequirements = reportedAspectsCount;
    }
    else
    {
        // In all other cases we'll just report the memory requirement count
        *pNumRequirements = isSparse ? 1 : 0;
    }
}

// =====================================================================================================================
// Calculate image's memory requirements from VkImageCreateInfo during image creation
void Image::CalculateMemoryRequirementsAtCreate(
    const Device*            pDevice,
    const VkImageCreateInfo* pCreateInfo,
    VkMemoryRequirements*    pMemoryRequirements)
{
    ExternalMemoryFlags externalFlags;
    externalFlags.u32All = 0;

    externalFlags.dedicatedRequired   = m_internalFlags.dedicatedRequired;
    externalFlags.externallyShareable = m_internalFlags.externallyShareable;
    externalFlags.externalD3DHandle   = m_internalFlags.externalD3DHandle;
    externalFlags.externalPinnedHost  = m_internalFlags.externalPinnedHost;

    Pal::IImage* pImages[MaxPalDevices] = {};

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
    {
        pImages[deviceIdx] = PalImage(deviceIdx);
    }

    CalculateMemoryRequirementsInternal(pDevice, pCreateInfo, externalFlags, pImages, pMemoryRequirements);
}

// =====================================================================================================================
// Get the image's memory requirements
void Image::CalculateMemoryRequirementsInternal(
    const Device*             pDevice,
    const VkImageCreateInfo*  pCreateInfo,
    const ExternalMemoryFlags externalFlags,
    Pal::IImage*              pImages[MaxPalDevices],
    VkMemoryRequirements*     pMemoryRequirements)
{
    const bool                 isSparse           = (pCreateInfo->flags & SparseEnablingFlags) != 0;
    Pal::GpuMemoryRequirements palReqs            = {};
    const auto                 virtualGranularity = pDevice->GetProperties().virtualMemAllocGranularity;

    VK_ASSERT(pImages[DefaultDeviceIndex] != nullptr);

    if (pImages[DefaultDeviceIndex] != nullptr)
    {
        pImages[DefaultDeviceIndex]->GetGpuMemoryRequirements(&palReqs);
    }

#if DEBUG
    for (uint32 deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
       VK_ASSERT(pImages[deviceIdx] != nullptr);

       if ((deviceIdx != DefaultDeviceIndex) && (pImages[deviceIdx] != nullptr))
        {
            Pal::GpuMemoryRequirements deviceReqs = {};
            pImages[deviceIdx]->GetGpuMemoryRequirements(&deviceReqs);
            VK_ASSERT(memcmp(&palReqs, &deviceReqs, sizeof(deviceReqs)) == 0);
        }
    }
#endif

    if (isSparse)
    {
        pMemoryRequirements->alignment = Util::RoundUpToMultiple(virtualGranularity, palReqs.alignment);
        pMemoryRequirements->size      = Util::RoundUpToMultiple(palReqs.size, virtualGranularity);
    }
    else
    {
        pMemoryRequirements->alignment = palReqs.alignment;
        pMemoryRequirements->size      = palReqs.size;
    }

    pMemoryRequirements->memoryTypeBits = 0;

    for (uint32_t i = 0; i < palReqs.heapCount; ++i)
    {
        uint32_t typeIndexBits;

        if (pDevice->GetVkTypeIndexBitsFromPalHeap(palReqs.heaps[i], &typeIndexBits))
        {
            pMemoryRequirements->memoryTypeBits |= typeIndexBits;
        }
    }

    // Limit heaps to those compatible with pinned system memory
    if (externalFlags.externalPinnedHost)
    {
        pMemoryRequirements->memoryTypeBits &= pDevice->GetPinnedSystemMemoryTypes();

        VK_ASSERT(pMemoryRequirements->memoryTypeBits != 0);
    }
    else if (externalFlags.externallyShareable)
    {
        pMemoryRequirements->memoryTypeBits &= pDevice->GetMemoryTypeMaskForExternalSharing();
    }

    if ((pCreateInfo->flags & VK_IMAGE_CREATE_PROTECTED_BIT) != 0)
    {
        // If the image is protected only keep the protected type
        pMemoryRequirements->memoryTypeBits &= pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_PROTECTED_BIT);
    }
    else
    {
        // If the image isn't protected remove the protected types
        pMemoryRequirements->memoryTypeBits &= ~pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_PROTECTED_BIT);
    }

    if (pDevice->GetEnabledFeatures().deviceCoherentMemory == false)
    {
        // If the state of the device coherent memory feature (defined by the extension VK_AMD_device_coherent_memory)
        // is disabled, remove the device coherent memory type
        pMemoryRequirements->memoryTypeBits &=
            ~pDevice->GetMemoryTypeMaskMatching(VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD);
    }

    // Images are not using memoryType for DescriptorBuffers
    pMemoryRequirements->memoryTypeBits &= ~pDevice->GetMemoryTypeMaskForDescriptorBuffers();

    // Add an extra memory padding. This can be enabled while capturing GFXR traces and disabled later. Capturing with
    // this setting enabled helps in replaying GFXR traces. When this setting is not used while capture, GFXR might
    // return a fatal error while replaying with different DCC threshold values. This is caused because gfxreconstruct
    // (just like vktrace used to) records the memory sizes and offsets at the time of capture and always resends the
    // same values during replay.
    if (pDevice->GetRuntimeSettings().addMemoryPaddingToImageMemoryRequirements)
    {
        pMemoryRequirements->size +=
            (uint64_t)((pDevice->GetRuntimeSettings().memoryPaddingFactorForImageMemoryRequirements) *
            (pMemoryRequirements->size));
    }

    // Adjust the size to account for internal padding required to align the base address
    pMemoryRequirements->size += CalcBaseAddrSizePadding(*pDevice, *pMemoryRequirements);

    if (isSparse)
    {
        pMemoryRequirements->size = Util::RoundUpToMultiple(palReqs.size, pMemoryRequirements->alignment);
    }
}

// =====================================================================================================================
// Create a Pal::Image and calculate its memory requirements
void Image::GetPalImageMemoryRequirements(
    Device*                  pDevice,
    const VkImageCreateInfo* pCreateInfo,
    VkMemoryRequirements2*   pMemoryRequirements)
{
    const VkAllocationCallbacks* pAllocator    = pDevice->VkInstance()->GetAllocCallbacks();
    Pal::Result                  palResult     = Pal::Result::Success;
    ImageExtStructs              extStructs    = {};
    Pal::ImageCreateInfo         palCreateInfo = {};
    const RuntimeSettings&       settings      = pDevice->GetRuntimeSettings();
    const uint32_t               numDevices    = pDevice->NumPalDevices();
    ResourceOptimizerKey         resourceKey;

    BuildResourceKey(pCreateInfo, &resourceKey, settings);

    HandleExtensionStructs(pCreateInfo, &extStructs);

    ConvertImageCreateInfo(pDevice, pCreateInfo, pAllocator, extStructs, resourceKey, &palCreateInfo);

    ExternalMemoryFlags externalFlags;
    externalFlags.u32All = 0;

    GetExternalMemoryFlags(pDevice,
                           extStructs,
                           ((pCreateInfo->flags & SparseEnablingFlags) != 0),
                           &externalFlags);

    // Calculate required system memory size
    const size_t palImgSize = pDevice->PalDevice(DefaultDeviceIndex)->GetImageSize(palCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    if (palResult == Pal::Result::Success)
    {
#if DEBUG
        for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
        {
            VK_ASSERT(palImgSize == pDevice->PalDevice(deviceIdx)->GetImageSize(palCreateInfo, &palResult));
            VK_ASSERT(palResult  == Pal::Result::Success);
        }
#endif

        // Allocate system memory for Pal image
        void* pMemory = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                palImgSize * numDevices,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        VK_ASSERT(pMemory != nullptr);

        if (pMemory != nullptr)
        {
            // Create PAL image
            Pal::IImage* pPalImages[MaxPalDevices] = {};
            void*        pPalImgAddr               = pMemory;
            size_t       palImgOffset              = 0;

            for (uint32_t deviceIdx = 0; (palResult == Pal::Result::Success) && (deviceIdx < numDevices); deviceIdx++)
            {
                palResult = pDevice->PalDevice(deviceIdx)->CreateImage(
                    palCreateInfo,
                    Util::VoidPtrInc(pMemory, palImgOffset),
                    &pPalImages[deviceIdx]);
                VK_ASSERT(palResult == Pal::Result::Success);
                VK_ASSERT(pPalImages[deviceIdx] != nullptr);

                palImgOffset += palImgSize;
            }

            if (palResult == Pal::Result::Success)
            {
                CalculateMemoryRequirementsInternal(
                    pDevice,
                    pCreateInfo,
                    externalFlags,
                    pPalImages,
                    &pMemoryRequirements->memoryRequirements);

            }

            for (uint32_t deviceIdx = 0; (deviceIdx < numDevices); deviceIdx++)
            {
                if (pPalImages[deviceIdx] != nullptr)
                {
                    pPalImages[deviceIdx]->Destroy();
                }
            }

            pAllocator->pfnFree(pAllocator->pUserData, pMemory);

            VkMemoryDedicatedRequirements* pMemDedicatedRequirements =
                static_cast<VkMemoryDedicatedRequirements*>(pMemoryRequirements->pNext);

            if ((pMemDedicatedRequirements != nullptr) &&
                (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS))
            {
                pMemDedicatedRequirements->prefersDedicatedAllocation  = externalFlags.dedicatedRequired != 0;
                pMemDedicatedRequirements->requiresDedicatedAllocation = externalFlags.dedicatedRequired != 0;
            }
        }
    }

    if (palCreateInfo.pViewFormats != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, const_cast<Pal::SwizzledFormat*>(palCreateInfo.pViewFormats));
    }
}

#if defined(__unix__)
// =====================================================================================================================
// Creates and initializes a new pal Image object with modifier list. If fails, try next preferable modifier.
VkResult Image::CreateImageWithModifierList(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator,
    VkFormat                     format,
    const ImageExtStructs&       extStructs,
    void*                        pPalImgAddr,
    uint32                       deviceIdx,
    Pal::ImageCreateInfo*        pPalCreateInfo,
    Pal::IImage**                pPalImage)
{
    VkResult    result        = VK_SUCCESS;
    Pal::Result palResult     = Pal::Result::Success;
    uint32      modifierCount = extStructs.pModifierListCreateInfo->drmFormatModifierCount;

    uint64 modifierArray[modifierCount];
    memset(modifierArray, 0, sizeof(modifierArray));

    memcpy(modifierArray,
           extStructs.pModifierListCreateInfo->pDrmFormatModifiers,
           sizeof(uint64) * modifierCount);

    while (modifierCount-- > 0)
    {
        uint32 modifierNum = GetPreferableModifier(
            pDevice,
            pAllocator,
            format,
            extStructs.pModifierListCreateInfo->drmFormatModifierCount,
            modifierArray,
            pPalCreateInfo);

        if (pPalCreateInfo->flags.hasModifier == 0)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
            break;
        }

        palResult = pDevice->PalDevice(deviceIdx)->CreateImage(
            *pPalCreateInfo,
            pPalImgAddr,
            pPalImage);

        if (palResult == Pal::Result::Success)
        {
            break;
        }
        else
        {
            if (*pPalImage != nullptr)
            {
                (*pPalImage)->Destroy();
                (*pPalImage) = nullptr;
            }

            pPalCreateInfo->flags.hasModifier = 0;
            pPalCreateInfo->modifier          = DRM_FORMAT_MOD_INVALID;
            modifierArray[modifierNum]        = DRM_FORMAT_MOD_INVALID;
        }
    }

    VK_ASSERT(palResult == Pal::Result::Success);

    if (palResult != Pal::Result::Success)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Get preferable drm format modifier from list.
uint32 Image::GetPreferableModifier(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator,
    VkFormat                     format,
    uint32                       modifierCount,
    const uint64*                pModifiers,
    Pal::ImageCreateInfo*        pPalCreateInfo)
{
    const RuntimeSettings& settings   = pDevice->GetRuntimeSettings();
    Pal::IDevice*          pPalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalDevice();

    uint32 modifiersListCount = 0;
    uint32 modifierNum        = 0;

    pPalDevice->GetModifiersList(VkToPalFormat(format, settings).format,
                                 &modifiersListCount,
                                 nullptr);

    if ((modifiersListCount != 0) && (Formats::IsDepthStencilFormat(format) == false))
    {
        uint64* pModifiersList = static_cast<uint64*>(pAllocator->pfnAllocation(
                pAllocator->pUserData,
                (modifiersListCount * sizeof(uint64)),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));

        VK_ASSERT(pModifiersList != nullptr);

        if (pModifiersList != nullptr)
        {
            pPalDevice->GetModifiersList(VkToPalFormat(format, settings).format,
                                         &modifiersListCount,
                                         pModifiersList);

            for (uint32 i = 0; i < modifiersListCount; i++)
            {
                for (uint32 j = 0; j < modifierCount; j++)
                {
                    if (pModifiersList[i] == pModifiers[j])
                    {
                        modifierNum                       = j;
                        pPalCreateInfo->modifier          = pModifiersList[i];
                        pPalCreateInfo->flags.hasModifier = 1;
                        pPalCreateInfo->tiling            =
                            (pModifiersList[i] == DRM_FORMAT_MOD_LINEAR) ? Pal::ImageTiling::Linear :
                                                                           Pal::ImageTiling::Optimal;
                        break;
                    }
                }

                if (pPalCreateInfo->flags.hasModifier == 1)
                {
                    break;
                }
            }

            pAllocator->pfnFree(pAllocator->pUserData, pModifiersList);
        }
    }

    return modifierNum;
}

// =====================================================================================================================
// Validate info for drm format modifier.
VkResult Image::ValidateModifierInfo(
    Device*                pDevice,
    VkFormat               format,
    const ImageExtStructs& extStructs,
    Pal::ImageCreateInfo*  pPalCreateInfo)
{
    VkResult result = VK_SUCCESS;

    if (extStructs.pModifierExplicitCreateInfo != nullptr)
    {
        if (pPalCreateInfo->flags.hasModifier == 0)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
        else
        {
            uint64 modifier   = extStructs.pModifierExplicitCreateInfo->drmFormatModifier;
            uint32 planeCount = extStructs.pModifierExplicitCreateInfo->drmFormatModifierPlaneCount;

            if (Formats::IsYuvPlanar(format))
            {
                if (Formats::GetYuvPlaneCounts(format) != planeCount)
                {
                    result = VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
                }
            }
            else if (AMD_FMT_MOD_GET(DCC_RETILE, modifier))
            {
                if (planeCount != 3)
                {
                    result = VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
                }
            }
            else if (AMD_FMT_MOD_GET(DCC, modifier))
            {
                if (planeCount != 2)
                {
                    result = VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
                }
            }
            else
            {
                if (planeCount != 1)
                {
                    result = VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
                }
            }
        }
    }
    else if ((extStructs.pModifierListCreateInfo != nullptr) &&
             (extStructs.pModifierListCreateInfo->drmFormatModifierCount == 0))
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}
#endif

// =====================================================================================================================
// Create a Pal::Image and calculate its subresource layout
void Image::CalculateSubresourceLayout(
    Device*                   pDevice,
    const VkImageCreateInfo*  pCreateInfo,
    const VkImageSubresource* pSubresource,
    VkSubresourceLayout*      pSubresourceLayout)
{
    VkImage image;

    VkResult result = pDevice->CreateImage(pCreateInfo, pDevice->VkInstance()->GetAllocCallbacks(), &image);

    if (result == VK_SUCCESS)
    {
        Image::ObjectFromHandle(image)->GetSubresourceLayout(
            pDevice,
            pSubresource,
            pSubresourceLayout);
        Image::ObjectFromHandle(image)->Destroy(pDevice, pDevice->VkInstance()->GetAllocCallbacks());
    }
}

// =====================================================================================================================
// Calculate image's memory requirements from VkImageCreateInfo
void Image::CalculateMemoryRequirements(
    Device*                                   pDevice,
    const VkDeviceImageMemoryRequirementsKHR* pInfo,
    VkMemoryRequirements2*                    pMemoryRequirements)
{
    GetPalImageMemoryRequirements(pDevice, pInfo->pCreateInfo, pMemoryRequirements);

    if (pDevice->GetEnabledFeatures().strictImageSizeRequirements &&
        Formats::IsDepthStencilFormat(pInfo->pCreateInfo->format) &&
        ((pInfo->pCreateInfo->flags & SparseEnablingFlags) == 0))
    {
        CalculateAlignedMemoryRequirements(
            pDevice,
            pInfo->pCreateInfo,
            &pMemoryRequirements->memoryRequirements);
    }
}

// =====================================================================================================================
// Calculate image's memory requirements from VkImageCreateInfo for depth/stencil formats
void Image::CalculateAlignedMemoryRequirements(
    Device*                  pDevice,
    const VkImageCreateInfo* pCreateInfo,
    VkMemoryRequirements*    pMemoryRequirements)
{
    VkImageCreateInfo     createInfo             = *pCreateInfo;
    VkMemoryRequirements2 pow2MemoryRequirements = {};

    if (!IsPowerOfTwo(createInfo.extent.width))
    {
        // Round width down to the nearest power of 2.
        createInfo.extent.width = Pow2Pad(createInfo.extent.width) >> 1;

        GetPalImageMemoryRequirements(pDevice, &createInfo, &pow2MemoryRequirements);

        if (pow2MemoryRequirements.memoryRequirements.size > pMemoryRequirements->size)
        {
            pMemoryRequirements->size = pow2MemoryRequirements.memoryRequirements.size;
        }

        createInfo.extent.width = pCreateInfo->extent.width;
    }

    if (!IsPowerOfTwo(createInfo.extent.height))
    {
        // Round height down to the nearest power of 2.
        createInfo.extent.height = Pow2Pad(createInfo.extent.height) >> 1;

        pow2MemoryRequirements = {};
        GetPalImageMemoryRequirements(pDevice, &createInfo, &pow2MemoryRequirements);

        if (pow2MemoryRequirements.memoryRequirements.size > pMemoryRequirements->size)
        {
            pMemoryRequirements->size = pow2MemoryRequirements.memoryRequirements.size;
        }
    }
}

// =====================================================================================================================
// Calculate sparse image's memory requirements from VkImageCreateInfo
void Image::CalculateSparseMemoryRequirements(
    Device*                                   pDevice,
    const VkDeviceImageMemoryRequirementsKHR* pInfo,
    uint32_t*                                 pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2*         pSparseMemoryRequirements)
{
    VkImage image;

    VkResult result = pDevice->CreateImage(pInfo->pCreateInfo, pDevice->VkInstance()->GetAllocCallbacks(), &image);

    if (result == VK_SUCCESS)
    {
        Image::ObjectFromHandle(image)->GetSparseMemoryRequirements(
            pDevice,
            pSparseMemoryRequirementCount,
            utils::ArrayView<VkSparseImageMemoryRequirements>(&pSparseMemoryRequirements->memoryRequirements));
        Image::ObjectFromHandle(image)->Destroy(pDevice, pDevice->VkInstance()->GetAllocCallbacks());
    }
}

// =====================================================================================================================
void Image::RegisterPresentableImageWithSwapChain(SwapChain* pSwapChain)
{
    // Registration is only allowed to happen once
    VK_ASSERT(m_pSwapChain == nullptr);
    m_pSwapChain = pSwapChain;

    // If swapchain requires this image to be treated as SRGB.
    m_internalFlags.treatAsSrgb = pSwapChain->GetProperties().flags.treatAsSrgb;

    // Find the corresponding SRGB format
    if (m_internalFlags.treatAsSrgb)
    {
        // Only two SRGB swapchain formats exist.
        if (m_format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            m_srgbFormat = VK_FORMAT_R8G8B8A8_SRGB;
        }
        else if (m_format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            m_srgbFormat = VK_FORMAT_B8G8R8A8_SRGB;
        }
    }
}

// =====================================================================================================================
// This is a function used to convert PAL PresentMode to Vk equivalents.
uint32_t Image::GetPresentLayoutUsage(
    Pal::PresentMode imagePresentSupport)
{
    uint32_t presentLayoutUsage = 0;

    switch (imagePresentSupport)
    {
    case Pal::PresentMode::Fullscreen:
        // In case of fullscreen presentation mode we may need to temporarily switch to windowed presents
        // so include both flags here.
        presentLayoutUsage = Pal::LayoutPresentWindowed | Pal::LayoutPresentFullscreen;
        break;

    case Pal::PresentMode::Windowed:
        presentLayoutUsage = Pal::LayoutPresentWindowed;
        break;

    default:
        VK_NEVER_CALLED();
        break;
    }

    return presentLayoutUsage;
}

// =====================================================================================================================
// This is a function used to convert RPImageLayouts to PAL equivalents.  These are basically Vulkan layouts but they
// are renderpass-specific instance specific and contain some extra internal requirements.
Pal::ImageLayout Image::GetAttachmentLayout(
    const RPImageLayout& layout,
    uint32_t             plane,
    const CmdBuffer*     pCmdBuffer
    ) const
{
    Pal::ImageLayout palLayout;

    palLayout = GetBarrierPolicy().GetAspectLayout(
    layout.layout, plane, pCmdBuffer->GetQueueFamilyIndex(), GetFormat());

    // Add any requested extra PAL usage
    palLayout.usages |= layout.extraUsage;

    return palLayout;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(
    VkDevice                                    device,
    VkImage                                     image,
    const VkAllocationCallbacks*                pAllocator)
{
    if (image != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Image::ObjectFromHandle(image)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return Image::ObjectFromHandle(image)->BindMemory(pDevice, memory, memoryOffset, 0, nullptr, 0, nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements*                       pMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    *pMemoryRequirements = Image::ObjectFromHandle(image)->GetMemoryRequirements();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    uint32_t*                                   pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements*            pSparseMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Image::ObjectFromHandle(image)->GetSparseMemoryRequirements(
        pDevice,
        pSparseMemoryRequirementCount,
        utils::ArrayView<VkSparseImageMemoryRequirements>(pSparseMemoryRequirements));
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Image::ObjectFromHandle(image)->GetSubresourceLayout(
        pDevice,
        pSubresource,
        pLayout);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2(
    VkDevice                                    device,
    const VkImageMemoryRequirementsInfo2*       pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Image* pImage = Image::ObjectFromHandle(pInfo->image);
    pMemoryRequirements->memoryRequirements = pImage->GetMemoryRequirements();

    VkMemoryDedicatedRequirements* pMemDedicatedRequirements =
        static_cast<VkMemoryDedicatedRequirements*>(pMemoryRequirements->pNext);

    if ((pMemDedicatedRequirements != nullptr) &&
        (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS))
    {
        pMemDedicatedRequirements->prefersDedicatedAllocation  = pImage->DedicatedMemoryRequired();
        pMemDedicatedRequirements->requiresDedicatedAllocation = pImage->DedicatedMemoryRequired();
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements2(
    VkDevice                                        device,
    const VkImageSparseMemoryRequirementsInfo2*     pInfo,
    uint32_t*                                       pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2*               pSparseMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Image* pImage = Image::ObjectFromHandle(pInfo->image);
    auto memReqsView = utils::ArrayView<VkSparseImageMemoryRequirements>(
        pSparseMemoryRequirements,
        &pSparseMemoryRequirements->memoryRequirements);
    pImage->GetSparseMemoryRequirements(pDevice, pSparseMemoryRequirementCount, memReqsView);
}

#if defined(__unix__)
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetImageDrmFormatModifierPropertiesEXT(
    VkDevice                                    device,
    VkImage                                     image,
    VkImageDrmFormatModifierPropertiesEXT*      pProperties)
{
    Image* pImage = Image::ObjectFromHandle(image);

    pProperties->drmFormatModifier = pImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo().modifier;

    return VK_SUCCESS;
}
#endif

} // namespace entry

} // namespace vk
