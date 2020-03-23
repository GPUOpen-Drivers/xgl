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
 * @file  vk_extensions.h
 * @brief Helper header to deal with Vulkan extensions.
 ***********************************************************************************************************************
 */

#ifndef __VK_EXTENSIONS_H__
#define __VK_EXTENSIONS_H__

#pragma once

#include "khronos/vulkan.h"
#include "strings/strings.h"

// Helper macros to specify extension parameters
#define VK_XX_EXTENSION(type, id)       type##Extensions::id, vk::strings::ext::VK_##id##_name, VK_##id##_SPEC_VERSION
#define VK_INSTANCE_EXTENSION(id)       VK_XX_EXTENSION(Instance, id)
#define VK_DEVICE_EXTENSION(id)         VK_XX_EXTENSION(Device, id)

#define VK_AMD_SHADER_CORE_PROPERTIES2_SPEC_VERSION         VK_AMD_SHADER_CORE_PROPERTIES_2_SPEC_VERSION

#define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2_SPEC_VERSION VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION
#define VK_KHR_GET_SURFACE_CAPABILITIES2_SPEC_VERSION       VK_KHR_GET_SURFACE_CAPABILITIES_2_SPEC_VERSION
#define VK_KHR_GET_MEMORY_REQUIREMENTS2_SPEC_VERSION        VK_KHR_GET_MEMORY_REQUIREMENTS_2_SPEC_VERSION
#define VK_KHR_BIND_MEMORY2_SPEC_VERSION                    VK_KHR_BIND_MEMORY_2_SPEC_VERSION
#define VK_KHR_GET_DISPLAY_PROPERTIES2_SPEC_VERSION         VK_KHR_GET_DISPLAY_PROPERTIES_2_SPEC_VERSION
#define VK_KHR_CREATE_RENDERPASS2_SPEC_VERSION              VK_KHR_CREATE_RENDERPASS_2_SPEC_VERSION

#define VK_EXT_SWAPCHAIN_COLORSPACE_EXTENSION_NAME          VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME
#define VK_EXT_SWAPCHAIN_COLORSPACE_SPEC_VERSION            VK_EXT_SWAPCHAIN_COLOR_SPACE_SPEC_VERSION

namespace vk
{

// =====================================================================================================================
// Helper template class to handle extensions.
template <class T>
class Extensions
{
public:
    class Supported
    {
    public:
        Supported()
            : m_supportedCount(0)
        {
            for (int32_t i = 0; i < T::Count; ++i)
            {
                m_supported[i].specVersion = 0;
            }
        }

        VK_INLINE bool IsExtensionSupported(typename T::ExtensionId id) const
        {
            return m_supported[id].specVersion != 0;
        }

        VK_INLINE void AddExtension(typename T::ExtensionId id, const char* name, uint32_t specVersion)
        {
            // Don't allow adding extensions redundantly.
            VK_ASSERT(!IsExtensionSupported(id));

            strncpy(m_supported[id].extensionName, name, VK_MAX_EXTENSION_NAME_SIZE);
            m_supported[id].specVersion     = specVersion;

            m_supportedCount++;
        }

        VK_INLINE const VkExtensionProperties& GetExtensionInfo(typename T::ExtensionId id) const
        {
            VK_ASSERT(IsExtensionSupported(id));
            return m_supported[id];
        }

        VK_INLINE uint32_t GetExtensionCount() const
        {
            return m_supportedCount;
        }

    protected:
        VkExtensionProperties   m_supported[T::Count];
        uint32_t                m_supportedCount;
    };

    class Enabled
    {
    public:
        Enabled()
        {
            for (int32_t i = 0; i < T::Count; ++i)
            {
                m_enabled[i] = false;
            }
        }

        VK_INLINE void EnableExtension(typename T::ExtensionId id)
        {
            m_enabled[id] = true;
        }

        VK_INLINE bool IsExtensionEnabled(typename T::ExtensionId id) const
        {
            return m_enabled[id];
        }

    protected:
        bool                    m_enabled[T::Count];
    };

    VK_INLINE static bool EnableExtensions(
        const char* const* const    extensionNames,
        uint32_t                    extensionNameCount,
        const Supported&            supported,
        Enabled&                    enabled,
        uint32_t                    curApiVersion)
    {
        bool invalidExtensionRequested = false;

        for (uint32_t i = 0; i < extensionNameCount && !invalidExtensionRequested; ++i)
        {
            int32_t j;

            for (j = 0; j < T::Count; ++j)
            {
                const typename T::ExtensionId id = static_cast<typename T::ExtensionId>(j);

                uint32_t apiVersion = T::GetRequiredApiVersion(id);

                if (supported.IsExtensionSupported(id) && (curApiVersion >= apiVersion))
                {
                    const VkExtensionProperties& ext = supported.GetExtensionInfo(id);

                    if (strcmp(extensionNames[i], ext.extensionName) == 0)
                    {
                        enabled.EnableExtension(id);
                        break;
                    }
                }
            }

            if (j == T::Count)
            {
                invalidExtensionRequested = true;
            }
        }

        return !invalidExtensionRequested;
    }
};

// =====================================================================================================================
// Helper class to handle instance extensions.
class InstanceExtensions : public Extensions<InstanceExtensions>
{
public:
    enum ExtensionId
    {
        KHR_SURFACE,
        KHR_XCB_SURFACE,
        KHR_XLIB_SURFACE,
        KHR_WIN32_SURFACE,
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        KHR_WAYLAND_SURFACE,
#endif
        KHR_GET_PHYSICAL_DEVICE_PROPERTIES2,
        KHR_GET_SURFACE_CAPABILITIES2,
        KHR_EXTERNAL_MEMORY_CAPABILITIES,
        KHR_DEVICE_GROUP_CREATION,
        KHR_EXTERNAL_SEMAPHORE_CAPABILITIES,
        KHR_EXTERNAL_FENCE_CAPABILITIES,
        EXT_DEBUG_REPORT,
        KHR_DISPLAY,
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
        EXT_ACQUIRE_XLIB_DISPLAY,
#endif
        EXT_DIRECT_MODE_DISPLAY,
        KHR_GET_DISPLAY_PROPERTIES2,
        EXT_DEBUG_UTILS,
        EXT_DISPLAY_SURFACE_COUNTER,
        EXT_SWAPCHAIN_COLORSPACE,
        Count
    };

    static uint32_t GetRequiredApiVersion(ExtensionId id)
    {
        uint32_t requiredApiVersion = 0xFFFFFFFF;

        if (id < ExtensionId::Count)
        {
            switch (id) {
                case KHR_DEVICE_GROUP_CREATION:
                case KHR_EXTERNAL_MEMORY_CAPABILITIES:
                case KHR_EXTERNAL_SEMAPHORE_CAPABILITIES:
                case KHR_EXTERNAL_FENCE_CAPABILITIES:
                case KHR_GET_PHYSICAL_DEVICE_PROPERTIES2:
                    requiredApiVersion = VK_API_VERSION_1_1;
                    break;
                default:
                    requiredApiVersion = VK_API_VERSION_1_0;
            }
        }

        return requiredApiVersion;
    }
};

// =====================================================================================================================
// Helper class to handle device extensions.
class DeviceExtensions : public Extensions<DeviceExtensions>
{
public:
    enum ExtensionId
    {
        KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE,
        KHR_SHADER_DRAW_PARAMETERS,
        KHR_SWAPCHAIN,
        KHR_MAINTENANCE1,
        KHR_MAINTENANCE2,
        KHR_MAINTENANCE3,
        KHR_RELAXED_BLOCK_LAYOUT,
        KHR_DEDICATED_ALLOCATION,
        KHR_DESCRIPTOR_UPDATE_TEMPLATE,
        KHR_EXTERNAL_MEMORY,
        KHR_EXTERNAL_MEMORY_FD,
        KHR_EXTERNAL_MEMORY_WIN32,
        KHR_DEVICE_GROUP,
        KHR_BIND_MEMORY2,
        KHR_EXTERNAL_SEMAPHORE,
        KHR_EXTERNAL_SEMAPHORE_FD,
        KHR_EXTERNAL_SEMAPHORE_WIN32,
        AMD_RASTERIZATION_ORDER,
        AMD_SHADER_BALLOT,
        AMD_SHADER_CORE_PROPERTIES2,
        AMD_SHADER_TRINARY_MINMAX,
        AMD_SHADER_EXPLICIT_VERTEX_PARAMETER,
        AMD_GCN_SHADER,
        AMD_DRAW_INDIRECT_COUNT,
        KHR_DRAW_INDIRECT_COUNT,
        AMD_NEGATIVE_VIEWPORT_HEIGHT,
        AMD_GPU_SHADER_HALF_FLOAT,
        AMD_SHADER_INFO,
        EXT_SAMPLER_FILTER_MINMAX,
        AMD_SHADER_FRAGMENT_MASK,
        EXT_HDR_METADATA,
        AMD_TEXTURE_GATHER_BIAS_LOD,
        AMD_MIXED_ATTACHMENT_SAMPLES,
        EXT_SAMPLE_LOCATIONS,
        EXT_DEBUG_MARKER,
        AMD_GPU_SHADER_INT16,
        EXT_SHADER_SUBGROUP_VOTE,
        KHR_16BIT_STORAGE,
        KHR_STORAGE_BUFFER_STORAGE_CLASS,
        AMD_GPA_INTERFACE,
        KHR_DEPTH_STENCIL_RESOLVE,
        EXT_SHADER_SUBGROUP_BALLOT,
        EXT_SHADER_STENCIL_EXPORT,
        EXT_SHADER_VIEWPORT_INDEX_LAYER,
        KHR_GET_MEMORY_REQUIREMENTS2,
        KHR_IMAGE_FORMAT_LIST,
        KHR_SWAPCHAIN_MUTABLE_FORMAT,
        KHR_SHADER_FLOAT_CONTROLS,
        EXT_INLINE_UNIFORM_BLOCK,
        KHR_SHADER_ATOMIC_INT64,
        KHR_DRIVER_PROPERTIES,
        KHR_CREATE_RENDERPASS2,
        KHR_8BIT_STORAGE,
        KHR_MULTIVIEW,
        KHR_SHADER_FLOAT16_INT8,
        KHR_TIMELINE_SEMAPHORE,
        KHR_EXTERNAL_FENCE,
        KHR_EXTERNAL_FENCE_FD,
        KHR_EXTERNAL_FENCE_WIN32,
        KHR_WIN32_KEYED_MUTEX,
        EXT_GLOBAL_PRIORITY,
        AMD_BUFFER_MARKER,
        AMD_SHADER_IMAGE_LOAD_STORE_LOD,
        EXT_EXTERNAL_MEMORY_HOST,
        EXT_DEPTH_CLIP_ENABLE,
        EXT_DEPTH_RANGE_UNRESTRICTED,
        AMD_SHADER_CORE_PROPERTIES,
        EXT_QUEUE_FAMILY_FOREIGN,
        EXT_DESCRIPTOR_INDEXING,
        KHR_VARIABLE_POINTERS,
        EXT_VERTEX_ATTRIBUTE_DIVISOR,
        EXT_CONSERVATIVE_RASTERIZATION,
        EXT_PCI_BUS_INFO,
        KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS,
        KHR_SHADER_CLOCK,

        KHR_SPIRV_1_4,

        GOOGLE_HLSL_FUNCTIONALITY1,
        GOOGLE_DECORATE_STRING,
        EXT_SCALAR_BLOCK_LAYOUT,

        AMD_MEMORY_OVERALLOCATION_BEHAVIOR,
        EXT_TRANSFORM_FEEDBACK,
        EXT_SEPARATE_STENCIL_USAGE,
        KHR_VULKAN_MEMORY_MODEL,
        EXT_MEMORY_PRIORITY,
        EXT_CONDITIONAL_RENDERING,
        AMD_DEVICE_COHERENT_MEMORY,
        EXT_MEMORY_BUDGET,
        EXT_POST_DEPTH_COVERAGE,
        EXT_HOST_QUERY_RESET,
        KHR_BUFFER_DEVICE_ADDRESS,
        EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION,
        EXT_LINE_RASTERIZATION,
        KHR_UNIFORM_BUFFER_STANDARD_LAYOUT,
        KHR_SHADER_SUBGROUP_EXTENDED_TYPES,
        EXT_SUBGROUP_SIZE_CONTROL,
        KHR_IMAGELESS_FRAMEBUFFER,
        EXT_PIPELINE_CREATION_FEEDBACK,
        EXT_CALIBRATED_TIMESTAMPS,
        KHR_PIPELINE_EXECUTABLE_PROPERTIES,
        KHR_SHADER_NON_SEMANTIC_INFO,
        KHR_SAMPLER_YCBCR_CONVERSION,
        EXT_TEXEL_BUFFER_ALIGNMENT,
        Count
    };

    static uint32_t GetRequiredApiVersion(ExtensionId id)
    {
        uint32_t requiredApiVersion = 0xFFFFFFFF;

        if (id < ExtensionId::Count)
        {
            switch (id) {
                case KHR_8BIT_STORAGE:
                case KHR_BUFFER_DEVICE_ADDRESS:
                case KHR_CREATE_RENDERPASS2:
                case KHR_DEPTH_STENCIL_RESOLVE:
                case EXT_DESCRIPTOR_INDEXING:
                case KHR_DRAW_INDIRECT_COUNT:
                case KHR_DRIVER_PROPERTIES:
                case EXT_HOST_QUERY_RESET:
                case KHR_IMAGE_FORMAT_LIST:
                case KHR_IMAGELESS_FRAMEBUFFER:
                case EXT_SAMPLER_FILTER_MINMAX:
                case KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE:
                case EXT_SCALAR_BLOCK_LAYOUT:
                case EXT_SEPARATE_STENCIL_USAGE:
                case KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS:
                case KHR_SPIRV_1_4:
                case KHR_SWAPCHAIN_MUTABLE_FORMAT:
                case KHR_SHADER_ATOMIC_INT64:
                case KHR_SHADER_FLOAT_CONTROLS:
                case KHR_SHADER_FLOAT16_INT8:
                case EXT_SHADER_VIEWPORT_INDEX_LAYER:
                case KHR_SHADER_SUBGROUP_EXTENDED_TYPES:
                case KHR_TIMELINE_SEMAPHORE:
                case KHR_UNIFORM_BUFFER_STANDARD_LAYOUT:
                case KHR_VULKAN_MEMORY_MODEL:
                    requiredApiVersion = VK_API_VERSION_1_2;
                    break;
                case KHR_16BIT_STORAGE:
                case KHR_BIND_MEMORY2:
                case KHR_DEDICATED_ALLOCATION:
                case KHR_DESCRIPTOR_UPDATE_TEMPLATE:
                case KHR_DEVICE_GROUP:
                case KHR_EXTERNAL_MEMORY:
                case KHR_EXTERNAL_SEMAPHORE:
                case KHR_EXTERNAL_FENCE:
                case KHR_GET_MEMORY_REQUIREMENTS2:
                case KHR_MAINTENANCE1:
                case KHR_MAINTENANCE2:
                case KHR_MAINTENANCE3:
                case KHR_MULTIVIEW:
                case KHR_RELAXED_BLOCK_LAYOUT:
                case KHR_SAMPLER_YCBCR_CONVERSION:
                case KHR_SHADER_DRAW_PARAMETERS:
                case KHR_STORAGE_BUFFER_STORAGE_CLASS:
                case KHR_VARIABLE_POINTERS:
                    requiredApiVersion = VK_API_VERSION_1_1;
                    break;
                default:
                    requiredApiVersion = VK_API_VERSION_1_0;
            }
        }

        return requiredApiVersion;
    }

};

} /* namespace vk */

#endif /* __VK_EXTENSIONS_H__ */
