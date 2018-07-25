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

#include "llpc.h"

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

struct ShaderProfilePattern
{
    // Defines which pattern tests are enabled
    union
    {
        struct
        {
            uint32_t stageActive      : 1; // Stage needs to be active
            uint32_t stageInactive    : 1; // Stage needs to be inactive
            uint32_t codeHash         : 1; // Test code hash (128-bit)
            uint32_t codeSizeLessThan : 1; // Test code size less than codeSizeLessThanValue
            uint32_t reserved         : 27;
        };
        uint32_t u32All;
    } match;

    Pal::ShaderHash codeHash;
    size_t          codeSizeLessThanValue;
};

struct PipelineProfilePattern
{
    // Defines which pattern tests are enabled
    union
    {
        struct
        {
            uint32_t always     : 1;  // Pattern always hits
            uint32_t reserved   : 31;
        };
        uint32_t u32All;
    }  match;

    ShaderProfilePattern shaders[ShaderStageCount];
};

/// Defines optimization tuning options for this shader.  These generally affect either how SCPC chooses to control
/// certain features (such as user-data spilling), or they directly control compiler options passed to SC when
/// compiling the shaders.
struct ShaderTuningOptions
{
    uint32_t vgprLimit;
    uint32_t sgprLimit;
    uint32_t ldsSpillLimitDwords;
    uint32_t maxArraySizeForFastDynamicIndexing;
    uint32_t userDataSpillThreshold;
    uint32_t maxThreadGroupsPerComputeUnit;
};

struct ShaderProfileAction
{
    // Applied to Scpc::ShaderCreateInfo:
    struct
    {
        // Defines which values are applied
        union
        {
            struct
            {
                uint32_t optStrategyFlags                   : 1;
                uint32_t minVgprOptions                     : 1;
                uint32_t vgprLimit                          : 1;
                uint32_t sgprLimit                          : 1;
                uint32_t ldsSpillLimitDwords                : 1;
                uint32_t maxArraySizeForFastDynamicIndexing : 1;
                uint32_t userDataSpillThreshold             : 1;
                uint32_t maxThreadGroupsPerComputeUnit      : 1;
                uint32_t scOptions                          : 1;
                uint32_t scOptionsMask                      : 1;
                uint32_t trapPresent                        : 1;
                uint32_t debugMode                          : 1;
                uint32_t allowReZ                           : 1;
                uint32_t shaderReplaceEnabled               : 1;
                uint32_t reserved                           : 18;
            };
            uint32_t u32All;
        } apply;

        ShaderTuningOptions tuningOptions;
    } shaderCreate;

    // Applied to Scpc::PipelineShaderInfo:
    struct
    {
        // Empty for now (potentially interesting tuning parameters exist though)
    } pipelineShader;

    // Applied to DynamicXShaderInfo
    struct
    {
        // Defines which values are applied
        union
        {
            struct
            {
                uint32_t maxWavesPerCu          : 1;
                uint32_t cuEnableMask           : 1;
                uint32_t maxThreadGroupsPerCu   : 1;
                uint32_t reserved               : 29;
            };
            uint32_t u32All;
        } apply;

        uint32_t                             maxWavesPerCu;
        uint32_t                             cuEnableMask;
        uint32_t                             maxThreadGroupsPerCu;
    } dynamicShaderInfo;
};

struct PipelineProfileAction
{
    // Applied to ShaderCreateInfo/PipelineShaderInfo/DynamicXShaderInfo:
    ShaderProfileAction shaders[ShaderStageCount];

    // Applied to Graphics/ComputePipelineCreateInfo:
    struct
    {
        union
        {
            struct
            {
                uint32_t lateAllocVsLimit : 1;
                uint32_t binningOverride  : 1;

                uint32_t reserved         : 30;
            };
            uint32_t u32All;
        } apply;

        Pal::BinningOverride binningOverride;
        uint32_t             lateAllocVsLimit;
    } createInfo;
};

// This struct describes a single entry in a per-application profile of shader compilation parameter tweaks.
//
// Each entry describes a pair of match patterns and actions.  For a given shader in a given pipeline, if all
// patterns defined by this entry match, then all actions are applied to that shader prior to compilation.
struct PipelineProfileEntry
{
    PipelineProfilePattern pattern;
    PipelineProfileAction action;
};

constexpr uint32_t MaxPipelineProfileEntries = 32;

// Describes a collection of entries that can be used to apply application-specific shader compilation tuning
// to different classes of shaders.
struct PipelineProfile
{
    uint32_t              entryCount;
    PipelineProfileEntry  entries[MaxPipelineProfileEntries];
};

// This struct represents unified shader compiler options
struct PipelineShaderOptionsPtr
{
    Llpc::PipelineShaderOptions* pOptions;
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
        Pal::DynamicGraphicsShaderInfos*  pGraphicsWaveLimitParams);

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
        Pal::DynamicGraphicsShaderInfos*  pGraphicsWaveLimitParams);

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

#if ICD_RUNTIME_APP_PROFILE
    PipelineProfile        m_runtimeProfile;
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    Util::Mutex            m_printMutex;
#endif
};

};
#endif /* __APP_SHADER_OPTIMIZER_H__ */
