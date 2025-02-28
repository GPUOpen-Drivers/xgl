/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_pipeline_binary.h"
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

            PipelineOptimizerKey* pPipelineProfileKey = static_cast<PipelineOptimizerKey*>(pSystemMem);
            pCreateInfo->pPipelineProfileKey = pPipelineProfileKey;

            pSystemMem = Util::VoidPtrInc(pSystemMem, sizeof(PipelineOptimizerKey));

            pPipelineProfileKey->shaderCount = shaderKeyCount;
            pPipelineProfileKey->pShaders    = static_cast<ShaderOptimizerKey*>(pSystemMem);
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
    const GraphicsPipelineExtStructs&       extStructs,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    const GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    GplModuleState*                         pTempModuleStages)
{
    VkResult          result            = VK_SUCCESS;
    PipelineCompiler* pCompiler         = pDevice->GetCompiler(DefaultDeviceIndex);

    const uint32_t shaderBuildMask = GraphicsPipelineLibrary::CalculateShaderBuildMask(
        pDevice,
        pCreateInfo,
        pLibInfo,
        pShaderStageInfo,
        pBinaryCreateInfo);

    for (uint32_t stageIndex = 0; stageIndex < ShaderStage::ShaderStageGfxCount; ++stageIndex)
    {
        if (Util::TestAnyFlagSet(shaderBuildMask, 1 << stageIndex))
        {
            const GraphicsLibraryType gplType = GetGraphicsLibraryType(pShaderStageInfo->stages[stageIndex].stage);

            result = pCompiler->CreateGraphicsShaderBinary(
                pDevice,
                pPipelineCache,
                gplType,
                pBinaryCreateInfo,
                &pTempModuleStages[stageIndex]);

            if (result != VK_SUCCESS)
            {
                break;
            }
        }
    }

    if ((result == VK_SUCCESS) && pLibInfo->flags.optimize)
    {
        // We need to re-compile some stage if related new state is available
        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pPreRasterizationShaderLib != nullptr))
        {
            // Parent library may not have vertex shader if it uses mesh shader.
            constexpr uint32_t TempIdx = ShaderStage::ShaderStageVertex;

            pBinaryCreateInfo->pipelineInfo.enableUberFetchShader = false;
            pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryPreRaster] = nullptr;

            VK_ASSERT(pTempModuleStages[TempIdx].elfPackage.codeSize == 0);
            result = pCompiler->CreateGraphicsShaderBinary(
                pDevice,
                pPipelineCache,
                GraphicsLibraryPreRaster,
                pBinaryCreateInfo,
                &pTempModuleStages[TempIdx]);
        }

        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pFragmentShaderLib != nullptr) &&
            (result == VK_SUCCESS))
        {
            constexpr uint32_t TempIdx = ShaderStage::ShaderStageFragment;

            VK_ASSERT(pTempModuleStages[TempIdx].elfPackage.codeSize == 0);

            result = pCompiler->CreateGraphicsShaderBinary(
                pDevice,
                pPipelineCache,
                GraphicsLibraryFragment,
                pBinaryCreateInfo,
                &pTempModuleStages[TempIdx]);
        }
    }

    // Create shader libraries for fast-link
    for (uint32_t stage = 0; (result == VK_SUCCESS) && (stage < ShaderStage::ShaderStageGfxCount); ++stage)
    {
        GraphicsLibraryType gplType = GetGraphicsLibraryType(static_cast<ShaderStage>(stage));
        if ((pBinaryCreateInfo->earlyElfPackage[gplType].pCode != nullptr) &&
            (pBinaryCreateInfo->pShaderLibraries[gplType] == nullptr))
        {
            Vkgc::BinaryData  palElfBinary = {};

            palElfBinary = pCompiler->GetSolution(pBinaryCreateInfo->compilerType)->
                ExtractPalElfBinary(pBinaryCreateInfo->earlyElfPackage[gplType]);
            if (palElfBinary.codeSize > 0)
            {
                result = pCompiler->CreateGraphicsShaderLibrary(pDevice,
                                                                palElfBinary,
                                                                pAllocator,
                                                                &pBinaryCreateInfo->pShaderLibraries[gplType]);
            }
        }
    }

    // If there is no fragment shader when create fragment library, we use a null pal graphics library.
    if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) &&
        (pBinaryCreateInfo->pipelineInfo.fs.pModuleData == nullptr) &&
        (pShaderStageInfo->stages[ShaderStageFragment].codeHash.lower == 0) &&
        (pShaderStageInfo->stages[ShaderStageFragment].codeHash.upper == 0))
    {
        const auto& fragmentCreateInfo = pDevice->GetNullFragmentLib()->GetPipelineBinaryCreateInfo();
        pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryFragment] =
            fragmentCreateInfo.pShaderLibraries[GraphicsLibraryFragment];
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipelineLibrary::CreateFinalCacheIds(
    const Device*                           pDevice,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    const GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo)
{
    const uint32_t shaderBuildMask = GraphicsPipelineLibrary::CalculateShaderBuildMask(
        pDevice,
        pCreateInfo,
        pLibInfo,
        pShaderStageInfo,
        pBinaryCreateInfo);

    const PipelineCompilerType compilerType = pDevice->GetCompiler(DefaultDeviceIndex)->
        CheckCompilerType<Vkgc::GraphicsPipelineBuildInfo>(nullptr, 0, 0);

    for (uint32_t stageIndex = 0; stageIndex < ShaderStage::ShaderStageGfxCount; ++stageIndex)
    {
        if (Util::TestAnyFlagSet(shaderBuildMask, 1 << stageIndex))
        {
            const GraphicsLibraryType gplType = GetGraphicsLibraryType(pShaderStageInfo->stages[stageIndex].stage);

            pBinaryCreateInfo->libraryHash[gplType] = Vkgc::IPipelineDumper::GetGraphicsShaderBinaryHash(
                &pBinaryCreateInfo->pipelineInfo,
                (gplType == GraphicsLibraryPreRaster) ? ShaderStageVertex : ShaderStageFragment);

            Util::MetroHash128 hasher;
            hasher.Update(pBinaryCreateInfo->libraryHash[gplType]);
            hasher.Update(compilerType);
            hasher.Update(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetSettingsLoader()->GetSettingsHash());
            hasher.Finalize(pBinaryCreateInfo->earlyElfPackageHash[gplType].bytes);
        }
    }
}

// =====================================================================================================================
uint32_t GraphicsPipelineLibrary::CalculateShaderBuildMask(
    const Device*                           pDevice,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    const GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    const GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo)
{
    const PipelineCompiler* pCompiler          = pDevice->GetCompiler(DefaultDeviceIndex);
    const uint64_t          dynamicStateFlags  = GetDynamicStateFlags(pCreateInfo->pDynamicState, pLibInfo);

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

    uint32_t shaderBuildMask = 0;
    uint32_t gplBuildMask    = 0;

    for (uint32_t stageIndex = 0; stageIndex < ShaderStage::ShaderStageGfxCount; ++stageIndex)
    {
        if (((pShaderInfos[stageIndex]->pModuleData != nullptr) &&
             pCompiler->IsValidShaderModule(pShaderStageInfo->stages[stageIndex].pModuleHandle)) ||
            (pShaderStageInfo->stages[stageIndex].codeHash.lower != 0) ||
            (pShaderStageInfo->stages[stageIndex].codeHash.upper != 0))
        {
            const GraphicsLibraryType gplType = GetGraphicsLibraryType(pShaderStageInfo->stages[stageIndex].stage);

            if (Util::TestAnyFlagSet(gplBuildMask, 1 << gplType))
            {
                continue;
            }

            if ((GetVkGraphicsLibraryFlagBit(pShaderStageInfo->stages[stageIndex].stage) & pLibInfo->libFlags) == 0)
            {
                continue;
            }

            if ((pShaderStageInfo->stages[stageIndex].stage != ShaderStage::ShaderStageFragment) ||
                (IsRasterizationDisabled(pCreateInfo, pLibInfo, dynamicStateFlags) == false))
            {
                gplBuildMask    |= (1 << gplType);
                shaderBuildMask |= (1 << stageIndex);
            }
        }
    }

    return shaderBuildMask;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::Create(
    Device*                             pDevice,
    PipelineCache*                      pPipelineCache,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const GraphicsPipelineExtStructs&   extStructs,
    VkPipelineCreateFlags2KHR           flags,
    uint32_t                            internalFlags,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    VkResult result  = VK_SUCCESS;
    size_t   apiSize = 0;
    void*    pSysMem = nullptr;

    GraphicsPipelineLibraryInfo libInfo = {};
    ExtractLibraryInfo(pDevice, pCreateInfo, extStructs, flags, &libInfo);

    GraphicsPipelineBinaryCreateInfo binaryCreateInfo = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo = {};
    GplModuleState                   tempModuleStates[ShaderStage::ShaderStageGfxCount] = {};

    binaryCreateInfo.pipelineInfo.iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (pCreateInfo->pInputAssemblyState != nullptr)
    {
        binaryCreateInfo.pipelineInfo.iaState.topology = pCreateInfo->pInputAssemblyState->topology;
    }
    else if (pCreateInfo->stageCount > 0)
    {
        for (uint32_t stage = 0; stage < pCreateInfo->stageCount; ++stage)
        {
            if ((pCreateInfo->pStages[stage].stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
                (pCreateInfo->pStages[stage].stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
            {
                binaryCreateInfo.pipelineInfo.iaState.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            }
        }
    }

    if ((internalFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FORCE_LLPC) != 0)
    {
        binaryCreateInfo.compilerType = PipelineCompilerTypeLlpc;
    }

    ShaderModuleHandle    tempModules[ShaderStage::ShaderStageGfxCount]         = {};
    PipelineOptimizerKey  pipelineOptimizerKey                                  = {};
    ShaderOptimizerKey    shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
    uint64_t              apiPsoHash                                            = 0;
    Util::MetroHash::Hash elfHash                                               = {};
    Util::MetroHash::Hash cacheIds[GraphicsLibraryCount]                        = {};
    bool                  binariesProvided                                      = false;
    Util::MetroHash::Hash providedElfHash                                       = {};
    uint32_t              providedLibraryMask                                   = 0;

    const VkPipelineBinaryInfoKHR* pPipelineBinaryInfoKHR = extStructs.pPipelineBinaryInfoKHR;

    if (pPipelineBinaryInfoKHR != nullptr)
    {
        binariesProvided = (pPipelineBinaryInfoKHR->binaryCount > 0);

        for (uint32_t binaryIdx = 0; binaryIdx < pPipelineBinaryInfoKHR->binaryCount; ++binaryIdx)
        {
            const auto pBinary = PipelineBinary::ObjectFromHandle(pPipelineBinaryInfoKHR->pPipelineBinaries[binaryIdx]);

            constexpr uint32_t MetadataSize = sizeof(PipelineBinaryGplMetadata);

            const void* pMemory = pBinary->BinaryData().pCode;

            // Retrieve GPL metadata needed for pipeline binary from the GPL binary
            const auto pGplMetadata = static_cast<const PipelineBinaryGplMetadata*>(pMemory);

            const GraphicsLibraryType gplType = pGplMetadata->gplType;
            providedElfHash                   = pGplMetadata->elfHash;
            pMemory = Util::VoidPtrInc(pMemory, MetadataSize);

            // Set cacheIds with cache Ids that includes hashes of spir-v code and populate the elf binary
            cacheIds[gplType]                                  = pBinary->BinaryKey();
            binaryCreateInfo.earlyElfPackage[gplType].codeSize = pBinary->BinaryData().codeSize - MetadataSize;
            binaryCreateInfo.earlyElfPackage[gplType].pCode    = pMemory;

            // Track provided libraries
            providedLibraryMask |= 1 << gplType;
        }
    }

    static_assert(VK_ARRAY_SIZE(shaderOptimizerKeys) == VK_ARRAY_SIZE(shaderStageInfo.stages),
                  "Please ensure stage count matches between gfx profile key and shader stage info.");

    // 1. Get pipeline layout
    const PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    if (pPipelineLayout == nullptr)
    {
        pPipelineLayout = pDevice->GetNullPipelineLayout();
    }

    PipelineMetadata binaryMetadata = {};

    // 2. Create Api PSO Hash, Elf Hash and CacheId
    result = GraphicsPipelineLibrary::CreateCacheId(
        pDevice,
        pCreateInfo,
        extStructs,
        libInfo,
        flags,
        &shaderStageInfo,
        &binaryCreateInfo,
        pPipelineLayout,
        shaderOptimizerKeys,
        &pipelineOptimizerKey,
        &apiPsoHash,
        &elfHash,
        tempModules,
        &binaryMetadata,
        binaryCreateInfo.earlyElfPackageHash);

    if (result == VK_SUCCESS)
    {
        pDevice->GetCompiler(DefaultDeviceIndex)->ConvertGraphicsPipelineExecutableState(
            pDevice,
            pCreateInfo,
            libInfo,
            flags,
            &shaderStageInfo,
            pPipelineLayout,
            &binaryCreateInfo);

        if (binariesProvided)
        {
            // Override elfHash and cacheId hashes with ones that include hash values calculated using spir-v code, as
            // pipeline_binary allows spir-v to be omitted when using pipeline binary objects to create the pipelines.
            // Using the elfHash and cacheIds calculated previously allows cache lookups to succeed.  The calculation of
            // cacheIds must occur regardless to set up the necessary state for pipeline library creation to succeed.
            elfHash = providedElfHash;

            for (uint32_t gplType = 0; gplType < GraphicsLibraryCount; ++gplType)
            {
                binaryCreateInfo.earlyElfPackageHash[gplType] = cacheIds[gplType];
            }
        }

        // 3. Initialize tempModuleStates
        binaryCreateInfo.apiPsoHash = apiPsoHash;

        for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; stage++)
        {
            if (shaderStageInfo.stages[stage].pModuleHandle != nullptr)
            {
                tempModuleStates[stage].stage = static_cast<ShaderStage>(stage);
            }
            else
            {
                tempModuleStates[stage].stage = ShaderStageInvalid;
            }

            if (pDevice->GetCompiler(DefaultDeviceIndex)->IsValidShaderModule(&tempModules[stage]))
            {
                tempModuleStates[stage].moduleHandle = tempModules[stage];
            }
        }
    }

    PipelineBinaryStorage binaryStorage         = {};
    const bool            storeBinaryToPipeline = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) != 0;

    if (result == VK_SUCCESS)
    {
        // 4. Create partial pipeline binary for fast-link
        result = CreatePartialPipelineBinary(
            pDevice,
            pPipelineCache,
            pCreateInfo,
            extStructs,
            &libInfo,
            &shaderStageInfo,
            &binaryCreateInfo,
            pAllocator,
            tempModuleStates);

        // 5. Store created binaries for pipeline_binary
        if ((result == VK_SUCCESS) && storeBinaryToPipeline)
        {
            uint32 binaryIndex = 0;

            for (uint32_t gplType = 0; gplType < GraphicsLibraryCount; ++gplType)
            {
                if ((binaryCreateInfo.earlyElfPackage[gplType].codeSize != 0)       &&
                    (binaryCreateInfo.earlyElfPackage[gplType].pCode    != nullptr) &&
                    (result == VK_SUCCESS))
                {
                    result = GraphicsPipelineLibrary::WriteGplAndMetadataToPipelineBinary(
                        pAllocator,
                        binaryCreateInfo.earlyElfPackage[gplType],
                        binaryCreateInfo.earlyElfPackageHash[gplType],
                        static_cast<GraphicsLibraryType>(gplType),
                        elfHash,
                        binaryIndex,
                        &binaryStorage);

                    if (result == VK_SUCCESS)
                    {
                        ++binaryIndex;
                    }
                    else
                    {
                        result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    }
                }
            }
        }

        // Clean up temporary storage
        for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
        {
            GraphicsLibraryType gplType = GetGraphicsLibraryType(static_cast<ShaderStage>(stage));

            binaryCreateInfo.earlyElfPackage[gplType].pCode = nullptr;

            if (tempModuleStates[stage].elfPackage.codeSize > 0)
            {
                pDevice->VkInstance()->FreeMem(const_cast<void*>(tempModuleStates[stage].elfPackage.pCode));
                tempModuleStates[stage].elfPackage = {};
            }
        }
    }

    // Cleanup temp memory in binaryCreateInfo.
    pDevice->GetCompiler(DefaultDeviceIndex)->FreeGraphicsPipelineCreateInfo(pDevice, &binaryCreateInfo, true);

    GraphicsPipelineObjectCreateInfo objectCreateInfo   = {};
    size_t                           auxiliarySize      = 0;
    PipelineBinaryStorage*           pPermBinaryStorage = nullptr;

    if (result == VK_SUCCESS)
    {
        // 6. Build pipeline object create info
        BuildPipelineObjectCreateInfo(
            pDevice,
            pCreateInfo,
            extStructs,
            libInfo,
            flags,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &objectCreateInfo,
            &binaryCreateInfo);

        // Calculate object size
        apiSize       = sizeof(GraphicsPipelineLibrary);
        auxiliarySize = 0;
        DumpGraphicsPipelineBinaryCreateInfo(&binaryCreateInfo, nullptr, &auxiliarySize);

        size_t objSize = apiSize + auxiliarySize;

        if (storeBinaryToPipeline)
        {
            objSize += sizeof(PipelineBinaryStorage);
        }

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

        if (storeBinaryToPipeline)
        {
            size_t pipelineBinaryOffset = apiSize + auxiliarySize;

            pPermBinaryStorage = static_cast<PipelineBinaryStorage*>(Util::VoidPtrInc(pSysMem,
                                                                                      pipelineBinaryOffset));

            // Simply copy the existing allocations to the new struct.
            memcpy(pPermBinaryStorage, &binaryStorage, sizeof(PipelineBinaryStorage));
        }

        VK_PLACEMENT_NEW(pSysMem) GraphicsPipelineLibrary(
            pDevice,
            objectCreateInfo,
            pBinInfo,
            libInfo,
            elfHash,
            apiPsoHash,
            tempModuleStates,
            pPermBinaryStorage,
            providedLibraryMask,
            pPipelineLayout);

        *pPipeline = GraphicsPipelineLibrary::HandleFromVoidPointer(pSysMem);

        // Generate feedback info
        PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

        auto pPipelineCreationFeedbackCreateInfo = extStructs.pPipelineCreationFeedbackCreateInfoEXT;

        PipelineCompiler::InitPipelineCreationFeedback(pPipelineCreationFeedbackCreateInfo);

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

        PipelineCompiler::SetPipelineCreationFeedbackInfo(
            pPipelineCreationFeedbackCreateInfo,
            pCreateInfo->stageCount,
            pCreateInfo->pStages,
            &pBinInfo->pipelineFeedback,
            pBinInfo->stageFeedback);
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::CreateCacheId(
    const Device*                           pDevice,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineExtStructs&       extStructs,
    const GraphicsPipelineLibraryInfo&      libInfo,
    VkPipelineCreateFlags2KHR               flags,
    GraphicsPipelineShaderStageInfo*        pShaderStageInfo,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo,
    const PipelineLayout*                   pPipelineLayout,
    ShaderOptimizerKey*                     pShaderOptimizerKeys,
    PipelineOptimizerKey*                   pPipelineOptimizerKey,
    uint64_t*                               pApiPsoHash,
    Util::MetroHash::Hash*                  pElfHash,
    ShaderModuleHandle*                     pTempModules,
    PipelineMetadata*                       pBinaryMetadata,
    Util::MetroHash::Hash*                  pCacheIds)
{
    // 1. Build shader stage infos
    VkResult result = BuildShaderStageInfo(pDevice,
                                           pCreateInfo->stageCount,
                                           pCreateInfo->pStages,
                                           [](const uint32_t inputIdx, const uint32_t stageIdx)
                                           {
                                               return stageIdx;
                                           },
                                           pShaderStageInfo->stages,
                                           pTempModules,
                                           pBinaryCreateInfo->stageFeedback);

    if (result == VK_SUCCESS)
    {
        // 2. Build ShaderOptimizer pipeline key
        GeneratePipelineOptimizerKey(pDevice,
                                     pCreateInfo,
                                     extStructs,
                                     libInfo,
                                     flags,
                                     pShaderStageInfo,
                                     pShaderOptimizerKeys,
                                     pPipelineOptimizerKey);

        // 3. Build API and ELF hashes
        BuildApiHash(pCreateInfo,
                     flags,
                     extStructs,
                     libInfo,
                     *pBinaryCreateInfo,
                     pApiPsoHash,
                     pElfHash);

        // 4. Populate binary create info
        result = pDevice->GetCompiler(DefaultDeviceIndex)->ConvertGraphicsPipelineInfo(
            pDevice,
            pCreateInfo,
            extStructs,
            libInfo,
            flags,
            pShaderStageInfo,
            pPipelineLayout,
            pPipelineOptimizerKey,
            pBinaryMetadata,
            pBinaryCreateInfo);
    }

    if (result == VK_SUCCESS)
    {
        // 5. Create CacheIds
        CreateFinalCacheIds(
            pDevice,
            pCreateInfo,
            &libInfo,
            pShaderStageInfo,
            pBinaryCreateInfo);

        // 6. Copy CacheIds
        for (uint32_t gplType = 0; gplType < GraphicsLibraryCount; ++gplType)
        {
            pCacheIds[gplType] = pBinaryCreateInfo->earlyElfPackageHash[gplType];
        }
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::WriteGplAndMetadataToPipelineBinary(
    const VkAllocationCallbacks*    pAllocator,
    const Vkgc::BinaryData&         binaryData,
    const Util::MetroHash::Hash&    cacheId,
    const GraphicsLibraryType       gplType,
    const Util::MetroHash::Hash&    elfHash,
    const uint32_t                  binaryIndex,
    PipelineBinaryStorage*          pBinaryStorage)
{
    VkResult           result       = VK_SUCCESS;
    constexpr uint32_t MetadataSize = sizeof(PipelineBinaryGplMetadata);
    const size_t       storageSize  = MetadataSize + binaryData.codeSize;

    void* pMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        storageSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);     // retained in the pipeline object

    if (pMemory != nullptr)
    {
        void* pCode = pMemory;

        // Store GPL metadata needed for pipeline binary with the GPL binary
        *static_cast<PipelineBinaryGplMetadata*>(pCode) =
            {
                .gplType = gplType,
                .elfHash = elfHash
            };
        pCode = Util::VoidPtrInc(pCode, MetadataSize);

        // Store the library binary
        memcpy(
            pCode,
            binaryData.pCode,
            binaryData.codeSize);

        InsertBinaryData(
            pBinaryStorage,
            binaryIndex,
            cacheId,
            storageSize,
            pMemory);
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    if (m_altLibrary != nullptr)
    {
        m_altLibrary->Destroy(pDevice, pAllocator);
        m_altLibrary = nullptr;
    }

    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    uint32_t libraryMask = 0;
    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if (m_gplModuleStates[i].stage != ShaderStage::ShaderStageInvalid)
        {
            libraryMask |= (1 << GetGraphicsLibraryType(m_gplModuleStates[i].stage));
        }

        pCompiler->FreeGplModuleState(&m_gplModuleStates[i]);
    }

    for (uint32_t i = 0; i < ArrayLen(m_pBinaryCreateInfo->pShaderLibraries); ++i)
    {
        Pal::IShaderLibrary* pShaderLib = m_pBinaryCreateInfo->pShaderLibraries[i];
        if ((Util::TestAnyFlagSet(libraryMask, 1 << i) || Util::TestAnyFlagSet(m_providedLibraryMask, 1 << i)) &&
            (pShaderLib != nullptr))
        {
            pShaderLib->Destroy();
            pAllocator->pfnFree(pAllocator->pUserData, pShaderLib);
        }
    }

    if (m_pBinaryCreateInfo->pInternalMem != nullptr)
    {
        pDevice->MemMgr()->FreeGpuMem(m_pBinaryCreateInfo->pInternalMem);
        Util::Destructor(m_pBinaryCreateInfo->pInternalMem);
        pDevice->VkInstance()->FreeMem(const_cast<InternalMemory*>(m_pBinaryCreateInfo->pInternalMem));
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
    const GplModuleState*                   pGplModuleStates,
    PipelineBinaryStorage*                  pBinaryStorage,
    const uint32_t                          providedLibraryMask,
    const PipelineLayout*                   pPipelineLayout)
    : GraphicsPipelineCommon(
#if VKI_RAY_TRACING
        false,
#endif
        pDevice),
      m_objectCreateInfo(objectInfo),
      m_pBinaryCreateInfo(pBinaryInfo),
      m_libInfo(libInfo),
      m_elfHash(elfHash),
      m_altLibrary(nullptr),
      m_providedLibraryMask(providedLibraryMask)
{
    Util::MetroHash::Hash dummyCacheHash = {};
    Pipeline::Init(
        nullptr,
        pPipelineLayout,
        pBinaryStorage,
        objectInfo.staticStateMask,
#if VKI_RAY_TRACING
        0,
#endif
        dummyCacheHash,
        apiHash);

    memcpy(m_gplModuleStates, pGplModuleStates, ShaderStage::ShaderStageGfxCount * sizeof(GplModuleState));
}

// =====================================================================================================================
void GraphicsPipelineLibrary::GetOwnedPalShaderLibraries(
    const Pal::IShaderLibrary* pLibraries[GraphicsLibraryCount]
    ) const
{
    uint32_t libraryMask = 0;
    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if (m_gplModuleStates[i].stage != ShaderStage::ShaderStageInvalid)
        {
            libraryMask |= (1 << GetGraphicsLibraryType(m_gplModuleStates[i].stage));
        }
    }

    for (uint32_t i = 0; i < ArrayLen(m_pBinaryCreateInfo->pShaderLibraries); ++i)
    {
        Pal::IShaderLibrary* pShaderLib = m_pBinaryCreateInfo->pShaderLibraries[i];
        if (Util::TestAnyFlagSet(libraryMask, 1 << i) && (pShaderLib != nullptr))
        {
            pLibraries[i] = pShaderLib;
        }
        else
        {
            pLibraries[i] = nullptr;
        }
    }
}
}
