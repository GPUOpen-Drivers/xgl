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
 * @file  llpcSpirvLowerResourceCollect.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerResourceCollect.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-resource-collect"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcSpirvLowerResourceCollect.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerResourceCollect::ID = 0;

// =====================================================================================================================
SpirvLowerResourceCollect::SpirvLowerResourceCollect()
    :
    SpirvLower(ID)
{
    initializeSpirvLowerResourceCollectPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerResourceCollect::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Resource-Collect\n");

    SpirvLower::Init(&module);

    m_pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    CollectExecutionModeUsage();

    if (m_shaderStage == ShaderStageVertex)
    {
        // Collect resource usages from vertex input create info
        auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
        auto pVertexInput = pPipelineInfo->pVertexInput;

        // TODO: In the future, we might check if the corresponding vertex attribute is active in vertex shader
        // and set the usage based on this info.
        if (pVertexInput != nullptr)
        {
            for (uint32_t i = 0; i < pVertexInput->vertexBindingDescriptionCount; ++i)
            {
                auto pBinding = &pVertexInput->pVertexBindingDescriptions[i];
                if (pBinding->inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
                {
                    m_pResUsage->builtInUsage.vs.vertexIndex = true;
                    m_pResUsage->builtInUsage.vs.baseVertex = true;
                }
                else
                {
                    LLPC_ASSERT(pBinding->inputRate == VK_VERTEX_INPUT_RATE_INSTANCE);
                    m_pResUsage->builtInUsage.vs.instanceIndex = true;
                    m_pResUsage->builtInUsage.vs.baseInstance = true;
                }
            }
        }
    }
    else if (m_shaderStage == ShaderStageFragment)
    {
        for (auto& func : *m_pModule)
        {
            if (func.getName() == "_Z4Killv")
            {
                m_pResUsage->builtInUsage.fs.discard = true;
            }
        }
    }

    // Collect unused globals and remove them
    std::unordered_set<GlobalVariable*> removedGlobals;
    for (auto pGlobal = m_pModule->global_begin(), pEnd = m_pModule->global_end(); pGlobal != pEnd; ++pGlobal)
    {
        if (pGlobal->user_empty())
        {
            removedGlobals.insert(&*pGlobal);
        }
    }

    for (auto pGlobal : removedGlobals)
    {
        pGlobal->dropAllReferences();
        pGlobal->eraseFromParent();
    }

    // Collect resource usages from globals
    for (auto pGlobal = m_pModule->global_begin(), pEnd = m_pModule->global_end(); pGlobal != pEnd; ++pGlobal)
    {
        const Type* pGlobalTy = pGlobal->getType()->getContainedType(0);

        auto addrSpace = pGlobal->getType()->getAddressSpace();
        switch (addrSpace)
        {
        case SPIRAS_Constant:
            {
                MDNode* pMetaNode = pGlobal->getMetadata(gSPIRVMD::Resource);

                auto descSet = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(0))->getZExtValue();
                auto binding = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(1))->getZExtValue();

                // TODO: Will support separated texture resource/sampler.
                DescriptorType descType = DescriptorType::Texture;

                // NOTE: For texture buffer and image buffer, the descriptor type should be set to "TexelBuffer".
                if (pGlobalTy->isPointerTy())
                {
                    Type* pImageType = pGlobalTy->getPointerElementType();
                    std::string imageTypeName = pImageType->getStructName();
                    // Format of image opaque type: ...[.SampledImage.<date type><dim>]...
                    if (imageTypeName.find(".SampledImage") != std::string::npos)
                    {
                        auto pos = imageTypeName.find("_");
                        LLPC_ASSERT(pos != std::string::npos);

                        ++pos;
                        Dim dim = static_cast<Dim>(imageTypeName[pos] - '0');
                        if (dim == DimBuffer)
                        {
                            descType = DescriptorType::TexelBuffer;
                        }
                        else if (dim == DimSubpassData)
                        {
                            LLPC_ASSERT(m_shaderStage == ShaderStageFragment);
                            m_pResUsage->builtInUsage.fs.fragCoord = true;
                        }
                    }
                }

                DescriptorBinding bindingInfo = {};
                bindingInfo.descType  = descType;
                bindingInfo.arraySize = GetFlattenArrayElementCount(pGlobalTy);

                CollectDescriptorUsage(descSet, binding, &bindingInfo);
                break;
            }
        case SPIRAS_Private:
        case SPIRAS_Global:
        case SPIRAS_Local:
            {
                // TODO: Will be implemented.
                break;
            }
        case SPIRAS_Input:
        case SPIRAS_Output:
            {
                MDNode* pMetaNode = pGlobal->getMetadata(gSPIRVMD::InOut);
                auto pMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

                if (pGlobalTy->isArrayTy())
                {
                    // NOTE: For tessellation shader and geometry shader, the outermost array index might be
                    // used for vertex indexing. Thus, it should be counted out when collect input/output usage
                    // info.
                    const bool isGsInput   = ((m_shaderStage == ShaderStageGeometry) && (addrSpace == SPIRAS_Input));
                    const bool isTcsInput  = ((m_shaderStage == ShaderStageTessControl) &&
                                              (addrSpace == SPIRAS_Input));
                    const bool isTcsOutput = ((m_shaderStage == ShaderStageTessControl) &&
                                              (addrSpace == SPIRAS_Output));
                    const bool isTesInput  = ((m_shaderStage == ShaderStageTessEval) && (addrSpace == SPIRAS_Input));

                    bool isVertexIdx = false;

                    if (isGsInput || isTcsInput || isTcsOutput || isTesInput)
                    {
                        ShaderInOutMetadata inOutMeta = {};
                        inOutMeta.U32All = cast<ConstantInt>(pMeta->getOperand(1))->getZExtValue();

                        if (inOutMeta.IsBuiltIn)
                        {
                            const BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);
                            isVertexIdx = ((builtInId == BuiltInPerVertex)    || // GLSL style per-vertex data
                                           (builtInId == BuiltInPosition)     || // HLSL style per-vertex data
                                           (builtInId == BuiltInPointSize)    ||
                                           (builtInId == BuiltInClipDistance) ||
                                           (builtInId == BuiltInCullDistance));
                        }
                        else
                        {
                            isVertexIdx = (isGsInput || isTcsInput ||
                                           ((isTcsOutput || isTesInput) && (inOutMeta.PerPatch == false)));
                        }
                    }

                    if (isVertexIdx)
                    {
                        // The outermost array index is for vertex indexing
                        pGlobalTy = pGlobalTy->getArrayElementType();
                        pMeta = cast<Constant>(pMeta->getOperand(2));
                    }
                }

                CollectInOutUsage(pGlobalTy, pMeta, static_cast<SPIRAddressSpace>(addrSpace));
                break;
            }
        case SPIRAS_Uniform:
            {
                // Buffer block
                MDNode* pMetaNode = pGlobal->getMetadata(gSPIRVMD::Resource);
                auto descSet   = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(0))->getZExtValue();
                auto binding   = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(1))->getZExtValue();
                auto blockType = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(2))->getZExtValue();
                LLPC_ASSERT((blockType == BlockTypeUniform) || (blockType == BlockTypeShaderStorage));

                DescriptorBinding bindingInfo = {};
                bindingInfo.descType  = (blockType == BlockTypeUniform) ? DescriptorType::UniformBlock :
                                                                          DescriptorType::ShaderStorageBlock;
                bindingInfo.arraySize = GetFlattenArrayElementCount(pGlobalTy);

                CollectDescriptorUsage(descSet, binding, &bindingInfo);
                break;
            }
        case SPIRAS_PushConst:
            {
                // Push constant
                MDNode* pMetaNode = pGlobal->getMetadata(gSPIRVMD::PushConst);
                auto pushConstSize = mdconst::dyn_extract<ConstantInt>(pMetaNode->getOperand(0))->getZExtValue();
                m_pResUsage->pushConstSizeInBytes = pushConstSize;
                break;
            }
        default:
            {
                LLPC_NEVER_CALLED();
                break;
            }
        }
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Gets element count if the specified type is an array (flattened for multi-dimension array).
uint32_t SpirvLowerResourceCollect::GetFlattenArrayElementCount(
    const Type* pTy // [in] Type to check
    ) const
{
    uint32_t elemCount = 1;

    auto pArrayTy = dyn_cast<ArrayType>(pTy);
    while (pArrayTy != nullptr)
    {
        elemCount *= pArrayTy->getArrayNumElements();
        pArrayTy = dyn_cast<ArrayType>(pArrayTy->getArrayElementType());
    }
    return elemCount;
}

// =====================================================================================================================
// Gets element type if the specified type is an array (flattened for multi-dimension array).
const Type* SpirvLowerResourceCollect::GetFlattenArrayElementType(
    const Type* pTy // [in] Type to check
    ) const
{
    const Type* pElemType = pTy;

    auto pArrayTy = dyn_cast<ArrayType>(pTy);
    while (pArrayTy != nullptr)
    {
        pElemType = pArrayTy->getArrayElementType();
        pArrayTy = dyn_cast<ArrayType>(pElemType);
    }
    return pElemType;
}

// =====================================================================================================================
// Collects the usage of execution modes from entry-point metadata.
void SpirvLowerResourceCollect::CollectExecutionModeUsage()
{
    const auto execModel = static_cast<ExecutionModel>(m_shaderStage);
    std::string execModeMetaName = gSPIRVMD::ExecutionMode + std::string(".") + getName(execModel);

    ShaderExecModeMetadata execModeMeta = {};

    auto pEntryMetaNodes = m_pModule->getNamedMetadata(gSPIRVMD::EntryPoints);
    LLPC_ASSERT(pEntryMetaNodes != nullptr);

    for (uint32_t entryIdx = 0, entryCount = pEntryMetaNodes->getNumOperands(); entryIdx < entryCount; ++entryIdx)
    {
        auto pEntryMetaNode = pEntryMetaNodes->getOperand(entryIdx);
        if (pEntryMetaNode->getNumOperands() == 0)
        {
            continue;
        }

        for (uint32_t argIdx = 1, argCount = pEntryMetaNode->getNumOperands(); argIdx < argCount; ++argIdx)
        {
            auto pArgMetaNode = dyn_cast<MDNode>(pEntryMetaNode->getOperand(argIdx));
            if (pArgMetaNode == nullptr)
            {
                continue;
            }

            auto argName = dyn_cast<MDString>(pArgMetaNode->getOperand(0))->getString();
            if (argName == execModeMetaName)
            {
                execModeMeta.U32All[0] =
                    mdconst::dyn_extract<ConstantInt>(pArgMetaNode->getOperand(1))->getZExtValue();
                execModeMeta.U32All[1] =
                    mdconst::dyn_extract<ConstantInt>(pArgMetaNode->getOperand(2))->getZExtValue();
                execModeMeta.U32All[2] =
                    mdconst::dyn_extract<ConstantInt>(pArgMetaNode->getOperand(3))->getZExtValue();

                if (m_shaderStage == ShaderStageTessControl)
                {
                    LLPC_ASSERT(execModeMeta.ts.OutputVertices <= MaxTessPatchVertices);
                    m_pResUsage->builtInUsage.tcs.outputVertices = execModeMeta.ts.OutputVertices;

                    // NOTE: These execution modes belong to tessellation evaluation shader. But SPIR-V allows
                    // them to appear in tessellation control shader.
                    m_pResUsage->builtInUsage.tcs.vertexSpacing = SpacingUnknown;
                    if (execModeMeta.ts.SpacingEqual)
                    {
                        m_pResUsage->builtInUsage.tcs.vertexSpacing = SpacingEqual;
                    }
                    else if (execModeMeta.ts.SpacingFractionalEven)
                    {
                        m_pResUsage->builtInUsage.tcs.vertexSpacing = SpacingFractionalEven;
                    }
                    else if (execModeMeta.ts.SpacingFractionalOdd)
                    {
                        m_pResUsage->builtInUsage.tcs.vertexSpacing = SpacingFractionalOdd;
                    }

                    m_pResUsage->builtInUsage.tcs.vertexOrder = VertexOrderUnknown;
                    if (execModeMeta.ts.VertexOrderCw)
                    {
                        m_pResUsage->builtInUsage.tcs.vertexOrder = VertexOrderCw;
                    }
                    else if (execModeMeta.ts.VertexOrderCcw)
                    {
                        m_pResUsage->builtInUsage.tcs.vertexOrder = VertexOrderCcw;
                    }

                    m_pResUsage->builtInUsage.tcs.primitiveMode = Unknown;
                    if (execModeMeta.ts.Triangles)
                    {
                        m_pResUsage->builtInUsage.tcs.primitiveMode = Triangles;
                    }
                    else if (execModeMeta.ts.Quads)
                    {
                        m_pResUsage->builtInUsage.tcs.primitiveMode = Quads;
                    }
                    else if (execModeMeta.ts.Isolines)
                    {
                        m_pResUsage->builtInUsage.tcs.primitiveMode = Isolines;
                    }

                    m_pResUsage->builtInUsage.tcs.pointMode = false;
                    if (execModeMeta.ts.PointMode)
                    {
                        m_pResUsage->builtInUsage.tcs.pointMode = true;
                    }
                }
                else if (m_shaderStage == ShaderStageTessEval)
                {
                    m_pResUsage->builtInUsage.tes.vertexSpacing = SpacingUnknown;
                    if (execModeMeta.ts.SpacingEqual)
                    {
                        m_pResUsage->builtInUsage.tes.vertexSpacing = SpacingEqual;
                    }
                    else if (execModeMeta.ts.SpacingFractionalEven)
                    {
                        m_pResUsage->builtInUsage.tes.vertexSpacing = SpacingFractionalEven;
                    }
                    else if (execModeMeta.ts.SpacingFractionalOdd)
                    {
                        m_pResUsage->builtInUsage.tes.vertexSpacing = SpacingFractionalOdd;
                    }

                    m_pResUsage->builtInUsage.tes.vertexOrder = VertexOrderUnknown;
                    if (execModeMeta.ts.VertexOrderCw)
                    {
                        m_pResUsage->builtInUsage.tes.vertexOrder = VertexOrderCw;
                    }
                    else if (execModeMeta.ts.VertexOrderCcw)
                    {
                        m_pResUsage->builtInUsage.tes.vertexOrder = VertexOrderCcw;
                    }

                    m_pResUsage->builtInUsage.tes.primitiveMode = Unknown;
                    if (execModeMeta.ts.Triangles)
                    {
                        m_pResUsage->builtInUsage.tes.primitiveMode = Triangles;
                    }
                    else if (execModeMeta.ts.Quads)
                    {
                        m_pResUsage->builtInUsage.tes.primitiveMode = Quads;
                    }
                    else if (execModeMeta.ts.Isolines)
                    {
                        m_pResUsage->builtInUsage.tes.primitiveMode = Isolines;
                    }

                    m_pResUsage->builtInUsage.tes.pointMode = false;
                    if (execModeMeta.ts.PointMode)
                    {
                        m_pResUsage->builtInUsage.tes.pointMode = true;
                    }

                    // NOTE: This execution mode belongs to tessellation control shader. But SPIR-V allows
                    // it to appear in tessellation evaluation shader.
                    LLPC_ASSERT(execModeMeta.ts.OutputVertices <= MaxTessPatchVertices);
                    m_pResUsage->builtInUsage.tes.outputVertices = execModeMeta.ts.OutputVertices;
                }
                else if (m_shaderStage == ShaderStageGeometry)
                {
                    m_pResUsage->builtInUsage.gs.invocations = 1;
                    if (execModeMeta.gs.Invocations > 0)
                    {
                        LLPC_ASSERT(execModeMeta.gs.Invocations <= MaxGeometryInvocations);
                        m_pResUsage->builtInUsage.gs.invocations = execModeMeta.gs.Invocations;
                    }

                    LLPC_ASSERT(execModeMeta.gs.OutputVertices <= MaxGeometryOutputVertices);
                    m_pResUsage->builtInUsage.gs.outputVertices = execModeMeta.gs.OutputVertices;

                    if (execModeMeta.gs.InputPoints)
                    {
                        m_pResUsage->builtInUsage.gs.inputPrimitive = InputPoints;
                    }
                    else if (execModeMeta.gs.InputLines)
                    {
                        m_pResUsage->builtInUsage.gs.inputPrimitive = InputLines;
                    }
                    else if (execModeMeta.gs.InputLinesAdjacency)
                    {
                        m_pResUsage->builtInUsage.gs.inputPrimitive = InputLinesAdjacency;
                    }
                    else if (execModeMeta.gs.Triangles)
                    {
                        m_pResUsage->builtInUsage.gs.inputPrimitive = InputTriangles;
                    }
                    else if (execModeMeta.gs.InputTrianglesAdjacency)
                    {
                        m_pResUsage->builtInUsage.gs.inputPrimitive = InputTrianglesAdjacency;
                    }

                    if (execModeMeta.gs.OutputPoints)
                    {
                        m_pResUsage->builtInUsage.gs.outputPrimitive = OutputPoints;
                    }
                    else if (execModeMeta.gs.OutputLineStrip)
                    {
                        m_pResUsage->builtInUsage.gs.outputPrimitive = OutputLineStrip;
                    }
                    else if (execModeMeta.gs.OutputTriangleStrip)
                    {
                        m_pResUsage->builtInUsage.gs.outputPrimitive = OutputTriangleStrip;
                    }
                }
                else if (m_shaderStage == ShaderStageFragment)
                {
                    m_pResUsage->builtInUsage.fs.originUpperLeft    = execModeMeta.fs.OriginUpperLeft;
                    m_pResUsage->builtInUsage.fs.pixelCenterInteger = execModeMeta.fs.PixelCenterInteger;
                    m_pResUsage->builtInUsage.fs.earlyFragmentTests = execModeMeta.fs.EarlyFragmentTests;

                    m_pResUsage->builtInUsage.fs.depthMode = DepthReplacing;
                    if (execModeMeta.fs.DepthReplacing)
                    {
                        m_pResUsage->builtInUsage.fs.depthMode = DepthReplacing;
                    }
                    else if (execModeMeta.fs.DepthGreater)
                    {
                        m_pResUsage->builtInUsage.fs.depthMode = DepthGreater;
                    }
                    else if (execModeMeta.fs.DepthLess)
                    {
                        m_pResUsage->builtInUsage.fs.depthMode = DepthLess;
                    }
                    else if (execModeMeta.fs.DepthUnchanged)
                    {
                        m_pResUsage->builtInUsage.fs.depthMode = DepthUnchanged;
                    }
                }
                else if (m_shaderStage == ShaderStageCompute)
                {
                    LLPC_ASSERT((execModeMeta.cs.LocalSizeX <= MaxComputeWorkgroupSize) &&
                                (execModeMeta.cs.LocalSizeY <= MaxComputeWorkgroupSize) &&
                                (execModeMeta.cs.LocalSizeZ <= MaxComputeWorkgroupSize));

                    m_pResUsage->builtInUsage.cs.workgroupSizeX =
                        (execModeMeta.cs.LocalSizeX > 0) ? execModeMeta.cs.LocalSizeX : 1;
                    m_pResUsage->builtInUsage.cs.workgroupSizeY =
                        (execModeMeta.cs.LocalSizeY > 0) ? execModeMeta.cs.LocalSizeY : 1;
                    m_pResUsage->builtInUsage.cs.workgroupSizeZ =
                        (execModeMeta.cs.LocalSizeZ > 0) ? execModeMeta.cs.LocalSizeZ : 1;
                }

                break;
            }
        }
    }
}

// =====================================================================================================================
// Collects the usage info of descriptor sets and their bindings.
void SpirvLowerResourceCollect::CollectDescriptorUsage(
    uint32_t                 descSet,       // ID of descriptor set
    uint32_t                 binding,       // ID of descriptor binding
    const DescriptorBinding* pBindingInfo)  // [in] Descriptor binding info
{
    // The set ID is somewhat larger than expected
    if ((descSet + 1) > m_pResUsage->descSets.size())
    {
        m_pResUsage->descSets.resize(descSet + 1);
    }

    auto pDescSet = &m_pResUsage->descSets[descSet];
    static const DescriptorBinding DummyBinding = {};
    while ((binding + 1) > pDescSet->size())
    {
        // Insert dummy bindings till the binding ID is reached
        pDescSet->push_back(DummyBinding);
    }

    (*pDescSet)[binding] = *pBindingInfo;
}

// =====================================================================================================================
// Collects the usage info of inputs and outputs.
void SpirvLowerResourceCollect::CollectInOutUsage(
    const Type*      pInOutTy,      // [in] Type of this input/output
    Constant*        pInOutMeta,    // [in] Metadata of this input/output
    SPIRAddressSpace addrSpace)     // Address space
{
    LLPC_ASSERT((addrSpace == SPIRAS_Input) || (addrSpace == SPIRAS_Output));

    ShaderInOutMetadata inOutMeta = {};
    uint32_t locCount = 0;

    const Type* pBaseTy = nullptr;
    if (pInOutTy->isArrayTy())
    {
        // Input/output is array type
        inOutMeta.U32All = cast<ConstantInt>(pInOutMeta->getOperand(1))->getZExtValue();

        if (inOutMeta.IsBuiltIn)
        {
            // Built-in arrayed input/output
            const BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);

            if (m_shaderStage == ShaderStageVertex)
            {
                switch (builtInId)
                {
                case BuiltInClipDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);
                        m_pResUsage->builtInUsage.vs.clipDistance = elemCount;
                        break;
                    }
                case BuiltInCullDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);
                        m_pResUsage->builtInUsage.vs.cullDistance = elemCount;
                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
            else if (m_shaderStage == ShaderStageTessControl)
            {
                switch (builtInId)
                {
                case BuiltInClipDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.tcs.clipDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.tcs.clipDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInCullDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.tcs.cullDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.tcs.cullDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInTessLevelOuter:
                    {
                        m_pResUsage->builtInUsage.tcs.tessLevelOuter = true;
                        break;
                    }
                case BuiltInTessLevelInner:
                    {
                        m_pResUsage->builtInUsage.tcs.tessLevelInner = true;
                        break;
                    }
                case BuiltInPerVertex:
                    {
                        // Do nothing
                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
            else if (m_shaderStage == ShaderStageTessEval)
            {
                switch (builtInId)
                {
                case BuiltInClipDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.tes.clipDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.tes.clipDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInCullDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.tes.cullDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.tes.cullDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInTessLevelOuter:
                    {
                        m_pResUsage->builtInUsage.tes.tessLevelOuter = true;
                        break;
                    }
                case BuiltInTessLevelInner:
                    {
                        m_pResUsage->builtInUsage.tes.tessLevelInner = true;
                        break;
                    }
                case BuiltInPerVertex:
                    {
                        if (addrSpace == SPIRAS_Input)
                        {
                            // Do nothing
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            LLPC_NEVER_CALLED();
                        }
                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
            else if (m_shaderStage == ShaderStageGeometry)
            {
                switch (builtInId)
                {
                case BuiltInClipDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.gs.clipDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.gs.clipDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInCullDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.gs.cullDistanceIn = elemCount;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.gs.cullDistance = elemCount;
                        }
                        break;
                    }
                case BuiltInPerVertex:
                    {
                        if (addrSpace == SPIRAS_Input)
                        {
                            // Do nothing
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            LLPC_NEVER_CALLED();
                        }
                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
            else if (m_shaderStage == ShaderStageFragment)
            {
                switch (builtInId)
                {
                case BuiltInClipDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);
                        m_pResUsage->builtInUsage.fs.clipDistance = elemCount;

                        // NOTE: gl_ClipDistance[] is emulated via general inputs. Those qualifiers therefore have to
                        // be marked as used.
                        m_pResUsage->builtInUsage.fs.noperspective = true;
                        m_pResUsage->builtInUsage.fs.center = true;
                        break;
                    }
                case BuiltInCullDistance:
                    {
                        const uint32_t elemCount = pInOutTy->getArrayNumElements();
                        LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);
                        m_pResUsage->builtInUsage.fs.cullDistance = elemCount;

                        // NOTE: gl_CullDistance[] is emulated via general inputs. Those qualifiers therefore have to
                        // be marked as used.
                        m_pResUsage->builtInUsage.fs.noperspective = true;
                        m_pResUsage->builtInUsage.fs.center = true;
                        break;
                    }
                case BuiltInSampleMask:
                    {
                        if (addrSpace == SPIRAS_Input)
                        {
                            m_pResUsage->builtInUsage.fs.sampleMaskIn = true;
                        }
                        else
                        {
                            LLPC_ASSERT(addrSpace == SPIRAS_Output);
                            m_pResUsage->builtInUsage.fs.sampleMask = true;
                        }

                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
        }
        else
        {
            // Generic arrayed input/output
            uint32_t stride  = cast<ConstantInt>(pInOutMeta->getOperand(0))->getZExtValue();

            const uint32_t startLoc = inOutMeta.Value;

            pBaseTy = GetFlattenArrayElementType(pInOutTy);
            locCount = (pInOutTy->getPrimitiveSizeInBits() > SizeOfVec4) ? 2 : 1;
            locCount *= (stride * cast<ArrayType>(pInOutTy)->getNumElements());

            // Prepare for location mapping
            if (addrSpace == SPIRAS_Input)
            {
                if (inOutMeta.PerPatch)
                {
                    LLPC_ASSERT(m_shaderStage == ShaderStageTessEval);
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.perPatchInputLocMap[startLoc + i] = InvalidValue;
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.inputLocMap[startLoc + i] = InvalidValue;
                    }
                }
            }
            else
            {
                LLPC_ASSERT(addrSpace == SPIRAS_Output);

                if (inOutMeta.PerPatch)
                {
                    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.perPatchOutputLocMap[startLoc + i] = InvalidValue;
                    }
                }
                else
                {
                    // TODO: Multiple output streams are not supported.
                    if (m_shaderStage != ShaderStageGeometry ||
                        ((m_shaderStage == ShaderStageGeometry) && (inOutMeta.StreamId == 0)))
                    {
                        for (uint32_t i = 0; i < locCount; ++i)
                        {
                            m_pResUsage->inOutUsage.outputLocMap[startLoc + i] = InvalidValue;
                        }
                    }
                }
            }

            // Special stage-specific processing
            if (m_shaderStage == ShaderStageVertex)
            {
                if (addrSpace == SPIRAS_Input)
                {
                    CollectVertexInputUsage(pBaseTy, (inOutMeta.Signedness != 0), startLoc, locCount);
                }
            }
            else if (m_shaderStage == ShaderStageFragment)
            {
                if (addrSpace == SPIRAS_Input)
                {
                    // Collect interpolation info
                    if (inOutMeta.InterpMode == InterpModeSmooth)
                    {
                        m_pResUsage->builtInUsage.fs.smooth = true;
                    }
                    else if (inOutMeta.InterpMode == InterpModeFlat)
                    {
                        m_pResUsage->builtInUsage.fs.flat = true;
                    }
                    else
                    {
                        LLPC_ASSERT(inOutMeta.InterpMode == InterpModeNoPersp);
                        m_pResUsage->builtInUsage.fs.noperspective = true;
                    }

                    if (inOutMeta.InterpLoc == InterpLocCenter)
                    {
                        m_pResUsage->builtInUsage.fs.center = true;
                    }
                    else if (inOutMeta.InterpLoc == InterpLocCentroid)
                    {
                        m_pResUsage->builtInUsage.fs.centroid = true;
                    }
                    else
                    {
                        LLPC_ASSERT(inOutMeta.InterpLoc == InterpLocSample);
                        m_pResUsage->builtInUsage.fs.sample = true;
                        m_pResUsage->builtInUsage.fs.runAtSampleRate = true;
                    }
                }
            }
        }
    }
    else if (pInOutTy->isStructTy())
    {
        // Input/output is structure type
        const uint32_t memberCount = pInOutTy->getStructNumElements();
        for (uint32_t memberIdx = 0; memberIdx < memberCount; ++memberIdx)
        {
            auto pMemberTy = pInOutTy->getStructElementType(memberIdx);
            auto pMemberMeta = cast<Constant>(pInOutMeta->getOperand(memberIdx));
            CollectInOutUsage(pMemberTy, pMemberMeta, addrSpace); // Collect usages for structure member
        }
    }
    else
    {
        // Input/output is scalar or vector type
        LLPC_ASSERT(pInOutTy->isSingleValueType());

        inOutMeta.U32All = cast<ConstantInt>(pInOutMeta)->getZExtValue();

        if (inOutMeta.IsBuiltIn)
        {
            // Built-in input/output
            const BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);

            if (m_shaderStage == ShaderStageVertex)
            {
                switch (builtInId)
                {
                case BuiltInVertexIndex:
                    m_pResUsage->builtInUsage.vs.vertexIndex = true;
                    m_pResUsage->builtInUsage.vs.baseVertex = true;
                    break;
                case BuiltInInstanceIndex:
                    m_pResUsage->builtInUsage.vs.instanceIndex = true;
                    m_pResUsage->builtInUsage.vs.baseInstance = true;
                    break;
                case BuiltInBaseVertex:
                    m_pResUsage->builtInUsage.vs.baseVertex = true;
                    break;
                case BuiltInBaseInstance:
                    m_pResUsage->builtInUsage.vs.baseInstance = true;
                    break;
                case BuiltInDrawIndex:
                    m_pResUsage->builtInUsage.vs.drawIndex = true;
                    break;
                case BuiltInPosition:
                    m_pResUsage->builtInUsage.vs.position = true;
                    break;
                case BuiltInPointSize:
                    m_pResUsage->builtInUsage.vs.pointSize = true;
                    break;
                case BuiltInViewportIndex:
                    m_pResUsage->builtInUsage.vs.viewportIndex = true;
                    break;
                case BuiltInLayer:
                    m_pResUsage->builtInUsage.vs.layer = true;
                    break;
                case BuiltInViewIndex:
                    m_pResUsage->builtInUsage.vs.viewIndex = true;
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else if (m_shaderStage == ShaderStageTessControl)
            {
                switch (builtInId)
                {
                case BuiltInPosition:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.tcs.positionIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.tcs.position = true;
                    }
                    break;
                case BuiltInPointSize:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.tcs.pointSizeIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.tcs.pointSize = true;
                    }
                    break;
                case BuiltInPatchVertices:
                    m_pResUsage->builtInUsage.tcs.patchVertices = true;
                    break;
                case BuiltInInvocationId:
                    m_pResUsage->builtInUsage.tcs.invocationId = true;
                    break;
                case BuiltInPrimitiveId:
                    m_pResUsage->builtInUsage.tcs.primitiveId = true;
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else if (m_shaderStage == ShaderStageTessEval)
            {
                switch (builtInId)
                {
                case BuiltInPosition:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.tes.positionIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.tes.position = true;
                    }
                    break;
                case BuiltInPointSize:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.tes.pointSizeIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.tes.pointSize = true;
                    }
                    break;
                case BuiltInPatchVertices:
                    m_pResUsage->builtInUsage.tes.patchVertices = true;
                    break;
                case BuiltInPrimitiveId:
                    m_pResUsage->builtInUsage.tes.primitiveId = true;
                    break;
                case BuiltInTessCoord:
                    m_pResUsage->builtInUsage.tes.tessCoord = true;
                    break;
                case BuiltInViewportIndex:
                    m_pResUsage->builtInUsage.tes.viewportIndex = true;
                    break;
                case BuiltInLayer:
                    m_pResUsage->builtInUsage.tes.layer = true;
                    break;
                case BuiltInViewIndex:
                    m_pResUsage->builtInUsage.tes.viewIndex = true;
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else if (m_shaderStage == ShaderStageGeometry)
            {
                switch (builtInId)
                {
                case BuiltInPosition:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.gs.positionIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.gs.position = true;
                    }
                    break;
                case BuiltInPointSize:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.gs.pointSizeIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.gs.pointSize = true;
                    }
                    break;
                case BuiltInInvocationId:
                    m_pResUsage->builtInUsage.gs.invocationId = true;
                    break;
                case BuiltInViewportIndex:
                    m_pResUsage->builtInUsage.gs.viewportIndex = true;
                    break;
                case BuiltInLayer:
                    m_pResUsage->builtInUsage.gs.layer = true;
                    break;
                case BuiltInViewIndex:
                    m_pResUsage->builtInUsage.gs.viewIndex = true;
                    break;
                case BuiltInPrimitiveId:
                    if (addrSpace == SPIRAS_Input)
                    {
                        m_pResUsage->builtInUsage.gs.primitiveIdIn = true;
                    }
                    else
                    {
                        LLPC_ASSERT(addrSpace == SPIRAS_Output);
                        m_pResUsage->builtInUsage.gs.primitiveId = true;
                    }
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else if (m_shaderStage == ShaderStageFragment)
            {
                switch (builtInId)
                {
                case BuiltInFragCoord:
                    m_pResUsage->builtInUsage.fs.fragCoord = true;
                    break;
                case BuiltInFrontFacing:
                    m_pResUsage->builtInUsage.fs.frontFacing = true;
                    break;
                case BuiltInPointCoord:
                    m_pResUsage->builtInUsage.fs.pointCoord = true;

                    // NOTE: gl_PointCoord is emulated via a general input. Those qualifiers therefore have to
                    // be marked as used.
                    m_pResUsage->builtInUsage.fs.smooth = true;
                    m_pResUsage->builtInUsage.fs.center = true;
                    break;
                case BuiltInPrimitiveId:
                    m_pResUsage->builtInUsage.fs.primitiveId = true;
                    break;
                case BuiltInSampleId:
                    m_pResUsage->builtInUsage.fs.sampleId = true;
                    m_pResUsage->builtInUsage.fs.runAtSampleRate = true;
                    break;
                case BuiltInSamplePosition:
                    m_pResUsage->builtInUsage.fs.samplePosition = true;
                    // NOTE: gl_SamplePostion is derived from gl_SampleID
                    m_pResUsage->builtInUsage.fs.sampleId = true;
                    m_pResUsage->builtInUsage.fs.runAtSampleRate = true;
                    break;
                case BuiltInLayer:
                    m_pResUsage->builtInUsage.fs.layer = true;
                    break;
                case BuiltInViewportIndex:
                    m_pResUsage->builtInUsage.fs.viewportIndex = true;
                    break;
                case BuiltInHelperInvocation:
                    m_pResUsage->builtInUsage.fs.helperInvocation = true;
                    break;
                case BuiltInFragDepth:
                    m_pResUsage->builtInUsage.fs.fragDepth = true;
                    break;
                case BuiltInFragStencilRefEXT:
                    m_pResUsage->builtInUsage.fs.fragStencilRef = true;
                    break;
                case BuiltInViewIndex:
                    m_pResUsage->builtInUsage.fs.viewIndex = true;
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else if (m_shaderStage == ShaderStageCompute)
            {
                switch (builtInId)
                {
                case BuiltInLocalInvocationId:
                    m_pResUsage->builtInUsage.cs.localInvocationId = true;
                    break;
                case BuiltInWorkgroupId:
                    m_pResUsage->builtInUsage.cs.workgroupId = true;
                    break;
                case BuiltInNumWorkgroups:
                    m_pResUsage->builtInUsage.cs.numWorkgroups = true;
                    break;
                case BuiltInGlobalInvocationId:
                    m_pResUsage->builtInUsage.cs.workgroupId = true;
                    m_pResUsage->builtInUsage.cs.localInvocationId = true;
                    break;
                case BuiltInLocalInvocationIndex:
                    m_pResUsage->builtInUsage.cs.workgroupId = true;
                    m_pResUsage->builtInUsage.cs.localInvocationId = true;
                    break;
                case BuiltInSubgroupSize:
                    m_pResUsage->builtInUsage.common.subgroupSize = true;
                    break;
                case BuiltInSubgroupLocalInvocationId:
                    m_pResUsage->builtInUsage.common.subgroupLocalInvocationId = true;
                    break;
                case BuiltInSubgroupEqMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupEqMask = true;
                    break;
                case BuiltInSubgroupGeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGeMask = true;
                    break;
                case BuiltInSubgroupGtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupGtMask = true;
                    break;
                case BuiltInSubgroupLeMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLeMask = true;
                    break;
                case BuiltInSubgroupLtMaskKHR:
                    m_pResUsage->builtInUsage.common.subgroupLtMask = true;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else
            {
                LLPC_NOT_IMPLEMENTED();
            }
        }
        else
        {
            // Generic input/output
            const uint32_t startLoc = inOutMeta.Value;

            pBaseTy = pInOutTy;
            locCount = (pInOutTy->getPrimitiveSizeInBits() / 8 > SizeOfVec4) ? 2 : 1;

            // Prepare for location mapping
            if (addrSpace == SPIRAS_Input)
            {
                if (inOutMeta.PerPatch)
                {
                    LLPC_ASSERT(m_shaderStage == ShaderStageTessEval);
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.perPatchInputLocMap[startLoc + i] = InvalidValue;
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.inputLocMap[startLoc + i] = InvalidValue;
                    }
                }
            }
            else
            {
                LLPC_ASSERT(addrSpace == SPIRAS_Output);

                if (inOutMeta.PerPatch)
                {
                    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.perPatchOutputLocMap[startLoc + i] = InvalidValue;
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.outputLocMap[startLoc + i] = InvalidValue;
                    }
                }
            }

            // Special stage-specific processing
            if (m_shaderStage == ShaderStageVertex)
            {
                if (addrSpace == SPIRAS_Input)
                {
                    CollectVertexInputUsage(pBaseTy, (inOutMeta.Signedness != 0), startLoc, locCount);
                }
            }
            else if (m_shaderStage == ShaderStageFragment)
            {
                if (addrSpace == SPIRAS_Input)
                {
                    // Collect interpolation info
                    if (inOutMeta.InterpMode == InterpModeSmooth)
                    {
                        m_pResUsage->builtInUsage.fs.smooth = true;
                    }
                    else if (inOutMeta.InterpMode == InterpModeFlat)
                    {
                        m_pResUsage->builtInUsage.fs.flat = true;
                    }
                    else
                    {
                        LLPC_ASSERT(inOutMeta.InterpMode == InterpModeNoPersp);
                        m_pResUsage->builtInUsage.fs.noperspective = true;
                    }

                    if (inOutMeta.InterpLoc == InterpLocCenter)
                    {
                        m_pResUsage->builtInUsage.fs.center = true;
                    }
                    else if (inOutMeta.InterpLoc == InterpLocCentroid)
                    {
                        m_pResUsage->builtInUsage.fs.centroid = true;
                    }
                    else
                    {
                        LLPC_ASSERT(inOutMeta.InterpLoc == InterpLocSample);
                        m_pResUsage->builtInUsage.fs.sample = true;
                        m_pResUsage->builtInUsage.fs.runAtSampleRate = true;
                    }
                }
                else
                {
                    LLPC_ASSERT(addrSpace == SPIRAS_Output);

                    // Collect CB shader mask
                    LLPC_ASSERT(pBaseTy->isSingleValueType());
                    const uint32_t compCount = pBaseTy->isVectorTy() ? pBaseTy->getVectorNumElements() : 1;
                    const uint32_t channelMask = ((1 << compCount) - 1);

                    LLPC_ASSERT(startLoc + locCount <= MaxColorTargets);
                    for (uint32_t i = 0; i < locCount; ++i)
                    {
                        m_pResUsage->inOutUsage.fs.cbShaderMask |= (channelMask << 4 * (startLoc + i));
                    }
                }
            }
        }
    }
}

// =====================================================================================================================
// Collects the usage info of vertex inputs (particularly for the map from vertex input location to vertex basic type).
void SpirvLowerResourceCollect::CollectVertexInputUsage(
    const Type* pVertexTy,  // [in] Vertex input type
    bool        signedness, // Whether the type is signed (valid for integer type)
    uint32_t    startLoc,   // Start location
    uint32_t    locCount)   // Count of locations
{
    auto bitWidth = pVertexTy->getScalarSizeInBits();
    auto pCompTy  = pVertexTy->isVectorTy() ? pVertexTy->getVectorElementType() : pVertexTy;

    // Get basic type of vertex input
    BasicType basicTy = BasicType::Unknown;
    if (pCompTy->isIntegerTy())
    {
        // Integer type
        if (bitWidth == 32)
        {
            basicTy = signedness ? BasicType::Int : BasicType::Uint;
        }
        else
        {
            LLPC_ASSERT(bitWidth == 64);
            basicTy = signedness ? BasicType::Int64 : BasicType::Uint64;
        }
    }
    else if (pCompTy->isFloatingPointTy())
    {
        // Floating-point type
        if (bitWidth == 16)
        {
            basicTy = BasicType::Float16;
        }
        else if (bitWidth == 32)
        {
            basicTy = BasicType::Float;
        }
        else
        {
            LLPC_ASSERT(bitWidth == 64);
            basicTy = BasicType::Double;
        }
    }
    else
    {
        LLPC_NEVER_CALLED();
    }

    auto& vsInputTypes = m_pResUsage->inOutUsage.vs.inputTypes;
    while ((startLoc + locCount) > vsInputTypes.size())
    {
        vsInputTypes.push_back(BasicType::Unknown);
    }

    for (uint32_t i = 0; i < locCount; ++i)
    {
        vsInputTypes[startLoc + i] = basicTy;
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for resource collecting.
INITIALIZE_PASS(SpirvLowerResourceCollect, "spirv-lower-resource-collect",
                "Lower SPIR-V resource collecting", false, false)
