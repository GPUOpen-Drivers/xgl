/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once
#ifndef __RAY_TRACING_UTIL_H__
#define __RAY_TRACING_UTIL_H__

#include "gpurt/gpurt.h"
#include "gpurt/gpurtLib.h"
#include "include/vk_device.h"

namespace vk
{

#define MAKE_GPURT_VERSION(MAJOR, MINOR) ((MAJOR << 16) | MINOR)

// =====================================================================================================================
// Converts a Vulkan triangle compression mode setting to the GpuRT equivalent of TriangleCompressionMode
inline GpuRt::TriangleCompressionMode ConvertGpuRtTriCompressMode(
     TriangleCompressionMode vkMode)
 {
    GpuRt::TriangleCompressionMode gpuRtMode = GpuRt::TriangleCompressionMode::None;

     switch (vkMode)
     {
     case NoTriangleCompression:
        gpuRtMode = GpuRt::TriangleCompressionMode::None;
        break;
     case PairTriangleCompression:
        gpuRtMode = GpuRt::TriangleCompressionMode::Pair;
        break;
     case AutoTriangleCompression:
        // Driver will do the auto selection, no need to translate to GpuRt::TriangleCompressionMode
        gpuRtMode = GpuRt::TriangleCompressionMode::None;
        break;
     default:
        VK_NEVER_CALLED();
        gpuRtMode = GpuRt::TriangleCompressionMode::None;
        break;
     }

     return gpuRtMode;
}

// =====================================================================================================================
// Converts a Vulkan triangle compression mode setting to the GpuRT equivalent of TriangleCompressionAutoMode
inline GpuRt::TriangleCompressionAutoMode ConvertGpuRtTriCompressionAutoMode(
    TriangleCompressionAutoMode triCompressionAutoMode)
{
    GpuRt::TriangleCompressionAutoMode gpuRtMode = GpuRt::TriangleCompressionAutoMode::Disabled;
    switch (triCompressionAutoMode)
    {
    case TriangleCompressionAutoModeDefaultBuild:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::DefaultBuild;
        break;
    case TriangleCompressionAutoModeFastTrace:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::FastTrace;
        break;
    case TriangleCompressionAutoModeCompaction:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::Compaction;
        break;
    case TriangleCompressionAutoModeDefaultBuildWithCompaction:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::DefaultBuildWithCompaction;
        break;
    case TriangleCompressionAutoModeFastTraceWithCompaction:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::FastTraceWithCompaction;
        break;
    case TriangleCompressionAutoModeDefaultBuildOrCompaction:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::DefaultBuildOrCompaction;
        break;
    case TriangleCompressionAutoModeFastTraceOrCompaction:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::FastTraceOrCompaction;
        break;
    case TriangleCompressionAutoModeDisabled:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::Disabled;
        break;
    case TriangleCompressionAutoModeAlwaysEnabled:
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::AlwaysEnabled;
        break;
    default:
        VK_NEVER_CALLED();
        gpuRtMode = GpuRt::TriangleCompressionAutoMode::Disabled;
        break;
    }

    return gpuRtMode;
}

// =====================================================================================================================
// Converts a Vulkan BVH build mode setting to the GpuRT equivalent
inline GpuRt::BvhBuildMode ConvertGpuRtBvhBuildMode(
    BvhBuildMode vkMode)
{
    GpuRt::BvhBuildMode gpuRtMode = GpuRt::BvhBuildMode::Linear;

    switch (vkMode)
    {
    case BvhBuildModeLinear:
        gpuRtMode = GpuRt::BvhBuildMode::Linear;
        break;
    case BvhBuildModePLOC:
        gpuRtMode = GpuRt::BvhBuildMode::PLOC;
        break;
    case BvhBuildModeAuto:
        // No override, fall back to regular build options
        gpuRtMode = GpuRt::BvhBuildMode::Auto;
        break;
    default:
        VK_NEVER_CALLED();
        gpuRtMode = GpuRt::BvhBuildMode::Linear;
        break;
    }

    return gpuRtMode;
}

// =====================================================================================================================
// Converts a Vulkan Fp16 box nodes in BLAS mode setting to the GpuRT equivalent
inline GpuRt::Fp16BoxNodesInBlasMode ConvertGpuRtFp16BoxNodesInBlasMode(
    Fp16BoxNodesInBlasMode vkMode)
{
    GpuRt::Fp16BoxNodesInBlasMode gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::NoNodes;

    switch (vkMode)
    {
    case Fp16BoxNodesInBlasModeNone:
        gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::NoNodes;
        break;
    case Fp16BoxNodesInBlasModeLeaves:
        gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::LeafNodes;
        break;
    case Fp16BoxNodesInBlasModeMixed:
        gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::MixedNodes;
        break;
    case Fp16BoxNodesInBlasModeAll:
        gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::AllNodes;
        break;
    default:
        VK_NEVER_CALLED();
        gpuRtMode = GpuRt::Fp16BoxNodesInBlasMode::NoNodes;
        break;
    }

    return gpuRtMode;
}

// =====================================================================================================================
// Converts a Vulkan RebraidType to a GpuRT RebraidType
inline GpuRt::RebraidType ConvertGpuRtRebraidType(
    RebraidType vkType)
{
    GpuRt::RebraidType gpuRtType = GpuRt::RebraidType::Off;

    switch (vkType)
    {
    case RebraidTypeOff:
        gpuRtType = GpuRt::RebraidType::Off;
        break;
    case RebraidTypeV1:
        gpuRtType = GpuRt::RebraidType::V1;
        break;
    case RebraidTypeV2:
        gpuRtType = GpuRt::RebraidType::V2;
        break;
    default:
        VK_NEVER_CALLED();
        gpuRtType = GpuRt::RebraidType::Off;
        break;
    }

    return gpuRtType;
}

}

#endif
