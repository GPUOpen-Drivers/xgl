/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
            &pBinInfo->pipelineInfo.vs,
            &pBinInfo->pipelineInfo.tcs,
            &pBinInfo->pipelineInfo.tes,
            &pBinInfo->pipelineInfo.gs,
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
                &pCreateInfo->pipelineInfo.vs,
                &pCreateInfo->pipelineInfo.tcs,
                &pCreateInfo->pipelineInfo.tes,
                &pCreateInfo->pipelineInfo.gs,
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
        }
    }
    else if (pSize != nullptr)
    {
        *pSize = 0;
    }

    return pCreateInfo;
}

// =====================================================================================================================
void GraphicsPipelineLibrary::CreatePartialPipelineBinary(
    const Device*                           pDevice,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    const GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    const bool                              disableRasterization,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo,
    ShaderModuleHandle*                     pTempModules,
    TempModuleState*                        pTempModuleStages)
{
    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    uint32_t tempIdx = 0;

    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if ((pShaderStageInfo->stages[i].pModuleHandle != nullptr) &&
            pCompiler->IsValidShaderModule(pShaderStageInfo->stages[i].pModuleHandle))
        {
            VK_ASSERT(pShaderStageInfo->stages[i].pModuleHandle == &pTempModules[tempIdx]);

            bool canBuildShader = (((pShaderStageInfo->stages[i].stage == ShaderStage::ShaderStageFragment) &&
                                    disableRasterization)
                                   == false);

            if (canBuildShader)
            {
                // We don't take care of return result. Early compile failure in some cases is expected
                pCompiler->CreateGraphicsShaderBinary(
                    pDevice, pShaderStageInfo->stages[i].stage, pBinaryCreateInfo, &pTempModules[tempIdx]);
            }

            pTempModuleStages[tempIdx].stage              = pShaderStageInfo->stages[i].stage;
            pTempModuleStages[tempIdx].needFreeBinaryOnly = false;

            ++tempIdx;
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

            VK_ASSERT(pParentHandle != nullptr);

            if (pParentHandle != nullptr)
            {
                pBinaryCreateInfo->pipelineInfo.enableUberFetchShader = false;

                pTempModules[tempIdx] = *pParentHandle;

                pCompiler->CreateGraphicsShaderBinary(
                    pDevice, ShaderStage::ShaderStageVertex, pBinaryCreateInfo, &pTempModules[tempIdx]);

                pTempModuleStages[tempIdx].stage              = ShaderStage::ShaderStageVertex;
                pTempModuleStages[tempIdx].needFreeBinaryOnly = true;

                ++tempIdx;
            }
        }

        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pFragmentShaderLib != nullptr))
        {
            const ShaderModuleHandle* pParentHandle =
                pLibInfo->pPreRasterizationShaderLib->GetShaderModuleHandle(ShaderStage::ShaderStageFragment);

            VK_ASSERT(pParentHandle != nullptr);

            if (pParentHandle != nullptr)
            {
                pTempModules[tempIdx] = *pParentHandle;

                pCompiler->CreateGraphicsShaderBinary(
                    pDevice, ShaderStage::ShaderStageVertex, pBinaryCreateInfo, &pTempModules[tempIdx]);

                pTempModuleStages[tempIdx].stage              = ShaderStage::ShaderStageFragment;
                pTempModuleStages[tempIdx].needFreeBinaryOnly = true;

                ++tempIdx;
            }
        }
    }

    for (uint32_t i = tempIdx; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        pTempModuleStages[i].stage = ShaderStage::ShaderStageInvalid;
    }

    for (uint32_t i = 0; i < tempIdx; ++i)
    {
        PipelineCompiler::SetPartialGraphicsPipelineBinaryInfo(
            &pTempModules[i], pTempModuleStages[i].stage, pBinaryCreateInfo);
    }
}

// =====================================================================================================================
VkResult GraphicsPipelineLibrary::Create(
    Device*                             pDevice,
    PipelineCache*                      pPipelineCache,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    VkResult                          result           = VK_SUCCESS;
    uint64_t                          apiPsoHash       = 0;
    size_t                            apiSize          = 0;
    void*                             pSysMem          = nullptr;

    GraphicsPipelineLibraryInfo libInfo;
    ExtractLibraryInfo(pCreateInfo, &libInfo);

    // 1. Get pipeline layout
    VK_ASSERT(pCreateInfo->layout != VK_NULL_HANDLE);
    PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    // 2. Fill GraphicsPipelineBinaryCreateInfo
    GraphicsPipelineBinaryCreateInfo binaryCreateInfo = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo = {};
    ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount] = {};
    TempModuleState                  tempModuleStates[ShaderStage::ShaderStageGfxCount] = {};
    VbBindingInfo                    vbInfo = {};
    PipelineInternalBufferInfo       internalBufferInfo = {};
    if (result == VK_SUCCESS)
    {
        result = BuildPipelineBinaryCreateInfo(
            pDevice,
            pCreateInfo,
            pPipelineLayout,
            pPipelineCache,
            &binaryCreateInfo,
            &shaderStageInfo,
            &vbInfo,
            &internalBufferInfo,
            tempModules);
    }

    // 3. Fill GraphicsPipelineObjectCreateInfo
    GraphicsPipelineObjectCreateInfo objectCreateInfo = {};
    if (result == VK_SUCCESS)
    {
        GraphicsPipelineBinaryInfo binaryInfo = {};
        binaryInfo.pOptimizerKey = &binaryCreateInfo.pipelineProfileKey;

        BuildPipelineObjectCreateInfo(
            pDevice,
            pCreateInfo,
            &vbInfo,
            &binaryInfo,
            pPipelineLayout,
            &objectCreateInfo);
    }

    if (result == VK_SUCCESS)
    {
        // 4. Create partial pipeline binary for fast-link
        CreatePartialPipelineBinary(
            pDevice,
            &libInfo,
            &shaderStageInfo,
            objectCreateInfo.immedInfo.rasterizerDiscardEnable,
            &binaryCreateInfo,
            tempModules,
            tempModuleStates);

        // 5. Create pipeline object
        apiPsoHash = BuildApiHash(pCreateInfo, &objectCreateInfo);

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
    // Free the temporary newly-built shader modules
    uint32_t newShaderCount = 0;
    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if ((m_tempModuleStates[i].stage != ShaderStage::ShaderStageInvalid) &&
            (m_tempModuleStates[i].needFreeBinaryOnly == false))
        {
            newShaderCount++;
        }
        else
        {
            break;
        }
    }
    FreeTempModules(pDevice, newShaderCount, m_tempModules);

    // Free the shader binary for the modules whose ownership is not fully belong to current library
    for (uint32_t i = newShaderCount; i < ShaderStage::ShaderStageGfxCount; ++i)
    {
        if ((m_tempModuleStates[i].stage != ShaderStage::ShaderStageInvalid) &&
            m_tempModuleStates[i].needFreeBinaryOnly)
        {
            PipelineCompiler::FreeGraphicsShaderBinary(m_tempModules + i);
        }
        else
        {
            break;
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
    const uint64_t                          apiHash,
    const ShaderModuleHandle*               pTempModules,
    const TempModuleState*                  pTempModuleStates,
    PipelineLayout*                         pPipelineLayout)
    : GraphicsPipelineCommon(pDevice),
      m_objectCreateInfo(objectInfo),
      m_pBinaryCreateInfo(pBinaryInfo),
      m_libInfo(libInfo)
{
    Pipeline::Init(
        nullptr,
        pPipelineLayout,
        nullptr,
        objectInfo.staticStateMask,
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
    case ShaderStage::ShaderStageVertex:
    case ShaderStage::ShaderStageTessControl:
    case ShaderStage::ShaderStageTessEval:
    case ShaderStage::ShaderStageGeometry:
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
