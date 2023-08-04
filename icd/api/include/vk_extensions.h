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

// These macros are sorted in the same order as they appear in Khronos spec: KHR, EXT, vendors. They are
// alphabetical within each section.
// KHR macros
#define VK_KHR_BIND_MEMORY2_SPEC_VERSION                    VK_KHR_BIND_MEMORY_2_SPEC_VERSION
#define VK_KHR_COPY_COMMANDS2_SPEC_VERSION                  VK_KHR_COPY_COMMANDS_2_SPEC_VERSION
#define VK_KHR_CREATE_RENDERPASS2_SPEC_VERSION              VK_KHR_CREATE_RENDERPASS_2_SPEC_VERSION
#define VK_KHR_FORMAT_FEATURE_FLAGS2_EXTENSION_NAME         VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME
#define VK_KHR_FORMAT_FEATURE_FLAGS2_SPEC_VERSION           VK_KHR_FORMAT_FEATURE_FLAGS_2_SPEC_VERSION
#define VK_KHR_GET_DISPLAY_PROPERTIES2_SPEC_VERSION         VK_KHR_GET_DISPLAY_PROPERTIES_2_SPEC_VERSION
#define VK_KHR_GET_MEMORY_REQUIREMENTS2_SPEC_VERSION        VK_KHR_GET_MEMORY_REQUIREMENTS_2_SPEC_VERSION
#define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2_SPEC_VERSION VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION
#define VK_KHR_GET_SURFACE_CAPABILITIES2_SPEC_VERSION       VK_KHR_GET_SURFACE_CAPABILITIES_2_SPEC_VERSION
#define VK_KHR_MAINTENANCE4_EXTENSION_NAME                  VK_KHR_MAINTENANCE_4_EXTENSION_NAME
#define VK_KHR_MAINTENANCE4_SPEC_VERSION                    VK_KHR_MAINTENANCE_4_SPEC_VERSION
#define VK_KHR_MAINTENANCE5_EXTENSION_NAME                  VK_KHR_MAINTENANCE_5_EXTENSION_NAME
#define VK_KHR_MAINTENANCE5_SPEC_VERSION                    VK_KHR_MAINTENANCE_5_SPEC_VERSION
#define VK_KHR_MAP_MEMORY2_SPEC_VERSION                     VK_KHR_MAP_MEMORY_2_SPEC_VERSION
#if VKI_RAY_TRACING
#define VK_KHR_RAY_TRACING_MAINTENANCE1_SPEC_VERSION        VK_KHR_RAY_TRACING_MAINTENANCE_1_SPEC_VERSION
#endif
#define VK_KHR_SYNCHRONIZATION2_SPEC_VERSION                VK_KHR_SYNCHRONIZATION_2_SPEC_VERSION

// EXT macros
#define VK_EXT_EXTENDED_DYNAMIC_STATE2_SPEC_VERSION         VK_EXT_EXTENDED_DYNAMIC_STATE_2_SPEC_VERSION
#define VK_EXT_EXTENDED_DYNAMIC_STATE3_SPEC_VERSION         VK_EXT_EXTENDED_DYNAMIC_STATE_3_SPEC_VERSION
#define VK_EXT_ROBUSTNESS2_SPEC_VERSION                     VK_EXT_ROBUSTNESS_2_SPEC_VERSION
#define VK_EXT_SHADER_ATOMIC_FLOAT2_SPEC_VERSION            VK_EXT_SHADER_ATOMIC_FLOAT_2_SPEC_VERSION
#define VK_EXT_SWAPCHAIN_COLORSPACE_EXTENSION_NAME          VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME
#define VK_EXT_SWAPCHAIN_COLORSPACE_SPEC_VERSION            VK_EXT_SWAPCHAIN_COLOR_SPACE_SPEC_VERSION

// AMD macros
#define VK_AMD_SHADER_CORE_PROPERTIES2_SPEC_VERSION         VK_AMD_SHADER_CORE_PROPERTIES_2_SPEC_VERSION

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
                m_supported[i].pName       = nullptr;
                m_supported[i].specVersion = 0;
            }
        }

        bool IsExtensionSupported(typename T::ExtensionId id) const
        {
            return m_supported[id].specVersion != 0;
        }

        void AddExtension(typename T::ExtensionId id, const char* pName, uint32_t specVersion)
        {
            // Don't allow adding extensions redundantly.
            VK_ASSERT(!IsExtensionSupported(id));

            m_supported[id].pName       = pName;
            m_supported[id].specVersion = specVersion;

            m_supportedCount++;
        }

        void GetExtensionInfo(typename T::ExtensionId id, VkExtensionProperties* pProperties) const
        {
            VK_ASSERT(IsExtensionSupported(id));

            strncpy(pProperties->extensionName, m_supported[id].pName, VK_MAX_EXTENSION_NAME_SIZE);

            pProperties->specVersion = m_supported[id].specVersion;
        }

        uint32_t GetExtensionCount() const
        {
            return m_supportedCount;
        }

    protected:
        /// Array of an internal VkExtensionProperties struct that uses a pointer for the name for a smaller size
        struct ExtensionProperties
        {
            const char* pName;
            uint32_t    specVersion;
        } m_supported[T::Count];

        uint32_t m_supportedCount;
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

        void EnableExtension(typename T::ExtensionId id)
        {
            m_enabled[id] = true;
        }

        bool IsExtensionEnabled(typename T::ExtensionId id) const
        {
            return m_enabled[id];
        }

    protected:
        bool                    m_enabled[T::Count];
    };

    static bool EnableExtensions(
        const char* const* const    extensionNames,
        uint32_t                    extensionNameCount,
        const Supported&            supported,
        const Supported&            ignored,
        Enabled*                    pEnabled)
    {
        bool invalidExtensionRequested = false;

        for (uint32_t i = 0; i < extensionNameCount && !invalidExtensionRequested; ++i)
        {
            VkExtensionProperties ext = {};

            int32_t j;

            for (j = 0; j < T::Count; ++j)
            {
                const typename T::ExtensionId id = static_cast<typename T::ExtensionId>(j);
                const bool isExtensionIgnored = ignored.IsExtensionSupported(id);

                if (supported.IsExtensionSupported(id) && (isExtensionIgnored == false))
                {
                    supported.GetExtensionInfo(id, &ext);

                    if (strcmp(extensionNames[i], ext.extensionName) == 0)
                    {
                        pEnabled->EnableExtension(id);
                        break;
                    }
                }
                else if (isExtensionIgnored)
                {
                    ignored.GetExtensionInfo(id, &ext);

                    if (strcmp(extensionNames[i], ext.extensionName) == 0)
                    {
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
class InstanceExtensions final : public Extensions<InstanceExtensions>
{
public:
    enum ExtensionId
    {
        // These extensions are sorted in the same order as they appear in Khronos spec: KHR, EXT, vendors. They are
        // alphabetical within each section.
        // KHR Extensions
        KHR_DEVICE_GROUP_CREATION,
        KHR_DISPLAY,
        KHR_EXTERNAL_FENCE_CAPABILITIES,
        KHR_EXTERNAL_MEMORY_CAPABILITIES,
        KHR_EXTERNAL_SEMAPHORE_CAPABILITIES,
        KHR_GET_DISPLAY_PROPERTIES2,
        KHR_GET_PHYSICAL_DEVICE_PROPERTIES2,
        KHR_GET_SURFACE_CAPABILITIES2,
        KHR_SURFACE,
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        KHR_WAYLAND_SURFACE,
#endif
        KHR_WIN32_SURFACE,
        KHR_XCB_SURFACE,
        KHR_XLIB_SURFACE,

        // EXT Extensions
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
        EXT_ACQUIRE_XLIB_DISPLAY,
#endif
        EXT_DEBUG_REPORT,
        EXT_DEBUG_UTILS,
        EXT_DIRECT_MODE_DISPLAY,
        EXT_DISPLAY_SURFACE_COUNTER,
        EXT_SWAPCHAIN_COLORSPACE,
        Count
    };

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(InstanceExtensions);
};

// =====================================================================================================================
// Helper class to handle device extensions.
class DeviceExtensions final : public Extensions<DeviceExtensions>
{
public:
    enum ExtensionId
    {
        // These extensions are sorted in the same order as they appear in Khronos spec: KHR, EXT, vendors. They are
        // alphabetical within each section.
        // KHR Extensions
        KHR_16BIT_STORAGE,
        KHR_8BIT_STORAGE,
#if VKI_RAY_TRACING
        KHR_ACCELERATION_STRUCTURE,
#endif
        KHR_BIND_MEMORY2,
        KHR_BUFFER_DEVICE_ADDRESS,
        KHR_COPY_COMMANDS2,
        KHR_CREATE_RENDERPASS2,
        KHR_DEDICATED_ALLOCATION,
#if VKI_RAY_TRACING
        KHR_DEFERRED_HOST_OPERATIONS,
#endif
        KHR_DEPTH_STENCIL_RESOLVE,
        KHR_DESCRIPTOR_UPDATE_TEMPLATE,
        KHR_DEVICE_GROUP,
        KHR_DRAW_INDIRECT_COUNT,
        KHR_DRIVER_PROPERTIES,
        KHR_DYNAMIC_RENDERING,
        KHR_EXTERNAL_FENCE,
        KHR_EXTERNAL_FENCE_FD,
        KHR_EXTERNAL_FENCE_WIN32,
        KHR_EXTERNAL_MEMORY,
        KHR_EXTERNAL_MEMORY_FD,
        KHR_EXTERNAL_MEMORY_WIN32,
        KHR_EXTERNAL_SEMAPHORE,
        KHR_EXTERNAL_SEMAPHORE_FD,
        KHR_EXTERNAL_SEMAPHORE_WIN32,
        KHR_FORMAT_FEATURE_FLAGS2,
        KHR_FRAGMENT_SHADER_BARYCENTRIC,
        KHR_FRAGMENT_SHADING_RATE,
        KHR_GET_MEMORY_REQUIREMENTS2,
        KHR_GLOBAL_PRIORITY,
        KHR_IMAGELESS_FRAMEBUFFER,
        KHR_IMAGE_FORMAT_LIST,
        KHR_INCREMENTAL_PRESENT,
        KHR_MAINTENANCE1,
        KHR_MAINTENANCE2,
        KHR_MAINTENANCE3,
        KHR_MAINTENANCE4,
        KHR_MAP_MEMORY2,
        KHR_MULTIVIEW,
        KHR_PIPELINE_EXECUTABLE_PROPERTIES,
        KHR_PIPELINE_LIBRARY,
        KHR_PUSH_DESCRIPTOR,
#if VKI_RAY_TRACING
        KHR_RAY_QUERY,
        KHR_RAY_TRACING_MAINTENANCE1,
        KHR_RAY_TRACING_PIPELINE,
        KHR_RAY_TRACING_POSITION_FETCH,
#endif
        KHR_RELAXED_BLOCK_LAYOUT,
        KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE,
        KHR_SAMPLER_YCBCR_CONVERSION,
        KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS,
        KHR_SHADER_ATOMIC_INT64,
        KHR_SHADER_CLOCK,
        KHR_SHADER_DRAW_PARAMETERS,
        KHR_SHADER_FLOAT16_INT8,
        KHR_SHADER_FLOAT_CONTROLS,
        KHR_SHADER_INTEGER_DOT_PRODUCT,
        KHR_SHADER_NON_SEMANTIC_INFO,
        KHR_SHADER_SUBGROUP_EXTENDED_TYPES,
        KHR_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW,
        KHR_SHADER_TERMINATE_INVOCATION,
        KHR_SPIRV_1_4,
        KHR_STORAGE_BUFFER_STORAGE_CLASS,
        KHR_SWAPCHAIN,
        KHR_SWAPCHAIN_MUTABLE_FORMAT,
        KHR_SYNCHRONIZATION2,
        KHR_TIMELINE_SEMAPHORE,
        KHR_UNIFORM_BUFFER_STANDARD_LAYOUT,
        KHR_VARIABLE_POINTERS,
        KHR_VULKAN_MEMORY_MODEL,
        KHR_WIN32_KEYED_MUTEX,
        KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT,
        KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY,

        // EXT Extensions
        EXT_4444_FORMATS,
        EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT,
        EXT_BORDER_COLOR_SWIZZLE,
        EXT_CALIBRATED_TIMESTAMPS,
        EXT_COLOR_WRITE_ENABLE,
        EXT_CONDITIONAL_RENDERING,
        EXT_CONSERVATIVE_RASTERIZATION,
        EXT_CUSTOM_BORDER_COLOR,
        EXT_DEBUG_MARKER,
        EXT_DEPTH_CLAMP_ZERO_ONE,
        EXT_DEPTH_CLIP_CONTROL,
        EXT_DEPTH_CLIP_ENABLE,
        EXT_DEPTH_RANGE_UNRESTRICTED,
        EXT_DESCRIPTOR_BUFFER,
        EXT_DESCRIPTOR_INDEXING,
        EXT_DEVICE_ADDRESS_BINDING_REPORT,
        EXT_DEVICE_FAULT,
        EXT_DEVICE_MEMORY_REPORT,
        EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS,
        EXT_EXTENDED_DYNAMIC_STATE,
        EXT_EXTENDED_DYNAMIC_STATE2,
        EXT_EXTENDED_DYNAMIC_STATE3,
        EXT_EXTERNAL_MEMORY_DMA_BUF,
        EXT_EXTERNAL_MEMORY_HOST,
        EXT_GLOBAL_PRIORITY,
        EXT_GLOBAL_PRIORITY_QUERY,
        EXT_GRAPHICS_PIPELINE_LIBRARY,
        EXT_HDR_METADATA,
        EXT_HOST_QUERY_RESET,
        EXT_IMAGE_2D_VIEW_OF_3D,
        EXT_IMAGE_DRM_FORMAT_MODIFIER,
        EXT_IMAGE_ROBUSTNESS,
        EXT_IMAGE_SLICED_VIEW_OF_3D,
        EXT_IMAGE_VIEW_MIN_LOD,
        EXT_INDEX_TYPE_UINT8,
        EXT_INLINE_UNIFORM_BLOCK,
        EXT_LINE_RASTERIZATION,
        EXT_LOAD_STORE_OP_NONE,
        EXT_MEMORY_BUDGET,
        EXT_MEMORY_PRIORITY,
        EXT_MESH_SHADER,
        EXT_MUTABLE_DESCRIPTOR_TYPE,
        EXT_NON_SEAMLESS_CUBE_MAP,
        EXT_PAGEABLE_DEVICE_LOCAL_MEMORY,
        EXT_PCI_BUS_INFO,
        EXT_PHYSICAL_DEVICE_DRM,
        EXT_PIPELINE_CREATION_CACHE_CONTROL,
        EXT_PIPELINE_CREATION_FEEDBACK,
#if VKI_RAY_TRACING
        EXT_PIPELINE_LIBRARY_GROUP_HANDLES,
#endif
        EXT_POST_DEPTH_COVERAGE,
        EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART,
        EXT_PRIMITIVES_GENERATED_QUERY,
        EXT_PRIVATE_DATA,
        EXT_PROVOKING_VERTEX,
        EXT_QUEUE_FAMILY_FOREIGN,
        EXT_ROBUSTNESS2,
        EXT_SAMPLER_FILTER_MINMAX,
        EXT_SAMPLE_LOCATIONS,
        EXT_SCALAR_BLOCK_LAYOUT,
        EXT_SEPARATE_STENCIL_USAGE,
        EXT_SHADER_ATOMIC_FLOAT,
        EXT_SHADER_ATOMIC_FLOAT2,
        EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION,
        EXT_SHADER_IMAGE_ATOMIC_INT64,
        EXT_SHADER_MODULE_IDENTIFIER,
        EXT_SHADER_STENCIL_EXPORT,
        EXT_SHADER_SUBGROUP_BALLOT,
        EXT_SHADER_SUBGROUP_VOTE,
        EXT_SHADER_VIEWPORT_INDEX_LAYER,
        EXT_SUBGROUP_SIZE_CONTROL,
        EXT_TEXEL_BUFFER_ALIGNMENT,
        EXT_TEXTURE_COMPRESSION_ASTC_HDR,
        EXT_TOOLING_INFO,
        EXT_TRANSFORM_FEEDBACK,
        EXT_VERTEX_ATTRIBUTE_DIVISOR,
        EXT_VERTEX_INPUT_DYNAMIC_STATE,
        EXT_YCBCR_IMAGE_ARRAYS,
        // AMD Extensions
        AMD_BUFFER_MARKER,
        AMD_DEVICE_COHERENT_MEMORY,
        AMD_DRAW_INDIRECT_COUNT,
        AMD_GCN_SHADER,
        AMD_GPA_INTERFACE,
        AMD_GPU_SHADER_HALF_FLOAT,
        AMD_GPU_SHADER_INT16,
        AMD_MEMORY_OVERALLOCATION_BEHAVIOR,
        AMD_MIXED_ATTACHMENT_SAMPLES,
        AMD_NEGATIVE_VIEWPORT_HEIGHT,
        AMD_RASTERIZATION_ORDER,
        AMD_SHADER_BALLOT,
        AMD_SHADER_CORE_PROPERTIES,
        AMD_SHADER_CORE_PROPERTIES2,
        AMD_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS,
        AMD_SHADER_EXPLICIT_VERTEX_PARAMETER,
        AMD_SHADER_FRAGMENT_MASK,
        AMD_SHADER_IMAGE_LOAD_STORE_LOD,
        AMD_SHADER_INFO,
        AMD_SHADER_TRINARY_MINMAX,
        AMD_TEXTURE_GATHER_BIAS_LOD,

        // GOOGLE Extensions
        GOOGLE_DECORATE_STRING,
        GOOGLE_HLSL_FUNCTIONALITY1,
        GOOGLE_USER_TYPE,

        VALVE_MUTABLE_DESCRIPTOR_TYPE,
        Count
    };

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DeviceExtensions);
};

} /* namespace vk */

#endif /* __VK_EXTENSIONS_H__ */
