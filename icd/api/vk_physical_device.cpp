/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if  LLPC_CLIENT_INTERFACE_MAJOR_VERSION>= 39
#include "llpc.h"
#endif

#include "res/ver.h"

#include "settings/settings.h"

#include "palDevice.h"
#include "palCmdBuffer.h"
#include "palFormatInfo.h"
#include "palLib.h"
#include "palMath.h"
#include "palMsaaState.h"
#include "palPlatformKey.h"
#include "palScreen.h"
#include "palHashLiteralString.h"
#include "palVectorImpl.h"
#include <string>
#include <vector>

#undef max
#undef min

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
    VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT|
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT|
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT|
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT|
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT|
    VK_FORMAT_FEATURE_DISJOINT_BIT|
    VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT|
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
// Returns true if the given physical device supports the minimum required compressed texture formats to report ETC2
// support
static bool VerifyEtc2FormatSupport(
    const PhysicalDevice& dev)
{
    // Based on vulkan spec Table 67: Mandatory format support: ETC2 and EAC compressed formats with VkImageType
    // VK_IMAGE_TYPE_2D
    const bool etc2Support =
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_EAC_R11_UNORM_BLOCK,       1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_EAC_R11_SNORM_BLOCK,       1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_EAC_R11G11_UNORM_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_EAC_R11G11_SNORM_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return etc2Support;
}

// =====================================================================================================================
// Returns true if the given physical device supports the minimum required compressed texture formats to report ASTC-LDR
// support
static bool VerifyAstcLdrFormatSupport(
    const PhysicalDevice& dev)
{
    // Based on vulkan spec Table 68: Mandatory format support: ASTC LDR compressed formats with VkImageType
    // VK_IMAGE_TYPE_2D
    const bool astcLdrSupport =
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_4x4_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_4x4_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_5x4_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_5x4_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_5x5_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_5x5_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_6x5_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_6x5_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_6x6_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_6x6_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x5_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x5_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x6_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x6_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x8_UNORM_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_8x8_SRGB_BLOCK,    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x5_UNORM_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x5_SRGB_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x6_UNORM_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x6_SRGB_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x8_UNORM_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x8_SRGB_BLOCK,   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x10_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_10x10_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_12x10_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_12x10_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_12x12_UNORM_BLOCK, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) &&
        VerifyFormatSupport(dev, VK_FORMAT_ASTC_12x12_SRGB_BLOCK,  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return astcLdrSupport;
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
    VulkanSettingsLoader*   pSettingsLoader,
    AppProfile              appProfile
    )
    :
    m_pPhysicalDeviceManager(pPhysicalDeviceManager),
    m_pPalDevice(pPalDevice),
    m_memoryTypeMask(0),
    m_memoryTypeMaskForExternalSharing(0),
    m_memoryVkIndexAttachmentImage(0),
    m_pSettingsLoader(pSettingsLoader),
    m_sampleLocationSampleCounts(0),
    m_vrHighPrioritySubEngineIndex(UINT32_MAX),
    m_RtCuHighComputeSubEngineIndex(UINT32_MAX),
    m_queueFamilyCount(0),
    m_appProfile(appProfile),
    m_prtOnDmaSupported(true),
    m_eqaaSupported(true),
    m_supportedExtensions(),
    m_allowedExtensions(),
    m_compiler(this),
    m_pPlatformKey(nullptr)
{
    memset(&m_limits, 0, sizeof(m_limits));
    memset(m_formatFeatureMsaaTarget, 0, sizeof(m_formatFeatureMsaaTarget));
    memset(&m_queueFamilies, 0, sizeof(m_queueFamilies));
    memset(&m_memoryProperties, 0, sizeof(m_memoryProperties));
    memset(&m_gpaProps, 0, sizeof(m_gpaProps));
    memset(&m_memoryVkIndexAddRemoteBackupHeap, 0, sizeof(m_memoryVkIndexAddRemoteBackupHeap));
    for (uint32_t i = 0; i < Pal::GpuHeapCount; i++)
    {
        m_memoryPalHeapToVkIndexBits[i] = 0; // invalid bits
        m_memoryPalHeapToVkHeap[i]      = Pal::GpuHeapCount; // invalid index
    }

    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    {
        m_memoryVkIndexToPalHeap[i] = Pal::GpuHeapCount; // invalid index
    }
    memset(&m_memoryUsageTracker, 0, sizeof(m_memoryUsageTracker));
    memset(&m_pipelineCacheUUID, 0, VK_UUID_SIZE);

    for (uint32_t i = 0; i < VkMemoryHeapNum; ++i)
    {
        m_heapVkToPal[i] = Pal::GpuHeapCount; // invalid index
    }
}

// =====================================================================================================================
// Creates a new Vulkan physical device object
VkResult PhysicalDevice::Create(
    PhysicalDeviceManager* pPhysicalDeviceManager,
    Pal::IDevice*          pPalDevice,
    VulkanSettingsLoader*  pSettingsLoader,
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

    VK_INIT_DISPATCHABLE(PhysicalDevice, pMemory, (pPhysicalDeviceManager, pPalDevice, pSettingsLoader, appProfile));

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
    VkFormatFeatureFlags*                   pOutFormatFeatureFlags,
    const RuntimeSettings&                  settings)
{
    const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(format);

    const size_t formatIdx = static_cast<size_t>(swizzledFormat.format);
    const size_t tilingIdx = ((imageTiling == VK_IMAGE_TILING_LINEAR) ? Pal::IsLinear : Pal::IsNonLinear);

    VkFormatFeatureFlags retFlags = PalToVkFormatFeatureFlags(formatProperties.features[formatIdx][tilingIdx]);

    // Only expect vertex buffer support for core formats for now (change this if needed otherwise in the future).
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT) && (imageTiling == VK_IMAGE_TILING_LINEAR))
    {
        bool canSupportVertexFormat = false;
        canSupportVertexFormat = Llpc::ICompiler::IsVertexFormatSupported(format);
        if (canSupportVertexFormat)
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

    if (Formats::IsDepthStencilFormat(format))
    {
        if (imageTiling == VK_IMAGE_TILING_LINEAR)
        {
            retFlags = static_cast<VkFormatFeatureFlags>(0);
        }

        retFlags &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        retFlags &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

        retFlags &= ~VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    }
    else
    {
        retFlags &= ~VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (Formats::IsYuvFormat(format))
    {
        retFlags &= ~VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        retFlags &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        retFlags &= ~VK_FORMAT_FEATURE_BLIT_SRC_BIT;
        retFlags &= ~VK_FORMAT_FEATURE_BLIT_DST_BIT;
    }

    *pOutFormatFeatureFlags = retFlags;
}

// =====================================================================================================================
// Checks to see if memory is available for PhysicalDevice local allocations made by the application (externally) and
// reports OOM if necessary
VkResult PhysicalDevice::TryIncreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     heapIdx)
{
    Util::MutexAuto lock(&m_memoryUsageTracker.trackerMutex);

    Pal::gpusize memorySizePostAllocation = m_memoryUsageTracker.allocatedMemorySize[heapIdx] + allocationSize;

    return (memorySizePostAllocation > m_memoryUsageTracker.totalMemorySize[heapIdx]) ?
        VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}

// =====================================================================================================================
// Increases the allocated memory size for PhysicalDevice local allocations made by the application (externally) and
// reports OOM if necessary
void PhysicalDevice::IncreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     heapIdx)
{
    Util::MutexAuto lock(&m_memoryUsageTracker.trackerMutex);

    m_memoryUsageTracker.allocatedMemorySize[heapIdx] += allocationSize;
}

// =====================================================================================================================
// Decreases the allocated memory size for PhysicalDevice local allocations made by the application (externally)
void PhysicalDevice::DecreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     heapIdx)
{
    Util::MutexAuto lock(&m_memoryUsageTracker.trackerMutex);

    VK_ASSERT(m_memoryUsageTracker.allocatedMemorySize[heapIdx] >= allocationSize);

    m_memoryUsageTracker.allocatedMemorySize[heapIdx] -= allocationSize;
}

// =====================================================================================================================
// Generate our platform key
void PhysicalDevice::InitializePlatformKey(
    const RuntimeSettings& settings)
{
    static constexpr Util::HashAlgorithm KeyAlgorithm = Util::HashAlgorithm::Sha1;

    struct
    {
        VkPhysicalDeviceProperties properties;
        char*                      timestamp[sizeof(__TIMESTAMP__)];
    } initialData;

    memset(&initialData, 0, sizeof(initialData));

    VkResult result = GetDeviceProperties(&initialData.properties);

    if (result == VK_SUCCESS)
    {
        size_t memSize = Util::GetPlatformKeySize(KeyAlgorithm);
        void*  pMem    = VkInstance()->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMem != nullptr)
        {
            if (settings.markPipelineCacheWithBuildTimestamp)
            {
                memcpy(initialData.timestamp, __TIMESTAMP__, sizeof(__TIMESTAMP__));
            }

            if (Util::CreatePlatformKey(KeyAlgorithm, &initialData, sizeof(initialData), pMem, &m_pPlatformKey) !=
                Util::Result::Success)
            {
                VkInstance()->FreeMem(pMem);
            }
        }
    }
}

// =====================================================================================================================
VkResult PhysicalDevice::Initialize()
{
    const bool nullGpu = VkInstance()->IsNullGpuModeEnabled();

    // Collect generic device properties
    Pal::Result result = m_pPalDevice->GetProperties(&m_properties);

    const RuntimeSettings& settings = GetRuntimeSettings();

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
                const auto& engineProps = m_properties.engineProperties[idx];
                finalizeInfo.requestedEngineCounts[idx].engines = ((1 << engineProps.engineCount) - 1);
            }
        }

        if (settings.fullScreenFrameMetadataSupport)
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

        finalizeInfo.internalTexOptLevel = VkToPalTexFilterQuality(settings.vulkanTexFilterQuality);

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
        for (uint32_t heapIdx = 0; heapIdx < Pal::GpuHeapCount; heapIdx++)
        {
            m_memoryUsageTracker.totalMemorySize[heapIdx] = heapProperties[heapIdx].heapSize;
        }

        if (m_memoryUsageTracker.totalMemorySize[Pal::GpuHeapInvisible] == 0)
        {
            // Disable tracking for the local invisible heap and allow it to overallocate when it has size 0
            m_memoryUsageTracker.totalMemorySize[Pal::GpuHeapInvisible] = UINT64_MAX;
        }

        result = m_memoryUsageTracker.trackerMutex.Init();
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
        constexpr Pal::GpuHeap priority[Pal::GpuHeapCount] =
        {
            Pal::GpuHeapInvisible,
            Pal::GpuHeapGartUswc,
            Pal::GpuHeapLocal,
            Pal::GpuHeapGartCacheable
        };

        const Pal::gpusize invisHeapSize = heapProperties[Pal::GpuHeapInvisible].heapSize;
        const Pal::gpusize localHeapSize = heapProperties[Pal::GpuHeapLocal].heapSize;

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

                m_heapVkToPal[heapIndex]            = palGpuHeap;
                m_memoryPalHeapToVkHeap[palGpuHeap] = heapIndex;

                if (palGpuHeap == Pal::GpuHeapGartUswc)
                {
                    // These two should match because the PAL GPU heaps share the same physical memory.
                    VK_ASSERT(memoryHeap.size == heapProperties[Pal::GpuHeapGartCacheable].heapSize);

                    heapIndices[Pal::GpuHeapGartCacheable] = heapIndex;
                }
                else if ((palGpuHeap == Pal::GpuHeapLocal) &&
                         (heapIndices[Pal::GpuHeapInvisible] == Pal::GpuHeapCount))
                {
                    // GPU invisible heap isn't present, but its memory properties are a subset of the GPU local heap.
                    heapIndices[Pal::GpuHeapInvisible] = heapIndex;
                }
            }
        }
        VK_ASSERT(m_memoryProperties.memoryHeapCount <= (Pal::GpuHeapCount - 1));

        // Spec requires at least one heap to include VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
        if (m_memoryProperties.memoryHeapCount == 1)
        {
            VK_ASSERT(m_properties.gpuType == Pal::GpuType::Integrated);
            m_memoryProperties.memoryHeaps[0].flags |= VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        }

        // Track that we want to add a matching coherent memory type (VK_AMD_device_coherent_memory)
        bool memTypeWantsCoherentMemory[VK_MAX_MEMORY_TYPES] = {};

        // Initialize memory types
        for (uint32_t orderedHeapIndex = 0; orderedHeapIndex < Pal::GpuHeapCount; ++orderedHeapIndex)
        {
            Pal::GpuHeap palGpuHeap = priority[orderedHeapIndex];

            uint32_t heapIndex = heapIndices[palGpuHeap];

            // We must have a heap capable of allocating this memory type to expose it.
            if (heapIndex < Pal::GpuHeapCount)
            {
                uint32_t memoryTypeIndex = m_memoryProperties.memoryTypeCount++;

                Pal::GpuHeap allocPalGpuHeap = ((palGpuHeap == Pal::GpuHeapInvisible) && (invisHeapSize == 0)) ? Pal::GpuHeapLocal : palGpuHeap;
                m_memoryVkIndexToPalHeap[memoryTypeIndex] = allocPalGpuHeap;
                m_memoryPalHeapToVkIndexBits[allocPalGpuHeap] |= (1UL << memoryTypeIndex);

                VkMemoryType* pMemoryType = &m_memoryProperties.memoryTypes[memoryTypeIndex];

                pMemoryType->heapIndex = heapIndex;

                m_memoryTypeMask |= 1 << memoryTypeIndex;

                const Pal::GpuMemoryHeapProperties& heapProps = heapProperties[palGpuHeap];

                if (heapProps.flags.cpuVisible)
                {
                    pMemoryType->propertyFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                }

                if (heapProps.flags.cpuGpuCoherent)
                {
                    pMemoryType->propertyFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                }

                if (heapProps.flags.cpuUncached == 0)
                {
                    pMemoryType->propertyFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                }

                if (m_memoryProperties.memoryHeaps[heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                {
                    pMemoryType->propertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                    if (m_memoryProperties.memoryHeapCount > 1)
                    {
                        // Add back-up heap for parts with VRAM smaller than 8GB.
                        // Note: The back-up heap can also be added if overallocation is allowed via
                        //       VK_AMD_memory_overallocation_behavior.
                        // Use m_heapVkToPal instead of palGpuHeap here to handle cases where multiple memory types
                        // share the same heap.
                        const Pal::gpusize heapSize = heapProperties[m_heapVkToPal[pMemoryType->heapIndex]].heapSize;
                        m_memoryVkIndexAddRemoteBackupHeap[memoryTypeIndex] =
                            (heapSize < settings.memoryRemoteBackupHeapMinHeapSize);
                    }
                }

                if (m_properties.gfxipProperties.flags.supportGl2Uncached)
                {
                    // Add device coherent memory type based on below type:
                    // 1. Visible and host coherent
                    // 2. Invisible
                    if (((pMemoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                        (pMemoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ||
                        (palGpuHeap == Pal::GpuHeapInvisible))
                    {
                        memTypeWantsCoherentMemory[memoryTypeIndex] = true;
                    }
                }

                if (palGpuHeap == Pal::GpuHeapInvisible)
                {
                    if ((invisHeapSize + localHeapSize) >= settings.memoryAttachmentImageMemoryTypeMinHeapSize)
                    {
                        // The attachment image memory type (unavailable for parts with VRAM smaller than 4GB)
                        // reports identical properties as the default invisible. However, it does
                        // not add a remote back-up heap unless overallocation is allowed via
                        // VK_AMD_memory_overallocation_behavior.
                        m_memoryVkIndexAttachmentImage = m_memoryProperties.memoryTypeCount++;

                        m_memoryProperties.memoryTypes[m_memoryVkIndexAttachmentImage] =
                            m_memoryProperties.memoryTypes[memoryTypeIndex];
                        m_memoryVkIndexToPalHeap[m_memoryVkIndexAttachmentImage] =
                            m_memoryVkIndexToPalHeap[memoryTypeIndex];
                    }
                    else
                    {
                        m_memoryVkIndexAttachmentImage = memoryTypeIndex;
                    }
                }

                // Optional: if we have exposed a memory type that is host visible, add a backup
                // memory type that is not host visible. We will use it for optimally tiled images.
                if (settings.addHostInvisibleMemoryTypesForOptimalImages &&
                    ((pMemoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)  &&
                    // Skip host visible+coherent+cached as we won't need it
                    ((pMemoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == 0))
                {
                    memoryTypeIndex = m_memoryProperties.memoryTypeCount++;

                    m_memoryTypeMask                         |= 1 << memoryTypeIndex;
                    m_memoryVkIndexToPalHeap[memoryTypeIndex] = palGpuHeap;
                    m_memoryPalHeapToVkIndexBits[palGpuHeap] |= (1UL << memoryTypeIndex);

                    VkMemoryType* pNextMemoryType = &m_memoryProperties.memoryTypes[memoryTypeIndex];

                    constexpr VkFlags hostMask = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

                    pNextMemoryType->heapIndex     = pMemoryType->heapIndex;
                    pNextMemoryType->propertyFlags = pMemoryType->propertyFlags & ~hostMask;
                }
            }
        }

        VkBool32 protectedMemorySupported = VK_FALSE;
        GetPhysicalDeviceProtectedMemoryFeatures(&protectedMemorySupported);

        if (protectedMemorySupported)
        {
            // The heap order of protected memory.
            constexpr Pal::GpuHeap ProtectedPriority[Pal::GpuHeapCount - 1] =
            {
                Pal::GpuHeapGartUswc,
                Pal::GpuHeapInvisible,
                Pal::GpuHeapLocal
            };

            bool protectedMemoryTypeFound = false;

            for (uint32_t orderedHeapIndex = 0; orderedHeapIndex < Pal::GpuHeapCount - 1; ++orderedHeapIndex)
            {
                Pal::GpuHeap palGpuHeap     = ProtectedPriority[orderedHeapIndex];
                const Pal::gpusize heapSize = heapProperties[palGpuHeap].heapSize;

                if ((heapSize > 0) && heapProperties[palGpuHeap].flags.supportsTmz)
                {
                    uint32_t memoryTypeIndex                  = m_memoryProperties.memoryTypeCount++;
                    m_memoryTypeMask                         |= 1 << memoryTypeIndex;
                    m_memoryVkIndexToPalHeap[memoryTypeIndex] = palGpuHeap;
                    m_memoryPalHeapToVkIndexBits[palGpuHeap] |= (1UL << memoryTypeIndex);
                    VkMemoryType* pMemType                    = &m_memoryProperties.memoryTypes[memoryTypeIndex];
                    pMemType->heapIndex                       = heapIndices[palGpuHeap];

                    if ((palGpuHeap != Pal::GpuHeapGartUswc) || (m_memoryProperties.memoryHeapCount == 1))
                    {
                        pMemType->propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    }
                    else
                    {
                        pMemType->propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT;
                    }
                    protectedMemoryTypeFound = true;
                }
            }

            if (protectedMemoryTypeFound == false)
            {
                VK_ALERT_ALWAYS_MSG("No protected memory type.");
                VK_NEVER_CALLED();
            }
        }

        // Add device coherent memory type based on memory types which have been added in m_memoryProperties.memoryTypes
        // In PAL, uncached device memory, which is always device coherent, will be allocated.
        if (m_properties.gfxipProperties.flags.supportGl2Uncached)
        {
            uint32_t currentTypeCount = m_memoryProperties.memoryTypeCount;
            for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < currentTypeCount; ++memoryTypeIndex)
            {
                VkMemoryType* pCurrentmemoryType = &m_memoryProperties.memoryTypes[memoryTypeIndex];
                VkMemoryType* pLastMemoryType    = &m_memoryProperties.memoryTypes[m_memoryProperties.memoryTypeCount];

                if (memTypeWantsCoherentMemory[memoryTypeIndex])
                {
                    pLastMemoryType->heapIndex     = pCurrentmemoryType->heapIndex;
                    pLastMemoryType->propertyFlags = pCurrentmemoryType->propertyFlags |
                                                   VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
                                                   VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;

                    m_memoryVkIndexToPalHeap[m_memoryProperties.memoryTypeCount] =
                        m_memoryVkIndexToPalHeap[memoryTypeIndex];
                    m_memoryPalHeapToVkIndexBits[m_memoryVkIndexToPalHeap[m_memoryProperties.memoryTypeCount]] |=
                        (1UL << memoryTypeIndex);

                    m_memoryTypeMask |= 1 << m_memoryProperties.memoryTypeCount;
                    ++m_memoryProperties.memoryTypeCount;
                }
            }
        }

        VK_ASSERT(m_memoryProperties.memoryTypeCount <= VK_MAX_MEMORY_TYPES);
        VK_ASSERT(m_memoryProperties.memoryHeapCount <= Pal::GpuHeapCount);
    }

    m_memoryTypeMaskForExternalSharing = m_memoryTypeMask;

    if (result == Pal::Result::Success)
    {
        // Determine if EQAA is supported by checking if, for each MSAA fragment count, all sample combos are okay.
        const auto& imgProps = PalProperties().imageProperties;
        m_eqaaSupported  = true;
        switch (imgProps.maxMsaaFragments)
        {
        default:
            VK_NEVER_CALLED();
            break;
        case 8:
            m_eqaaSupported &= Util::TestAllFlagsSet(imgProps.msaaSupport, Pal::MsaaFlags::MsaaAllF8);
            // fallthrough
        case 4:
            m_eqaaSupported &= Util::TestAllFlagsSet(imgProps.msaaSupport, Pal::MsaaFlags::MsaaAllF4);
            // fallthrough
        case 2:
            m_eqaaSupported &= Util::TestAllFlagsSet(imgProps.msaaSupport, Pal::MsaaFlags::MsaaAllF2);
            // fallthrough
        case 1:
            m_eqaaSupported &= Util::TestAllFlagsSet(imgProps.msaaSupport, Pal::MsaaFlags::MsaaAllF1);
            break;
        }
    }

    if (result == Pal::Result::Success)
    {
        // Get properties from PAL
        const Pal::DeviceProperties& palProps = PalProperties();

        // This UUID identifies whether a previously created pipeline cache is compatible with the currently installed
        // device/driver.
        constexpr uint16_t PalMajorVersion  = PAL_INTERFACE_MAJOR_VERSION;
        constexpr uint8_t  PalMinorVersion  = PAL_INTERFACE_MINOR_VERSION;

        constexpr char timestamp[]    = __DATE__ __TIME__;
        const uint32_t timestampHash  = Util::HashLiteralString<sizeof(timestamp)>(timestamp);

        memset(m_pipelineCacheUUID, 0, VK_UUID_SIZE);

        Util::SystemInfo systemInfo = {};

        result = Util::QuerySystemInfo(&systemInfo);
        if (result == Pal::Result::Success)
        {
            Util::MetroHash128 hash = {};
            hash.Update(timestampHash);
            hash.Update(palProps.vendorId);
            hash.Update(palProps.deviceId);
            hash.Update(PalMajorVersion);
            hash.Update(PalMinorVersion);
            hash.Update(systemInfo.cpuType);
            hash.Update(systemInfo.cpuVendorString[0]);
            hash.Update(systemInfo.cpuBrandString[0]);
            hash.Update(systemInfo.cpuLogicalCoreCount);
            hash.Update(systemInfo.cpuPhysicalCoreCount);
            hash.Update(systemInfo.totalSysMemSize);
            hash.Finalize(m_pipelineCacheUUID);
        }
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
        InitializePlatformKey(settings);
        vkResult = m_compiler.Initialize();
    }

    return vkResult;
}

// =====================================================================================================================
uint32_t PhysicalDevice::GetMemoryTypeMaskMatching(VkMemoryPropertyFlags flags) const
{
    uint32_t mask = 0;

    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < m_memoryProperties.memoryTypeCount; ++memoryTypeIndex)
    {
        if ((flags & m_memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags) == flags)
        {
            mask |= 1u << memoryTypeIndex;
        }
    }

    return mask;
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

    for (uint32_t i = 0; i < VK_SUPPORTED_FORMAT_COUNT; i++)
    {
        VkFormat format = Formats::FromIndex(i);

        VkFormatFeatureFlags linearFlags  = 0;
        VkFormatFeatureFlags optimalFlags = 0;
        VkFormatFeatureFlags bufferFlags  = 0;

        GetFormatFeatureFlags(fmtProperties, format, VK_IMAGE_TILING_LINEAR, &linearFlags, GetRuntimeSettings());
        GetFormatFeatureFlags(fmtProperties, format, VK_IMAGE_TILING_OPTIMAL, &optimalFlags, GetRuntimeSettings());

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

        if (Formats::IsYuvFormat(format))
        {
            if (IsExtensionSupported(DeviceExtensions::KHR_SAMPLER_YCBCR_CONVERSION))
            {
                linearFlags  |= VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT |
                                VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
                optimalFlags |= VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT |
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT |
                                VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
            }
        }

        linearFlags  &= AllImgFeatures;
        optimalFlags &= AllImgFeatures;
        bufferFlags  &= AllBufFeatures;

        if ((format == VK_FORMAT_R64_SINT) || (format == VK_FORMAT_R64_UINT))
        {
            memset(&m_formatFeaturesTable[i], 0, sizeof(VkFormatProperties));
        }
        else
        {
            m_formatFeaturesTable[i].bufferFeatures        = bufferFlags;
            m_formatFeaturesTable[i].linearTilingFeatures  = linearFlags;
            m_formatFeaturesTable[i].optimalTilingFeatures = optimalFlags;
        }

        // Vulkan doesn't have a corresponding flag for multisampling support.  If there ends up being more cases
        // like this, just store the entire PAL format table in the physical device instead of using a bitfield.
        const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(format);
        const size_t              formatIdx      = static_cast<size_t>(swizzledFormat.format);

        if (fmtProperties.features[formatIdx][Pal::IsNonLinear] & Pal::FormatFeatureMsaaTarget)
        {
            Util::WideBitfieldSetBit(m_formatFeatureMsaaTarget, i);
        }
    }

    // We should always support some kind of compressed format
    VK_ASSERT(VerifyBCFormatSupport(*this) || VerifyEtc2FormatSupport(*this) || VerifyAstcLdrFormatSupport(*this));
}

// =====================================================================================================================
// Determines which extensions are supported by this physical device.
void PhysicalDevice::PopulateExtensions()
{
    m_supportedExtensions = GetAvailableExtensions(VkInstance(), this);
    m_allowedExtensions = m_supportedExtensions;

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
    if (m_pPlatformKey != nullptr)
    {
        m_pPlatformKey->Destroy();
        VkInstance()->FreeMem(m_pPlatformKey);
    }

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
    uint32_t*                       pCount,
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
            switch (static_cast<uint32_t>(pHeader->sType))
            {
                case VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2:
                {
                    pQueueProps->queueFamilyProperties = m_queueFamilies[i].properties;
                    break;
                }
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
    pFeatures->alphaToOne                               =
        (PalProperties().gfxipProperties.flags.supportAlphaToOne ? VK_TRUE : VK_FALSE);
    pFeatures->multiViewport                            = VK_TRUE;
    pFeatures->samplerAnisotropy                        = VK_TRUE;
    pFeatures->textureCompressionETC2                   = VerifyEtc2FormatSupport(*this);
    pFeatures->textureCompressionASTC_LDR               = VerifyAstcLdrFormatSupport(*this);
    pFeatures->textureCompressionBC                     = VerifyBCFormatSupport(*this);
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
    pFeatures->shaderFloat64                            =
        (PalProperties().gfxipProperties.flags.support64BitInstructions ? VK_TRUE : VK_FALSE);
    pFeatures->shaderInt64                              =
        (PalProperties().gfxipProperties.flags.support64BitInstructions ? VK_TRUE : VK_FALSE);

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

    if (GetRuntimeSettings().optEnablePrt)
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
    const RuntimeSettings& settings = m_pSettingsLoader->GetSettings();

    Pal::SwizzledFormat palFormat = VkToPalFormat(format);

    // NOTE: BytesPerPixel obtained from PAL is per block not per pixel for compressed formats.  Therefore,
    //       maxResourceSize/maxExtent are also in terms of blocks for compressed formats.  I.e. we don't
    //       increase our exposed limits for compressed formats even though PAL/HW operating in terms of
    //       blocks makes that possible.
    const uint64_t bytesPerPixel = Pal::Formats::BytesPerPixel(palFormat.format);

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
        if (GetRuntimeSettings().optEnablePrt == false)
        {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const bool sparseBinding = m_properties.gpuMemoryProperties.flags.virtualRemappingSupport;
        if (!sparseBinding)
        {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        if (Formats::IsYuvFormat(format))
        {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        if (flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
        {
            // PAL doesn't expose all the information required to support a planar depth/stencil format
            if (Formats::IsDepthStencilFormat(format))
            {
                const bool sparseDepthStencil = (GetPrtFeatures() & Pal::PrtFeatureImageDepthStencil) != 0;
                if (!sparseDepthStencil)
                {
                    return VK_ERROR_FORMAT_NOT_SUPPORTED;
                }
            }

            const bool supported =
                // Currently we only support optimally tiled sparse images
                (tiling == VK_IMAGE_TILING_OPTIMAL)
                // Currently we don't support 1D sparse images
                && (type != VK_IMAGE_TYPE_1D)
                // 2D sparse images depend on HW capability
                && ((type != VK_IMAGE_TYPE_2D) || (GetPrtFeatures() & Pal::PrtFeatureImage2D))
                // 3D sparse images depend on HW capability
                && ((type != VK_IMAGE_TYPE_3D) ||
                ((GetPrtFeatures() & (Pal::PrtFeatureImage3D | Pal::PrtFeatureNonStandardImage3D)) != 0))
                // We only support pixel sizes not larger than 128 bits
                && (Util::Pow2Pad(bytesPerPixel) <= 16)
                // A combination of 3D image and 128-bit BC format is not supported.
                && (((type == VK_IMAGE_TYPE_3D) && (Util::Pow2Pad(bytesPerPixel) == 16) && Formats::IsBcCompressedFormat(format)) == false);

            if (supported == false)
            {
                return VK_ERROR_FORMAT_NOT_SUPPORTED;
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
    if ((FormatSupportsMsaa(format) == false)                                           ||
        (type != VK_IMAGE_TYPE_2D)                                                      ||
        (tiling == VK_IMAGE_TILING_LINEAR)                                              ||
        ((flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)                            ||
        ((supportedFeatures & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0))
    {
        pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    }
    else
    {
        pImageFormatProperties->sampleCounts = MaxSampleCountToSampleCountFlags(imageProps.maxMsaaFragments) &
                                               settings.limitSampleCounts;
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

    bool supported =
        // Multisampled sparse images depend on HW capability
        ((samples == VK_SAMPLE_COUNT_1_BIT) ||
         ((type == VK_IMAGE_TYPE_2D) && (GetPrtFeatures() & Pal::PrtFeatureImageMultisampled)))
        // Up to 16 MSAA coverage samples are supported by HW if EQAA is supported
        && (samples <= (m_eqaaSupported ? Pal::MaxMsaaRasterizerSamples
                                        : PalProperties().imageProperties.maxMsaaFragments));

    if (supported)
    {
        VkImageFormatProperties imageFormatProperties;
        supported = (GetImageFormatProperties(format,
                                              type,
                                              tiling,
                                              usage,
                                              (VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                                               | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT),
                                              &imageFormatProperties) == VK_SUCCESS);
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

VkResult PhysicalDevice::GetPhysicalDeviceCalibrateableTimeDomainsEXT(
    uint32_t*                           pTimeDomainCount,
    VkTimeDomainEXT*                    pTimeDomains)
{
    Pal::DeviceProperties deviceProperties = {};
    VkResult result = PalToVkResult(m_pPalDevice->GetProperties(&deviceProperties));
    VK_ASSERT(result == VK_SUCCESS);

    uint32_t timeDomainCount = Util::CountSetBits(deviceProperties.osProperties.timeDomains.u32All);

    if (pTimeDomains == nullptr)
    {
        *pTimeDomainCount = timeDomainCount;
    }
    else
    {
        *pTimeDomainCount = Util::Min(timeDomainCount, *pTimeDomainCount);

        uint32_t i = 0;

        if (deviceProperties.osProperties.timeDomains.supportDevice && (i < *pTimeDomainCount))
        {
            pTimeDomains[i] = VK_TIME_DOMAIN_DEVICE_EXT;
            ++i;
        }
        if (deviceProperties.osProperties.timeDomains.supportClockMonotonic && (i < *pTimeDomainCount))
        {
            pTimeDomains[i] = VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT;
            ++i;
        }
        if (deviceProperties.osProperties.timeDomains.supportClockMonotonicRaw && (i < *pTimeDomainCount))
        {
            pTimeDomains[i] = VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
            ++i;
        }
        if (deviceProperties.osProperties.timeDomains.supportQueryPerformanceCounter && (i < *pTimeDomainCount))
        {
            pTimeDomains[i] = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
            ++i;
        }

        result = ((timeDomainCount == *pTimeDomainCount) ? VK_SUCCESS : VK_INCOMPLETE);
    }

    return result;
}

// =====================================================================================================================
// Returns the API version supported by this device.
uint32_t PhysicalDevice::GetSupportedAPIVersion() const
{
    // Currently all of our HW supports Vulkan 1.2
    return (VK_API_VERSION_1_2 | VK_HEADER_VERSION);
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

    pProperties->apiVersion    = GetSupportedAPIVersion();

    static_assert(VULKAN_ICD_BUILD_VERSION < (1 << 12), "Radeon Settings UI diplays driverVersion using sizes 10.10.12 "
                                                        "like apiVersion, but our driverVersion uses 10.22. If this"
                                                        "assert ever triggers, verify that it and other driver info "
                                                        "tools that parse the raw value have been updated to avoid "
                                                        "any confusion.");
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

    memcpy(pProperties->pipelineCacheUUID, m_pipelineCacheUUID, VK_UUID_SIZE);

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

    Pal::PresentMode presentMode =
        (platform == VK_ICD_WSI_PLATFORM_DISPLAY)? Pal::PresentMode::Fullscreen : Pal::PresentMode::Windowed;

    return (engineProps.engineCount > 0) &&
           (m_pPalDevice->GetSupportedSwapChainModes(VkToPalWsiPlatform(platform), presentMode) != 0);

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
    const RuntimeSettings& settings = GetRuntimeSettings();

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
#if defined(__unix__)
    // relax the limitation on Linux since there is no real limitation from OS's perspective.
    m_limits.maxMemoryAllocationCount = UINT_MAX;
#else
    m_limits.maxMemoryAllocationCount = 4096;
#endif
    if (settings.memoryCustomDeviceAllocationCountLimit > 0)
    {
        m_limits.maxMemoryAllocationCount = settings.memoryCustomDeviceAllocationCountLimit;
    }

    // Maximum number of sampler objects
    // 1G - This limit was chosen heuristally. The vulkan CTS tests the limit we provide, which is a threoretical
    // limit and is dependent of the _system_ memory.
    m_limits.maxSamplerAllocationCount = 1048576;

    // Granularity, in bytes, at which buffers and images can be bound to adjacent memory for simultaneous usage.
    m_limits.bufferImageGranularity = 1;

    // Virtual memory address space size for sparse resources, which may be just the default VA range on some platforms
    m_limits.sparseAddressSpaceSize = palProps.gpuMemoryProperties.maxVirtualMemSize;

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
    m_limits.maxVertexInputBindings = Pal::MaxVertexBuffers;

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
    m_limits.maxComputeSharedMemorySize = palProps.gfxipProperties.shaderCore.ldsSizePerThreadGroup;

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
    m_limits.minTexelBufferOffsetAlignment = 4;

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
    m_limits.framebufferColorSampleCounts = MaxSampleCountToSampleCountFlags(palProps.imageProperties.maxMsaaFragments);

    // framebufferColorSampleCounts, framebufferDepthSampleCounts, framebufferStencilSampleCounts and
    // framebufferNoAttachmentSampleCounts are already clamped by the setting in GetImageFormatProperties() in
    // GetMaxFormatSampleCount() above. However, because the value of framebufferColorSampleCounts is
    // hardcoded above, we are limiting it according to the setting again.
    m_limits.framebufferColorSampleCounts &= settings.limitSampleCounts;

    m_sampleLocationSampleCounts = m_limits.framebufferColorSampleCounts;

    if (m_properties.gfxipProperties.flags.support1xMsaaSampleLocations == false)
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
        // Make sure that the sample count masks in the settings were not set when checking this.
        VK_ASSERT((settings.limitSampleCounts != 0xFFFFFFFF) || minSampledCount == 8);
        VK_ASSERT((settings.limitSampleCounts != 0xFFFFFFFF) || minSampledIntCount == 8);
        VK_ASSERT((settings.limitSampleCounts != 0xFFFFFFFF) || minSampledDepthCount == 8);
        VK_ASSERT((settings.limitSampleCounts != 0xFFFFFFFF) || minSampledStencilCount == 8);

        // Sample counts supported for all non-integer, integer, depth, and stencil sampled images, respectively
        m_limits.sampledImageColorSampleCounts      = MaxSampleCountToSampleCountFlags(minSampledCount);
        m_limits.sampledImageIntegerSampleCounts    = MaxSampleCountToSampleCountFlags(minSampledIntCount);
        m_limits.sampledImageDepthSampleCounts      = MaxSampleCountToSampleCountFlags(minSampledDepthCount);
        m_limits.sampledImageStencilSampleCounts    = MaxSampleCountToSampleCountFlags(minSampledStencilCount);

        // Sample counts supported for storage images
        m_limits.storageImageSampleCounts           = MaxSampleCountToSampleCountFlags(minStorageCount);
    }

    // Maximum number of components in the SampleMask or SampleMaskIn shader built-in.
    uint32_t maxCoverageSamples = (m_eqaaSupported ? 16 : palProps.imageProperties.maxMsaaFragments);

    m_limits.maxSampleMaskWords = (maxCoverageSamples + 32 - 1) / 32;

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
    VkSurfaceKHR             surface,
    Pal::OsDisplayHandle     displayHandle,
    T                        pSurfaceCapabilities
    ) const
{
    VkResult result = VK_SUCCESS;
    DisplayableSurfaceInfo displayableInfo = {};

    Surface* pSurface = Surface::ObjectFromHandle(surface);
    result = UnpackDisplayableSurface(pSurface, &displayableInfo);

    if (result == VK_SUCCESS)
    {
        if (displayHandle != 0)
        {
            VK_ASSERT(displayableInfo.displayHandle == 0);
            displayableInfo.displayHandle = displayHandle;
        }

        Pal::SwapChainProperties swapChainProperties = {};
#if defined(__unix__)
        if (displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY)
        {
            VkIcdSurfaceDisplay* pDisplaySurface     = pSurface->GetDisplaySurface();
            swapChainProperties.currentExtent.width  = pDisplaySurface->imageExtent.width;
            swapChainProperties.currentExtent.height = pDisplaySurface->imageExtent.height;
        }
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 610
            pSurfaceCapabilities->supportedCompositeAlpha =
                PalToVkSupportedCompositeAlphaMode(swapChainProperties.compositeAlphaMode);
#else
            pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif

            pSurfaceCapabilities->supportedTransforms = swapChainProperties.supportedTransforms;
            pSurfaceCapabilities->currentTransform = PalToVkSurfaceTransform(swapChainProperties.currentTransforms);

            pSurfaceCapabilities->supportedUsageFlags = PalToVkImageUsageFlags(swapChainProperties.supportedUsageFlags);

            if (std::is_same<T, VkSurfaceCapabilities2EXT*>::value)
            {
                // The capablility of surface counter is not supported until the VK_EXT_display_control is implemented.
                VkSurfaceCapabilities2EXT* pSurfaceCap2EXT = reinterpret_cast<VkSurfaceCapabilities2EXT*>(pSurfaceCapabilities);
                pSurfaceCap2EXT->supportedSurfaceCounters  = 0;
            }
        }
    }

    return result;
}

// Instantiate the template for the linker.
template
VkResult PhysicalDevice::GetSurfaceCapabilities<VkSurfaceCapabilitiesKHR*>(
    VkSurfaceKHR              surface,
    Pal::OsDisplayHandle      displayHandle,
    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) const;

template
VkResult PhysicalDevice::GetSurfaceCapabilities<VkSurfaceCapabilities2EXT*>(
    VkSurfaceKHR              surface,
    Pal::OsDisplayHandle      displayHandle,
    VkSurfaceCapabilities2EXT* pSurfaceCapabilities) const;

// =====================================================================================================================
VkResult PhysicalDevice::GetSurfaceCapabilities2KHR(
    const VkPhysicalDeviceSurfaceInfo2KHR*  pSurfaceInfo,
    VkSurfaceCapabilities2KHR*              pSurfaceCapabilities) const
{
    union
    {
        const VkStructHeader*                     pHeader;
        const VkPhysicalDeviceSurfaceInfo2KHR*    pVkPhysicalDeviceSurfaceInfo2KHR;
        VkSurfaceCapabilities2KHR*                pVkSurfaceCapabilities2KHR;
        VkHdrMetadataEXT*                         pVkHdrMetadataEXT;
    };

    VkResult                 result                    = VK_SUCCESS;
    bool                     fullScreenExplicitEnabled = false;
    Pal::OsDisplayHandle     displayHandle             = 0;
    VkSurfaceKHR             surface                   = VK_NULL_HANDLE;

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
                result = GetSurfaceCapabilities(
                            surface,
                            displayHandle,
                            &pVkSurfaceCapabilities2KHR->surfaceCapabilities);
                break;
            }

            case VK_STRUCTURE_TYPE_HDR_METADATA_EXT:
            {
                VkHdrMetadataEXT& vkMetadata = *pVkHdrMetadataEXT;

                Surface* pSurface = Surface::ObjectFromHandle(surface);

                DisplayableSurfaceInfo displayableInfo = {};

                result = PhysicalDevice::UnpackDisplayableSurface(pSurface, &displayableInfo);

                VK_ASSERT(displayableInfo.icdPlatform == VK_ICD_WSI_PLATFORM_DISPLAY);

                Pal::IScreen* pPalScreen = displayableInfo.pScreen;
                if (pPalScreen != nullptr)
                {
                    Pal::ScreenColorCapabilities screenCaps;
                    Pal::Result palResult = pPalScreen->GetColorCapabilities(&screenCaps);
                    VK_ASSERT(palResult == Pal::Result::Success);

                    const Pal::ColorGamut& colorGamut = screenCaps.nativeColorGamut;

                    constexpr double scale             = 1.0 / 50000.0;
                    constexpr double minLuminanceScale = 1.0 / 10000.0;

                    vkMetadata.displayPrimaryRed.x       = static_cast<float>(colorGamut.chromaticityRedX        * scale);
                    vkMetadata.displayPrimaryRed.y       = static_cast<float>(colorGamut.chromaticityRedY        * scale);
                    vkMetadata.displayPrimaryGreen.x     = static_cast<float>(colorGamut.chromaticityGreenX      * scale);
                    vkMetadata.displayPrimaryGreen.y     = static_cast<float>(colorGamut.chromaticityGreenY      * scale);
                    vkMetadata.displayPrimaryBlue.x      = static_cast<float>(colorGamut.chromaticityBlueX       * scale);
                    vkMetadata.displayPrimaryBlue.y      = static_cast<float>(colorGamut.chromaticityBlueY       * scale);
                    vkMetadata.whitePoint.x              = static_cast<float>(colorGamut.chromaticityWhitePointX * scale);
                    vkMetadata.whitePoint.y              = static_cast<float>(colorGamut.chromaticityWhitePointY * scale);

                    vkMetadata.minLuminance = static_cast<float>(colorGamut.minLuminance * minLuminanceScale);
                    vkMetadata.maxLuminance              = static_cast<float>(colorGamut.maxLuminance);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 512
                    vkMetadata.maxFrameAverageLightLevel = static_cast<float>(colorGamut.maxFrameAverageLightLevel);
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 506
                    vkMetadata.maxContentLightLevel      = static_cast<float>(colorGamut.maxContentLightLevel);
#endif
                }
                else
                {
                    // Standard Red Green Blue.
                    vkMetadata.displayPrimaryRed.x       = 0.6400f;
                    vkMetadata.displayPrimaryRed.y       = 0.3300f;
                    vkMetadata.displayPrimaryGreen.x     = 0.3000f;
                    vkMetadata.displayPrimaryGreen.y     = 0.6000f;
                    vkMetadata.displayPrimaryBlue.x      = 0.1500f;
                    vkMetadata.displayPrimaryBlue.y      = 0.0600f;
                    vkMetadata.whitePoint.x              = 0.3127f;
                    vkMetadata.whitePoint.y              = 0.3290f;
                    vkMetadata.minLuminance              = 0.0f;
                    vkMetadata.maxLuminance              = 0.0f;
                    vkMetadata.maxFrameAverageLightLevel = 0.0f;
                    vkMetadata.maxContentLightLevel      = 0.0f;
                }
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
// Note:
//  DirectDisplay platform has only fullscreen mode.
//  Win32 fullscreen provides additional fifo relaxed mode,
//  it will fallback to fifo for windowed mode.
VkResult PhysicalDevice::GetSurfacePresentModes(
    const DisplayableSurfaceInfo& displayableInfo,
    Pal::PresentMode              presentType,
    uint32_t*                     pPresentModeCount,
    VkPresentModeKHR*             pPresentModes
) const
{
    VkPresentModeKHR presentModes[4] = {};
    uint32_t modeCount = 0;

    // Get which swap chain modes are supported for the given present type (windowed vs fullscreen)
    uint32_t swapChainModes = 0;
    if (presentType == Pal::PresentMode::Count)
    {
        swapChainModes  = m_pPalDevice->GetSupportedSwapChainModes(displayableInfo.palPlatform, Pal::PresentMode::Windowed);
        swapChainModes |= m_pPalDevice->GetSupportedSwapChainModes(displayableInfo.palPlatform, Pal::PresentMode::Fullscreen);
    }
    else
    {
        swapChainModes = m_pPalDevice->GetSupportedSwapChainModes(displayableInfo.palPlatform, presentType);
    }

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

    // Write out information
    VkResult result = VK_SUCCESS;

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

    return result;
}

// =====================================================================================================================
// Retrieve display and window handles from the VKSurface object
VkResult PhysicalDevice::UnpackDisplayableSurface(
    Surface*                pSurface,
    DisplayableSurfaceInfo* pInfo)
{
    VkResult result = VK_SUCCESS;

#if defined(__unix__)
    if (pSurface->GetDisplaySurface()->base.platform == VK_ICD_WSI_PLATFORM_DISPLAY)
    {
        VkIcdSurfaceDisplay* pDisplaySurface = pSurface->GetDisplaySurface();
        pInfo->icdPlatform   = pDisplaySurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pDisplaySurface->base.platform);
        pInfo->surfaceExtent = pDisplaySurface->imageExtent;
        DisplayModeObject* pDisplayMode = reinterpret_cast<DisplayModeObject*>(pDisplaySurface->displayMode);
        pInfo->pScreen       = pDisplayMode->pScreen;
    }
#ifdef VK_USE_PLATFORM_XCB_KHR
    else if (pSurface->GetXcbSurface()->base.platform == VK_ICD_WSI_PLATFORM_XCB)
    {
        const VkIcdSurfaceXcb* pXcbSurface = pSurface->GetXcbSurface();

        pInfo->icdPlatform   = pXcbSurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pXcbSurface->base.platform);
        pInfo->displayHandle = pXcbSurface->connection;
        pInfo->windowHandle.win  = pXcbSurface->window;
    }
#endif
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
#ifdef VK_USE_PLATFORM_XLIB_KHR
    else if (pSurface->GetXlibSurface()->base.platform == VK_ICD_WSI_PLATFORM_XLIB)
    {
        const VkIcdSurfaceXlib* pXlibSurface = pSurface->GetXlibSurface();

        pInfo->icdPlatform   = pXlibSurface->base.platform;
        pInfo->palPlatform   = VkToPalWsiPlatform(pXlibSurface->base.platform);
        pInfo->displayHandle = pXlibSurface->dpy;
        pInfo->windowHandle.win  = pXlibSurface->window;
    }
#endif
    else
    {
        result = VK_ERROR_SURFACE_LOST_KHR;
    }
#endif

    return result;
}

// =====================================================================================================================
// Returns the presentable image formats we support for both windowed and fullscreen modes
VkResult PhysicalDevice::GetSurfaceFormats(
    Surface*             pSurface,
    Pal::OsDisplayHandle osDisplayHandle,
    uint32_t*            pSurfaceFormatCount,
    VkSurfaceFormatKHR*  pSurfaceFormats) const
{
    VkResult result = VK_SUCCESS;

    uint32_t numPresentFormats = 0;
    const uint32_t maxBufferCount = (pSurfaceFormats != nullptr) ? *pSurfaceFormatCount : 0;

    DisplayableSurfaceInfo displayableInfo = {};

    if (pSurface != nullptr)
    {
        // If this fails for any reason, we should end up with a null handle and
        // eventually a nullscreen that will get handled below.
        result = PhysicalDevice::UnpackDisplayableSurface(pSurface, &displayableInfo);
        VK_ASSERT(result == VK_SUCCESS);
    }

    Pal::ScreenColorCapabilities palColorCaps = {};

    bool isWindowed = false;

    Pal::IScreen* pScreen = displayableInfo.pScreen;
    isWindowed = (displayableInfo.icdPlatform != VK_ICD_WSI_PLATFORM_DISPLAY);

    if (pScreen != nullptr)
    {
        Pal::Result palResult = pScreen->GetColorCapabilities(&palColorCaps);
        VK_ASSERT(palResult == Pal::Result::Success);
    }

    bool needsWorkaround = pScreen == nullptr ? isWindowed :
                           (palColorCaps.supportedColorSpaces == Pal::ScreenColorSpace::TfUndefined);

    // This workaround is needed on Windows in cases where we get a valid screen object but it has
    // no valid display properties. This scenario can happen when running apps without a display connected.
    if (needsWorkaround)
    {
        // The w/a here will be removed once more presentable format is supported on base driver side.
        const VkSurfaceFormatKHR formatList[] = {
            { VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
            { VK_FORMAT_B8G8R8A8_SRGB,  VK_COLORSPACE_SRGB_NONLINEAR_KHR }
        };
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

            *pSurfaceFormatCount = count;
        }
    }
    else if (pScreen == nullptr)
    {
        // Error out if pScreen was null on fullscreen request.
        if (pSurfaceFormats == nullptr)
        {
            *pSurfaceFormatCount = 0;
            result = VK_SUCCESS;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    else
    {
        uint32_t colorSpaceCount = 0;
        uint32_t numImgFormats = 0;

        // Enumerate
        ColorSpaceHelper::GetSupportedFormats(palColorCaps.supportedColorSpaces, &colorSpaceCount, nullptr);
        Pal::Result palResult = pScreen->GetFormats(&numImgFormats, nullptr);
        VK_ASSERT(palResult == Pal::Result::Success);
        const size_t totalMem = (sizeof(Pal::SwizzledFormat)        * numImgFormats) +
                                (sizeof(VkFormat)                   * numImgFormats) +
                                (sizeof(vk::ColorSpaceHelper::Fmts) * colorSpaceCount);

        // Allocate
        void* pAllocMem = VkInstance()->AllocMem(totalMem, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        if (pAllocMem == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        // Populate
        Pal::SwizzledFormat* pPalFormats = static_cast<Pal::SwizzledFormat*>(pAllocMem);

        palResult = pScreen->GetFormats(&numImgFormats, pPalFormats);
        VK_ASSERT(palResult == Pal::Result::Success);

        VkFormat* pVkFormats = reinterpret_cast<VkFormat*>(pPalFormats + numImgFormats);
        memset(pVkFormats, 0, sizeof(VkFormat)* numImgFormats);

        vk::ColorSpaceHelper::Fmts* pColorSpaces =
            reinterpret_cast<vk::ColorSpaceHelper::Fmts*>(pVkFormats + numImgFormats);

        Pal::MergedFormatPropertiesTable formatProperties = {};
        palResult = m_pPalDevice->GetFormatProperties(&formatProperties);
        VK_ASSERT(palResult == Pal::Result::Success);

        Util::Vector<VkFormat, 32, PalAllocator>  windowedFormats(VkInstance()->Allocator());

        for (uint32_t vkFmtIdx = VK_FORMAT_BEGIN_RANGE; vkFmtIdx <= VK_FORMAT_END_RANGE; vkFmtIdx++)
        {
            bool isFullscreenFormat = false;
            const Pal::SwizzledFormat cmpFormat = VkToPalFormat(static_cast<VkFormat>(vkFmtIdx));

            for (uint32_t fmtIndx = 0; fmtIndx < numImgFormats; fmtIndx++)
            {
                const Pal::SwizzledFormat srcFormat = pPalFormats[fmtIndx];

                if ((srcFormat.format == cmpFormat.format) &&
                    (srcFormat.swizzle.r == cmpFormat.swizzle.r) &&
                    (srcFormat.swizzle.g == cmpFormat.swizzle.g) &&
                    (srcFormat.swizzle.b == cmpFormat.swizzle.b) &&
                    (srcFormat.swizzle.a == cmpFormat.swizzle.a))
                {
                    pVkFormats[fmtIndx] = static_cast<VkFormat>(vkFmtIdx);
                    isFullscreenFormat = true;
                    break;
                }
            }

            const Pal::FormatFeatureFlags formatBits =
                formatProperties.features[static_cast<size_t>(cmpFormat.format)][Pal::IsLinear];

            if ((isFullscreenFormat == false) && ((formatBits & Pal::FormatFeatureWindowedPresent) != 0))
            {
                windowedFormats.PushBack(static_cast<VkFormat>(vkFmtIdx));
            }
        }

        ColorSpaceHelper::GetSupportedFormats(palColorCaps.supportedColorSpaces, &colorSpaceCount, pColorSpaces);

        const RuntimeSettings& settings = GetSettingsLoader()->GetSettings();
        // Report HDR in windowed mode only if OS is in HDR mode. Always report on fullscreen
        bool reportHdrSupport = (isWindowed == false) || palColorCaps.isHdrEnabled || settings.alwaysReportHdrFormats;

        // First add all the fullscreen formats, with supported colorspaces, we keep the windowed
        // check here cause fullscereen formats may support windowed presents.
        for (uint32_t colorSpaceIndex = 0; colorSpaceIndex < colorSpaceCount; colorSpaceIndex++)
        {
            const VkColorSpaceKHR              colorSpaceFmt = pColorSpaces[colorSpaceIndex].colorSpace;
            const ColorSpaceHelper::FmtSupport bitSupport    = pColorSpaces[colorSpaceIndex].fmtSupported;

            if (ColorSpaceHelper::IsFmtHdr(pColorSpaces[colorSpaceIndex]) && (reportHdrSupport == false))
            {
                // Goto next color space if we don't want to report HDR
                continue;
            }

            for (uint32_t fmtIndx = 0; fmtIndx < numImgFormats; fmtIndx++)
            {
                const Pal::FormatFeatureFlags formatBits =
                    formatProperties.features[static_cast<size_t>(pPalFormats[fmtIndx].format)][Pal::IsLinear];

                if (ColorSpaceHelper::IsFormatColorSpaceCompatible(pPalFormats[fmtIndx].format, bitSupport) &&
                    ((isWindowed == false) || (formatBits & Pal::FormatFeatureWindowedPresent) != 0))
                {
                    if (pSurfaceFormats != nullptr)
                    {
                        if (numPresentFormats < maxBufferCount)
                        {
                            pSurfaceFormats[numPresentFormats].format = pVkFormats[fmtIndx];
                            pSurfaceFormats[numPresentFormats].colorSpace = colorSpaceFmt;
                        }
                        else
                        {
                            result = VK_INCOMPLETE;
                            break;
                        }
                    }
                    ++numPresentFormats;
                }
            }
        }

        // Add all windowed formats
        if (isWindowed)
        {
            if (pSurfaceFormats != nullptr)
            {
                for (uint32_t i = 0; i < windowedFormats.NumElements(); i++)
                {
                    if (numPresentFormats < maxBufferCount)
                    {
                        pSurfaceFormats[numPresentFormats].format = windowedFormats.At(i);
                        pSurfaceFormats[numPresentFormats].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                    }
                    else
                    {
                        result = VK_INCOMPLETE;
                        break;
                    }
                    ++numPresentFormats;
                }
            }
            else
            {
                numPresentFormats += windowedFormats.NumElements();
            }
        }

        if (pSurfaceFormatCount != nullptr)
        {
            *pSurfaceFormatCount = numPresentFormats;
        }

        VkInstance()->FreeMem(pAllocMem);
    }

    return result;
}

// =====================================================================================================================
// called in response to vkGetPhysicalDeviceSurfaceFormats2KHR
VkResult PhysicalDevice::GetSurfaceFormats(
    Surface*             pSurface,
    Pal::OsDisplayHandle osDisplayHandle,
    uint32_t*            pSurfaceFormatCount,
    VkSurfaceFormat2KHR* pSurfaceFormats) const
{
    VkResult result = VK_SUCCESS;
    if (pSurfaceFormats == nullptr)
    {
        result = GetSurfaceFormats(
            pSurface,
            osDisplayHandle,
            pSurfaceFormatCount,
            static_cast<VkSurfaceFormatKHR*>(nullptr));
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

        result = GetSurfaceFormats(pSurface, osDisplayHandle, pSurfaceFormatCount, pTempSurfaceFormats);

        for (uint32_t i = 0; i < *pSurfaceFormatCount; i++)
        {
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
    VK_ASSERT(pRectCount != nullptr);
    VkResult result = VK_SUCCESS;

    if (pRects != nullptr)
    {
        if (*pRectCount > 0)
        {
            Surface* pSurface = Surface::ObjectFromHandle(surface);

            Pal::OsDisplayHandle     osDisplayHandle     = 0;
            VkSurfaceCapabilitiesKHR surfaceCapabilities = {};

            result = GetSurfaceCapabilities(
                surface, osDisplayHandle, &surfaceCapabilities);

            if (result == VK_SUCCESS)
            {
                // TODO: We don't support VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR
                //       so just return a single rect matching the surface.
                VK_ASSERT(pRects != nullptr);
                pRects[0].offset.x = 0;
                pRects[0].offset.y = 0;
                pRects[0].extent   = surfaceCapabilities.currentExtent;

                *pRectCount = 1;
            }
        }
        else
        {
            result = VK_INCOMPLETE;
        }
    }
    else
    {
        *pRectCount = 1;
    }

    return result;
}

// =====================================================================================================================
static bool IsConditionalRenderingSupported(
    const PhysicalDevice* pPhysicalDevice)
{
    bool isSupported = false;

    if (isSupported && (pPhysicalDevice != nullptr))
    {
        // Conditional rendering must be supported on all exposed graphics and compute queue types.
        for (uint32_t engineType = 0; engineType < Pal::EngineTypeCount; engineType++)
        {
            const auto& engineProps = pPhysicalDevice->PalProperties().engineProperties[engineType];

            if ((engineProps.queueSupport & (Pal::SupportQueueTypeUniversal | Pal::SupportQueueTypeCompute)) &&
                (engineProps.flags.supports32bitMemoryPredication == 0))
            {
                isSupported = false;
                break;
            }
        }
    }

    return isSupported;
}

// =====================================================================================================================
static bool IsSingleChannelMinMaxFilteringSupported(
    const PhysicalDevice* pPhysicalDevice)
{
#if VK_IS_PAL_VERSION_AT_LEAST(560, 1)
    return ((pPhysicalDevice == nullptr) ||
            pPhysicalDevice->PalProperties().gfxipProperties.flags.supportSingleChannelMinMaxFilter);
#else
    return true;
#endif
}

// =====================================================================================================================

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
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DRAW_INDIRECT_COUNT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_SUBGROUP_BALLOT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_SUBGROUP_VOTE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_STENCIL_EXPORT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_VIEWPORT_INDEX_LAYER));

    if (pInstance->IsExtensionSupported(InstanceExtensions::KHR_DEVICE_GROUP_CREATION))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DEVICE_GROUP));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_BIND_MEMORY2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DEDICATED_ALLOCATION));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DESCRIPTOR_UPDATE_TEMPLATE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_MEMORY));
#if defined(__unix__)
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_MEMORY_FD));
#endif

    if (pInstance->IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_SEMAPHORE_CAPABILITIES))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_SEMAPHORE));
#if defined(__unix__)
        if ((pPhysicalDevice == nullptr) ||
            (pPhysicalDevice->PalProperties().osProperties.supportOpaqueFdSemaphore))
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_SEMAPHORE_FD));
        }
#endif
    }
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_GET_MEMORY_REQUIREMENTS2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE1));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE2));

    if (IsSingleChannelMinMaxFilteringSupported(pPhysicalDevice))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SAMPLER_FILTER_MINMAX));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MAINTENANCE3));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_RELAXED_BLOCK_LAYOUT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_IMAGE_FORMAT_LIST));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SWAPCHAIN_MUTABLE_FORMAT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_8BIT_STORAGE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_ATOMIC_INT64));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DRIVER_PROPERTIES));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_FLOAT_CONTROLS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_CREATE_RENDERPASS2));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_CALIBRATED_TIMESTAMPS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_HDR_METADATA));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SAMPLE_LOCATIONS));

    // If RGP tracing is enabled, report support for VK_EXT_debug_marker extension since RGP traces can trap
    // application-provided debug markers and visualize them in RGP traces.
    if (pInstance->IsTracingSupportEnabled())
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DEBUG_MARKER));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_STORAGE_BUFFER_STORAGE_CLASS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_16BIT_STORAGE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_DEPTH_STENCIL_RESOLVE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_INLINE_UNIFORM_BLOCK));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_FLOAT16_INT8));

    if ((pPhysicalDevice == nullptr) ||
        (pPhysicalDevice->PalProperties().osProperties.supportQueuePriority))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_GLOBAL_PRIORITY));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_FENCE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_EXTERNAL_FENCE_FD));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_MULTIVIEW));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_TEXEL_BUFFER_ALIGNMENT));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_EXTERNAL_MEMORY_HOST));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DEPTH_CLIP_ENABLE));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DEPTH_RANGE_UNRESTRICTED));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_QUEUE_FAMILY_FOREIGN));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_DESCRIPTOR_INDEXING));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_VARIABLE_POINTERS));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_VERTEX_ATTRIBUTE_DIVISOR));

    if ((pPhysicalDevice == nullptr) ||
        (pPhysicalDevice->PalProperties().gfxipProperties.flags.supportConservativeRasterization &&
            pInstance->IsExtensionSupported(InstanceExtensions::KHR_GET_PHYSICAL_DEVICE_PROPERTIES2)))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_CONSERVATIVE_RASTERIZATION));
    }

#if defined(__unix__)
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_PCI_BUS_INFO));
#endif
    if ((pPhysicalDevice == nullptr) ||
        pPhysicalDevice->PalProperties().osProperties.timelineSemaphore.support)
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_TIMELINE_SEMAPHORE));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_CLOCK));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SPIRV_1_4));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(GOOGLE_HLSL_FUNCTIONALITY1));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(GOOGLE_DECORATE_STRING));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SCALAR_BLOCK_LAYOUT));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_MEMORY_PRIORITY));
    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_MEMORY_BUDGET));

    if ((pPhysicalDevice == nullptr) || pPhysicalDevice->PalProperties().gfxipProperties.flags.supportPostDepthCoverage)
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_POST_DEPTH_COVERAGE));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_TRANSFORM_FEEDBACK));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SEPARATE_STENCIL_USAGE));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_VULKAN_MEMORY_MODEL));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_PIPELINE_CREATION_CACHE_CONTROL));

    if (IsConditionalRenderingSupported(pPhysicalDevice))
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_CONDITIONAL_RENDERING));
    }

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_HOST_QUERY_RESET));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_UNIFORM_BUFFER_STANDARD_LAYOUT));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_SUBGROUP_EXTENDED_TYPES));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_SUBGROUP_SIZE_CONTROL));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_LINE_RASTERIZATION));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_IMAGELESS_FRAMEBUFFER));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(EXT_PIPELINE_CREATION_FEEDBACK));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_PIPELINE_EXECUTABLE_PROPERTIES));

    availableExtensions.AddExtension(VK_DEVICE_EXTENSION(KHR_SHADER_NON_SEMANTIC_INFO));

#if defined(__unix__)
#endif

    bool disableAMDVendorExtensions = false;
    if (pPhysicalDevice != nullptr)
    {
        const RuntimeSettings& settings = pPhysicalDevice->GetRuntimeSettings();
        disableAMDVendorExtensions = settings.disableAMDVendorExtensions;
    }

    // AMD Extensions
    if (!disableAMDVendorExtensions)
    {
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_TRINARY_MINMAX));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_EXPLICIT_VERTEX_PARAMETER));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GCN_SHADER));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_BALLOT));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_DRAW_INDIRECT_COUNT));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_IMAGE_LOAD_STORE_LOD));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_INFO));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_FRAGMENT_MASK));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_TEXTURE_GATHER_BIAS_LOD));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPA_INTERFACE));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_BUFFER_MARKER));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_CORE_PROPERTIES));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_SHADER_CORE_PROPERTIES2));
        availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_MEMORY_OVERALLOCATION_BEHAVIOR));

        if ((pPhysicalDevice == nullptr) || pPhysicalDevice->m_eqaaSupported)
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_MIXED_ATTACHMENT_SAMPLES));
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 493
        if ((pPhysicalDevice == nullptr) ||
            (pPhysicalDevice->PalProperties().gfxipProperties.flags.supportOutOfOrderPrimitives))
#endif
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_RASTERIZATION_ORDER));
        }

        // Don't report VK_AMD_negative_viewport_height in Vulkan 1.1, it must not be used.
        if (pInstance->GetAPIVersion() < VK_MAKE_VERSION(1, 1, 0))
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_NEGATIVE_VIEWPORT_HEIGHT));
        }

        if ((pPhysicalDevice == nullptr) ||
            ((pPhysicalDevice->PalProperties().gfxipProperties.flags.support16BitInstructions) &&
                ((pPhysicalDevice->GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
                    (pPhysicalDevice->PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9))))
        {
            // Deprecation by shaderFloat16 from VK_KHR_shader_float16_int8
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPU_SHADER_HALF_FLOAT));
        }

        if ((pPhysicalDevice == nullptr) ||
            ((pPhysicalDevice->PalProperties().gfxipProperties.flags.support16BitInstructions) &&
                ((pPhysicalDevice->GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
                    (pPhysicalDevice->PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9))))
        {
            // Deprecation by shaderFloat16 from VK_KHR_shader_float16_int8 and shaderInt16
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_GPU_SHADER_INT16));
        }

        if ((pPhysicalDevice == nullptr) ||
            (pPhysicalDevice->PalProperties().gfxipProperties.flags.supportGl2Uncached))
        {
            availableExtensions.AddExtension(VK_DEVICE_EXTENSION(AMD_DEVICE_COHERENT_MEMORY));
        }

    }

    return availableExtensions;
}

// =====================================================================================================================
// Is the queue suitable for normal use (i.e. non-exclusive and no elevated priority).
template<class T>
VK_INLINE static bool IsNormalQueue(const T& engineCapabilities)
{
    return ((engineCapabilities.flags.exclusive == 0) &&
            (((engineCapabilities.queuePrioritySupport & Pal::QueuePrioritySupport::SupportQueuePriorityNormal) != 0) ||
             (engineCapabilities.queuePrioritySupport == 0)));
}

// =====================================================================================================================
// Populates the device queue families. Note that there's not a one-to-one association between PAL queue types and
// Vulkan queue families due to many reasons:
// - We statically don't expose all PAL queue types
// - We dynamically don't expose PAL queue types that don't have the associated extension/feature enabled
// - We dynamically don't expose PAL queue types that don't have any queues present on the device
void PhysicalDevice::PopulateQueueFamilies()
{
    static uint32_t vkQueueFlags[] =
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
        // Pal::EngineTypeDma
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT,
        // Pal::EngineTypeTimer
        0,
    };

    // While it's possible for an engineType to support multiple queueTypes,
    // we'll simplify things by associating each engineType with a primary queueType.
    static const Pal::QueueType palQueueTypes[] =
    {
        Pal::QueueTypeUniversal,
        Pal::QueueTypeCompute,
        Pal::QueueTypeDma,
        Pal::QueueTypeTimer,

    };

    static_assert((VK_ARRAY_SIZE(vkQueueFlags) == Pal::EngineTypeCount) &&
                  (VK_ARRAY_SIZE(palQueueTypes) == Pal::EngineTypeCount) &&
                  (Pal::EngineTypeUniversal        == 0) &&
                  (Pal::EngineTypeCompute          == 1) &&
                  (Pal::EngineTypeDma              == 2) &&
                  (Pal::EngineTypeTimer            == 3)
        , "PAL engine types have changed, need to update the tables above");

    // Always enable core queue flags.  Final determination of support will be done on a per-engine basis.
    uint32_t enabledQueueFlags =
        VK_QUEUE_GRAPHICS_BIT |
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT |
        VK_QUEUE_SPARSE_BINDING_BIT;

    VkBool32 protectedMemorySupported = VK_FALSE;
    GetPhysicalDeviceProtectedMemoryFeatures(&protectedMemorySupported);

    if (protectedMemorySupported)
    {
        vkQueueFlags[Pal::EngineTypeUniversal] |= VK_QUEUE_PROTECTED_BIT;
        vkQueueFlags[Pal::EngineTypeCompute]   |= VK_QUEUE_PROTECTED_BIT;
        vkQueueFlags[Pal::EngineTypeDma]       |= VK_QUEUE_PROTECTED_BIT;
        enabledQueueFlags                      |= VK_QUEUE_PROTECTED_BIT;
    }

    // find out the sub engine index of VrHighPriority and indices for compute engines that aren't exclusive.
    {
        const auto& computeProps = m_properties.engineProperties[Pal::EngineTypeCompute];
        uint32_t engineIndex = 0u;
        for (uint32_t subEngineIndex = 0; subEngineIndex < computeProps.engineCount; subEngineIndex++)
        {
            if (computeProps.capabilities[subEngineIndex].flags.exclusive == 1)
            {
                if (computeProps.capabilities[subEngineIndex].queuePrioritySupport &
                         Pal::QueuePrioritySupport::SupportQueuePriorityRealtime)
                {
                    m_RtCuHighComputeSubEngineIndex = subEngineIndex;
                }
                else if (computeProps.capabilities[subEngineIndex].queuePrioritySupport &
                    Pal::QueuePrioritySupport::SupportQueuePriorityHigh)
                {
                    m_vrHighPrioritySubEngineIndex = subEngineIndex;
                }
            }
            else if (IsNormalQueue(computeProps.capabilities[subEngineIndex]))
            {
                m_compQueueEnginesNdx[engineIndex++] = subEngineIndex;
            }
        }
    }

    // find out universal engines that aren't exclusive.
    {
        const auto& universalProps = m_properties.engineProperties[Pal::EngineTypeUniversal];
        uint32_t engineIndex = 0u;
        for (uint32_t subEngineIndex = 0; subEngineIndex < universalProps.engineCount; subEngineIndex++)
        {
            if (IsNormalQueue(universalProps.capabilities[subEngineIndex]))
            {
                m_universalQueueEnginesNdx[engineIndex++] = subEngineIndex;
            }
        }
    }

    // Remember the following lookups for later.
    VkQueueFamilyProperties* pTransferQueueFamilyProperties = nullptr;
    VkQueueFamilyProperties* pComputeQueueFamilyProperties  = nullptr;

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

        // Vulkan requires a protected capable queue to support both protected and unprotected submissions.
        if (protectedMemorySupported                                        &&
           ((engineProps.tmzSupportLevel == Pal::TmzSupportLevel::PerQueue) ||
            (engineProps.tmzSupportLevel == Pal::TmzSupportLevel::None)))
        {
            supportedQueueFlags &= ~VK_QUEUE_PROTECTED_BIT;
        }

        if ((engineProps.engineCount != 0) && ((vkQueueFlags[engineType] & supportedQueueFlags) != 0))
        {
            m_queueFamilies[m_queueFamilyCount].palEngineType = static_cast<Pal::EngineType>(engineType);

            const Pal::QueueType primaryQueueType = palQueueTypes[GetQueueFamilyPalEngineType(m_queueFamilyCount)];
            VK_ASSERT((engineProps.queueSupport & (1 << primaryQueueType)) != 0);
            m_queueFamilies[m_queueFamilyCount].palQueueType = primaryQueueType;

            uint32_t palImageLayoutFlag = 0;
            uint32_t transferGranularityOverride = 0;

            m_queueFamilies[m_queueFamilyCount].validShaderStages = 0;

            const RuntimeSettings& settings = GetRuntimeSettings();

            switch (engineType)
            {
            case Pal::EngineTypeUniversal:
                palImageLayoutFlag            = Pal::LayoutUniversalEngine;
                transferGranularityOverride   = settings.transferGranularityUniversalOverride;
                m_queueFamilies[m_queueFamilyCount].validShaderStages = VK_SHADER_STAGE_ALL_GRAPHICS |
                                                                        VK_SHADER_STAGE_COMPUTE_BIT;
                break;
            case Pal::EngineTypeCompute:
                pComputeQueueFamilyProperties = &m_queueFamilies[m_queueFamilyCount].properties;
                palImageLayoutFlag            = Pal::LayoutComputeEngine;
                transferGranularityOverride   = settings.transferGranularityComputeOverride;
                m_queueFamilies[m_queueFamilyCount].validShaderStages |= VK_SHADER_STAGE_COMPUTE_BIT;
                break;
            case Pal::EngineTypeDma:
                pTransferQueueFamilyProperties = &m_queueFamilies[m_queueFamilyCount].properties;
                palImageLayoutFlag             = Pal::LayoutDmaEngine;
                transferGranularityOverride    = settings.transferGranularityDmaOverride;
#if VK_IS_PAL_VERSION_AT_LEAST(465, 2)
                m_prtOnDmaSupported            = engineProps.flags.supportsUnmappedPrtPageAccess;
#else
                m_prtOnDmaSupported            = true;
#endif
                break;
            default:
                break; // no-op
            }

            m_queueFamilies[m_queueFamilyCount].palImageLayoutFlag = palImageLayoutFlag;

            VkQueueFamilyProperties* pQueueFamilyProps     = &m_queueFamilies[m_queueFamilyCount].properties;

            pQueueFamilyProps->queueFlags                  = (vkQueueFlags[engineType] & supportedQueueFlags);
            pQueueFamilyProps->queueCount                  = 0u;

            for (uint32 engineNdx = 0u; engineNdx < engineProps.engineCount; ++engineNdx)
            {
                if (IsNormalQueue(engineProps.capabilities[engineNdx]))
                {
                    pQueueFamilyProps->queueCount++;
                }
            }
            pQueueFamilyProps->queueCount = (engineType == Pal::EngineTypeCompute)
                ? Util::Min(settings.asyncComputeQueueLimit, pQueueFamilyProps->queueCount)
                : pQueueFamilyProps->queueCount;

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

    if (protectedMemorySupported)
    {
        bool protectedQueueFound = false;
        for (uint32_t queueFamilyCount = 0; queueFamilyCount < m_queueFamilyCount; queueFamilyCount++)
        {
            VkQueueFamilyProperties* pQueueFamilyProps = &m_queueFamilies[queueFamilyCount].properties;

            if (pQueueFamilyProps->queueFlags & VK_QUEUE_PROTECTED_BIT)
            {
                protectedQueueFound = true;
            }
        }
        VK_ASSERT(protectedQueueFound);
    }

    // If PRT is not supported on the DMA engine, we have to fall-back on compute. Check that transfer and compute
    // queues have compatible family properties.
    if ((m_prtOnDmaSupported == false) &&
        (pTransferQueueFamilyProperties != nullptr) && (pComputeQueueFamilyProperties != nullptr))
    {
        // If compute doesn't support sparse binding, remove it from transfer as well.
        if ((pComputeQueueFamilyProperties->queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == 0)
        {
            pTransferQueueFamilyProperties->queueFlags &= ~VK_QUEUE_SPARSE_BINDING_BIT;
        }

        // Don't report more transfer queues than compute queues.
        if (pTransferQueueFamilyProperties->queueCount > pComputeQueueFamilyProperties->queueCount)
        {
            pTransferQueueFamilyProperties->queueCount = pComputeQueueFamilyProperties->queueCount;
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

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
// =====================================================================================================================
VkResult PhysicalDevice::AcquireXlibDisplay(
    Display*        dpy,
    VkDisplayKHR    display)
{
    Pal::OsDisplayHandle hDisplay = dpy;
    Pal::IScreen* pScreens        = reinterpret_cast<Pal::IScreen*>(display);

    return PalToVkResult(pScreens->AcquireScreenAccess(hDisplay,
                                                       VkToPalWsiPlatform(VK_ICD_WSI_PLATFORM_XLIB)));
}

// =====================================================================================================================
VkResult PhysicalDevice::GetRandROutputDisplay(
    Display*        dpy,
    uint32_t        randrOutput,
    VkDisplayKHR*   pDisplay)
{
    VkResult result       = VK_SUCCESS;
    Pal::IScreen* pScreen = nullptr;

    pScreen = VkInstance()->FindScreenFromRandrOutput(PalDevice(), dpy, randrOutput);

    *pDisplay = reinterpret_cast<VkDisplayKHR>(pScreen);

    if (pScreen == nullptr)
    {
        result = VK_INCOMPLETE;
    }

    return result;
}
#endif

// =====================================================================================================================
VkResult PhysicalDevice::ReleaseDisplay(
    VkDisplayKHR display)
{
    Pal::IScreen* pScreen = reinterpret_cast<Pal::IScreen*>(display);

    return PalToVkResult(pScreen->ReleaseScreenAccess());
}

#endif

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
#if defined(__unix__)
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
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

    memset(pDeviceLUID, 0, VK_LUID_SIZE);
    memset(pDeviceUUID, 0, VK_UUID_SIZE);
    memset(pDriverUUID, 0, VK_UUID_SIZE);

    *pBusNumber      = props.pciProperties.busNumber;
    *pDeviceNumber   = props.pciProperties.deviceNumber;
    *pFunctionNumber = props.pciProperties.functionNumber;

    *pDeviceNodeMask = (1u << props.gpuIndex);

    *pDeviceLUIDValid = VK_FALSE;

#if defined(INTEROP_DRIVER_UUID)
    const char driverUuidString[] = INTEROP_DRIVER_UUID;
#else
    const char driverUuidString[] = "AMD-LINUX-DRV";
#endif

    static_assert(VK_UUID_SIZE >= sizeof(driverUuidString),
                  "The driver UUID string has changed and now exceeds the maximum length permitted by Vulkan");

    memcpy(pDriverUUID,
           driverUuidString,
           strlen(driverUuidString));
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceMaintenance3Properties(
    uint32_t*     pMaxPerSetDescriptors,
    VkDeviceSize* pMaxMemoryAllocationSize
    ) const
{
    // We don't have limits on number of desc sets
    *pMaxPerSetDescriptors    = UINT32_MAX;

    // Return 2GB in bytes as max allocation size
    *pMaxMemoryAllocationSize = 2u * 1024u * 1024u * 1024u;
}

// ====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceMultiviewProperties(
    uint32_t* pMaxMultiviewViewCount,
    uint32_t* pMaxMultiviewInstanceIndex
    ) const
{
    *pMaxMultiviewViewCount     = Pal::MaxViewInstanceCount;
    *pMaxMultiviewInstanceIndex = UINT_MAX;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDevicePointClippingProperties(
    VkPointClippingBehavior* pPointClippingBehavior
    ) const
{
    // Points are clipped when their centers fall outside the clip volume, i.e. the desktop GL behavior.
    *pPointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceProtectedMemoryProperties(
    VkBool32* pProtectedNoFault
    ) const
{
    *pProtectedNoFault = VK_FALSE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceSubgroupProperties(
    uint32_t*               pSubgroupSize,
    VkShaderStageFlags*     pSupportedStages,
    VkSubgroupFeatureFlags* pSupportedOperations,
    VkBool32*               pQuadOperationsInAllStages
    ) const
{
    *pSubgroupSize        = GetSubgroupSize();

    *pSupportedStages     = VK_SHADER_STAGE_VERTEX_BIT |
                            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                            VK_SHADER_STAGE_GEOMETRY_BIT |
                            VK_SHADER_STAGE_FRAGMENT_BIT |
                            VK_SHADER_STAGE_COMPUTE_BIT;

    *pSupportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT |
                            VK_SUBGROUP_FEATURE_VOTE_BIT |
                            VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
                            VK_SUBGROUP_FEATURE_BALLOT_BIT |
                            VK_SUBGROUP_FEATURE_CLUSTERED_BIT |
                            VK_SUBGROUP_FEATURE_SHUFFLE_BIT |
                            VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
                            VK_SUBGROUP_FEATURE_QUAD_BIT;

    *pQuadOperationsInAllStages = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceDriverProperties(
    VkDriverId*           pDriverID,
    char*                 pDriverName,
    char*                 pDriverInfo,
    VkConformanceVersion* pConformanceVersion
    ) const
{
    *pDriverID = VULKAN_DRIVER_ID;

    memset(pDriverName, 0, VK_MAX_DRIVER_NAME_SIZE);
    memset(pDriverInfo, 0, VK_MAX_DRIVER_INFO_SIZE);

    Util::Strncpy(pDriverName, VULKAN_DRIVER_NAME_STR, VK_MAX_DRIVER_NAME_SIZE);

        Util::Strncpy(pDriverInfo, VULKAN_DRIVER_INFO_STR, VK_MAX_DRIVER_INFO_SIZE);

    pConformanceVersion->major     = CTS_VERSION_MAJOR;
    pConformanceVersion->minor     = CTS_VERSION_MINOR;
    pConformanceVersion->subminor  = CTS_VERSION_SUBMINOR;
    pConformanceVersion->patch     = CTS_VERSION_PATCH;
}

// =====================================================================================================================
template<typename T>
void PhysicalDevice::GetPhysicalDeviceFloatControlsProperties(
    T pFloatControlsProperties
    ) const
{
    pFloatControlsProperties->shaderSignedZeroInfNanPreserveFloat32  = VK_TRUE;
    pFloatControlsProperties->shaderDenormPreserveFloat32            = VK_TRUE;
    pFloatControlsProperties->shaderDenormFlushToZeroFloat32         = VK_TRUE;
    pFloatControlsProperties->shaderRoundingModeRTEFloat32           = VK_TRUE;
    pFloatControlsProperties->shaderRoundingModeRTZFloat32           = VK_TRUE;

    bool supportFloat16 = PalProperties().gfxipProperties.flags.supportDoubleRate16BitInstructions;
    if (supportFloat16)
    {
        pFloatControlsProperties->shaderSignedZeroInfNanPreserveFloat16  = VK_TRUE;
        pFloatControlsProperties->shaderDenormPreserveFloat16            = VK_TRUE;
        pFloatControlsProperties->shaderDenormFlushToZeroFloat16         = VK_TRUE;
        pFloatControlsProperties->shaderRoundingModeRTEFloat16           = VK_TRUE;
        pFloatControlsProperties->shaderRoundingModeRTZFloat16           = VK_TRUE;
    }
    else
    {
        pFloatControlsProperties->shaderSignedZeroInfNanPreserveFloat16  = VK_FALSE;
        pFloatControlsProperties->shaderDenormPreserveFloat16            = VK_FALSE;
        pFloatControlsProperties->shaderDenormFlushToZeroFloat16         = VK_FALSE;
        pFloatControlsProperties->shaderRoundingModeRTEFloat16           = VK_FALSE;
        pFloatControlsProperties->shaderRoundingModeRTZFloat16           = VK_FALSE;
    }

    bool supportFloat64 = PalProperties().gfxipProperties.flags.support64BitInstructions;
    if (supportFloat64)
    {
        pFloatControlsProperties->shaderSignedZeroInfNanPreserveFloat64 = VK_TRUE;
        pFloatControlsProperties->shaderDenormPreserveFloat64           = VK_TRUE;
        pFloatControlsProperties->shaderDenormFlushToZeroFloat64        = VK_TRUE;
        pFloatControlsProperties->shaderRoundingModeRTEFloat64          = VK_TRUE;
        pFloatControlsProperties->shaderRoundingModeRTZFloat64          = VK_TRUE;
    }
    else
    {
        pFloatControlsProperties->shaderSignedZeroInfNanPreserveFloat64 = VK_FALSE;
        pFloatControlsProperties->shaderDenormPreserveFloat64           = VK_FALSE;
        pFloatControlsProperties->shaderDenormFlushToZeroFloat64        = VK_FALSE;
        pFloatControlsProperties->shaderRoundingModeRTEFloat64          = VK_FALSE;
        pFloatControlsProperties->shaderRoundingModeRTZFloat64          = VK_FALSE;
    }

    if (supportFloat16 && supportFloat64)
    {
        // Float controls of float16 and float64 are determined by the same hardware register fields (not independent).
        pFloatControlsProperties->denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY;
        pFloatControlsProperties->roundingModeIndependence   = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY;
    }
    else
    {
        pFloatControlsProperties->denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
        pFloatControlsProperties->roundingModeIndependence   = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
    }
}

// =====================================================================================================================
template<typename T>
void PhysicalDevice::GetPhysicalDeviceDescriptorIndexingProperties(
    T pDescriptorIndexingProperties
    ) const
{
    pDescriptorIndexingProperties->maxUpdateAfterBindDescriptorsInAllPools              = UINT32_MAX;
    pDescriptorIndexingProperties->shaderUniformBufferArrayNonUniformIndexingNative     = VK_FALSE;
    pDescriptorIndexingProperties->shaderSampledImageArrayNonUniformIndexingNative      = VK_FALSE;
    pDescriptorIndexingProperties->shaderStorageBufferArrayNonUniformIndexingNative     = VK_FALSE;
    pDescriptorIndexingProperties->shaderStorageImageArrayNonUniformIndexingNative      = VK_FALSE;
    pDescriptorIndexingProperties->shaderInputAttachmentArrayNonUniformIndexingNative   = VK_FALSE;
    pDescriptorIndexingProperties->robustBufferAccessUpdateAfterBind                    = VK_FALSE;
    pDescriptorIndexingProperties->quadDivergentImplicitLod                             = VK_FALSE;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindSamplers         = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindUniformBuffers   = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindStorageBuffers   = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindSampledImages    = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindStorageImages    = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageDescriptorUpdateAfterBindInputAttachments = UINT32_MAX;
    pDescriptorIndexingProperties->maxPerStageUpdateAfterBindResources                  = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindSamplers              = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindUniformBuffers        = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic = MaxDynamicUniformDescriptors;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageBuffers        = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic = MaxDynamicStorageDescriptors;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindSampledImages         = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindStorageImages         = UINT32_MAX;
    pDescriptorIndexingProperties->maxDescriptorSetUpdateAfterBindInputAttachments      = UINT32_MAX;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceDepthStencilResolveProperties(
    VkResolveModeFlags* pSupportedDepthResolveModes,
    VkResolveModeFlags* pSupportedStencilResolveModes,
    VkBool32*           pIndependentResolveNone,
    VkBool32*           pIndependentResolve
    ) const
{
    *pSupportedDepthResolveModes   = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT |
                                     VK_RESOLVE_MODE_MIN_BIT |
                                     VK_RESOLVE_MODE_MAX_BIT;
    *pSupportedStencilResolveModes = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT |
                                     VK_RESOLVE_MODE_MIN_BIT |
                                     VK_RESOLVE_MODE_MAX_BIT;
    *pIndependentResolveNone       = VK_TRUE;
    *pIndependentResolve           = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceSamplerFilterMinmaxProperties(
    VkBool32* pFilterMinmaxSingleComponentFormats,
    VkBool32* pFilterMinmaxImageComponentMapping
    ) const
{
    *pFilterMinmaxSingleComponentFormats = IsSingleChannelMinMaxFilteringSupported(this) ? VK_TRUE : VK_FALSE;
    *pFilterMinmaxImageComponentMapping  = IsPerChannelMinMaxFilteringSupported() ? VK_TRUE : VK_FALSE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceTimelineSemaphoreProperties(
    uint64_t* pMaxTimelineSemaphoreValueDifference
    ) const
{
    *pMaxTimelineSemaphoreValueDifference = UINT32_MAX;
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
#if defined(__unix__)
        if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        {
            pExternalMemoryProperties->externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT |
                                                                VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT     |
                                                                VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
        }
#endif
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
void PhysicalDevice::GetPhysicalDevice16BitStorageFeatures(
    VkBool32* pStorageBuffer16BitAccess,
    VkBool32* pUniformAndStorageBuffer16BitAccess,
    VkBool32* pStoragePushConstant16,
    VkBool32* pStorageInputOutput16
    ) const
{
    // We support 16-bit buffer load/store on all ASICs
    *pStorageBuffer16BitAccess           = VK_TRUE;
    *pUniformAndStorageBuffer16BitAccess = VK_TRUE;

    // We don't plan to support 16-bit push constants
    *pStoragePushConstant16              = VK_FALSE;

    // Currently we seem to only support 16-bit inputs/outputs on ASICs supporting
    // 16-bit ALU. It's unclear at this point whether we can do any better.
    if (PalProperties().gfxipProperties.flags.support16BitInstructions &&
        ((GetRuntimeSettings().optOnlyEnableFP16ForGfx9Plus == false) ||
         (PalProperties().gfxLevel >= Pal::GfxIpLevel::GfxIp9)))
    {
        *pStorageInputOutput16           = VK_TRUE;
    }
    else
    {
        *pStorageInputOutput16           = VK_FALSE;
    }
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceMultiviewFeatures(
    VkBool32* pMultiview,
    VkBool32* pMultiviewGeometryShader,
    VkBool32* pMultiviewTessellationShader
    ) const
{
    *pMultiview                   = VK_TRUE;
    *pMultiviewGeometryShader     = VK_FALSE;
    *pMultiviewTessellationShader = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceVariablePointerFeatures(
    VkBool32* pVariablePointersStorageBuffer,
    VkBool32* pVariablePointers
    ) const
{
    *pVariablePointers = VK_TRUE;
    *pVariablePointersStorageBuffer = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceProtectedMemoryFeatures(
    VkBool32* pProtectedMemory
    ) const
{
    *pProtectedMemory = VK_FALSE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceSamplerYcbcrConversionFeatures(
    VkBool32* pSamplerYcbcrConversion
    ) const
{
    *pSamplerYcbcrConversion = VK_FALSE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceShaderDrawParameterFeatures(
    VkBool32* pShaderDrawParameters
    ) const
{
    *pShaderDrawParameters = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDevice8BitStorageFeatures(
    VkBool32* pStorageBuffer8BitAccess,
    VkBool32* pUniformAndStorageBuffer8BitAccess,
    VkBool32* pStoragePushConstant8
    ) const
{
    *pStorageBuffer8BitAccess           = VK_TRUE;
    *pUniformAndStorageBuffer8BitAccess = VK_TRUE;

    // We don't plan to support 8-bit push constants
    *pStoragePushConstant8              = VK_FALSE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceShaderAtomicInt64Features(
    VkBool32* pShaderBufferInt64Atomics,
    VkBool32* pShaderSharedInt64Atomics
    ) const
{
    if (PalProperties().gfxipProperties.flags.support64BitInstructions)
    {
        *pShaderBufferInt64Atomics = VK_TRUE;
        *pShaderSharedInt64Atomics = VK_TRUE;
    }
    else
    {
        *pShaderBufferInt64Atomics = VK_FALSE;
        *pShaderSharedInt64Atomics = VK_FALSE;
    }
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceFloat16Int8Features(
    VkBool32* pShaderFloat16,
    VkBool32* pShaderInt8
    ) const
{
    *pShaderFloat16 = PalProperties().gfxipProperties.flags.supportDoubleRate16BitInstructions ? VK_TRUE : VK_FALSE;
    *pShaderInt8    = VK_TRUE;
}

// =====================================================================================================================
template<typename T>
void PhysicalDevice::GetPhysicalDeviceDescriptorIndexingFeatures(
    T pDescriptorIndexingFeatures
    ) const
{
    pDescriptorIndexingFeatures->shaderInputAttachmentArrayDynamicIndexing           = VK_FALSE;
    pDescriptorIndexingFeatures->shaderUniformTexelBufferArrayDynamicIndexing        = VK_TRUE;
    pDescriptorIndexingFeatures->shaderStorageTexelBufferArrayDynamicIndexing        = VK_TRUE;
    pDescriptorIndexingFeatures->shaderUniformBufferArrayNonUniformIndexing          = VK_TRUE;
    pDescriptorIndexingFeatures->shaderSampledImageArrayNonUniformIndexing           = VK_TRUE;
    pDescriptorIndexingFeatures->shaderStorageBufferArrayNonUniformIndexing          = VK_TRUE;
    pDescriptorIndexingFeatures->shaderStorageImageArrayNonUniformIndexing           = VK_TRUE;
    pDescriptorIndexingFeatures->shaderInputAttachmentArrayNonUniformIndexing        = VK_FALSE;
    pDescriptorIndexingFeatures->shaderUniformTexelBufferArrayNonUniformIndexing     = VK_TRUE;
    pDescriptorIndexingFeatures->shaderStorageTexelBufferArrayNonUniformIndexing     = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingUniformBufferUpdateAfterBind       = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingSampledImageUpdateAfterBind        = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingStorageImageUpdateAfterBind        = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingStorageBufferUpdateAfterBind       = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingUniformTexelBufferUpdateAfterBind  = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingStorageTexelBufferUpdateAfterBind  = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingUpdateUnusedWhilePending           = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingPartiallyBound                     = VK_TRUE;
    pDescriptorIndexingFeatures->descriptorBindingVariableDescriptorCount            = VK_TRUE;
    pDescriptorIndexingFeatures->runtimeDescriptorArray                              = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceScalarBlockLayoutFeatures(
    VkBool32* pScalarBlockLayout
    ) const
{
    *pScalarBlockLayout = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceImagelessFramebufferFeatures(
    VkBool32* pImagelessFramebuffer
    ) const
{
    *pImagelessFramebuffer = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceUniformBufferStandardLayoutFeatures(
    VkBool32* pUniformBufferStandardLayout
    ) const
{
    *pUniformBufferStandardLayout = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceSubgroupExtendedTypesFeatures(
    VkBool32* pShaderSubgroupExtendedTypes
    ) const
{
    *pShaderSubgroupExtendedTypes = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
    VkBool32* pSeparateDepthStencilLayouts
    ) const
{
    *pSeparateDepthStencilLayouts = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceHostQueryResetFeatures(
    VkBool32* pHostQueryReset
    ) const
{
    *pHostQueryReset = VK_TRUE;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceTimelineSemaphoreFeatures(
    VkBool32* pTimelineSemaphore
    ) const
{
    *pTimelineSemaphore = PalProperties().osProperties.timelineSemaphore.support;
}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceBufferAddressFeatures(
    VkBool32* pBufferDeviceAddress,
    VkBool32* pBufferDeviceAddressCaptureReplay,
    VkBool32* pBufferDeviceAddressMultiDevice
    ) const
{
    *pBufferDeviceAddress              = VK_FALSE;
    *pBufferDeviceAddressCaptureReplay = VK_FALSE;
    *pBufferDeviceAddressMultiDevice   = VK_FALSE;

}

// =====================================================================================================================
void PhysicalDevice::GetPhysicalDeviceVulkanMemoryModelFeatures(
    VkBool32* pVulkanMemoryModel,
    VkBool32* pVulkanMemoryModelDeviceScope,
    VkBool32* pVulkanMemoryModelAvailabilityVisibilityChains
) const
{
    *pVulkanMemoryModel                             = VK_TRUE;
    *pVulkanMemoryModelDeviceScope                  = VK_TRUE;
    *pVulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;

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

                GetPhysicalDevice16BitStorageFeatures(
                    &pStorageFeatures->storageBuffer16BitAccess,
                    &pStorageFeatures->uniformAndStorageBuffer16BitAccess,
                    &pStorageFeatures->storagePushConstant16,
                    &pStorageFeatures->storageInputOutput16);

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            {
                VkPhysicalDevice8BitStorageFeatures* pStorageFeatures =
                    reinterpret_cast<VkPhysicalDevice8BitStorageFeatures*>(pHeader);

                GetPhysicalDevice8BitStorageFeatures(
                    &pStorageFeatures->storageBuffer8BitAccess,
                    &pStorageFeatures->uniformAndStorageBuffer8BitAccess,
                    &pStorageFeatures->storagePushConstant8);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            {
                VkPhysicalDeviceShaderAtomicInt64Features* pShaderAtomicInt64Features =
                    reinterpret_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(pHeader);

                GetPhysicalDeviceShaderAtomicInt64Features(
                    &pShaderAtomicInt64Features->shaderBufferInt64Atomics,
                    &pShaderAtomicInt64Features->shaderSharedInt64Atomics);

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
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            {
                VkPhysicalDeviceTimelineSemaphoreFeatures* pTimelineSemaphoreFeatures =
                    reinterpret_cast<VkPhysicalDeviceTimelineSemaphoreFeatures*>(pHeader);

                GetPhysicalDeviceTimelineSemaphoreFeatures(&pTimelineSemaphoreFeatures->timelineSemaphore);

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            {
                VkPhysicalDeviceSamplerYcbcrConversionFeatures* pSamplerYcbcrConversionFeatures =
                    reinterpret_cast<VkPhysicalDeviceSamplerYcbcrConversionFeatures*>(pHeader);

                GetPhysicalDeviceSamplerYcbcrConversionFeatures(
                    &pSamplerYcbcrConversionFeatures->samplerYcbcrConversion);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES:
            {
                VkPhysicalDeviceVariablePointerFeatures* pVariablePointerFeatures =
                    reinterpret_cast<VkPhysicalDeviceVariablePointerFeatures*>(pHeader);

                GetPhysicalDeviceVariablePointerFeatures(
                    &pVariablePointerFeatures->variablePointersStorageBuffer,
                    &pVariablePointerFeatures->variablePointers);

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            {
                VkPhysicalDeviceProtectedMemoryFeatures* pProtectedMemory =
                    reinterpret_cast<VkPhysicalDeviceProtectedMemoryFeatures*>(pHeader);

                GetPhysicalDeviceProtectedMemoryFeatures(&pProtectedMemory->protectedMemory);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            {
                VkPhysicalDeviceMultiviewFeatures* pMultiviewFeatures =
                    reinterpret_cast<VkPhysicalDeviceMultiviewFeatures*>(pHeader);

                GetPhysicalDeviceMultiviewFeatures(
                    &pMultiviewFeatures->multiview,
                    &pMultiviewFeatures->multiviewGeometryShader,
                    &pMultiviewFeatures->multiviewTessellationShader);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES:
            {
                VkPhysicalDeviceShaderDrawParameterFeatures* pShaderDrawParameterFeatures =
                    reinterpret_cast<VkPhysicalDeviceShaderDrawParameterFeatures*>(pHeader);

                GetPhysicalDeviceShaderDrawParameterFeatures(
                    &pShaderDrawParameterFeatures->shaderDrawParameters);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            {
                VkPhysicalDeviceDescriptorIndexingFeatures* pDescIndexingFeatures =
                    reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeatures*>(pHeader);

                GetPhysicalDeviceDescriptorIndexingFeatures(pDescIndexingFeatures);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR:
            {
                VkPhysicalDeviceFloat16Int8FeaturesKHR* pFloat16Int8Features =
                    reinterpret_cast<VkPhysicalDeviceFloat16Int8FeaturesKHR*>(pHeader);

                GetPhysicalDeviceFloat16Int8Features(
                    &pFloat16Int8Features->shaderFloat16,
                    &pFloat16Int8Features->shaderInt8);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT:
            {
                VkPhysicalDeviceInlineUniformBlockFeaturesEXT* pInlineUniformBlockFeatures =
                    reinterpret_cast<VkPhysicalDeviceInlineUniformBlockFeaturesEXT*>(pHeader);

                pInlineUniformBlockFeatures->inlineUniformBlock                                 = VK_TRUE;
                pInlineUniformBlockFeatures->descriptorBindingInlineUniformBlockUpdateAfterBind = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            {
                VkPhysicalDeviceScalarBlockLayoutFeatures* pScalarBlockLayoutFeatures =
                    reinterpret_cast<VkPhysicalDeviceScalarBlockLayoutFeatures*>(pHeader);

                GetPhysicalDeviceScalarBlockLayoutFeatures(&pScalarBlockLayoutFeatures->scalarBlockLayout);

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            {
                VkPhysicalDeviceTransformFeedbackFeaturesEXT* pFeedback =
                    reinterpret_cast<VkPhysicalDeviceTransformFeedbackFeaturesEXT*>(pHeader);

                pFeedback->geometryStreams   = VK_TRUE;
                pFeedback->transformFeedback = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            {
                VkPhysicalDeviceVulkanMemoryModelFeatures* pMemoryModel =
                    reinterpret_cast<VkPhysicalDeviceVulkanMemoryModelFeatures*>(pHeader);

                GetPhysicalDeviceVulkanMemoryModelFeatures(
                    &pMemoryModel->vulkanMemoryModel,
                    &pMemoryModel->vulkanMemoryModelDeviceScope,
                    &pMemoryModel->vulkanMemoryModelAvailabilityVisibilityChains);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT:
            {
                VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT* pShaderDemoteToHelperInvocation =
                    reinterpret_cast<VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT*>(pHeader);
                pShaderDemoteToHelperInvocation->shaderDemoteToHelperInvocation = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT:
            {
                VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT* pPipelineCreationCacheControl =
                    reinterpret_cast<VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT*>(pHeader);
                pPipelineCreationCacheControl->pipelineCreationCacheControl = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT:
            {
                VkPhysicalDeviceMemoryPriorityFeaturesEXT* pMemPriorityFeature =
                    reinterpret_cast<VkPhysicalDeviceMemoryPriorityFeaturesEXT *>(pHeader);
                pMemPriorityFeature->memoryPriority = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT:
            {
                VkPhysicalDeviceDepthClipEnableFeaturesEXT* pDepthClipFeatures =
                    reinterpret_cast<VkPhysicalDeviceDepthClipEnableFeaturesEXT *>(pHeader);
                pDepthClipFeatures->depthClipEnable = VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            {
                VkPhysicalDeviceHostQueryResetFeatures* pHostQueryReset =
                    reinterpret_cast<VkPhysicalDeviceHostQueryResetFeatures*>(pHeader);

                GetPhysicalDeviceHostQueryResetFeatures(&pHostQueryReset->hostQueryReset);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT:
            {
                VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT* pVertexAttributeDivisorFeatures =
                    reinterpret_cast<VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*>(pHeader);

                pVertexAttributeDivisorFeatures->vertexAttributeInstanceRateDivisor     = VK_TRUE;
                pVertexAttributeDivisorFeatures->vertexAttributeInstanceRateZeroDivisor = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD:
            {
                VkPhysicalDeviceCoherentMemoryFeaturesAMD * pDeviceCoherentMemory =
                    reinterpret_cast<VkPhysicalDeviceCoherentMemoryFeaturesAMD *>(pHeader);

                bool deviceCoherentMemoryEnabled = false;

                deviceCoherentMemoryEnabled = PalProperties().gfxipProperties.flags.supportGl2Uncached;

                pDeviceCoherentMemory->deviceCoherentMemory = deviceCoherentMemoryEnabled;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            {
                VkPhysicalDeviceBufferDeviceAddressFeatures* pBufferAddressFeatures =
                    reinterpret_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(pHeader);

                GetPhysicalDeviceBufferAddressFeatures(
                    &pBufferAddressFeatures->bufferDeviceAddress,
                    &pBufferAddressFeatures->bufferDeviceAddressCaptureReplay,
                    &pBufferAddressFeatures->bufferDeviceAddressMultiDevice);

                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT:
            {
                VkPhysicalDeviceLineRasterizationFeaturesEXT* pPhysicalDeviceLineRasterizationFeaturesEXT =
                    reinterpret_cast<VkPhysicalDeviceLineRasterizationFeaturesEXT*>(pHeader);
                pPhysicalDeviceLineRasterizationFeaturesEXT->rectangularLines = VK_FALSE;
                pPhysicalDeviceLineRasterizationFeaturesEXT->bresenhamLines   = VK_TRUE;
                pPhysicalDeviceLineRasterizationFeaturesEXT->smoothLines      = VK_FALSE;

                pPhysicalDeviceLineRasterizationFeaturesEXT->stippledRectangularLines = VK_FALSE;
                pPhysicalDeviceLineRasterizationFeaturesEXT->stippledBresenhamLines   = VK_TRUE;
                pPhysicalDeviceLineRasterizationFeaturesEXT->stippledSmoothLines      = VK_FALSE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            {
                VkPhysicalDeviceUniformBufferStandardLayoutFeatures* pUniformBufferStandardLayoutFeatures =
                    reinterpret_cast<VkPhysicalDeviceUniformBufferStandardLayoutFeatures*>(pHeader);

                GetPhysicalDeviceUniformBufferStandardLayoutFeatures(
                    &pUniformBufferStandardLayoutFeatures->uniformBufferStandardLayout);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            {
                VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* pPhysicalDeviceSeparateDepthStencilLayouts =
                    reinterpret_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(pHeader);

                GetPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
                    &pPhysicalDeviceSeparateDepthStencilLayouts->separateDepthStencilLayouts);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR:
            {
                VkPhysicalDeviceShaderClockFeaturesKHR* pPhysicalDeviceShaderClockFeatures =
                    reinterpret_cast<VkPhysicalDeviceShaderClockFeaturesKHR*>(pHeader);

                pPhysicalDeviceShaderClockFeatures->shaderSubgroupClock =
                    PalProperties().gfxipProperties.flags.supportShaderSubgroupClock;
                pPhysicalDeviceShaderClockFeatures->shaderDeviceClock   =
                    PalProperties().gfxipProperties.flags.supportShaderDeviceClock;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            {
                VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures* pSubgroupExtendedTypesFeatures =
                    reinterpret_cast<VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures*>(pHeader);

                GetPhysicalDeviceSubgroupExtendedTypesFeatures(
                    &pSubgroupExtendedTypesFeatures->shaderSubgroupExtendedTypes);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT:
            {
                VkPhysicalDeviceSubgroupSizeControlFeaturesEXT* pSubgroupSizeControlFeatures =
                    reinterpret_cast<VkPhysicalDeviceSubgroupSizeControlFeaturesEXT *>(pHeader);

                pSubgroupSizeControlFeatures->subgroupSizeControl  = VK_TRUE;
                pSubgroupSizeControlFeatures->computeFullSubgroups = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            {
                VkPhysicalDeviceImagelessFramebufferFeatures* pImagelessFramebufferFeatures =
                    reinterpret_cast<VkPhysicalDeviceImagelessFramebufferFeatures*>(pHeader);

                GetPhysicalDeviceImagelessFramebufferFeatures(&pImagelessFramebufferFeatures->imagelessFramebuffer);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR:
            {
                VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR* pPipelineExecutablePropertiesFeatures =
                    reinterpret_cast<VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR*>(pHeader);

                pPipelineExecutablePropertiesFeatures->pipelineExecutableInfo = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            {
                VkPhysicalDeviceVulkan11Features* pVulkan11Features =
                    reinterpret_cast<VkPhysicalDeviceVulkan11Features*>(pHeader);

                GetPhysicalDevice16BitStorageFeatures(
                    &pVulkan11Features->storageBuffer16BitAccess,
                    &pVulkan11Features->uniformAndStorageBuffer16BitAccess,
                    &pVulkan11Features->storagePushConstant16,
                    &pVulkan11Features->storageInputOutput16);

                GetPhysicalDeviceMultiviewFeatures(
                    &pVulkan11Features->multiview,
                    &pVulkan11Features->multiviewGeometryShader,
                    &pVulkan11Features->multiviewTessellationShader);

                GetPhysicalDeviceVariablePointerFeatures(
                    &pVulkan11Features->variablePointersStorageBuffer,
                    &pVulkan11Features->variablePointers);

                GetPhysicalDeviceProtectedMemoryFeatures(
                    &pVulkan11Features->protectedMemory);

                GetPhysicalDeviceSamplerYcbcrConversionFeatures(
                    &pVulkan11Features->samplerYcbcrConversion);

                GetPhysicalDeviceShaderDrawParameterFeatures(
                    &pVulkan11Features->shaderDrawParameters);

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            {
                VkPhysicalDeviceVulkan12Features* pVulkan12Features =
                    reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(pHeader);

                GetPhysicalDevice8BitStorageFeatures(
                    &pVulkan12Features->storageBuffer8BitAccess,
                    &pVulkan12Features->uniformAndStorageBuffer8BitAccess,
                    &pVulkan12Features->storagePushConstant8);

                GetPhysicalDeviceShaderAtomicInt64Features(
                    &pVulkan12Features->shaderBufferInt64Atomics,
                    &pVulkan12Features->shaderSharedInt64Atomics);

                GetPhysicalDeviceFloat16Int8Features(
                    &pVulkan12Features->shaderFloat16,
                    &pVulkan12Features->shaderInt8);

                GetPhysicalDeviceDescriptorIndexingFeatures(pVulkan12Features);

                GetPhysicalDeviceScalarBlockLayoutFeatures(
                    &pVulkan12Features->scalarBlockLayout);

                GetPhysicalDeviceImagelessFramebufferFeatures(
                    &pVulkan12Features->imagelessFramebuffer);

                GetPhysicalDeviceUniformBufferStandardLayoutFeatures(
                    &pVulkan12Features->uniformBufferStandardLayout);

                GetPhysicalDeviceSubgroupExtendedTypesFeatures(
                    &pVulkan12Features->shaderSubgroupExtendedTypes);

                GetPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
                    &pVulkan12Features->separateDepthStencilLayouts);

                GetPhysicalDeviceHostQueryResetFeatures(
                    &pVulkan12Features->hostQueryReset);

                GetPhysicalDeviceTimelineSemaphoreFeatures(
                    &pVulkan12Features->timelineSemaphore);

                GetPhysicalDeviceBufferAddressFeatures(
                    &pVulkan12Features->bufferDeviceAddress,
                    &pVulkan12Features->bufferDeviceAddressCaptureReplay,
                    &pVulkan12Features->bufferDeviceAddressMultiDevice);

                GetPhysicalDeviceVulkanMemoryModelFeatures(
                    &pVulkan12Features->vulkanMemoryModel,
                    &pVulkan12Features->vulkanMemoryModelDeviceScope,
                    &pVulkan12Features->vulkanMemoryModelAvailabilityVisibilityChains);

                // These features aren't new to Vulkan 1.2, but the caps just didn't exist in their original extensions.
                pVulkan12Features->samplerMirrorClampToEdge   = VK_TRUE;
                pVulkan12Features->drawIndirectCount          = VK_TRUE;
                pVulkan12Features->descriptorIndexing         = VK_TRUE;
                pVulkan12Features->samplerFilterMinmax        =
                    IsSingleChannelMinMaxFilteringSupported(this) ? VK_TRUE : VK_FALSE;
                pVulkan12Features->shaderOutputViewportIndex  = VK_TRUE;
                pVulkan12Features->shaderOutputLayer          = VK_TRUE;
                pVulkan12Features->subgroupBroadcastDynamicId = VK_TRUE;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT:
            {
                VkPhysicalDeviceConditionalRenderingFeaturesEXT* pConditionalRenderingFeatures =
                    reinterpret_cast<VkPhysicalDeviceConditionalRenderingFeaturesEXT*>(pHeader);

                if (IsConditionalRenderingSupported(this))
                {
                    pConditionalRenderingFeatures->conditionalRendering          = VK_TRUE;
                    pConditionalRenderingFeatures->inheritedConditionalRendering = VK_TRUE;
                }
                else
                {
                    pConditionalRenderingFeatures->conditionalRendering          = VK_FALSE;
                    pConditionalRenderingFeatures->inheritedConditionalRendering = VK_FALSE;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT:
            {
                VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT* pTexelBufferAlignmentFeature =
                    reinterpret_cast<VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT*>(pHeader);
                pTexelBufferAlignmentFeature->texelBufferAlignment = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT:
            {
                VkPhysicalDeviceRobustness2FeaturesEXT* pRobustness2Features =
                    reinterpret_cast<VkPhysicalDeviceRobustness2FeaturesEXT*>(pHeader);
                pRobustness2Features->robustImageAccess2  = VK_FALSE;
                pRobustness2Features->robustBufferAccess2 = VK_FALSE;
                pRobustness2Features->nullDescriptor      = VK_FALSE;
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

    const VkStructHeader*                                   pHeader;
    const VkPhysicalDeviceExternalImageFormatInfo*          pExternalImageFormatInfo                     = nullptr;
    const VkImageStencilUsageCreateInfoEXT*                 pImageStencilUsageCreateInfo                 = nullptr;

    VkStructHeaderNonConst*                                 pHeader2;
    VkExternalImageFormatProperties*                        pExternalImageProperties                     = nullptr;
    VkTextureLODGatherFormatPropertiesAMD*                  pTextureLODGatherFormatProperties            = nullptr;
    VkSamplerYcbcrConversionImageFormatProperties*          pSamplerYcbcrConversionImageFormatProperties = nullptr;

    for (pHeader = reinterpret_cast<const VkStructHeader*>(pImageFormatInfo->pNext);
         pHeader != nullptr;
         pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
        {
            pExternalImageFormatInfo = reinterpret_cast<const VkPhysicalDeviceExternalImageFormatInfo*>(pHeader);
            break;
        }
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT:
        {
            pImageStencilUsageCreateInfo = reinterpret_cast<const VkImageStencilUsageCreateInfoEXT*>(pHeader);
            break;
        }

        default:
            break;
        }
    }

    for (pHeader2 = reinterpret_cast<VkStructHeaderNonConst*>(pImageFormatProperties->pNext);
         pHeader2 != nullptr;
         pHeader2 = reinterpret_cast<VkStructHeaderNonConst*>(pHeader2->pNext))
    {
        switch (static_cast<uint32_t>(pHeader2->sType))
        {
        case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
        {
            pExternalImageProperties = reinterpret_cast<VkExternalImageFormatProperties*>(pHeader2);
            break;
        }
        case VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD:
        {
            pTextureLODGatherFormatProperties = reinterpret_cast<VkTextureLODGatherFormatPropertiesAMD*>(pHeader2);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
        {
            pSamplerYcbcrConversionImageFormatProperties = reinterpret_cast<VkSamplerYcbcrConversionImageFormatProperties*>(pHeader2);
            pSamplerYcbcrConversionImageFormatProperties->combinedImageSamplerDescriptorCount = 1;
            break;
        }
        default:
            break;
        }
    }

    // handle VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT and the common path
    VK_ASSERT((pImageStencilUsageCreateInfo == nullptr) || (pImageStencilUsageCreateInfo->stencilUsage != 0));

    result = GetImageFormatProperties(
                pImageFormatInfo->format,
                pImageFormatInfo->type,
                pImageFormatInfo->tiling,
                pImageStencilUsageCreateInfo ? pImageFormatInfo->usage | pImageStencilUsageCreateInfo->stencilUsage
                                             : pImageFormatInfo->usage,
                pImageFormatInfo->flags,
                &pImageFormatProperties->imageFormatProperties);

    // handle VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO
    if ((pExternalImageFormatInfo != nullptr) && (result == VK_SUCCESS))
    {
        // decide the supported handle type for the specific image info.
        VK_ASSERT(pExternalImageFormatInfo->handleType != 0);

        if (pExternalImageProperties != nullptr)
        {
            result = GetExternalMemoryProperties(
                            ((pImageFormatInfo->flags & Image::SparseEnablingFlags) != 0),
                            pExternalImageFormatInfo->handleType,
                            &pExternalImageProperties->externalMemoryProperties);
        }
    }

    // handle VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD
    if ((pTextureLODGatherFormatProperties != nullptr) && (result == VK_SUCCESS))
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
        const VkStructHeader*                                     pHeader;
        VkPhysicalDeviceProperties2*                              pProp;
        VkPhysicalDevicePointClippingProperties*                  pPointClippingProperties;
        VkPhysicalDeviceIDProperties*                             pIDProperties;
        VkPhysicalDeviceSampleLocationsPropertiesEXT*             pSampleLocationsPropertiesEXT;
        VkPhysicalDeviceGpaPropertiesAMD*                         pGpaProperties;
        VkPhysicalDeviceMaintenance3Properties*                   pMaintenance3Properties;
        VkPhysicalDeviceMultiviewProperties*                      pMultiviewProperties;
        VkPhysicalDeviceProtectedMemoryProperties*                pProtectedMemoryProperties;
        VkPhysicalDeviceSubgroupProperties*                       pSubgroupProperties;
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT*          pExternalMemoryHostProperties;
        VkPhysicalDeviceSamplerFilterMinmaxProperties*            pMinMaxProperties;
        VkPhysicalDeviceShaderCorePropertiesAMD*                  pShaderCoreProperties;
        VkPhysicalDeviceShaderCoreProperties2AMD*                 pShaderCoreProperties2;
        VkPhysicalDeviceDescriptorIndexingProperties*             pDescriptorIndexingProperties;
        VkPhysicalDeviceConservativeRasterizationPropertiesEXT*   pConservativeRasterizationProperties;

        VkPhysicalDeviceDriverProperties*                         pDriverProperties;
        VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT*      pVertexAttributeDivisorProperties;
        VkPhysicalDeviceFloatControlsProperties*                  pFloatControlsProperties;
        VkPhysicalDeviceInlineUniformBlockPropertiesEXT*          pInlineUniformBlockProperties;
        VkPhysicalDevicePCIBusInfoPropertiesEXT*                  pPCIBusInfoProperties;
        VkPhysicalDeviceTransformFeedbackPropertiesEXT*           pFeedbackProperties;
        VkPhysicalDeviceDepthStencilResolveProperties*            pDepthStencilResolveProperties;
        VkPhysicalDeviceTimelineSemaphoreProperties*              pTimelineSemaphoreProperties;
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT*         pSubgroupSizeControlProperties;
        VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT*        pTexelBufferAlignmentProperties;
        VkPhysicalDeviceRobustness2PropertiesEXT*                 pRobustness2Properties;
        VkPhysicalDeviceLineRasterizationPropertiesEXT*           pLineRasterizationProperties;
        VkPhysicalDeviceVulkan11Properties*                       pVulkan11Properties;
        VkPhysicalDeviceVulkan12Properties*                       pVulkan12Properties;
    };

    for (pProp = pProperties; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
        {
            GetPhysicalDevicePointClippingProperties(&pPointClippingProperties->pointClippingBehavior);
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
            GetPhysicalDeviceMaintenance3Properties(
                &pMaintenance3Properties->maxPerSetDescriptors,
                &pMaintenance3Properties->maxMemoryAllocationSize);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
        {
            GetPhysicalDeviceProtectedMemoryProperties(&pProtectedMemoryProperties->protectedNoFault);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
        {
            GetPhysicalDeviceMultiviewProperties(
                &pMultiviewProperties->maxMultiviewViewCount,
                &pMultiviewProperties->maxMultiviewInstanceIndex);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
        {
            GetPhysicalDeviceSubgroupProperties(
                &pSubgroupProperties->subgroupSize,
                &pSubgroupProperties->supportedStages,
                &pSubgroupProperties->supportedOperations,
                &pSubgroupProperties->quadOperationsInAllStages);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT:
        {
            GetPhysicalDeviceSamplerFilterMinmaxProperties(
                &pMinMaxProperties->filterMinmaxSingleComponentFormats,
                &pMinMaxProperties->filterMinmaxImageComponentMapping);
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
            pShaderCoreProperties->wavefrontSize = props.gfxipProperties.shaderCore.maxWavefrontSize;

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

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD:
        {
            const Pal::DeviceProperties& props = PalProperties();

            pShaderCoreProperties2->shaderCoreFeatures = 0;
            pShaderCoreProperties2->activeComputeUnitCount = 0;
            for (uint32_t i = 0; i < props.gfxipProperties.shaderCore.numShaderEngines; ++i)
            {
                for (uint32_t j = 0; j < props.gfxipProperties.shaderCore.numShaderArrays; ++j)
                {
                    pShaderCoreProperties2->activeComputeUnitCount +=
                        Util::CountSetBits(props.gfxipProperties.shaderCore.activeCuMask[i][j]);
                }
            }

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
        {
            GetPhysicalDeviceDescriptorIndexingProperties(pDescriptorIndexingProperties);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT:
        {
            pConservativeRasterizationProperties->primitiveOverestimationSize                   = 0;
            pConservativeRasterizationProperties->maxExtraPrimitiveOverestimationSize           = 0;
            pConservativeRasterizationProperties->extraPrimitiveOverestimationSizeGranularity   = 0;
            pConservativeRasterizationProperties->primitiveUnderestimation                      = VK_TRUE;
            pConservativeRasterizationProperties->conservativePointAndLineRasterization         = VK_FALSE;
            pConservativeRasterizationProperties->degenerateTrianglesRasterized                 = VK_TRUE;
            pConservativeRasterizationProperties->degenerateLinesRasterized                     = VK_FALSE;
            pConservativeRasterizationProperties->fullyCoveredFragmentShaderInputVariable       = VK_FALSE;
            pConservativeRasterizationProperties->conservativeRasterizationPostDepthCoverage    = IsExtensionSupported(DeviceExtensions::EXT_POST_DEPTH_COVERAGE) ? VK_TRUE : VK_FALSE;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
        {
            GetPhysicalDeviceDriverProperties(
                &pDriverProperties->driverID,
                pDriverProperties->driverName,
                pDriverProperties->driverInfo,
                &pDriverProperties->conformanceVersion);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT:
        {
            pVertexAttributeDivisorProperties->maxVertexAttribDivisor = UINT32_MAX;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
        {
            GetPhysicalDeviceFloatControlsProperties(pFloatControlsProperties);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT:
        {
            const Pal::DeviceProperties& palProps = PalProperties();

            pPCIBusInfoProperties->pciDomain   = palProps.pciProperties.domainNumber;
            pPCIBusInfoProperties->pciBus      = palProps.pciProperties.busNumber;
            pPCIBusInfoProperties->pciDevice   = palProps.pciProperties.deviceNumber;
            pPCIBusInfoProperties->pciFunction = palProps.pciProperties.functionNumber;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT:
        {
            pInlineUniformBlockProperties->maxInlineUniformBlockSize                                = 64*1024;
            pInlineUniformBlockProperties->maxPerStageDescriptorInlineUniformBlocks                 = 16;
            pInlineUniformBlockProperties->maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks  = 16;
            pInlineUniformBlockProperties->maxDescriptorSetInlineUniformBlocks                      = 16;
            pInlineUniformBlockProperties->maxDescriptorSetUpdateAfterBindInlineUniformBlocks       = 16;

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
        {
            // For now, the transform feedback draw is only supported by CmdDrawOpaque, but the hardware register
            // VGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE used in this method only has 9 bits, which means the register can
            // represent 511 bytes at most. Due to this limitation, the max values of StreamDataSize, BufferDataSize
            // and DataStride are all hardcode to 512, partly because 512 is VK spec's minimum requirement.
            pFeedbackProperties->maxTransformFeedbackStreamDataSize         = 512;
            pFeedbackProperties->maxTransformFeedbackBufferDataSize         = 512;
            pFeedbackProperties->maxTransformFeedbackBufferDataStride       = 512;
            pFeedbackProperties->maxTransformFeedbackBufferSize             = 0xffffffff;
            pFeedbackProperties->maxTransformFeedbackBuffers                = Pal::MaxStreamOutTargets;
            pFeedbackProperties->maxTransformFeedbackStreams                = Pal::MaxStreamOutTargets;
            pFeedbackProperties->transformFeedbackDraw                      = true;
            pFeedbackProperties->transformFeedbackQueries                   = true;
            pFeedbackProperties->transformFeedbackStreamsLinesTriangles     = true;
            pFeedbackProperties->transformFeedbackRasterizationStreamSelect = false;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
        {
            GetPhysicalDeviceDepthStencilResolveProperties(
                &pDepthStencilResolveProperties->supportedDepthResolveModes,
                &pDepthStencilResolveProperties->supportedStencilResolveModes,
                &pDepthStencilResolveProperties->independentResolveNone,
                &pDepthStencilResolveProperties->independentResolve);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
        {
            GetPhysicalDeviceTimelineSemaphoreProperties(
                &pTimelineSemaphoreProperties->maxTimelineSemaphoreValueDifference);
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT:
        {
            pSubgroupSizeControlProperties->minSubgroupSize = m_properties.gfxipProperties.shaderCore.minWavefrontSize;
            pSubgroupSizeControlProperties->maxSubgroupSize = m_properties.gfxipProperties.shaderCore.maxWavefrontSize;

            // No limits on the maximum number of subgroups allowed within a workgroup.
            pSubgroupSizeControlProperties->maxComputeWorkgroupSubgroups = UINT32_MAX;

            // Do not support setting a required subgroup size in any stage.
            pSubgroupSizeControlProperties->requiredSubgroupSizeStages = 0;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT:
        {
            pLineRasterizationProperties->lineSubPixelPrecisionBits = Pal::SubPixelBits;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
        {
            GetPhysicalDeviceIDProperties(
                &pVulkan11Properties->deviceUUID[0],
                &pVulkan11Properties->driverUUID[0],
                &pVulkan11Properties->deviceLUID[0],
                &pVulkan11Properties->deviceNodeMask,
                &pVulkan11Properties->deviceLUIDValid);

            GetPhysicalDeviceMaintenance3Properties(
                &pVulkan11Properties->maxPerSetDescriptors,
                &pVulkan11Properties->maxMemoryAllocationSize);

            GetPhysicalDeviceMultiviewProperties(
                &pVulkan11Properties->maxMultiviewViewCount,
                &pVulkan11Properties->maxMultiviewInstanceIndex);

            GetPhysicalDevicePointClippingProperties(
                &pVulkan11Properties->pointClippingBehavior);

            GetPhysicalDeviceProtectedMemoryProperties(
                &pVulkan11Properties->protectedNoFault);

            GetPhysicalDeviceSubgroupProperties(
                &pVulkan11Properties->subgroupSize,
                &pVulkan11Properties->subgroupSupportedStages,
                &pVulkan11Properties->subgroupSupportedOperations,
                &pVulkan11Properties->subgroupQuadOperationsInAllStages);

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
        {
            GetPhysicalDeviceDriverProperties(
                &pVulkan12Properties->driverID,
                &pVulkan12Properties->driverName[0],
                &pVulkan12Properties->driverInfo[0],
                &pVulkan12Properties->conformanceVersion);

            GetPhysicalDeviceFloatControlsProperties(pVulkan12Properties);

            GetPhysicalDeviceDescriptorIndexingProperties(pVulkan12Properties);

            GetPhysicalDeviceDepthStencilResolveProperties(
                &pVulkan12Properties->supportedDepthResolveModes,
                &pVulkan12Properties->supportedStencilResolveModes,
                &pVulkan12Properties->independentResolveNone,
                &pVulkan12Properties->independentResolve);

            GetPhysicalDeviceSamplerFilterMinmaxProperties(
                &pVulkan12Properties->filterMinmaxSingleComponentFormats,
                &pVulkan12Properties->filterMinmaxImageComponentMapping);

            GetPhysicalDeviceTimelineSemaphoreProperties(
                &pVulkan12Properties->maxTimelineSemaphoreValueDifference);

            pVulkan12Properties->framebufferIntegerColorSampleCounts =
                (VK_SAMPLE_COUNT_1_BIT |
                 VK_SAMPLE_COUNT_2_BIT |
                 VK_SAMPLE_COUNT_4_BIT |
                 VK_SAMPLE_COUNT_8_BIT) &
                GetRuntimeSettings().limitSampleCounts;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT:
        {
            // Properties are guaranteed by the comment for PAL's definition of CreateTypedBufferViewSrds().
            pTexelBufferAlignmentProperties->storageTexelBufferOffsetAlignmentBytes = m_limits.minTexelBufferOffsetAlignment;
            pTexelBufferAlignmentProperties->storageTexelBufferOffsetSingleTexelAlignment = VK_TRUE;
            pTexelBufferAlignmentProperties->uniformTexelBufferOffsetAlignmentBytes = m_limits.minTexelBufferOffsetAlignment;
            pTexelBufferAlignmentProperties->uniformTexelBufferOffsetSingleTexelAlignment = VK_TRUE;
            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT:
        {
            pRobustness2Properties->robustStorageBufferAccessSizeAlignment = 4;
            pRobustness2Properties->robustUniformBufferAccessSizeAlignment = 4;
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

    union
    {
        const VkStructHeader*                      pHeader;
        VkPhysicalDeviceMemoryProperties2*         pMemProps;
        VkPhysicalDeviceMemoryBudgetPropertiesEXT* pMemBudgetProps;
    };

    for (pMemProps = pMemoryProperties; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT:
        {
            GetMemoryBudgetProperties(pMemBudgetProps);
            break;
        }
        default:
            break;
        }
    }
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

#if defined(__unix__)
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
#endif

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

#if defined(__unix__)
    if (IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_FENCE_FD))
    {
        if ((pExternalFenceInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
            (pExternalFenceInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT))
        {
            if (props.osProperties.supportSyncFileFence)
            {
                pExternalFenceProperties->externalFenceFeatures = VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT |
                                                                  VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;
            }
        }
    }
#endif

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
    VK_ASSERT(VerifyBCFormatSupport(dev)      || (features.textureCompressionBC       == VK_FALSE));
    VK_ASSERT(VerifyEtc2FormatSupport(dev)    || (features.textureCompressionETC2     == VK_FALSE));
    VK_ASSERT(VerifyAstcLdrFormatSupport(dev) || (features.textureCompressionASTC_LDR == VK_FALSE));

    // Table 30.20. Mandatory support of at least one texture compression scheme (BC, ETC2, or ASTC)
    VK_ASSERT(features.textureCompressionBC || features.textureCompressionETC2 || features.textureCompressionASTC_LDR);
}

// =====================================================================================================================
// Verifies that the given device/instance supports and exposes the necessary extensions.
static void VerifyExtensions(
    const PhysicalDevice& dev)
{
    const uint32_t apiVersion = dev.GetSupportedAPIVersion();

    // The spec does not require Vulkan 1.1 implementations to expose the corresponding 1.0 extensions, but we'll
    // continue doing so anyways to maximize application compatibility (which is why the spec allows this).
    if (apiVersion >= VK_API_VERSION_1_1)
    {
        VK_ASSERT(dev.IsExtensionSupported(DeviceExtensions::KHR_16BIT_STORAGE)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_BIND_MEMORY2)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DEDICATED_ALLOCATION)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DESCRIPTOR_UPDATE_TEMPLATE)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DEVICE_GROUP)
               && dev.IsExtensionSupported(InstanceExtensions::KHR_DEVICE_GROUP_CREATION)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_MEMORY)
               && dev.IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_MEMORY_CAPABILITIES)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_SEMAPHORE)
               && dev.IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_SEMAPHORE_CAPABILITIES)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_EXTERNAL_FENCE)
               && dev.IsExtensionSupported(InstanceExtensions::KHR_EXTERNAL_FENCE_CAPABILITIES)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_GET_MEMORY_REQUIREMENTS2)
               && dev.IsExtensionSupported(InstanceExtensions::KHR_GET_PHYSICAL_DEVICE_PROPERTIES2)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE1)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE2)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_MAINTENANCE3)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_MULTIVIEW)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_RELAXED_BLOCK_LAYOUT)
//             && dev.IsExtensionSupported(DeviceExtensions::KHR_SAMPLER_YCBCR_CONVERSION)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SHADER_DRAW_PARAMETERS)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_STORAGE_BUFFER_STORAGE_CLASS)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_VARIABLE_POINTERS));
    }

    if (apiVersion >= VK_API_VERSION_1_2)
    {
        VK_ASSERT(dev.IsExtensionSupported(DeviceExtensions::KHR_8BIT_STORAGE)
//             && dev.IsExtensionSupported(DeviceExtensions::KHR_BUFFER_DEVICE_ADDRESS)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_CREATE_RENDERPASS2)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DEPTH_STENCIL_RESOLVE)
               && dev.IsExtensionSupported(DeviceExtensions::EXT_DESCRIPTOR_INDEXING)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DRAW_INDIRECT_COUNT)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_DRIVER_PROPERTIES)
               && dev.IsExtensionSupported(DeviceExtensions::EXT_HOST_QUERY_RESET)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_IMAGE_FORMAT_LIST)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_IMAGELESS_FRAMEBUFFER)
//             && dev.IsExtensionSupported(DeviceExtensions::EXT_SAMPLER_FILTER_MINMAX)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE)
               && dev.IsExtensionSupported(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT)
               && dev.IsExtensionSupported(DeviceExtensions::EXT_SEPARATE_STENCIL_USAGE)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SPIRV_1_4)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SWAPCHAIN_MUTABLE_FORMAT)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SHADER_ATOMIC_INT64)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SHADER_FLOAT_CONTROLS)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SHADER_FLOAT16_INT8)
               && dev.IsExtensionSupported(DeviceExtensions::EXT_SHADER_VIEWPORT_INDEX_LAYER)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_SHADER_SUBGROUP_EXTENDED_TYPES)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_TIMELINE_SEMAPHORE)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_UNIFORM_BUFFER_STANDARD_LAYOUT)
               && dev.IsExtensionSupported(DeviceExtensions::KHR_VULKAN_MEMORY_MODEL));
    }
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

    VerifyExtensions(device);
}
#endif

// =====================================================================================================================
VkResult PhysicalDevice::GetDisplayProperties(
    uint32_t*                                   pPropertyCount,
    utils::ArrayView<VkDisplayPropertiesKHR>    properties)
{
    uint32_t screenCount = 0;

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

        properties[i].display                   = reinterpret_cast<VkDisplayKHR>(pAttachedScreens[i]);
        properties[i].displayName               = nullptr;
        properties[i].physicalDimensions.width  = props.physicalDimension.width;
        properties[i].physicalDimensions.height = props.physicalDimension.height;
        properties[i].physicalResolution.width  = props.physicalResolution.width;
        properties[i].physicalResolution.height = props.physicalResolution.height;
        properties[i].supportedTransforms       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        properties[i].planeReorderPossible      = false;
        properties[i].persistentContent         = false;
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

// =====================================================================================================================
// GetSurfaceCapabilities2EXT is mainly used to query the capabilities of a display (VK_ICD_WSI_PLATFORM_DISPLAY). It's
// similar to GetSurfaceCapabilities2KHR, except for it can report some display-related capabilities.
VkResult PhysicalDevice::GetSurfaceCapabilities2EXT(
    VkSurfaceKHR                surface,
    VkSurfaceCapabilities2EXT*  pSurfaceCapabilitiesExt
    ) const
{
    VkResult result = VK_SUCCESS;
    Pal::OsDisplayHandle osDisplayHandle = 0;

    union
    {
        const VkStructHeader*       pHeader;
        VkSurfaceCapabilities2EXT*  pVkSurfaceCapabilities2EXT;
    };

    for (pVkSurfaceCapabilities2EXT = pSurfaceCapabilitiesExt;
        (pHeader != nullptr) && (result == VK_SUCCESS);
        pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT:
            result = GetSurfaceCapabilities(surface, osDisplayHandle, pVkSurfaceCapabilities2EXT);
            break;
        default:
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Get memory budget and usage info for VkPhysicalDeviceMemoryBudgetPropertiesEXT
void PhysicalDevice::GetMemoryBudgetProperties(
    VkPhysicalDeviceMemoryBudgetPropertiesEXT* pMemBudgetProps)
{
    memset(pMemBudgetProps->heapBudget, 0, sizeof(pMemBudgetProps->heapBudget));
    memset(pMemBudgetProps->heapUsage, 0, sizeof(pMemBudgetProps->heapUsage));

    {
        Util::MutexAuto lock(&m_memoryUsageTracker.trackerMutex);

        for (uint32_t heapIndex = 0; heapIndex < m_memoryProperties.memoryHeapCount; ++heapIndex)
        {
            const Pal::GpuHeap palHeap = GetPalHeapFromVkHeapIndex(heapIndex);
            // Non-local will have only 1 heap, which is GpuHeapGartUswc in Vulkan.
            VK_ASSERT(palHeap != Pal::GpuHeapGartCacheable);

            pMemBudgetProps->heapUsage[heapIndex] = m_memoryUsageTracker.allocatedMemorySize[palHeap];

            if (palHeap == Pal::GpuHeapGartUswc)
            {
                // GartCacheable also belongs to non-local heap.
                pMemBudgetProps->heapUsage[heapIndex] +=
                    m_memoryUsageTracker.allocatedMemorySize[Pal::GpuHeapGartCacheable];
            }

            uint32_t budgetRatio = 100;

            const RuntimeSettings& settings = GetRuntimeSettings();

            switch (palHeap)
            {
            case Pal::GpuHeapLocal:
                budgetRatio = settings.heapBudgetRatioOfHeapSizeLocal;
                break;
            case Pal::GpuHeapInvisible:
                budgetRatio = settings.heapBudgetRatioOfHeapSizeInvisible;
                break;
            case Pal::GpuHeapGartUswc:
                budgetRatio = settings.heapBudgetRatioOfHeapSizeNonlocal;
                break;
            default:
                VK_NEVER_CALLED();
                break;
            }

            pMemBudgetProps->heapBudget[heapIndex] =
                static_cast<VkDeviceSize>(m_memoryProperties.memoryHeaps[heapIndex].size / 100.0f * budgetRatio + 0.5f);
        }
    }
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
        result = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfacePresentModes(
            displayableInfo,
            Pal::PresentMode::Count,
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
    Pal::OsDisplayHandle osDisplayHandle = 0;

    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceCapabilities(
        surface, osDisplayHandle, pSurfaceCapabilities);
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
    Pal::OsDisplayHandle osDisplayhandle = 0;

    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceFormats(
                Surface::ObjectFromHandle(surface), osDisplayhandle, pSurfaceFormatCount, pSurfaceFormats);
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

    Pal::OsDisplayHandle osDisplayhandle           = 0;
    VkResult             result                    = VK_SUCCESS;
    VkSurfaceKHR         surface                   = VK_NULL_HANDLE;
    bool                 fullScreenExplicitEnabled = false;

    for (pVkPhysicalDeviceSurfaceInfo2KHR = pSurfaceInfo;
        (pHeader != nullptr) && ((result == VK_SUCCESS) || (result == VK_INCOMPLETE));
        pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR:
                surface = pVkPhysicalDeviceSurfaceInfo2KHR->surface;
                VK_ASSERT(surface != VK_NULL_HANDLE);
                break;

            default:
                break;
        }
    }

    if (surface != VK_NULL_HANDLE)
    {
        result = ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceFormats(
            Surface::ObjectFromHandle(surface),
            osDisplayhandle,
            pSurfaceFormatCount,
            pSurfaceFormats);
    }

    return result;
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

#if defined(__unix__)

#ifdef VK_USE_PLATFORM_XCB_KHR
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
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
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
#endif

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

#endif

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetPhysicalDevicePresentRectangles(
                                                surface,
                                                pRectCount,
                                                pRects);
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

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2EXT(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilities2EXT*                  pSurfaceCapabilities)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetSurfaceCapabilities2EXT(surface,
                                                                                           pSurfaceCapabilities);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pTimeDomainCount,
    VkTimeDomainEXT*                            pTimeDomains)
{
    return ApiPhysicalDevice::ObjectFromHandle(physicalDevice)->GetPhysicalDeviceCalibrateableTimeDomainsEXT(pTimeDomainCount,
                                                                                                             pTimeDomains);
}

}

}
