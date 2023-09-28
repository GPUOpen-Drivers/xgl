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
* @file  vk_defines.h
* @brief Contains various global defines
***********************************************************************************************************************
*/

#ifndef __VK_DEFINES_H__
#define __VK_DEFINES_H__

#pragma once

#include <stdint.h>
#include "include/khronos/vulkan.h"

namespace vk
{

#define EXTRACT_VK_STRUCTURES_0(id, CoreType, pInit, CoreStructureType) \
    const Vk##CoreType*    p##CoreType   = nullptr; \
    union \
    { \
        const VkStructHeader*   pStruct##id##Header; \
        const Vk##CoreType*     p##CoreType##Header; \
    }; \
    for (p##CoreType##Header = pInit; pStruct##id##Header != nullptr; pStruct##id##Header = pStruct##id##Header->pNext) \
    { \
        switch (static_cast<uint32_t>(pStruct##id##Header->sType)) \
        { \
        case VK_STRUCTURE_TYPE_##CoreStructureType: \
        { \
            p##CoreType = p##CoreType##Header; \
        } \
        break; \
        default: \
            break; \
        } \
    }

#define EXTRACT_VK_STRUCTURES_1(id, CoreType, NextType_1, pInit, CoreStructureType, NextStructureType_1) \
    const Vk##CoreType*    p##CoreType   = nullptr; \
    const Vk##NextType_1*  p##NextType_1 = nullptr; \
    union \
    { \
        const VkStructHeader*   pStruct##id##Header; \
        const Vk##CoreType*     p##CoreType##Header; \
        const Vk##NextType_1*   p##NextType_1##Header; \
    }; \
    for (p##CoreType##Header = pInit; pStruct##id##Header != nullptr; pStruct##id##Header = pStruct##id##Header->pNext) \
    { \
        switch (static_cast<uint32_t>(pStruct##id##Header->sType)) \
        { \
        case VK_STRUCTURE_TYPE_##CoreStructureType: \
        { \
            p##CoreType = p##CoreType##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_1: \
        { \
            p##NextType_1 = p##NextType_1##Header; \
        } \
        break; \
        default: \
            break; \
        } \
    }

#define EXTRACT_VK_STRUCTURES_2(id, CoreType, NextType_1, NextType_2, pInit, CoreStructureType, NextStructureType_1, NextStructureType_2) \
    const Vk##CoreType*    p##CoreType   = nullptr; \
    const Vk##NextType_1*  p##NextType_1 = nullptr; \
    const Vk##NextType_2*  p##NextType_2 = nullptr; \
    union \
    { \
        const VkStructHeader*   pStruct##id##Header; \
        const Vk##CoreType*     p##CoreType##Header; \
        const Vk##NextType_1*   p##NextType_1##Header; \
        const Vk##NextType_2*   p##NextType_2##Header; \
    }; \
    for (p##CoreType##Header = pInit; pStruct##id##Header != nullptr; pStruct##id##Header = pStruct##id##Header->pNext) \
    { \
        switch (static_cast<uint32_t>(pStruct##id##Header->sType)) \
        { \
        case VK_STRUCTURE_TYPE_##CoreStructureType: \
        { \
            p##CoreType = p##CoreType##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_1: \
        { \
            p##NextType_1 = p##NextType_1##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_2: \
        { \
            p##NextType_2 = p##NextType_2##Header; \
        } \
        break; \
        default: \
            break; \
        } \
    }

#define EXTRACT_VK_STRUCTURES_3(id, CoreType, NextType_1, NextType_2, NextType_3, pInit, CoreStructureType, NextStructureType_1, NextStructureType_2, NextStructureType_3) \
    const Vk##CoreType*    p##CoreType   = nullptr; \
    const Vk##NextType_1*  p##NextType_1 = nullptr; \
    const Vk##NextType_2*  p##NextType_2 = nullptr; \
    const Vk##NextType_3*  p##NextType_3 = nullptr; \
    union \
    { \
        const VkStructHeader*   pStruct##id##Header; \
        const Vk##CoreType*     p##CoreType##Header; \
        const Vk##NextType_1*   p##NextType_1##Header; \
        const Vk##NextType_2*   p##NextType_2##Header; \
        const Vk##NextType_3*   p##NextType_3##Header; \
    }; \
    for (p##CoreType##Header = pInit; pStruct##id##Header != nullptr; pStruct##id##Header = pStruct##id##Header->pNext) \
    { \
        switch (static_cast<uint32_t>(pStruct##id##Header->sType)) \
        { \
        case VK_STRUCTURE_TYPE_##CoreStructureType: \
        { \
            p##CoreType = p##CoreType##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_1: \
        { \
            p##NextType_1 = p##NextType_1##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_2: \
        { \
            p##NextType_2 = p##NextType_2##Header; \
        } \
        break; \
        case VK_STRUCTURE_TYPE_##NextStructureType_3: \
        { \
            p##NextType_3 = p##NextType_3##Header; \
        } \
        break; \
        default: \
            break; \
        } \
    }

    static const uint32_t DefaultDeviceIndex = 0;

    // The default memory instance to use for multi-instance heaps
    static const uint32_t DefaultMemoryInstanceIdx = 0;

    // Maximum number of Pal devices
#ifndef VKI_BUILD_MAX_NUM_GPUS
#define VKI_BUILD_MAX_NUM_GPUS 4
#endif
    static const uint32_t MaxPalDevices = VKI_BUILD_MAX_NUM_GPUS;

    // Invalid Mask
    static const uint32_t InvalidPalDeviceMask = 1 << (MaxPalDevices+1);

    // Maximum number of dynamic descriptors
    static const uint32_t MaxDynamicUniformDescriptors = 8;
    static const uint32_t MaxDynamicStorageDescriptors = 8;
    static const uint32_t MaxDynamicDescriptors = MaxDynamicUniformDescriptors + MaxDynamicStorageDescriptors;

    // The maximum number of sets that can appear in a pipeline layout
    static const uint32_t MaxDescriptorSets = 32;

    // The maximum size of push constants in bytes
    static const uint32_t MaxPushConstants = 128;

    // The maximum number of push descriptors that can appear in a descriptor set
    static const uint32_t MaxPushDescriptors = 32;

    // The default, full stencil write mask
    static const uint8_t StencilWriteMaskFull = 0xFF;

    // The max palette size for CustomBorderColor.
    static const uint32_t MaxBorderColorPaletteSize = 4096;

    // The max number of descriptors required for a single descriptor type.
    // This is currently 3 for YCbCr formats
    static const uint32_t MaxCombinedImageSamplerDescriptorCount = 3;

    // Enumerates the compiler types
    enum PipelineCompilerType : uint32_t
    {
        PipelineCompilerTypeLlpc,  // Use shader compiler provided by LLPC
    };

    // Point size must be set via gl_PointSize, otherwise it must be 1.0f
    static const float DefaultPointSize = 1.0f;
    static const float DefaultLineWidth = 1.0f;

    typedef VkPipelineCreateFlags PipelineCreateFlags;
    typedef VkBufferUsageFlagBits BufferUsageFlagBits;
}// namespace vk

#endif
