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
**************************************************************************************************
* @file  app_shader_optimizer.cpp
* @brief Functions for tuning specific shader compile parameters for optimized code generation.
**************************************************************************************************
*/
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 39
#define Vkgc Llpc
#endif

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

    if (m_settings.enablePipelineProfileDumping)
    {
        m_appShaderProfile.PipelineProfileToJson(m_tuningProfile, m_settings.pipelineProfileDumpFile);
    }

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
                if (shaderCreate.tuningOptions.forceLoopUnrollCount != 0)
                {
                    options.pOptions->forceLoopUnrollCount = shaderCreate.tuningOptions.forceLoopUnrollCount;
                }
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 35
                if (shaderCreate.tuningOptions.disableLicm)
                {
                    options.pOptions->disableLicm = true;
                }
#endif
                if (shaderCreate.tuningOptions.unrollThreshold != 0)
                {
                    options.pOptions->unrollThreshold = shaderCreate.tuningOptions.unrollThreshold;
                }
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
                        static_cast<Vkgc::WaveBreakSize>(shaderCreate.tuningOptions.waveBreakSize);
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

    action.shaderCreate.apply.allowReZ                = m_settings.overrideAllowReZ;
    action.shaderCreate.apply.enableSelectiveInline   = m_settings.overrideEnableSelectiveInline;

    if (m_settings.overrideDisableLoopUnrolls)
    {
        action.shaderCreate.apply.disableLoopUnrolls = true;
    }

    if (m_settings.overrideUseSiScheduler)
    {
        action.shaderCreate.tuningOptions.useSiScheduler = true;
    }

    if (m_settings.overrideReconfigWorkgroupLayout)
    {
        action.shaderCreate.tuningOptions.reconfigWorkgroupLayout = true;
    }

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

    action.shaderCreate.apply.nggDisable      = m_settings.overrideUseNgg;
    action.shaderCreate.apply.enableSubvector = m_settings.overrideEnableSubvector;

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

    m_appShaderProfile.BuildAppProfileLlpc(appProfile, gfxIpLevel, &m_appProfile);

    if (appProfile == AppProfile::Dota2)
    {
        if ((asicRevision >= Pal::AsicRevision::Polaris10) && (asicRevision <= Pal::AsicRevision::Polaris12))
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xdd6c573c46e6adf8;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x751207727c904749;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x71093bf7c6e98da8;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xfbc956d87a6d6631;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xedd89880de2091f9;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x506d0ac3995d2f1b;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xbc583b30527e9f1d;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x1ef8276d42a14220;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x012ddab000f80610;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x3a65a6325756203d;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x78095b5acf62f4d5;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x2c1afc1c6f669e33;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x22803b077988ec36;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x7ba50586c34e1662;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x313dab8ff9408da0;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xbb11905194a55485;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.apply.allowReZ = true;
        }
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
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.unrollThreshold = 900;

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
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

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
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.unrollThreshold = 900;

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

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x7FE1BAB7A796A7977765A6FAEB079FF9, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x7765A6FAEB079FF9;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x7FE1BAB7A796A797;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.unrollThreshold = 900;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB020780B537A01C426365F3E39BE59E6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x26365F3E39BE59E6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xB020780B537A01C4;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE0A7E913F914052F26CAD2EF6C447782, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x26CAD2EF6C447782;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xE0A7E913F914052F;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x154112D144C95DE5ECF087B422ED60CE, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xECF087B422ED60CE;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x154112D144C95DE5;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
    }
    else if (appProfile == AppProfile::WarHammerII)
    {
        if (Pal::GfxIpLevel::GfxIp9 >= gfxIpLevel)
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
        else
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 0x730EEEB82E6434A876D57AACBD824DBD, PS
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x76D57AACBD824DBD;
            m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x730EEEB82E6434A8;
            m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.forceLoopUnrollCount = 2;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // All PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.match.always = 1;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.unrollThreshold = 2150;

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
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xBC9A17755B98BA8FB2DCAC35E290D358, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0xB2DCAC35E290D358;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0xBC9A17755B98BA8F;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x009DF75BD4DEBCA4D287C35F916A0082, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xD287C35F916A0082;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x009DF75BD4DEBCA4;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x8A6DB015D18CFA5AE7E4820E902750AD, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0xE7E4820E902750AD;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x8A6DB015D18CFA5A;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x7C7BE79C0C10118B2579C71E29F36AD5, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x2579C71E29F36AD5;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x7C7BE79C0C10118B;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x081F1C2167F254DBE3A7759FCC6120F6, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0xE3A7759FCC6120F6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x081F1C2167F254DB;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x85D431DFF448DCDD802B5059F23C17E7, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0x802B5059F23C17E7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x85D431DFF448DCDD;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x91950C2AEB1BC2A9A548718852253D37, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0xA548718852253D37;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x91950C2AEB1BC2A9;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x6B53D1FB5D5AD9EEF5BCDB9801E6037B, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xF5BCDB9801E6037B;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x6B53D1FB5D5AD9EE;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE58161C4EDB2021F62729DC182370222, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x62729DC182370222;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0xE58161C4EDB2021F;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE8D91B9B085305105DBE1F87F1B3EBEF, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x5DBE1F87F1B3EBEF;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0xE8D91B9B08530510;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;
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
    else if (appProfile == AppProfile::RiseOfTheTombra)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x505651AC055F537AC4D98DF71AFD07B9, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xC4D98DF71AFD07B9;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x505651AC055F537A;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xD226BA9E2A5BD8D69BF782559B7E4C74, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x9BF782559B7E4C74;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xD226BA9E2A5BD8D6;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x67F3B02E8A7707129396E9EF9FF3F616, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x9396E9EF9FF3F616;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x67F3B02E8A770712;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xA1BAD82B65AB576032661ABEE21C5880, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x32661ABEE21C5880;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xA1BAD82B65AB5760;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x349F0CB03A54D3FB849A89C60B8D27F6, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x849A89C60B8D27F6;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x349F0CB03A54D3FB;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB0F626077CDBD932B1372DAD4EC698E7, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xB1372DAD4EC698E7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xB0F626077CDBD932;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xD33A2617087D2E05DC3E0E358DBE7EC8, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xDC3E0E358DBE7EC8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xD33A2617087D2E05;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x88E3517DBB964D7F2AE8021F616DA377, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x2AE8021F616DA377;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x88E3517DBB964D7F;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xC7A986985A693F84406B42FB06DA1DB4, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x406B42FB06DA1DB4;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xC7A986985A693F84;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xAB7E77553A4790F75213D389B1F5F611, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x5213D389B1F5F611;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xAB7E77553A4790F7;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x579F291BA16E852607C84A20ECE7A38D, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x07C84A20ECE7A38D;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x579F291BA16E8526;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xF8D297EBB07612CD476E6779BD4275FD, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0x476E6779BD4275FD;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0xF8D297EBB07612CD;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x6CFA095297E1EBD9C5503C7ADCB99118, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0xC5503C7ADCB99118;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0x6CFA095297E1EBD9;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x8A53595F143C1D121A19414C76783DF3, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x1A19414C76783DF3;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x8A53595F143C1D12;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x1784CCA187CA21315C992B149FDE1ACA, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x5C992B149FDE1ACA;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x1784CCA187CA2131;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x2F8BD73037839855F514EACF88A02A7D, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xF514EACF88A02A7D;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x2F8BD73037839855;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.enableLoadScalarizer = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x77C34B1C08B3C185BE3C260D6EC230AF, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0xBE3C260D6EC230AF;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x77C34B1C08B3C185;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x86C9BAF4D3389204601E54D3F386CDB8, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x601E54D3F386CDB8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x86C9BAF4D3389204;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xB396D24454277733EFC9AABA5E5EA0E9, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0xEFC9AABA5E5EA0E9;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0xB396D24454277733;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x59790C4419A57854C5D36B08C4B35CB7, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0xC5D36B08C4B35CB7;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x59790C4419A57854;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xCA6C4EC171D01EC11436591682E49AA4, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x1436591682E49AA4;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0xCA6C4EC171D01EC1;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x6AA53B86573DCB393FD5A53E2881BCAA, VS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.lower = 0x3FD5A53E2881BCAA;
        m_appProfile.entries[i].pattern.shaders[ShaderStageVertex].codeHash.upper = 0x6AA53B86573DCB39;
        m_appProfile.entries[i].action.shaders[ShaderStageVertex].shaderCreate.tuningOptions.useSiScheduler = true;
    }
    else if (appProfile == AppProfile::DiRT4)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xE4EB5EA3FB70EEB72989E44BA02788A8, CS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.lower = 0x2989E44BA02788A8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageCompute].codeHash.upper = 0xE4EB5EA3FB70EEB7;
        m_appProfile.entries[i].action.shaders[ShaderStageCompute].shaderCreate.apply.disableLoopUnrolls = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x86BB99062EADF83958DDFE449FC0B3D8, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x58DDFE449FC0B3D8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x86BB99062EADF839;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x5A0E57337FE9533E52BFE4AF6F91CA9E, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x52BFE4AF6F91CA9E;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x5A0E57337FE9533E;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xDCDD10E03921B0659F8DE289B463BEA5, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x9F8DE289B463BEA5;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xDCDD10E03921B065;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x98245ECF0F55D763AD0576DABD7C7E8A, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0xAD0576DABD7C7E8A;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x98245ECF0F55D763;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0x70291F07F48E9C91017842590A932DC9, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x017842590A932DC9;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0x70291F07F48E9C91;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 0xA645928927B0469358BFFF013C011EC8, PS
        i = m_appProfile.entryCount++;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.stageActive = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].match.codeHash = true;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.lower = 0x58BFFF013C011EC8;
        m_appProfile.entries[i].pattern.shaders[ShaderStageFragment].codeHash.upper = 0xA645928927B04693;
        m_appProfile.entries[i].action.shaders[ShaderStageFragment].shaderCreate.tuningOptions.useSiScheduler = true;
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
                        bool success = m_appShaderProfile.ParseJsonProfile(pJson, &m_runtimeProfile);

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
