/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_object.h"
#include "include/vk_swapchain.h"
#include "include/vk_queue.h"
#include "include/peer_resource.h"

#include "palGpuMemory.h"
#include "palImage.h"
#include "palAutoBuffer.h"

namespace vk
{

// =====================================================================================================================
// This function computes certain "maximum" PAL layout usage and cache coher masks that this image can possibly affect
// based on its declared usage bits at create time.  These masks come in handy when trying to decide optimal PAL layouts
// and caches coherency flags given an input VkImageLayout during a pipeline barrier.
//
// For example, in Vulkan the GENERAL layout is a valid layout.  In such a layout, all operations supported by the
// image's usage are legal without an explicit layout transition in between.  For the GENERAL layout, we would use this
// computed layout mask directly.
//
// In another case, PAL can flexibly clear and copy images in a number of ways.  When transitioning to a
// TRANSFER_DST layout in preparation of a vkCmdClear* command, it may be beneficial to still maintain things
// like "LayoutDepthStencilTarget" if the image has declared such a usage so that PAL does not accidentally decompress
// the image.  To do this, a very lenient TRANSFER_DST mask is computed for various kinds of clears, which is
// then pruned to a legal subset based on this mask.
//
// See Image::GetLayoutFromUsage() where this is used.
void Image::CalcBarrierUsage(
    VkImageUsageFlags           usage,
    Pal::PresentMode*           pPresentMode)
{
    // By default, don't allow any layout usage other than uninitialized
    m_layoutUsageMask = Pal::LayoutUninitializedTarget;

    // Always allow CPU and memory reads/writes
    m_outputCacheMask = Pal::CoherCpu | Pal::CoherMemory;
    m_inputCacheMask  = Pal::CoherCpu | Pal::CoherMemory;

    if (pPresentMode != nullptr)
    {
        if (*pPresentMode == Pal::PresentMode::Fullscreen)
        {
            m_layoutUsageMask |= (Pal::LayoutPresentWindowed | Pal::LayoutPresentFullscreen);
        }
        else if (*pPresentMode == Pal::PresentMode::Windowed)
        {
            m_layoutUsageMask |= Pal::LayoutPresentWindowed;
        }
        else
        {
            VK_ASSERT(!"Unexpected present mode");
        }
    }

    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        m_layoutUsageMask |= Pal::LayoutCopySrc;

        // for single sample image, there is no true need to be in resolve source layout
        if (m_imageSamples > VK_SAMPLE_COUNT_1_BIT)
        {
            m_layoutUsageMask |= Pal::LayoutResolveSrc;
        }
        m_inputCacheMask  |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear;
    }

    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        m_layoutUsageMask |= Pal::LayoutCopyDst | Pal::LayoutResolveDst;
        m_outputCacheMask |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear;
    }

    if ((usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) != 0)
    {
        m_layoutUsageMask |= Pal::LayoutShaderRead;
        m_inputCacheMask  |= Pal::CoherShader;
    }

    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        m_layoutUsageMask |= Pal::LayoutShaderWrite;
        m_outputCacheMask |= Pal::CoherShader;
    }

    // Note that the below code enables clear support for color/depth targets because they can also be cleared inside
    // render passes (either as load op clears or vkCmdClearAttachments) which do not require the transfer destination
    // bit to be set.

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        m_layoutUsageMask |= Pal::LayoutColorTarget;

        VK_ASSERT(m_pDevice != VK_NULL_HANDLE);
        const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

        // Note here that we enable resolve support when color attachment bit is set, because MSAA color attachment image
        // is always expected to support ResolveSrc layout for render pass resolves sourcing it which needn't
        // TRANSFER_SRC_BIT to be specified. single sample color attachment image is always expected to support ResolveDst
        // layout for render pass resolves targeting it which needn't TRANSFER_DST_BIT to be specified.
        if (m_imageSamples > VK_SAMPLE_COUNT_1_BIT)
        {
            m_layoutUsageMask |= Pal::LayoutResolveSrc;
        }
        else
        {
            // If application creates image with usage bit of color_target and then use general layout
            // for the image to be resolve target, we need m_layoutUsageMask to cover
            // resolve_dst layout.
            // If app uses transfer dst usage bit instead, we should be safely covered. The benefit of
            // not setting resolvedst layout bit is, If application create image with usage of
            // color target and sampling, but some how use general layout the change between the read
            // and the write layout, having resolve_dst bit for all current ASICs means meta data needs to be
            // decompressed. That is not ideal.
            if (settings.optColorTargetUsageDoesNotContainResolveLayout == false)
            {
                m_layoutUsageMask |= Pal::LayoutResolveDst;
            }
        }

        m_outputCacheMask |= Pal::CoherColorTarget | Pal::CoherClear;
        m_inputCacheMask  |= Pal::CoherColorTarget | Pal::CoherClear;
    }

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        // See the above note for CoherClear.
        m_layoutUsageMask |= Pal::LayoutDepthStencilTarget;
        m_outputCacheMask |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
        m_inputCacheMask  |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
    }

    // We don't do anything special in case of transient attachment images
    VK_IGNORE(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
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
// Computes the priority level of this image based on its usage.
void Image::CalcMemoryPriority()
{
    const auto& settings = m_pDevice->GetRuntimeSettings();

    m_priority = MemoryPriority::FromSetting(settings.memoryPriorityDefault);

    UpgradeToHigherPriority(settings.memoryPriorityImageAny, &m_priority);

    if (m_layoutUsageMask & Pal::LayoutShaderRead)
    {
        UpgradeToHigherPriority(settings.memoryPriorityImageShaderRead, &m_priority);
    }

    if (m_layoutUsageMask & Pal::LayoutShaderWrite)
    {
        UpgradeToHigherPriority(settings.memoryPriorityImageShaderWrite, &m_priority);
    }

    if (m_layoutUsageMask & Pal::LayoutColorTarget)
    {
        UpgradeToHigherPriority(settings.memoryPriorityImageColorTarget, &m_priority);
    }

    if (m_layoutUsageMask & Pal::LayoutDepthStencilTarget)
    {
        UpgradeToHigherPriority(settings.memoryPriorityImageDepthStencil, &m_priority);
    }
}

// =====================================================================================================================
Image::Image(
    Device*                 pDevice,
    VkImageCreateFlags      flags,
    Pal::IImage**           pPalImages,
    Pal::IGpuMemory**       pPalMemory,
    VkExtent3D              tileSize,
    uint32_t                mipLevels,
    uint32_t                arraySize,
    VkFormat                imageFormat,
    VkSampleCountFlagBits   imageSamples,
    VkImageTiling           imageTiling,
    VkImageUsageFlags       usage,
    VkSharingMode           sharingMode,
    uint32_t                concurrentQueueFlags,
    ImageFlags              internalFlags,
    Pal::PresentMode*       pPresentMode)
    :
    m_pDevice(pDevice),
    m_flags(flags),
    m_pMemory(nullptr),
    m_tileSize(tileSize),
    m_mipLevels(mipLevels),
    m_arraySize(arraySize),
    m_format(imageFormat),
    m_imageSamples(imageSamples),
    m_imageTiling(imageTiling),
    m_imageUsage(usage),
    m_layoutUsageMask(0),
    m_outputCacheMask(0),
    m_inputCacheMask(0),
    m_sharingMode(sharingMode),
    m_concurrentQueueFlags(concurrentQueueFlags),
    m_pSwapChain(nullptr),
    m_baseAddrOffset(0)
{
    m_internalFlags.u32All = internalFlags.u32All;

    memset(m_pPalImages, 0, sizeof(m_pPalImages));
    memset(m_pPalMemory, 0, sizeof(m_pPalMemory));

    memcpy(m_pPalImages, pPalImages, sizeof(pPalImages[0]) * pDevice->NumPalDevices());

    if (pPalMemory != nullptr)
    {
        memcpy(m_pPalMemory, pPalMemory, sizeof(pPalMemory[0]) * pDevice->NumPalDevices());
    }

    memset(m_pSampleLocationsMetaDataMemory, 0, sizeof(m_pSampleLocationsMetaDataMemory));
    memset(m_sampleLocationsMetaDataOffset, 0, sizeof(m_sampleLocationsMetaDataOffset));

    CalcBarrierUsage(usage, pPresentMode);
    CalcMemoryPriority();
}

// =====================================================================================================================
static VkResult ConvertImageCreateInfo(
    const Device*            pDevice,
    const VkImageCreateInfo* pCreateInfo,
    Pal::ImageCreateInfo*    pPalCreateInfo,
    uint32_t*                pConcurrentQueueFlags)
{
    // Initialize sharing mode to concurrent and use all available queue's flags for the image layout.
    *pConcurrentQueueFlags = pDevice->GetSupportedBarrierQueues();

    VkResult               result     = VK_SUCCESS;
    VkImageUsageFlags      imageUsage = pCreateInfo->usage;
    const RuntimeSettings& settings   = pDevice->GetRuntimeSettings();

    // VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR indicates that the image can be created with usage flags that are not
    // supported for the format the image is created with but are supported for at least one format a VkImageView
    // created from the image can have.  For PAL, restrict the usage to only those supported for this format and set
    // formatChangeSrd and formatChangeTgt flags to handle the other usages.  This image will still contain the superset
    // of the usages to makes sure barriers properly handle each.
    if ((pCreateInfo->flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR) != 0)
    {
        VkFormatProperties formatProperties;

        pDevice->VkPhysicalDevice()->GetFormatProperties(pCreateInfo->format, &formatProperties);

        imageUsage &= VkFormatFeatureFlagsToImageUsageFlags((pCreateInfo->tiling == VK_IMAGE_TILING_OPTIMAL) ?
                                                            formatProperties.optimalTilingFeatures :
                                                            formatProperties.linearTilingFeatures);
    }

    memset(pPalCreateInfo, 0, sizeof(*pPalCreateInfo));

    pPalCreateInfo->extent.width     = pCreateInfo->extent.width;
    pPalCreateInfo->extent.height    = pCreateInfo->extent.height;
    pPalCreateInfo->extent.depth     = pCreateInfo->extent.depth;
    pPalCreateInfo->imageType        = VkToPalImageType(pCreateInfo->imageType);
    pPalCreateInfo->swizzledFormat   = VkToPalFormat(pCreateInfo->format);
    pPalCreateInfo->mipLevels        = pCreateInfo->mipLevels;
    pPalCreateInfo->arraySize        = pCreateInfo->arrayLayers;
    pPalCreateInfo->samples          = pCreateInfo->samples;
    pPalCreateInfo->fragments        = pCreateInfo->samples;
    pPalCreateInfo->tiling           = VkToPalImageTiling(pCreateInfo->tiling);
    pPalCreateInfo->tilingOptMode    = settings.imageTilingOptMode;
    pPalCreateInfo->tilingPreference = settings.imageTilingPreference;
    pPalCreateInfo->flags.u32All     = VkToPalImageCreateFlags(pCreateInfo->flags);
    pPalCreateInfo->usageFlags       = VkToPalImageUsageFlags(
                                         imageUsage,
                                         pCreateInfo->format,
                                         pCreateInfo->samples,
                                         (VkImageUsageFlags)(settings.optImgMaskToApplyShaderReadUsageForTransferSrc),
                                         (VkImageUsageFlags)(settings.optImgMaskToApplyShaderWriteUsageForTransferDst));

    if (((pCreateInfo->flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0) &&
        (pDevice->GetRuntimeSettings().ignoreMutableFlag == false))
    {
        // Set viewFormatCount to Pal::AllCompatibleFormats to indicate that all compatible formats can be used for
        // image views created from the image. This gets overridden later if VK_KHR_image_format_list is used.
        pPalCreateInfo->viewFormatCount = Pal::AllCompatibleFormats;
    }

    // Vulkan allows individual subresources to be transitioned from uninitialized layout which means we
    // have to set this bit for PAL to be able to support this.  This may have performance implications
    // regarding DCC.
    pPalCreateInfo->flags.perSubresInit = 1;

    if (pCreateInfo->sharingMode == VK_SHARING_MODE_CONCURRENT)
    {
        // In case of concurrent sharing mode collect the image layout queue flags to be used.
        *pConcurrentQueueFlags = 0;

        for (uint32_t i = 0; i < pCreateInfo->queueFamilyIndexCount; ++i)
        {
            *pConcurrentQueueFlags |= pDevice->GetQueueFamilyPalImageLayoutFlag(pCreateInfo->pQueueFamilyIndices[i]);
        }
    }

    return result;
}

// =====================================================================================================================
// Creates virtual memory allocation for sparse images.
static VkResult InitSparseVirtualMemory(
    Device*                         pDevice,
    const VkImageCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    Pal::IImage*                    pPalImage[MaxPalDevices],
    Pal::IGpuMemory*                pSparseMemory[MaxPalDevices],
    Pal::GpuMemoryCreateInfo*       pSparseMemCreateInfo,
    VkExtent3D*                     pSparseTileSize)
{
    VkResult result = VK_SUCCESS;

    Pal::GpuMemoryRequirements palReqs = {};

    pPalImage[DefaultDeviceIndex]->GetGpuMemoryRequirements(&palReqs);

    const VkDeviceSize sparseAllocGranularity = pDevice->GetProperties().virtualMemAllocGranularity;

    memset(pSparseMemCreateInfo, 0, sizeof(*pSparseMemCreateInfo));

    pSparseMemCreateInfo->flags.virtualAlloc = 1;
    pSparseMemCreateInfo->alignment          = Util::RoundUpToMultiple(sparseAllocGranularity, palReqs.alignment);
    pSparseMemCreateInfo->size               = Util::RoundUpToMultiple(palReqs.size, pSparseMemCreateInfo->alignment);
    pSparseMemCreateInfo->heapCount          = 0;

    // Virtual resource should return 0 on unmapped read if residencyNonResidentStrict is set.
    if (pDevice->VkPhysicalDevice()->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
    {
        pSparseMemCreateInfo->virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
    }

    size_t palMemSize = 0;

    for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < pDevice->NumPalDevices()); deviceIdx++)
    {
        Pal::Result palResult = Pal::Result::Success;

        palMemSize += pDevice->PalDevice()->GetGpuMemorySize(*pSparseMemCreateInfo, &palResult);

        if (palResult != Pal::Result::Success)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    // If it's a sparse image we should also cache sparse image block dimensions (tile size) to
    // optimize sparse binding update, keeping in mind that each supported aspect (color, depth,
    // stencil) is permitted to use different granularity
    uint32_t                      propertyCount = 1;
    VkSparseImageFormatProperties sparseFormatProperties;

    pDevice->VkPhysicalDevice()->GetSparseImageFormatProperties(
        pCreateInfo->format,
        pCreateInfo->imageType,
        pCreateInfo->samples,
        pCreateInfo->usage,
        pCreateInfo->tiling,
        &propertyCount,
        &sparseFormatProperties);

    *pSparseTileSize = sparseFormatProperties.imageGranularity;

    void* pPalMemoryObj = nullptr;

    if (result == VK_SUCCESS)
    {
        pPalMemoryObj = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            palMemSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pPalMemoryObj == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    size_t palMemOffset = 0;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < pDevice->NumPalDevices()) && (result == VK_SUCCESS);
        deviceIdx++)
    {
        Pal::Result palResult = pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
            *pSparseMemCreateInfo,
            Util::VoidPtrInc(pPalMemoryObj, palMemOffset),
            &pSparseMemory[deviceIdx]);

        if (palResult == Pal::Result::Success)
        {
            palResult = pPalImage[deviceIdx]->BindGpuMemory(pSparseMemory[deviceIdx], 0);
        }

        if (palResult == Pal::Result::Success)
        {
            palMemOffset += pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(*pSparseMemCreateInfo, &palResult);
        }

        if (palResult != Pal::Result::Success)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return result;
}

// =====================================================================================================================
static VkResult BindNullSparseMemory(
    Device*                         pDevice,
    VkImage                         sparseImage,
    const Pal::GpuMemoryCreateInfo& sparseMemCreateInfo)
{
    VkBindSparseInfo                  bindSparseInfo        = {};
    VkSparseImageOpaqueMemoryBindInfo imageOpaqueBind       = {};
    VkSparseMemoryBind                imageOpaqueMemoryBind = {};
    VkQueue                           universalQueue        = VK_NULL_HANDLE;
    vk::Queue*                        universalQueuePtr     = nullptr;

    VkResult result = pDevice->GetQueue(Pal::EngineTypeUniversal, 0, &universalQueue);

    if (result == VK_SUCCESS)
    {
        VK_ASSERT(universalQueue != VK_NULL_HANDLE);

        universalQueuePtr = ApiQueue::ObjectFromHandle(universalQueue);

        imageOpaqueMemoryBind.size           = sparseMemCreateInfo.size;

        imageOpaqueBind.image                = sparseImage;
        imageOpaqueBind.bindCount            = 1;
        imageOpaqueBind.pBinds               = &imageOpaqueMemoryBind;

        bindSparseInfo.sType                 = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        bindSparseInfo.imageOpaqueBindCount  = 1;
        bindSparseInfo.pImageOpaqueBinds     = &imageOpaqueBind;

        result = universalQueuePtr->BindSparse(1, &bindSparseInfo, VK_NULL_HANDLE);
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
    const size_t palImgSize = pDevice->PalDevice()->GetImageSize(*pPalCreateInfo, &palResult);
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
        const void*  pPalImgAddr = Util::VoidPtrInc(pMemory, 0);

        palResult = pDevice->PalDevice()->CreateImage(
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
// Create a new image object
VkResult Image::Create(
    Device*                         pDevice,
    const VkImageCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkImage*                        pImage)
{
    // Convert input create info
    Pal::ImageCreateInfo palCreateInfo  = {};
    uint32_t concurrentQueueFlags    = 0;
    Pal::PresentableImageCreateInfo presentImageCreateInfo = {};

    const VkImageCreateInfo*  pImageCreateInfo = nullptr;

    uint32_t                  viewFormatCount = 0;
    const VkFormat*           pViewFormats    = nullptr;

    const uint32_t numDevices = pDevice->NumPalDevices();
    const bool     isSparse   = (pCreateInfo->flags & SparseEnablingFlags) != 0;
    VkResult       result     = VkResult::VK_SUCCESS;

    union
    {
        const VkStructHeader*                       pHeader;
        const VkImageCreateInfo*                    pVkImageCreateInfo;
        const VkImageSwapchainCreateInfoKHX*        pVkImageSwapchainCreateInfoKHX;
        const VkExternalMemoryImageCreateInfoKHR*   pExternalMemoryImageCreateInfoKHR;
        const VkImageFormatListCreateInfoKHR*       pVkImageFormatListCreateInfoKHR;
    };

    ImageFlags imageFlags;

    imageFlags.u32All = 0;

    for (pVkImageCreateInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO:
        {
            VK_ASSERT(pCreateInfo == pVkImageCreateInfo);
            pImageCreateInfo = pVkImageCreateInfo;
            result = ConvertImageCreateInfo(pDevice, pImageCreateInfo, &palCreateInfo, &concurrentQueueFlags);

            break;
        }
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR:
        {
            palCreateInfo.flags.invariant = 1;

            VkExternalMemoryPropertiesKHR externalMemoryProperties = {};

            pDevice->VkPhysicalDevice()->GetExternalMemoryProperties(
                isSparse,
                static_cast<VkExternalMemoryHandleTypeFlagBitsKHR>(pExternalMemoryImageCreateInfoKHR->handleTypes),
                &externalMemoryProperties);

            if (externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR)
            {
                imageFlags.dedicatedRequired = true;
            }

            if (externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR |
                                                                   VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR))
            {
                imageFlags.externallyShareable = true;

                if ((pExternalMemoryImageCreateInfoKHR->handleTypes &
                    (VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR |
                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT_KHR |
                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHR |
                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR)) != 0)
                {
                    imageFlags.externalD3DHandle = true;
                }

                if ((pExternalMemoryImageCreateInfoKHR->handleTypes &
                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT) != 0)
                {
                    imageFlags.externalPinnedHost = true;
                }
            }

            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHX:
        {
            // TODO: SWDEV-120361 - peer memory swapchain binding not implemented yet.
            VK_NOT_IMPLEMENTED;
            break;
        }

        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR:
        {
            // Processing of the actual contents of this happens later due to AutoBuffer scoping.
            viewFormatCount = pVkImageFormatListCreateInfoKHR->viewFormatCount;
            pViewFormats    = pVkImageFormatListCreateInfoKHR->pViewFormats;
            break;
        }

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    Util::AutoBuffer<Pal::SwizzledFormat, 16, PalAllocator> palFormatList(
        viewFormatCount,
        pDevice->VkInstance()->Allocator());

    if (viewFormatCount > 0)
    {
        palCreateInfo.viewFormatCount = 0;
        palCreateInfo.pViewFormats    = &palFormatList[0];

        for (uint32_t i = 0; i < viewFormatCount; ++i)
        {
            // Skip any entries that specify the same format as the base format of the image as the PAL interface
            // expects that to be excluded from the list.
            if (pViewFormats[i] != pImageCreateInfo->format)
            {
                palFormatList[palCreateInfo.viewFormatCount++] = VkToPalFormat(pViewFormats[i]);
            }
        }
    }

    // If flags contains VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR, imageType must be VK_IMAGE_TYPE_3D
    VK_ASSERT(((pImageCreateInfo->flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR) == 0) ||
               (pImageCreateInfo->imageType == VK_IMAGE_TYPE_3D));

    if (imageFlags.androidPresentable)
    {
        VkDeviceMemory    pDeviceMemory = {};
        result = Image::CreatePresentableImage(
            pDevice,
            &presentImageCreateInfo,
            pAllocator,
            pCreateInfo->usage,
            Pal::PresentMode::Windowed,
            pImage,
            pCreateInfo->format,
            pCreateInfo->sharingMode,
            concurrentQueueFlags,
            &pDeviceMemory);
        Image* pTempImage = Image::ObjectFromHandle(*pImage);
        pTempImage->m_pMemory = Memory::ObjectFromHandle(pDeviceMemory);
        return result;
    }

    // Calculate required system memory size
    const size_t apiSize   = sizeof(Image);
    size_t       totalSize = apiSize;
    void*        pMemory   = nullptr;
    Pal::Result  palResult = Pal::Result::Success;

    const size_t palImgSize = pDevice->PalDevice()->GetImageSize(palCreateInfo, &palResult);
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
            pMemory = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                totalSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            if (pMemory == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }

    // Create PAL images
    Pal::IImage* pPalImages[MaxPalDevices] = {};
    const void*  pPalImgAddr  = Util::VoidPtrInc(pMemory, apiSize);
    size_t       palImgOffset = 0;

    if (result == VK_SUCCESS)
    {
        for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < pDevice->NumPalDevices()); deviceIdx++)
        {
            palResult = pDevice->PalDevice(deviceIdx)->CreateImage(
                palCreateInfo,
                Util::VoidPtrInc(pPalImgAddr, palImgOffset),
                &pPalImages[deviceIdx]);

            palImgOffset += palImgSize;

            if (palResult != Pal::Result::Success)
            {
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
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
    Pal::GpuMemoryCreateInfo sparseMemCreateInfo   = {};
    VkExtent3D sparseTileSize                      = {};

    if ((result == VK_SUCCESS) && isSparse)
    {
        result = InitSparseVirtualMemory(pDevice, pImageCreateInfo, pAllocator, pPalImages,
                                         pSparseMemory, &sparseMemCreateInfo, &sparseTileSize);
    }

    VkImage imageHandle = VK_NULL_HANDLE;

    if (result == VK_SUCCESS)
    {
        imageFlags.internalMemBound = isSparse;

        VK_PLACEMENT_NEW (pMemory) Image(
            pDevice,
            pImageCreateInfo->flags,
            pPalImages,
            pSparseMemory,
            sparseTileSize,
            palCreateInfo.mipLevels,
            palCreateInfo.arraySize,
            pImageCreateInfo->format,
            pImageCreateInfo->samples,
            pImageCreateInfo->tiling,
            pImageCreateInfo->usage,
            pImageCreateInfo->sharingMode,
            concurrentQueueFlags,
            imageFlags);

        imageHandle = Image::HandleFromVoidPointer(pMemory);
    }

    if (isSparse && (result == VK_SUCCESS))
    {
        // In order to disable page faults for this image in KMD, we need to explicitly bind null
        // physical memory to the virtual memory.
        VK_ASSERT(imageHandle != VK_NULL_HANDLE);

        result = BindNullSparseMemory(pDevice, imageHandle, sparseMemCreateInfo);
    }

    if (result == VK_SUCCESS)
    {
        *pImage = imageHandle;
    }
    else
    {
        if (imageHandle != VK_NULL_HANDLE)
        {
            Image::ObjectFromHandle(imageHandle)->Destroy(pDevice, pAllocator);
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
            pAllocator->pfnFree(pAllocator->pUserData, pMemory);
        }
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
    uint32_t                                concurrentQueueFlags,
    VkDeviceMemory*                         pDeviceMemory)
{
    Pal::Result palResult;

    Memory* pMemory = nullptr;

    // Allocate system memory for objects
    const uint32_t numDevices         = pDevice->NumPalDevices();
    const uint32_t allocateDeviceMask = pDevice->GetPalDeviceMask();
    const bool multiInstanceHeap      = true; // Always use a local heap for presentable images

    size_t palImgSize = 0;
    size_t palMemSize = 0;

    pDevice->PalDevice()->GetPresentableImageSizes(*pCreateInfo, &palImgSize, &palMemSize, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

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

    void* pImgObjMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(Image) + (palImgSize * numDevices),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pImgObjMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    const size_t peerMemorySize = PeerMemory::GetMemoryRequirements(
        pDevice, multiInstanceHeap, allocateDeviceMask, static_cast<uint32_t>(palMemSize));

    void* pMemObjMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(Memory) + (palMemSize * numDevices) + peerMemorySize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemObjMemory == nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pImgObjMemory);

        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Create the PAL image
    Pal::IImage*        pPalImage[MaxPalDevices]  = {};
    Pal::IGpuMemory*    pPalMemory[MaxPalDevices] = {};

    Pal::Result result = Pal::Result::Success;

    size_t palImgOffset = sizeof(Image);
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
    }

    PeerMemory* pPeerMemory = nullptr;
    if (peerMemorySize > 0)
    {
        pPeerMemory = VK_PLACEMENT_NEW(Util::VoidPtrInc(pMemObjMemory, palMemOffset)) PeerMemory(
            pDevice, pPalMemory, static_cast<uint32_t>(palMemSize));
    }

#ifdef DEBUG
    Pal::PeerImageOpenInfo peerInfo = {};
    peerInfo.pOriginalImage = pPalImage[0];

    for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        size_t peerImgSize = 0;
        size_t peerMemSize = 0;

        pDevice->PalDevice(deviceIdx)->GetPeerImageSizes(peerInfo, &peerImgSize, &peerMemSize, &palResult);

        // If any of these asserts begins firing, we will have a memory overwrite problem
        // in VkResult Image::BindSwapchainMemory()
        VK_ASSERT(palResult == Pal::Result::Success);
        VK_ASSERT(peerImgSize <= palImgSize);
        VK_ASSERT(peerMemSize <= palMemSize);
    }
#endif

    // from PAL, toomanyflippableAllocation is a warning, instead of a failure. the allocate should be success.
    // but when they warn us, future flippable image allocation may fail based on OS.
    if ((result == Pal::Result::Success) || (result == Pal::Result::TooManyFlippableAllocations))
    {
        // Presentable images are never sparse so tile size doesn't matter
        constexpr VkExtent3D dummyTileSize = {};

        // Default presentable images to a single mip and arraySize
        const uint32_t miplevels = 1;
        const uint32_t arraySize = 1;

        ImageFlags imageFlags;

        imageFlags.u32All            = 0;
        imageFlags.internalMemBound  = false;
        imageFlags.dedicatedRequired = true;

        // Create API image object
        VK_PLACEMENT_NEW (pImgObjMemory) Image(
            pDevice,
            0,
            pPalImage,
            nullptr,
            dummyTileSize,
            miplevels,
            arraySize,
            imageFormat,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            imageUsageFlags,
            sharingMode,
            concurrentQueueFlags,
            imageFlags,
            &presentMode);

        *pImage = Image::HandleFromVoidPointer(pImgObjMemory);

        // Create API memory object
        pMemory = VK_PLACEMENT_NEW(pMemObjMemory) Memory(pDevice, pPalMemory, pPeerMemory, allocateDeviceMask);

        *pDeviceMemory = Memory::HandleFromObject(pMemory);

        return VK_SUCCESS;
    }
    else
    {
        pAllocator->pfnFree(pAllocator->pUserData, pImgObjMemory);
        pAllocator->pfnFree(pAllocator->pUserData, pMemObjMemory);
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Destroy image object
VkResult Image::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalImages[deviceIdx] != nullptr)
        {
            bool skipDestroy = m_internalFlags.boundToSwapchainMemory ||
                               (m_internalFlags.boundToExternalMemory && (deviceIdx == DefaultDeviceIndex));

            if (skipDestroy == false)
            {
                m_pPalImages[deviceIdx]->Destroy();
            }
        }

        if ((m_pPalMemory[deviceIdx] != nullptr) && (m_internalFlags.internalMemBound != 0))
        {
            m_pDevice->RemoveMemReference(m_pDevice->PalDevice(deviceIdx), m_pPalMemory[deviceIdx]);
            m_pPalMemory[deviceIdx]->Destroy();
        }
    }

    if (IsSparse())
    {
        // Free the system memory allocated by InitSparseVirtualMemory
        pAllocator->pfnFree(pAllocator->pUserData, m_pPalMemory[0]);
    }

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

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

    VK_ASSERT(rectCount == 0);  // TODO: SWDEV-110556
                                // We have not exposed VK_IMAGE_CREATE_BIND_SFR_BIT so rectCount must be zero.
    if (deviceIndexCount != 0)
    {
        // Binding Indices were supplied
        VK_ASSERT((deviceIndexCount == numDevices) && (rectCount == 0) && (multiInstanceHeap == true));

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
    VkDeviceMemory     mem,
    VkDeviceSize       memOffset,
    uint32_t           deviceIndexCount,
    const uint32_t*    pDeviceIndices,
    uint32_t           rectCount,
    const VkRect2D*    pRects)
{
    VkMemoryRequirements reqs = {};

    if (GetMemoryRequirements(&reqs) == VK_SUCCESS)
    {
        Pal::gpusize baseAddrOffset = 0;

        m_pMemory = (mem != VK_NULL_HANDLE) ? Memory::ObjectFromHandle(mem) : nullptr;

        if (m_internalFlags.externallyShareable && (m_pMemory->GetExternalPalImage() != nullptr))
        {
            // For MGPU, the external sharing resource only uses the first PAL image.
            m_pPalImages[DefaultDeviceIndex]->Destroy();
            m_pPalImages[DefaultDeviceIndex]      = m_pMemory->GetExternalPalImage();
            m_internalFlags.boundToExternalMemory = 1;
        }

        Pal::Result result = Pal::Result::Success;

        const uint32_t numDevices = m_pDevice->NumPalDevices();

        uint8_t bindIndices[MaxPalDevices];
        GenerateBindIndices(numDevices,
            bindIndices,
            deviceIndexCount,
            pDeviceIndices,
            rectCount,
            pRects,
            ((m_pMemory == nullptr) ? false : m_pMemory->IsMultiInstance()));

        for (uint32_t localDeviceIdx = 0; localDeviceIdx < numDevices; localDeviceIdx++)
        {
            const uint32_t sourceMemInst = bindIndices[localDeviceIdx];

            m_multiInstanceIndices[localDeviceIdx] = sourceMemInst;

            Pal::IImage*     pPalImage = m_pPalImages[localDeviceIdx];
            Pal::IGpuMemory* pGpuMem   = nullptr;

            if (m_pMemory != nullptr)
            {
                if ((localDeviceIdx == sourceMemInst) || m_pMemory->IsMirroredAllocation(localDeviceIdx))
                {
                    pGpuMem = m_pMemory->PalMemory(localDeviceIdx);
                }
                else
                {
                    pGpuMem = m_pMemory->GetPeerMemory()->AllocatePeerMemory(
                        m_pDevice->PalDevice(localDeviceIdx), localDeviceIdx, sourceMemInst);
                }

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
                    VK_ASSERT(baseAddrOffset <= CalcBaseAddrSizePadding(*m_pDevice, reqs));
                }

                // After applying any necessary base address offset, the full GPU address should be aligned
                VK_ASSERT(Util::IsPow2Aligned(baseGpuAddr + baseAddrOffset + memOffset, reqs.alignment));

                m_pMemory->ElevatePriority(m_priority);
            }

            result = pPalImage->BindGpuMemory(pGpuMem, baseAddrOffset + memOffset);
        }

        if (result == Pal::Result::Success)
        {
            // Record the private base address offset.  This is necessary for things like subresource layout
            // calculation for linear images.
            m_baseAddrOffset = baseAddrOffset;
        }

        return PalToVkResult(result);
    }
    else
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
}

// =====================================================================================================================
// Binds to Gpu memory already allocated to a swapchain object
VkResult Image::BindSwapchainMemory(
    uint32_t           swapChainImageIndex,
    SwapChain*         pSwapchain,
    uint32_t           deviceIndexCount,
    const uint32_t*    pDeviceIndices,
    uint32_t           rectCount,
    const VkRect2D*    pRects)
{
    const uint32_t numDevices = m_pDevice->NumPalDevices();

    // We need to destroy the unbound pal image objects because the swap chain image we are about to bind probably
    // has different compression capabilities.
    for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
    {
        m_pPalImages[deviceIdx]->Destroy();
    }

    // Ensure we do not later destroy the Pal image objects that we bind in this function.
    m_internalFlags.boundToSwapchainMemory = 1;

    const SwapChain::Properties& properties = pSwapchain->GetProperties();

    m_pSwapChain = pSwapchain;
    m_pMemory    = Memory::ObjectFromHandle(properties.imageMemory[swapChainImageIndex]);

    Image*  pSwapchainImage    = Image::ObjectFromHandle(properties.images[swapChainImageIndex]);
    Memory* pSwapchainImageMem = Memory::ObjectFromHandle(properties.imageMemory[swapChainImageIndex]);

    constexpr uint32_t presentMask = (Pal::LayoutPresentWindowed | Pal::LayoutPresentFullscreen);

    // Clear present flags and inherit from the swapchain image that owns the underlying memory
    m_layoutUsageMask |= pSwapchainImage->GetSupportedLayouts();

    uint8_t bindIndices[MaxPalDevices];
    GenerateBindIndices(numDevices,
                        bindIndices,
                        deviceIndexCount,
                        pDeviceIndices,
                        rectCount,
                        pRects,
                        ((m_pMemory == nullptr) ? false : m_pMemory->IsMultiInstance()));

    for (uint32_t localDeviceIdx = 0; localDeviceIdx < numDevices; localDeviceIdx++)
    {
        const uint32_t sourceMemInst = bindIndices[localDeviceIdx];

        m_multiInstanceIndices[localDeviceIdx] = sourceMemInst;

        if (localDeviceIdx == sourceMemInst)
        {
            m_pPalImages[localDeviceIdx] = pSwapchainImage->PalImage(localDeviceIdx);
        }
        else
        {
            Pal::IDevice* pPalDevice = m_pDevice->PalDevice(localDeviceIdx);
            Pal::IImage* pPalImage   = pSwapchainImage->PalImage(localDeviceIdx);

            Pal::PeerImageOpenInfo peerInfo = {};
            peerInfo.pOriginalImage = pPalImage;

            Pal::IGpuMemory* pGpuMemory = m_pMemory->GetPeerMemory()->AllocatePeerMemory(
                m_pDevice->PalDevice(localDeviceIdx), localDeviceIdx, sourceMemInst);

            void* pImageMem = m_pPalImages[localDeviceIdx];

            Pal::Result palResult = pPalDevice->OpenPeerImage(
                peerInfo, pImageMem, nullptr, &m_pPalImages[localDeviceIdx], &pGpuMemory);

            VK_ASSERT(palResult == Pal::Result::Success);
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Implementation of vkGetImageSubresourceLayout
VkResult Image::GetSubresourceLayout(
    const VkImageSubresource* pSubresource,
    VkSubresourceLayout*      pLayout
    ) const
{
    // Request the subresource information from PAL
    Pal::SubresLayout palLayout = {};
    Pal::SubresId palSubResId = {};

    palSubResId.aspect     = VkToPalImageAspectSingle(pSubresource->aspectMask);
    palSubResId.mipLevel   = pSubresource->mipLevel;
    palSubResId.arraySlice = pSubresource->arrayLayer;

    Pal::Result palResult = PalImage(DefaultDeviceIndex)->GetSubresourceLayout(palSubResId, &palLayout);

    if (palResult != Pal::Result::Success)
    {
        return PalToVkResult(palResult);
    }

    const Pal::ImageCreateInfo& createInfo = PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

    pLayout->offset     = m_baseAddrOffset + palLayout.offset;
    pLayout->size       = palLayout.size;
    pLayout->rowPitch   = palLayout.rowPitch;
    pLayout->arrayPitch = (createInfo.arraySize > 1)    ? palLayout.depthPitch : 0;
    pLayout->depthPitch = (createInfo.extent.depth > 1) ? palLayout.depthPitch : 0;

    return VK_SUCCESS;
}

// =====================================================================================================================
// Implementation of vkGetImageSparseMemoryRequirements
void Image::GetSparseMemoryRequirements(
    uint32_t*                        pNumRequirements,
    VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
{
    // TODO: Use PAL interface for probing aspect availability, once it is introduced.
    uint32_t               usedAspectsCount    = 0;
    const bool             isSparse            = PalImage(DefaultDeviceIndex)->GetImageCreateInfo().flags.prt;
    bool                   needsMetadataAspect = false;
    Pal::Result            palResult;
    const PhysicalDevice*  physDevice          = m_pDevice->VkPhysicalDevice();

    // Count the number of aspects
    struct
    {
        Pal::ImageAspect      aspectPal;
        VkImageAspectFlagBits aspectVk;
        bool                  available;
    } aspects[] =
    {
        {Pal::ImageAspect::Color,   VK_IMAGE_ASPECT_COLOR_BIT,   vk::Formats::IsColorFormat(m_format)},
        {Pal::ImageAspect::Depth,   VK_IMAGE_ASPECT_DEPTH_BIT,   vk::Formats::HasDepth     (m_format)},
        {Pal::ImageAspect::Stencil, VK_IMAGE_ASPECT_STENCIL_BIT, vk::Formats::HasStencil   (m_format)}
    };
    uint32_t supportedAspectsCount = sizeof(aspects) / sizeof(aspects[0]);

    const Pal::ImageMemoryLayout& memoryLayout = PalImage()->GetMemoryLayout();

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
    else if (isSparse && (pSparseMemoryRequirements != nullptr) && (*pNumRequirements >= 1))
    {
        const uint32_t aspectsToReportCount = Util::Min(*pNumRequirements, usedAspectsCount);
        uint32_t       reportedAspectsCount = 0;

        // Get the memory layout of the sparse image

        for (uint32_t nAspect = 0; nAspect < supportedAspectsCount; ++(int&)nAspect)
        {
            const auto&                      currentAspect       = aspects[nAspect];
            Pal::SubresLayout                miptailLayouts[2]   = {};
            VkSparseImageMemoryRequirements* pCurrentRequirement = pSparseMemoryRequirements + reportedAspectsCount;

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
                        currentAspect.aspectPal,
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
            pCurrentRequirement->imageMipTailSize     = memoryLayout.prtMipTailTileCount *
                                                        physDevice->PalProperties().imageProperties.prtTileSize;

            // for per-slice-miptail, the miptail should only take one tile and the base address is tile aligned.
            // for single-miptail, the offset of first in-miptail mip level of slice 0 refers to the offset of miptail.
            pCurrentRequirement->imageMipTailOffset   = Util::RoundDownToMultiple(miptailLayouts[0].offset,
                                                        physDevice->PalProperties().imageProperties.prtTileSize);

            pCurrentRequirement->imageMipTailStride   = (miptailLayoutCount > 1)
                                                      ? miptailLayouts[1].offset - miptailLayouts[0].offset : 0;
        }

        if (needsMetadataAspect && reportedAspectsCount < *pNumRequirements)
        {
            VkSparseImageMemoryRequirements* pCurrentRequirement = pSparseMemoryRequirements + reportedAspectsCount;
            Pal::GpuMemoryRequirements       palReqs             = {};

            PalImage()->GetGpuMemoryRequirements(&palReqs);

            pCurrentRequirement->formatProperties.aspectMask       = VK_IMAGE_ASPECT_METADATA_BIT;
            pCurrentRequirement->formatProperties.flags            = VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
            pCurrentRequirement->formatProperties.imageGranularity = {0};
            pCurrentRequirement->imageMipTailFirstLod              = 0;
            pCurrentRequirement->imageMipTailSize                  = Util::RoundUpToMultiple(memoryLayout.metadataSize,
                                                                                             palReqs.alignment);
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
// This function converts a set of PAL layout usages to a full PAL ImageLayout which includes which queues this layout
// will also be used for.  The usages ("palLayoutUsages") can be obtained from a VkImageLayout by calling
// VkToPalImageLayoutUsages() or by calling the simpler function GetLayout().
//
// The PAL ImageLayout also contains a description of which queue types this layout is compatible with and this depends
// on the image's sharing mode.  If queueFamilyIndex is VK_QUEUE_FAMILY_IGNORED and the image's sharing mode is
// VK_SHARING_MODE_EXCLUSIVE, then pCmdBuffer must not be nullptr as that command buffer's queue family is substituted.
//
// If the image's sharing mode is VK_SHARING_MODE_CONCURRENT, then queueFamilyIndex is always ignored and the layout
// will always be compatible on all queue families that were declared during image creation.
Pal::ImageLayout Image::GetLayoutFromUsage(
    uint32_t         palLayoutUsages,
    const CmdBuffer* pCmdBuffer,
    uint32_t         queueFamilyIndex
    ) const
{
    Pal::ImageLayout layout;

    // Exclude layout usages that the image usage flags themselves prohibit.
    layout.usages = (palLayoutUsages & m_layoutUsageMask);

    if ((layout.usages & Pal::LayoutShaderRead)                  &&
        m_pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead &&
        (PalImage()->GetImageCreateInfo().samples > 1)          &&
        PalImage()->GetImageCreateInfo().usageFlags.shaderRead  &&
        PalImage()->GetImageCreateInfo().usageFlags.colorTarget)
    {
        // Replace LayoutShaderRead with LayoutShaderFmaskBasedRead for resources that support FMASK based read
        layout.usages &= ~(Pal::LayoutShaderRead);
        layout.usages |= Pal::LayoutShaderFmaskBasedRead;
    }

    layout.engines = GetLayoutEngines(pCmdBuffer, queueFamilyIndex);

    // If the layout usage is 0, it likely means that an application is trying to transition to an image layout that
    // is not supported by that image's usage flags.
    VK_ASSERT(layout.usages != 0);

    return layout;
}

// =====================================================================================================================
// Get the image's memory requirements
VkResult Image::GetMemoryRequirements(
    VkMemoryRequirements* pReqs)
{
    const bool                 isSparse           = IsSparse();
    Pal::GpuMemoryRequirements palReqs            = {};
    const auto                 virtualGranularity = m_pDevice->GetProperties().virtualMemAllocGranularity;

    PalImage(DefaultDeviceIndex)->GetGpuMemoryRequirements(&palReqs);

    if (isSparse)
    {
        pReqs->alignment = Util::RoundUpToMultiple(virtualGranularity, palReqs.alignment);
    }
    else
    {
        pReqs->alignment = palReqs.alignment;
    }

    pReqs->memoryTypeBits = 0;
    pReqs->size           = palReqs.size;

    for (uint32_t i = 0; i < palReqs.heapCount; ++i)
    {
        uint32_t typeIndex;

        if (m_pDevice->GetVkTypeIndexFromPalHeap(palReqs.heaps[i], &typeIndex))
        {
            pReqs->memoryTypeBits |= 1 << typeIndex;
        }
    }

    // Limit heaps to those compatible with pinned system memory
    if (m_internalFlags.externalPinnedHost)
    {
        pReqs->memoryTypeBits &= m_pDevice->GetPinnedSystemMemoryTypes();

        VK_ASSERT(pReqs->memoryTypeBits != 0);
    }

    // Adjust the size to account for internal padding required to align the base address
    pReqs->size += CalcBaseAddrSizePadding(*m_pDevice, *pReqs);

    if (isSparse)
    {
        pReqs->size = Util::RoundUpToMultiple(palReqs.size, pReqs->alignment);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// This is a function used to convert RPImageLayouts to PAL equivalents.  These are basically Vulkan layouts but they
// are renderpass-specific instance specific and contain some extra internal requirements.
Pal::ImageLayout Image::GetAttachmentLayout(
    const RPImageLayout&  layout,
    Pal::ImageAspect      aspect,
    const CmdBuffer*      pCmdBuffer
    ) const
{
    Pal::ImageLayout palLayout;

    if (((aspect == Pal::ImageAspect::Color)   && Formats::IsColorFormat(m_format)) ||
        ((aspect == Pal::ImageAspect::Depth)   && Formats::HasDepth(m_format))      ||
        ((aspect == Pal::ImageAspect::Stencil) && Formats::HasStencil(m_format)))
    {
        // Convert the Vulkan image layout to PAL image layout usages (these may be aspect specific)
        uint32_t palLayoutUsages[MaxRangePerAttachment];

        VkToPalImageLayoutUsages(layout.layout, m_format, palLayoutUsages);

        uint32_t layoutUsage;

        if ((aspect == Pal::ImageAspect::Color) ||
            (aspect == Pal::ImageAspect::Depth) ||
            (Formats::HasDepth(m_format) == false)) // Stencil aspect for stencil-only format
        {
            layoutUsage = palLayoutUsages[0];
        }
        else
        {
            // Stencil-aspect usages for combined depth-stencil formats usages are returned in usages[1]
            VK_ASSERT(aspect == Pal::ImageAspect::Stencil && Formats::HasDepth(m_format));

            layoutUsage = palLayoutUsages[1];
        }

        // Add any requested extra PAL usage
        layoutUsage |= layout.extraUsage;

        // Return full optimized layout
        palLayout = GetLayoutFromUsage(layoutUsage, pCmdBuffer, VK_QUEUE_FAMILY_IGNORED);
    }
    else
    {
        // Return a null-usage layout (set the engine still because there are some PAL asserts that hit)
        palLayout.usages  = 0;
        palLayout.engines = GetLayoutEngines(pCmdBuffer, VK_QUEUE_FAMILY_IGNORED);
    }

    return palLayout;
}

// =====================================================================================================================
// Returns the Pal::ImageLayout::engines part of a PAL image layout.  Called by a number of the more general layout
// conversion routines within this class.
VK_INLINE uint32_t Image::GetLayoutEngines(
    const CmdBuffer* pCmdBuffer,
    uint32_t         queueFamilyIndex
    ) const
{
    uint32_t engines;
    // For now, we only want to full decompress the image if queueFamilyIndex is VK_QUEUE_FAMILY_EXTERNAL_KHR, and it's
    // equal to set engines to Pal::LayoutAllEngines.
    if (queueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL_KHR)
    {
        engines = Pal::LayoutAllEngines;
    }
    else if (m_sharingMode == VK_SHARING_MODE_EXCLUSIVE)
    {
        if (queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
        {
            queueFamilyIndex = pCmdBuffer->GetQueueFamilyIndex();
        }

        // In case of exclusive sharing mode simply use the target queue flag.
        engines = m_pDevice->GetQueueFamilyPalImageLayoutFlag(queueFamilyIndex);

        // The target queue flag should identify one of the queue types requested at device creation.
        VK_ASSERT((m_pDevice->GetSupportedBarrierQueues() & engines) != 0);
    }
    else
    {
        VK_ASSERT(m_sharingMode == VK_SHARING_MODE_CONCURRENT);

        // In case of concurrent sharing mode, use the concurrent queue flags determined at image creation time.
        engines = m_concurrentQueueFlags;
    }

    return engines;
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
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
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
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return Image::ObjectFromHandle(image)->BindMemory(memory, memoryOffset, 0, nullptr, 0, nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements*                       pMemoryRequirements)
{
    Image::ObjectFromHandle(image)->GetMemoryRequirements(pMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    uint32_t*                                   pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements*            pSparseMemoryRequirements)
{
    Image::ObjectFromHandle(image)->GetSparseMemoryRequirements(
        pSparseMemoryRequirementCount,
        pSparseMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout)
{
    Image::ObjectFromHandle(image)->GetSubresourceLayout(
        pSubresource,
        pLayout);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2KHR(
    VkDevice                                    device,
    const VkImageMemoryRequirementsInfo2KHR*    pInfo,
    VkMemoryRequirements2KHR*                   pMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);
    VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::KHR_GET_MEMORY_REQUIREMENTS2));

    union
    {
        const VkStructHeader*                    pHeader;
        const VkImageMemoryRequirementsInfo2KHR* pRequirementsInfo2;
    };

    pRequirementsInfo2 = pInfo;
    pHeader = utils::GetExtensionStructure(pHeader, VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR);

    if (pHeader != nullptr)
    {
        VkMemoryRequirements* pMemReq = &pMemoryRequirements->memoryRequirements;
        Image* pImage = Image::ObjectFromHandle(pRequirementsInfo2->image);
        pImage->GetMemoryRequirements(pMemReq);

        if (pMemoryRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR)
        {
            VkMemoryDedicatedRequirementsKHR* pMemDedicatedRequirements =
                static_cast<VkMemoryDedicatedRequirementsKHR*>(pMemoryRequirements->pNext);

            if ((pMemDedicatedRequirements != nullptr) &&
                (pMemDedicatedRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR))
            {
                pMemDedicatedRequirements->prefersDedicatedAllocation  = pImage->DedicatedMemoryRequired();
                pMemDedicatedRequirements->requiresDedicatedAllocation = pImage->DedicatedMemoryRequired();
            }
        }
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements2KHR(
    VkDevice                                        device,
    const VkImageSparseMemoryRequirementsInfo2KHR*  pInfo,
    uint32_t*                                       pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2KHR*            pSparseMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);
    VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::KHR_GET_MEMORY_REQUIREMENTS2));

    union
    {
        const VkStructHeader*                          pHeader;
        const VkImageSparseMemoryRequirementsInfo2KHR* pRequirementsInfo2;
    };

    pRequirementsInfo2 = pInfo;
    pHeader = utils::GetExtensionStructure(pHeader, VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2_KHR);

    if (pHeader != nullptr)
    {
        VkSparseImageMemoryRequirements* pMemReq = &pSparseMemoryRequirements->memoryRequirements;
        Image* pImage = Image::ObjectFromHandle(pRequirementsInfo2->image);
        pImage->GetSparseMemoryRequirements(pSparseMemoryRequirementCount, pMemReq);
    }
}

} // namespace entry

} // namespace vk
