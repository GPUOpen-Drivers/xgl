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
void ShaderOptimizer::CreateShaderOptimizerKey(
    const void*            pModuleData,
    const Pal::ShaderHash& shaderHash,
    const ShaderStage      stage,
    const size_t           shaderSize,
    ShaderOptimizerKey*    pShaderKey
    ) const
{
    const auto& settings = m_pDevice->GetRuntimeSettings();

    if (settings.pipelineUseShaderHashAsProfileHash)
    {
        pShaderKey->codeHash.lower = Vkgc::IPipelineDumper::GetShaderHash(pModuleData);
        pShaderKey->codeHash.upper = 0;
    }
    else
    {
        // Populate the pipeline profile key.  The hash used by the profile is different from the default
        // internal hash in that it only depends on the SPIRV code + entry point.  This is to reduce the
        // chance that internal changes to our hash calculation logic drop us off pipeline profiles.
        pShaderKey->codeHash = shaderHash;
    }
    pShaderKey->codeSize = shaderSize;
    pShaderKey->stage    = stage;
}

// =====================================================================================================================
bool ShaderOptimizer::HasMatchingProfileEntry(
    const PipelineProfile&      profile,
    const PipelineOptimizerKey& pipelineKey
    ) const
{
    bool foundMatch = false;

    for (uint32_t entryIdx = 0; entryIdx < profile.entryCount; ++entryIdx)
    {
        const auto& pattern = profile.pEntries[entryIdx].pattern;

        if ((GetFirstMatchingShader(pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex))
        {
            foundMatch = true;
            break;
        }
    }

    return foundMatch;
}

// =====================================================================================================================
bool ShaderOptimizer::HasMatchingProfileEntry(
    const PipelineOptimizerKey& pipelineKey
    ) const
{
    bool foundMatch = HasMatchingProfileEntry(m_appProfile, pipelineKey);

    if (foundMatch == false)
    {
        foundMatch = HasMatchingProfileEntry(m_tuningProfile, pipelineKey);
    }

#if ICD_RUNTIME_APP_PROFILE
    if (foundMatch == false)
    {
        foundMatch = HasMatchingProfileEntry(m_runtimeProfile, pipelineKey);
    }
#endif

    return foundMatch;
}

// =====================================================================================================================
void ShaderOptimizer::CalculateMatchingProfileEntriesHash(
    const PipelineProfile&      profile,
    const PipelineOptimizerKey& pipelineKey,
    Util::MetroHash128*         pHasher
    ) const
{
    for (uint32_t entryIdx = 0; entryIdx < profile.entryCount; ++entryIdx)
    {
        const auto& pattern = profile.pEntries[entryIdx].pattern;

        for (uint32_t shaderIdx = 0; shaderIdx < pipelineKey.shaderCount; ++shaderIdx)
        {
            if ((GetFirstMatchingShader(pattern, shaderIdx, pipelineKey) != InvalidShaderIndex))
            {
                const auto& action       = profile.pEntries[entryIdx].action;
                const auto& shaderAction = action.shaders[static_cast<uint32_t>(pipelineKey.pShaders[shaderIdx].stage)];

                pHasher->Update(action.createInfo);
                pHasher->Update(shaderAction.dynamicShaderInfo);
                pHasher->Update(shaderAction.pipelineShader);
                pHasher->Update(shaderAction.shaderCreate);

                if (shaderAction.shaderCreate.apply.shaderReplaceEnabled &&
                    (shaderAction.shaderReplace.pCode != nullptr))
                {
                    pHasher->Update(
                        static_cast<const uint8_t*>(shaderAction.shaderReplace.pCode),
                        shaderAction.shaderReplace.sizeInBytes);
                }

                // Include the shaderIdx in case the same entry moves to a different shader
                pHasher->Update(shaderIdx);
            }
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::CalculateMatchingProfileEntriesHash(
    const PipelineOptimizerKey& pipelineKey,
    Util::MetroHash128*         pHasher
    ) const
{
    CalculateMatchingProfileEntriesHash(m_appProfile, pipelineKey, pHasher);
    CalculateMatchingProfileEntriesHash(m_tuningProfile, pipelineKey, pHasher);
#if ICD_RUNTIME_APP_PROFILE
    CalculateMatchingProfileEntriesHash(m_runtimeProfile, pipelineKey, pHasher);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToShaderCreateInfo(
    const PipelineProfile&           profile,
    const PipelineOptimizerKey&      pipelineKey,
    uint32_t                         shaderIndex,
    PipelineShaderOptionsPtr         options
    ) const
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = profile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, shaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const Vkgc::ShaderStage stage = pipelineKey.pShaders[shaderIndex].stage;
            const auto& shaderCreate      = profileEntry.action.shaders[static_cast<uint32_t>(stage)].shaderCreate;

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

                if (shaderCreate.apply.disableFMA != 0)
                {
                    options.pOptions->disableFMA = shaderCreate.tuningOptions.disableFMA;
                }

                if (shaderCreate.apply.workaroundStorageImageFormats)
                {
                    options.pOptions->workaroundStorageImageFormats = true;
                }

                if (shaderCreate.apply.disableLoopUnrolls)
                {
                    options.pOptions->disableLoopUnroll = true;
                }
                if (shaderCreate.apply.useSiScheduler)
                {
                    options.pOptions->useSiScheduler = shaderCreate.tuningOptions.useSiScheduler;
                }
                if (shaderCreate.apply.disableCodeSinking)
                {
                    options.pOptions->disableCodeSinking = shaderCreate.tuningOptions.disableCodeSinking;
                }
                if (shaderCreate.apply.nsaThreshold)
                {
                    options.pOptions->nsaThreshold = shaderCreate.tuningOptions.nsaThreshold;
                }
                if (shaderCreate.apply.aggressiveInvariantLoads)
                {
                    options.pOptions->aggressiveInvariantLoads = shaderCreate.tuningOptions.aggressiveInvariantLoads;
                }
                if (shaderCreate.apply.favorLatencyHiding)
                {
                    options.pOptions->favorLatencyHiding = shaderCreate.tuningOptions.favorLatencyHiding;
                }
                if (shaderCreate.apply.reconfigWorkgroupLayout)
                {
                    options.pPipelineOptions->reconfigWorkgroupLayout =
                        shaderCreate.tuningOptions.reconfigWorkgroupLayout;
                }
                if (shaderCreate.apply.enableLoadScalarizer)
                {
                    options.pOptions->enableLoadScalarizer = shaderCreate.tuningOptions.enableLoadScalarizer;
                }
                if (shaderCreate.apply.forceLoopUnrollCount != 0)
                {
                    options.pOptions->forceLoopUnrollCount = shaderCreate.tuningOptions.forceLoopUnrollCount;
                }
                if (shaderCreate.apply.disableLicm)
                {
                    options.pOptions->disableLicm = shaderCreate.tuningOptions.disableLicm;
                }
                if (shaderCreate.apply.unrollThreshold != 0)
                {
                    options.pOptions->unrollThreshold = shaderCreate.tuningOptions.unrollThreshold;
                }
                if (shaderCreate.apply.ldsSpillLimitDwords != 0)
                {
                    options.pOptions->ldsSpillLimitDwords = shaderCreate.tuningOptions.ldsSpillLimitDwords;
                }
                if (shaderCreate.apply.fp32DenormalMode != 0)
                {
                    options.pOptions->fp32DenormalMode = shaderCreate.tuningOptions.fp32DenormalMode;
                }
                if (shaderCreate.apply.fastMathFlags != 0)
                {
                    options.pOptions->fastMathFlags = shaderCreate.tuningOptions.fastMathFlags;
                }
                if (shaderCreate.apply.disableFastMathFlags != 0)
                {
                    options.pOptions->disableFastMathFlags = shaderCreate.tuningOptions.disableFastMathFlags;
                }
                if (shaderCreate.apply.scalarizeWaterfallLoads)
                {
                    options.pOptions->scalarizeWaterfallLoads = shaderCreate.tuningOptions.scalarizeWaterfallLoads;
                }
                if (shaderCreate.apply.waveSize)
                {
                    options.pOptions->waveSize = shaderCreate.tuningOptions.waveSize;
                }

                if (shaderCreate.apply.wgpMode)
                {
                    options.pOptions->wgpMode = shaderCreate.tuningOptions.wgpMode;
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
    uint32_t                           shaderIndex,
    PipelineShaderOptionsPtr           options) const
{

    ApplyProfileToShaderCreateInfo(m_appProfile, pipelineKey, shaderIndex, options);

    ApplyProfileToShaderCreateInfo(m_tuningProfile, pipelineKey, shaderIndex, options);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToShaderCreateInfo(m_runtimeProfile, pipelineKey, shaderIndex, options);
#endif
}

// =====================================================================================================================
Vkgc::ThreadGroupSwizzleMode ShaderOptimizer::OverrideThreadGroupSwizzleMode(
    ShaderStage                 shaderStage,
    const PipelineOptimizerKey& pipelineKey
    ) const
{
    Vkgc::ThreadGroupSwizzleMode swizzleMode = Vkgc::ThreadGroupSwizzleMode::Default;

    for (uint32_t entry = 0; entry < m_appProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_appProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            if (shaders[shaderStage].shaderCreate.apply.threadGroupSwizzleMode)
            {
                swizzleMode = shaders[shaderStage].shaderCreate.tuningOptions.threadGroupSwizzleMode;
            }
        }
    }

    for (uint32_t entry = 0; entry < m_tuningProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_tuningProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            if (shaders[shaderStage].shaderCreate.apply.threadGroupSwizzleMode)
            {
                swizzleMode = shaders[shaderStage].shaderCreate.tuningOptions.threadGroupSwizzleMode;
            }
        }
    }

    return swizzleMode;
}

// =====================================================================================================================
bool ShaderOptimizer::OverrideThreadIdSwizzleMode(
    ShaderStage                 shaderStage,
    const PipelineOptimizerKey& pipelineKey
    ) const
{
    bool swizzleMode = false;

    for (uint32_t entry = 0; entry < m_appProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_appProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            swizzleMode = shaders[shaderStage].shaderCreate.tuningOptions.threadIdSwizzleMode;
        }
    }

    for (uint32_t entry = 0; entry < m_tuningProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_tuningProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            swizzleMode = shaders[shaderStage].shaderCreate.tuningOptions.threadIdSwizzleMode;
        }
    }

    return swizzleMode;
}

#if VKI_RAY_TRACING
#endif

// =====================================================================================================================
void ShaderOptimizer::OverrideShaderThreadGroupSize(
    ShaderStage                 shaderStage,
    const PipelineOptimizerKey& pipelineKey,
    uint32_t*                   pThreadGroupSizeX,
    uint32_t*                   pThreadGroupSizeY,
    uint32_t*                   pThreadGroupSizeZ
    ) const
{
    for (uint32_t entry = 0; entry < m_appProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_appProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            if (shaders[shaderStage].shaderCreate.apply.overrideShaderThreadGroupSize)
            {
                *pThreadGroupSizeX = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeX;
                *pThreadGroupSizeY = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeY;
                *pThreadGroupSizeZ = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeZ;
            }
        }
    }

    for (uint32_t entry = 0; entry < m_tuningProfile.entryCount; ++entry)
    {
        const PipelineProfileEntry& profileEntry = m_tuningProfile.pEntries[entry];

        if (GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey) != InvalidShaderIndex)
        {
            const auto& shaders = profileEntry.action.shaders;

            if (shaders[shaderStage].shaderCreate.apply.overrideShaderThreadGroupSize)
            {
                *pThreadGroupSizeX = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeX;
                *pThreadGroupSizeY = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeY;
                *pThreadGroupSizeZ = shaders[shaderStage].shaderCreate.tuningOptions.overrideShaderThreadGroupSizeZ;
            }
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::OverrideGraphicsPipelineCreateInfo(
    const PipelineOptimizerKey&       pipelineKey,
    VkShaderStageFlagBits             shaderStages,
    Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
    Pal::DynamicGraphicsShaderInfos*  pGraphicsShaderInfos) const
{
    ApplyProfileToGraphicsPipelineCreateInfo(
        m_appProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsShaderInfos);

    ApplyProfileToGraphicsPipelineCreateInfo(
        m_tuningProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsShaderInfos);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToGraphicsPipelineCreateInfo(
        m_runtimeProfile, pipelineKey, shaderStages, pPalCreateInfo, pGraphicsShaderInfos);
#endif
}

// =====================================================================================================================
void ShaderOptimizer::OverrideComputePipelineCreateInfo(
    const PipelineOptimizerKey&      pipelineKey,
    Pal::DynamicComputeShaderInfo*   pDynamicCompueShaderInfo) const
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
    const VkAllocationCallbacks* pAllocCB = m_pDevice->VkInstance()->GetAllocCallbacks();

    if (m_appProfile.pEntries != nullptr)
    {
        pAllocCB->pfnFree(pAllocCB->pUserData, m_appProfile.pEntries);
    }
    if (m_tuningProfile.pEntries != nullptr)
    {
        pAllocCB->pfnFree(pAllocCB->pUserData, m_tuningProfile.pEntries);
    }
#if ICD_RUNTIME_APP_PROFILE
    if (m_runtimeProfile.pEntries != nullptr)
    {
        pAllocCB->pfnFree(pAllocCB->pUserData, m_runtimeProfile.pEntries);
    }
#endif
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToDynamicComputeShaderInfo(
    const ShaderProfileAction&     action,
    Pal::DynamicComputeShaderInfo* pComputeShaderInfo) const
{
    if (action.dynamicShaderInfo.apply.maxWavesPerCu)
    {
        pComputeShaderInfo->maxWavesPerCu = static_cast<float>(action.dynamicShaderInfo.maxWavesPerCu);
    }

    if (action.dynamicShaderInfo.apply.maxThreadGroupsPerCu)
    {
        pComputeShaderInfo->maxThreadGroupsPerCu = action.dynamicShaderInfo.maxThreadGroupsPerCu;
    }
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToDynamicGraphicsShaderInfo(
    const ShaderProfileAction&      action,
    Pal::DynamicGraphicsShaderInfo* pGraphicsShaderInfo) const
{
    if (action.dynamicShaderInfo.apply.maxWavesPerCu)
    {
        pGraphicsShaderInfo->maxWavesPerCu = static_cast<float>(action.dynamicShaderInfo.maxWavesPerCu);
    }
}

// =====================================================================================================================
void ShaderOptimizer::ApplyProfileToGraphicsPipelineCreateInfo(
    const PipelineProfile&            profile,
    const PipelineOptimizerKey&       pipelineKey,
    VkShaderStageFlagBits             shaderStages,
    Pal::GraphicsPipelineCreateInfo*  pPalCreateInfo,
    Pal::DynamicGraphicsShaderInfos*  pGraphicsShaderInfos) const
{
    uint32_t vkgcStages = VkToVkgcShaderStageMask(shaderStages);

    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const auto& profileEntry     = profile.pEntries[entry];
        uint32_t    firstShaderMatch = GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey);

        if (firstShaderMatch != InvalidShaderIndex)
        {
            // Apply parameters to DynamicGraphicsShaderInfo
            const auto& shaders = profileEntry.action.shaders;

            for (uint32_t shaderIdx = 0; shaderIdx < pipelineKey.shaderCount; ++shaderIdx)
            {
                const auto vkgcStage = pipelineKey.pShaders[shaderIdx].stage;

                if (Util::BitfieldIsSet(vkgcStages, vkgcStage))
                {
                    switch (vkgcStage)
                    {
                    case ShaderStage::ShaderStageTask:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->ts);
                        break;
                    case ShaderStage::ShaderStageVertex:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->vs);
                        break;
                    case ShaderStage::ShaderStageTessControl:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->hs);
                        break;
                    case ShaderStage::ShaderStageTessEval:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->ds);
                        break;
                    case ShaderStage::ShaderStageGeometry:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->gs);
                        break;
                    case ShaderStage::ShaderStageMesh:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->ms);
                        break;
                    case ShaderStage::ShaderStageFragment:
                        ApplyProfileToDynamicGraphicsShaderInfo(shaders[vkgcStage], &pGraphicsShaderInfos->ps);
                        break;
                    default:
                        PAL_ASSERT_ALWAYS();
                        break;
                    }

                }
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
    Pal::DynamicComputeShaderInfo*   pDynamicComputeShaderInfo) const
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const auto& profileEntry     = profile.pEntries[entry];
        uint32_t    firstShaderMatch = GetFirstMatchingShader(profileEntry.pattern, InvalidShaderIndex, pipelineKey);

        if (firstShaderMatch != InvalidShaderIndex)
        {
            ApplyProfileToDynamicComputeShaderInfo(
                profileEntry.action.shaders[ShaderStage::ShaderStageCompute],
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
uint32_t ShaderOptimizer::GetFirstMatchingShader(
    const PipelineProfilePattern& pattern,
    uint32_t                      targetShader,
    const PipelineOptimizerKey&   pipelineKey) const
{
    uint32_t firstMatchIndex = InvalidShaderIndex;
    bool     pipelineMatch   = true;

    if (pattern.match.always == 0)
    {
        if (pattern.match.shaderOnly && (targetShader != InvalidShaderIndex))
        {
            const ShaderOptimizerKey&   shaderKey     = pipelineKey.pShaders[targetShader];
            const ShaderProfilePattern& shaderPattern = pattern.shaders[shaderKey.stage];

            if (Pal::ShaderHashesEqual(shaderPattern.codeHash, shaderKey.codeHash))
            {
                firstMatchIndex = targetShader;
            }
            else
            {
                pipelineMatch = false;
            }
        }
        else
        {
            uint32_t activeMatchStages = 0;
            uint32_t hashMatchStages   = 0;

            // Set bits for stages to be matched inclusively
            for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
            {
                const ShaderProfilePattern& shaderPattern = pattern.shaders[stage];

                if (shaderPattern.match.u32All != 0)
                {
                    uint32_t stageBit = (1 << static_cast<uint32_t>(stage));

                    if (shaderPattern.match.stageActive != 0)
                    {
                        activeMatchStages |= stageBit;
                    }

                    if (shaderPattern.match.codeHash != 0)
                    {
                        hashMatchStages |= stageBit;
                    }
                }
            }

            for (uint32_t shaderIdx = 0; shaderIdx < pipelineKey.shaderCount; ++shaderIdx)
            {
                const ShaderOptimizerKey&   shaderKey     = pipelineKey.pShaders[shaderIdx];
                const ShaderProfilePattern& shaderPattern = pattern.shaders[shaderKey.stage];

                if ((shaderPattern.match.u32All != 0) && (shaderKey.codeSize > 0))
                {
                    uint32_t notStageBit = ~(1 << static_cast<uint32_t>(shaderKey.stage));

                    // Unset bit to match an active stage
                    activeMatchStages &= notStageBit;

                    // Unset bit to match a stage hash
                    if (Pal::ShaderHashesEqual(shaderPattern.codeHash, shaderKey.codeHash))
                    {
                        if (firstMatchIndex == InvalidShaderIndex)
                        {
                            firstMatchIndex = shaderIdx;
                        }

                        hashMatchStages &= notStageBit;
                    }

                    // Fail if stage is expected to be inactive
                    if (shaderPattern.match.stageInactive != 0)
                    {
                        pipelineMatch = false;
                        break;
                    }

                    // Test by code size (less than)
                    if ((shaderPattern.match.codeSizeLessThan != 0) &&
                        (shaderPattern.codeSizeLessThanValue >= shaderKey.codeSize))
                    {
                        pipelineMatch = false;
                        break;
                    }
                }
            }

            // Fail if there are any remaining stages left to match
            if ((activeMatchStages != 0) || (hashMatchStages != 0))
            {
                pipelineMatch = false;
            }
        }
    }

    // Check if pipeline was matched due to means other than shader hash
    if (pipelineMatch && (firstMatchIndex == InvalidShaderIndex))
    {
        for (uint32_t shaderIdx = 0; shaderIdx < pipelineKey.shaderCount; ++shaderIdx)
        {
            if (pipelineKey.pShaders[shaderIdx].codeSize > 0)
            {
                firstMatchIndex = shaderIdx;
                break;
            }
        }
    }

    return pipelineMatch ? firstMatchIndex : InvalidShaderIndex;
}

// =====================================================================================================================
void ShaderOptimizer::BuildTuningProfile()
{
    m_tuningProfile.entryCount = 0;
    m_tuningProfile.entryCapacity = InitialPipelineProfileEntries;

    const VkAllocationCallbacks* pAllocCB = m_pDevice->VkInstance()->GetAllocCallbacks();
    size_t newSize = m_tuningProfile.entryCapacity * sizeof(PipelineProfileEntry);
    void* pMemory = pAllocCB->pfnAllocation(pAllocCB->pUserData,
                                            newSize,
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    m_tuningProfile.pEntries = static_cast<PipelineProfileEntry*>(pMemory);

    if (pMemory != nullptr)
    {
        memset(pMemory, 0, newSize);

        if (m_settings.overrideShaderParams)
        {
            // Only a single entry is currently supported
            m_tuningProfile.entryCount = 1;
            auto pEntry = &m_tuningProfile.pEntries[0];

            bool matchHash = false;
            if ((m_settings.overrideShaderHashLower != 0) ||
                (m_settings.overrideShaderHashUpper != 0))
            {
                matchHash = true;
            }
            else
            {
                pEntry->pattern.match.always = 1;
            }

            // Assign ShaderStage type according to setting file
            uint32_t shaderStage = ShaderStage::ShaderStageFragment;
            switch (m_settings.overrideShaderStage)
            {
            case ShaderMode::TaskShader:
                shaderStage = ShaderStage::ShaderStageTask;
                break;
            case ShaderMode::VertexShader:
                shaderStage = ShaderStage::ShaderStageVertex;
                break;
            case ShaderMode::TessellationControlShader:
                shaderStage = ShaderStage::ShaderStageTessControl;
                break;
            case ShaderMode::TessellationEvaluationShader:
                shaderStage = ShaderStage::ShaderStageTessEval;
                break;
            case ShaderMode::GeometryShader:
                shaderStage = ShaderStage::ShaderStageGeometry;
                break;
            case ShaderMode::MeshShader:
                shaderStage = ShaderStage::ShaderStageMesh;
                break;
            case ShaderMode::FragmentShader:
                shaderStage = ShaderStage::ShaderStageFragment;
                break;
            case ShaderMode::ComputeShader:
                shaderStage = ShaderStage::ShaderStageCompute;
                break;
            default:
                VK_NEVER_CALLED();
            }

            pEntry->pattern.match.shaderOnly = (shaderStage == ShaderStage::ShaderStageCompute) ? 1 : 0;

            VK_ASSERT(shaderStage < ShaderStage::ShaderStageCount);

            auto pPattern = &pEntry->pattern.shaders[shaderStage];
            auto pAction  = &pEntry->action.shaders[shaderStage];

            pPattern->match.codeHash = matchHash;
            pPattern->codeHash.lower = m_settings.overrideShaderHashLower;
            pPattern->codeHash.upper = m_settings.overrideShaderHashUpper;

            if (m_settings.overrideNumVGPRsAvailable != 0)
            {
                pAction->shaderCreate.apply.vgprLimit         = true;
                pAction->shaderCreate.tuningOptions.vgprLimit = m_settings.overrideNumVGPRsAvailable;
            }

            if (m_settings.overrideMaxLdsSpillDwords != 0)
            {
                pAction->shaderCreate.apply.ldsSpillLimitDwords         = true;
                pAction->shaderCreate.tuningOptions.ldsSpillLimitDwords = m_settings.overrideMaxLdsSpillDwords;
            }

            if (m_settings.overrideUserDataSpillThreshold)
            {
                pAction->shaderCreate.apply.userDataSpillThreshold         = true;
                pAction->shaderCreate.tuningOptions.userDataSpillThreshold = 0;
            }

            if (m_settings.overrideThreadGroupSwizzling != 0)
            {
                pAction->shaderCreate.apply.threadGroupSwizzleMode  = true;
                Vkgc::ThreadGroupSwizzleMode swizzlingMode = Vkgc::ThreadGroupSwizzleMode::Default;

                switch (m_settings.overrideThreadGroupSwizzling)
                {
                case vk::ThreadGroupSwizzleModeDefault:
                    swizzlingMode = Vkgc::ThreadGroupSwizzleMode::Default;
                    break;
                case vk::ThreadGroupSwizzleMode4x4:
                    swizzlingMode = Vkgc::ThreadGroupSwizzleMode::_4x4;
                    break;
                case vk::ThreadGroupSwizzleMode8x8:
                    swizzlingMode = Vkgc::ThreadGroupSwizzleMode::_8x8;
                    break;
                case vk::ThreadGroupSwizzleMode16x16:
                    swizzlingMode = Vkgc::ThreadGroupSwizzleMode::_16x16;
                    break;
                default:
                    VK_NEVER_CALLED();
                }

                pAction->shaderCreate.tuningOptions.threadGroupSwizzleMode = swizzlingMode;
            }

            if (m_settings.overrideThreadIdSwizzle)
            {
                pAction->shaderCreate.tuningOptions.threadIdSwizzleMode = true;
            }

            if (m_settings.overrideShaderThreadGroupSizeX != 0)
            {
                pAction->shaderCreate.apply.overrideShaderThreadGroupSize = true;
                pAction->shaderCreate.tuningOptions.overrideShaderThreadGroupSizeX =
                    m_settings.overrideShaderThreadGroupSizeX;
            }
            if (m_settings.overrideShaderThreadGroupSizeY != 0)
            {
                pAction->shaderCreate.apply.overrideShaderThreadGroupSize = true;
                pAction->shaderCreate.tuningOptions.overrideShaderThreadGroupSizeY =
                    m_settings.overrideShaderThreadGroupSizeY;
            }
            if (m_settings.overrideShaderThreadGroupSizeZ != 0)
            {
                pAction->shaderCreate.apply.overrideShaderThreadGroupSize = true;
                pAction->shaderCreate.tuningOptions.overrideShaderThreadGroupSizeZ =
                    m_settings.overrideShaderThreadGroupSizeZ;
            }

            pAction->shaderCreate.apply.allowReZ                = m_settings.overrideAllowReZ;
            pAction->shaderCreate.apply.disableLoopUnrolls      = m_settings.overrideDisableLoopUnrolls;

            if (m_settings.overrideUseSiScheduler)
            {
                pAction->shaderCreate.apply.useSiScheduler = true;
                pAction->shaderCreate.tuningOptions.useSiScheduler = m_settings.overrideUseSiScheduler;
            }
            if (m_settings.overrideReconfigWorkgroupLayout)
            {
                pAction->shaderCreate.apply.reconfigWorkgroupLayout = true;
                pAction->shaderCreate.tuningOptions.reconfigWorkgroupLayout =
                    m_settings.overrideReconfigWorkgroupLayout;
            }
            if (m_settings.overrideDisableLicm)
            {
                pAction->shaderCreate.apply.disableLicm = true;
                pAction->shaderCreate.tuningOptions.disableLicm = m_settings.overrideDisableLicm;
            }
            if (m_settings.overrideEnableLoadScalarizer)
            {
                pAction->shaderCreate.apply.enableLoadScalarizer = true;
                pAction->shaderCreate.tuningOptions.enableLoadScalarizer = m_settings.overrideEnableLoadScalarizer;
            }
            if (m_settings.overrideDisableCodeSinking)
            {
                pAction->shaderCreate.apply.disableCodeSinking = true;
                pAction->shaderCreate.tuningOptions.disableCodeSinking = m_settings.overrideDisableCodeSinking;
            }
            if (m_settings.overrideFavorLatencyHiding)
            {
                pAction->shaderCreate.apply.favorLatencyHiding = true;
                pAction->shaderCreate.tuningOptions.favorLatencyHiding = m_settings.overrideFavorLatencyHiding;
            }
            if (m_settings.overrideForceLoopUnrollCount != 0)
            {
                pAction->shaderCreate.apply.forceLoopUnrollCount = true;
                pAction->shaderCreate.tuningOptions.forceLoopUnrollCount = m_settings.overrideForceLoopUnrollCount;
            }
            if (m_settings.overrideUnrollThreshold != 0)
            {
                pAction->shaderCreate.apply.unrollThreshold = true;
                pAction->shaderCreate.tuningOptions.unrollThreshold = m_settings.overrideUnrollThreshold;
            }
            if (m_settings.overrideFastMathFlags != 0)
            {
                pAction->shaderCreate.apply.fastMathFlags = true;
                pAction->shaderCreate.tuningOptions.fastMathFlags = m_settings.overrideFastMathFlags;
            }
            if (m_settings.overrideDisableFastMathFlags != 0)
            {
                pAction->shaderCreate.apply.disableFastMathFlags = true;
                pAction->shaderCreate.tuningOptions.disableFastMathFlags = m_settings.overrideDisableFastMathFlags;
            }
            if (m_settings.overrideNsaThreshold != 0)
            {
                pAction->shaderCreate.apply.nsaThreshold = true;
                pAction->shaderCreate.tuningOptions.nsaThreshold = m_settings.overrideNsaThreshold;
            }
            if (m_settings.overrideAggressiveInvariantLoads != 0)
            {
                pAction->shaderCreate.apply.aggressiveInvariantLoads = true;
                pAction->shaderCreate.tuningOptions.aggressiveInvariantLoads =
                    static_cast<Vkgc::InvariantLoads>(m_settings.overrideAggressiveInvariantLoads);
            }
            if (m_settings.overrideScalarizeWaterfallLoads)
            {
                pAction->shaderCreate.apply.scalarizeWaterfallLoads = true;
                pAction->shaderCreate.tuningOptions.scalarizeWaterfallLoads =
                    m_settings.overrideScalarizeWaterfallLoads;
            }

            switch (m_settings.overrideWaveSize)
            {
            case ShaderWaveSize::WaveSizeAuto:
                break;
            case ShaderWaveSize::WaveSize64:
                pAction->shaderCreate.apply.waveSize = true;
                pAction->shaderCreate.tuningOptions.waveSize = 64;
                break;
            case ShaderWaveSize::WaveSize32:
                pAction->shaderCreate.apply.waveSize = true;
                pAction->shaderCreate.tuningOptions.waveSize = 32;
                break;
            default:
                VK_NEVER_CALLED();
            }

            switch (m_settings.overrideWgpMode)
            {
            case WgpMode::WgpModeAuto:
                break;
            case WgpMode::WgpModeCu:
            case WgpMode::WgpModeWgp:
                pAction->shaderCreate.apply.wgpMode = true;
                pAction->shaderCreate.tuningOptions.wgpMode = m_settings.overrideWgpMode;
                break;
            default:
                VK_NEVER_CALLED();
            }

            pAction->shaderCreate.apply.nggDisable      = m_settings.overrideUseNgg;

            if (m_settings.overrideWavesPerCu != 0)
            {
                pAction->dynamicShaderInfo.apply.maxWavesPerCu = true;
                pAction->dynamicShaderInfo.maxWavesPerCu       = m_settings.overrideWavesPerCu;
            }

            if ((m_settings.overrideCsTgPerCu != 0) &&
                (shaderStage == ShaderStage::ShaderStageCompute))
            {
                pAction->dynamicShaderInfo.apply.maxThreadGroupsPerCu = true;
                pAction->dynamicShaderInfo.maxThreadGroupsPerCu       = m_settings.overrideCsTgPerCu;
            }

            if (m_settings.overrideUsePbbPerCrc != PipelineBinningModeDefault)
            {
                pEntry->action.createInfo.apply.binningOverride = true;

                switch (m_settings.overrideUsePbbPerCrc)
                {
                case PipelineBinningModeEnable:
                    pEntry->action.createInfo.binningOverride = Pal::BinningOverride::Enable;
                    break;

                case PipelineBinningModeDisable:
                    pEntry->action.createInfo.binningOverride = Pal::BinningOverride::Disable;
                    break;

                case PipelineBinningModeDefault:
                default:
                    pEntry->action.createInfo.binningOverride = Pal::BinningOverride::Default;
                    break;
                }
            }
        }
    }
}

// =====================================================================================================================
void ShaderOptimizer::BuildAppProfile()
{
    m_appProfile.entryCount = 0;
    m_appProfile.entryCapacity = InitialPipelineProfileEntries;

    const VkAllocationCallbacks* pAllocCB = m_pDevice->VkInstance()->GetAllocCallbacks();
    size_t newSize = m_appProfile.entryCapacity * sizeof(PipelineProfileEntry);
    void* pMemory = pAllocCB->pfnAllocation(pAllocCB->pUserData,
                                            newSize,
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    m_appProfile.pEntries = static_cast<PipelineProfileEntry*>(pMemory);

    // Early-out if the panel has dictated that we should ignore any active pipeline optimizations due to app profile
    if ((m_settings.pipelineProfileIgnoresAppProfile == false) && (pMemory != nullptr))
    {
        memset(pMemory, 0, newSize);
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

    m_appShaderProfile.BuildAppProfileLlpc(appProfile, gfxIpLevel, asicRevision, &m_appProfile);

    if (appProfile == AppProfile::Dota2)
    {
        if ((asicRevision >= Pal::AsicRevision::Polaris10) && (asicRevision <= Pal::AsicRevision::Polaris12))
        {
            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0xdd6c573c46e6adf8;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x751207727c904749;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0x71093bf7c6e98da8;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0xfbc956d87a6d6631;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0xedd89880de2091f9;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x506d0ac3995d2f1b;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0xbc583b30527e9f1d;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x1ef8276d42a14220;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0x012ddab000f80610;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x3a65a6325756203d;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0x78095b5acf62f4d5;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x2c1afc1c6f669e33;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0x22803b077988ec36;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0x7ba50586c34e1662;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;

            i = m_appProfile.entryCount++;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.stageActive = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].match.codeHash = true;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.lower = 0x313dab8ff9408da0;
            m_appProfile.pEntries[i].pattern.shaders[ShaderStage::ShaderStageFragment].codeHash.upper = 0xbb11905194a55485;
            m_appProfile.pEntries[i].action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.allowReZ = true;
        }
    }

    if (appProfile == AppProfile::ShadowOfTheTombRaider)
    {
        i = m_appProfile.entryCount++;
        PipelineProfileEntry *pEntry = &m_appProfile.pEntries[i];
        pEntry->pattern.match.always = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.apply.disableFastMathFlags = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.tuningOptions.disableFastMathFlags = 8u | 32u;
        pEntry->action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.disableFastMathFlags = true;
        pEntry->action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.tuningOptions.disableFastMathFlags = 8u;
    }

    if (appProfile == AppProfile::SOTTR)
    {
        i = m_appProfile.entryCount++;
        PipelineProfileEntry* pEntry = &m_appProfile.pEntries[i];
        pEntry->pattern.match.always = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.apply.disableFMA = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.tuningOptions.disableFMA = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.apply.disableFastMathFlags = true;
        pEntry->action.shaders[ShaderStage::ShaderStageVertex].shaderCreate.tuningOptions.disableFastMathFlags = 8u | 32u;
    }

    if (appProfile == AppProfile::CSGO)
    {
        if (gfxIpLevel >= Pal::GfxIpLevel::GfxIp10_1)
        {
            i = m_appProfile.entryCount++;
            PipelineProfileEntry *pEntry = &m_appProfile.pEntries[i];
            pEntry->pattern.match.always = true;
            pEntry->action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.apply.disableFastMathFlags = true;
            pEntry->action.shaders[ShaderStage::ShaderStageFragment].shaderCreate.tuningOptions.disableFastMathFlags = 32u;
        }
    }

    if (appProfile == AppProfile::WarHammerIII)
    {
        i = m_appProfile.entryCount++;
        PipelineProfileEntry *pEntry = &m_appProfile.pEntries[i];
        pEntry->pattern.shaders[ShaderStage::ShaderStageCompute].match.stageActive = true;
        pEntry->pattern.shaders[ShaderStage::ShaderStageCompute].match.codeHash = true;
        pEntry->pattern.shaders[ShaderStage::ShaderStageCompute].codeHash.lower = 0x0f30f0381ae22148;
        pEntry->pattern.shaders[ShaderStage::ShaderStageCompute].codeHash.upper = 0xc36b57df811c69ff;
        pEntry->action.shaders[ShaderStage::ShaderStageCompute].shaderCreate.apply.disableFastMathFlags = true;
        pEntry->action.shaders[ShaderStage::ShaderStageCompute].shaderCreate.tuningOptions.disableFastMathFlags = 32u;
    }

    if (appProfile == AppProfile::TheSurge2)
    {
        i = m_appProfile.entryCount++;
        PipelineProfileEntry *pEntry = &m_appProfile.pEntries[i];
        pEntry->pattern.match.always = true;
        pEntry->action.shaders[ShaderStage::ShaderStageCompute].shaderCreate.apply.workaroundStorageImageFormats = true;
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
void ShaderOptimizer::PrintProfileEntryMatch(
    const PipelineProfile&      profile,
    uint32_t                    index,
    const PipelineOptimizerKey& key) const
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

    for (uint32_t shaderIdx = 0; shaderIdx < key.shaderCount; ++shaderIdx)
    {
        const auto& shader = key.pShaders[shaderIdx];

        if (shader.codeSize != 0)
        {
            const char* pStage = "???";

            switch (shader.stage)
            {
            case ShaderStage::ShaderStageTask:
                pStage = "TS";
                break;
            case ShaderStage::ShaderStageVertex:
                pStage = "VS";
                break;
            case ShaderStage::ShaderStageTessControl:
                pStage = "HS";
                break;
            case ShaderStage::ShaderStageTessEval:
                pStage = "DS";
                break;
            case ShaderStage::ShaderStageGeometry:
                pStage = "GS";
                break;
            case ShaderStage::ShaderStageMesh:
                pStage = "MS";
                break;
            case ShaderStage::ShaderStageFragment:
                pStage = "PS";
                break;
            case ShaderStage::ShaderStageCompute:
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
    m_runtimeProfile.entryCount = 0;
    m_runtimeProfile.entryCapacity = InitialPipelineProfileEntries;

    const VkAllocationCallbacks* pAllocCB = m_pDevice->VkInstance()->GetAllocCallbacks();
    size_t newSize = m_runtimeProfile.entryCapacity * sizeof(PipelineProfileEntry);
    void* pMemory = pAllocCB->pfnAllocation(pAllocCB->pUserData,
                                            newSize,
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    m_runtimeProfile.pEntries = static_cast<PipelineProfileEntry*>(pMemory);

    if (pMemory == nullptr)
    {
        return;
    }

    memset(pMemory, 0, newSize);

    utils::JsonSettings jsonSettings = utils::JsonMakeInstanceSettings(pAllocCB);
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
                        bool success = m_appShaderProfile.ParseJsonProfile(
                            pJson,
                            &m_runtimeProfile,
                            m_pDevice->VkInstance()->GetAllocCallbacks());

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
