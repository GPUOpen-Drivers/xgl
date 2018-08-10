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
 * @file  vk_physical_device.cpp
 * @brief Contains implementation of Vulkan physical device.
 ***********************************************************************************************************************
 */

#include "include/khronos/vulkan.h"
#include "include/color_space_helper.h"
#include "include/vk_buffer_view.h"
#include "include/vk_dispatch.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_physical_device_manager.h"
#include "include/vk_display.h"
#include "include/vk_display_manager.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "include/vk_surface.h"

#include "include/vert_buf_binding_mgr.h"
#include "include/khronos/vk_icd.h"

#include "llpc.h"

#include "res/ver.h"

#include "settings/settings.h"

#include "palDevice.h"
#include "palCmdBuffer.h"
#include "palFormatInfo.h"
#include "palLib.h"
#include "palMath.h"
#include "palMsaaState.h"
#include "palScreen.h"
#include "palHashLiteralString.h"
#include <vector>

#undef max
#undef min

#include <new>
#include <cstring>
#include <algorithm>
#include <climits>
#include <type_traits>

namespace vk
{

// Vulkan Spec Table 30.11: All features in optimalTilingFeatures
constexpr VkFormatFeatureFlags AllImgFeatures =
    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
    VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
    VK_FORMAT_FEATURE_BLIT_SRC_BIT |
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
    VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
    VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT |
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT |
    VK_FORMAT_FEATURE_BLIT_DST_BIT;

// Vulkan Spec Table 30.12: All features in bufferFeatures
constexpr VkFormatFeatureFlags AllBufFeatures =
    VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT |
    VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT |
    VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT |
    VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;

#if PAL_ENABLE_PRINTS_ASSERTS
static void VerifyProperties(const PhysicalDevice& device);
#endif

// =====================================================================================================================
static bool VerifyFormatSupport(
    const PhysicalDevice& device,
    VkFormat              format,
    uint32_t              sampledImageBit,
    uint32_t              blitSrcBit,
    uint32_t              sampledImageFilterLinearBit,
    uint32_t              storageImageBit,
    uint32_t              storageImageAtomicBit,
    uint32_t              colorAttachmentBit,
    uint32_t              blitDstBit,
    uint32_t              colorAttachmentBlendBit,
    uint32_t              depthStencilAttachmentBit,
    uint32_t              vertexBufferBit,
    uint32_t              uniformTexelBufferBit,
    uint32_t              storageTexelBufferBit,
    uint32_t              storageTexelBufferAtomicBit)
{
    bool supported = true;

    VkFormatProperties props = {};

    VkResult result = device.GetFormatProperties(format, &props);

    if (result == VK_SUCCESS)
    {
        VK_ASSERT((props.optimalTilingFeatures & ~AllImgFeatures) == 0);
        VK_ASSERT((props.linearTilingFeatures & ~AllImgFeatures) == 0);
        VK_ASSERT((props.bufferFeatures & ~AllBufFeatures) == 0);

        if (sampledImageBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;

            // Formats that are required to support VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT must also support
            // VK_FORMAT_FEATURE_TRANSFER_SRC_BIT and VK_FORMAT_FEATURE_TRANSFER_DST_BIT.
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0;
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0;
        }

        if (blitSrcBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0;
        }

        if (sampledImageFilterLinearBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
        }

        if (storageImageBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        }

        if (storageImageAtomicBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) != 0;
        }

        if (colorAttachmentBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
        }

        if (blitDstBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
        }

        if (colorAttachmentBlendBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0;
        }

        if (depthStencilAttachmentBit)
        {
            supported &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
        }

        if (vertexBufferBit)
        {
            supported &= (props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0;
        }

        if (uniformTexelBufferBit)
        {
            supported &= (props.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) != 0;
        }

        if (storageTexelBufferBit)
        {
            supported &= (props.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT) != 0;
        }

        if (storageTexelBufferAtomicBit)
        {
            supported &= (props.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT) != 0;
        }
    }
    else
    {
        supported = false;
    }

    return supported;
}

// =====================================================================================================================
// Returns true if the given physical device supports the minimum required BC compressed texture format
// requirements
static bool VerifyBCFormatSupport(
    const PhysicalDevice& dev)
{
    // Based on Vulkan Spec Table 30.20. Mandatory format support: BC compressed formats with VkImageType VK_IMAGE_TYPE_2D and
    // VK_IMAGE_TYPE_3D.
    const bool bcSupport =
        VerifyFormatSupport(dev, VK_FORMAT_BC1_RGB_UNORM_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC1_RGB_SRGB_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC1_RGBA_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC2_UNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC2_SRGB_BLOCK,       1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC3_UNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC3_SRGB_BLOCK,       1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC4_UNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC4_SNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC5_UNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC5_SNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC6H_UFLOAT_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC6H_SFLOAT_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC7_UNORM_BLOCK,      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_BC7_SRGB_BLOCK,       1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    return bcSupport;
}

// =====================================================================================================================
PhysicalDevice::PhysicalDevice(
    PhysicalDeviceManager*  pPhysicalDeviceManager,
    Pal::IDevice*           pPalDevice,
    const RuntimeSettings&  settings,
    AppProfile              appProfile
    )
    :
    m_pPhysicalDeviceManager(pPhysicalDeviceManager),
    m_pPalDevice(pPalDevice),
    m_memoryTypeMask(0),
    m_settings(settings),
    m_sampleLocationSampleCounts(0),
    m_vrHighPrioritySubEngineIndex(UINT32_MAX),
    m_queueFamilyCount(0),
    m_appProfile(appProfile),
    m_supportedExtensions(),
    m_compiler(this)
{
    memset(&m_limits, 0, sizeof(m_limits));
    memset(m_formatFeatureMsaaTarget, 0, sizeof(m_formatFeatureMsaaTarget));
    memset(&m_queueFamilies, 0, sizeof(m_queueFamilies));
    memset(&m_memoryProperties, 0, sizeof(m_memoryProperties));
    memset(&m_gpaProps, 0, sizeof(m_gpaProps));
    for (uint32_t i = 0; i < VK_MEMORY_TYPE_NUM; i++)
    {
        m_memoryPalHeapToVkIndex[i] = VK_MEMORY_TYPE_NUM; // invalid index
        m_memoryVkIndexToPalHeap[i] = Pal::GpuHeapCount; // invalid index
    }
}

// =====================================================================================================================
// Creates a new Vulkan physical device object
VkResult PhysicalDevice::Create(
    PhysicalDeviceManager* pPhysicalDeviceManager,
    Pal::IDevice*          pPalDevice,
    const RuntimeSettings& settings,
    AppProfile             appProfile,
    VkPhysicalDevice*      pPhysicalDevice)
{
    VK_ASSERT(pPhysicalDeviceManager != nullptr);

    void* pMemory = pPhysicalDeviceManager->VkInstance()->AllocMem(sizeof(ApiPhysicalDevice),
                                                                   VK_DEFAULT_MEM_ALIGN,
                                                                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VK_INIT_DISPATCHABLE(PhysicalDevice, pMemory, (pPhysicalDeviceManager, pPalDevice, settings, appProfile));

    VkPhysicalDevice handle = reinterpret_cast<VkPhysicalDevice>(pMemory);

    PhysicalDevice* pObject = ApiPhysicalDevice::ObjectFromHandle(handle);

    VkResult result = pObject->Initialize();

    if (result == VK_SUCCESS)
    {
        *pPhysicalDevice = handle;
    }
    else
    {
        pObject->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Converts from PAL format feature properties to Vulkan equivalents.
static void GetFormatFeatureFlags(
    const Pal::MergedFormatPropertiesTable& formatProperties,
    VkFormat                                format,
    VkImageTiling                           imageTiling,
    const bool                              multiChannelMinMaxFilter,
    VkFormatFeatureFlags*                   pOutFormatFeatureFlags)
{
    const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(format);

    const size_t formatIdx = static_cast<size_t>(swizzledFormat.format);
    const size_t tilingIdx = ((imageTiling == VK_IMAGE_TILING_LINEAR) ? Pal::IsLinear : Pal::IsNonLinear);

    VkFormatFeatureFlags retFlags = PalToVkFormatFeatureFlags(formatProperties.features[formatIdx][tilingIdx]);

    // Only expect vertex buffer support for core formats for now (change this if needed otherwise in the future).
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT))
    {
        if ((imageTiling == VK_IMAGE_TILING_LINEAR) &&
            Llpc::ICompiler::IsVertexFormatSupported(format))
        {
            retFlags |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
        }
    }

    // As in Vulkan we have to return support for VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT based on
    // the depth aspect for depth-stencil images we have to handle this case explicitly here.
    if (Formats::HasDepth(format) && ((retFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0))
    {
        Pal::SwizzledFormat depthFormat = VkToPalFormat(Formats::GetAspectFormat(format, VK_IMAGE_ASPECT_DEPTH_BIT));

        const size_t depthFormatIdx = static_cast<size_t>(depthFormat.format);

        VkFormatFeatureFlags depthFlags = PalToVkFormatFeatureFlags(
            formatProperties.features[depthFormatIdx][tilingIdx]);

        if ((depthFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
        {
            retFlags |= (depthFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        }

        // According to the Vulkan Spec (section 32.2.0)
        // Re: VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT - If the format is a depth / stencil format,
        // this bit only indicates that the depth aspect(not the stencil aspect) of an image of this format
        // supports min/max filtering.
        if ((depthFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT) != 0)
        {
            retFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT;
        }
    }

    if (!Formats::IsDepthStencilFormat(format))
    {
        retFlags &= ~VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    *pOutFormatFeatureFlags = retFlags;
}

// =====================================================================================================================
VkResult PhysicalDevice::Initialize()
{
    const bool nullGpu = VkInstance()->IsNullGpuModeEnabled();

    // Collect generic device properties
    Pal::Result result = m_pPalDevice->GetProperties(&m_properties);

    if (result == Pal::Result::Success)
    {
        // Finalize the PAL device
        Pal::DeviceFinalizeInfo finalizeInfo = {};

        // Ask PAL to create the maximum possible number of engines.  We ask for maximum support because this has to be
        // done before the first Vulkan device is created, and we do not yet know exactly how many engines are needed
        // by those devices.
        if (nullGpu == false)
        {
            for (uint32_t idx = 0; idx < Pal::EngineTypeCount; ++idx)
            {
                // We do not currently create any high priority graphic queue
                // so we don't need those engines.
                // In order to support global priority, we still need exclusive compute engine to be initialized
                // but this engine can only be selected according to the global priority set by application
                if (idx != static_cast<uint32_t>(Pal::EngineTypeHighPriorityUniversal) &&
                    idx != static_cast<uint32_t>(Pal::EngineTypeHighPriorityGraphics))
                {
                    const auto& engineProps = m_properties.engineProperties[idx];
                    finalizeInfo.requestedEngineCounts[idx].engines = ((1 << engineProps.engineCount) - 1);
                }
            }
        }

        // Ask for CE-RAM (indirect user data table) support for the vertex buffer table.  We need enough CE-RAM to
        // represent the maximum vertex buffer SRD table size.
        const size_t vertBufTableCeRamOffset = finalizeInfo.ceRamSizeUsed[Pal::EngineTypeUniversal];
        const size_t vertBufTableCeRamSize = VertBufBindingMgr::GetMaxVertBufTableDwSize(this);

        finalizeInfo.ceRamSizeUsed[Pal::EngineTypeUniversal] += vertBufTableCeRamSize;

        // Set up the vertex buffer indirect user data table information
        constexpr uint32_t tableId = VertBufBindingMgr::VertexBufferTableId;

        static_assert(tableId < Pal::MaxIndirectUserDataTables, "Invalid vertex buffer indirect user data table ID");

        finalizeInfo.indirectUserDataTable[tableId].offsetInDwords = vertBufTableCeRamOffset;
        finalizeInfo.indirectUserDataTable[tableId].sizeInDwords   = vertBufTableCeRamSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 403
        finalizeInfo.indirectUserDataTable[tableId].ringSize       = m_settings.vbTableInstanceRingSize;
#endif

        if (m_settings.fullScreenFrameMetadataSupport)
        {
            finalizeInfo.flags.requireFlipStatus = true;
            finalizeInfo.flags.requireFrameMetadata = true;
            finalizeInfo.supportedFullScreenFrameMetadata.timerNodeSubmission       = true;
            finalizeInfo.supportedFullScreenFrameMetadata.frameBeginFlag            = true;
            finalizeInfo.supportedFullScreenFrameMetadata.frameEndFlag              = true;
            finalizeInfo.supportedFullScreenFrameMetadata.primaryHandle             = true;
            finalizeInfo.supportedFullScreenFrameMetadata.p2pCmdFlag                = true;
            finalizeInfo.supportedFullScreenFrameMetadata.forceSwCfMode             = true;
            finalizeInfo.supportedFullScreenFrameMetadata.postFrameTimerSubmission  = true;
        }

        finalizeInfo.internalTexOptLevel = VkToPalTexFilterQuality(m_settings.vulkanTexFilterQuality);

            // Finalize the PAL device
            result = m_pPalDevice->Finalize(finalizeInfo);
    }

    Pal::GpuMemoryHeapProperties heapProperties[Pal::GpuHeapCount] = {};

    // Collect memory properties
    if (result == Pal::Result::Success)
    {
        result = m_pPalDevice->GetGpuMemoryHeapProperties(heapProperties);
    }

    if (result == Pal::Result::Success)
    {
        // Pal in some case can give Vulkan a heap with heapSize = 0 or multiple heaps for the same physical memory.
        // Make sure we expose only the valid heap that has a heapSize > 0 and only expose each heap once.
        // Vulkan uses memory types to communicate memory properties, so the number exposed is based on our
        // choosing in order to communicate possible memory requirements as long as they can be associated
        // with an available heap that supports a superset of those requirements.
        m_memoryProperties.memoryTypeCount = 0;
        m_memoryProperties.memoryHeapCount = 0;

        uint32_t heapIndices[Pal::GpuHeapCount] =
        {
            Pal::GpuHeapCount,
            Pal::GpuHeapCount,
            Pal::GpuHeapCount,
            Pal::GpuHeapCount
        };

        // this order indicate a simple ordering logic we expose to API
        constexpr Pal::GpuHeap priority[VK_MEMORY_TYPE_NUM] =
        {
            Pal::GpuHeapInvisible,
            Pal::GpuHeapGartUswc,
            Pal::GpuHeapLocal,
            Pal::GpuHeapGartCacheable
        };

        // Initialize memory heaps
        for (uint32_t orderedHeapIndex = 0; orderedHeapIndex < Pal::GpuHeapCount; ++orderedHeapIndex)
        {
            Pal::GpuHeap                        palGpuHeap = priority[orderedHeapIndex];
            const Pal::GpuMemoryHeapProperties& heapProps  = heapProperties[palGpuHeap];

            // Initialize each heap if it exists other than GartCacheable, which we know will be shared with GartUswc.
            if ((heapProps.heapSize > 0) && (palGpuHeap != Pal::GpuHeapGartCacheable))
            {
                uint32_t      heapIndex  = m_memoryProperties.memoryHeapCount++;
                VkMemoryHeap& memoryHeap = m_memoryProperties.memoryHeaps[heapIndex];

                heapIndices[palGpuHeap] = heapIndex;

                memoryHeap.flags = PalGpuHeapToVkMemoryHeapFlags(palGpuHeap);
                memoryHeap.size  = heapProps.heapSize;

                if (palGpuHeap == Pal::GpuHeapGartUswc)
                {
                    // These two should match because the PAL GPU heaps share the same physical memory.
                    VK_ASSERT(memoryHeap.size == heapProperties[Pal::GpuHeapGartCacheable].heapSize);

                    heapIndices[Pal::GpuHeapGartCacheable] = heapIndex;

                }
                else if ((palGpuHeap == Pal::GpuHeapLocal) &&
                         (heapIndices[Pal::GpuHeapInvisible] == Pal::GpuHeapCount) &&
                         (m_settings.disableDeviceOnlyMemoryTypeWithoutHeap == false))
                {
                    // GPU invisible heap isn't present, but its memory properties are a subset of the GPU local heap.
                    heapIndices[Pal::GpuHeapInvisible] = heapIndex;
                }
            }
        }

        // Initialize memory types
        for (uint32_t orderedHeapIndex = 0; orderedHeapIndex < Pal::GpuHeapCount; ++orderedHeapIndex)
        {
            Pal::GpuHeap palGpuHeap = priority[orderedHeapIndex];

            // We must have a heap capable of allocating this memory type to expose it.
            if (heapIndices[palGpuHeap] < Pal::GpuHeapCount)
            {
                uint32_t memoryTypeIndex = m_memoryProperties.memoryTypeCount++;

                m_memoryVkIndexToPalHeap[memoryTypeIndex] = palGpuHeap;
                m_memoryPalHeapToVkIndex[palGpuHeap]      = memoryTypeIndex;

                VkMemoryType& memoryType = m_memoryProperties.memoryTypes[memoryTypeIndex];

                memoryType.heapIndex = heapIndices[palGpuHeap];

                m_memoryTypeMask |= 1 << memoryTypeIndex;

                const Pal::GpuMemoryHeapProperties& heapProps = heapProperties[palGpuHeap];

                if (heapProps.flags.cpuVisible)
                {
                    memoryType.propertyFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                }

                if (heapProps.flags.cpuGpuCoherent)
                {
                    memoryType.propertyFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                }

                if (heapProps.flags.cpuUncached == 0)
                {
                    memoryType.propertyFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                }

                if ((palGpuHeap == Pal::GpuHeapInvisible) ||
                    (palGpuHeap == Pal::GpuHeapLocal))
                {
                    memoryType.propertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                }
            }
        }

        VK_ASSERT(m_memoryProperties.memoryTypeCount <= Pal::GpuHeapCount);
        VK_ASSERT(m_memoryProperties.memoryHeapCount <= Pal::GpuHeapCount);
    }

    // Collect properties for perf experiments (this call can fail; we just don't report support for
    // perf measurement extension then)
    if (result == Pal::Result::Success)
    {
        PopulateGpaProperties();
    }

    VkResult vkResult = PalToVkResult(result);
    if (vkResult == VK_SUCCESS)
    {
        vkResult = m_compiler.Initialize();
    }

    return vkResult;
}

// =====================================================================================================================
static VkGpaPerfBlockPropertiesAMD ConvertGpaPerfBlock(
    VkGpaPerfBlockAMD                  blockType,
    Pal::GpuBlock                      gpuBlock,
    const Pal::GpuBlockPerfProperties& perfBlock)
{
    VkGpaPerfBlockPropertiesAMD props = {};

    props.blockType               = blockType;
    props.instanceCount           = perfBlock.instanceCount;
    props.maxEventID              = perfBlock.maxEventId;
    props.maxGlobalOnlyCounters   = perfBlock.maxGlobalOnlyCounters;
    props.maxGlobalSharedCounters = perfBlock.maxGlobalSharedCounters;
    props.maxStreamingCounters    = perfBlock.maxSpmCounters;

    return props;
}

// =====================================================================================================================
void PhysicalDevice::PopulateGpaProperties()
{
    if (m_pPalDevice->GetPerfExperimentProperties(&m_gpaProps.palProps) == Pal::Result::Success)
    {
        m_gpaProps.features.clockModes            = VK_TRUE;
        m_gpaProps.features.perfCounters          = m_gpaProps.palProps.features.counters;
        m_gpaProps.features.sqThreadTracing       = m_gpaProps.palProps.features.threadTrace;
        m_gpaProps.features.streamingPerfCounters = m_gpaProps.palProps.features.spmTrace;

        m_gpaProps.properties.flags               = 0;
        m_gpaProps.properties.shaderEngineCount   = m_gpaProps.palProps.shaderEngineCount;
        m_gpaProps.properties.perfBlockCount      = 0;
        m_gpaProps.properties.maxSqttSeBufferSize = m_gpaProps.palProps.features.threadTrace ?
                                                    static_cast<VkDeviceSize>(m_gpaProps.palProps.maxSqttSeBufferSize) :
                                                    0;

        for (uint32_t perfBlock = VK_GPA_PERF_BLOCK_BEGIN_RANGE_AMD;
                      perfBlock <= VK_GPA_PERF_BLOCK_END_RANGE_AMD;
                      ++perfBlock)
        {
            const Pal::GpuBlock gpuBlock = VkToPalGpuBlock(static_cast<VkGpaPerfBlockAMD>(perfBlock));

            if (gpuBlock < Pal::GpuBlock::Count)
            {
                if (m_gpaProps.palProps.blocks[static_cast<uint32_t>(gpuBlock)].available)
                {
                    m_gpaProps.properties.perfBlockCount++;
                }
            }
        }
    }
}

// =====================================================================================================================
void PhysicalDevice::PopulateFormatProperties()
{
    // Collect format properties
    Pal::MergedFormatPropertiesTable fmtProperties = {};
    m_pPalDevice->GetFormatProperties(&fmtProperties);

    const bool multiChannelMinMaxFilter = IsPerChannelMinMaxFilteringSupported();

    for (uint32_t i = 0; i < VK_SUPPORTED_FORMAT_COUNT; i++)
    {
        VkFormat format = Formats::FromIndex(i);

        VkFormatFeatureFlags linearFlags  = 0;
        VkFormatFeatureFlags optimalFlags = 0;
        VkFormatFeatureFlags bufferFlags  = 0;

        GetFormatFeatureFlags(fmtProperties, format, VK_IMAGE_TILING_LINEAR, multiChannelMinMaxFilter, &linearFlags);
        GetFormatFeatureFlags(fmtProperties, format, VK_IMAGE_TILING_OPTIMAL, multiChannelMinMaxFilter, &optimalFlags);

        bufferFlags = linearFlags;

        // Add support for USCALED/SSCALED formats for ISV customer.
        // The BLT tests are incorrect in the conformance test
        // TODO: This should be removed when the CTS errors are fixed
        const Pal::SwizzledFormat palFormat = VkToPalFormat(format);
        const auto numFmt = Formats::GetNumberFormat(format);

         if (numFmt == Pal::Formats::NumericSupportFlags::Uscaled ||
             numFmt == Pal::Formats::NumericSupportFlags::Sscaled)
         {
             const auto disabledScaledFeatures = VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                                 VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                                 VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

             linearFlags  &= ~disabledScaledFeatures;
             optimalFlags &= ~disabledScaledFeatures;

             bufferFlags = linearFlags;
          }

        linearFlags  &= AllImgFeatures;
        optimalFlags &= AllImgFeatures;
        bufferFlags  &= AllBufFeatures;

        m_formatFeaturesTable[i].bufferFeatures        = bufferFlags;
        m_formatFeaturesTable[i].linearTilingFeatures  = linearFlags;
        m_formatFeaturesTable[i].optimalTilingFeatures = optimalFlags;

        // Vulkan doesn't have a corresponding flag for multisampling support.  If there ends up being more cases
        // like this, just store the entire PAL format table in the physical device instead of using a bitfield.
        const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(format);
        const size_t              formatIdx      = static_cast<size_t>(swizzledFormat.format);

        if (fmtProperties.features[formatIdx][Pal::IsNonLinear] & Pal::FormatFeatureMsaaTarget)
        {
            Util::WideBitfieldSetBit(m_formatFeatureMsaaTarget, i);
        }
    }

    // We should always support BC formats
    VK_ASSERT(VerifyBCFormatSupport(*this));
}

// =====================================================================================================================
// Determines which extensions are supported by this physical device.
void PhysicalDevice::PopulateExtensions()
{
    m_supportedExtensions = GetAvailableExtensions(VkInstance(), this);
}

// =====================================================================================================================
// This function is called during instance creation on each physical device after some global operations have been
// initialized that may impact the global instance environment.  This includes things like loading individual settings
// from each GPU's panel that may impact the instance environment, or initialize gpuopen developer mode which may
// cause certain intermediate layers to be installed, etc.
void PhysicalDevice::LateInitialize()
{
    PopulateExtensions();
    PopulateFormatProperties();
    PopulateLimits();
    PopulateQueueFamilies();

#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyProperties(*this);
#endif
}

// =====================================================================================================================
VkResult PhysicalDevice::Destroy(void)
{
    m_compiler.Destroy();
    this->~PhysicalDevice();

    VkInstance()->FreeMem(ApiPhysicalDevice::FromObject(this));

    return VK_SUCCESS;
}

// =====================================================================================================================
// Creates a new vk::Device object.
VkResult PhysicalDevice::CreateDevice(
    const VkDeviceCreateInfo*       pCreateInfo,    // Create info from application
    const VkAllocationCallbacks*    pAllocator,     // Allocation callbacks from application
    VkDevice*                       pDevice)        // New device object
{
    return Device::Create(
        this,
        pCreateInfo,
        pAllocator,
        reinterpret_cast<DispatchableDevice**>(pDevice));
}

// =====================================================================================================================
// Retrieve queue family properties. Called in response to vkGetPhysicalDeviceQueueFamilyProperties
VkResult PhysicalDevice::GetQueueFamilyProperties(
    uint32_t*                        pCount,
    VkQueueFamilyProperties*         pQueueProperties
    ) const
{
    if (pQueueProperties == nullptr)
    {
        *pCount = m_queueFamilyCount;
        return VK_SUCCESS;
    }

    *pCount = Util::Min(m_queueFamilyCount, *pCount);

    for (uint32_t i = 0; i < *pCount; ++i)
    {
        pQueueProperties[i] = m_queueFamilies[i].properties;
    }

    return ((m_queueFamilyCount == *pCount) ? VK_SUCCESS : VK_INCOMPLETE);
}

// =====================================================================================================================
// Retrieve queue family properties. Called in response to vkGetPhysicalDeviceQueueFamilyProperties2KHR
VkResult PhysicalDevice::GetQueueFamilyProperties(
    uint32_t*                        pCount,
    VkQueueFamilyProperties2*       pQueueProperties
    ) const
{
    if (pQueueProperties == nullptr)
    {
        *pCount = m_queueFamilyCount;
        return VK_SUCCESS;
    }

    *pCount = Util::Min(m_queueFamilyCount, *pCount);

    for (uint32_t i = 0; i < *pCount; ++i)
    {
        union
        {
            const VkStructHeader*     pHeader;
            VkQueueFamilyProperties2* pQueueProps;
        };

        for (pQueueProps = &pQueueProperties[i]; pHeader != nullptr; pHeader = pHeader->pNext)
        {
            switch (pHeader->sType)
            {
            case VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2:
            {
                pQueueProps->queueFamilyProperties = m_queueFamilies[i].properties;
            }
            break;

            default:
                // Skip any unknown extension structures
                break;
            }
        }
    }

    return ((m_queueFamilyCount == *pCount) ? VK_SUCCESS : VK_INCOMPLETE);
}

// =====================================================================================================================
// Retrieve device feature support. Called in response to vkGetPhysicalDeviceFeatures
VkResult PhysicalDevice::GetFeatures(
    VkPhysicalDeviceFeatures* pFeatures
    ) const
{
    pFeatures->robustBufferAccess                       = VK_TRUE;
    pFeatures->fullDrawIndexUint32                      = VK_TRUE;
    pFeatures->imageCubeArray                           = VK_TRUE;
    pFeatures->independentBlend                         = VK_TRUE;
    pFeatures->geometryShader                           = VK_TRUE;
    pFeatures->tessellationShader                       = VK_TRUE;
    pFeatures->sampleRateShading                        = VK_TRUE;
    pFeatures->dualSrcBlend                             = VK_TRUE;
    pFeatures->logicOp                                  = VK_TRUE;

    pFeatures->multiDrawIndirect                        = VK_TRUE;
    pFeatures->drawIndirectFirstInstance                = VK_TRUE;

    pFeatures->depthClamp                               = VK_TRUE;
    pFeatures->depthBiasClamp                           = VK_TRUE;
    pFeatures->fillModeNonSolid                         = VK_TRUE;
    pFeatures->depthBounds                              = VK_TRUE;
    pFeatures->wideLines                                = VK_TRUE;
    pFeatures->largePoints                              = VK_TRUE;
    pFeatures->alphaToOne                               = VK_FALSE;
    pFeatures->multiViewport                            = VK_TRUE;
    pFeatures->samplerAnisotropy                        = VK_TRUE;
    pFeatures->textureCompressionETC2                   = VK_FALSE;
    pFeatures->textureCompressionASTC_LDR               = VK_FALSE;
    pFeatures->textureCompressionBC                     = VK_TRUE;
    pFeatures->occlusionQueryPrecise                    = VK_TRUE;
    pFeatures->pipelineStatisticsQuery                  = VK_TRUE;
    pFeatures->vertexPipelineStoresAndAtomics           = VK_TRUE;
    pFeatures->fragmentStoresAndAtomics                 = VK_TRUE;

    pFeatures->shaderTessellationAndGeometryPointSize   = VK_TRUE;
    pFeatures->shaderImageGatherExtended                = VK_TRUE;

    pFeatures->shaderStorageImageExtendedFormats        = VK_TRUE;
    pFeatures->shaderStorageImageMultisample            = VK_TRUE;
    pFeatures->shaderStorageImageReadWithoutFormat      = VK_TRUE;
    pFeatures->shaderStorageImageWriteWithoutFormat     = VK_TRUE;
    pFeatures->shaderUniformBufferArrayDynamicIndexing  = VK_TRUE;
    pFeatures->shaderSampledImageArrayDynamicIndexing   = VK_TRUE;
    pFeatures->shaderStorageBufferArrayDynamicIndexing  = VK_TRUE;
    pFeatures->shaderStorageImageArrayDynamicIndexing   = VK_TRUE;
    pFeatures->shaderClipDistance                       = VK_TRUE;
    pFeatures->shaderCullDistance                       = VK_TRUE;
    pFeatures->shaderFloat64                            = VK_TRUE;
    pFeatures->shaderInt64                              = VK_TRUE;

    if ((PalProperties().gfxipProperties.flags.support16BitInstructions) &&
        ((GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false)      ||
         (PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9)))
    {
        pFeatures->shaderInt16 = VK_TRUE;
    }
    else
    {
        pFeatures->shaderInt16 = VK_FALSE;
    }

    if ((GetRuntimeSettings().optEnablePrt)
        )
    {
        pFeatures->shaderResourceResidency =
            GetPrtFeatures() & Pal::PrtFeatureShaderStatus ? VK_TRUE : VK_FALSE;

        pFeatures->shaderResourceMinLod =
            GetPrtFeatures() & Pal::PrtFeatureShaderLodClamp ? VK_TRUE : VK_FALSE;

        pFeatures->sparseBinding =
            m_properties.gpuMemoryProperties.flags.virtualRemappingSupport ? VK_TRUE : VK_FALSE;

        pFeatures->sparseResidencyBuffer =
            GetPrtFeatures() & Pal::PrtFeatureBuffer ? VK_TRUE : VK_FALSE;

        pFeatures->sparseResidencyImage2D =
            GetPrtFeatures() & Pal::PrtFeatureImage2D ? VK_TRUE : VK_FALSE;

        pFeatures->sparseResidencyImage3D =
            (GetPrtFeatures() & (Pal::PrtFeatureImage3D | Pal::PrtFeatureNonStandardImage3D)) != 0 ? VK_TRUE : VK_FALSE;

        const VkBool32 sparseMultisampled =
            GetPrtFeatures() & Pal::PrtFeatureImageMultisampled ? VK_TRUE : VK_FALSE;

        pFeatures->sparseResidency2Samples  = sparseMultisampled;
        pFeatures->sparseResidency4Samples  = sparseMultisampled;
        pFeatures->sparseResidency8Samples  = sparseMultisampled;
        pFeatures->sparseResidency16Samples = VK_FALSE;

        pFeatures->sparseResidencyAliased =
            GetPrtFeatures() & Pal::PrtFeatureTileAliasing ? VK_TRUE : VK_FALSE;
    }
    else
    {
        pFeatures->shaderResourceResidency  = VK_FALSE;
        pFeatures->shaderResourceMinLod     = VK_FALSE;
        pFeatures->sparseBinding            = VK_FALSE;
        pFeatures->sparseResidencyBuffer    = VK_FALSE;
        pFeatures->sparseResidencyImage2D   = VK_FALSE;
        pFeatures->sparseResidencyImage3D   = VK_FALSE;
        pFeatures->sparseResidency2Samples  = VK_FALSE;
        pFeatures->sparseResidency4Samples  = VK_FALSE;
        pFeatures->sparseResidency8Samples  = VK_FALSE;
        pFeatures->sparseResidency16Samples = VK_FALSE;
        pFeatures->sparseResidencyAliased   = VK_FALSE;
    }

    pFeatures->variableMultisampleRate = VK_TRUE;
    pFeatures->inheritedQueries        = VK_TRUE;

    return VK_SUCCESS;
}

// =====================================================================================================================
// Retrieve format properites. Called in response to vkGetPhysicalDeviceImageFormatProperties
VkResult PhysicalDevice::GetImageFormatProperties(
    VkFormat                 format,
    VkImageType              type,
    VkImageTiling            tiling,
    VkImageUsageFlags        usage,
    VkImageCreateFlags       flags,
    VkImageFormatProperties* pImageFormatProperties
    ) const
{
    memset(pImageFormatProperties, 0, sizeof(VkImageFormatProperties));

    const auto& imageProps = PalProperties().imageProperties;

    Pal::SwizzledFormat palFormat = VkToPalFormat(format);

    // Block-compressed formats are not supported for 1D textures (PAL image creation will fail).
    if (Pal::Formats::IsBlockCompressed(palFormat.format) && (type == VK_IMAGE_TYPE_1D))
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // not implemented due to issue binding single images to multiple peer memory allocations (page table support)
    if ((flags & VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT) != 0)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // Currently we just disable the support of linear 3d surfaces, since they aren't required by spec.
    if (type == VK_IMAGE_TYPE_3D && tiling == VK_IMAGE_TILING_LINEAR)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    if (flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
    {
        if ((GetRuntimeSettings().optEnablePrt == false)
        )
        {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const bool sparseBinding = m_properties.gpuMemoryProperties.flags.virtualRemappingSupport;
        if (!sparseBinding)
        {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        if (flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
        {
            if (Formats::IsDepthStencilFormat(format))
            {
                const bool sparseDepthStencil = (GetPrtFeatures() & Pal::PrtFeatureImageDepthStencil) != 0;
                if (!sparseDepthStencil)
                {
                    return VK_ERROR_FORMAT_NOT_SUPPORTED;
                }
            }

            switch (type)
            {
                case VK_IMAGE_TYPE_1D:
                    return VK_ERROR_FORMAT_NOT_SUPPORTED;
                case VK_IMAGE_TYPE_2D:
                {
                    const bool sparseResidencyImage2D = (GetPrtFeatures() & Pal::PrtFeatureImage2D) != 0;
                    if (!sparseResidencyImage2D)
                    {
                        return VK_ERROR_FORMAT_NOT_SUPPORTED;
                    }
                    break;
                }
                case VK_IMAGE_TYPE_3D:
                {
                    const bool sparseResidencyImage3D = (GetPrtFeatures() & (Pal::PrtFeatureImage3D |
                                                                             Pal::PrtFeatureNonStandardImage3D)) != 0;
                    if (!sparseResidencyImage3D)
                    {
                        return VK_ERROR_FORMAT_NOT_SUPPORTED;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (flags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)
        {
            const bool sparseResidencyAliased = (GetPrtFeatures() & Pal::PrtFeatureTileAliasing) != 0;
            if (!sparseResidencyAliased)
            {
                return VK_ERROR_FORMAT_NOT_SUPPORTED;
            }
        }
    }

    VkFormatProperties formatProperties;

    GetFormatProperties(format, &formatProperties);

    if (formatProperties.linearTilingFeatures  == 0 &&
        formatProperties.optimalTilingFeatures == 0)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkFormatFeatureFlags supportedFeatures = tiling == VK_IMAGE_TILING_OPTIMAL
                                           ? formatProperties.optimalTilingFeatures
                                           : formatProperties.linearTilingFeatures;

    // 3D textures with depth or stencil format are not supported
    if ((type == VK_IMAGE_TYPE_3D) && (Formats::HasDepth(format) || Formats::HasStencil(format)))
    {
         supportedFeatures = 0;
    }

    // Depth stencil attachment usage is not supported for 3D textures (this is distinct from the preceding depth
    // format check because some tests attempt to create an R8_UINT surface and use it as a stencil attachment).
    if (type == VK_IMAGE_TYPE_3D)
    {
         supportedFeatures &= ~VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if ((supportedFeatures == 0)                                                        ||
        (((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)                               &&
          (supportedFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)                ||
        (((usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)                               &&
          (supportedFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0)                ||
        (((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)                                    &&
         ((supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0))              ||
        (((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0)                                    &&
         ((supportedFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0))              ||
        (((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)                           &&
         ((supportedFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0))           ||
        (((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)                   &&
         ((supportedFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0))   ||
        (((usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) != 0)                           &&
         ((supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)))
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // Calculate maxResourceSize
    //
    // NOTE: The spec requires the reported value to be at least 2**31, even though it does not make
    //       much sense for some cases ..
    //
    // NOTE: BytesPerPixel obtained from PAL is per block not per pixel for compressed formats.  Therefore,
    //       maxResourceSize/maxExtent are also in terms of blocks for compressed formats.  I.e. we don't
    //       increase our exposed limits for compressed formats even though PAL/HW operating in terms of
    //       blocks makes that possible.
    uint64_t bytesPerPixel = Pal::Formats::BytesPerPixel(palFormat.format);
    uint32_t currMipSize[3] =
    {
        imageProps.maxDimensions.width,
        (type == VK_IMAGE_TYPE_1D) ? 1 : imageProps.maxDimensions.height,
        (type != VK_IMAGE_TYPE_3D) ? 1 : imageProps.maxDimensions.depth
    };
    uint32_t     maxMipLevels    = Util::Max(Util::Log2(imageProps.maxDimensions.width),
                                             Util::Max(Util::Log2(imageProps.maxDimensions.height),
                                                       Util::Log2(imageProps.maxDimensions.depth))) + 1;
    VkDeviceSize maxResourceSize = 0;
    uint32_t     nLayers         = (type != VK_IMAGE_TYPE_3D) ? imageProps.maxArraySlices : 1;

    if (type != VK_IMAGE_TYPE_1D &&
        type != VK_IMAGE_TYPE_2D &&
        type != VK_IMAGE_TYPE_3D)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    for (uint32_t currMip = 0;
                  currMip < maxMipLevels;
                ++currMip)
    {
        currMipSize[0] = Util::Max(currMipSize[0], 1u);
        currMipSize[1] = Util::Max(currMipSize[1], 1u);
        currMipSize[2] = Util::Max(currMipSize[2], 1u);

        maxResourceSize += currMipSize[0] * currMipSize[1] * currMipSize[2] * bytesPerPixel * nLayers;

        currMipSize[0] /= 2u;
        currMipSize[1] /= 2u;
        currMipSize[2] /= 2u;
    }

    pImageFormatProperties->maxResourceSize = Util::Max(maxResourceSize, VkDeviceSize(1LL << 31) );

    // Check that the HW supports multisampling for this format.
    // Additionally, the Spec requires us to report VK_SAMPLE_COUNT_1_BIT for the following cases:
    //    1- Non-2D images.
    //    2- Linear image formats.
    //    3- Images created with the VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.
    //    4- Image formats that do not support any of the following uses:
    //         a- color attachment.
    //         b- depth/stencil attachment.
    //         c- storage image.
    if ((FormatSupportsMsaa(format) == false)                                       ||
        (type != VK_IMAGE_TYPE_2D)                                                  ||
        (tiling == VK_IMAGE_TILING_LINEAR)                                          ||
        ((flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)                        ||
        ((supportedFeatures & (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT    |
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0))
    {
        pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    }
    else
    {
        pImageFormatProperties->sampleCounts = MaxSampleCountToSampleCountFlags(Pal::MaxMsaaFragments);
    }

    pImageFormatProperties->maxExtent.width  = imageProps.maxDimensions.width;
    pImageFormatProperties->maxExtent.height = imageProps.maxDimensions.height;
    pImageFormatProperties->maxExtent.depth  = imageProps.maxDimensions.depth;
    pImageFormatProperties->maxMipLevels     = maxMipLevels;
    pImageFormatProperties->maxArrayLayers   = (type != VK_IMAGE_TYPE_3D) ? imageProps.maxArraySlices : 1;

    // Clamp reported extent to adhere to the requested image type.
    switch (type)
    {
    case VK_IMAGE_TYPE_1D:
        pImageFormatProperties->maxExtent.depth  = 1;
        pImageFormatProperties->maxExtent.height = 1;
        break;

    case VK_IMAGE_TYPE_2D:
        pImageFormatProperties->maxExtent.depth = 1;
        break;

    case VK_IMAGE_TYPE_3D:
        break;

    default:
        VK_ASSERT(type == VK_IMAGE_TYPE_1D ||
                  type == VK_IMAGE_TYPE_2D ||
                  type == VK_IMAGE_TYPE_3D);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Retrieve format properites. Called in response to vkGetPhysicalDeviceImageFormatProperties
void PhysicalDevice::GetSparseImageFormatProperties(
    VkFormat                                        format,
    VkImageType                                     type,
    VkSampleCountFlagBits                           samples,
    VkImageUsageFlags                               usage,
    VkImageTiling                                   tiling,
    uint32_t*                                       pPropertyCount,
    utils::ArrayView<VkSparseImageFormatProperties> properties) const
{
    const struct AspectLookup
    {
        Pal::ImageAspect      aspectPal;
        VkImageAspectFlagBits aspectVk;
        bool                  available;
    } aspects[] =
    {
        {Pal::ImageAspect::Color,   VK_IMAGE_ASPECT_COLOR_BIT,   vk::Formats::IsColorFormat(format)},
        {Pal::ImageAspect::Depth,   VK_IMAGE_ASPECT_DEPTH_BIT,   vk::Formats::HasDepth     (format)},
        {Pal::ImageAspect::Stencil, VK_IMAGE_ASPECT_STENCIL_BIT, vk::Formats::HasStencil   (format)}
    };
    const uint32_t nAspects = sizeof(aspects) / sizeof(aspects[0]);

    uint32_t bytesPerPixel = Util::Pow2Pad(Pal::Formats::BytesPerPixel(VkToPalFormat(format).format));

    bool supported = true
        // Currently we only support optimally tiled sparse images
        && (tiling == VK_IMAGE_TILING_OPTIMAL)
        // Currently we don't support 1D sparse images
        && (type != VK_IMAGE_TYPE_1D)
        // 2D sparse images depend on HW capability
        && ((type != VK_IMAGE_TYPE_2D) || (GetPrtFeatures() & Pal::PrtFeatureImage2D))
        // 3D sparse images depend on HW capability
        && ((type != VK_IMAGE_TYPE_3D) ||
            ((GetPrtFeatures() & (Pal::PrtFeatureImage3D | Pal::PrtFeatureNonStandardImage3D)) != 0))
        // Multisampled sparse images depend on HW capability
        && ((samples == VK_SAMPLE_COUNT_1_BIT) ||
            ((type == VK_IMAGE_TYPE_2D) && (GetPrtFeatures() & Pal::PrtFeatureImageMultisampled)))
        // We only support pixel sizes not larger than 128 bits
        && (bytesPerPixel <= 16)
        // Up to 16 MSAA coverage samples are supported by HW
        && (samples <= Pal::MaxMsaaRasterizerSamples);

    // Check if hardware supports sparse depth/stencil aspects.
    if (aspects[1].available || aspects[2].available)
    {
        supported &= ((PalProperties().imageProperties.prtFeatures & Pal::PrtFeatureImageDepthStencil) != 0);

        // PAL doesn't expose all the information required to support a planar depth/stencil format.
        if (aspects[1].available && aspects[2].available)
        {
            supported = false;
        }
    }

    if (supported)
    {
        const uint32_t requiredPropertyCount = (aspects[0].available ? 1 : 0)
                                             + (aspects[1].available ? 1 : 0)
                                             + (aspects[2].available ? 1 : 0); // Stencil is in a separate plane

        VK_ASSERT(pPropertyCount != nullptr);

        if (properties.IsNull())
        {
            *pPropertyCount = requiredPropertyCount;
        }
        else
        {
            uint32_t writtenPropertyCount = 0;

            for (const AspectLookup* pAspect = aspects; pAspect != aspects + nAspects; ++pAspect)
            {
                if (!pAspect->available)
                {
                    continue;
                }

                if (writtenPropertyCount == *pPropertyCount)
                {
                    break;
                }

                VkSparseImageFormatProperties* const pProperties = &properties[writtenPropertyCount];

                pProperties->aspectMask = pAspect->aspectVk;

                const VkFormat aspectFormat = Formats::GetAspectFormat(format, pAspect->aspectVk);
                bytesPerPixel = Util::Pow2Pad(Pal::Formats::BytesPerPixel(VkToPalFormat(aspectFormat).format));

                // Determine pixel size index (log2 of the pixel byte size, used to index into the tables below)
                // Note that we only support standard block shapes currently
                const uint32_t pixelSizeIndex = Util::Log2(bytesPerPixel);

                if ((type == VK_IMAGE_TYPE_2D) && (samples == VK_SAMPLE_COUNT_1_BIT))
                {
                    // Report standard 2D sparse block shapes
                    static const VkExtent3D Std2DBlockShapes[] =
                    {
                        {   256,    256,    1   },  // 8-bit
                        {   256,    128,    1   },  // 16-bit
                        {   128,    128,    1   },  // 32-bit
                        {   128,    64,     1   },  // 64-bit
                        {   64,     64,     1   },  // 128-bit
                    };

                    VK_ASSERT(pixelSizeIndex < VK_ARRAY_SIZE(Std2DBlockShapes));

                    pProperties->imageGranularity = Formats::ElementsToTexels(aspectFormat,
                                                                              Std2DBlockShapes[pixelSizeIndex]);
                }
                else if (type == VK_IMAGE_TYPE_3D)
                {
                    if (GetPrtFeatures() & Pal::PrtFeatureImage3D)
                    {
                        // Report standard 3D sparse block shapes
                        static const VkExtent3D Std3DBlockShapes[] =
                        {
                            {   64,     32,     32  },  // 8-bit
                            {   32,     32,     32  },  // 16-bit
                            {   32,     32,     16  },  // 32-bit
                            {   32,     16,     16  },  // 64-bit
                            {   16,     16,     16  },  // 128-bit
                        };

                        VK_ASSERT(pixelSizeIndex < VK_ARRAY_SIZE(Std3DBlockShapes));

                        pProperties->imageGranularity = Formats::ElementsToTexels(aspectFormat,
                                                                                  Std3DBlockShapes[pixelSizeIndex]);
                    }
                    else
                    {
                        VK_ASSERT((GetPrtFeatures() & Pal::PrtFeatureNonStandardImage3D) != 0);

                        // When standard shapes aren't supported, report shapes with a depth equal to the tile
                        // thickness, 4, except for 64-bit and larger, which may cause a tile split on some ASICs.
                        // PAL chooses PRT thick mode for 3D images, and addrlib uses these unmodified for CI/VI.
                        static const VkExtent3D NonStd3DBlockShapes[] =
                        {
                            {   128,    128,    4  },  // 8-bit
                            {   128,    64,     4  },  // 16-bit
                            {   64,     64,     4  },  // 32-bit
                            {   128,    64,     1  },  // 64-bit
                            {   64,     64,     1  },  // 128-bit
                        };

                        VK_ASSERT(pixelSizeIndex < VK_ARRAY_SIZE(NonStd3DBlockShapes));

                        pProperties->imageGranularity = Formats::ElementsToTexels(aspectFormat,
                                                                                  NonStd3DBlockShapes[pixelSizeIndex]);
                    }
                }
                else if ((type == VK_IMAGE_TYPE_2D) && (samples != VK_SAMPLE_COUNT_1_BIT))
                {
                    // Report standard MSAA sparse block shapes
                    static const VkExtent3D StdMSAABlockShapes[][5] =
                    {
                        { // 2x MSAA
                            {   128,    256,    1   },  // 8-bit
                            {   128,    128,    1   },  // 16-bit
                            {   64,     128,    1   },  // 32-bit
                            {   64,     64,     1   },  // 64-bit
                            {   32,     64,     1   },  // 128-bit
                        },
                        { // 4x MSAA
                            {   128,    128,    1   },  // 8-bit
                            {   128,    64,     1   },  // 16-bit
                            {   64,     64,     1   },  // 32-bit
                            {   64,     32,     1   },  // 64-bit
                            {   32,     32,     1   },  // 128-bit
                        },
                        { // 8x MSAA
                            {   64,     128,    1   },  // 8-bit
                            {   64,     64,     1   },  // 16-bit
                            {   32,     64,     1   },  // 32-bit
                            {   32,     32,     1   },  // 64-bit
                            {   16,     32,     1   },  // 128-bit
                        },
                        { // 16x MSAA
                            {   64,     64,     1   },  // 8-bit
                            {   64,     32,     1   },  // 16-bit
                            {   32,     32,     1   },  // 32-bit
                            {   32,     16,     1   },  // 64-bit
                            {   16,     16,     1   },  // 128-bit
                        },
                    };

                    const uint32_t sampleCountIndex = Util::Log2(static_cast<uint32_t>(samples)) - 1;

                    VK_ASSERT(sampleCountIndex < VK_ARRAY_SIZE(StdMSAABlockShapes));
                    VK_ASSERT(pixelSizeIndex < VK_ARRAY_SIZE(StdMSAABlockShapes[0]));

                    pProperties->imageGranularity = StdMSAABlockShapes[sampleCountIndex][pixelSizeIndex];
                }
                else
                {
                    VK_ASSERT(!"Unexpected parameter combination");
                }

                pProperties->flags = 0;

                // If per-layer miptail isn't supported then set SINGLE_MIPTAIL_BIT
                if ((GetPrtFeatures() & Pal::PrtFeaturePerSliceMipTail) == 0)
                {
                    pProperties->flags |= VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
                }

                // If unaligned mip size isn't supported then set ALIGNED_MIP_SIZE_BIT
                if ((GetPrtFeatures() & Pal::PrtFeatureUnalignedMipSize) == 0)
                {
                    pProperties->flags |= VK_SPARSE_IMAGE_FORMAT_ALIGNED_MIP_SIZE_BIT;
                }

                ++writtenPropertyCount;

            } // for pAspect

            *pPropertyCount = writtenPropertyCount;
        }
    }
    else
    {
        // Combination not supported
        *pPropertyCount = 0;
    }
}

// =====================================================================================================================
// Returns the API version supported by this device.
uint32_t PhysicalDevice::GetSupportedAPIVersion() const
{
    // Currently all of our HW supports Vulkan 1.1
    uint32_t apiVersion = (VK_API_VERSION_1_1 | VK_HEADER_VERSION);

    // For sanity check we do at least want to make sure that all the necessary extensions are supported and exposed.
    // The spec does not require Vulkan 1.1 implementations to expose the corresponding 1.0 extensions, but we'll
    // continue doing so anyways to maximize application compatibility (which is why the spec allows this).
    VK_ASSERT( IsExtensionSupported(DeviceExtensions::KHR_16BIT_STORAGE)
            && IsExtensionSupported(DeviceExtensions::KHR_BIND_MEMORY2)
            && IsExtensionSupported(DeviceExtensions::KHR_DEDICATED_ALLOCATION)
            && IsExtensionSupported(DeviceExtensions::KHR_DESCRIPTOR_UPDATE_TEMPLATE)
            && IsExtensionSupported(DeviceExtensions::KHR_DEVICE_GROUP)
            && IsExtensionSupported(InstanceExtensions::KHR_DEVICE_GROUP_CREATION)
            && IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_MEMORY)
            && IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_MEMORY_CAPABILITIES)
            && IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_SEMAPHORE)
            && IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_SEMAPHORE_CAPABILITIES)
            && IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_FENCE)
            && IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_FENCE_CAPABILITIES)
            && IsExtensionSupported(DeviceExtensions::KHR_GET_MEMORY_REQUIREMENTS2)
            && IsExtensionSupported(InstanceExtensions::KHR_GET_PHYSICAL_DEVICE_PROPERTIES2)
            && IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE1)
            && IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE2)
            && IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE3)
            && IsExtensionSupported(DeviceExtensions::KHR_MULTIVIEW)
            && IsExtensionSupported(DeviceExtensions::KHR_RELAXED_BLOCK_LAYOUT)
//            && IsExtensionSupported(DeviceExtensions::KHR_SAMPLER_YCBCR_CONVERSION)
            && IsExtensionSupported(DeviceExtensions::KHR_SHADER_DRAW_PARAMETERS)
            && IsExtensionSupported(DeviceExtensions::KHR_STORAGE_BUFFER_STORAGE_CLASS)
            && IsExtensionSupported(DeviceExtensions::KHR_VARIABLE_POINTERS)
        );

    return apiVersion;
}

// =====================================================================================================================
// Retrieve device properties. Called in response to vkGetPhysicalDeviceProperties.
VkResult PhysicalDevice::GetDeviceProperties(
    VkPhysicalDeviceProperties* pProperties) const
{
    VK_ASSERT(pProperties != nullptr);

    memset(pProperties, 0, sizeof(*pProperties));

    // Get properties from PAL
    const Pal::DeviceProperties& palProps = PalProperties();

#if VKI_SDK_1_0 == 0
    pProperties->apiVersion    = GetSupportedAPIVersion();
#else
    pProperties->apiVersion    = (VK_API_VERSION_1_0 | VK_HEADER_VERSION);
#endif

    // Radeon Settings UI diplays driverVersion using sizes 10.10.12 like apiVersion, but our driverVersion uses 10.22.
    // If this assert ever triggers, verify that it and other driver info tools that parse the raw value have been
    // updated to avoid any confusion.
    VK_ASSERT(VULKAN_ICD_BUILD_VERSION < (1 << 12));
    pProperties->driverVersion = (VULKAN_ICD_MAJOR_VERSION << 22) | (VULKAN_ICD_BUILD_VERSION & ((1 << 22) - 1));

    // Convert PAL properties to Vulkan
    pProperties->vendorID   = palProps.vendorId;
    pProperties->deviceID   = palProps.deviceId;
    pProperties->deviceType = PalToVkGpuType(palProps.gpuType);

    if (VkInstance()->IsNullGpuModeEnabled())
    {
        pProperties->deviceType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    }

    memcpy(pProperties->deviceName, palProps.gpuName,
        std::min(Pal::MaxDeviceName, Pal::uint32(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE)));
    pProperties->deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = 0;

    pProperties->limits = GetLimits();

    pProperties->sparseProperties.residencyStandard2DBlockShape =
        GetPrtFeatures() & Pal::PrtFeatureImage2D ? VK_TRUE : VK_FALSE;

    pProperties->sparseProperties.residencyStandard2DMultisampleBlockShape =
        GetPrtFeatures() & Pal::PrtFeatureImageMultisampled ? VK_TRUE : VK_FALSE;

    // NOTE: GFX7 and GFX8 may expose sparseResidencyImage3D but are unable to support residencyStandard3DBlockShape
    pProperties->sparseProperties.residencyStandard3DBlockShape =
        GetPrtFeatures() & Pal::PrtFeatureImage3D ? VK_TRUE : VK_FALSE;

    pProperties->sparseProperties.residencyAlignedMipSize =
        GetPrtFeatures() & Pal::PrtFeatureUnalignedMipSize ? VK_FALSE : VK_TRUE;

    pProperties->sparseProperties.residencyNonResidentStrict =
        GetPrtFeatures() & Pal::PrtFeatureStrictNull ? VK_TRUE : VK_FALSE;

    // This UUID identifies whether a previously created pipeline cache is compatible with the currently installed
    // device/driver.  The UUID below is probably not accurate enough and needs to be re-evaluated once we actually
    // implement serialization support.
    constexpr uint16_t PalMajorVersion  = PAL_INTERFACE_MAJOR_VERSION;
    constexpr uint8_t  PalMinorVersion  = PAL_INTERFACE_MINOR_VERSION;
    constexpr uint8_t  UuidVersion      = 1;

    constexpr char timestamp[]    = __DATE__ __TIME__;
    const uint32_t timestampHash  = Util::HashLiteralString<sizeof(timestamp)>(timestamp);

    const uint8_t* pVendorId      = reinterpret_cast<const uint8_t*>(&palProps.vendorId);
    const uint8_t* pDeviceId      = reinterpret_cast<const uint8_t*>(&palProps.deviceId);
    const uint8_t* pPalMajor      = reinterpret_cast<const uint8_t*>(&PalMajorVersion);
    const uint8_t* pTimestampHash = reinterpret_cast<const uint8_t*>(&timestampHash);

    pProperties->pipelineCacheUUID[0] = pVendorId[0];
    pProperties->pipelineCacheUUID[1] = pVendorId[1];
    pProperties->pipelineCacheUUID[2] = pVendorId[2];
    pProperties->pipelineCacheUUID[3] = pVendorId[3];

    pProperties->pipelineCacheUUID[4] = pDeviceId[0];
    pProperties->pipelineCacheUUID[5] = pDeviceId[1];
    pProperties->pipelineCacheUUID[6] = pDeviceId[2];
    pProperties->pipelineCacheUUID[7] = pDeviceId[3];

    pProperties->pipelineCacheUUID[8]  = pPalMajor[0];
    pProperties->pipelineCacheUUID[9]  = pPalMajor[1];
    pProperties->pipelineCacheUUID[10] = PalMinorVersion;
    pProperties->pipelineCacheUUID[11] = UuidVersion;

    pProperties->pipelineCacheUUID[12] = pTimestampHash[0];
    pProperties->pipelineCacheUUID[13] = pTimestampHash[1];
    pProperties->pipelineCacheUUID[14] = pTimestampHash[2];
    pProperties->pipelineCacheUUID[15] = pTimestampHash[3];

    return VK_SUCCESS;
}

// =====================================================================================================================
// Returns true if the given queue family (engine type) supports presents.
bool PhysicalDevice::QueueSupportsPresents(
    uint32_t         queueFamilyIndex,
    VkIcdWsiPlatform platform
    ) const
{
    // Do we have any of this engine type and, if so, does it support a queueType that supports presents?
    const Pal::EngineType palEngineType = m_queueFamilies[queueFamilyIndex].palEngineType;
    const auto& engineProps             = m_properties.engineProperties[palEngineType];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    Pal::PresentMode presentMode =
        (platform == VK_ICD_WSI_PLATFORM_DISPLAY)? Pal::PresentMode::Fullscreen : Pal::PresentMode::Windowed;

    bool supported = (engineProps.engineCount > 0) &&
                     (m_pPalDevice->GetSupportedSwapChainModes(VkToPalWsiPlatform(platform), presentMode) != 0);
#else
    const uint32_t presentMode = static_cast<uint32_t>(Pal::PresentMode::Windowed);

    bool supported = (engineProps.engineCount > 0) &&
                     (m_properties.swapChainProperties.supportedSwapChainModes[presentMode]);
#endif

    return supported;
}

// =====================================================================================================================
// Aggregates the maximum supported samples for a particular image format with user-specified tiling mode, across all
// possible image types that support a particular format feature flag.
static uint32_t GetMaxFormatSampleCount(
    const PhysicalDevice* pPhysDevice,
    VkFormat              format,
    VkFormatFeatureFlags  reqFeatures,
    VkImageTiling         tiling,
    VkImageUsageFlags     imgUsage)
{
    static_assert(VK_IMAGE_TYPE_RANGE_SIZE == 3, "Need to add new image types here");

    constexpr VkImageType ImageTypes[] =
    {
        VK_IMAGE_TYPE_1D,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TYPE_3D
    };

    VkFormatProperties props;

    pPhysDevice->GetFormatProperties(format, &props);

    uint32_t maxSamples = 0;

    for (uint32_t typeIdx = VK_IMAGE_TYPE_BEGIN_RANGE; typeIdx <= VK_IMAGE_TYPE_END_RANGE; ++typeIdx)
    {
        // NOTE: Spec requires us to return x1 sample count for linearly-tiled image format. Only focus on
        // optimally-tiled formats then.
        {
            VkImageType imgType = static_cast<VkImageType>(typeIdx);

            const VkFormatFeatureFlags features = (tiling == VK_IMAGE_TILING_LINEAR) ?
                props.linearTilingFeatures : props.optimalTilingFeatures;

            if ((features & reqFeatures) == reqFeatures)
            {
                VkImageFormatProperties formatProps = {};

                VkResult result = pPhysDevice->GetImageFormatProperties(
                    format,
                    imgType,
                    tiling,
                    imgUsage,
                    0,
                    &formatProps);

                if (result == VK_SUCCESS)
                {
                     uint32_t sampleCount = 0;

                    for (uint32_t bit = 0; formatProps.sampleCounts != 0; ++bit)
                    {
                        if (((1UL << bit) & formatProps.sampleCounts) != 0)
                        {
                            sampleCount = (1UL << bit);
                            formatProps.sampleCounts &= ~(1UL << bit);
                        }
                    }
                    maxSamples = Util::Max(maxSamples, sampleCount);
                }
            }
        }
    }

    return maxSamples;
}

// =====================================================================================================================
// Populates the physical device limits for this physical device.
void PhysicalDevice::PopulateLimits()
{
    // NOTE: The comments describing these limits were pulled from the Vulkan specification at a time when it was
    // still in flux.  Changes may have been made to the spec that changed some of the language (e.g. the units)
    // of a limit's description that may not have been reflected in the comments.  You should double check with the
    // spec for the true language always if suspecting a particular limit is incorrect.

    constexpr uint32_t MaxFramebufferLayers = 2048;

    const Pal::DeviceProperties& palProps = PalProperties();
    const auto& imageProps = palProps.imageProperties;

    // Maximum dimension (width) of an image created with an imageType of VK_IMAGE_TYPE_1D.
    m_limits.maxImageDimension1D = imageProps.maxDimensions.width;

    // Maximum dimension (width or height) of an image created with an imageType of VK_IMAGE_TYPE_2D and without
    // VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT set in flags.
    m_limits.maxImageDimension2D = Util::Min(imageProps.maxDimensions.width, imageProps.maxDimensions.height);

    // Maximum dimension(width, height, or depth) of an image created with an imageType of VK_IMAGE_TYPE_3D.
    // Depth is further limited by max framebuffer layers when a 3D image slice is used as a render target.
    m_limits.maxImageDimension3D = Util::Min(Util::Min(m_limits.maxImageDimension2D, imageProps.maxDimensions.depth),
                                             MaxFramebufferLayers);

    // Maximum dimension (width or height) of an image created with an imageType of VK_IMAGE_TYPE_2D and with
    // VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT set in flags.
    m_limits.maxImageDimensionCube = m_limits.maxImageDimension2D;

    // Maximum number of layers (arrayLayers) for an image.
    m_limits.maxImageArrayLayers = imageProps.maxArraySlices;

    // Maximum number of addressable texels for a buffer view created on a buffer which was created with the
    // VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT or VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT set in the usage member of
    // the VkBufferCreateInfo structure.
    m_limits.maxTexelBufferElements = UINT_MAX;

    // Maximum range, in bytes, that can be specified in the bufferInfo struct of VkDescriptorInfo when used for
    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    m_limits.maxUniformBufferRange = UINT_MAX;

    // Maximum range, in bytes, that can be specified in the bufferInfo struct of VkDescriptorInfo when used for
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC.
    m_limits.maxStorageBufferRange = UINT_MAX;

    // Maximum size, in bytes, of the push constants pool that can be referenced by the vkCmdPushConstants commands.
    // For each of the push constant ranges indicated by the pPushConstantRanges member of the
    // VkPipelineLayoutCreateInfo structure, the value of start + length must be less than or equal to this limit.
    m_limits.maxPushConstantsSize = MaxPushConstants;

    // Maximum number of device memory allocations, as created by vkAllocMemory, that can exist simultaneously.
    // relax the limitation on Linux since there is no real limitation from OS's perspective.
    m_limits.maxMemoryAllocationCount = UINT_MAX;
    if (m_settings.memoryCustomDeviceAllocationCountLimit > 0)
    {
        m_limits.maxMemoryAllocationCount = m_settings.memoryCustomDeviceAllocationCountLimit;
    }

    // Maximum number of sampler objects
    // 1G - This limit was chosen heuristally. The vulkan CTS tests the limit we provide, which is a threoretical
    // limit and is dependent of the _system_ memory.
    m_limits.maxSamplerAllocationCount = 1048576;

    // Granularity, in bytes, at which buffers and images can be bound to adjacent memory for simultaneous usage.
    m_limits.bufferImageGranularity = 1;

    // Virtual memory address space size for sparse resources
    m_limits.sparseAddressSpaceSize = palProps.gpuMemoryProperties.vaEnd - palProps.gpuMemoryProperties.vaStart;

    // Maximum number of descriptor sets that can be simultaneously used by a pipeline. Set numbers used by all
    // shaders must be less than the value of maxBoundDescriptorSets.
    m_limits.maxBoundDescriptorSets = MaxDescriptorSets;

    // Maximum number of samplers, uniform buffers, storage buffers, sampled images and storage images that can be
    // referenced in a pipeline layout for any single shader stage.
    m_limits.maxPerStageDescriptorSamplers          = UINT32_MAX;
    m_limits.maxPerStageDescriptorUniformBuffers    = UINT32_MAX;
    m_limits.maxPerStageDescriptorStorageBuffers    = UINT32_MAX;
    m_limits.maxPerStageDescriptorSampledImages     = UINT32_MAX;
    m_limits.maxPerStageDescriptorStorageImages     = UINT32_MAX;
    m_limits.maxPerStageDescriptorInputAttachments  = UINT32_MAX;
    m_limits.maxPerStageResources                   = UINT32_MAX;

    // Same as above, but total limit across all pipeline stages in a single descriptor set.
    m_limits.maxDescriptorSetSamplers               = UINT32_MAX;
    m_limits.maxDescriptorSetUniformBuffers         = UINT32_MAX;
    m_limits.maxDescriptorSetUniformBuffersDynamic  = MaxDynamicUniformDescriptors;
    m_limits.maxDescriptorSetStorageBuffers         = UINT32_MAX;
    m_limits.maxDescriptorSetStorageBuffersDynamic  = MaxDynamicStorageDescriptors;
    m_limits.maxDescriptorSetSampledImages          = UINT32_MAX;
    m_limits.maxDescriptorSetStorageImages          = UINT32_MAX;
    m_limits.maxDescriptorSetInputAttachments       = UINT32_MAX;

    // Maximum number of vertex input attributes that can be specified for a graphics pipeline. These are described in
    // the VkVertexInputAttributeDescription structure that is provided at graphics pipeline creation time via the
    // pVertexAttributeDescriptions member of the VkPipelineVertexInputStateCreateInfo structure.
    m_limits.maxVertexInputAttributes = 64;

    // Maximum number of vertex buffers that can be specified for providing vertex attributes to a graphics pipeline.
    // These are described in the VkVertexInputBindingDescription structure that is provided at graphics pipeline
    // creation time via the pVertexBindingDescriptions member of the VkPipelineVertexInputStateCreateInfo structure.
    m_limits.maxVertexInputBindings = MaxVertexBuffers;

    // Maximum vertex input attribute offset that can be added to the vertex input binding stride. The offsetInBytes
    // member of the VkVertexInputAttributeDescription structure must be less than or equal to the value of this limit.
    m_limits.maxVertexInputAttributeOffset = UINT32_MAX;

    // Maximum vertex input binding stride that can be specified in a vertex input binding. The strideInBytes member
    // of the VkVertexInputBindingDescription structure must be less than or equal to the value of this limit.
    m_limits.maxVertexInputBindingStride = palProps.gfxipProperties.maxBufferViewStride;

    // Maximum number of components of output variables which may be output by a vertex shader.
    m_limits.maxVertexOutputComponents = 128;

    // OGL: SI_MAX_VP_VARYING_COMPONENTS

    // Maximum tessellation generation level supported by the fixed function tessellation primitive generator.
    m_limits.maxTessellationGenerationLevel = 64;

    // OGL: SI_MAX_TESS_FACTOR

    // Maximum patch size, in vertices, of patches that can be processed by the tessellation primitive generator.
    // This is specified by the patchControlPoints of the VkPipelineTessellationStateCreateInfo structure.
    m_limits.maxTessellationPatchSize = 32;

    // OGL: pHpCaps->maxVertexCountPerPatch = SI_MAX_VERTEX_COUNT_PER_PATCH;

    // Maximum number of components of input variables which may be provided as per-vertex inputs to the tessellation
    // control shader stage.
    m_limits.maxTessellationControlPerVertexInputComponents = 128;

    // OGL: pHpCaps->maxTessControlInputComponents = SI_MAX_TESS_CONTROL_INPUT_COMPONENTS;

    // Maximum number of components of per-vertex output variables which may be output from the tessellation control
    // shader stage.
    m_limits.maxTessellationControlPerVertexOutputComponents = 128;

    // OGL: pHpCaps->maxHullVaryingComponents = SI_MAX_TESS_CONTROL_INPUT_COMPONENTS;

    // Maximum number of components of per-patch output variables which may be output from the tessellation control
    // shader stage.
    m_limits.maxTessellationControlPerPatchOutputComponents = 120;

    // OGL: pHpCaps->maxTessControlPatchComponents = SI_MAX_TESS_CONTROL_PATCH_COMPONENTS;

    // Maximum total number of components of per-vertex and per-patch output variables which may be output from the
    // tessellation control shader stage.  (The total number of components of active per-vertex and per-patch outputs is
    // derived by multiplying the per-vertex output component count by the output patch size and then adding the
    // per-patch output component count.  The total component count may not exceed this limit.)
    m_limits.maxTessellationControlTotalOutputComponents = 4096;

    // OGL: pHpCaps->maxTessControlTotalOutputComponents = SI_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS;

    // Maximum number of components of input variables which may be provided as per-vertex inputs to the tessellation
    // evaluation shader stage.
    m_limits.maxTessellationEvaluationInputComponents = 128;

    // OGL: pDpCaps->maxTessEvaluationInputComponents = SI_MAX_TESS_CONTROL_INPUT_COMPONENTS [sic]

    // Maximum number of components of per-vertex output variables which may be output from the tessellation evaluation
    // shader stage
    m_limits.maxTessellationEvaluationOutputComponents = 128;

    // OGL: pDpCaps->maxDomainVaryingComponents = SI_MAX_TESS_CONTROL_INPUT_COMPONENTS [sic]

    // Maximum invocation count (per input primitive) supported for an instanced geometry shader.
    m_limits.maxGeometryShaderInvocations = 127;

    // OGL: pGpCaps->maxGeometryInvocations = SI_MAX_GP_INVOCATIONS

    // Maximum number of components of input variables which may be provided as inputs to the geometry shader stage
    m_limits.maxGeometryInputComponents = 128;

    // OGL: pGpCaps->maxGeometryVaryingComponents = SI_MAX_GP_VARYING_COMPONENTS

    // Maximum number of components of output variables which may be output from the geometry shader stage.
    m_limits.maxGeometryOutputComponents = 128;

    // OGL: pGpCaps->maxGeometryVaryingComponents = SI_MAX_GP_VARYING_COMPONENTS; (NOTE: Not a separate cap)

    // Maximum number of vertices which may be emitted by any geometry shader.
    m_limits.maxGeometryOutputVertices = 1024;

    // OGL: pGpCaps->maxGeometryOutputVertices = SI_MAX_GP_OUTPUT_VERTICES;

    // Maximum total number of components of output, across all emitted vertices, which may be output from the geometry
    // shader stage.
    m_limits.maxGeometryTotalOutputComponents = 4 * 4096;

    // OGL: pGpCaps->maxGeometryTotalOutputComponents = SI_MAX_GP_TOTAL_OUTPUT_COMPONENTS;

    // Maximum number of components of input variables which may be provided as inputs to the fragment shader stage.
    m_limits.maxFragmentInputComponents = 128;

    // OGL: pFpCaps->maxFragmentInputComponents = SI_MAX_VP_VARYING_COMPONENTS;

    // Maximum number of output attachments which may be written to by the fragment shader stage.
    m_limits.maxFragmentOutputAttachments = Pal::MaxColorTargets;

    // Maximum number of output attachments which may be written to by the fragment shader stage when blending is
    // enabled and one of the dual source blend modes is in use.
    m_limits.maxFragmentDualSrcAttachments = 1;

    // OGL: pCaps->buf.maxDualSourceDrawBuf = SI_MAX_DUAL_SOURCE_COLOR_BUFFERS;

    // NOTE: This could be num_cbs / 2 = 4.  When dual source blending is on, two source colors are written per
    // attachment and to facilitate this the HW operates such that the odd-numbered CBs do not get used.  OGL still
    // reports only 1 dual source attachment though, and I think DX API spec locks you into a single dual source
    // attachment also, (which means more than 1 is actually not fully tested by any driver), so for safety we
    // conservatively also only report 1 dual source attachment.

    // The total number of storage buffers, storage images, and output buffers which may be used in the fragment
    // shader stage.
    m_limits.maxFragmentCombinedOutputResources = UINT_MAX;

    // Maximum total storage size, in bytes, of all variables declared with the WorkgroupLocal SPIRV Storage
    // Class (the shared storage qualifier in GLSL) in the compute shader stage.
    m_limits.maxComputeSharedMemorySize = 32 * 1024;

    // OGL: pCpCaps->maxComputeSharedMemorySize = SI_MAX_LDS_SIZE;

    // Maximum number of work groups that may be dispatched by a single dispatch command.  These three values represent
    // the maximum number of work groups for the X, Y, and Z dimensions, respectively.  The x, y, and z parameters to
    // the vkCmdDispatch command, or members of the VkDispatchIndirectCmd structure must be less than or equal to the
    // corresponding limit.
    m_limits.maxComputeWorkGroupCount[0] = 65535;
    m_limits.maxComputeWorkGroupCount[1] = 65535;
    m_limits.maxComputeWorkGroupCount[2] = 65535;

    // OGL: pCpCaps->maxComputeWorkGroupCount[i] = SI_MAX_WORK_GROUP_COUNT;

    const uint32_t clampedMaxThreads = Util::Min(palProps.gfxipProperties.maxThreadGroupSize,
                                                 palProps.gfxipProperties.maxAsyncComputeThreadGroupSize);

    m_limits.maxComputeWorkGroupInvocations = clampedMaxThreads;

    // Maximum size of a local compute work group, per dimension.These three values represent the maximum local work
    // group size in the X, Y, and Z dimensions, respectively.The x, y, and z sizes specified by the LocalSize
    // Execution Mode in SPIR - V must be less than or equal to the corresponding limit.
    m_limits.maxComputeWorkGroupSize[0] = clampedMaxThreads;
    m_limits.maxComputeWorkGroupSize[1] = clampedMaxThreads;
    m_limits.maxComputeWorkGroupSize[2] = clampedMaxThreads;

    // Number of bits of subpixel precision in x/y screen coordinates.
    m_limits.subPixelPrecisionBits = 8;

    // NOTE: We support higher sub-pixel precisions but not for arbitrary sized viewports (or specifically
    // guardbands).  PAL always uses the minimum 8-bit sub-pixel precision at the moment.

    // The number of bits of precision in the division along an axis of a texture used for minification and
    // magnification filters. 2^subTexelPrecisionBits is the actual number of divisions along each axis of the texture
    // represented.  The filtering hardware will snap to these locations when computing the filtered results.
    m_limits.subTexelPrecisionBits = 8;

    // The number of bits of division that the LOD calculation for mipmap fetching get snapped to when determining the
    // contribution from each miplevel to the mip filtered results. 2 ^ mipmapPrecisionBits is the actual number of
    // divisions. For example, if this value is 2 bits then when linearly filtering between two levels each level could
    // contribute: 0%, 33%, 66%, or 100% (note this is just an example and the amount of contribution should be covered
    // by different equations in the spec).
    m_limits.mipmapPrecisionBits = 8;

    // Maximum index value that may be used for indexed draw calls when using 32-bit indices. This excludes the
    // primitive restart index value of 0xFFFFFFFF
    m_limits.maxDrawIndexedIndexValue = UINT_MAX;

    // Maximum instance count that is supported for indirect draw calls.
    m_limits.maxDrawIndirectCount = UINT_MAX;

    // NOTE: Primitive restart for patches (or any non-strip topology) makes no sense.

    // Maximum absolute sampler level of detail bias.
    m_limits.maxSamplerLodBias = Util::Math::SFixedToFloat(0xFFF, 5, 8);

    // NOTE: LOD_BIAS SRD field has a 5.8 signed fixed format so the maximum positive value is 0xFFF

    // Maximum degree of sampler anisotropy
    m_limits.maxSamplerAnisotropy = 16.0f;

    // Maximum number of active viewports. The value of the viewportCount member of the
    // VkPipelineViewportStateCreateInfo structure that is provided at pipeline creation must be less than or equal to
    // the value of this limit.
    m_limits.maxViewports = Pal::MaxViewports;

    // NOTE: These are temporarily from gfx6Chip.h

    // Maximum viewport dimensions in the X(width) and Y(height) dimensions, respectively.  The maximum viewport
    // dimensions must be greater than or equal to the largest image which can be created and used as a
    // framebuffer attachment.

    // NOTE: We shouldn't export the actual HW bounds for viewport coordinates as we need space for the guardband.
    // Instead use the following values which are suitable to render to any image:
    m_limits.maxViewportDimensions[0] = 16384;
    m_limits.maxViewportDimensions[1] = 16384;

    // Viewport bounds range [minimum,maximum]. The location of a viewport's upper-left corner are clamped to be
    // within the range defined by these limits.

    m_limits.viewportBoundsRange[0] = -32768;
    m_limits.viewportBoundsRange[1] = 32767;

    // Number of bits of subpixel precision for viewport bounds.The subpixel precision that floating - point viewport
    // bounds are interpreted at is given by this limit.
    m_limits.viewportSubPixelBits = m_limits.subPixelPrecisionBits;

    // NOTE: My understanding is that the viewport transform offset and scale is done in floating-point, so there is
    // no internal fixed subpixel resolution for the floating-point viewport offset.  However, immediately after
    // the offset and scale, the VTE converts the screen-space position to subpixel precision, so that is why we report
    // the same limit here.

    // Minimum required alignment, in bytes, of pointers returned by vkMapMemory. Subtracting offset bytes from the
    // returned pointer will always produce a multiple of the value of this limit.
    m_limits.minMemoryMapAlignment = 64;

    // NOTE: The WDDM lock function will always map at page boundaries, but for safety let's just stick with the
    // limit required.

    // Minimum required alignment, in bytes, for the offset member of the VkBufferViewCreateInfo structure for texel
    // buffers.  When a buffer view is created for a buffer which was created with
    // VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT or VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT set in the usage member
    // of the VkBufferCreateInfo structure, the offset must be an integer multiple of this limit.
    m_limits.minTexelBufferOffsetAlignment = 1;

    // NOTE: The buffers above are formatted buffers (i.e. typed buffers in PAL terms).  Their offset additionally must
    // be aligned on element size boundaries, and that is not reflected in the above limit.

    // Minimum required alignment, in bytes, for the offset member of the VkDescriptorBufferInfo structure for uniform
    // buffers.  When a descriptor of type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or
    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC is updated to reference a buffer which was created with the
    // VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT set in the usage member of the VkBufferCreateInfo structure, the offset must
    // be an integer multiple of this limit.
    m_limits.minUniformBufferOffsetAlignment = 16;

    // NOTE: Uniform buffer SRDs are internally created as typed RGBA32_UINT with a stride of 16 bytes because that is
    // what SC expects.  Due to the offset alignment having to match the element size for typed buffer SRDs, we set the
    // required min alignment here to 16.

    // Minimum required alignment, in bytes, for the offset member of the VkDescriptorBufferInfo structure for storage
    // buffers.  When a descriptor of type VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC is updated to reference a buffer which was created with the
    // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT set in the usage member of the VkBufferCreateInfo structure, the offset must
    // be an integer multiple of this limit.
    m_limits.minStorageBufferOffsetAlignment = 4;

    // Minimum/maximum offset value for the ConstOffset image operand of any of the OpImageSample* or OpImageFetch
    // SPIR-V image instructions.
    // These values are from the AMDIL specification and correspond to the optional "aoffset" operand that
    // applies an immediate texel-space integer offset to the texture coordinate prior to fetch.  The legal range of
    // these values is in 7.1 fixed point i.e. [-64..63.5]
    m_limits.minTexelOffset = -64;
    m_limits.maxTexelOffset = 63;

    // Minimum/maximum offset value for the Offset or ConstOffsets image operands of any of the OpImageGather or
    // OpImageDrefGather SPIR-V image instructions
    // These are similar limits as above except for the more restrictive AMDIL FETCH4PO instruction.
    m_limits.minTexelGatherOffset = -32;
    m_limits.maxTexelGatherOffset = 31;

    // Minimum negative offset value (inclusive) and maximum positive offset value (exclusive) for the offset operand
    // of the InterpolateAtOffset SPIR-V extended instruction.
    // This corresponds to the AMDIL EVAL_SNAPPED instruction which re-interpolates an interpolant some given
    // floating-point offset from the pixel center.  There are no known limitations to the inputs of this instruction
    // but we are picking reasonably safe values here.
    m_limits.minInterpolationOffset = -2.0f;
    m_limits.maxInterpolationOffset = 2.0f;

    // The number of subpixel fractional bits that the x and y offsets to the InterpolateAtOffset SPIR-V extended
    // instruction may be rounded to as fixed-point values.
    m_limits.subPixelInterpolationOffsetBits = m_limits.subPixelPrecisionBits;

    // NOTE: The above value is set to the subpixel resolution which governs the resolution of the original
    // barycentric coordinate at the pixel.  However, this is not exactly correct for the instruction here because
    // the additional offset and the subsequent re-evaluation should be at full floating-point precision.  Again though,
    // I'm not 100% sure about this so we are picking reasonably safe values here.

    // Required sample counts for all multisample images:
    const VkSampleCountFlags RequiredSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;

    // Maximum width, height, layer count for a framebuffer.  Note that this may be larger than the maximum renderable
    // image size for framebuffers that use no attachments.
    m_limits.maxFramebufferWidth  = 16384;
    m_limits.maxFramebufferHeight = 16384;
    m_limits.maxFramebufferLayers = MaxFramebufferLayers;

    // NOTE: These values are currently match OGL gfx6 values and they are probably overly conservative.  Need to
    // compare CB/DB limits and test with attachmentless framebuffers for proper limits.

    // Framebuffer sample count support determination
    {
        uint32_t maxColorSampleCount    = 0;
        uint32_t maxDepthSampleCount    = 0;
        uint32_t maxStencilSampleCount  = 0;

        for (uint32_t formatIdx = VK_FORMAT_BEGIN_RANGE; formatIdx <= VK_FORMAT_END_RANGE; formatIdx++)
        {
            const VkFormat format = static_cast<VkFormat>(formatIdx);

            if (Formats::IsDepthStencilFormat(format) == false)
            {
                uint32_t maxSamples = GetMaxFormatSampleCount(
                    this,
                    format,
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

                maxColorSampleCount = Util::Max(maxColorSampleCount, maxSamples);
            }
            else
            {
                uint32_t maxSamples = GetMaxFormatSampleCount(
                    this,
                    format,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

                if (Formats::HasDepth(format))
                {
                    maxDepthSampleCount = Util::Max(maxDepthSampleCount, maxSamples);
                }

                if (Formats::HasStencil(format))
                {
                    maxStencilSampleCount = Util::Max(maxStencilSampleCount, maxSamples);
                }
            }
        }

        // Supported color, depth, and stencil sample counts for a framebuffer attachment.
        m_limits.framebufferColorSampleCounts           = MaxSampleCountToSampleCountFlags(maxColorSampleCount);
        m_limits.framebufferDepthSampleCounts           = MaxSampleCountToSampleCountFlags(maxDepthSampleCount);
        m_limits.framebufferStencilSampleCounts         = MaxSampleCountToSampleCountFlags(maxStencilSampleCount);
        m_limits.framebufferNoAttachmentsSampleCounts   = m_limits.framebufferColorSampleCounts;

        VK_ASSERT((m_limits.framebufferColorSampleCounts   & RequiredSampleCounts) == RequiredSampleCounts);
        VK_ASSERT((m_limits.framebufferDepthSampleCounts   & RequiredSampleCounts) == RequiredSampleCounts);
        VK_ASSERT((m_limits.framebufferStencilSampleCounts & RequiredSampleCounts) == RequiredSampleCounts);
    }

    // NOTE: Although there is a "rasterSamples" field in the pipeline create info that seems to relate to
    // orthogonally defining coverage samples from color/depth fragments (i.e. EQAA), there is no limit to describe
    // the "maximum coverage samples".  The spec itself seems to not ever explicitly address the possibility of
    // having a larger number of raster samples to color/depth fragments, but it never seems to explicitly prohibit
    // it either.

    // Supported sample counts for attachment-less framebuffers
    m_limits.framebufferColorSampleCounts = VK_SAMPLE_COUNT_1_BIT |
                                            VK_SAMPLE_COUNT_2_BIT |
                                            VK_SAMPLE_COUNT_4_BIT |
                                            VK_SAMPLE_COUNT_8_BIT;

    m_sampleLocationSampleCounts = m_limits.framebufferColorSampleCounts;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 417
    if (m_properties.gfxipProperties.flags.support1xMsaaSampleLocations == false)
#endif
    {
        m_sampleLocationSampleCounts &= ~VK_SAMPLE_COUNT_1_BIT;
    }

    // Maximum number of color attachments that can be be referenced by a subpass in a render pass.
    m_limits.maxColorAttachments = Pal::MaxColorTargets;

    // Minimum supported sample count determination for images of different types
    {
        uint32_t minSampledCount        = UINT_MAX;
        uint32_t minSampledIntCount     = UINT_MAX;
        uint32_t minSampledDepthCount   = UINT_MAX;
        uint32_t minSampledStencilCount = UINT_MAX;
        uint32_t minStorageCount        = UINT_MAX;

        for (uint32_t formatIdx = VK_FORMAT_BEGIN_RANGE; formatIdx <= VK_FORMAT_END_RANGE; formatIdx++)
        {
            const VkFormat format = static_cast<VkFormat>(formatIdx);

            uint32_t maxSamples = GetMaxFormatSampleCount(
                this, format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT);

            if (maxSamples > 1)
            {
                const Pal::SwizzledFormat palFormat = VkToPalFormat(format);

                // Depth format
                if (Formats::HasDepth(format))
                {
                    minSampledDepthCount = Util::Min(minSampledDepthCount, maxSamples);
                }
                // Stencil format
                if (Formats::HasStencil(format))
                {
                    minSampledStencilCount = Util::Min(minSampledStencilCount, maxSamples);
                }
                // Integer color format
                else if (Pal::Formats::IsUint(palFormat.format) || Pal::Formats::IsSint(palFormat.format))
                {
                    minSampledIntCount = Util::Min(minSampledIntCount, maxSamples);
                }
                // Normalized/float color format
                else
                {
                    minSampledCount = Util::Min(minSampledCount, maxSamples);
                }
            }

            maxSamples = GetMaxFormatSampleCount(
                this, format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);

            if (maxSamples > 1)
            {
                minStorageCount = Util::Min(minStorageCount, maxSamples);
            }
        }

        // If we didn't find any supported format of a certain type then we report a minimum maximum sample count of
        // zero
        minSampledCount        = (minSampledCount        == UINT_MAX) ? 0 : minSampledCount;
        minSampledIntCount     = (minSampledIntCount     == UINT_MAX) ? 0 : minSampledIntCount;
        minSampledDepthCount   = (minSampledDepthCount   == UINT_MAX) ? 0 : minSampledDepthCount;
        minSampledStencilCount = (minSampledStencilCount == UINT_MAX) ? 0 : minSampledStencilCount;
        minStorageCount        = (minStorageCount        == UINT_MAX) ? 0 : minStorageCount;

        // This is a sanity check on the above logic.
        VK_ASSERT(minSampledCount == 8);
        VK_ASSERT(minSampledIntCount == 8);
        VK_ASSERT(minSampledDepthCount == 8);
        VK_ASSERT(minSampledStencilCount == 8);

        // Sample counts supported for all non-integer, integer, depth, and stencil sampled images, respectively
        m_limits.sampledImageColorSampleCounts      = MaxSampleCountToSampleCountFlags(minSampledCount);
        m_limits.sampledImageIntegerSampleCounts    = MaxSampleCountToSampleCountFlags(minSampledIntCount);
        m_limits.sampledImageDepthSampleCounts      = MaxSampleCountToSampleCountFlags(minSampledDepthCount);
        m_limits.sampledImageStencilSampleCounts    = MaxSampleCountToSampleCountFlags(minSampledStencilCount);

        // Sample counts supported for storage images
        m_limits.storageImageSampleCounts           = MaxSampleCountToSampleCountFlags(minStorageCount);
    }

    // Maximum number of components in the SampleMask or SampleMaskIn shader built-in.
    constexpr uint32_t MaxCoverageSamples = 16;

    m_limits.maxSampleMaskWords = (MaxCoverageSamples + 32 - 1) / 32;

    // Support for timestamps on all compute and graphics queues
    m_limits.timestampComputeAndGraphics = VK_TRUE;

    // The number of nanoseconds it takes for a timestamp value to be incremented by 1.
    m_limits.timestampPeriod = static_cast<float>(1000000000.0 / palProps.timestampFrequency);

    // Maximum number of clip/cull distances that can be written to via the ClipDistance/CullDistance shader built-in
    // in a single shader stage
    m_limits.maxClipDistances = 8;
    m_limits.maxCullDistances = 8;

    // Maximum combined number of clip and cull distances that can be written to via the ClipDistance and CullDistances
    // shader built-ins in a single shader stage
    m_limits.maxCombinedClipAndCullDistances = 8;

    // Number of discrete priorities that can be assigned to a queue based on the value of each member of
    // sname:VkDeviceQueueCreateInfo::pname:pQueuePriorities.
    m_limits.discreteQueuePriorities = 2;

    // The range[minimum, maximum] of supported sizes for points.  Values written to the PointSize shader built-in are
    // clamped to this range.
    constexpr uint32_t PointSizeMaxRegValue = 0xffff;
    constexpr uint32_t PointSizeIntBits = 12;
    constexpr uint32_t PointSizeFracBits = 4;

    m_limits.pointSizeRange[0] = 0.0f;
    m_limits.pointSizeRange[1] = Util::Math::UFixedToFloat(PointSizeMaxRegValue, PointSizeIntBits,
        PointSizeFracBits) * 2.0f;

    // The range[minimum, maximum] of supported widths for lines.  Values specified by the lineWidth member of the
    // VkPipelineRasterStateCreateInfo or the lineWidth parameter to vkCmdSetLineWidth are clamped to this range.
    constexpr uint32_t LineWidthMaxRegValue = 0xffff;
    constexpr uint32_t LineWidthIntBits = 12;
    constexpr uint32_t LineWidthFracBits = 4;

    m_limits.lineWidthRange[0] = 0.0f;
    m_limits.lineWidthRange[1] = Util::Math::UFixedToFloat(LineWidthMaxRegValue, LineWidthIntBits,
        LineWidthFracBits) * 2.0f;

    // NOTE: The same 12.4 half-size encoding is used for line widths as well.

    // The granularity of supported point sizes. Not all point sizes in the range defined by pointSizeRange are
    // supported. The value of this limit specifies the granularity (or increment) between successive supported point
    // sizes.
    m_limits.pointSizeGranularity = 2.0f / (1 << PointSizeFracBits);

    // NOTE: Numerator is 2 here instead of 1 because points are represented as half-sizes and not the diameter.

    // The granularity of supported line widths.Not all line widths in the range defined by lineWidthRange are
    // supported.  The value of this limit specifies the granularity(or increment) between successive supported line
    // widths.
    m_limits.lineWidthGranularity = 2.0f / (1 << LineWidthFracBits);

    // Tells whether lines are rasterized according to the preferred method of rasterization. If set to ename:VK_FALSE,
    // lines may: be rasterized under a relaxed set of rules. If set to ename:VK_TRUE, lines are rasterized as per the
    // strict definition.

    m_limits.strictLines = VK_FALSE;

    // Tells whether rasterization uses the standard sample locations. If set to VK_TRUE, the implementation uses the
    // documented sample locations. If set to VK_FALSE, the implementation may: use different sample locations.
    m_limits.standardSampleLocations = VK_TRUE;

    // Optimal buffer offset alignment in bytes for vkCmdCopyBufferToImage and vkCmdCopyImageToBuffer.
    m_limits.optimalBufferCopyOffsetAlignment = 1;

    // Optimal buffer row pitch alignment in bytes for vkCmdCopyBufferToImage and vkCmdCopyImageToBuffer.
    m_limits.optimalBufferCopyRowPitchAlignment = 1;

    // The size and alignment in bytes that bounds concurrent access to host-mapped device memory.
    m_limits.nonCoherentAtomSize = 128;

}

// =====================================================================================================================
// Retrieve surface capabilities. Called in response to vkGetPhysicalDeviceSurfaceCapabilitiesKHR
template <typename T>
VkResult PhysicalDevice::GetSurfaceCapabilities(
    VkSurfaceKHR    surface,
    T               pSurfaceCapabilities
    ) const
{
    VkResult result = VK_SUCCESS;
    DisplayableSurfaceInfo displayableInfo = {};

    Surface* pSurface = Surface::ObjectFromHandle(surface);
    result = UnpackDisplayableSurface(pSurface, &displayableInfo);

    if (result == VK_SUCCESS)
    {
        Pal::SwapChainProperties swapChainProperties = {};
        if (displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY)
        {
            VkIcdSurfaceDisplay* pDisplaySurface     = pSurface->GetDisplaySurface();
            swapChainProperties.currentExtent.width  = pDisplaySurface->imageExtent.width;
            swapChainProperties.currentExtent.height = pDisplaySurface->imageExtent.height;
        }
        result = PalToVkResult(m_pPalDevice->GetSwapChainInfo(
            displayableInfo.displayHandle,
            displayableInfo.windowHandle,
            displayableInfo.palPlatform,
            &swapChainProperties));

        if (result == VK_SUCCESS)
        {
            {
                // From Vulkan spec, currentExtent of a valid window surface(Win32/Xlib/Xcb) must have both width
                // and height greater than 0, or both of them 0.
                pSurfaceCapabilities->currentExtent.width   =
                    (swapChainProperties.currentExtent.height == 0) ? 0 : swapChainProperties.currentExtent.width;
                pSurfaceCapabilities->currentExtent.height  =
                    (swapChainProperties.currentExtent.width == 0) ? 0 : swapChainProperties.currentExtent.height;
                pSurfaceCapabilities->minImageExtent.width  = swapChainProperties.minImageExtent.width;
                pSurfaceCapabilities->minImageExtent.height = swapChainProperties.minImageExtent.height;
                pSurfaceCapabilities->maxImageExtent.width  = swapChainProperties.maxImageExtent.width;
                pSurfaceCapabilities->maxImageExtent.height = swapChainProperties.maxImageExtent.height;
                pSurfaceCapabilities->maxImageArrayLayers   = swapChainProperties.maxImageArraySize;
            }

            pSurfaceCapabilities->minImageCount = swapChainProperties.minImageCount;
            pSurfaceCapabilities->maxImageCount = swapChainProperties.maxImageCount;

            pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

            pSurfaceCapabilities->supportedTransforms = swapChainProperties.supportedTransforms;
            pSurfaceCapabilities->currentTransform = PalToVkSurfaceTransform(swapChainProperties.currentTransforms);

            pSurfaceCapabilities->supportedUsageFlags = PalToVkImageUsageFlags(swapChainProperties.supportedUsageFlags);
        }
    }

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetSurfaceCapabilities2KHR(
    const VkPhysicalDeviceSurfaceInfo2KHR*  pSurfaceInfo,
    VkSurfaceCapabilities2KHR*              pSurfaceCapabilities) const
{
    union
    {
        const VkStructHeader*                  pHeader;
        const VkPhysicalDeviceSurfaceInfo2KHR* pVkPhysicalDeviceSurfaceInfo2KHR;
        VkSurfaceCapabilities2KHR*             pVkSurfaceCapabilities2KHR;
    };

    VkResult result = VK_SUCCESS;

    VkSurfaceKHR       surface = VK_NULL_HANDLE;

    for (pVkPhysicalDeviceSurfaceInfo2KHR = pSurfaceInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR:
            {
                surface = pVkPhysicalDeviceSurfaceInfo2KHR->surface;
                break;
            }
            default:
                break;
        }
    }

    for (pVkSurfaceCapabilities2KHR = pSurfaceCapabilities;
        (pHeader != nullptr) && (result == VK_SUCCESS);
        pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR:
            {
                VK_ASSERT(surface != VK_NULL_HANDLE);
                result = GetSurfaceCapabilities(surface, &pVkSurfaceCapabilities2KHR->surfaceCapabilities);
                break;
            }

            default:
                break;
        }
    }

    return result;
}

// =====================================================================================================================
// Determine if the presentation is supported upon the requested connection
VkBool32 PhysicalDevice::DeterminePresentationSupported(
    Pal::OsDisplayHandle hDisplay,
    VkIcdWsiPlatform     platform,
    int64_t              visualId,
    uint32_t             queueFamilyIndex)
{
    VkBool32 ret    = VK_FALSE;
    VkResult result = PalToVkResult(m_pPalDevice->DeterminePresentationSupported(hDisplay,
                                                                                 VkToPalWsiPlatform(platform),
                                                                                 visualId));

    if (result == VK_SUCCESS)
    {
        const bool supported = QueueSupportsPresents(queueFamilyIndex, platform);

        ret = supported ? VK_TRUE: VK_FALSE;
    }
    else
    {
        ret = VK_FALSE;
    }

    return ret;
}

// =====================================================================================================================
// Retrieve surface present modes. Called in response to vkGetPhysicalDeviceSurfacePresentModesKHR
VkResult PhysicalDevice::GetSurfacePresentModes(
    const DisplayableSurfaceInfo& displayableInfo,
    Pal::PresentMode              presentType,
    uint32_t*                     pPresentModeCount,
    VkPresentModeKHR*             pPresentModes
) const
{
    VkPresentModeKHR presentModes[4] = {};
    uint32_t modeCount = 0;

    // Query which swap chain modes are supported by the window/display
    Pal::SwapChainProperties swapChainProperties = {};
    if (displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY)
    {
        swapChainProperties.currentExtent.width  = displayableInfo.surfaceExtent.width;
        swapChainProperties.currentExtent.height = displayableInfo.surfaceExtent.height;
    }
    Pal::Result palResult = m_pPalDevice->GetSwapChainInfo(
        displayableInfo.displayHandle,
        displayableInfo.windowHandle,
        displayableInfo.palPlatform,
        &swapChainProperties);

    if (palResult == Pal::Result::Success)
    {
        // Get which swap chain modes are supported for the given present type (windowed vs fullscreen)
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
        const uint32_t swapChainModes =
            m_pPalDevice->GetSupportedSwapChainModes(displayableInfo.palPlatform, presentType);
#else
        const uint32_t swapChainModes =
            m_properties.swapChainProperties.supportedSwapChainModes[static_cast<size_t>(presentType)];
#endif
        // Translate to Vulkan present modes
        if (swapChainModes & Pal::SwapChainModeSupport::SupportImmediateSwapChain)
        {
            presentModes[modeCount++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }

        if (swapChainModes & Pal::SwapChainModeSupport::SupportMailboxSwapChain)
        {
            presentModes[modeCount++] = VK_PRESENT_MODE_MAILBOX_KHR;
        }

        if (swapChainModes & Pal::SwapChainModeSupport::SupportFifoSwapChain)
        {
            presentModes[modeCount++] = VK_PRESENT_MODE_FIFO_KHR;
        }

        if (swapChainModes & Pal::SwapChainModeSupport::SupportFifoRelaxedSwapChain)
        {
            presentModes[modeCount++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
    }

    // Write out information
    VkResult result = VK_SUCCESS;

    if (result == VK_SUCCESS)
    {
        if (pPresentModes == NULL)
        {
            *pPresentModeCount = modeCount;
        }
        else
        {
            uint32_t writeCount = Util::Min(modeCount, *pPresentModeCount);

            for (uint32_t i = 0; i < writeCount; ++i)
            {
                pPresentModes[i] = presentModes[i];
            }

            *pPresentModeCount = writeCount;

            result = (writeCount >= modeCount) ? result : VK_INCOMPLETE;
        }
    }

    return result;
}

// =====================================================================================================================
// Retrieve display and window handles from the VKSurface object
VkResult PhysicalDevice::UnpackDisplayableSurface(
    Surface*                pSurface,
    DisplayableSurfaceInfo* pInfo)
{
    VkResult result = VK_SUCCESS;

    if (pSurface->GetXcbSurface()->base.platform == VK_ICD_WSI_PLATFORM_XCB)
    {
        const VkIcdSurfaceXcb* pXcbSurface = pSurface->GetXcbSurface();

        pInfo->icdPlatform   = pXcbSurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pXcbSurface->base.platform);
        pInfo->displayHandle = pXcbSurface->connection;
        pInfo->windowHandle.win  = pXcbSurface->window;
    }
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    else if (pSurface->GetWaylandSurface()->base.platform == VK_ICD_WSI_PLATFORM_WAYLAND)
    {
        const VkIcdSurfaceWayland* pWaylandSurface = pSurface->GetWaylandSurface();

        pInfo->icdPlatform   = pWaylandSurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pWaylandSurface->base.platform);
        pInfo->displayHandle = pWaylandSurface->display;
        pInfo->windowHandle.pSurface  = pWaylandSurface->surface;
    }
#endif
    else if (pSurface->GetXlibSurface()->base.platform == VK_ICD_WSI_PLATFORM_XLIB)
    {
        const VkIcdSurfaceXlib* pXlibSurface = pSurface->GetXlibSurface();

        pInfo->icdPlatform   = pXlibSurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pXlibSurface->base.platform);
        pInfo->displayHandle = pXlibSurface->dpy;
        pInfo->windowHandle.win  = pXlibSurface->window;
    }
    else if (pSurface->GetDisplaySurface()->base.platform == VK_ICD_WSI_PLATFORM_DISPLAY)
    {
        VkIcdSurfaceDisplay* pDisplaySurface = pSurface->GetDisplaySurface();
        pInfo->icdPlatform   = pDisplaySurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pDisplaySurface->base.platform);;
        pInfo->surfaceExtent = pDisplaySurface->imageExtent;
        DisplayModeObject* pDisplayMode = reinterpret_cast<DisplayModeObject*>(pDisplaySurface->displayMode);
        pInfo->pScreen       = pDisplayMode->pScreen;
    }
    else
    {
        result = VK_ERROR_SURFACE_LOST_KHR;
    }

    return result;
}

// =====================================================================================================================
// Returns the presentable image formats we support for both windowed and fullscreen modes
VkResult PhysicalDevice::GetSurfaceFormats(
    Surface*            pSurface,
    uint32_t*           pSurfaceFormatCount,
    VkSurfaceFormatKHR* pSurfaceFormats) const
{
    VkResult result = VK_SUCCESS;

    uint32_t numPresentFormats = 0;
    const uint32_t maxBufferCount = (pSurfaceFormats != nullptr) ? *pSurfaceFormatCount : 0;

    {
        // Windowed Presents

        // The w/a here will be removed once more presentable format is supported on base driver side.
        const VkSurfaceFormatKHR formatList[] = {
            { VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
            { VK_FORMAT_B8G8R8A8_SRGB,  VK_COLORSPACE_SRGB_NONLINEAR_KHR } };
        const uint32_t formatCount = sizeof(formatList) / sizeof(formatList[0]);

        if (pSurfaceFormats == nullptr)
        {
            *pSurfaceFormatCount = formatCount;
        }
        else
        {
            uint32_t count = Util::Min(*pSurfaceFormatCount, formatCount);

            for (uint32_t i = 0; i < count; i++)
            {
                pSurfaceFormats[i].format = formatList[i].format;
                pSurfaceFormats[i].colorSpace = formatList[i].colorSpace;
            }

            if (count < formatCount)
            {
                result = VK_INCOMPLETE;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// called in response to vkGetPhysicalDeviceSurfaceFormats2KHR
VkResult PhysicalDevice::GetSurfaceFormats(
    Surface*             pSurface,
    uint32_t*            pSurfaceFormatCount,
    VkSurfaceFormat2KHR* pSurfaceFormats) const
{
    VkResult result = VK_SUCCESS;
    if (pSurfaceFormats == nullptr)
    {
        result = GetSurfaceFormats(pSurface, pSurfaceFormatCount, static_cast<VkSurfaceFormatKHR*>(nullptr));
    }
    else
    {
        VkSurfaceFormatKHR* pTempSurfaceFormats = static_cast<VkSurfaceFormatKHR*>(Manager()->VkInstance()->AllocMem(
                sizeof(VkSurfaceFormatKHR) * *pSurfaceFormatCount,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (pTempSurfaceFormats == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        result = GetSurfaceFormats(pSurface, pSurfaceFormatCount, pTempSurfaceFormats);

        for (uint32_t i = 0; i < *pSurfaceFormatCount; i++)
        {
            VK_ASSERT(pSurfaceFormats[i].sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
            pSurfaceFormats[i].surfaceFormat = pTempSurfaceFormats[i];
        }

        Manager()->VkInstance()->FreeMem(pTempSurfaceFormats);
    }

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetPhysicalDevicePresentRectangles(
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects)
{
    VK_NOT_IMPLEMENTED;

    return VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Get available device extensions or populate the specified physical device with the extensions supported by it.
//
// If the device pointer is not nullptr, this function returns all extensions supported by that physical device.
//
// If the device pointer is nullptr, all available device extensions are returned (though not necessarily ones
// supported on every device).
DeviceExtensions::Supported PhysicalDevice::GetAvailableExtensions(
    const Instance*       pInstance,
    const PhysicalDevice* pPhysicalDevice)
{
    DeviceExtensions::Supported availableExtensions;

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_DRAW_PARAMETERS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SWAPCHAIN));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_RASTERIZATION_ORDER));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_TRINARY_MINMAX));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_EXPLICIT_VERTEX_PARAMETER));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GCN_SHADER));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_BALLOT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_DRAW_INDIRECT_COUNT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DRAW_INDIRECT_COUNT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_SUBGROUP_BALLOT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_SUBGROUP_VOTE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_STENCIL_EXPORT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_VIEWPORT_INDEX_LAYER));

    // Don't report VK_AMD_negative_viewport_height in Vulkan 1.1, it must not be used.
    if (pInstance->GetAPIVersion() < VK_MAKE_VERSION(1, 1, 0))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_NEGATIVE_VIEWPORT_HEIGHT));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_IMAGE_LOAD_STORE_LOD));

    if (pInstance->IsExtensionSupported(InstanceExtensions::KHX_DEVICE_GROUP_CREATION))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHX_DEVICE_GROUP));
    }

    if (pInstance->IsExtensionSupported(InstanceExtensions::KHR_DEVICE_GROUP_CREATION))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DEVICE_GROUP));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_BIND_MEMORY2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DEDICATED_ALLOCATION));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DESCRIPTOR_UPDATE_TEMPLATE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_MEMORY));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_MEMORY_FD));

    if (pInstance->IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_SEMAPHORE_CAPABILITIES))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_SEMAPHORE));
        if ((pPhysicalDevice == nullptr) ||
            (pPhysicalDevice->PalProperties().osProperties.supportOpaqueFdSemaphore))
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_SEMAPHORE_FD));
        }
    }
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_GET_MEMORY_REQUIREMENTS2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE1));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SAMPLER_FILTER_MINMAX));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE3));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_RELAXED_BLOCK_LAYOUT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_IMAGE_FORMAT_LIST));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_8BIT_STORAGE));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_CREATE_RENDERPASS2));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_INFO));

    if ((pPhysicalDevice == nullptr) ||
        ((pPhysicalDevice->PalProperties().gfxipProperties.flags.support16BitInstructions) &&
         ((pPhysicalDevice->GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
          (pPhysicalDevice->PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9))))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPU_SHADER_HALF_FLOAT));
    }

    if ((pPhysicalDevice == nullptr) ||
        pPhysicalDevice->PalProperties().gfxipProperties.flags.supportFp16Fetch)
    {
    }

    if ((pPhysicalDevice == nullptr) ||
        ((pPhysicalDevice->PalProperties().gfxipProperties.flags.support16BitInstructions) &&
         ((pPhysicalDevice->GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
          (pPhysicalDevice->PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9))))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPU_SHADER_INT16));
    }
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_FRAGMENT_MASK));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_TEXTURE_GATHER_BIAS_LOD));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_MIXED_ATTACHMENT_SAMPLES));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SAMPLE_LOCATIONS));

    // If RGP tracing is enabled, report support for VK_EXT_debug_marker extension since RGP traces can trap
    // application-provided debug markers and visualize them in RGP traces.
    if (pInstance->IsTracingSupportEnabled())
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DEBUG_MARKER));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_STORAGE_BUFFER_STORAGE_CLASS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_16BIT_STORAGE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPA_INTERFACE));

    if ((pPhysicalDevice == nullptr) ||
        (pPhysicalDevice->PalProperties().osProperties.supportQueuePriority))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_GLOBAL_PRIORITY));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_FENCE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_FENCE_FD));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MULTIVIEW));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_BUFFER_MARKER));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_EXTERNAL_MEMORY_HOST));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DEPTH_RANGE_UNRESTRICTED));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_CORE_PROPERTIES));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_QUEUE_FAMILY_FOREIGN));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DESCRIPTOR_INDEXING));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_VARIABLE_POINTERS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_VERTEX_ATTRIBUTE_DIVISOR));

    return availableExtensions;
}

// =====================================================================================================================
// Populates the device queue families. Note that there's not a one-to-one association between PAL queue types and
// Vulkan queue families due to many reasons:
// - We statically don't expose all PAL queue types
// - We dynamically don't expose PAL queue types that don't have the associated extension/feature enabled
// - We dynamically don't expose PAL queue types that don't have any queues present on the device
void PhysicalDevice::PopulateQueueFamilies()
{
    static const uint32_t vkQueueFlags[] =
    {
        // Pal::EngineTypeUniversal
        VK_QUEUE_GRAPHICS_BIT |
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT,
        // Pal::EngineTypeCompute
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT,
        // Pal::EngineTypeExclusiveCompute
        0,
        // Pal::EngineTypeDma
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT,
        // Pal::EngineTypeTimer
        0,
        // Pal::EngineTypeHighPriorityUniversal
        0,
        // Pal::EngineTypeHighPriorityGraphics
        0,
    };

    // While it's possible for an engineType to support multiple queueTypes,
    // we'll simplify things by associating each engineType with a primary queueType.
    static const Pal::QueueType palQueueTypes[] =
    {
        Pal::QueueTypeUniversal,
        Pal::QueueTypeCompute,
        Pal::QueueTypeCompute,
        Pal::QueueTypeDma,
        Pal::QueueTypeTimer,
        Pal::QueueTypeUniversal,
        Pal::QueueTypeUniversal,
    };

    static_assert((VK_ARRAY_SIZE(vkQueueFlags) == Pal::EngineTypeCount) &&
                  (VK_ARRAY_SIZE(palQueueTypes) == Pal::EngineTypeCount) &&
                  (Pal::EngineTypeUniversal        == 0) &&
                  (Pal::EngineTypeCompute          == 1) &&
                  (Pal::EngineTypeExclusiveCompute == 2) &&
                  (Pal::EngineTypeDma              == 3) &&
                  (Pal::EngineTypeTimer            == 4) &&
                  (Pal::EngineTypeHighPriorityUniversal == 0x5) &&
                  (Pal::EngineTypeHighPriorityGraphics  == 0x6),
        "PAL engine types have changed, need to update the tables above");

    // Always enable core queue flags.  Final determination of support will be done on a per-engine basis.
    uint32_t enabledQueueFlags =
        VK_QUEUE_GRAPHICS_BIT |
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT;

    // find out the sub engine index of VrHighPriority.
    const auto& exclusiveComputeProps = m_properties.engineProperties[Pal::EngineTypeExclusiveCompute];
    for (uint32_t subEngineIndex = 0; subEngineIndex < exclusiveComputeProps.engineCount; subEngineIndex++)
    {
        if (exclusiveComputeProps.engineSubType[subEngineIndex] == Pal::EngineSubType::VrHighPriority)
        {
            m_vrHighPrioritySubEngineIndex = subEngineIndex;
        }
    }

    // Determine the queue family to PAL engine type mapping and populate its properties
    for (uint32_t engineType = 0; engineType < Pal::EngineTypeCount; ++engineType)
    {
        // Only add queue families for PAL engine types that have at least one queue present and that supports some
        // functionality exposed in Vulkan.
        const auto& engineProps = m_properties.engineProperties[engineType];

        // Update supportedQueueFlags based on what is enabled, as well as specific engine properties.
        // In particular, sparse binding support requires the engine to support virtual memory remap.
        uint32_t supportedQueueFlags = enabledQueueFlags;
        if (engineProps.flags.supportVirtualMemoryRemap == 0)
        {
            supportedQueueFlags &= ~VK_QUEUE_SPARSE_BINDING_BIT;
        }

        if ((engineProps.engineCount != 0) && ((vkQueueFlags[engineType] & supportedQueueFlags) != 0))
        {
            m_queueFamilies[m_queueFamilyCount].palEngineType = static_cast<Pal::EngineType>(engineType);

            const Pal::QueueType primaryQueueType = palQueueTypes[GetQueueFamilyPalEngineType(m_queueFamilyCount)];
            VK_ASSERT((engineProps.queueSupport & (1 << primaryQueueType)) != 0);
            m_queueFamilies[m_queueFamilyCount].palQueueType = primaryQueueType;

            uint32_t palImageLayoutFlag = 0;
            uint32_t transferGranularityOverride = 0;

            switch (engineType)
            {
            case Pal::EngineTypeUniversal:
                palImageLayoutFlag          = Pal::LayoutUniversalEngine;
                transferGranularityOverride = m_settings.transferGranularityUniversalOverride;
                break;
            case Pal::EngineTypeCompute:
            case Pal::EngineTypeExclusiveCompute:
                palImageLayoutFlag          = Pal::LayoutComputeEngine;
                transferGranularityOverride = m_settings.transferGranularityComputeOverride;
                break;
            case Pal::EngineTypeDma:
                palImageLayoutFlag          = Pal::LayoutDmaEngine;
                transferGranularityOverride = m_settings.transferGranularityDmaOverride;
                break;
            default:
                break; // no-op
            }

            m_queueFamilies[m_queueFamilyCount].palImageLayoutFlag = palImageLayoutFlag;

            VkQueueFamilyProperties* pQueueFamilyProps     = &m_queueFamilies[m_queueFamilyCount].properties;

            pQueueFamilyProps->queueFlags                  = (vkQueueFlags[engineType] & supportedQueueFlags);
            pQueueFamilyProps->queueCount                  = engineProps.engineCount;
            pQueueFamilyProps->timestampValidBits          = (engineProps.flags.supportsTimestamps != 0) ? 64 : 0;
            pQueueFamilyProps->minImageTransferGranularity = PalToVkExtent3d(engineProps.minTiledImageCopyAlignment);

            // Override reported transfer granularity via panel setting
            if ((transferGranularityOverride & 0xf0000000) != 0)
            {
                pQueueFamilyProps->minImageTransferGranularity.width  = ((transferGranularityOverride >> 0)  & 0xff);
                pQueueFamilyProps->minImageTransferGranularity.height = ((transferGranularityOverride >> 8)  & 0xff);
                pQueueFamilyProps->minImageTransferGranularity.depth  = ((transferGranularityOverride >> 16) & 0xff);
            }

            m_queueFamilyCount++;
        }
    }
}

// =====================================================================================================================
// Retrieve an array of supported physical device-level extensions.
VkResult PhysicalDevice::EnumerateExtensionProperties(
    const char*            pLayerName,
    uint32_t*              pPropertyCount,
    VkExtensionProperties* pProperties
    ) const
{
    VkResult result = VK_SUCCESS;

    const DeviceExtensions::Supported& supportedExtensions = GetSupportedExtensions();
    const uint32_t extensionCount = supportedExtensions.GetExtensionCount();

    if (pProperties == nullptr)
    {
        *pPropertyCount = extensionCount;
        return VK_SUCCESS;
    }

    // Expect to return all extensions
    uint32_t copyCount = extensionCount;

    // If not all extensions can be reported then we have to adjust the copy count and return VK_INCOMPLETE at the end
    if (*pPropertyCount < extensionCount)
    {
        copyCount = *pPropertyCount;
        result = VK_INCOMPLETE;
    }

    // Report the actual number of extensions that will be returned
    *pPropertyCount = copyCount;

    // Loop through all extensions known to the driver
    for (int32_t i = 0; (i < DeviceExtensions::Count) && (copyCount > 0); ++i)
    {
        const DeviceExtensions::ExtensionId id = static_cast<DeviceExtensions::ExtensionId>(i);

        // If this extension is supported then report it
        if (supportedExtensions.IsExtensionSupported(id))
        {
            *pProperties = supportedExtensions.GetExtensionInfo(id);
            pProperties++;
            copyCount--;
        }
    }

    return result;
}

#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
// =====================================================================================================================
VkResult PhysicalDevice::AcquireXlibDisplay(
    Display*        dpy,
    VkDisplayKHR    display)
{
    Pal::OsDisplayHandle hDisplay = dpy;
    Pal::IScreen* pScreens        = reinterpret_cast<Pal::IScreen*>(display);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    VkResult result = PalToVkResult(pScreens->AcquireScreenAccess(hDisplay,
                                                                  VkToPalWsiPlatform(VK_ICD_WSI_PLATFORM_XLIB)));
#else
    VkResult result = VK_INCOMPLETE;
#endif

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetRandROutputDisplay(
    Display*        dpy,
    uint32_t        randrOutput,
    VkDisplayKHR*   pDisplay)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    VkResult result       = VK_SUCCESS;
    Pal::IScreen* pScreen = nullptr;
    pScreen = VkInstance()->FindScreenFromRandrOutput(PalDevice(), randrOutput);

    if (pScreen == nullptr)
    {
        Pal::OsDisplayHandle hDisplay = dpy;
        uint32_t connectorId          = UINT32_MAX;

        result = PalToVkResult(m_pPalDevice->GetConnectorIdFromOutput(hDisplay,
                                                                      randrOutput,
                                                                      VkToPalWsiPlatform(VK_ICD_WSI_PLATFORM_XLIB),
                                                                      &connectorId));
        if (result == VK_SUCCESS)
        {
            pScreen = VkInstance()->FindScreenFromConnectorId(PalDevice(), connectorId);

            if ((pScreen != nullptr))
            {
                pScreen->SetRandrOutput(randrOutput);
            }
            else
            {
                result = VK_INCOMPLETE;
            }
        }
    }

    *pDisplay = reinterpret_cast<VkDisplayKHR>(pScreen);
#else
    VkResult result = VK_INCOMPLETE;
#endif
    return result;
}
#endif

// =====================================================================================================================
VkResult PhysicalDevice::ReleaseDisplay(
    VkDisplayKHR display)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    Pal::IScreen* pScreen = reinterpret_cast<Pal::IScreen*>(display);
    VkResult result = PalToVkResult(pScreen->ReleaseScreenAccess());
#else
    VkResult result = VK_INCOMPLETE;
#endif
    return result;
}

// =====================================================================================================================

// =====================================================================================================================
// Retrieving the UUID of device/driver as well as the LUID if it is for windows platform.
// - DeviceUUID
//   domain:bus:device:function is enough to identify the pci device even for gemini or vf.
//   The current interface did not provide the domain so we just use bdf to compose the DeviceUUID.
// - DriverUUID
//   the timestamp of the icd plus maybe the palVersion sounds like a way to identify the driver.
// - DriverLUID
//   it is used on Windows only. If the LUID is valid, the deviceLUID can be casted to LUID object and must equal to the
//   locally unique identifier of a IDXGIAdapter1 object that corresponding to physicalDevice.
// It seems better to call into Pal to get those information filled since it might be OS specific.
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
void  PhysicalDevice::GetPhysicalDeviceIDProperties(
    uint8_t*            pDeviceUUID,
    uint8_t*            pDriverUUID,
    uint8_t*            pDeviceLUID,
    uint32_t*           pDeviceNodeMask,
    VkBool32*           pDeviceLUIDValid
    ) const
{
    const Pal::DeviceProperties& props = PalProperties();

    uint32_t* pBusNumber        = reinterpret_cast<uint32_t *>(pDeviceUUID);
    uint32_t* pDeviceNumber     = reinterpret_cast<uint32_t *>(pDeviceUUID+4);
    uint32_t* pFunctionNumber   = reinterpret_cast<uint32_t *>(pDeviceUUID+8);
    uint32_t* pPalMajor         = reinterpret_cast<uint32_t *>(pDriverUUID);
    uint32_t* pPalMinor         = reinterpret_cast<uint32_t *>(pDriverUUID+4);

    memset(pDeviceLUID, 0, VK_LUID_SIZE);
    memset(pDeviceUUID, 0, VK_UUID_SIZE);
    memset(pDriverUUID, 0, VK_UUID_SIZE);

    *pBusNumber      = props.pciProperties.busNumber;
    *pDeviceNumber   = props.pciProperties.deviceNumber;
    *pFunctionNumber = props.pciProperties.functionNumber;

    *pPalMajor  = PAL_INTERFACE_MAJOR_VERSION;
    *pPalMinor  = PAL_INTERFACE_MINOR_VERSION;

    *pDeviceNodeMask = (1u << props.gpuIndex);

    *pDeviceLUIDValid = VK_FALSE;
    uint32_t* pIcdTimeStamp = reinterpret_cast<uint32_t *>(pDriverUUID+8);

    Dl_info info = {};
    struct stat st = {};
    if (dladdr("vk_icdGetInstanceProcAddr", &info) || !info.dli_fname)
    {
        if (info.dli_fname && (stat(info.dli_fname, &st) == 0))
        {
            *pIcdTimeStamp = st.st_mtim.tv_sec;
        }
    }
}

// =====================================================================================================================
VkResult PhysicalDevice::GetExternalMemoryProperties(
    bool                                    isSparse,
    VkExternalMemoryHandleTypeFlagBits      handleType,
    VkExternalMemoryProperties*             pExternalMemoryProperties
) const
{
    VkResult result = VK_SUCCESS;

    // For windows, kmt and NT are mutually exclusive. You can only enable one type at creation time.
    pExternalMemoryProperties->compatibleHandleTypes         = handleType;
    pExternalMemoryProperties->exportFromImportedHandleTypes = handleType;
    pExternalMemoryProperties->externalMemoryFeatures        = 0;

    if (isSparse == false)
    {
        const Pal::DeviceProperties& props = PalProperties();
        if ((handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT))
        {
            pExternalMemoryProperties->externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT |
                                                                VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT     |
                                                                VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
        }
        else if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
        {
            pExternalMemoryProperties->externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
        }
        else if ((handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT) &&
                 props.gpuMemoryProperties.flags.supportHostMappedForeignMemory)
        {
            pExternalMemoryProperties->externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
        }
    }

    if (pExternalMemoryProperties->externalMemoryFeatures == 0)
    {
        // The handle type is not supported.
        pExternalMemoryProperties->compatibleHandleTypes         = 0;
        pExternalMemoryProperties->exportFromImportedHandleTypes = 0;

        result = VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    return result;
}

// =====================================================================================================================
// Retrieve device feature support. Called in response to vkGetPhysicalDeviceFeatures2
// NOTE: Don't memset here.  Otherwise, VerifyRequestedPhysicalDeviceFeatures needs to compare member by member
void PhysicalDevice::GetFeatures2(
    VkStructHeaderNonConst* pFeatures
    ) const
{
    VkStructHeaderNonConst* pHeader = pFeatures;

    while (pHeader)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
            {
                VkPhysicalDeviceFeatures2* pPhysicalDeviceFeatures2 =
                    reinterpret_cast<VkPhysicalDeviceFeatures2*>(pHeader);

                GetFeatures(&pPhysicalDeviceFeatures2->features);
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            {
                VkPhysicalDevice16BitStorageFeatures* pStorageFeatures =
                    reinterpret_cast<VkPhysicalDevice16BitStorageFeatures*>(pHeader);

                // We support 16-bit buffer load/store on all ASICs
                pStorageFeatures->storageBuffer16BitAccess           = VK_TRUE;
                pStorageFeatures->uniformAndStorageBuffer16BitAccess = VK_TRUE;

                // We don't plan to support 16-bit push constants
                pStorageFeatures->storagePushConstant16              = VK_FALSE;

                // Currently we seem to only support 16-bit inputs/outputs on ASICs supporting
                // 16-bit ALU. It's unclear at this point whether we can do any better.
                if (PalProperties().gfxipProperties.flags.support16BitInstructions &&
                    ((GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
                     (PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9)))
                {
                    pStorageFeatures->storageInputOutput16               = VK_TRUE;
                }
                else
                {
                    pStorageFeatures->storageInputOutput16               = VK_FALSE;
                }

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR:
            {
                VkPhysicalDevice8BitStorageFeaturesKHR* pStorageFeatures =
                    reinterpret_cast<VkPhysicalDevice8BitStorageFeaturesKHR*>(pHeader);
                pStorageFeatures->storageBuffer8BitAccess           = VK_TRUE;
                pStorageFeatures->uniformAndStorageBuffer8BitAccess = VK_TRUE;

                // We don't plan to support 8-bit push constants
                pStorageFeatures->storagePushConstant8              = VK_FALSE;

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_FEATURES_AMD:
            {
                VkPhysicalDeviceGpaFeaturesAMD* pGpaFeatures =
                    reinterpret_cast<VkPhysicalDeviceGpaFeaturesAMD*>(pHeader);

                pGpaFeatures->clockModes            = m_gpaProps.features.clockModes;
                pGpaFeatures->perfCounters          = m_gpaProps.features.perfCounters;
                pGpaFeatures->sqThreadTracing       = m_gpaProps.features.sqThreadTracing;
                pGpaFeatures->streamingPerfCounters = m_gpaProps.features.streamingPerfCounters;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            {
                VkPhysicalDeviceSamplerYcbcrConversionFeatures* pSamplerYcbcrConversionFeatures =
                    reinterpret_cast<VkPhysicalDeviceSamplerYcbcrConversionFeatures*>(pHeader);

                pSamplerYcbcrConversionFeatures->samplerYcbcrConversion = VK_FALSE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES:
            {
                VkPhysicalDeviceVariablePointerFeatures* pVariablePointerFeatures =
                    reinterpret_cast<VkPhysicalDeviceVariablePointerFeatures*>(pHeader);
                pVariablePointerFeatures->variablePointers = VK_TRUE;
                pVariablePointerFeatures->variablePointersStorageBuffer = VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            {
                VkPhysicalDeviceProtectedMemoryFeatures* pProtectedMemory =
                    reinterpret_cast<VkPhysicalDeviceProtectedMemoryFeatures*>(pHeader);

                pProtectedMemory->protectedMemory = VK_FALSE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            {
                VkPhysicalDeviceMultiviewFeatures* pMultiviewFeatures =
                    reinterpret_cast<VkPhysicalDeviceMultiviewFeatures*>(pHeader);

                pMultiviewFeatures->multiview                   = VK_TRUE;
                pMultiviewFeatures->multiviewGeometryShader     = VK_FALSE;
                pMultiviewFeatures->multiviewTessellationShader = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES:
            {
                VkPhysicalDeviceShaderDrawParameterFeatures* pShaderDrawParameterFeatures =
                    reinterpret_cast<VkPhysicalDeviceShaderDrawParameterFeatures*>(pHeader);

                pShaderDrawParameterFeatures->shaderDrawParameters = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT:
            {
                VkPhysicalDeviceDescriptorIndexingFeaturesEXT * pDescIndexingFeatures =
                    reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT *>(pHeader);

                pDescIndexingFeatures->shaderInputAttachmentArrayDynamicIndexing           = VK_FALSE;
                pDescIndexingFeatures->shaderUniformTexelBufferArrayDynamicIndexing        = VK_TRUE;
                pDescIndexingFeatures->shaderStorageTexelBufferArrayDynamicIndexing        = VK_TRUE;
                pDescIndexingFeatures->shaderUniformBufferArrayNonUniformIndexing          = VK_FALSE;
                pDescIndexingFeatures->shaderSampledImageArrayNonUniformIndexing           = VK_FALSE;
                pDescIndexingFeatures->shaderStorageBufferArrayNonUniformIndexing          = VK_FALSE;
                pDescIndexingFeatures->shaderStorageImageArrayNonUniformIndexing           = VK_FALSE;
                pDescIndexingFeatures->shaderInputAttachmentArrayNonUniformIndexing        = VK_FALSE;
                pDescIndexingFeatures->shaderUniformTexelBufferArrayNonUniformIndexing     = VK_FALSE;
                pDescIndexingFeatures->shaderStorageTexelBufferArrayNonUniformIndexing     = VK_FALSE;
                pDescIndexingFeatures->descriptorBindingUniformBufferUpdateAfterBind       = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingSampledImageUpdateAfterBind        = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingStorageImageUpdateAfterBind        = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingStorageBufferUpdateAfterBind       = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingUniformTexelBufferUpdateAfterBind  = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingStorageTexelBufferUpdateAfterBind  = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingUpdateUnusedWhilePending           = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingPartiallyBound                     = VK_TRUE;
                pDescIndexingFeatures->descriptorBindingVariableDescriptorCount            = VK_TRUE;
                pDescIndexingFeatures->runtimeDescriptorArray                              = VK_TRUE;

                break;
            }

            default:
            {
                // skip any unsupported extension structures
                break;
            }
        }

        pHeader = reinterpret_cast<VkStructHeaderNonConst*>(pHeader->pNext);
    }
}

// =====================================================================================================================
VkResult PhysicalDevice::GetImageFormatProperties2(
    const VkPhysicalDeviceImageFormatInfo2*     pImageFormatInfo,
    VkImageFormatProperties2*                   pImageFormatProperties)
{
    VkResult result = VK_SUCCESS;
    VK_ASSERT(pImageFormatInfo->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2);

    result = GetImageFormatProperties(
                 pImageFormatInfo->format,
                 pImageFormatInfo->type,
                 pImageFormatInfo->tiling,
                 pImageFormatInfo->usage,
                 pImageFormatInfo->flags,
                 &pImageFormatProperties->imageFormatProperties);

    if (result == VK_SUCCESS)
    {
        union
        {
            const VkStructHeader*                                   pHeader;
            const VkPhysicalDeviceImageFormatInfo2*                 pFormatInfo;
            const VkPhysicalDeviceExternalImageFormatInfo*          pExternalImageFormatInfo;
        };

        union
        {
            const VkStructHeader*                                   pHeader2;
            VkImageFormatProperties2*                               pImageFormatProps;
            VkExternalImageFormatProperties*                        pExternalImageProperties;
            VkTextureLODGatherFormatPropertiesAMD*                  pTextureLODGatherFormatProperties;
        };

        pFormatInfo = pImageFormatInfo;
        pHeader = utils::GetExtensionStructure(pHeader,
                                               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO);

        if (pHeader)
        {
            // decide the supported handle type for the specific image info.
            VK_ASSERT(pExternalImageFormatInfo->handleType != 0);
            pImageFormatProps = pImageFormatProperties;
            pHeader2 = utils::GetExtensionStructure(
                                  pHeader2,
                                  VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);

            if (pExternalImageProperties)
            {
                result = GetExternalMemoryProperties(
                             ((pImageFormatInfo->flags & Image::SparseEnablingFlags) != 0),
                             pExternalImageFormatInfo->handleType,
                             &pExternalImageProperties->externalMemoryProperties);
            }
        }

        // handle VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD
        pFormatInfo = pImageFormatInfo;
        pHeader = utils::GetExtensionStructure(pHeader, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2);

        if (pHeader)
        {
            pImageFormatProps = pImageFormatProperties;
            pHeader2 = utils::GetExtensionStructure(
                pHeader2,
                VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD);

            if (pTextureLODGatherFormatProperties)
            {
                const VkFormat& format = pImageFormatInfo->format;

                if (PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9)
                {
                    pTextureLODGatherFormatProperties->supportsTextureGatherLODBiasAMD = true;
                }
                else
                {
                    const auto formatType = vk::Formats::GetNumberFormat(format);
                    const bool isInteger  = (formatType == Pal::Formats::NumericSupportFlags::Sint) ||
                                            (formatType == Pal::Formats::NumericSupportFlags::Uint);

                    pTextureLODGatherFormatProperties->supportsTextureGatherLODBiasAMD = !isInteger;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
void PhysicalDevice::GetDeviceProperties2(
    VkPhysicalDeviceProperties2* pProperties)
{
    VK_ASSERT(pProperties->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);

    GetDeviceProperties(&pProperties->properties);

    union
    {
        const VkStructHeader*                                    pHeader;
        VkPhysicalDeviceProperties2*                             pProp;
        VkPhysicalDevicePointClippingProperties*                 pPointClippingProperties;
        VkPhysicalDeviceIDProperties*                            pIDProperties;
        VkPhysicalDeviceSampleLocationsPropertiesEXT*            pSampleLocationsPropertiesEXT;
        VkPhysicalDeviceGpaPropertiesAMD*                        pGpaProperties;
        VkPhysicalDeviceMaintenance3Properties*                  pMaintenance3Properties;
        VkPhysicalDeviceMultiviewProperties*                     pMultiviewProperties;
        VkPhysicalDeviceProtectedMemoryProperties*               pProtectedMemoryProperties;
        VkPhysicalDeviceSubgroupProperties*                      pSubgroupProperties;
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT*         pExternalMemoryHostProperties;
        VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT*        pMinMaxProperties;
        VkPhysicalDeviceShaderCorePropertiesAMD*                 pShaderCoreProperties;
        VkPhysicalDeviceDescriptorIndexingPropertiesEXT*         pDescriptorIndexingProperties;

        VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT*     pVertexAttributeDivisorProperties;
    };

    for (pProp = pProperties; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
        {
            // Points are clipped when their centers fall outside the clip volume, i.e. the desktop GL behavior.
            pPointClippingProperties->pointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
        {
            GetPhysicalDeviceIDProperties(
                &pIDProperties->deviceUUID[0],
                &pIDProperties->driverUUID[0],
                &pIDProperties->deviceLUID[0],
                &pIDProperties->deviceNodeMask,
                &pIDProperties->deviceLUIDValid);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT:
        {
            pSampleLocationsPropertiesEXT->sampleLocationSampleCounts       = m_sampleLocationSampleCounts;
            pSampleLocationsPropertiesEXT->maxSampleLocationGridSize.width  = Pal::MaxGridSize.width;
            pSampleLocationsPropertiesEXT->maxSampleLocationGridSize.height = Pal::MaxGridSize.height;
            pSampleLocationsPropertiesEXT->sampleLocationCoordinateRange[0] = 0.0f;
            pSampleLocationsPropertiesEXT->sampleLocationCoordinateRange[1] = 1.0f;
            pSampleLocationsPropertiesEXT->sampleLocationSubPixelBits       = Pal::SubPixelBits;
            pSampleLocationsPropertiesEXT->variableSampleLocations          = VK_TRUE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_PROPERTIES_AMD:
        {
            GetDeviceGpaProperties(pGpaProperties);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
        {
            // We don't have limits on number of desc sets
            pMaintenance3Properties->maxPerSetDescriptors    = UINT32_MAX;

            // TODO: SWDEV-79454 - Get these limits from PAL
            // Return 2GB in bytes as max allocation size
            pMaintenance3Properties->maxMemoryAllocationSize = 2u * 1024u * 1024u * 1024u;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
        {
            pProtectedMemoryProperties->protectedNoFault = VK_FALSE;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
        {
            pMultiviewProperties->maxMultiviewViewCount     = Pal::MaxViewInstanceCount;
            pMultiviewProperties->maxMultiviewInstanceIndex = UINT_MAX;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
        {
            pSubgroupProperties->subgroupSize        = GetSubgroupSize();

            pSubgroupProperties->supportedStages     = VK_SHADER_STAGE_VERTEX_BIT |
                                                       VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                                       VK_SHADER_STAGE_GEOMETRY_BIT |
                                                       VK_SHADER_STAGE_FRAGMENT_BIT |
                                                       VK_SHADER_STAGE_COMPUTE_BIT;

            pSubgroupProperties->supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT |
                                                       VK_SUBGROUP_FEATURE_VOTE_BIT |
                                                       VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
                                                       VK_SUBGROUP_FEATURE_BALLOT_BIT |
                                                       VK_SUBGROUP_FEATURE_SHUFFLE_BIT |
                                                       VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
                                                       VK_SUBGROUP_FEATURE_QUAD_BIT;

            pSubgroupProperties->quadOperationsInAllStages = VK_TRUE;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT:
        {
            pMinMaxProperties->filterMinmaxImageComponentMapping  = IsPerChannelMinMaxFilteringSupported();
            pMinMaxProperties->filterMinmaxSingleComponentFormats = VK_TRUE;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT:
        {
            const Pal::DeviceProperties& palProps = PalProperties();

            pExternalMemoryHostProperties->minImportedHostPointerAlignment =
                palProps.gpuMemoryProperties.realMemAllocGranularity;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD:
        {
            const Pal::DeviceProperties& props = PalProperties();

            pShaderCoreProperties->shaderEngineCount = props.gfxipProperties.shaderCore.numShaderEngines;
            pShaderCoreProperties->shaderArraysPerEngineCount = props.gfxipProperties.shaderCore.numShaderArrays;
            pShaderCoreProperties->computeUnitsPerShaderArray = props.gfxipProperties.shaderCore.numCusPerShaderArray;
            pShaderCoreProperties->simdPerComputeUnit = props.gfxipProperties.shaderCore.numSimdsPerCu;
            pShaderCoreProperties->wavefrontsPerSimd = props.gfxipProperties.shaderCore.numWavefrontsPerSimd;
            pShaderCoreProperties->wavefrontSize = props.gfxipProperties.shaderCore.wavefrontSize;

            // Scalar General Purpose Registers (SGPR)
            pShaderCoreProperties->sgprsPerSimd = props.gfxipProperties.shaderCore.sgprsPerSimd;
            pShaderCoreProperties->minSgprAllocation = props.gfxipProperties.shaderCore.minSgprAlloc;
            pShaderCoreProperties->maxSgprAllocation = props.gfxipProperties.shaderCore.numAvailableSgprs;
            pShaderCoreProperties->sgprAllocationGranularity = props.gfxipProperties.shaderCore.sgprAllocGranularity;

            // Vector General Purpose Registers (VGPR)
            pShaderCoreProperties->vgprsPerSimd = props.gfxipProperties.shaderCore.vgprsPerSimd;
            pShaderCoreProperties->minVgprAllocation = props.gfxipProperties.shaderCore.minVgprAlloc;
            pShaderCoreProperties->maxVgprAllocation = props.gfxipProperties.shaderCore.numAvailableVgprs;
            pShaderCoreProperties->vgprAllocationGranularity = props.gfxipProperties.shaderCore.vgprAllocGranularity;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT:
        {
            pDescriptorIndexingProperties->maxUpdateAfterBindDescriptorsInAllPools                  = UINT32_MAX;
            pDescriptorIndexingProperties->shaderUniformBufferArrayNonUniformIndexingNative         = VK_FALSE;
            pDescriptorIndexingProperties->shaderSampledImageArrayNonUniformIndexingNative          = VK_FALSE;
            pDescriptorIndexingProperties->shaderStorageBufferArrayNonUniformIndexingNative         = VK_FALSE;
            pDescriptorIndexingProperties->shaderStorageImageArrayNonUniformIndexingNative          = VK_FALSE;
            pDescriptorIndexingProperties->shaderInputAttachmentArrayNonUniformIndexingNative       = VK_FALSE;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindSamplers             = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindUniformBuffers       = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindStorageBuffers       = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindSampledImages        = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindStorageImages        = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindInputAttachments     = UINT32_MAX;
            pDescriptorIndexingProperties->maxPerStageUpdateAfterBindResources                      = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindSamplers                  = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindUniformBuffers            = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic     = MaxDynamicUniformDescriptors;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageBuffers            = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic     = MaxDynamicStorageDescriptors;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindSampledImages             = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageImages             = UINT32_MAX;
            pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindInputAttachments          = UINT32_MAX;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT:
        {
            pVertexAttributeDivisorProperties->maxVertexAttribDivisor = UINT32_MAX;
            break;
        }
        default:
            break;
        }
    }
}

// =====================================================================================================================
void PhysicalDevice::GetFormatProperties2(
    VkFormat                                    format,
    VkFormatProperties2*                        pFormatProperties)
{
    VK_ASSERT(pFormatProperties->sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
    GetFormatProperties(format, &pFormatProperties->formatProperties);
}

// =====================================================================================================================
void PhysicalDevice::GetMemoryProperties2(
    VkPhysicalDeviceMemoryProperties2*          pMemoryProperties)
{
    VK_ASSERT(pMemoryProperties->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
    pMemoryProperties->memoryProperties = GetMemoryProperties();
}

// =====================================================================================================================
void PhysicalDevice::GetSparseImageFormatProperties2(
    const VkPhysicalDeviceSparseImageFormatInfo2*       pFormatInfo,
    uint32_t*                                           pPropertyCount,
    VkSparseImageFormatProperties2*                     pProperties)
{
    VK_ASSERT(pFormatInfo->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2);

        GetSparseImageFormatProperties(
            pFormatInfo->format,
            pFormatInfo->type,
            pFormatInfo->samples,
            pFormatInfo->usage,
            pFormatInfo->tiling,
            pPropertyCount,
            utils::ArrayView<VkSparseImageFormatProperties>(pProperties, &pProperties->properties));
}

// =====================================================================================================================
void PhysicalDevice::GetDeviceMultisampleProperties(
    VkSampleCountFlagBits                       samples,
    VkMultisamplePropertiesEXT*                 pMultisampleProperties)
{
    if ((samples & m_sampleLocationSampleCounts) != 0)
    {
        pMultisampleProperties->maxSampleLocationGridSize.width  = Pal::MaxGridSize.width;
        pMultisampleProperties->maxSampleLocationGridSize.height = Pal::MaxGridSize.height;
    }
    else
    {
        pMultisampleProperties->maxSampleLocationGridSize.width  = 0;
        pMultisampleProperties->maxSampleLocationGridSize.height = 0;
    }
}

// =====================================================================================================================
void PhysicalDevice::GetExternalBufferProperties(
    const VkPhysicalDeviceExternalBufferInfo*       pExternalBufferInfo,
    VkExternalBufferProperties*                     pExternalBufferProperties)
{
    VK_ASSERT(pExternalBufferInfo->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO);

    GetExternalMemoryProperties(((pExternalBufferInfo->flags & Buffer::SparseEnablingFlags) != 0),
                                pExternalBufferInfo->handleType,
                                &pExternalBufferProperties->externalMemoryProperties);
}

// =====================================================================================================================
void PhysicalDevice::GetExternalSemaphoreProperties(
    const VkPhysicalDeviceExternalSemaphoreInfo*    pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties*                  pExternalSemaphoreProperties)
{
    VK_ASSERT(pExternalSemaphoreInfo->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO);

    // For windows, kmt and NT are mutually exclusive. You can only enable one type at creation time.
    pExternalSemaphoreProperties->compatibleHandleTypes         = pExternalSemaphoreInfo->handleType;
    pExternalSemaphoreProperties->exportFromImportedHandleTypes = pExternalSemaphoreInfo->handleType;
    pExternalSemaphoreProperties->externalSemaphoreFeatures     = 0;
    const Pal::DeviceProperties& props                          = PalProperties();

    if (IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_SEMAPHORE_FD))
    {
        if (pExternalSemaphoreInfo->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
        {
            pExternalSemaphoreProperties->externalSemaphoreFeatures = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT |
                                                                      VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
        }
        else if ((pExternalSemaphoreInfo->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT) &&
                 (props.osProperties.supportSyncFileSemaphore))
        {
            pExternalSemaphoreProperties->externalSemaphoreFeatures = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT |
                                                                      VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
        }
    }

    if (pExternalSemaphoreProperties->externalSemaphoreFeatures == 0)
    {
        // The handle type is not supported.
        pExternalSemaphoreProperties->compatibleHandleTypes         = 0;
        pExternalSemaphoreProperties->exportFromImportedHandleTypes = 0;
    }
}

// =====================================================================================================================
void PhysicalDevice::GetExternalFenceProperties(
    const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo,
    VkExternalFenceProperties*               pExternalFenceProperties)
{
    VK_ASSERT(pExternalFenceInfo->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO);

    // For windows, kmt and NT are mutually exclusive. You can only enable one type at creation time.
    pExternalFenceProperties->compatibleHandleTypes         = pExternalFenceInfo->handleType;
    pExternalFenceProperties->exportFromImportedHandleTypes = pExternalFenceInfo->handleType;
    pExternalFenceProperties->externalFenceFeatures         = 0;
    const Pal::DeviceProperties& props                      = PalProperties();

    if (IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_FENCE_FD))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 398
        if ((pExternalFenceInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
            (pExternalFenceInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT))
        {
            if (props.osProperties.supportSyncFileFence)
            {
                pExternalFenceProperties->externalFenceFeatures = VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT |
                                                                  VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;
            }
        }
#endif
    }

    if (pExternalFenceProperties->externalFenceFeatures == 0)
    {
        // The handle type is not supported.
        pExternalFenceProperties->compatibleHandleTypes         = 0;
        pExternalFenceProperties->exportFromImportedHandleTypes = 0;
    }
}

// =====================================================================================================================
void PhysicalDevice::GetDeviceGpaProperties(
    VkPhysicalDeviceGpaPropertiesAMD* pGpaProperties
    ) const
{
    pGpaProperties->flags               = m_gpaProps.properties.flags;
    pGpaProperties->maxSqttSeBufferSize = m_gpaProps.properties.maxSqttSeBufferSize;
    pGpaProperties->shaderEngineCount   = m_gpaProps.properties.shaderEngineCount;

    if (pGpaProperties->pPerfBlocks == nullptr)
    {
        pGpaProperties->perfBlockCount = m_gpaProps.properties.perfBlockCount;
    }
    else
    {
        uint32_t count   = Util::Min(pGpaProperties->perfBlockCount, m_gpaProps.properties.perfBlockCount);
        uint32_t written = 0;

        for (uint32_t perfBlock = VK_GPA_PERF_BLOCK_BEGIN_RANGE_AMD;
                      (perfBlock <= VK_GPA_PERF_BLOCK_END_RANGE_AMD) && (written < count);
                      ++perfBlock)
        {
            const Pal::GpuBlock gpuBlock = VkToPalGpuBlock(static_cast<VkGpaPerfBlockAMD>(perfBlock));

            if (gpuBlock < Pal::GpuBlock::Count)
            {
                if (m_gpaProps.palProps.blocks[static_cast<uint32_t>(gpuBlock)].available)
                {
                    pGpaProperties->pPerfBlocks[written++] = ConvertGpaPerfBlock(
                        static_cast<VkGpaPerfBlockAMD>(perfBlock),
                        gpuBlock,
                        m_gpaProps.palProps.blocks[static_cast<uint32_t>(gpuBlock)]);
                }
            }
        }
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Verifies the given device conforms to the required Vulkan 1.0 min/max limits.
static void VerifyLimits(
    const PhysicalDevice&           device,
    const VkPhysicalDeviceLimits&   limits,
    const VkPhysicalDeviceFeatures& features)
{
    // These values are from Table 31.2 of the Vulkan 1.0 specification
    VK_ASSERT(limits.maxImageDimension1D                   >= 4096);
    VK_ASSERT(limits.maxImageDimension2D                   >= 4096);
    VK_ASSERT(limits.maxImageDimension3D                   >= 256);
    VK_ASSERT(limits.maxImageDimensionCube                 >= 4096);
    VK_ASSERT(limits.maxImageArrayLayers                   >= 256);
    VK_ASSERT(limits.maxTexelBufferElements                >= 65536);
    VK_ASSERT(limits.maxUniformBufferRange                 >= 16384);
    VK_ASSERT(limits.maxStorageBufferRange                 >= (1UL << 27));
    VK_ASSERT(limits.maxPushConstantsSize                  >= 128);
    VK_ASSERT(limits.maxMemoryAllocationCount              >= 4096);
    VK_ASSERT(limits.maxSamplerAllocationCount             >= 4000);
    VK_ASSERT(limits.bufferImageGranularity                <= 131072);
    VK_ASSERT(limits.sparseAddressSpaceSize                >= (features.sparseBinding ? (1ULL << 31) : 0));
    VK_ASSERT(limits.maxBoundDescriptorSets                >= 4);
    VK_ASSERT(limits.maxPerStageDescriptorSamplers         >= 16);
    VK_ASSERT(limits.maxPerStageDescriptorUniformBuffers   >= 12);
    VK_ASSERT(limits.maxPerStageDescriptorStorageBuffers   >= 4);
    VK_ASSERT(limits.maxPerStageDescriptorSampledImages    >= 16);
    VK_ASSERT(limits.maxPerStageDescriptorStorageImages    >= 4);
    VK_ASSERT(limits.maxPerStageDescriptorInputAttachments >= 4);

    const uint64_t reqMaxPerStageResources = Util::Min(
        static_cast<uint64_t>(limits.maxPerStageDescriptorUniformBuffers) +
        static_cast<uint64_t>(limits.maxPerStageDescriptorStorageBuffers) +
        static_cast<uint64_t>(limits.maxPerStageDescriptorSampledImages) +
        static_cast<uint64_t>(limits.maxPerStageDescriptorStorageImages) +
        static_cast<uint64_t>(limits.maxPerStageDescriptorInputAttachments) +
        static_cast<uint64_t>(limits.maxColorAttachments),
        static_cast<uint64_t>(128));

    VK_ASSERT(limits.maxPerStageResources                  >= reqMaxPerStageResources);
    VK_ASSERT(limits.maxDescriptorSetSamplers              >= 96);
    VK_ASSERT(limits.maxDescriptorSetUniformBuffers        >= 72);
    VK_ASSERT(limits.maxDescriptorSetUniformBuffersDynamic >= 8);
    VK_ASSERT(limits.maxDescriptorSetStorageBuffers        >= 24);
    VK_ASSERT(limits.maxDescriptorSetStorageBuffersDynamic >= 4);
    VK_ASSERT(limits.maxDescriptorSetSampledImages         >= 96);
    VK_ASSERT(limits.maxDescriptorSetStorageImages         >= 24);
    VK_ASSERT(limits.maxDescriptorSetInputAttachments      >= 4);
    VK_ASSERT(limits.maxVertexInputAttributes              >= 16);
    VK_ASSERT(limits.maxVertexInputBindings                >= 16);
    VK_ASSERT(limits.maxVertexInputAttributeOffset         >= 2047);
    VK_ASSERT(limits.maxVertexInputBindingStride           >= 2048);
    VK_ASSERT(limits.maxVertexOutputComponents             >= 64);

    VK_ASSERT(features.tessellationShader);

    if (features.tessellationShader)
    {
        VK_ASSERT(limits.maxTessellationGenerationLevel                  >= 64);
        VK_ASSERT(limits.maxTessellationPatchSize                        >= 32);
        VK_ASSERT(limits.maxTessellationControlPerVertexInputComponents  >= 64);
        VK_ASSERT(limits.maxTessellationControlPerVertexOutputComponents >= 64);
        VK_ASSERT(limits.maxTessellationControlPerPatchOutputComponents  >= 120);
        VK_ASSERT(limits.maxTessellationControlTotalOutputComponents     >= 2048);
        VK_ASSERT(limits.maxTessellationEvaluationInputComponents        >= 64);
        VK_ASSERT(limits.maxTessellationEvaluationOutputComponents       >= 64);
    }
    else
    {
        VK_ASSERT(limits.maxTessellationGenerationLevel                  == 0);
        VK_ASSERT(limits.maxTessellationPatchSize                        == 0);
        VK_ASSERT(limits.maxTessellationControlPerVertexInputComponents  == 0);
        VK_ASSERT(limits.maxTessellationControlPerVertexOutputComponents == 0);
        VK_ASSERT(limits.maxTessellationControlPerPatchOutputComponents  == 0);
        VK_ASSERT(limits.maxTessellationControlTotalOutputComponents     == 0);
        VK_ASSERT(limits.maxTessellationEvaluationInputComponents        == 0);
        VK_ASSERT(limits.maxTessellationEvaluationOutputComponents       == 0);
    }

    VK_ASSERT(features.geometryShader);

    if (features.geometryShader)
    {
        VK_ASSERT(limits.maxGeometryShaderInvocations     >= 32);
        VK_ASSERT(limits.maxGeometryInputComponents       >= 64);
        VK_ASSERT(limits.maxGeometryOutputComponents      >= 64);
        VK_ASSERT(limits.maxGeometryOutputVertices        >= 256);
        VK_ASSERT(limits.maxGeometryTotalOutputComponents >= 1024);
        VK_ASSERT(limits.maxGeometryTotalOutputComponents >= 1024);
    }
    else
    {
        VK_ASSERT(limits.maxGeometryShaderInvocations     == 0);
        VK_ASSERT(limits.maxGeometryInputComponents       == 0);
        VK_ASSERT(limits.maxGeometryOutputComponents      == 0);
        VK_ASSERT(limits.maxGeometryOutputVertices        == 0);
        VK_ASSERT(limits.maxGeometryTotalOutputComponents == 0);
        VK_ASSERT(limits.maxGeometryTotalOutputComponents == 0);
    }

    VK_ASSERT(limits.maxFragmentInputComponents   >= 64);
    VK_ASSERT(limits.maxFragmentOutputAttachments >= 4);

    if (features.dualSrcBlend)
    {
        VK_ASSERT(limits.maxFragmentDualSrcAttachments >= 1);
    }
    else
    {
        VK_ASSERT(limits.maxFragmentDualSrcAttachments == 0);
    }

    VK_ASSERT(limits.maxFragmentCombinedOutputResources >= 4);
    VK_ASSERT(limits.maxComputeSharedMemorySize         >= 16384);
    VK_ASSERT(limits.maxComputeWorkGroupCount[0]        >= 65535);
    VK_ASSERT(limits.maxComputeWorkGroupCount[1]        >= 65535);
    VK_ASSERT(limits.maxComputeWorkGroupCount[2]        >= 65535);
    VK_ASSERT(limits.maxComputeWorkGroupInvocations     >= 128);
    VK_ASSERT(limits.maxComputeWorkGroupSize[0]         >= 128);
    VK_ASSERT(limits.maxComputeWorkGroupSize[1]         >= 128);
    VK_ASSERT(limits.maxComputeWorkGroupSize[2]         >= 64);
    VK_ASSERT(limits.subPixelPrecisionBits              >= 4);
    VK_ASSERT(limits.subTexelPrecisionBits              >= 4);
    VK_ASSERT(limits.mipmapPrecisionBits                >= 4);

    VK_ASSERT(features.fullDrawIndexUint32);

    if (features.fullDrawIndexUint32)
    {
        VK_ASSERT(limits.maxDrawIndexedIndexValue >= 0xffffffff);
    }
    else
    {
        VK_ASSERT(limits.maxDrawIndexedIndexValue >= ((1UL << 24) - 1));
    }

    if (features.multiDrawIndirect)
    {
        VK_ASSERT(limits.maxDrawIndirectCount >= ((1UL << 16) - 1));
    }
    else
    {
        VK_ASSERT(limits.maxDrawIndirectCount == 1);
    }

    VK_ASSERT(limits.maxSamplerLodBias >= 2);

    VK_ASSERT(features.samplerAnisotropy);

    if (features.samplerAnisotropy)
    {
        VK_ASSERT(limits.maxSamplerAnisotropy >= 16);
    }
    else
    {
        VK_ASSERT(limits.maxSamplerAnisotropy == 1);
    }

    VK_ASSERT(features.multiViewport);

    if (features.multiViewport)
    {
        VK_ASSERT(limits.maxViewports >= 16);
    }
    else
    {
        VK_ASSERT(limits.maxViewports == 1);
    }

    VK_ASSERT(limits.maxViewportDimensions[0]        >= 4096);
    VK_ASSERT(limits.maxViewportDimensions[1]        >= 4096);
    VK_ASSERT(limits.maxViewportDimensions[0]        >= limits.maxFramebufferWidth);
    VK_ASSERT(limits.maxViewportDimensions[1]        >= limits.maxFramebufferHeight);
    VK_ASSERT(limits.viewportBoundsRange[0]          <= -8192);
    VK_ASSERT(limits.viewportBoundsRange[1]          >= 8191);
    VK_ASSERT(limits.viewportBoundsRange[0]          <= -2 * limits.maxViewportDimensions[0]);
    VK_ASSERT(limits.viewportBoundsRange[1]          >=  2 * limits.maxViewportDimensions[0] - 1);
    VK_ASSERT(limits.viewportBoundsRange[0]          <= -2 * limits.maxViewportDimensions[1]);
    VK_ASSERT(limits.viewportBoundsRange[1]          >=  2 * limits.maxViewportDimensions[1] - 1);
    /* Always true: VK_ASSERT(limits.viewportSubPixelBits            >= 0); */
    VK_ASSERT(limits.minMemoryMapAlignment           >= 64);
    VK_ASSERT(limits.minTexelBufferOffsetAlignment   <= 256);
    VK_ASSERT(limits.minUniformBufferOffsetAlignment <= 256);
    VK_ASSERT(limits.minStorageBufferOffsetAlignment <= 256);
    VK_ASSERT(limits.minTexelOffset                  <= -8);
    VK_ASSERT(limits.maxTexelOffset                  >= 7);

    VK_ASSERT(features.shaderImageGatherExtended);

    if (features.shaderImageGatherExtended)
    {
        VK_ASSERT(limits.minTexelGatherOffset <= -8);
        VK_ASSERT(limits.maxTexelGatherOffset >=  7);
    }
    else
    {
        VK_ASSERT(limits.minTexelGatherOffset == 0);
        VK_ASSERT(limits.maxTexelGatherOffset == 0);
    }

    VK_ASSERT(features.sampleRateShading);

    if (features.sampleRateShading)
    {
        const float ULP = 1.0f / (1UL << limits.subPixelInterpolationOffsetBits);

        VK_ASSERT(limits.minInterpolationOffset          <= -0.5f);
        VK_ASSERT(limits.maxInterpolationOffset          >=  0.5f - ULP);
        VK_ASSERT(limits.subPixelInterpolationOffsetBits >= 4);
    }
    else
    {
        VK_ASSERT(limits.minInterpolationOffset          == 0.0f);
        VK_ASSERT(limits.maxInterpolationOffset          == 0.0f);
        VK_ASSERT(limits.subPixelInterpolationOffsetBits == 0);
    }

    VK_ASSERT(limits.maxFramebufferWidth                  >= 4096);
    VK_ASSERT(limits.maxFramebufferHeight                 >= 4096);
    VK_ASSERT(limits.maxFramebufferLayers                 >= 256);
    VK_ASSERT(limits.framebufferColorSampleCounts         & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.framebufferColorSampleCounts         & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.framebufferDepthSampleCounts         & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.framebufferDepthSampleCounts         & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.framebufferStencilSampleCounts       & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.framebufferStencilSampleCounts       & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.maxColorAttachments                  >= 4);
    VK_ASSERT(limits.sampledImageColorSampleCounts        & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.sampledImageColorSampleCounts        & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.sampledImageIntegerSampleCounts      & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.sampledImageDepthSampleCounts        & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.sampledImageDepthSampleCounts        & VK_SAMPLE_COUNT_4_BIT);
    VK_ASSERT(limits.sampledImageStencilSampleCounts      & VK_SAMPLE_COUNT_1_BIT);
    VK_ASSERT(limits.sampledImageStencilSampleCounts      & VK_SAMPLE_COUNT_4_BIT);

    VK_ASSERT(features.shaderStorageImageMultisample);

    if (features.shaderStorageImageMultisample)
    {
        VK_ASSERT(limits.storageImageSampleCounts & VK_SAMPLE_COUNT_1_BIT);
        VK_ASSERT(limits.storageImageSampleCounts & VK_SAMPLE_COUNT_4_BIT);
    }
    else
    {
        VK_ASSERT(limits.storageImageSampleCounts == VK_SAMPLE_COUNT_1_BIT);
    }

    VK_ASSERT(limits.maxSampleMaskWords >= 1);

    VK_ASSERT(features.shaderClipDistance);

    if (features.shaderClipDistance)
    {
        VK_ASSERT(limits.maxClipDistances >= 8);
    }
    else
    {
        VK_ASSERT(limits.maxClipDistances == 0);
    }

    VK_ASSERT(features.shaderCullDistance);

    if (features.shaderCullDistance)
    {
        VK_ASSERT(limits.maxCullDistances                >= 8);
        VK_ASSERT(limits.maxCombinedClipAndCullDistances >= 8);
    }
    else
    {
        VK_ASSERT(limits.maxCullDistances                == 0);
        VK_ASSERT(limits.maxCombinedClipAndCullDistances == 0);
    }

    VK_ASSERT(limits.discreteQueuePriorities >= 2);

    VK_ASSERT(features.largePoints);

    if (features.largePoints)
    {
        const float ULP = limits.pointSizeGranularity;

        VK_ASSERT(limits.pointSizeRange[0] <= 1.0f);
        VK_ASSERT(limits.pointSizeRange[1] >= 64.0f - limits.pointSizeGranularity);
    }
    else
    {
        VK_ASSERT(limits.pointSizeRange[0] == 1.0f);
        VK_ASSERT(limits.pointSizeRange[1] == 1.0f);
    }

    VK_ASSERT(features.wideLines);

    if (features.wideLines)
    {
        const float ULP = limits.lineWidthGranularity;

        VK_ASSERT(limits.lineWidthRange[0] <= 1.0f);
        VK_ASSERT(limits.lineWidthRange[1] >= 8.0f - ULP);
    }
    else
    {
        VK_ASSERT(limits.lineWidthRange[0] == 0.0f);
        VK_ASSERT(limits.lineWidthRange[1] == 1.0f);
    }

    if (features.largePoints)
    {
        VK_ASSERT(limits.pointSizeGranularity <= 1.0f);
    }
    else
    {
        VK_ASSERT(limits.pointSizeGranularity == 0.0f);
    }

    if (features.wideLines)
    {
        VK_ASSERT(limits.lineWidthGranularity <= 1.0f);
    }
    else
    {
        VK_ASSERT(limits.lineWidthGranularity == 0.0f);
    }

    VK_ASSERT(limits.nonCoherentAtomSize >= 128);
}

// =====================================================================================================================
// Verifies the given device conforms to the required Vulkan 1.0 required format support.
static void VerifyRequiredFormats(
    const PhysicalDevice&           dev,
    const VkPhysicalDeviceFeatures& features)
{
    // Go through every format and require nothing.  This will still sanity check some other state to make sure the
    // values make sense
    for (uint32_t formatIdx = VK_FORMAT_BEGIN_RANGE; formatIdx <= VK_FORMAT_END_RANGE; ++formatIdx)
    {
        const VkFormat format = static_cast<VkFormat>(formatIdx);

        if (format != VK_FORMAT_UNDEFINED)
        {
            VK_ASSERT(VerifyFormatSupport(dev, format, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
        }
    }

    // Table 30.13. Mandatory format support: sub-byte channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_B4G4R4A4_UNORM_PACK16,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R5G6B5_UNORM_PACK16,      1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A1R5G5B5_UNORM_PACK16,    1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0));

    // Table 30.14. Mandatory format support: 1-3 byte sized channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8_UNORM,                 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8_SNORM,                 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8_UINT,                  1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8_SINT,                  1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8_UNORM,               1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8_SNORM,               1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8_UINT,                1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8_SINT,                1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));

    // Table 30.15. Mandatory format support: 4 byte-sized channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8B8A8_UNORM,           1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8B8A8_SNORM,           1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8B8A8_UINT,            1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8B8A8_SINT,            1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R8G8B8A8_SRGB,            1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_B8G8R8A8_UNORM,           1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_B8G8R8A8_SRGB,            1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A8B8G8R8_UNORM_PACK32,    1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A8B8G8R8_SNORM_PACK32,    1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A8B8G8R8_UINT_PACK32,     1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A8B8G8R8_SINT_PACK32,     1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A8B8G8R8_SRGB_PACK32,     1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0));

    // Table 30.16. Mandatory format support: 10-bit channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_A2B10G10R10_UINT_PACK32,  1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0));

    // Table 30.17. Mandatory format support: 16-bit channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16_UNORM,                0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16_SNORM,                0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16_UINT,                 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16_SINT,                 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16_SFLOAT,               1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16_UNORM,             0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16_SNORM,             0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16_UINT,              1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16_SINT,              1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16_SFLOAT,            1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16B16A16_UNORM,       0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16B16A16_SNORM,       0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16B16A16_UINT,        1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16B16A16_SINT,        1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R16G16B16A16_SFLOAT,      1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0));

    // Table 30.18. Mandatory format support: 32-bit channels
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32_UINT,                 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32_SINT,                 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32_SFLOAT,               1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32_UINT,              1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32_SINT,              1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32_SFLOAT,            1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32_UINT,           0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32_SINT,           0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32_SFLOAT,         0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32A32_UINT,        1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32A32_SINT,        1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_R32G32B32A32_SFLOAT,      1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0));

    // Table 30.19. Mandatory format support: 64-bit/uneven channels and depth/stencil
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_B10G11R11_UFLOAT_PACK32,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_D16_UNORM,                1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_X8_D24_UNORM_PACK32,      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0) ||
              VerifyFormatSupport(dev, VK_FORMAT_D32_SFLOAT,               0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_D32_SFLOAT,               1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    VK_ASSERT(VerifyFormatSupport(dev, VK_FORMAT_D24_UNORM_S8_UINT,        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0) ||
              VerifyFormatSupport(dev, VK_FORMAT_D32_SFLOAT_S8_UINT,       0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0));

    // Table 30.20.
    VK_ASSERT(VerifyBCFormatSupport(dev) || (features.textureCompressionBC == VK_FALSE));

    // Table 30.20. Mandatory support of at least one texture compression scheme (BC, ETC2, or ASTC)
    VK_ASSERT(features.textureCompressionBC);
}

// =====================================================================================================================
static void VerifyProperties(
    const PhysicalDevice& device)
{
    const VkPhysicalDeviceLimits limits = device.GetLimits();

    VkPhysicalDeviceFeatures features = {};

    VK_ASSERT(device.GetFeatures(&features) == VK_SUCCESS);

    VerifyLimits(device, limits, features);

    VerifyRequiredFormats(device, features);
}
#endif

// =====================================================================================================================
VkResult PhysicalDevice::GetDisplayProperties(
    uint32_t*                                   pPropertyCount,
    utils::ArrayView<VkDisplayPropertiesKHR>    properties)
{
    uint32_t  screenCount   = 0;
    uint32_t  count         = 0;

    Pal::IScreen*   pScreens      = nullptr;
    uint32_t        propertyCount = *pPropertyCount;

    if (properties.IsNull())
    {
        VkInstance()->FindScreens(PalDevice(), pPropertyCount, nullptr);
        return VK_SUCCESS;
    }

    Pal::IScreen* pAttachedScreens[Pal::MaxScreens];

    VkResult result = VkInstance()->FindScreens(PalDevice(), &propertyCount, pAttachedScreens);

    uint32_t loopCount = Util::Min(*pPropertyCount, propertyCount);

    for (uint32_t i = 0; i < loopCount; i++)
    {
        Pal::ScreenProperties props = {};

        pAttachedScreens[i]->GetProperties(&props);

        properties[count].display = reinterpret_cast<VkDisplayKHR>(pAttachedScreens[i]);
        properties[count].displayName = nullptr;
        properties[count].physicalDimensions.width  = props.physicalDimension.width;
        properties[count].physicalDimensions.height = props.physicalDimension.height;
        properties[count].physicalResolution.width  = props.physicalResolution.width;
        properties[count].physicalResolution.height = props.physicalResolution.height;
        properties[count].supportedTransforms       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        properties[count].planeReorderPossible      = false;
        properties[count].persistentContent         = false;
    }

    *pPropertyCount = loopCount;

    return result;
}

// =====================================================================================================================
// So far we don't support overlay and underlay. Therefore, it will just return the main plane.
VkResult PhysicalDevice::GetDisplayPlaneProperties(
    uint32_t*                                       pPropertyCount,
    utils::ArrayView<VkDisplayPlanePropertiesKHR>   properties)
{
    uint32_t        propertyCount = *pPropertyCount;

    if (properties.IsNull())
    {
        VkInstance()->FindScreens(PalDevice(), pPropertyCount, nullptr);
        return VK_SUCCESS;
    }

    Pal::IScreen* pAttachedScreens[Pal::MaxScreens];

    VkResult result = VkInstance()->FindScreens(PalDevice(), &propertyCount, pAttachedScreens);

    uint32_t loopCount = Util::Min(*pPropertyCount, propertyCount);

    for (uint32_t i = 0; i < loopCount; i++)
    {
        properties[i].currentDisplay = reinterpret_cast<VkDisplayKHR>(pAttachedScreens[i]);
        properties[i].currentStackIndex = 0;
    }

    *pPropertyCount = loopCount;

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetDisplayPlaneSupportedDisplays(
    uint32_t                                    planeIndex,
    uint32_t*                                   pDisplayCount,
    VkDisplayKHR*                               pDisplays)
{
    uint32_t displayCount = *pDisplayCount;

    if (pDisplays == nullptr)
    {
        VkInstance()->FindScreens(PalDevice(), pDisplayCount, nullptr);
        return VK_SUCCESS;
    }

    Pal::IScreen* pAttachedScreens[Pal::MaxScreens];

    VkResult result = VkInstance()->FindScreens(PalDevice(), &displayCount, pAttachedScreens);

    uint32_t loopCount = Util::Min(*pDisplayCount, displayCount);

    for (uint32_t i = 0; i < loopCount; i++)
    {
        pDisplays[i] = reinterpret_cast<VkDisplayKHR>(pAttachedScreens[i]);
    }

    *pDisplayCount = loopCount;

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetDisplayModeProperties(
    VkDisplayKHR                                  display,
    uint32_t*                                     pPropertyCount,
    utils::ArrayView<VkDisplayModePropertiesKHR>  properties)
{
    VkResult result = VK_SUCCESS;

    Pal::IScreen* pScreen = reinterpret_cast<Pal::IScreen*>(display);
    VK_ASSERT(pScreen != nullptr);

    if (properties.IsNull())
    {
        return VkInstance()->GetScreenModeList(pScreen, pPropertyCount, nullptr);
    }

    Pal::ScreenMode* pScreenMode[Pal::MaxModePerScreen];

    uint32_t propertyCount = *pPropertyCount;

    result = VkInstance()->GetScreenModeList(pScreen, &propertyCount, pScreenMode);

    uint32_t loopCount = Util::Min(*pPropertyCount, propertyCount);

    for (uint32_t i = 0; i < loopCount; i++)
    {
        DisplayModeObject* pDisplayMode =
            reinterpret_cast<DisplayModeObject*>(VkInstance()->AllocMem(sizeof(DisplayModeObject),
                                                                        VK_DEFAULT_MEM_ALIGN,
                                                                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
        pDisplayMode->pScreen = pScreen;
        memcpy(&pDisplayMode->palScreenMode, pScreenMode[i], sizeof(Pal::ScreenMode));
        properties[i].displayMode = reinterpret_cast<VkDisplayModeKHR>(pDisplayMode);
        properties[i].parameters.visibleRegion.width  = pScreenMode[i]->extent.width;
        properties[i].parameters.visibleRegion.height = pScreenMode[i]->extent.height;
        // The refresh rate returned by pal is HZ.
        // Spec requires refresh rate to be "the number of times the display is refreshed each second
        // multiplied by 1000", in other words, HZ * 1000
        properties[i].parameters.refreshRate = pScreenMode[i]->refreshRate * 1000;
    }

    *pPropertyCount = loopCount;

    return result;
}

// =====================================================================================================================
VkResult PhysicalDevice::GetDisplayPlaneCapabilities(
    VkDisplayModeKHR                            mode,
    uint32_t                                    planeIndex,
    VkDisplayPlaneCapabilitiesKHR*              pCapabilities)
{
    Pal::ScreenMode* pMode = &(reinterpret_cast<DisplayModeObject*>(mode)->palScreenMode);
    VK_ASSERT(pCapabilities != nullptr);

    pCapabilities->supportedAlpha = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    pCapabilities->minSrcPosition.x = 0;
    pCapabilities->minSrcPosition.y = 0;
    pCapabilities->maxSrcPosition.x = 0;
    pCapabilities->maxSrcPosition.y = 0;
    pCapabilities->minDstPosition.x = 0;
    pCapabilities->minDstPosition.y = 0;
    pCapabilities->maxDstPosition.x = 0;
    pCapabilities->maxDstPosition.y = 0;

    pCapabilities->minSrcExtent.width  = pMode->extent.width;
    pCapabilities->minSrcExtent.height = pMode->extent.height;
    pCapabilities->maxSrcExtent.width  = pMode->extent.width;
    pCapabilities->maxSrcExtent.height = pMode->extent.height;
    pCapabilities->minDstExtent.width  = pMode->extent.width;
    pCapabilities->minDstExtent.height = pMode->extent.height;
    pCapabilities->maxDstExtent.width  = pMode->extent.width;
    pCapabilities->maxDstExtent.height = pMode->extent.height;

    return VK_SUCCESS;
}

// =====================================================================================================================
// So far, we don't support customized mode.
// we only create/insert mode if it matches existing mode.
VkResult PhysicalDevice::CreateDisplayMode(
    VkDisplayKHR                                display,
    const VkDisplayModeCreateInfoKHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDisplayModeKHR*                           pMode)
{
    Pal::IScreen*    pScreen = reinterpret_cast<Pal::IScreen*>(display);

    VkResult result = VK_SUCCESS;

    Pal::ScreenMode* pScreenMode[Pal::MaxModePerScreen];
    uint32_t propertyCount = Pal::MaxModePerScreen;

    VkInstance()->GetScreenModeList(pScreen, &propertyCount, pScreenMode);

    bool isValidMode = false;

    for (uint32_t i = 0; i < propertyCount; i++)
    {
        // The modes are considered as identical if the dimension as well as the refresh rate are the same.
        if ((pCreateInfo->parameters.visibleRegion.width  == pScreenMode[i]->extent.width) &&
            (pCreateInfo->parameters.visibleRegion.height == pScreenMode[i]->extent.height) &&
            (pCreateInfo->parameters.refreshRate          == pScreenMode[i]->refreshRate * 1000))
        {
            isValidMode = true;
            break;
        }
    }

    if (isValidMode)
    {
        DisplayModeObject* pNewMode = nullptr;
        if (pAllocator)
        {
            pNewMode = reinterpret_cast<DisplayModeObject*>(
                pAllocator->pfnAllocation(
                pAllocator->pUserData,
                sizeof(DisplayModeObject),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
        }
        else
        {
            pNewMode = reinterpret_cast<DisplayModeObject*>(VkInstance()->AllocMem(
                sizeof(DisplayModeObject),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
        }

        if (pNewMode)
        {
            pNewMode->palScreenMode.extent.width  = pCreateInfo->parameters.visibleRegion.width;
            pNewMode->palScreenMode.extent.height = pCreateInfo->parameters.visibleRegion.height;
            pNewMode->palScreenMode.refreshRate   = pCreateInfo->parameters.refreshRate;
            pNewMode->palScreenMode.flags.u32All  = 0;
            pNewMode->pScreen                     = pScreen;
            *pMode = reinterpret_cast<VkDisplayModeKHR>(pNewMode);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    else
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// C-style entry points
namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice)
{
    PhysicalDevice*              pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(physicalDevice);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pPhysicalDevice->VkInstance()->GetAllocCallbacks();

    return pPhysicalDevice->CreateDevice(pCreateInfo, pAllocCB, pDevice);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice                            physicalDevice,
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->EnumerateExtensionProperties(
        pLayerName,
        pPropertyCount,
        pProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetFeatures(pFeatures);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
    VK_ASSERT(pProperties != nullptr);

    VkResult result = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDeviceProperties(pProperties);

    VK_IGNORE(result);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetImageFormatProperties(
        format,
        type,
        tiling,
        usage,
        flags,
        pImageFormatProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetFormatProperties(format, pFormatProperties);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties)
{
    // According to SDK 1.0.33 release notes, this function is deprecated.
    // However, most apps link to older vulkan loaders so we need to keep this function active just in case the app or
    // an earlier loader works incorrectly if this function is removed from the dispatch table.
    // TODO: Remove when it is safe to do so.

    if (pProperties == nullptr)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
    *pMemoryProperties = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetMemoryProperties();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetQueueFamilyProperties(pQueueFamilyPropertyCount,
        pQueueFamilyProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkSampleCountFlagBits                       samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pPropertyCount,
    VkSparseImageFormatProperties*              pProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSparseImageFormatProperties(
        format,
        type,
        samples,
        usage,
        tiling,
        pPropertyCount,
        utils::ArrayView<VkSparseImageFormatProperties>(pProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    VkSurfaceKHR                                surface,
    VkBool32*                                   pSupported)
{
    DisplayableSurfaceInfo displayableInfo = {};

    VkResult result = PhysicalDevice::UnpackDisplayableSurface(Surface::ObjectFromHandle(surface), &displayableInfo);

    const bool supported = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->QueueSupportsPresents(queueFamilyIndex, displayableInfo.icdPlatform);

    *pSupported = supported ? VK_TRUE : VK_FALSE;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pPresentModeCount,
    VkPresentModeKHR*                           pPresentModes)
{
    DisplayableSurfaceInfo displayableInfo = {};

    VkResult result = PhysicalDevice::UnpackDisplayableSurface(Surface::ObjectFromHandle(surface), &displayableInfo);

    if (result == VK_SUCCESS)
    {
        Pal::PresentMode presentMode = (displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY)
                                        ? Pal::PresentMode::Fullscreen : Pal::PresentMode::Windowed;
        result = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfacePresentModes(
            displayableInfo,
            presentMode,
            pPresentModeCount,
            pPresentModes);
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilitiesKHR*                   pSurfaceCapabilities)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceCapabilities(
        surface, pSurfaceCapabilities);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceSurfaceInfo2KHR*      pSurfaceInfo,
    VkSurfaceCapabilities2KHR*                  pSurfaceCapabilities)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceCapabilities2KHR(
        pSurfaceInfo, pSurfaceCapabilities);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pSurfaceFormatCount,
    VkSurfaceFormatKHR*                         pSurfaceFormats)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceFormats(
                Surface::ObjectFromHandle(surface),pSurfaceFormatCount, pSurfaceFormats);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceSurfaceInfo2KHR*      pSurfaceInfo,
    uint32_t*                                   pSurfaceFormatCount,
    VkSurfaceFormat2KHR*                        pSurfaceFormats)
{
    union
    {
        const VkStructHeader*                  pHeader;
        const VkPhysicalDeviceSurfaceInfo2KHR* pVkPhysicalDeviceSurfaceInfo2KHR;
    };

    VkResult result = VK_SUCCESS;

    for (pVkPhysicalDeviceSurfaceInfo2KHR = pSurfaceInfo;
        (pHeader != nullptr) && ((result == VK_SUCCESS) || (result == VK_INCOMPLETE));
        pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR:
                result = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceFormats(
                    Surface::ObjectFromHandle(pVkPhysicalDeviceSurfaceInfo2KHR->surface),
                    pSurfaceFormatCount,
                    pSurfaceFormats);
                break;

            default:
                break;
        }
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHX(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->
        GetPhysicalDevicePresentRectangles(surface, pRectCount, pRects);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures2*                  pFeatures)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetFeatures2(
        reinterpret_cast<VkStructHeaderNonConst*>(pFeatures));
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties2*                pProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDeviceProperties2(pProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties2*                        pFormatProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetFormatProperties2(
                format,
                pFormatProperties);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2*     pImageFormatInfo,
    VkImageFormatProperties2*                   pImageFormatProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetImageFormatProperties2(
                        pImageFormatInfo,
                        pImageFormatProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMultisamplePropertiesEXT(
    VkPhysicalDevice                            physicalDevice,
    VkSampleCountFlagBits                       samples,
    VkMultisamplePropertiesEXT*                 pMultisampleProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDeviceMultisampleProperties(
        samples,
        pMultisampleProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2*                   pQueueFamilyProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetQueueFamilyProperties(
            pQueueFamilyPropertyCount,
            pQueueFamilyProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties2*          pMemoryProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetMemoryProperties2(pMemoryProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice                                    physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2*       pFormatInfo,
    uint32_t*                                           pPropertyCount,
    VkSparseImageFormatProperties2*                     pProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSparseImageFormatProperties2(
            pFormatInfo,
            pPropertyCount,
            pProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice                                physicalDevice,
    const VkPhysicalDeviceExternalBufferInfo*       pExternalBufferInfo,
    VkExternalBufferProperties*                     pExternalBufferProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetExternalBufferProperties(
            pExternalBufferInfo,
            pExternalBufferProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice                                physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo*    pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties*                  pExternalSemaphoreProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetExternalSemaphoreProperties(
            pExternalSemaphoreInfo,
            pExternalSemaphoreProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFenceProperties(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo*    pExternalFenceInfo,
    VkExternalFenceProperties*                  pExternalFenceProperties)
{
    ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetExternalFenceProperties(
        pExternalFenceInfo,
        pExternalFenceProperties);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkTrimCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolTrimFlags                      flags)
{
}

#include <xcb/xcb.h>

// =====================================================================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXcbPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    xcb_connection_t*                           connection,
    xcb_visualid_t                              visual_id)
{
    Pal::OsDisplayHandle displayHandle = connection;
    VkIcdWsiPlatform     platform      = VK_ICD_WSI_PLATFORM_XCB;
    int64_t              visualId      = visual_id;

    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->DeterminePresentationSupported(displayHandle,
                                                                                               platform,
                                                                                               visualId,
                                                                                               queueFamilyIndex);
}

// =====================================================================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    Display*                                    dpy,
    VisualID                                    visualId)
{
    Pal::OsDisplayHandle displayHandle = dpy;
    VkIcdWsiPlatform     platform      = VK_ICD_WSI_PLATFORM_XLIB;
    int64_t              visual        = visualId;

    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->DeterminePresentationSupported(displayHandle,
                                                                                               platform,
                                                                                               visual,
                                                                                               queueFamilyIndex);
}

// =====================================================================================================================
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceWaylandPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    struct wl_display*                          display)
{
    Pal::OsDisplayHandle displayHandle = display;
    VkIcdWsiPlatform     platform      = VK_ICD_WSI_PLATFORM_WAYLAND;

    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->DeterminePresentationSupported(displayHandle,
                                                                                               platform,
                                                                                               0,
                                                                                               queueFamilyIndex);
}
#endif

#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireXlibDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    Display*                                    dpy,
    VkDisplayKHR                                display)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->AcquireXlibDisplay(dpy, display);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetRandROutputDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    Display*                                    dpy,
    RROutput                                    randrOutput,
    VkDisplayKHR*                               pDisplay)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetRandROutputDisplay(dpy, randrOutput, pDisplay);
}
#endif

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkReleaseDisplayEXT(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->ReleaseDisplay(display);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects)
{
    // TODO: Currently we just return zero rectangles, as we don't support
    // VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR.
    *pRectCount = 0;
    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPropertiesKHR*                     pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayProperties(
                    pPropertyCount,
                    utils::ArrayView<VkDisplayPropertiesKHR>(pProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPlanePropertiesKHR*                pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayPlaneProperties(
                    pPropertyCount,
                    utils::ArrayView<VkDisplayPlanePropertiesKHR>(pProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneSupportedDisplaysKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    planeIndex,
    uint32_t*                                   pDisplayCount,
    VkDisplayKHR*                               pDisplays)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayPlaneSupportedDisplays(
                                                planeIndex,
                                                pDisplayCount,
                                                pDisplays);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayModePropertiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    uint32_t*                                   pPropertyCount,
    VkDisplayModePropertiesKHR*                 pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayModeProperties(
                                                display,
                                                pPropertyCount,
                                                utils::ArrayView<VkDisplayModePropertiesKHR>(pProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDisplayModeKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    const VkDisplayModeCreateInfoKHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDisplayModeKHR*                           pMode)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->CreateDisplayMode(
                                                display,
                                                pCreateInfo,
                                                pAllocator,
                                                pMode);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneCapabilitiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayModeKHR                            mode,
    uint32_t                                    planeIndex,
    VkDisplayPlaneCapabilitiesKHR*              pCapabilities)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayPlaneCapabilities(
                                                mode,
                                                planeIndex,
                                                pCapabilities);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayProperties2KHR*                    pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayProperties(
                    pPropertyCount,
                    utils::ArrayView<VkDisplayPropertiesKHR>(pProperties, &pProperties->displayProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceDisplayPlaneProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pPropertyCount,
    VkDisplayPlaneProperties2KHR*               pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayPlaneProperties(
                    pPropertyCount,
                    utils::ArrayView<VkDisplayPlanePropertiesKHR>(pProperties, &pProperties->displayPlaneProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayModeProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkDisplayKHR                                display,
    uint32_t*                                   pPropertyCount,
    VkDisplayModeProperties2KHR*                pProperties)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayModeProperties(
                    display,
                    pPropertyCount,
                    utils::ArrayView<VkDisplayModePropertiesKHR>(pProperties, &pProperties->displayModeProperties));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDisplayPlaneCapabilities2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkDisplayPlaneInfo2KHR*               pDisplayPlaneInfo,
    VkDisplayPlaneCapabilities2KHR*             pCapabilities)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetDisplayPlaneCapabilities(
                                                                    pDisplayPlaneInfo->mode,
                                                                    pDisplayPlaneInfo->planeIndex,
                                                                    &pCapabilities->capabilities);
}

}

}
