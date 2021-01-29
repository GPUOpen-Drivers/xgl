/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
**************************************************************************************************
* @file  app_shader_optimizer.h
* @brief Functions for tuning compile output of specific application shaders.
**************************************************************************************************
*/

#ifndef __APP_SHADER_OPTIMIZER_H__
#define __APP_SHADER_OPTIMIZER_H__
#pragma once
#include "include/khronos/vulkan.h"

#include "include/vk_shader_code.h"
#include "appopt/g_shader_profile.h"

#include "vkgcDefs.h"

// Forward declare PAL classes used in this file
namespace Pal
{
};

// Forward declare Vulkan classes used in this file
namespace vk
{
class Device;
class Instance;
class PhysicalDevice;
struct RuntimeSettings;
};

namespace vk
{

struct ShaderOptimizerKey
{
    Pal::ShaderHash codeHash;      // Hash of the shader
    size_t          codeSize;      // Size of original shader code
};

struct PipelineOptimizerKey
{
    ShaderOptimizerKey shaders[ShaderStageCount];
};

// This struct represents unified shader compiler options
struct PipelineShaderOptionsPtr
{
    Vkgc::PipelineOptions*       pPipelineOptions;
    Vkgc::PipelineShaderOptions* pOptions;
    Vkgc::NggState*              pNggState;

};

// =====================================================================================================================
// This class can tune pre-compile SC parameters based on known shader hashes in order to improve SC code generation
// output.
//
// These tuning values are shader and workload specific and have to be tuned on a per-application basis.
class ShaderOptimizer
{
public:
    ShaderOptimizer(
        Device*         pDevice,
        PhysicalDevice* pPhysicalDevice);
    ~ShaderOptimizer();

    void Init();

    void OverrideShaderCreateInfo(
        const PipelineOptimizerKey&  pipelineKey,
        ShaderStage                  shaderStage,
        PipelineShaderOptionsPtr     options);

    void OverrideGraphicsPipelineCreateInfo(
        const PipelineOptimizerKey&       pipelineKey,
        VkShaderStageFlagBits             shaderStages,
        Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
        Pal::DynamicGraphicsShaderInfos*  pGraphicsShaderInfos);

    void OverrideComputePipelineCreateInfo(
        const PipelineOptimizerKey&      pipelineKey,
        Pal::DynamicComputeShaderInfo*   pDynamicCompueShaderInfo);

private:
    void ApplyProfileToShaderCreateInfo(
        const PipelineProfile&           profile,
        const PipelineOptimizerKey&      pipelineKey,
        ShaderStage                      shaderStage,
        PipelineShaderOptionsPtr         options);

    void ApplyProfileToGraphicsPipelineCreateInfo(
        const PipelineProfile&            profile,
        const PipelineOptimizerKey&       pipelineKey,
        VkShaderStageFlagBits             shaderStages,
        Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
        Pal::DynamicGraphicsShaderInfos*  pGraphicsShaderInfos);

    void ApplyProfileToComputePipelineCreateInfo(
        const PipelineProfile&           profile,
        const PipelineOptimizerKey&      pipelineKey,
        Pal::DynamicComputeShaderInfo*   pDynamicComputeShaderInfo);

    void ApplyProfileToDynamicGraphicsShaderInfo(
        const ShaderProfileAction&      action,
        Pal::DynamicGraphicsShaderInfo* pGraphicsShaderInfo);

    void ApplyProfileToDynamicComputeShaderInfo(
        const ShaderProfileAction&     action,
        Pal::DynamicComputeShaderInfo* pComputeShaderInfo);

    bool ProfilePatternMatchesPipeline(
        const PipelineProfilePattern& pattern,
        const PipelineOptimizerKey&   pipelineKey);

    Pal::ShaderHash GetFirstMatchingShaderHash(
        const PipelineProfilePattern& pattern,
        const PipelineOptimizerKey&   pipelineKey);

    void BuildTuningProfile();
    void BuildAppProfile();

    void BuildAppProfileLlpc();

#if ICD_RUNTIME_APP_PROFILE
    void BuildRuntimeProfile();
    void RuntimeProfileParseError();
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    void PrintProfileEntryMatch(const PipelineProfile& profile, uint32_t index, const PipelineOptimizerKey& key);
#endif

    Device*                m_pDevice;
    const RuntimeSettings& m_settings;

    PipelineProfile        m_tuningProfile;
    PipelineProfile        m_appProfile;

    ShaderProfile          m_appShaderProfile;

#if ICD_RUNTIME_APP_PROFILE
    PipelineProfile        m_runtimeProfile;
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    Util::Mutex            m_printMutex;
#endif
};

};
#endif /* __APP_SHADER_OPTIMIZER_H__ */
