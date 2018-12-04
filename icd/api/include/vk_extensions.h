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
        Enabled&                    enabled)
    {
        bool invalidExtensionRequested = false;

        for (uint32_t i = 0; i < extensionNameCount && !invalidExtensionRequested; ++i)
        {
            int32_t j;

            for (j = 0; j < T::Count; ++j)
            {
                const typename T::ExtensionId id = static_cast<typename T::ExtensionId>(j);

                if (supported.IsExtensionSupported(id))
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
        Count
    };
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
        AMD_SHADER_TRINARY_MINMAX,
        AMD_SHADER_EXPLICIT_VERTEX_PARAMETER,
        AMD_GCN_SHADER,
        AMD_DRAW_INDIRECT_COUNT,
        KHR_DRAW_INDIRECT_COUNT,
        AMD_NEGATIVE_VIEWPORT_HEIGHT,
        AMD_GPU_SHADER_HALF_FLOAT,
        AMD_SHADER_INFO,
        AMD_GPU_SHADER_HALF_FLOAT_FETCH,
        EXT_SAMPLER_FILTER_MINMAX,
        AMD_SHADER_FRAGMENT_MASK,
        EXT_HDR_METADATA,
        EXT_SWAPCHAIN_COLORSPACE,
        AMD_TEXTURE_GATHER_BIAS_LOD,
        AMD_MIXED_ATTACHMENT_SAMPLES,
        EXT_SAMPLE_LOCATIONS,
        EXT_DEBUG_MARKER,
        AMD_GPU_SHADER_INT16,
        EXT_SHADER_SUBGROUP_VOTE,
        KHR_16BIT_STORAGE,
        KHR_STORAGE_BUFFER_STORAGE_CLASS,
        AMD_GPA_INTERFACE,
        EXT_SHADER_SUBGROUP_BALLOT,
        EXT_SHADER_STENCIL_EXPORT,
        EXT_SHADER_VIEWPORT_INDEX_LAYER,
        KHR_GET_MEMORY_REQUIREMENTS2,
        KHR_IMAGE_FORMAT_LIST,
        KHR_SWAPCHAIN_MUTABLE_FORMAT,

        KHR_SHADER_ATOMIC_INT64,
        KHR_DRIVER_PROPERTIES,
        KHR_CREATE_RENDERPASS2,
        KHR_8BIT_STORAGE,
        KHR_MULTIVIEW,
        KHR_EXTERNAL_FENCE,
        KHR_EXTERNAL_FENCE_FD,
        KHR_EXTERNAL_FENCE_WIN32,
        KHR_WIN32_KEYED_MUTEX,
        EXT_GLOBAL_PRIORITY,
        AMD_BUFFER_MARKER,
        AMD_SHADER_IMAGE_LOAD_STORE_LOD,
        EXT_EXTERNAL_MEMORY_HOST,
        EXT_DEPTH_RANGE_UNRESTRICTED,
        AMD_SHADER_CORE_PROPERTIES,
        EXT_QUEUE_FAMILY_FOREIGN,
        EXT_DESCRIPTOR_INDEXING,
        KHR_VARIABLE_POINTERS,
        EXT_VERTEX_ATTRIBUTE_DIVISOR,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 425
        EXT_CONSERVATIVE_RASTERIZATION,
#endif
#if VKI_SHADER_COMPILER_CONTROL
        AMD_SHADER_COMPILER_CONTROL,
#endif

        GOOGLE_HLSL_FUNCTIONALITY1,
        GOOGLE_DECORATE_STRING,
        EXT_SCALAR_BLOCK_LAYOUT,

        AMD_MEMORY_OVERALLOCATION_BEHAVIOR,
        Count
    };
};

} /* namespace vk */

#endif /* __VK_EXTENSIONS_H__ */
