/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 ***********************************************************************************************************************
 * @file  llpcPipelineContext.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PipelineContext.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-pipeline-context"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/CommandLine.h"

#include "llpcCompiler.h"
#include "llpcPipelineContext.h"

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
PipelineContext::PipelineContext(
    GfxIpVersion        gfxIp,     // Graphics IP version info
    const GpuProperty*  pGpuProp,  // [in] GPU property
    MetroHash::Hash*    pHash)     // [in] Pipeline hash code
    :
    m_gfxIp(gfxIp),
    m_hash(*pHash),
    m_pGpuProperty(pGpuProp)
{

}

// =====================================================================================================================
// Gets the name string of GPU target according to graphics IP version info.
const char* PipelineContext::GetGpuNameString() const
{
    struct GpuNameStringMap
    {
        GfxIpVersion gfxIp;
        const char*  pNameString;
    };

    static const GpuNameStringMap GpuNameMap[] =
    {   // Graphics IP  Target Name   Compatible Target Name
        { { 6, 0, 0 }, "tahiti"   },  // [6.0.0] gfx600, tahiti
        { { 6, 0, 1 }, "pitcairn" },  // [6.0.1] gfx601, pitcairn, verde, oland, hainan
        { { 7, 0, 0 }, "bonaire"  },  // [7.0.0] gfx700, bonaire, kaveri
        { { 7, 0, 1 }, "hawaii"   },  // [7.0.1] gfx701, hawaii
        { { 7, 0, 2 }, "gfx702"   },  // [7.0.2] gfx702
        { { 7, 0, 3 }, "kabini"   },  // [7.0.3] gfx703, kabini, mullins
        { { 8, 0, 0 }, "iceland"  },  // [8.0.0] gfx800, iceland
        { { 8, 0, 1 }, "carrizo"  },  // [8.0.1] gfx801, carrizo
        { { 8, 0, 2 }, "tonga"    },  // [8.0.2] gfx802, tonga
        { { 8, 0, 3 }, "fiji"     },  // [8.0.3] gfx803, fiji, polaris10, polaris11
        { { 8, 0, 4 }, "gfx804"   },  // [8.0.4] gfx804
        { { 8, 1, 0 }, "stoney"   },  // [8.1.0] gfx810, stoney
        { { 9, 0, 0 }, "gfx900"   },  // [9.0.0] gfx900
        { { 9, 0, 1 }, "gfx901"   },  // [9.0.1] gfx901
        { { 9, 0, 2 }, "gfx902"   },  // [9.0.2] gfx902
        { { 9, 0, 3 }, "gfx903"   },  // [9.0.3] gfx903
    };

    const GpuNameStringMap* pNameMap = nullptr;
    for (auto& nameMap : GpuNameMap)
    {
        if ((nameMap.gfxIp.major    == m_gfxIp.major) &&
            (nameMap.gfxIp.minor    == m_gfxIp.minor) &&
            (nameMap.gfxIp.stepping == m_gfxIp.stepping))
        {
            pNameMap = &nameMap;
            break;
        }
    }

    return (pNameMap != nullptr) ? pNameMap->pNameString : "";
}

// =====================================================================================================================
// Gets the name string of the abbreviation for GPU target according to graphics IP version info.
const char* PipelineContext::GetGpuNameAbbreviation() const
{
    const char* pNameAbbr = nullptr;
    switch (m_gfxIp.major)
    {
    case 6:
        pNameAbbr = "SI";
        break;
    case 7:
        pNameAbbr = "CI";
        break;
    case 8:
        pNameAbbr = "VI";
        break;
    case 9:
        pNameAbbr = "GFX9";
        break;
    default:
        pNameAbbr = "UNKNOWN";
        break;
    }

    return pNameAbbr;
}

// =====================================================================================================================
// Automatically layout descriptor (used by LLPC standalone tool)
void PipelineContext::AutoLayoutDescriptor(
    ShaderStage shaderStage) // Shader stage
{
    uint32_t userDataIdx = 0;
    auto pResUsage = GetShaderResourceUsage(shaderStage);
    auto pDummyResNodes = GetDummyResourceMapNodes(shaderStage);
    LLPC_ASSERT(pDummyResNodes->empty());

    // Add node for the descriptor table
    uint32_t setNodeCount = 0;
    for (uint32_t setIdx = 0, setCount = pResUsage->descSets.size(); setIdx < setCount; ++setIdx)
    {
        if (pResUsage->descSets[setIdx].size() > 0)
        {
            ResourceMappingNode setNode = {};
            setNode.type = ResourceMappingNodeType::DescriptorTableVaPtr;
            setNode.offsetInDwords = userDataIdx++;
            setNode.sizeInDwords = 1;

            pDummyResNodes->push_back(setNode);
            ++setNodeCount;
        }
    }

    // Add node for vertex buffer table
    const uint32_t vsInputTypeCount = pResUsage->inOutUsage.vs.inputTypes.size();
    if (vsInputTypeCount > 0)
    {
        LLPC_ASSERT(shaderStage == ShaderStageVertex);

        ResourceMappingNode vbNode = {};
        vbNode.type           = ResourceMappingNodeType::IndirectUserDataVaPtr;
        vbNode.sizeInDwords   = 1;
        vbNode.offsetInDwords = userDataIdx++;
        vbNode.userDataPtr.sizeInDwords = vsInputTypeCount * 4;
        pDummyResNodes->push_back(vbNode);

        // Create dummy vertex info
        auto pDummyVertexInput    = GetDummyVertexInputInfo();
        auto pDummyVertexBindings = GetDummyVertexBindings();
        auto pDummyVertexAttribs  = GetDummyVertexAttributes();

        static_assert(static_cast<uint32_t>(BasicType::Unknown) == 0, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Float)   == 1, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Double)  == 2, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int)     == 3, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint)    == 4, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int64)   == 5, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint64)  == 6, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Float16) == 7, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int16)   == 8, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint16)  == 9, "Unexpected value!");

        static const VkFormat DummyVertexFormat[] =
        {
            VK_FORMAT_UNDEFINED,            // BasicType::Unknown
            VK_FORMAT_R32G32B32A32_SFLOAT , // BasicType::Float
            VK_FORMAT_R64G64_SFLOAT,        // BasicType::Double
            VK_FORMAT_R32G32B32A32_SINT,    // BasicType::Int
            VK_FORMAT_R32G32B32A32_UINT,    // BasicType::Uint
            VK_FORMAT_R64G64_SINT,          // BasicType::Int64
            VK_FORMAT_R64G64_UINT,          // BasicType::Uint64
            VK_FORMAT_R16G16B16A16_SFLOAT,  // BasicType::Float16
            VK_FORMAT_R16G16B16A16_SINT,    // BasicType::Int16
            VK_FORMAT_R16G16B16A16_UINT,    // BasicType::Uint16
        };

        for (size_t loc = 0; loc < vsInputTypeCount; ++loc)
        {
            const BasicType basicTy = pResUsage->inOutUsage.vs.inputTypes[loc];
            if (basicTy != BasicType::Unknown)
            {
                VkVertexInputBindingDescription vertexBinding = {};
                vertexBinding.binding   = loc;
                vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                vertexBinding.stride    = SizeOfVec4;

                VkVertexInputAttributeDescription vertexAttrib = {};
                vertexAttrib.binding    = loc;
                vertexAttrib.location   = loc;
                vertexAttrib.offset     = 0;
                vertexAttrib.format     = DummyVertexFormat[static_cast<uint32_t>(basicTy)];

                pDummyVertexBindings->push_back(vertexBinding);
                pDummyVertexAttribs->push_back(vertexAttrib);
            }
        }

        pDummyVertexInput->vertexBindingDescriptionCount   = pDummyVertexBindings->size();
        pDummyVertexInput->pVertexBindingDescriptions      = pDummyVertexBindings->data();

        pDummyVertexInput->vertexAttributeDescriptionCount = pDummyVertexAttribs->size();
        pDummyVertexInput->pVertexAttributeDescriptions    = pDummyVertexAttribs->data();

        // Always assume we use vertex as input rate
        pResUsage->builtInUsage.vs.vertexIndex = true;
        pResUsage->builtInUsage.vs.baseVertex = true;

        auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(GetPipelineBuildInfo());
        const_cast<GraphicsPipelineBuildInfo*>(pPipelineInfo)->pVertexInput = pDummyVertexInput;
    }

    // Add node for push constant
    if (pResUsage->pushConstSizeInBytes > 0)
    {
        ResourceMappingNode pushConstNode = {};
        pushConstNode.type           = ResourceMappingNodeType::PushConst;
        pushConstNode.offsetInDwords = userDataIdx;
        pushConstNode.sizeInDwords   = pResUsage->pushConstSizeInBytes / sizeof(uint32_t);
        userDataIdx += pushConstNode.sizeInDwords;
        pDummyResNodes->push_back(pushConstNode);
    }

    const uint32_t userDataNodeCount = pDummyResNodes->size();

    // Add nodes for generic descriptors (various resources)
    for (uint32_t setIdx = 0, setCount = pResUsage->descSets.size(), setNodeIdx = 0; setIdx < setCount; ++setIdx)
    {
        if (pResUsage->descSets[setIdx].size() > 0)
        {
            uint32_t nodeCount = 0;
            uint32_t nodeOffset = 0;

            auto& descSet = pResUsage->descSets[setIdx];
            for (uint32_t bindingIdx = 0, bindingCount = descSet.size(); bindingIdx < bindingCount; ++bindingIdx)
            {
                // For arrayed binding, flatten it
                auto& binding = descSet[bindingIdx];
                if (binding.arraySize > 0)
                {
                    ResourceMappingNode node = {};
                    node.type             = GetResourceMapNodeType(binding.descType);
                    node.sizeInDwords     = GetResourceMapNodeSize(&binding);
                    node.offsetInDwords   = nodeOffset;
                    node.srdRange.set     = setIdx;
                    node.srdRange.binding = bindingIdx;

                    pDummyResNodes->push_back(node);
                    ++nodeCount;
                    nodeOffset += node.sizeInDwords;
                }
            }

            (*pDummyResNodes)[setNodeIdx].tablePtr.nodeCount = nodeCount;
            ++setNodeIdx;
        }
    }

    // Update info of user data nodes
    auto pShaderInfo = const_cast<PipelineShaderInfo*>(GetPipelineShaderInfo(shaderStage));
    pShaderInfo->userDataNodeCount = userDataNodeCount;
    pShaderInfo->pUserDataNodes = (userDataNodeCount > 0) ? pDummyResNodes->data() : nullptr;

    // Link descriptor set nodes with descriptor nodes
    uint32_t nodeOffset = userDataNodeCount;
    for (uint32_t setNodeIdx = 0; setNodeIdx < setNodeCount; ++setNodeIdx)
    {
        LLPC_ASSERT((*pDummyResNodes)[setNodeIdx].type == ResourceMappingNodeType::DescriptorTableVaPtr);
        (*pDummyResNodes)[setNodeIdx].tablePtr.pNext = &(*pDummyResNodes)[0] + nodeOffset;
        nodeOffset += (*pDummyResNodes)[setNodeIdx].tablePtr.nodeCount;
    }

    // Set dummy color formats for fragment outputs
    if (shaderStage == ShaderStageFragment)
    {
        auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(GetPipelineBuildInfo());
        auto pCbState = &(const_cast<GraphicsPipelineBuildInfo*>(pPipelineInfo)->cbState);

        const uint32_t cbShaderMask = pResUsage->inOutUsage.fs.cbShaderMask;

        static_assert(static_cast<uint32_t>(BasicType::Unknown) == 0, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Float)   == 1, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Double)  == 2, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int)     == 3, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint)    == 4, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int64)   == 5, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint64)  == 6, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Float16) == 7, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Int16)   == 8, "Unexpected value!");
        static_assert(static_cast<uint32_t>(BasicType::Uint16)  == 9, "Unexpected value!");

        static const VkFormat DummyFragColorFormat[][4] =
        {
            // BasicType::Unknown
            {
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
            },
            // BasicType::Float
            {
                VK_FORMAT_R32_SFLOAT,
                VK_FORMAT_R32G32_SFLOAT,
                VK_FORMAT_R32G32B32_SFLOAT,
                VK_FORMAT_R32G32B32A32_SFLOAT,
            },
            // BasicType::Double
            {
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
            },
            // BasicType::Int
            {
                VK_FORMAT_R32_SINT,
                VK_FORMAT_R32G32_SINT,
                VK_FORMAT_R32G32B32_SINT,
                VK_FORMAT_R32G32B32A32_SINT,
            },
            // BasicType::Uint
            {
                VK_FORMAT_R32_UINT,
                VK_FORMAT_R32G32_UINT,
                VK_FORMAT_R32G32B32_UINT,
                VK_FORMAT_R32G32B32A32_UINT,
            },
            // BasicType::Int64
            {
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
            },
            // BasicType::Uint64
            {
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
                VK_FORMAT_UNDEFINED,
            },
            // BasicType::Float16
            {
                VK_FORMAT_R16_SFLOAT,
                VK_FORMAT_R16G16_SFLOAT,
                VK_FORMAT_R16G16B16_SFLOAT,
                VK_FORMAT_R16G16B16A16_SFLOAT,
            },
            // BasicType::Int16
            {
                VK_FORMAT_R16_SINT,
                VK_FORMAT_R16G16_SINT,
                VK_FORMAT_R16G16B16_SINT,
                VK_FORMAT_R16G16B16A16_SINT,
            },
            // BasicType::Uint16
            {
                VK_FORMAT_R16_UINT,
                VK_FORMAT_R16G16_UINT,
                VK_FORMAT_R16G16B16_UINT,
                VK_FORMAT_R16G16B16A16_UINT,
            },
        };

        for (uint32_t i = 0; i < MaxColorTargets; ++i)
        {
            if (pCbState->target[i].format == VK_FORMAT_UNDEFINED)
            {
                const BasicType basicTy = pResUsage->inOutUsage.fs.outputTypes[i];
                if (basicTy != BasicType::Unknown)
                {
                    const uint32_t channelMask = ((cbShaderMask >> (4 * i)) & 0xF);
                    const uint32_t compCount = Log2(Pow2Align(channelMask, 2));
                    LLPC_ASSERT(compCount >= 1);

                    VkFormat format = DummyFragColorFormat[static_cast<uint32_t>(basicTy)][compCount - 1];
                    LLPC_ASSERT(format != VK_FORMAT_UNDEFINED);

                    pCbState->target[i].format = format;
                }
                else
                {
                    // This color target is not used, set R32G32B32A32_SFLOAT as default format
                    pCbState->target[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                }
            }
        }
    }
}

// =====================================================================================================================
// Gets type of the resource mapping node corresponding to the specified descriptor type.
ResourceMappingNodeType PipelineContext::GetResourceMapNodeType(
    DescriptorType descType)  // [in] Descriptor binding info
{

    static_assert(static_cast<uint32_t>(DescriptorType::UniformBlock)       == 0, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::ShaderStorageBlock) == 1, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::Texture)            == 2, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TextureResource)    == 3, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TextureSampler)     == 4, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TexelBuffer)        == 5, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::Image)              == 6, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::SubpassInput)       == 7, "Unexpected value!");

    static const ResourceMappingNodeType NodeTypes[] =
    {
        ResourceMappingNodeType::DescriptorBuffer,          // DescriptorType::UniformBlock
        ResourceMappingNodeType::DescriptorBuffer,          // DescriptorType::ShaderStorageBlock
        ResourceMappingNodeType::DescriptorCombinedTexture, // DescriptorType::Texture
        ResourceMappingNodeType::DescriptorResource,        // DescriptorType::TextureResource
        ResourceMappingNodeType::DescriptorSampler,         // DescriptorType::TextureSampler
        ResourceMappingNodeType::DescriptorTexelBuffer,     // DescriptorType::TexelBuffer
        ResourceMappingNodeType::DescriptorResource,        // DescriptorType::Image
        ResourceMappingNodeType::DescriptorResource,        // DescriptorType::SubpassInput
    };

    return NodeTypes[static_cast<uint32_t>(descType)];
}

// =====================================================================================================================
// Get required size of the resource mapping node corresponding to the specified descriptor binding info.
uint32_t PipelineContext::GetResourceMapNodeSize(
  const DescriptorBinding* pBinding) // [in] Descriptor binding info
{
    static_assert(static_cast<uint32_t>(DescriptorType::UniformBlock)       == 0, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::ShaderStorageBlock) == 1, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::Texture)            == 2, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TextureResource)    == 3, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TextureSampler)     == 4, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::TexelBuffer)        == 5, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::Image)              == 6, "Unexpected value!");
    static_assert(static_cast<uint32_t>(DescriptorType::SubpassInput)       == 7, "Unexpected value!");

    static const uint32_t NodeSizes[] =
    {
        4,      // DescriptorType::UniformBlock
        4,      // DescriptorType::ShaderStorageBlock
        8 + 4,  // DescriptorType::Texture
        8,      // DescriptorType::TextureResource
        4,      // DescriptorType::TextureSampler
        4,      // DescriptorType::TexelBuffer
        8,      // DescriptorType::Image
        8,      // DescriptorType::SubpassInput
    };
    return NodeSizes[static_cast<uint32_t>(pBinding->descType)] * pBinding->arraySize;
}

// =====================================================================================================================
// Updates hash code context from pipeline shader info for shader hash code.
void PipelineContext::UpdateShaderHashForPipelineShaderInfo(
    ShaderStage               stage,           // Shader stage
    const PipelineShaderInfo* pShaderInfo,     // [in] Shader info in specified shader stage
    MetroHash64*              pHasher          // [in,out] Haher to generate hash code
    ) const
{
    const ShaderModuleData* pModuleData = reinterpret_cast<const ShaderModuleData*>(pShaderInfo->pModuleData);
    pHasher->Update(stage);
    pHasher->Update(pModuleData->hash);

    if (pShaderInfo->pEntryTarget)
    {
        size_t entryNameLen = strlen(pShaderInfo->pEntryTarget);
        pHasher->Update(reinterpret_cast<const uint8_t*>(pShaderInfo->pEntryTarget), entryNameLen);
    }

    if ((pShaderInfo->pSpecializationInfo) && (pShaderInfo->pSpecializationInfo->mapEntryCount > 0))
    {
        auto pSpecializationInfo = pShaderInfo->pSpecializationInfo;
        pHasher->Update(pSpecializationInfo->mapEntryCount);
        pHasher->Update(reinterpret_cast<const uint8_t*>(pSpecializationInfo->pMapEntries),
                        sizeof(VkSpecializationMapEntry) * pSpecializationInfo->mapEntryCount);
        pHasher->Update(pSpecializationInfo->dataSize);
        pHasher->Update(reinterpret_cast<const uint8_t*>(pSpecializationInfo->pData), pSpecializationInfo->dataSize);
    }
}

// =====================================================================================================================
// Initializes resource usage of the specified shader stage.
void PipelineContext::InitShaderResourceUsage(
    ShaderStage shaderStage)    // Shader stage
{
    auto pResUsage = GetShaderResourceUsage(shaderStage);

    memset(&pResUsage->builtInUsage, 0, sizeof(pResUsage->builtInUsage));

    pResUsage->pushConstSizeInBytes = 0;
    pResUsage->imageWrite = false;
    pResUsage->perShaderTable = false;

    pResUsage->inOutUsage.inputMapLocCount = 0;
    pResUsage->inOutUsage.outputMapLocCount = 0;
    pResUsage->inOutUsage.perPatchInputMapLocCount = 0;
    pResUsage->inOutUsage.perPatchOutputMapLocCount = 0;

    pResUsage->inOutUsage.expCount = 0;

    pResUsage->inOutUsage.pEsGsRingBufDesc = nullptr;

    if (shaderStage == ShaderStageVertex)
    {
        // NOTE: For vertex shader, PAL expects base vertex and base instance in user data,
        // even if they are not used in shader.
        pResUsage->builtInUsage.vs.baseVertex = true;
        pResUsage->builtInUsage.vs.baseInstance = true;
    }
    else if (shaderStage == ShaderStageTessControl)
    {
        auto& calcFactor = pResUsage->inOutUsage.tcs.calcFactor;

        calcFactor.inVertexStride           = InvalidValue;
        calcFactor.outVertexStride          = InvalidValue;
        calcFactor.patchCountPerThreadGroup = InvalidValue;
        calcFactor.offChip.outPatchStart    = InvalidValue;
        calcFactor.offChip.patchConstStart  = InvalidValue;
        calcFactor.onChip.outPatchStart     = InvalidValue;
        calcFactor.onChip.patchConstStart   = InvalidValue;
        calcFactor.outPatchSize             = InvalidValue;
        calcFactor.patchConstSize           = InvalidValue;

        pResUsage->inOutUsage.tcs.pTessFactorBufDesc    = nullptr;
        pResUsage->inOutUsage.tcs.pPrimitiveId          = nullptr;
        pResUsage->inOutUsage.tcs.pInvocationId         = nullptr;
        pResUsage->inOutUsage.tcs.pRelativeId           = nullptr;
        pResUsage->inOutUsage.tcs.pOffChipLdsDesc       = nullptr;
    }
    else if (shaderStage == ShaderStageTessEval)
    {
        pResUsage->inOutUsage.tes.pTessCoord            = nullptr;
        pResUsage->inOutUsage.tes.pOffChipLdsDesc       = nullptr;
    }
    else if (shaderStage == ShaderStageGeometry)
    {
        pResUsage->inOutUsage.gs.pEsGsOffsets           = nullptr;
        pResUsage->inOutUsage.gs.pGsVsRingBufDesc       = nullptr;
        pResUsage->inOutUsage.gs.pEmitCounterPtr        = nullptr;

        auto& calcFactor = pResUsage->inOutUsage.gs.calcFactor;
        memset(&calcFactor, 0, sizeof(calcFactor));
    }
    else if (shaderStage == ShaderStageFragment)
    {
        for (uint32_t i = 0; i < MaxColorTargets; ++i)
        {
            pResUsage->inOutUsage.fs.expFmts[i] = EXP_FORMAT_ZERO;
            pResUsage->inOutUsage.fs.outputTypes[i] = BasicType::Unknown;
        }

        pResUsage->inOutUsage.fs.cbShaderMask = 0;
        pResUsage->inOutUsage.fs.dualSourceBlend = false;
        pResUsage->inOutUsage.fs.pViewIndex = nullptr;
    }
}

// =====================================================================================================================
// Initializes interface data of the specified shader stage.
void PipelineContext::InitShaderInterfaceData(
    ShaderStage shaderStage)  // Shader stage
{
    auto pIntfData = GetShaderInterfaceData(shaderStage);

    memset(pIntfData->descTablePtrs, 0, sizeof(pIntfData->descTablePtrs));
    memset(pIntfData->shadowDescTablePtrs, 0, sizeof(pIntfData->shadowDescTablePtrs));
    memset(pIntfData->userDataMap, InterfaceData::UserDataUnmapped, sizeof(pIntfData->userDataMap));
    memset(pIntfData->dynDescs, 0, sizeof(pIntfData->dynDescs));

    pIntfData->pInternalTablePtr = nullptr;
    pIntfData->pInternalPerShaderTablePtr = nullptr;
    pIntfData->vbTable.pTablePtr = nullptr;
    pIntfData->pNumWorkgroups = nullptr;

    memset(&pIntfData->entryArgIdxs, 0, sizeof(pIntfData->entryArgIdxs));
    memset(&pIntfData->pushConst, 0, sizeof(pIntfData->pushConst));
    memset(&pIntfData->spillTable, 0, sizeof(pIntfData->spillTable));
    memset(&pIntfData->userDataUsage, 0, sizeof(pIntfData->userDataUsage));

    pIntfData->entryArgIdxs.spillTable = InvalidValue;
    pIntfData->pushConst.resNodeIdx = InvalidValue;
    pIntfData->spillTable.offsetInDwords = InvalidValue;
    pIntfData->vbTable.resNodeIdx = InvalidValue;
}

} // Llpc
