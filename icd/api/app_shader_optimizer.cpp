/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

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

#if ICD_RUNTIME_APP_PROFILE
    BuildRuntimeProfile();
#endif
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToShaderCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    ShaderStage                      shaderStage,
    Pal::ShaderCreateInfo*           pCreateInfo)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            const auto& shaderCreate = profileEntry.action.shaders[static_cast<uint32_t>(shaderStage)].shaderCreate;

            if (shaderCreate.apply.optStrategyFlags)
            {
                pCreateInfo->optStrategy.flags = shaderCreate.optStrategyFlags;
            }

            if (shaderCreate.apply.vgprLimit)
            {
                pCreateInfo->optStrategy.vgprLimit = shaderCreate.vgprLimit;
            }

            if (shaderCreate.apply.maxLdsSpillDwords)
            {
                pCreateInfo->optStrategy.maxLdsSpillDwords = shaderCreate.maxLdsSpillDwords;
            }

            if (shaderCreate.apply.minVgprStrategyFlags)
            {
                pCreateInfo->optStrategy.minVgprStrategyFlags = shaderCreate.minVgprStrategyFlags;
            }

            if (shaderCreate.apply.userDataSpillThreshold)
            {
                pCreateInfo->optStrategy.userDataSpillThreshold = shaderCreate.userDataSpillThreshold;
            }

        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::OverrideShaderCreateInfo(
    const PipelineOptimizerKey& pipelineKey,
    ShaderStage                 shaderStage,
    Pal::ShaderCreateInfo*      pCreateInfo)
{
    ApplyProfileToShaderCreateInfo(m_appProfile, pipelineKey, shaderStage, pCreateInfo);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToShaderCreateInfo(m_runtimeProfile, pipelineKey, shaderStage, pCreateInfo);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::OverrideGraphicsPipelineCreateInfo(
    const PipelineOptimizerKey&      pipelineKey,
    Pal::GraphicsPipelineCreateInfo* pCreateInfo,
    Pal::DynamicGraphicsShaderInfos* pGraphicsWaveLimitParams)
{
    ApplyProfileToGraphicsPipelineCreateInfo(m_appProfile, pipelineKey, pCreateInfo, pGraphicsWaveLimitParams);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToGraphicsPipelineCreateInfo(m_runtimeProfile, pipelineKey, pCreateInfo, pGraphicsWaveLimitParams);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::OverrideComputePipelineCreateInfo(
    const PipelineOptimizerKey&     pipelineKey,
    Pal::ComputePipelineCreateInfo* pCreateInfo,
    Pal::DynamicComputeShaderInfo*  pDynamicCompueShaderInfo)
{
    ApplyProfileToComputePipelineCreateInfo(m_appProfile, pipelineKey, pCreateInfo, pDynamicCompueShaderInfo);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToComputePipelineCreateInfo(m_runtimeProfile, pipelineKey, pCreateInfo, pDynamicCompueShaderInfo);
#endif
}

// =====================================================================================================================
ShaderOptimizer::~ShaderOptimizer()
{

}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToGraphicsPipelineCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    Pal::GraphicsPipelineCreateInfo* pCreateInfo,
    Pal::DynamicGraphicsShaderInfos* pGraphicsWaveLimitParams)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            // Apply parameters to PipelineShaderInfo structs
            const auto& shaders = profileEntry.action.shaders;

            // Apply parameters to GraphicsPipelineCreateInfo
            const auto& createInfo = profileEntry.action.createInfo;

            if (createInfo.apply.lateAllocVsLimit)
            {
                pCreateInfo->useLateAllocVsLimit = true;
                pCreateInfo->lateAllocVsLimit    = createInfo.lateAllocVsLimit;
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
void ShaderOptimizer::ApplyProfileToComputePipelineShaderInfo(
    const ShaderProfileAction& actions,
    Pal::DynamicComputeShaderInfo* pDynamicComputeShaderInfo)
{
    const auto& pipelineShader = actions.pipelineShader;

}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToComputePipelineCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    Pal::ComputePipelineCreateInfo*  pCreateInfo,
    Pal::DynamicComputeShaderInfo*   pDynamicComputeShaderInfo)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.entries[entry];

        if (ProfilePatternMatchesPipeline(profileEntry.pattern, pipelineKey))
        {
            ApplyProfileToComputePipelineShaderInfo(
                profileEntry.action.shaders[ShaderStageCompute],
                pDynamicComputeShaderInfo);
        }
    }
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
void ShaderOptimizer::BuildAppProfile()
{
    const AppProfile appProfile      = m_pDevice->GetAppProfile();
    const Pal::GfxIpLevel gfxIpLevel = m_pDevice->VkPhysicalDevice()->PalProperties().gfxLevel;

    // TODO: These need to be auto-generated from source JSON but for now we write profile programmatically
    memset(&m_appProfile, 0, sizeof(m_appProfile));

    // Early-out if the panel has dictated that we should ignore any active pipeline optimizations due to app profile
    if (m_settings.pipelineProfileIgnoresAppProfile)
    {
        return;
    }

    if (appProfile == AppProfile::Dota2)
    {
        // DOTA 2 has a 4K downsample shader that does nothing but vmem instructions in a loop.  Thread trace view
        // shows latency in those instructions' issuing, presumably because there is some bottleneck between SQ and the
        // TAs.  Limiting waves per CU improves performance of this shader significantly.
        m_appProfile.entryCount = 1;

        m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].match.codeHash      = true;
        m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].codeHash.upper      = 0x9022c6756bbe4314ULL;
        m_appProfile.entries[0].pattern.shaders[ShaderStageFragment].codeHash.lower      = 0x1bf99c3524392578ULL;
        m_appProfile.entries[0].action.shaders[ShaderStageFragment].pipelineShader.apply.maxWavesPerCu = true;
        m_appProfile.entries[0].action.shaders[ShaderStageFragment].pipelineShader.maxWavesPerCu       = 2;
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
static bool ParseJsonMinVgprStrategyFlags(
    utils::Json*                     pJson,
    Pal::ShaderMinVgprStrategyFlags* pFlags)
{
    bool success = true;

    if (pJson->type == utils::JsonValueType::Number)
    {
        pFlags->u32All = static_cast<uint32_t>(pJson->integerValue);
    }
    else if (pJson->type == utils::JsonValueType::Object)
    {
        utils::Json* pItem = nullptr;

        pFlags->u32All = 0;

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageGcmseq")) != nullptr)
        {
            pFlags->minimizeVgprsUsageGcmseq = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageSched")) != nullptr)
        {
            pFlags->minimizeVgprsUsageSched = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageRegAlloc")) != nullptr)
        {
            pFlags->minimizeVgprsUsageRegAlloc = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageMergeChain")) != nullptr)
        {
            pFlags->minimizeVgprsUsageMergeChain = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsagePeepHole")) != nullptr)
        {
            pFlags->minimizeVgprsUsagePeepHole = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageCubeCoord")) != nullptr)
        {
            pFlags->minimizeVgprsUsageCubeCoord = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageFactorMad")) != nullptr)
        {
            pFlags->minimizeVgprsUsageFactorMad = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageVN")) != nullptr)
        {
            pFlags->minimizeVgprsUsageVN = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprsUsageBCM")) != nullptr)
        {
            pFlags->minimizeVgprsUsageBCM = pItem->booleanValue;
        }
    }
    else
    {
        success = false;
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonOptStrategyFlags(
    utils::Json*                          pJson,
    Pal::ShaderOptimizationStrategyFlags* pFlags)
{
    bool success = true;

    if (pJson->type == utils::JsonValueType::Number)
    {
        pFlags->u32All = static_cast<uint32_t>(pJson->integerValue);
    }
    else if (pJson->type == utils::JsonValueType::Object)
    {
        utils::Json* pItem = nullptr;

        pFlags->u32All = 0;

        if ((pItem = utils::JsonGetValue(pJson, "minimizeVgprs")) != nullptr)
        {
            pFlags->minimizeVgprs = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "clientVgprLimit")) != nullptr)
        {
            pFlags->clientVgprLimit = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "userDataSpill")) != nullptr)
        {
            pFlags->userDataSpill = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScGroup")) != nullptr)
        {
            pFlags->useScGroup = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScLiveness")) != nullptr)
        {
            pFlags->useScLiveness = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScRemat")) != nullptr)
        {
            pFlags->useScRemat = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScUseMoreD16")) != nullptr)
        {
            pFlags->useScUseMoreD16 = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScUseUnsafeMadMix")) != nullptr)
        {
            pFlags->useScUseUnsafeMadMix = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScUnsafeConvertToF16")) != nullptr)
        {
            pFlags->useScUnsafeConvertToF16 = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "removeNullParameterExports")) != nullptr)
        {
            pFlags->removeNullParameterExports = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScAggressiveHoist")) != nullptr)
        {
            pFlags->useScAggressiveHoist = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useScXnackEnable")) != nullptr)
        {
            pFlags->useScXnackEnable = pItem->booleanValue;
        }

        if ((pItem = utils::JsonGetValue(pJson, "useNonIeeeFpInstructions")) != nullptr)
        {
            pFlags->useNonIeeeFpInstructions = pItem->booleanValue;
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
    utils::Json*          pJson,
    ShaderStage           shaderStage,
    ShaderProfileAction* pActions)
{
    bool success = true;
    utils::Json* pItem = nullptr;

    if ((pItem = utils::JsonGetValue(pJson, "optStrategyFlags")) != nullptr)
    {
        pActions->shaderCreate.apply.optStrategyFlags = true;

        success &= ParseJsonOptStrategyFlags(pItem, &pActions->shaderCreate.optStrategyFlags);
    }

    if ((pItem = utils::JsonGetValue(pJson, "vgprLimit")) != nullptr)
    {
        pActions->shaderCreate.apply.vgprLimit = true;
        pActions->shaderCreate.vgprLimit       = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxLdsSpillDwords")) != nullptr)
    {
        pActions->shaderCreate.apply.maxLdsSpillDwords = true;
        pActions->shaderCreate.maxLdsSpillDwords       = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "minVgprStrategyFlags")) != nullptr)
    {
        pActions->shaderCreate.apply.minVgprStrategyFlags = true;

        success &= ParseJsonMinVgprStrategyFlags(pItem, &pActions->shaderCreate.minVgprStrategyFlags);
    }

    if ((pItem = utils::JsonGetValue(pJson, "userDataSpillThreshold")) != nullptr)
    {
        pActions->shaderCreate.apply.userDataSpillThreshold = true;
        pActions->shaderCreate.userDataSpillThreshold       = static_cast<uint32_t>(pItem->integerValue);
    }

    if ((pItem = utils::JsonGetValue(pJson, "maxWavesPerCu")) != nullptr)
    {
        pActions->pipelineShader.apply.maxWavesPerCu = true;
        pActions->pipelineShader.maxWavesPerCu       = static_cast<uint32_t>(pItem->integerValue);
    }

    return success;
}

// =====================================================================================================================
static bool ParseJsonProfileEntryAction(
    utils::Json*           pJson,
    PipelineProfileAction* pAction)
{
    bool success = true;

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

                pJson = utils::JsonParse(jsonSettings, pJsonBuffer, bytesRead);

                m_pDevice->VkInstance()->FreeMem(pJsonBuffer);
            }

            jsonFile.Close();
        }
    }

    if (pJson != nullptr)
    {
        bool success = ParseJsonProfile(pJson, &m_runtimeProfile);

        VK_ASSERT(success);

        utils::JsonDestroy(jsonSettings, pJson);
    }
}
#endif

};
