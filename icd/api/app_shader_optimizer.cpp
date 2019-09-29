/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  app_shader_optimizer.cpp
* @brief Functions for tuning specific shader compile parameters for optimized code generation.
**************************************************************************************************
*/
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_physical_device.h"
#include "include/vk_shader_code.h"
#include "include/vk_utils.h"

#include "include/app_shader_optimizer.h"

#include "palDbgPrint.h"
#include "palFile.h"

#if ICD_RUNTIME_APP_PROFILE
#include "utils/json_reader.h"
#endif

namespace vk
{

// =====================================================================================================================
ShaderOptimizer::ShaderOptimizer(
    Device*         pDevice,
    PhysicalDevice* pPhysicalDevice)
    :
    m_pDevice(pDevice),
    m_settings(pPhysicalDevice->GetRuntimeSettings())
{
#if PAL_ENABLE_PRINTS_ASSERTS
    m_printMutex.Init();
#endif
}

// =====================================================================================================================
void ShaderOptimizer::Init()
{
    BuildAppProfile();

    BuildTuningProfile();

#if ICD_RUNTIME_APP_PROFILE
    BuildRuntimeProfile();

#endif
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToShaderCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    ShaderStage                      shaderStage,
    PipelineShaderOptionsPtr         options)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            const auto& shaderCreate = profileEntry.action.shaders[static_cast<uint32_t>(shaderStage)].shaderCreate;

            if (options.pOptions != nullptr)
            {
                if (shaderCreate.apply.vgprLimit)
                {
                    options.pOptions->vgprLimit = shaderCreate.tuningOptions.vgprLimit;
                }

                if (shaderCreate.apply.sgprLimit)
                {
                    options.pOptions->sgprLimit = shaderCreate.tuningOptions.sgprLimit;
                }

                if (shaderCreate.apply.maxThreadGroupsPerComputeUnit)
                {
                    options.pOptions->maxThreadGroupsPerComputeUnit =
                        shaderCreate.tuningOptions.maxThreadGroupsPerComputeUnit;
                }

                if (shaderCreate.apply.debugMode)
                {
                    options.pOptions->debugMode = true;
                }

                if (shaderCreate.apply.trapPresent)
                {
                    options.pOptions->trapPresent = true;
                }

                if (shaderCreate.apply.allowReZ)
                {
                    options.pOptions->allowReZ = true;
                }

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 24
                if (shaderCreate.apply.disableLoopUnrolls)
                {
                    options.pOptions->forceLoopUnrollCount = 1;
                }
#endif
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 28
                if (shaderCreate.tuningOptions.useSiScheduler)
                {
                    options.pOptions->useSiScheduler = true;
                }
                if (shaderCreate.tuningOptions.reconfigWorkgroupLayout)
                {
                    options.pPipelineOptions->reconfigWorkgroupLayout = true;
                }
#endif
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 33
                if (shaderCreate.tuningOptions.enableLoadScalarizer)
                {
                    options.pOptions->enableLoadScalarizer = true;
                }
#endif

                if (shaderCreate.apply.waveSize)
                {
                    options.pOptions->waveSize = shaderCreate.tuningOptions.waveSize;
                }

                if (shaderCreate.apply.wgpMode)
                {
                    options.pOptions->wgpMode = true;
                }

                if (shaderCreate.apply.waveBreakSize)
                {
                    options.pOptions->waveBreakSize =
                        static_cast<Llpc::WaveBreakSize>(shaderCreate.tuningOptions.waveBreakSize);
                }

                if (shaderCreate.apply.nggDisable)
                {
                    options.pNggState->enableNgg = false;
                }

                if (shaderCreate.apply.nggFasterLaunchRate)
                {
                    options.pNggState->enableFastLaunch = true;
                }

                if (shaderCreate.apply.nggVertexReuse)
                {
                    options.pNggState->enableVertexReuse = true;
                }

                if (shaderCreate.apply.nggEnableFrustumCulling)
                {
                    options.pNggState->enableFrustumCulling = true;
                }

                if (shaderCreate.apply.nggEnableBoxFilterCulling)
                {
                    options.pNggState->enableBoxFilterCulling = true;
                }

                if (shaderCreate.apply.nggEnableSphereCulling)
                {
                    options.pNggState->enableSphereCulling = true;
                }

                if (shaderCreate.apply.nggEnableBackfaceCulling)
                {
                    options.pNggState->enableBackfaceCulling = true;
                }

                if (shaderCreate.apply.nggEnableSmallPrimFilter)
                {
                    options.pNggState->enableSmallPrimFilter = true;
                }
            }

        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::OverrideShaderCreateInfo(
    const PipelineOptimizerKey&        pipelineKey,
    ShaderStage                        shaderStage,
    PipelineShaderOptionsPtr           options)
{

    ApplyProfileToShaderCreateInfo(m_appProfile, pipelineKey, shaderStage, options);

    ApplyProfileToShaderCreateInfo(m_tuningProfile, pipelineKey, shaderStage, options);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToShaderCreateInfo(m_runtimeProfile, pipelineKey, shaderStage, options);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::OverrideGraphicsPipelineCreateInfo(
    const PipelineOptimizerKey&       pipelineKey,
    VkShaderStageFlagBits             shaderStages,
    Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
    Pal::DynamicGraphicsShaderInfos*  pGraphicsWaveLimitParams)
{
    ApplyProfileToGraphicsPipelineCreateInfo(
        m_appProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsWaveLimitParams);

    ApplyProfileToGraphicsPipelineCreateInfo(
        m_tuningProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsWaveLimitParams);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToGraphicsPipelineCreateInfo(
        m_runtimeProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsWaveLimitParams);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::OverrideComputePipelineCreateInfo(
    const PipelineOptimizerKey&      pipelineKey,
    Pal::DynamicComputeShaderInfo*   pDynamicCompueShaderInfo)
{
    ApplyProfileToComputePipelineCreateInfo(m_appProfile, pipelineKey, pDynamicCompueShaderInfo);

    ApplyProfileToComputePipelineCreateInfo(m_tuningProfile, pipelineKey, pDynamicCompueShaderInfo);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToComputePipelineCreateInfo(m_runtimeProfile, pipelineKey, pDynamicCompueShaderInfo);
#endif
}

// =====================================================================================================================
ShaderOptimizer::~ShaderOptimizer()
{

}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToDynamicComputeShaderInfo(
    const ShaderProfileAction&     action,
    Pal::DynamicComputeShaderInfo* pComputeShaderInfo)
{
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToDynamicGraphicsShaderInfo(
    const ShaderProfileAction&      action,
    Pal::DynamicGraphicsShaderInfo* pGraphicsShaderInfo)
{

    if (action.dynamicShaderInfo.apply.cuEnableMask)
    {
        pGraphicsShaderInfo->cuEnableMask = action.dynamicShaderInfo.cuEnableMask;
    }
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToGraphicsPipelineCreateInfo(
    const PipelineProfile&            profile,
    const PipelineOptimizerKey&       pipelineKey,
    VkShaderStageFlagBits             shaderStages,
    Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
    Pal::DynamicGraphicsShaderInfos*  pGraphicsShaderInfos)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            // Apply parameters to DynamicGraphicsShaderInfo
            const auto& shaders = profileEntry.action.shaders;

            if (shaderStages & VK_SHADER_STAGE_VERTEX_BIT)
            {
                ApplyProfileToDynamicGraphicsShaderInfo(shaders[ShaderStageVertex], &pGraphicsShaderInfos->vs);
            }

            if (shaderStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            {
                ApplyProfileToDynamicGraphicsShaderInfo(shaders[ShaderStageTessControl], &pGraphicsShaderInfos->hs);
            }

            if (shaderStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            {
                ApplyProfileToDynamicGraphicsShaderInfo(shaders[ShaderStageTessEvaluation], &pGraphicsShaderInfos->ds);
            }

            if (shaderStages & VK_SHADER_STAGE_GEOMETRY_BIT)
            {
                ApplyProfileToDynamicGraphicsShaderInfo(shaders[ShaderStageGeometry], &pGraphicsShaderInfos->gs);
            }

            if (shaderStages & VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                ApplyProfileToDynamicGraphicsShaderInfo(shaders[ShaderStageFragment], &pGraphicsShaderInfos->ps);
            }

            // Apply parameters to Pal::GraphicsPipelineCreateInfo
            const auto& createInfo = profileEntry.action.createInfo;

            if (createInfo.apply.lateAllocVsLimit)
            {
                pPalCreateInfo->useLateAllocVsLimit = true;
                pPalCreateInfo->lateAllocVsLimit    = createInfo.lateAllocVsLimit;
            }

            if (createInfo.apply.binningOverride)
            {
                pPalCreateInfo->rsState.binningOverride = createInfo.binningOverride;
            }

#if PAL_ENABLE_PRINTS_ASSERTS
            if (m_settings.pipelineProfileDbgPrintProfileMatch)
            {
                PrintProfileEntryMatch(profile, entry, pipelineKey);
            }
#endif
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToComputePipelineCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    Pal::DynamicComputeShaderInfo*   pDynamicComputeShaderInfo)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            ApplyProfileToDynamicComputeShaderInfo(
                profileEntry.action.shaders[ShaderStageCompute],
                pDynamicComputeShaderInfo);

#if PAL_ENABLE_PRINTS_ASSERTS
            if (m_settings.pipelineProfileDbgPrintProfileMatch)
            {
                PrintProfileEntryMatch(profile, entry, pipelineKey);
            }
#endif
        }
    }
}

// =====================================================================================================================
Pal::ShaderHash ShaderOptimizer::GetFirstMatchingShaderHash(
    const PipelineProfilePattern& pattern,
    const PipelineOptimizerKey&   pipelineKey)
{
    for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
    {
        const ShaderProfilePattern& shaderPattern = pattern.shaders[stage];

        if (shaderPattern.match.u32All != 0)
        {
            const ShaderOptimizerKey& shaderKey = pipelineKey.shaders[stage];

            if (shaderPattern.match.codeHash &&
                (Pal::ShaderHashesEqual(
                    shaderPattern.codeHash,
                    shaderKey.codeHash)))
            {
                return shaderKey.codeHash;
            }
        }
    }

    Pal::ShaderHash emptyHash = {};
    return emptyHash;
}

// =====================================================================================================================
bool ShaderOptimizer::ProfilePatternMatchesPipeline(
    const PipelineProfilePattern& pattern,
    const PipelineOptimizerKey&   pipelineKey)
{
    if (pattern.match.always)
    {
        return true;
    }

    for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
    {
        const ShaderProfilePattern& shaderPattern = pattern.shaders[stage];

        if (shaderPattern.match.u32All != 0)
        {
            const ShaderOptimizerKey& shaderKey = pipelineKey.shaders[stage];

            // Test if this stage is active in the pipeline
            if (shaderPattern.match.stageActive && (shaderKey.codeSize == 0))
            {
                return false;
            }

            // Test if this stage is inactive in the pipeline
            if (shaderPattern.match.stageInactive && (shaderKey.codeSize != 0))
            {
                return false;
            }

            // Test if lower code hash word matches
            if (shaderPattern.match.codeHash &&
                (shaderPattern.codeHash.lower != shaderKey.codeHash.lower ||
                 shaderPattern.codeHash.upper != shaderKey.codeHash.upper))
            {
                return false;
            }

            // Test by code size (less than)
            if ((shaderPattern.match.codeSizeLessThan != 0) &&
                (shaderPattern.codeSizeLessThanValue >= shaderKey.codeSize))
            {
                return false;
            }
        }
    }

    return true;
}

// =====================================================================================================================
void ShaderOptimizer::BuildTuningProfile()
{
    memset(&m_tuningProfile, 0, sizeof(m_tuningProfile));

    if (m_settings.overrideShaderParams == false)
    {
        return;
    }

    // Only a single entry is currently supported
    m_tuningProfile.entryCount = 1;
    PipelineProfileEntry& entry = m_tuningProfile.entries[0];

    bool matchHash = false;
    if ((m_settings.overrideShaderHashLower != 0) ||
        (m_settings.overrideShaderHashUpper != 0))
    {
        matchHash = true;
    }
    else
    {
        entry.pattern.match.always = 1;
    }

    const uint32_t shaderStage = m_settings.overrideShaderStage;

    VK_ASSERT(shaderStage < ShaderStage::ShaderStageCount);

    ShaderProfilePattern& pattern = entry.pattern.shaders[shaderStage];
    ShaderProfileAction& action   = entry.action.shaders[shaderStage];

    pattern.match.codeHash = matchHash;
    pattern.codeHash.lower = m_settings.overrideShaderHashLower;
    pattern.codeHash.upper = m_settings.overrideShaderHashUpper;

    if (m_settings.overrideNumVGPRsAvailable != 0)
    {
        action.shaderCreate.apply.vgprLimit         = true;
        action.shaderCreate.tuningOptions.vgprLimit = m_settings.overrideNumVGPRsAvailable;
    }

    if (m_settings.overrideMaxLdsSpillDwords != 0)
    {
        action.shaderCreate.apply.ldsSpillLimitDwords         = true;
        action.shaderCreate.tuningOptions.ldsSpillLimitDwords = m_settings.overrideMaxLdsSpillDwords;
    }

    if (m_settings.overrideUserDataSpillThreshold)
    {
        action.shaderCreate.apply.userDataSpillThreshold         = true;
        action.shaderCreate.tuningOptions.userDataSpillThreshold = 0;
    }

    action.shaderCreate.apply.allowReZ = m_settings.overrideAllowReZ;

    switch (m_settings.overrideWaveSize)
    {
    case ShaderWaveSize::WaveSizeAuto:
        break;
    case ShaderWaveSize::WaveSize64:
        action.shaderCreate.apply.waveSize = true;
        action.shaderCreate.tuningOptions.waveSize = 64;
        break;
    case ShaderWaveSize::WaveSize32:
        action.shaderCreate.apply.waveSize = true;
        action.shaderCreate.tuningOptions.waveSize = 32;
        break;
    default:
        VK_NEVER_CALLED();
    }

    switch (m_settings.overrideWgpMode)
    {
    case WgpMode::WgpModeAuto:
        break;
    case WgpMode::WgpModeCu:
        break;
    case WgpMode::WgpModeWgp:
        action.shaderCreate.apply.wgpMode = true;
        break;
    default:
        VK_NEVER_CALLED();
    }

    if (m_settings.overrideWavesPerCu != 0)
    {
        action.dynamicShaderInfo.apply.maxWavesPerCu = true;
        action.dynamicShaderInfo.maxWavesPerCu       = m_settings.overrideWavesPerCu;
    }

    if ((m_settings.overrideCsTgPerCu != 0) &&
        (shaderStage == ShaderStageCompute))
    {
        action.dynamicShaderInfo.apply.maxThreadGroupsPerCu = true;
        action.dynamicShaderInfo.maxThreadGroupsPerCu       = m_settings.overrideCsTgPerCu;
    }

    if (m_settings.overrideUsePbbPerCrc != PipelineBinningModeDefault)
    {
        entry.action.createInfo.apply.binningOverride = true;

        switch (m_settings.overrideUsePbbPerCrc)
        {
        case PipelineBinningModeEnable:
            entry.action.createInfo.binningOverride = Pal::BinningOverride::Enable;
            break;

        case PipelineBinningModeDisable:
            entry.action.createInfo.binningOverride = Pal::BinningOverride::Disable;
            break;

        case PipelineBinningModeDefault:
        default:
            entry.action.createInfo.binningOverride = Pal::BinningOverride::Default;
            break;
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::BuildAppProfile()
{
    memset(&m_appProfile, 0, sizeof(m_appProfile));

    // Early-out if the panel has dictated that we should ignore any active pipeline optimizations due to app profile
    if (m_settings.pipelineProfileIgnoresAppProfile == false)
    {
        {
            BuildAppProfileLlpc();
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::BuildAppProfileLlpc()
{
    const AppProfile appProfile = m_pDevice->GetAppProfile();
    const Pal::GfxIpLevel gfxIpLevel = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxLevel;
    const Pal::AsicRevision asicRevision = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().revision;

    uint32_t i = 0;

    // TODO: These need to be auto-generated from source JSON but for now we write profile programmatically

    if (appProfile == AppProfile::Doom)
    {
        if (Pal::GfxIpLevel::GfxIp9 == gfxIpLevel)
        {
            // Apply late VS alloc to all (graphics) pipelines
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.always = 1;
            m_appProfile.entries[i].action.createInfo.apply.lateAllocVsLimit = true;
            m_appProfile.entries[i].action.createInfo.lateAllocVsLimit = 0;

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // fa535f5e84c8b76eef8212debb55d37f,PS,False,PBB,1
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xef8212debb55d37f;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xfa535f5e84c8b76e;
            m_appProfile.entries[i].action.createInfo.apply.binningOverride = true;
            m_appProfile.entries[i].action.createInfo.binningOverride = Pal::BinningOverride::Disable;
        }
    }
    else if (appProfile == AppProfile::DoomVFR)
    {
        if (Pal::GfxIpLevel::GfxIp9 == gfxIpLevel)
        {
            // Apply late VS alloc to all (graphics) pipelines
            m_appProfile.entryCount = 1;

            m_appProfile.entries[0].pattern.match.always = 1;
            m_appProfile.entries[0].action.createInfo.apply.lateAllocVsLimit = true;
            m_appProfile.entries[0].action.createInfo.lateAllocVsLimit = 0;
        }
    }
    else if (appProfile == AppProfile::Dota2)
    {
        if ((asicRevision >= Pal::AsicRevision::Polaris10) && (asicRevision <= Pal::AsicRevision::Polaris12))
        {
            m_appProfile.entryCount = 8;

            m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xdd6c573c46e6adf8;
            m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x751207727c904749;
            m_appProfile.entries[0].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[1].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[1].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[1].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x71093bf7c6e98da8;
            m_appProfile.entries[1].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xfbc956d87a6d6631;
            m_appProfile.entries[1].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[2].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[2].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[2].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xedd89880de2091f9;
            m_appProfile.entries[2].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x506d0ac3995d2f1b;
            m_appProfile.entries[2].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[3].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[3].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[3].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xbc583b30527e9f1d;
            m_appProfile.entries[3].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x1ef8276d42a14220;
            m_appProfile.entries[3].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[4].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[4].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[4].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x012ddab000f80610;
            m_appProfile.entries[4].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x3a65a6325756203d;
            m_appProfile.entries[4].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[5].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[5].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[5].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x78095b5acf62f4d5;
            m_appProfile.entries[5].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x2c1afc1c6f669e33;
            m_appProfile.entries[5].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[6].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[6].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[6].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x22803b077988ec36;
            m_appProfile.entries[6].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x7ba50586c34e1662;
            m_appProfile.entries[6].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            m_appProfile.entries[7].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[7].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[7].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x313dab8ff9408da0;
            m_appProfile.entries[7].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xbb11905194a55485;
            m_appProfile.entries[7].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;
        }

        if (Pal::GfxIpLevel::GfxIp8 == gfxIpLevel)
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // fd59b52b7db5ef6bf9b17451c9c6cf06,PS,ALLOWREZ,1
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xf9b17451c9c6cf06;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xfd59b52b7db5ef6b;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;
        }
        else if (Pal::GfxIpLevel::GfxIp9 == gfxIpLevel)
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // fd59b52b7db5ef6bf9b17451c9c6cf06,PS,WAVES,24
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xf9b17451c9c6cf06;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xfd59b52b7db5ef6b;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.maxWavesPerCu = 24u;
        }
    }
    else if (appProfile == AppProfile::Talos)
    {
        if (Pal::GfxIpLevel::GfxIp9 == gfxIpLevel)
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // f6c8c56787c45cd28e61611c4549de8e,PS,False,PBB,2
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x8e61611c4549de8e;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xf6c8c56787c45cd2;
            m_appProfile.entries[i].action.createInfo.apply.binningOverride = true;
            m_appProfile.entries[i].action.createInfo.binningOverride = Pal::BinningOverride::Enable;

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 9694cac603ea1b4591072c717850730b,PS,False,WAVES,8
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x91072c717850730b;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x9694cac603ea1b45;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.maxWavesPerCu = 8u;

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 305ea8779d8214f684c7ec88fcb6cf1b,PS,False,PBB,1
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x84c7ec88fcb6cf1b;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x305ea8779d8214f6;
            m_appProfile.entries[i].action.createInfo.apply.binningOverride = true;
            m_appProfile.entries[i].action.createInfo.binningOverride = Pal::BinningOverride::Disable;
        }
        else if (Pal::GfxIpLevel::GfxIp8 == gfxIpLevel)
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 744da8f639e20a25aab87c78fa3a7673,PS,False,WAVES,8
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xaab87c78fa3a7673;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x744da8f639e20a25;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.maxWavesPerCu = 8u;

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 9694cac603ea1b4591072c717850730b,PS,False,WAVES,8
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x91072c717850730b;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x9694cac603ea1b45;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.maxWavesPerCu = 8u;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x435A117D4C9A824B4E7F7BFEB93755B6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x4E7F7BFEB93755B6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x435A117D4C9A824B;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x872392726F4754B0265F213698C57054, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x265F213698C57054;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x872392726F4754B0;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x8294A7FB3EA3A6FC6E14B43995F96871, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x6E14B43995F96871;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x8294A7FB3EA3A6FC;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x2FD1481768E5010166C6B7E7268C1F39, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x66C6B7E7268C1F39;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x2FD1481768E50101;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE394E60E5EC992FD3688A97277E808F7, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x3688A97277E808F7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE394E60E5EC992FD;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xC79A37CD260277EFE5CA053E0978210F, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xE5CA053E0978210F;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xC79A37CD260277EF;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x44FA946844F626968296579A6570BC13, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x8296579A6570BC13;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x44FA946844F62696;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE4B55319684F59F228A2B57C92339574, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x28A2B57C92339574;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE4B55319684F59F2;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xD22A4FE1A6B61288879B2B5C5F578EB0, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x879B2B5C5F578EB0;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xD22A4FE1A6B61288;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xA3EB7292C77A03657E1F46BE56E427AA, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x7E1F46BE56E427AA;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xA3EB7292C77A0365;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xF341093EF870C70A0AECE7808011C4B8, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x0AECE7808011C4B8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xF341093EF870C70A;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB60900B3E1256DDFC7A889DBAC76F591, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xC7A889DBAC76F591;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xB60900B3E1256DDF;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x41DF226419CD26C217CE9268FE52D03B, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x17CE9268FE52D03B;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x41DF226419CD26C2;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x1D9EB7DDBA66FDF78AED19D93B57535B, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x8AED19D93B57535B;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x1D9EB7DDBA66FDF7;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x20E5DA2E5917E2416A43398F36D72603, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x6A43398F36D72603;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x20E5DA2E5917E241;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xF3AF74681BD7980350FBF528DC8AFBA5, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x50FBF528DC8AFBA5;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xF3AF74681BD79803;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x51D59E18E8BD64D9955B7EEAB9F6CDAA, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x955B7EEAB9F6CDAA;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x51D59E18E8BD64D9;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x154112D144C95DE5ECF087B422ED60CE, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xECF087B422ED60CE;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x154112D144C95DE5;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE39F6C59BF345B466DE524A0717A4D67, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x6DE524A0717A4D67;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE39F6C59BF345B46;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB020780B537A01C426365F3E39BE59E6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x26365F3E39BE59E6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xB020780B537A01C4;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xFBAD8E5EE07D12D0F5E3F18201C348E6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xF5E3F18201C348E6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xFBAD8E5EE07D12D0;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xCD911627E2D20F9B7D5DFF0970FB823A, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x7D5DFF0970FB823A;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xCD911627E2D20F9B;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x2DAC71E14EB7945D50DD68ED10CBE1AF, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x50DD68ED10CBE1AF;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x2DAC71E14EB7945D;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x3C1101DC3E3B206E2D99D8DAAF0FE1BE, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x2D99D8DAAF0FE1BE;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x3C1101DC3E3B206E;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x474C4C2966E08232DE5274426C9F365C, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xDE5274426C9F365C;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x474C4C2966E08232;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xD85FA2403788076B3BA507665B126C33, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x3BA507665B126C33;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xD85FA2403788076B;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x6A07F5C0DAAB96E6D1C630198DDC7F21, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xD1C630198DDC7F21;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x6A07F5C0DAAB96E6;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;
    }
    else if (appProfile == AppProfile::SeriousSamFusion)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xC79A37CD260277EFE5CA053E0978210F, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xE5CA053E0978210F;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xC79A37CD260277EF;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x8AF14CFF0496E80BD73AAFA65ED26E2C, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xD73AAFA65ED26E2C;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x8AF14CFF0496E80B;
        m_appProfile.entries[i].action.createInfo.apply.binningOverride = true;
        m_appProfile.entries[i].action.createInfo.binningOverride = Pal::BinningOverride::Enable;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x70379E5982D0D369FBF50B9F866B1DAA, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xFBF50B9F866B1DAA;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x70379E5982D0D369;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x620A4E559EB52DED870DB091946A7585, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x870DB091946A7585;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x620A4E559EB52DED;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB60900B3E1256DDFC7A889DBAC76F591, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xC7A889DBAC76F591;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xB60900B3E1256DDF;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x435A117D4C9A824B4E7F7BFEB93755B6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x4E7F7BFEB93755B6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x435A117D4C9A824B;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xD22A4FE1A6B61288879B2B5C5F578EB0, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x879B2B5C5F578EB0;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xD22A4FE1A6B61288;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x9E4C92D858A5577856901799F5CBB608, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x56901799F5CBB608;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x9E4C92D858A55778;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE394E60E5EC992FD3688A97277E808F7, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x3688A97277E808F7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE394E60E5EC992FD;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xC49272618F2AC58C8E0B62AA62452B75, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x8E0B62AA62452B75;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xC49272618F2AC58C;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;
    }
    else if (appProfile == AppProfile::WarHammerII)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x730EEEB82E6434A876D57AACBD824DBD, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x76D57AACBD824DBD;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x730EEEB82E6434A8;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE449709F7ED22376A6DA0F40D4C8B54F, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xA6DA0F40D4C8B54F;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE449709F7ED22376;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.disableLoopUnrolls = true;
    }
    else if (appProfile == AppProfile::StrangeBrigade)
    {
        if (Pal::GfxIpLevel::GfxIp9 == gfxIpLevel)
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 3895042bcf33699ada756541f86d98d8,CS,False,WAVES,16
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0xda756541f86d98d8;
            m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x3895042bcf33699a;
            m_appProfile.entries[i].action.shaders[ShaderStageCompute].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageCompute].dynamicShaderInfo.maxWavesPerCu = 16u;

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 4aadf469c56d08f3530864a25609abcd,PS,False,WAVES,20
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x530864a25609abcd;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x4aadf469c56d08f3;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.apply.maxWavesPerCu = true;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].dynamicShaderInfo.maxWavesPerCu = 20u;
        }
    }
    else if (appProfile == AppProfile::DawnOfWarIII)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x4D6AE91E42846DDA45C950CF0DA3B6A1, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x45C950CF0DA3B6A1;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x4D6AE91E42846DDA;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x85D431DFF448DCDD802B5059F23C17E7, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0x802B5059F23C17E7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x85D431DFF448DCDD;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x549373FA25856E20D04006855D9CD368, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xD04006855D9CD368;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x549373FA25856E20;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
    }
    else if (appProfile == AppProfile::F1_2017)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x7C92A52E3084149659025B19EDAE3734, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0x59025B19EDAE3734;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x7C92A52E30841496;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x767991F055DE051DEC878C820BD1D81E, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xEC878C820BD1D81E;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x767991F055DE051D;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x8648E5203943C0B00EBEFF2CBF131944, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x0EBEFF2CBF131944;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x8648E5203943C0B0;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
void ShaderOptimizer::PrintProfileEntryMatch(
    const PipelineProfile&      profile,
    uint32_t                    index,
    const PipelineOptimizerKey& key)
{
    Util::MutexAuto lock(&m_printMutex);

    const char* pProfile = "Unknown profile";

    if (&profile == &m_appProfile)
    {
        pProfile = "Application";
    }
#if ICD_RUNTIME_APP_PROFILE
    else if (&profile == &m_runtimeProfile)
    {
        pProfile = "Runtime";
    }
#endif
    else
    {
        VK_NEVER_CALLED();
    }

    Util::DbgPrintf(Util::DbgPrintCatInfoMsg, Util::DbgPrintStyleDefault,
        "%s pipeline profile entry %u triggered for pipeline:", pProfile, index);

    for (uint32_t stageIdx = 0; stageIdx < ShaderStageCount; ++stageIdx)
    {
        const auto& shader = key.shaders[stageIdx];

        if (shader.codeSize != 0)
        {
            const char* pStage = "???";

            switch (stageIdx)
            {
            case ShaderStageVertex:
                pStage = "VS";
                break;
            case ShaderStageTessControl:
                pStage = "HS";
                break;
            case ShaderStageTessEvaluation:
                pStage = "DS";
                break;
            case ShaderStageGeometry:
                pStage = "GS";
                break;
            case ShaderStageFragment:
                pStage = "PS";
                break;
            case ShaderStageCompute:
                pStage = "CS";
                break;
            default:
                VK_NEVER_CALLED();
                break;
            }

            Util::DbgPrintf(Util::DbgPrintCatInfoMsg, Util::DbgPrintStyleDefault,
                "  %s: Hash: %016llX %016llX Size: %8zu", pStage, shader.codeHash.upper, shader.codeHash.lower, shader.codeSize);
        }
    }
}
#endif

#if ICD_RUNTIME_APP_PROFILE

// =====================================================================================================================
// Tests that each key in the given JSON object matches at least one of the keys in the array.
static bool CheckValidKeys(
    utils::Json* pObject,
    size_t       numKeys,
    const char** pKeys)
{
    bool success = true;

    if ((pObject != nullptr) && (pObject->type == utils::JsonValueType::Object))
    {
        for (utils::Json* pChild = pObject->pChild; success && (pChild != nullptr); pChild = pChild->pNext)
        {
            if (pChild->pKey != nullptr)
            {
                bool found = false;

                for (size_t i = 0; (found == false) && (i < numKeys); ++i)
                {
                    found |= (strcmp(pKeys[i], pChild->pKey) == 0);
                }

                success &= found;
            }
        }
    }
    else
    {
        success = false;
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfileActionShader(
    utils::Json*         pJson,
    ShaderStage          shaderStage,
    ShaderProfileAction* pActions)
{
    bool success = true;
    utils::Json* pItem = nullptr;

    static const char* ValidKeys[] =
    {
        "optStrategyFlags",
        "maxOccupancyOptions",
        "lowLatencyOptions",
        "vgprLimit",
        "sgprLimit",
        "ldsSpillLimitDwords",
        "maxArraySizeForFastDynamicIndexing",
        "userDataSpillThreshold",
        "maxThreadGroupsPerComputeUnit",
#if PAL_DEVELOPER_BUILD
        "scOptions",
        "scOptionsMask",
        "scSetOption",
#endif
        "maxWavesPerCu",
        "cuEnableMask",
        "maxThreadGroupsPerCu",
        "trapPresent",
        "debugMode",
        "disableLoopUnrolls",
        "enableSelectiveInline",
        "useSiScheduler",
        "reconfigWorkgroupLayout",
        "enableLoadScalarizer",
        "waveSize",
        "wgpMode",
        "waveBreakSize",
        "nggDisable",
        "nggFasterLaunchRate",
        "nggVertexReuse",
        "nggEnableFrustumCulling",
        "nggEnableBoxFilterCulling",
        "nggEnableSphereCulling",
        "nggEnableBackfaceCulling",
        "nggEnableSmallPrimFilter",
        "enableSubvector",
        "enableSubvectorSharedVgprs",
    };

    success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    if ((pItem = utils::JsonGetValue(pJson, "vgprLimit")) != nullptr)
    {
        pActions->shaderCreate.apply.vgprLimit         = true;
        pActions->shaderCreate.tuningOptions.vgprLimit = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "sgprLimit")) != nullptr)
    {
        pActions->shaderCreate.apply.sgprLimit         = true;
        pActions->shaderCreate.tuningOptions.sgprLimit = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "ldsSpillLimitDwords")) != nullptr)
    {
        pActions->shaderCreate.apply.ldsSpillLimitDwords         = true;
        pActions->shaderCreate.tuningOptions.ldsSpillLimitDwords = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxArraySizeForFastDynamicIndexing")) != nullptr)
    {
        pActions->shaderCreate.apply.maxArraySizeForFastDynamicIndexing         = true;
        pActions->shaderCreate.tuningOptions.maxArraySizeForFastDynamicIndexing = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "userDataSpillThreshold")) != nullptr)
    {
        pActions->shaderCreate.apply.userDataSpillThreshold         = true;
        pActions->shaderCreate.tuningOptions.userDataSpillThreshold = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxThreadGroupsPerComputeUnit")) != nullptr)
    {
        pActions->shaderCreate.apply.maxThreadGroupsPerComputeUnit         = true;
        pActions->shaderCreate.tuningOptions.maxThreadGroupsPerComputeUnit = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "trapPresent")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.trapPresent = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "debugMode")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.debugMode = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "disableLoopUnrolls")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.disableLoopUnrolls = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "enableSelectiveInline")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.enableSelectiveInline = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "waveSize")) != nullptr)
    {
        pActions->shaderCreate.apply.waveSize = true;
        pActions->shaderCreate.tuningOptions.waveSize = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "wgpMode")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.wgpMode = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "waveBreakSize")) != nullptr)
    {
        pActions->shaderCreate.apply.waveBreakSize = true;
        pActions->shaderCreate.tuningOptions.waveBreakSize = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggDisable")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggDisable = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggFasterLaunchRate")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggFasterLaunchRate = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggVertexReuse")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggVertexReuse = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggEnableFrustumCulling")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggEnableFrustumCulling = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggEnableBoxFilterCulling")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggEnableBoxFilterCulling = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggEnableSphereCulling")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggEnableSphereCulling = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggEnableBackfaceCulling")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggEnableBackfaceCulling = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "nggEnableSmallPrimFilter")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.nggEnableSmallPrimFilter = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "enableSubvector")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.enableSubvector = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "enableSubvectorSharedVgprs")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.apply.enableSubvectorSharedVgprs = 1;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxWavesPerCu")) != nullptr)
    {
        pActions->dynamicShaderInfo.apply.maxWavesPerCu = true;
        pActions->dynamicShaderInfo.maxWavesPerCu       = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "cuEnableMask")) != nullptr)
    {
        if (shaderStage != ShaderStageCompute)
        {
            pActions->dynamicShaderInfo.apply.cuEnableMask = true;
            pActions->dynamicShaderInfo.cuEnableMask       = static_cast<uint32_t>(pItem->integerValue);
        }
        else
        {
            success = false;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxThreadGroupsPerCu")) != nullptr)
    {
        if (shaderStage == ShaderStageCompute)
        {
            pActions->dynamicShaderInfo.apply.maxThreadGroupsPerCu = true;
            pActions->dynamicShaderInfo.maxThreadGroupsPerCu       = static_cast<uint32_t>(pItem->integerValue);
        }
        else
        {
            success = false;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "useSiScheduler")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.tuningOptions.useSiScheduler = true;
        }
    }
    if ((pItem = utils::JsonGetValue(pJson, "reconfigWorkgroupLayout")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.tuningOptions.reconfigWorkgroupLayout = true;
        }
    }

    if ((pItem = utils::JsonGetValue(pJson, "enableLoadScalarizer")) != nullptr)
    {
        if (pItem->integerValue != 0)
        {
            pActions->shaderCreate.tuningOptions.enableLoadScalarizer = true;
        }
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfileEntryAction(
    utils::Json*           pJson,
    PipelineProfileAction* pAction)
{
    bool success = true;

    static const char* ValidKeys[] =
    {
        "lateAllocVsLimit",
        "vs",
        "hs",
        "ds",
        "gs",
        "ps",
        "cs"
    };

    success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    utils::Json* pItem;

    if ((pItem = utils::JsonGetValue(pJson, "lateAllocVsLimit")) != nullptr)
    {
        pAction->createInfo.apply.lateAllocVsLimit = true;
        pAction->createInfo.lateAllocVsLimit       = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "vs")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageVertex, &pAction->shaders[ShaderStageVertex]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "hs")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageTessControl,
            &pAction->shaders[ShaderStageTessControl]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "ds")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageTessEvaluation,
            &pAction->shaders[ShaderStageTessEvaluation]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "gs")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageGeometry, &pAction->shaders[ShaderStageGeometry]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "ps")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageFragment, &pAction->shaders[ShaderStageFragment]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "cs")) != nullptr)
    {
        success &= ParseJsonProfileActionShader(pItem, ShaderStageCompute, &pAction->shaders[ShaderStageCompute]);
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfilePatternShader(
    utils::Json*          pJson,
    ShaderStage           shaderStage,
    ShaderProfilePattern* pPattern)
{
    bool success = true;

    static const char* ValidKeys[] =
    {
        "stageActive",
        "stageInactive",
        "codeHash",
        "codeSizeLessThan"
    };

    success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    utils::Json* pItem = nullptr;

    if ((pItem = utils::JsonGetValue(pJson, "stageActive")) != nullptr)
    {
        pPattern->match.stageActive = pItem->booleanValue;
    }

    if ((pItem = utils::JsonGetValue(pJson, "stageInactive")) != nullptr)
    {
        pPattern->match.stageInactive = pItem->booleanValue;
    }

    // The hash is a 128-bit value interpreted from a JSON hex string.  It should be split by a space into two
    // 64-bit sections, e.g.: { "codeHash" : "0x1234567812345678 1234567812345678" }.
    if ((pItem = utils::JsonGetValue(pJson, "codeHash")) != nullptr)
    {
        char* pLower64 = nullptr;

        pPattern->match.codeHash = true;

        pPattern->codeHash.upper = strtoull(pItem->pStringValue, &pLower64, 16);
        pPattern->codeHash.lower = strtoull(pLower64, nullptr, 16);
    }

    if ((pItem = utils::JsonGetValue(pJson, "codeSizeLessThan")) != nullptr)
    {
        pPattern->match.codeSizeLessThan = true;

        pPattern->codeSizeLessThanValue  = static_cast<size_t>(pItem->integerValue);
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfileEntryPattern(
    utils::Json*            pJson,
    PipelineProfilePattern* pPattern)
{
    bool success = true;

    static const char* ValidKeys[] =
    {
        "always",
        "vs",
        "hs",
        "ds",
        "gs",
        "ps",
        "cs"
    };

    success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    utils::Json* pItem = nullptr;

    if ((pItem = utils::JsonGetValue(pJson, "always")) != nullptr)
    {
        pPattern->match.always = pItem->booleanValue;
    }

    if ((pItem = utils::JsonGetValue(pJson, "vs")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageVertex, &pPattern->shaders[ShaderStageVertex]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "hs")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageTessControl,
                                                 &pPattern->shaders[ShaderStageTessControl]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "ds")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageTessEvaluation,
                                                 &pPattern->shaders[ShaderStageTessEvaluation]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "gs")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageGeometry, &pPattern->shaders[ShaderStageGeometry]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "ps")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageFragment, &pPattern->shaders[ShaderStageFragment]);
    }

    if ((pItem = utils::JsonGetValue(pJson, "cs")) != nullptr)
    {
        success &= ParseJsonProfilePatternShader(pItem, ShaderStageCompute, &pPattern->shaders[ShaderStageCompute]);
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfileEntry(
    utils::Json*          pPatterns,
    utils::Json*          pActions,
    utils::Json*          pEntry,
    PipelineProfileEntry* pProfileEntry)
{
    bool success = true;

    static const char* ValidKeys[] =
    {
        "pattern",
        "action"
    };

    success &= CheckValidKeys(pEntry, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    PipelineProfileEntry entry = {};

    utils::Json* pPattern = utils::JsonGetValue(pEntry, "pattern");

    if (pPattern != nullptr)
    {
        if (pPattern->type == utils::JsonValueType::String)
        {
            pPattern = (pPatterns != nullptr) ? utils::JsonGetValue(pPatterns, pPattern->pStringValue) : nullptr;
        }
    }

    if (pPattern != nullptr && pPattern->type != utils::JsonValueType::Object)
    {
        pPattern = nullptr;
    }

    utils::Json* pAction = utils::JsonGetValue(pEntry, "action");

    if (pAction != nullptr)
    {
        if (pAction->type == utils::JsonValueType::String)
        {
            pAction = (pActions != nullptr) ? utils::JsonGetValue(pActions, pAction->pStringValue) : nullptr;
        }
    }

    if (pAction != nullptr && pAction->type != utils::JsonValueType::Object)
    {
        pAction = nullptr;
    }

    if (pPattern != nullptr && pAction != nullptr)
    {
        success &= ParseJsonProfileEntryPattern(pPattern, &pProfileEntry->pattern);
        success &= ParseJsonProfileEntryAction(pAction, &pProfileEntry->action);
    }
    else
    {
        success = false;
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfile(
    utils::Json*     pJson,
    PipelineProfile* pProfile)
{
/*  Example of the run-time profile:
    {
      "entries": [
        {
          "pattern": {
            "always": false,
            "vs": {
              "stageActive": true,
              "codeHash": "0x0 0x7B9BFA968C24EB11"
            }
          },
          "action": {
            "lateAllocVsLimit": 1000000,
            "vs": {
              "maxThreadGroupsPerComputeUnit": 10
            }
          }
        }
      ]
    }
*/

    bool success = true;

    if (pJson != nullptr)
    {
        utils::Json* pEntries  = utils::JsonGetValue(pJson, "entries");
        utils::Json* pPatterns = utils::JsonGetValue(pJson, "patterns");
        utils::Json* pActions  = utils::JsonGetValue(pJson, "actions");

        if (pEntries != nullptr)
        {
            for (utils::Json* pEntry = pEntries->pChild; (pEntry != nullptr) && success; pEntry = pEntry->pNext)
            {
                if (pProfile->entryCount < MaxPipelineProfileEntries)
                {
                    success &= ParseJsonProfileEntry(pPatterns, pActions, pEntry, &pProfile->entries[pProfile->entryCount++]);
                }
                else
                {
                    success = false;
                }
            }
        }
    }
    else
    {
        success = false;
    }

    return success;
}

// =====================================================================================================================
void ShaderOptimizer::RuntimeProfileParseError()
{
    VK_ASSERT(false && "Failed to parse runtime pipeline profile file");

    // Trigger an infinite loop if the panel setting is set to notify that a profile parsing failure has occurred
    // on release driver builds where asserts are not compiled in.
    while (m_settings.pipelineProfileHaltOnParseFailure)
    {
    }
}

// =====================================================================================================================
void ShaderOptimizer::BuildRuntimeProfile()
{
    memset(&m_runtimeProfile, 0, sizeof(m_runtimeProfile));

    utils::JsonSettings jsonSettings = utils::JsonMakeInstanceSettings(m_pDevice->VkInstance());
    utils::Json* pJson               = nullptr;

    if (m_settings.pipelineProfileRuntimeFile[0] != '\0')
    {
        Util::File jsonFile;

        if (jsonFile.Open(m_settings.pipelineProfileRuntimeFile, Util::FileAccessRead) == Pal::Result::Success)
        {
            size_t size = jsonFile.GetFileSize(m_settings.pipelineProfileRuntimeFile);

            void* pJsonBuffer = m_pDevice->VkInstance()->AllocMem(size, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pJsonBuffer != nullptr)
            {
                size_t bytesRead = 0;

                jsonFile.Read(pJsonBuffer, size, &bytesRead);

                if (bytesRead > 0)
                {
                    pJson = utils::JsonParse(jsonSettings, pJsonBuffer, bytesRead);

                    if (pJson != nullptr)
                    {
                        bool success = ParseJsonProfile(pJson, &m_runtimeProfile);

                        if (success == false)
                        {
                            // Failed to parse some part of the profile (e.g. unsupported/missing key name)
                            RuntimeProfileParseError();
                        }

                        utils::JsonDestroy(jsonSettings, pJson);
                    }
                    else
                    {
                        // Failed to parse JSON file entirely
                        RuntimeProfileParseError();
                    }
                }

                m_pDevice->VkInstance()->FreeMem(pJsonBuffer);
            }

            jsonFile.Close();
        }
    }
}
#endif // ICD_RUNTIME_APP_PROFILE

};
