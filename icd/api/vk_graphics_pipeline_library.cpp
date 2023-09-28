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

#include "include/vk_graphics_pipeline_library.h"
#include "include/vk_pipeline_layout.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
static const VkPipelineVertexInputDivisorStateCreateInfoEXT* DumpVkPipelineVertexInputDivisorStateCreateInfoEXT(
    const VkPipelineVertexInputDivisorStateCreateInfoEXT* pSrc,
    void*                                                 pDst,
    size_t*                                               pSize)
{
    VkPipelineVertexInputDivisorStateCreateInfoEXT* pDivisorState = nullptr;

    if (pSrc != nullptr)
    {
        const size_t bindingSize = pSrc->vertexBindingDivisorCount * sizeof(VkVertexInputBindingDivisorDescriptionEXT);

        if (pSize != nullptr)
        {
            *pSize = sizeof(VkPipelineVertexInputDivisorStateCreateInfoEXT) + bindingSize;
        }

        if (pDst != nullptr)
        {
            pDivisorState = reinterpret_cast<VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pDst);

            VkVertexInputBindingDivisorDescriptionEXT* pVertexBindingDivisor =
                reinterpret_cast<VkVertexInputBindingDivisorDescriptionEXT*>(
                    Util::VoidPtrInc(pDst, sizeof(VkPipelineVertexInputDivisorStateCreateInfoEXT)));

            memcpy(pVertexBindingDivisor, pSrc->pVertexBindingDivisors, bindingSize);

            pDivisorState->sType                     = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
            pDivisorState->pNext                     = nullptr;
            pDivisorState->vertexBindingDivisorCount = pSrc->vertexBindingDivisorCount;
            pDivisorState->pVertexBindingDivisors    = pVertexBindingDivisor;
        }
    }
    else if (pSize != nullptr)
    {
        *pSize = 0;
    }

    return pDivisorState;
}

// =====================================================================================================================
static const VkPipelineVertexInputStateCreateInfo* DumpVkPipelineVertexInputStateCreateInfo(
    const VkPipelineVertexInputStateCreateInfo* pSrc,
    void*                                       pDst,
    size_t*                                     pSize)
{
    VkPipelineVertexInputStateCreateInfo* pVertexInput = nullptr;

    if (pSrc != nullptr)
    {
        EXTRACT_VK_STRUCTURES_0(
            divisorState,
            PipelineVertexInputDivisorStateCreateInfoEXT,
            static_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pSrc->pNext),
            PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT);

        const size_t bindingDescSize =
            pSrc->vertexBindingDescriptionCount * sizeof(VkVertexInputBindingDescription);
        const size_t AttribDescSize =
            pSrc->vertexAttributeDescriptionCount * sizeof(VkVertexInputAttributeDescription);

        if (pSize != nullptr)
        {
            *pSize = sizeof(VkPipelineVertexInputStateCreateInfo) + bindingDescSize + AttribDescSize;

            size_t divisorState = 0;
            DumpVkPipelineVertexInputDivisorStateCreateInfoEXT(
                pPipelineVertexInputDivisorStateCreateInfoEXT, nullptr, &divisorState);
            *pSize += divisorState;
        }

        if (pDst != nullptr)
        {
            pVertexInput = reinterpret_cast<VkPipelineVertexInputStateCreateInfo*>(pDst);
            VkVertexInputBindingDescription* pBindingDesc =
                reinterpret_cast<VkVertexInputBindingDescription*>(
                    Util::VoidPtrInc(pDst, sizeof(VkPipelineVertexInputStateCreateInfo)));
            VkVertexInputAttributeDescription* pAttribDesc =
                reinterpret_cast<VkVertexInputAttributeDescription*>(
                    Util::VoidPtrInc(pBindingDesc, bindingDescSize));

            const VkPipelineVertexInputDivisorStateCreateInfoEXT* pDivisorState =
                DumpVkPipelineVertexInputDivisorStateCreateInfoEXT(
                    pPipelineVertexInputDivisorStateCreateInfoEXT,
                    Util::VoidPtrInc(pAttribDesc, AttribDescSize),
                    nullptr);

            memcpy(pBindingDesc, pSrc->pVertexBindingDescriptions, bindingDescSize);
            memcpy(pAttribDesc, pSrc->pVertexAttributeDescriptions, AttribDescSize);

            pVertexInput->sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            pVertexInput->pNext                           = pDivisorState;
            pVertexInput->flags                           = pSrc->flags;
            pVertexInput->vertexBindingDescriptionCount   = pSrc->vertexBindingDescriptionCount;
            pVertexInput->vertexAttributeDescriptionCount = pSrc->vertexAttributeDescriptionCount;
            pVertexInput->pVertexBindingDescriptions      = pBindingDesc;
            pVertexInput->pVertexAttributeDescriptions    = pAttribDesc;
        }
    }
    else if (pSize != nullptr)
    {
        *pSize = 0;
    }

    return pVertexInput;
}

// =====================================================================================================================
static const VkSpecializationInfo* DumpVkSpecializationInfo(
    const VkSpecializationInfo* pSrc,
    void*                       pDst,
    size_t*                     pSize)
{
    VkSpecializationInfo* pInfo = nullptr;

    if (pSrc != nullptr)
    {
        const size_t mapEntrySize = pSrc->mapEntryCount * sizeof(VkSpecializationMapEntry);

        if (pSize != nullptr)
        {
            *pSize = mapEntrySize + pSrc->dataSize + sizeof(VkSpecializationInfo);
        }

        if (pDst != nullptr)
        {
            pInfo = reinterpret_cast<VkSpecializationInfo*>(pDst);
            void* pMapEntries = Util::VoidPtrInc(pInfo, sizeof(VkSpecializationInfo));
            void* pData       = Util::VoidPtrInc(pMapEntries, mapEntrySize);

            memcpy(pMapEntries, pSrc->pMapEntries, mapEntrySize);
            memcpy(pData, pSrc->pData, pSrc->dataSize);

            pInfo->mapEntryCount = pSrc->mapEntryCount;
            pInfo->pMapEntries   = reinterpret_cast<const VkSpecializationMapEntry*>(pMapEntries);
            pInfo->dataSize      = pSrc->dataSize;
            pInfo->pData         = pData;
        }
    }
    else if (pSize != nullptr)
    {
        *pSize = 0;
    }

    return pInfo;
}

// =====================================================================================================================
// Copy the content of PipelineShaderInfo in GraphicsPipelineBinaryCreateInfo
// Note that module data Vkgc::PipelineShaderInfo::pModuleData is not copied here.
// Module data is maintained by graphics pipeline library directly.
static GraphicsPipelineBinaryCreateInfo* DumpGraphicsPipelineBinaryCreateInfo(
    const GraphicsPipelineBinaryCreateInfo* pBinInfo,
    void*                                   pDst,
    size_t*                                 pSize)
{
    GraphicsPipelineBinaryCreateInfo* pCreateInfo = nullptr;

    if (pBinInfo != nullptr)
    {
        const Vkgc::PipelineShaderInfo* pInShaderInfos[] =
        {
            &pBinInfo->pipelineInfo.task,
            &pBinInfo->pipelineInfo.vs,
            &pBinInfo->pipelineInfo.tcs,
            &pBinInfo->pipelineInfo.tes,
            &pBinInfo->pipelineInfo.gs,
            &pBinInfo->pipelineInfo.mesh,
            &pBinInfo->pipelineInfo.fs,
        };

        size_t objSize = 0;

        // Calculate the size used by VkPipelineVertexInputStateCreateInfo in GraphicsPipelineBinaryCreateInfo
        size_t vertexInputSize = 0;
        DumpVkPipelineVertexInputStateCreateInfo(pBinInfo->pipelineInfo.pVertexInput, nullptr, &vertexInputSize);
        objSize += vertexInputSize;

        size_t specializationInfoSizes[ShaderStage::ShaderStageGfxCount] = {};
        size_t entryTargetSizes[ShaderStage::ShaderStageGfxCount] = {};
        for (uint32_t stage = 0; stage < Util::ArrayLen(pInShaderInfos); ++stage)
        {
            DumpVkSpecializationInfo(pInShaderInfos[stage]->pSpecializationInfo,
                                     nullptr,
                                     &specializationInfoSizes[stage]);

            entryTargetSizes[stage] = pInShaderInfos[stage]->pEntryTarget == nullptr ? 0 :
                strlen(pInShaderInfos[stage]->pEntryTarget) + 1;

            objSize += (specializationInfoSizes[stage] + entryTargetSizes[stage]);
        }

        // Calculate the size used by underlying memory of optimizer keys
        const uint32_t shaderKeyCount = pBinInfo->pPipelineProfileKey->shaderCount;
        const size_t   shaderKeyBytes = sizeof(ShaderOptimizerKey) * shaderKeyCount;
        objSize += sizeof(PipelineOptimizerKey) + shaderKeyBytes;

        // Calculate the size used by underlying binary metadata
        objSize += sizeof(PipelineMetadata);

        if (pSize != nullptr)
        {
            *pSize = objSize + sizeof(GraphicsPipelineBinaryCreateInfo);
        }

        if (pDst != nullptr)
        {
            void* pSystemMem = pDst;

            pCreateInfo = reinterpret_cast<GraphicsPipelineBinaryCreateInfo*>(pSystemMem);
            *pCreateInfo = *pBinInfo;

            pSystemMem = Util::VoidPtrInc(pSystemMem, sizeof(GraphicsPipelineBinaryCreateInfo));

            pCreateInfo->pipelineInfo.pVertexInput =
                DumpVkPipelineVertexInputStateCreateInfo(pBinInfo->pipelineInfo.pVertexInput, pSystemMem, nullptr);

            pSystemMem = Util::VoidPtrInc(pSystemMem, vertexInputSize);

            Vkgc::PipelineShaderInfo* pOutShaderInfos[] =
            {
                &pCreateInfo->pipelineInfo.task,
                &pCreateInfo->pipelineInfo.vs,
                &pCreateInfo->pipelineInfo.tcs,
                &pCreateInfo->pipelineInfo.tes,
                &pCreateInfo->pipelineInfo.gs,
                &pCreateInfo->pipelineInfo.mesh,
                &pCreateInfo->pipelineInfo.fs,
            };

            for (uint32_t stage = 0; stage < Util::ArrayLen(pOutShaderInfos); ++stage)
            {
                if (specializationInfoSizes[stage] != 0)
                {
                    pOutShaderInfos[stage]->pSpecializationInfo =
                        DumpVkSpecializationInfo(pInShaderInfos[stage]->pSpecializationInfo, pSystemMem, nullptr);

                    pSystemMem = Util::VoidPtrInc(pSystemMem, specializationInfoSizes[stage]);
                }

                if (entryTargetSizes[stage] != 0)
                {
                    memcpy(pSystemMem, pInShaderInfos[stage]->pEntryTarget, entryTargetSizes[stage]);

                    pOutShaderInfos[stage]->pEntryTarget = static_cast<char*>(pSystemMem);

                    pSystemMem = Util::VoidPtrInc(pSystemMem, entryTargetSizes[stage]);
                }
            }

            pCreateInfo->pPipelineProfileKey = static_cast<PipelineOptimizerKey*>(pSystemMem);

            pSystemMem = Util::VoidPtrInc(pSystemMem, sizeof(PipelineOptimizerKey));

            pCreateInfo->pPipelineProfileKey->shaderCount = shaderKeyCount;
            pCreateInfo->pPipelineProfileKey->pShaders    = static_cast<ShaderOptimizerKey*>(pSystemMem);
            memcpy(pSystemMem, pBinInfo->pPipelineProfileKey->pShaders, shaderKeyBytes);

            pSystemMem = Util::VoidPtrInc(pSystemMem, shaderKeyBytes);

            pCreateInfo->pBinaryMetadata = static_cast<PipelineMetadata*>(pSystemMem);
            memcpy(pSystemMem, pBinInfo->pBinaryMetadata, sizeof(PipelineMetadata));

            pSystemMem = Util::VoidPtrInc(pSystemMem, sizeof(PipelineMetadata));
        }
    }
    else if (pSize != nullptr)
    {
        *pSize = 0;
    }

    return pCreateInfo;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::CreatePartialPipelineBinary(
    const Device*                           pDevice,
    PipelineCache*                          pPipelineCache,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    const GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    ShaderModuleHandle*                     pTempModules,
    TempModuleState*                        pTempModuleStages)
{
    VkResult          result            = VK_SUCCESS;
    PipelineCompiler* pCompiler         = pDevice->GetCompiler(DefaultDeviceIndex);
    uint64_t          dynamicStateFlags = GetDynamicStateFlags(pCreateInfo->pDynamicState, pLibInfo);

    // Pipeline info only includes the shaders that match the enabled VkGraphicsPipelineLibraryFlagBitsEXT.
    // Use this information to skip the compilation of unused shader modules.
    const Vkgc::PipelineShaderInfo* pShaderInfos[] =
    {
        &pBinaryCreateInfo->pipelineInfo.task,
        &pBinaryCreateInfo->pipelineInfo.vs,
        &pBinaryCreateInfo->pipelineInfo.tcs,
        &pBinaryCreateInfo->pipelineInfo.tes,
        &pBinaryCreateInfo->pipelineInfo.gs,
        &pBinaryCreateInfo->pipelineInfo.mesh,
        &pBinaryCreateInfo->pipelineInfo.fs,
    };

    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if ((pShaderInfos[i]->pModuleData != nullptr) &&
            (pShaderStageInfo->stages[i].pModuleHandle != nullptr) &&
            pCompiler->IsValidShaderModule(pShaderStageInfo->stages[i].pModuleHandle))
        {
            VK_ASSERT(pShaderStageInfo->stages[i].pModuleHandle == &pTempModules[i]);

            bool canBuildShader = (((pShaderStageInfo->stages[i].stage == ShaderStage::ShaderStageFragment) &&
                                    IsRasterizationDisabled(pCreateInfo, pLibInfo, dynamicStateFlags))
                                   == false);

            if (canBuildShader && (result == VK_SUCCESS))
            {
                // We don't take care of the result. Early compile failure in some cases is expected
                result = pCompiler->CreateGraphicsShaderBinary(
                    pDevice, pPipelineCache, pShaderStageInfo->stages[i].stage, pBinaryCreateInfo, &pTempModules[i]);
            }

            pTempModuleStages[i].stage          = pShaderStageInfo->stages[i].stage;
            pTempModuleStages[i].freeBinaryOnly = false;
        }
    }

    if (pLibInfo->flags.optimize)
    {
        // We need to re-compile some stage if related new state is available
        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pPreRasterizationShaderLib != nullptr))
        {
            const ShaderModuleHandle* pParentHandle =
                pLibInfo->pPreRasterizationShaderLib->GetShaderModuleHandle(ShaderStage::ShaderStageVertex);

            // Parent library may not have vertex shader if it uses mesh shader.
            if ((pParentHandle != nullptr) && (result == VK_SUCCESS))
            {
                constexpr uint32_t TempIdx = ShaderStage::ShaderStageVertex;

                pBinaryCreateInfo->pipelineInfo.enableUberFetchShader = false;

                pTempModules[TempIdx] = *pParentHandle;

                result = pCompiler->CreateGraphicsShaderBinary(
                    pDevice, pPipelineCache, ShaderStage::ShaderStageVertex, pBinaryCreateInfo, &pTempModules[TempIdx]);

                pTempModuleStages[TempIdx].stage          = ShaderStage::ShaderStageVertex;
                pTempModuleStages[TempIdx].freeBinaryOnly = true;
            }
        }

        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pFragmentShaderLib != nullptr))
        {
            const ShaderModuleHandle* pParentHandle =
                pLibInfo->pPreRasterizationShaderLib->GetShaderModuleHandle(ShaderStage::ShaderStageFragment);

            VK_ASSERT(pParentHandle != nullptr);

            if ((pParentHandle != nullptr) && (result == VK_SUCCESS))
            {
                constexpr uint32_t TempIdx = ShaderStage::ShaderStageFragment;

                pTempModules[TempIdx] = *pParentHandle;

                result = pCompiler->CreateGraphicsShaderBinary(pDevice, pPipelineCache,
                    ShaderStage::ShaderStageFragment, pBinaryCreateInfo, &pTempModules[TempIdx]);

                pTempModuleStages[TempIdx].stage          = ShaderStage::ShaderStageFragment;
                pTempModuleStages[TempIdx].freeBinaryOnly = true;
            }
        }
    }

    for (uint32_t stage = 0; (result == VK_SUCCESS) && (stage < ShaderStage::ShaderStageGfxCount); ++stage)
    {
        if (pCompiler->IsValidShaderModule(&pTempModules[stage]) == false)
        {
            pTempModuleStages[stage].stage = ShaderStage::ShaderStageInvalid;
        }
        else if (pDevice->GetRuntimeSettings().useShaderLibraryForPipelineLibraryFastLink)
        {
            // Create shader libraries for fast-link
            GraphicsLibraryType gplType = GetGraphicsLibraryType(static_cast<ShaderStage>(stage));
            if (pBinaryCreateInfo->earlyElfPackage[gplType].codeSize != 0)
            {
                result = pCompiler->CreateGraphicsShaderLibrary(pDevice,
                                                                pBinaryCreateInfo->earlyElfPackage[gplType],
                                                                pAllocator,
                                                                &pBinaryCreateInfo->pShaderLibraries[gplType]);
            }
        }
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::Create(
    Device*                             pDevice,
    PipelineCache*                      pPipelineCache,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    PipelineCreateFlags                 flags,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    VkResult result  = VK_SUCCESS;
    size_t   apiSize = 0;
    void*    pSysMem = nullptr;

    GraphicsPipelineLibraryInfo libInfo;
    ExtractLibraryInfo(pCreateInfo, flags, &libInfo);

    GraphicsPipelineBinaryCreateInfo binaryCreateInfo = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo = {};
    ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount] = {};
    TempModuleState                  tempModuleStates[ShaderStage::ShaderStageGfxCount] = {};

    binaryCreateInfo.pipelineInfo.iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // 1. Build shader stage infos
    if (result == VK_SUCCESS)
    {
        result = BuildShaderStageInfo(pDevice,
                                      pCreateInfo->stageCount,
                                      pCreateInfo->pStages,
                                      pCreateInfo->flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                                      [](const uint32_t inputIdx, const uint32_t stageIdx)
                                      {
                                          return stageIdx;
                                      },
                                      shaderStageInfo.stages,
                                      tempModules,
                                      pPipelineCache,
                                      binaryCreateInfo.stageFeedback);
    }

    // 2. Build ShaderOptimizer pipeline key
    PipelineOptimizerKey pipelineOptimizerKey = {};
    ShaderOptimizerKey   shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
    if (result == VK_SUCCESS)
    {
        static_assert(
            VK_ARRAY_SIZE(shaderOptimizerKeys) == VK_ARRAY_SIZE(shaderStageInfo.stages),
            "Please ensure stage count matches between gfx profile key and shader stage info.");

        GeneratePipelineOptimizerKey(
            pDevice,
            pCreateInfo,
            flags,
            &shaderStageInfo,
            shaderOptimizerKeys,
            &pipelineOptimizerKey);
    }

    // 3. Build API and ELF hashes
    uint64_t              apiPsoHash = {};
    Util::MetroHash::Hash elfHash    = {};
    BuildApiHash(pCreateInfo, flags, &apiPsoHash, &elfHash);
    binaryCreateInfo.apiPsoHash = apiPsoHash;

    // 4. Get pipeline layout
    const PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    if (pPipelineLayout == nullptr)
    {
        pPipelineLayout = pDevice->GetNullPipelineLayout();
    }

    // 5. Populate binary create info
    PipelineMetadata binaryMetadata = {};
    if (result == VK_SUCCESS)
    {
        result = pDevice->GetCompiler(DefaultDeviceIndex)->ConvertGraphicsPipelineInfo(
            pDevice,
            pCreateInfo,
            flags,
            &shaderStageInfo,
            pPipelineLayout,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &binaryCreateInfo);
    }

    GraphicsPipelineObjectCreateInfo objectCreateInfo = {};
    if (result == VK_SUCCESS)
    {
        // 6. Create partial pipeline binary for fast-link
        result = CreatePartialPipelineBinary(
            pDevice,
            pPipelineCache,
            pCreateInfo,
            &libInfo,
            &shaderStageInfo,
            &binaryCreateInfo,
            pAllocator,
            tempModules,
            tempModuleStates);
    }

    // Cleanup temp memory in binaryCreateInfo.
    pDevice->GetCompiler(DefaultDeviceIndex)->FreeGraphicsPipelineCreateInfo(&binaryCreateInfo, false);

    if (result == VK_SUCCESS)
    {
        // 7. Build pipeline object create info
        BuildPipelineObjectCreateInfo(
            pDevice,
            pCreateInfo,
            flags,
            &shaderStageInfo,
            pPipelineLayout,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &objectCreateInfo);

        // Calculate object size
        apiSize = sizeof(GraphicsPipelineLibrary);
        size_t auxiliarySize = 0;
        DumpGraphicsPipelineBinaryCreateInfo(&binaryCreateInfo, nullptr, &auxiliarySize);

        const size_t objSize = apiSize + auxiliarySize;

        // Allocate memory
        pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

        if (pSysMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS)
    {
        GraphicsPipelineBinaryCreateInfo* pBinInfo =
            DumpGraphicsPipelineBinaryCreateInfo(&binaryCreateInfo, Util::VoidPtrInc(pSysMem, apiSize), nullptr);

        VK_PLACEMENT_NEW(pSysMem) GraphicsPipelineLibrary(
            pDevice,
            objectCreateInfo,
            pBinInfo,
            libInfo,
            elfHash,
            apiPsoHash,
            tempModules,
            tempModuleStates,
            pPipelineLayout);

        *pPipeline = GraphicsPipelineLibrary::HandleFromVoidPointer(pSysMem);

        // Generate feedback info
        PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo = nullptr;
        pCompiler->GetPipelineCreationFeedback(static_cast<const VkStructHeader*>(pCreateInfo->pNext),
                                               &pPipelineCreationFeedbackCreateInfo);

        uint64_t durationTicks = Util::GetPerfCpuTime() - startTimeTicks;
        uint64_t duration      = vk::utils::TicksToNano(durationTicks);
        pBinInfo->pipelineFeedback.feedbackValid = true;
        pBinInfo->pipelineFeedback.duration      = duration;

        bool hitPipelineCache  = true;
        bool containValidStage = false;
        for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
        {
            const bool isValidStage = (shaderStageInfo.stages[i].pModuleHandle == nullptr) ?
                false : pCompiler->IsValidShaderModule(shaderStageInfo.stages[i].pModuleHandle);
            containValidStage |= isValidStage;
            hitPipelineCache  &= ((isValidStage == false) ||
                                  (pBinInfo->stageFeedback[i].hitApplicationCache == true));
        }
        pBinInfo->pipelineFeedback.hitApplicationCache = (hitPipelineCache && containValidStage);

        pCompiler->SetPipelineCreationFeedbackInfo(
            pPipelineCreationFeedbackCreateInfo,
            pCreateInfo->stageCount,
            pCreateInfo->pStages,
            &pBinInfo->pipelineFeedback,
            pBinInfo->stageFeedback);
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if (m_tempModuleStates[i].stage != ShaderStage::ShaderStageInvalid)
        {
            if (m_tempModuleStates[i].freeBinaryOnly)
            {
                PipelineCompiler::FreeGraphicsShaderBinary(m_tempModules + i);
            }
            else
            {
                pCompiler->FreeShaderModule(m_tempModules + i);
            }
        }
    }

    for (Pal::IShaderLibrary* pShaderLib : m_pBinaryCreateInfo->pShaderLibraries)
    {
        if (pShaderLib != nullptr)
        {
            pShaderLib->Destroy();
            pAllocator->pfnFree(pAllocator->pUserData, pShaderLib);
        }
    }

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
GraphicsPipelineLibrary::GraphicsPipelineLibrary(
    Device*                                 pDevice,
    const GraphicsPipelineObjectCreateInfo& objectInfo,
    const GraphicsPipelineBinaryCreateInfo* pBinaryInfo,
    const GraphicsPipelineLibraryInfo&      libInfo,
    const Util::MetroHash::Hash&            elfHash,
    const uint64_t                          apiHash,
    const ShaderModuleHandle*               pTempModules,
    const TempModuleState*                  pTempModuleStates,
    const PipelineLayout*                   pPipelineLayout)
#if VKI_RAY_TRACING
    : GraphicsPipelineCommon(false, pDevice),
#else
    : GraphicsPipelineCommon(pDevice),
#endif
      m_objectCreateInfo(objectInfo),
      m_pBinaryCreateInfo(pBinaryInfo),
      m_libInfo(libInfo),
      m_elfHash(elfHash)
{
    Util::MetroHash::Hash dummyCacheHash = {};
    Pipeline::Init(
        nullptr,
        pPipelineLayout,
        objectInfo.staticStateMask,
#if VKI_RAY_TRACING
        0,
#endif
        dummyCacheHash,
        apiHash);

    memcpy(m_tempModules,      pTempModules,      ShaderStage::ShaderStageGfxCount * sizeof(ShaderModuleHandle));
    memcpy(m_tempModuleStates, pTempModuleStates, ShaderStage::ShaderStageGfxCount * sizeof(TempModuleState));
}

// =====================================================================================================================
const ShaderModuleHandle* GraphicsPipelineLibrary::GetShaderModuleHandle(
    const ShaderStage stage
    ) const
{
    const ShaderModuleHandle*         pHandle = nullptr;
    VkGraphicsPipelineLibraryFlagsEXT libFlag = 0;

    switch (stage)
    {
    case ShaderStage::ShaderStageTask:
    case ShaderStage::ShaderStageVertex:
    case ShaderStage::ShaderStageTessControl:
    case ShaderStage::ShaderStageTessEval:
    case ShaderStage::ShaderStageGeometry:
    case ShaderStage::ShaderStageMesh:
        libFlag = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
        break;
    case ShaderStage::ShaderStageFragment:
        libFlag = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    // Find shader module handle from temp modules in current library
    if (libFlag & m_libInfo.libFlags)
    {
        for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
        {
            if (stage == m_tempModuleStates[i].stage)
            {
                pHandle = m_tempModules + i;
                break;
            }
            else if (ShaderStageInvalid == m_tempModuleStates[i].stage)
            {
                break;
            }
        }
    }
    // Find the shader module handle from parent library
    else if (libFlag & (m_pBinaryCreateInfo->libFlags & ~m_libInfo.libFlags))
    {
        if (libFlag == VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
        {
            pHandle = m_libInfo.pPreRasterizationShaderLib->GetShaderModuleHandle(stage);
        }
        else if (libFlag == VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
        {
            pHandle = m_libInfo.pFragmentShaderLib->GetShaderModuleHandle(stage);
        }
    }

    return pHandle;
}

}
