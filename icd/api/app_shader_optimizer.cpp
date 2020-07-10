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
                    options.pOptions->disableLoopUnroll = true;
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
#endif

};
