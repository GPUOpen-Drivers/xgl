/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcSpirvLowerGlobal.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerGlobal.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-global"

#include "llvm/IR/Constant.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/PassSupport.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_set>
#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcSpirvLowerGlobal.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerGlobal::ID = 0;

// =====================================================================================================================
SpirvLowerGlobal::SpirvLowerGlobal()
    :
    SpirvLower(ID),
    m_pRetBlock(nullptr),
    m_lowerInputInPlace(false),
    m_lowerOutputInPlace(false)
{
    initializeSpirvLowerGlobalPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerGlobal::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Global\n");

    SpirvLower::Init(&module);

    // Map globals to proxy variables
    for (auto pGlobal = m_pModule->global_begin(), pEnd = m_pModule->global_end(); pGlobal != pEnd; ++pGlobal)
    {
        if (pGlobal->getType()->getAddressSpace() == SPIRAS_Private)
        {
            MapGlobalVariableToProxy(&*pGlobal);
        }
        else if (pGlobal->getType()->getAddressSpace() == SPIRAS_Input)
        {
            MapInputToProxy(&*pGlobal);
        }
        else if (pGlobal->getType()->getAddressSpace() == SPIRAS_Output)
        {
            MapOutputToProxy(&*pGlobal);
        }
    }

    // NOTE: Global variable, inlcude general global variable, input and output is a special constant variable, so if
    // it is referenced by constant expression, we need translate constant expression to normal instruction first,
    // Otherwise, we will hit assert in replaceAllUsesWith() when we replace global variable with proxy variable.
    RemoveConstantExpr();

    // Do lowering operations
    LowerGlobalVar();

    if (m_lowerInputInPlace && m_lowerOutputInPlace)
    {
        // Both input and output have to be lowered in-place (without proxy variables)
        LowerInOutInPlace(); // Just one lowering operation is sufficient
    }
    else
    {
        // Either input or output has to be lowered in-place, not both
        if (m_lowerInputInPlace)
        {
            LowerInOutInPlace();
        }
        else
        {
            LowerInput();
        }

        if (m_lowerOutputInPlace)
        {
            LowerInOutInPlace();
        }
        else
        {
            LowerOutput();
        }
    }

    DEBUG(dbgs() << "After the pass Spirv-Lower-Global: " << module);

    std::string errMsg;
    raw_string_ostream errStream(errMsg);
    if (verifyModule(module, &errStream))
    {
        LLPC_ERRS("Fails to verify module (" DEBUG_TYPE "): " << errStream.str() << "\n");
    }

    return true;
}

// =====================================================================================================================
// Visits "return" instruction.
void SpirvLowerGlobal::visitReturnInst(
    ReturnInst& retInst)    // [in] "Ret" instruction
{
    // Skip if "return" instructions are not expected to be handled.
    if (m_instVisitFlags.checkReturn == false)
    {
        return;
    }

    // We only handle the "return" in entry point
    if (retInst.getParent()->getParent()->getCallingConv() == CallingConv::SPIR_FUNC)
    {
        return;
    }

    LLPC_ASSERT(m_pRetBlock != nullptr); // Must have been created
    auto pBranch = BranchInst::Create(m_pRetBlock, retInst.getParent());
    m_retInsts.insert(&retInst);
}

// =====================================================================================================================
// Visits "call" instruction.
void SpirvLowerGlobal::visitCallInst(
    CallInst& callInst) // [in] "Call" instruction
{
    // Skip if "emit" and interpolaton calls are not expected to be handled
    if ((m_instVisitFlags.checkEmitCall == false) && (m_instVisitFlags.checkInterpCall == false))
    {
        return;
    }

    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    auto mangledName = pCallee->getName();

    if (m_instVisitFlags.checkEmitCall)
    {
        if (mangledName.startswith("_Z10EmitVertex") ||
            mangledName.startswith("_Z16EmitStreamVertex"))
        {
            m_emitCalls.insert(&callInst);
        }
    }
    else
    {
        LLPC_ASSERT(m_instVisitFlags.checkInterpCall);

        if (mangledName.startswith("_Z21interpolateAtCentroid") ||
            mangledName.startswith("_Z19interpolateAtSample") ||
            mangledName.startswith("_Z19interpolateAtOffset"))
        {
            // Translate interpolation functions to LLPC intrinsic calls
            auto pLoadSrc = callInst.getArgOperand(0);
            uint32_t interpLoc = InterpLocUnknown;
            Value* pSampleId = nullptr;
            Value* pPixelOffset = nullptr;

            if (mangledName.startswith("_Z21interpolateAtCentroid"))
            {
                interpLoc = InterpLocCentroid;
            }
            else if (mangledName.startswith("_Z19interpolateAtSample"))
            {
                interpLoc = InterpLocSample;
                pSampleId = callInst.getArgOperand(1);
            }
            else
            {
                interpLoc = InterpLocCenter;
                pPixelOffset = callInst.getArgOperand(1);
            }

            if (isa<GetElementPtrInst>(pLoadSrc))
            {
                // The interpolant is an element of the input
                GetElementPtrInst* pGetElemInst = cast<GetElementPtrInst>(pLoadSrc);

                std::vector<Value*> indexOperands;
                for (uint32_t i = 0, indexOperandCount = pGetElemInst->getNumIndices(); i < indexOperandCount; ++i)
                {
                    indexOperands.push_back(ToInt32Value(m_pContext, pGetElemInst->getOperand(1 + i), &callInst));
                }
                uint32_t operandIdx = 0;

                auto pInput = cast<GlobalVariable>(pGetElemInst->getPointerOperand());
                auto pInputTy = pInput->getType()->getContainedType(0);

                MDNode* pMetaNode = pInput->getMetadata(gSPIRVMD::InOut);
                LLPC_ASSERT(pMetaNode != nullptr);
                auto pInputMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

                auto pLoadValue = LoadInOutMember(pInputTy,
                                                  SPIRAS_Input,
                                                  indexOperands,
                                                  operandIdx,
                                                  pInputMeta,
                                                  nullptr,
                                                  nullptr,
                                                  interpLoc,
                                                  pSampleId,
                                                  pPixelOffset,
                                                  &callInst);

                m_interpCalls.insert(&callInst);
                callInst.replaceAllUsesWith(pLoadValue);
            }
            else
            {
                // The interpolant is an input
                LLPC_ASSERT(isa<GlobalVariable>(pLoadSrc));

                auto pInput = cast<GlobalVariable>(pLoadSrc);
                auto pInputTy = pInput->getType()->getContainedType(0);

                MDNode* pMetaNode = pInput->getMetadata(gSPIRVMD::InOut);
                LLPC_ASSERT(pMetaNode != nullptr);
                auto pInputMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

                auto pLoadValue = AddCallInstForInOutImport(pInputTy,
                                                            SPIRAS_Input,
                                                            pInputMeta,
                                                            nullptr,
                                                            nullptr,
                                                            nullptr,
                                                            interpLoc,
                                                            pSampleId,
                                                            pPixelOffset,
                                                            &callInst);

                m_interpCalls.insert(&callInst);
                callInst.replaceAllUsesWith(pLoadValue);
            }
        }
    }
}

// =====================================================================================================================
// Visits "load" instruction.
void SpirvLowerGlobal::visitLoadInst(
    LoadInst& loadInst) // [in] "Load" instruction
{
    Value* pLoadSrc = loadInst.getOperand(0);
    const uint32_t addrSpace = pLoadSrc->getType()->getPointerAddressSpace();

    // Skip if "load" instructions are not expected to be handled
    const bool isTcsInput  = ((m_shaderStage == ShaderStageTessControl) && (addrSpace == SPIRAS_Input));
    const bool isTcsOutput = ((m_shaderStage == ShaderStageTessControl) && (addrSpace == SPIRAS_Output));
    const bool isTesInput  = ((m_shaderStage == ShaderStageTessEval) && (addrSpace == SPIRAS_Input));

    if ((m_instVisitFlags.checkLoad == false) ||
        ((isTcsInput == false) && (isTcsOutput == false) && (isTesInput == false)))
    {
        return;
    }

    if (isa<GetElementPtrInst>(pLoadSrc))
    {
        GetElementPtrInst* pGetElemInst = cast<GetElementPtrInst>(pLoadSrc);

        std::vector<Value*> indexOperands;
        for (uint32_t i = 0, indexOperandCount = pGetElemInst->getNumIndices(); i < indexOperandCount; ++i)
        {
            indexOperands.push_back(ToInt32Value(m_pContext, pGetElemInst->getOperand(1 + i), &loadInst));
        }
        uint32_t operandIdx = 0;

        auto pInOut = cast<GlobalVariable>(pGetElemInst->getPointerOperand());
        auto pInOutTy = pInOut->getType()->getContainedType(0);

        MDNode* pMetaNode = pInOut->getMetadata(gSPIRVMD::InOut);
        LLPC_ASSERT(pMetaNode != nullptr);
        auto pInOutMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

        Value* pVertexIdx = nullptr;

        // If the input/output is arrayed, the outermost index might be used for vertex indexing
        if (pInOutTy->isArrayTy())
        {
            bool isVertexIdx = false;

            LLPC_ASSERT(pInOutMeta->getNumOperands() == 3);
            ShaderInOutMetadata inOutMeta = {};
            inOutMeta.U32All = cast<ConstantInt>(pInOutMeta->getOperand(1))->getZExtValue();

            if (inOutMeta.IsBuiltIn)
            {
                BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);
                isVertexIdx = ((builtInId == BuiltInPerVertex)    || // GLSL style per-vertex data
                               (builtInId == BuiltInPosition)     || // HLSL style per-vertex data
                               (builtInId == BuiltInPointSize)    ||
                               (builtInId == BuiltInClipDistance) ||
                               (builtInId == BuiltInCullDistance));
            }
            else
            {
                isVertexIdx = (inOutMeta.PerPatch == false);
            }

            if (isVertexIdx)
            {
                pInOutTy = pInOutTy->getArrayElementType();
                pVertexIdx = pGetElemInst->getOperand(2);
                ++operandIdx;

                pInOutMeta = cast<Constant>(pInOutMeta->getOperand(2));
            }
        }

        auto pLoadValue = LoadInOutMember(pInOutTy,
                                          addrSpace,
                                          indexOperands,
                                          operandIdx,
                                          pInOutMeta,
                                          nullptr,
                                          pVertexIdx,
                                          InterpLocUnknown,
                                          nullptr,
                                          nullptr,
                                          &loadInst);

        m_loadInsts.insert(&loadInst);
        loadInst.replaceAllUsesWith(pLoadValue);
    }
    else
    {
        LLPC_ASSERT(isa<GlobalVariable>(pLoadSrc));

        auto pInOut = cast<GlobalVariable>(pLoadSrc);
        auto pInOutTy = pInOut->getType()->getContainedType(0);

        MDNode* pMetaNode = pInOut->getMetadata(gSPIRVMD::InOut);
        LLPC_ASSERT(pMetaNode != nullptr);
        auto pInOutMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

        auto pLoadValue = AddCallInstForInOutImport(pInOutTy,
                                                    addrSpace,
                                                    pInOutMeta,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    InterpLocUnknown,
                                                    nullptr,
                                                    nullptr,
                                                    &loadInst);

        m_loadInsts.insert(&loadInst);
        loadInst.replaceAllUsesWith(pLoadValue);
    }
}

// =====================================================================================================================
// Visits "store" instruction.
void SpirvLowerGlobal::visitStoreInst(
    StoreInst& storeInst) // [in] "Store" instruction
{
    Value* pStoreValue = storeInst.getOperand(0);
    Value* pStoreDest  = storeInst.getOperand(1);

    const uint32_t addrSpace = pStoreDest->getType()->getPointerAddressSpace();

    // Skip if "store" instructions are not expected to be handled
    const bool isTcsOutput = ((m_shaderStage == ShaderStageTessControl) && (addrSpace == SPIRAS_Output));
    if ((m_instVisitFlags.checkStore == false) || (isTcsOutput == false))
    {
        return;
    }

    if (isa<GetElementPtrInst>(pStoreDest))
    {
        GetElementPtrInst* pGetElemInst = cast<GetElementPtrInst>(pStoreDest);

        std::vector<Value*> indexOperands;
        for (uint32_t i = 0, indexOperandCount = pGetElemInst->getNumIndices(); i < indexOperandCount; ++i)
        {
            indexOperands.push_back(ToInt32Value(m_pContext, pGetElemInst->getOperand(1 + i), &storeInst));
        }
        uint32_t operandIdx = 0;

        auto pOutput = cast<GlobalVariable>(pGetElemInst->getPointerOperand());
        auto pOutputTy = pOutput->getType()->getContainedType(0);

        MDNode* pMetaNode = pOutput->getMetadata(gSPIRVMD::InOut);
        LLPC_ASSERT(pMetaNode != nullptr);
        auto pOutputMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

        Value* pVertexIdx = nullptr;

        // If the output is arrayed, the outermost index might be used for vertex indexing
        if (pOutputTy->isArrayTy())
        {
            bool isVertexIdx = false;

            LLPC_ASSERT(pOutputMeta->getNumOperands() == 3);
            ShaderInOutMetadata outputMeta = {};
            outputMeta.U32All = cast<ConstantInt>(pOutputMeta->getOperand(1))->getZExtValue();

            if (outputMeta.IsBuiltIn)
            {
                BuiltIn builtInId = static_cast<BuiltIn>(outputMeta.Value);
                isVertexIdx = ((builtInId == BuiltInPerVertex)    || // GLSL style per-vertex data
                               (builtInId == BuiltInPosition)     || // HLSL style per-vertex data
                               (builtInId == BuiltInPointSize)    ||
                               (builtInId == BuiltInClipDistance) ||
                               (builtInId == BuiltInCullDistance));
            }
            else
            {
                isVertexIdx = (outputMeta.PerPatch == false);
            }

            if (isVertexIdx)
            {
                pOutputTy = pOutputTy->getArrayElementType();
                pVertexIdx = pGetElemInst->getOperand(2);
                ++operandIdx;

                pOutputMeta = cast<Constant>(pOutputMeta->getOperand(2));
            }
        }

        StoreOutputMember(pOutputTy,
                          pStoreValue,
                          indexOperands,
                          operandIdx,
                          pOutputMeta,
                          nullptr,
                          pVertexIdx,
                          &storeInst);

        m_storeInsts.insert(&storeInst);
    }
    else
    {
        LLPC_ASSERT(isa<GlobalVariable>(pStoreDest));

        auto pOutput = cast<GlobalVariable>(pStoreDest);
        auto pOutputy = pOutput->getType()->getContainedType(0);

        MDNode* pMetaNode = pOutput->getMetadata(gSPIRVMD::InOut);
        LLPC_ASSERT(pMetaNode != nullptr);
        auto pOutputMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

        AddCallInstForOutputExport(pStoreValue, pOutputMeta, nullptr, nullptr, nullptr, InvalidValue, &storeInst);

        m_storeInsts.insert(&storeInst);
    }

}

// =====================================================================================================================
// Maps the specified global variable to proxy variable.
void SpirvLowerGlobal::MapGlobalVariableToProxy(
    GlobalVariable* pGlobalVar) // [in] Global variable to be mapped
{
    const auto& dataLayout = m_pModule->getDataLayout();
    Type* pGlobalVarTy = pGlobalVar->getType()->getContainedType(0);
    Twine prefix = LlpcName::GlobalProxyPrefix;
    auto pInsertPos = m_pEntryPoint->begin()->getFirstInsertionPt();

    auto pProxy = new AllocaInst(pGlobalVarTy,
                                 dataLayout.getAllocaAddrSpace(),
                                 prefix + pGlobalVar->getName(),
                                 &*pInsertPos);

    if (pGlobalVar->hasInitializer())
    {
        auto pInitializer = pGlobalVar->getInitializer();
        new StoreInst(pInitializer, pProxy, &*pInsertPos);
    }

    m_globalVarProxyMap[pGlobalVar] = pProxy;
}

// =====================================================================================================================
// Maps the specified input to proxy variable.
void SpirvLowerGlobal::MapInputToProxy(
    GlobalVariable* pInput) // [in] Input to be mapped
{
    // NOTE: For tessellation shader, we do not map inputs to real proxy variables. Instead, we directly replace
    // "load" instructions with import calls in the lowering operation.
    if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
    {
        m_inputProxyMap[pInput] = nullptr;
        m_lowerInputInPlace = true;
        return;
    }

    const auto& dataLayout = m_pModule->getDataLayout();
    Type* pInputTy = pInput->getType()->getContainedType(0);
    Twine prefix = LlpcName::InputProxyPrefix;
    auto pInsertPos = m_pEntryPoint->begin()->getFirstInsertionPt();

    MDNode* pMetaNode  = pInput->getMetadata(gSPIRVMD::InOut);
    LLPC_ASSERT(pMetaNode != nullptr);

    auto pMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));
    auto pProxy = new AllocaInst(pInputTy,
                                 dataLayout.getAllocaAddrSpace(),
                                 prefix + pInput->getName(),
                                 &*pInsertPos);

    // Import input to proxy variable
    auto pInputValue = AddCallInstForInOutImport(pInputTy,
                                                 SPIRAS_Input,
                                                 pMeta,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 InterpLocUnknown,
                                                 nullptr,
                                                 nullptr,
                                                 &*pInsertPos);
    new StoreInst(pInputValue, pProxy, &*pInsertPos);

    m_inputProxyMap[pInput] = pProxy;
}

// =====================================================================================================================
// Maps the specified output to proxy variable.
void SpirvLowerGlobal::MapOutputToProxy(
    GlobalVariable* pOutput) // [in] Output to be mapped
{
    // NOTE: For tessellation control shader, we do not map outputs to real proxy variables. Instead, we directly
    // replace "store" instructions with export calls in the lowering operation.
    if (m_shaderStage == ShaderStageTessControl)
    {
        m_outputProxyMap.push_back(std::pair<Value*, Value*>(pOutput, nullptr));
        m_lowerOutputInPlace = true;
        return;
    }

    const auto& dataLayout = m_pModule->getDataLayout();
    Type* pOutputTy = pOutput->getType()->getContainedType(0);
    Twine prefix = LlpcName::OutputProxyPrefix;
    auto pInsertPos = m_pEntryPoint->begin()->getFirstInsertionPt();

    auto pProxy = new AllocaInst(pOutputTy,
                                 dataLayout.getAllocaAddrSpace(),
                                 prefix + pOutput->getName(),
                                 &*pInsertPos);

    if (pOutput->hasInitializer())
    {
        auto pInitializer = pOutput->getInitializer();
        new StoreInst(pInitializer, pProxy, &*pInsertPos);
    }

    m_outputProxyMap.push_back(std::pair<Value*, Value*>(pOutput, pProxy));
}

// =====================================================================================================================
// Removes those constant expressions that reference global variables.
void SpirvLowerGlobal::RemoveConstantExpr()
{
    std::unordered_map<ConstantExpr*, Instruction*> constantExprMap;

    // Collect contextant expressions and translate them to regular instructions
    for (auto globalVarMap : m_globalVarProxyMap)
    {
        auto pGlobalVar = cast<GlobalVariable>(globalVarMap.first);
        auto pProxy = cast<Instruction>(globalVarMap.second);

        std::vector<Instruction*> insts;
        for (auto pUser : pGlobalVar->users())
        {
            auto* pConstExpr = dyn_cast<ConstantExpr>(pUser);
            if (pConstExpr != nullptr)
            {
                // Map this constant expression to normal instruction if it has not been visited
                if (constantExprMap.find(pConstExpr) == constantExprMap.end())
                {
                    if (pConstExpr->user_empty() == false)
                    {
                        auto pInst = pConstExpr->getAsInstruction();
                        insts.push_back(pInst);
                        constantExprMap[pConstExpr] = pInst;
                    }
                    else
                    {
                        // NOTE: Some constant expressions do not have users acutally, we should exclude them from
                        // our handling.
                        constantExprMap[pConstExpr] = nullptr;
                    }
                }
            }
        }

        for (auto pInst : insts)
        {
            pInst->insertAfter(pProxy);
        }
    }

    auto pInsertPos = &*(m_pEntryPoint->begin()->getFirstInsertionPt());

    for (auto inputMap : m_inputProxyMap)
    {
        auto pInput = cast<GlobalVariable>(inputMap.first);
        auto pProxy = inputMap.second;
        if (pProxy != nullptr)
        {
            pInsertPos = cast<Instruction>(pProxy);
        }

        std::vector<Instruction*> insts;
        for (auto pUser : pInput->users())
        {
            // NOTE: Some constant expressions do not have users acutally, we should exclude them from
            // our handling.
            auto pConstExpr = dyn_cast<ConstantExpr>(pUser);
            if (pConstExpr != nullptr)
            {
                // Map this constant expression to normal instruction if it has not been visited
                if (constantExprMap.find(pConstExpr) == constantExprMap.end())
                {
                    if (pConstExpr->user_empty() == false)
                    {
                        auto pInst = pConstExpr->getAsInstruction();
                        insts.push_back(pInst);
                        constantExprMap[pConstExpr] = pInst;
                    }
                    else
                    {
                        // NOTE: Some constant expressions do not have users acutally, we should exclude them from
                        // our handling.
                        constantExprMap[pConstExpr] = nullptr;
                    }
                }
            }
        }

        for (auto pInst : insts)
        {
            pInst->insertAfter(pInsertPos);
        }
    }

    for (auto outputMap : m_outputProxyMap)
    {
        auto pOutput = cast<GlobalVariable>(outputMap.first);
        auto pProxy = outputMap.second;
        if (pProxy != nullptr)
        {
            pInsertPos = cast<Instruction>(pProxy);
        }

        std::vector<Instruction*> insts;
        for (auto pUser : pOutput->users())
        {
            auto pConstExpr = dyn_cast<ConstantExpr>(pUser);
            if (pConstExpr != nullptr)
            {
                // Map this constant expression to normal instruction if it has not been visited
                if (constantExprMap.find(pConstExpr) == constantExprMap.end())
                {
                    if (pConstExpr->user_empty() == false)
                    {
                        auto pInst = pConstExpr->getAsInstruction();
                        insts.push_back(pInst);
                        constantExprMap[pConstExpr] = pInst;
                    }
                    else
                    {
                        // NOTE: Some constant expressions do not have users acutally, we should exclude them from
                        // our handling.
                        constantExprMap[pConstExpr] = nullptr;
                    }
                }
            }
        }

        for (auto pInst : insts)
        {
            pInst->insertAfter(pInsertPos);
        }
    }

    if (constantExprMap.empty() == false)
    {
        // Replace constant expressions with the mapped normal instructions

        // NOTE: It seems user list of constant expression is incorrect. Here, we traverse all instructions
        // in the entry-point and do replacement.
        for (auto pBlock = m_pEntryPoint->begin(), pBlockEnd =  m_pEntryPoint->end(); pBlock != pBlockEnd; ++pBlock)
        {
            for (auto pInst = pBlock->begin(), pInstEnd =  pBlock->end(); pInst != pInstEnd; ++pInst)
            {
                for (auto pOperand = pInst->op_begin(), pOperandEnd = pInst->op_end();
                     pOperand != pOperandEnd;
                     ++pOperand)
                {
                    if (isa<ConstantExpr>(pOperand))
                    {
                        auto pConstExpr = cast<ConstantExpr>(pOperand);
                        if (constantExprMap.find(pConstExpr) != constantExprMap.end())
                        {
                            LLPC_ASSERT(constantExprMap[pConstExpr] != nullptr);
                            pInst->replaceUsesOfWith(pConstExpr, constantExprMap[pConstExpr]);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Remove constant expressions
    for (auto& constExprMap : constantExprMap)
    {
        auto pConstExpr = constExprMap.first;
        pConstExpr->removeDeadConstantUsers();
        pConstExpr->dropAllReferences();
    }
}

// =====================================================================================================================
// Does lowering opertions for SPIR-V global variables, replaces global variables with proxy variables.
void SpirvLowerGlobal::LowerGlobalVar()
{
    if (m_globalVarProxyMap.empty())
    {
        // Skip lowering if there is no global variable
        return;
    }

    // Replace global variable with proxy variable
    for (auto globalVarMap : m_globalVarProxyMap)
    {
        auto pGlobalVar = cast<GlobalVariable>(globalVarMap.first);
        auto pProxy = globalVarMap.second;
        pGlobalVar->mutateType(pProxy->getType()); // To clear address space for pointer to make replacement valid
        pGlobalVar->replaceAllUsesWith(pProxy);
        pGlobalVar->dropAllReferences();
        pGlobalVar->eraseFromParent();
    }
}

// =====================================================================================================================
// Does lowering opertions for SPIR-V inputs, replaces inputs with proxy variables.
void SpirvLowerGlobal::LowerInput()
{
    if (m_inputProxyMap.empty())
    {
        // Skip lowering if there is no input
        return;
    }

    // NOTE: For tessellation shader, we invoke handling of "load"/"store" instructions and replace all those
    // instructions with import/export calls in-place.
    LLPC_ASSERT((m_shaderStage != ShaderStageTessControl) && (m_shaderStage != ShaderStageTessEval));

    // NOTE: For fragment shader, we have to handle interpolation functions first since input interpolants must be
    // lowered in-place.
    if (m_shaderStage == ShaderStageFragment)
    {
        // Invoke handling of interpolation calls
        m_instVisitFlags.u32All = 0;
        m_instVisitFlags.checkInterpCall = true;
        visit(m_pModule);

        // Remove interpolation calls, they must have been replaced with LLPC intrinsics
        std::unordered_set<GetElementPtrInst*> getElemInsts;
        for (auto pInterpCall : m_interpCalls)
        {
            GetElementPtrInst* pGetElemInst = dyn_cast<GetElementPtrInst>(pInterpCall->getArgOperand(0));
            if (pGetElemInst != nullptr)
            {
                getElemInsts.insert(pGetElemInst);
            }

            LLPC_ASSERT(pInterpCall->use_empty());
            pInterpCall->dropAllReferences();
            pInterpCall->eraseFromParent();
        }

        for (auto pGetElemInst : getElemInsts)
        {
            if (pGetElemInst->use_empty())
            {
                pGetElemInst->dropAllReferences();
                pGetElemInst->eraseFromParent();
            }
        }
    }

    for (auto inputMap : m_inputProxyMap)
    {
        auto pInput = cast<GlobalVariable>(inputMap.first);

        for (auto pUser = pInput->user_begin(), pEnd = pInput->user_end(); pUser != pEnd; ++pUser)
        {
            // NOTE: "Getelementptr" and "bitcast" will propogate the address space of pointer value (input variable)
            // to the element pointer value (destination). We have to clear the address space of this element pointer
            // value. The original pointer value has been lowered and therefore the address space is invalid now.
            Instruction* pInst = dyn_cast<Instruction>(*pUser);
            if (pInst != nullptr)
            {
                Type* pInstTy = pInst->getType();
                if (isa<PointerType>(pInstTy) && (pInstTy->getPointerAddressSpace() == SPIRAS_Input))
                {
                    LLPC_ASSERT(isa<GetElementPtrInst>(pInst) || isa<BitCastInst>(pInst));
                    Type* pNewInstTy = PointerType::get(pInstTy->getContainedType(0), SPIRAS_Private);
                    pInst->mutateType(pNewInstTy);
                }
            }
        }

        auto pProxy = inputMap.second;
        pInput->mutateType(pProxy->getType()); // To clear address space for pointer to make replacement valid
        pInput->replaceAllUsesWith(pProxy);
        pInput->eraseFromParent();
    }
}

// =====================================================================================================================
// Does lowering opertions for SPIR-V outputs, replaces outputs with proxy variables.
void SpirvLowerGlobal::LowerOutput()
{
    // NOTE: For tessellation control shader, we invoke handling of "load"/"store" instructions and replace all those
    // instructions with import/export calls in-place.
    LLPC_ASSERT(m_shaderStage != ShaderStageTessControl);

    m_pRetBlock = BasicBlock::Create(*m_pContext, "", m_pEntryPoint);

    // Invoke handling of "return" instructions or "emit" calls
    m_instVisitFlags.u32All = 0;
    if (m_shaderStage == ShaderStageGeometry)
    {
        m_instVisitFlags.checkEmitCall = true;
    }
    else
    {
        m_instVisitFlags.checkReturn = true;
    }
    visit(m_pModule);

    auto pRetInst = ReturnInst::Create(*m_pContext, m_pRetBlock);

    for (auto retInst : m_retInsts)
    {
        retInst->dropAllReferences();
        retInst->eraseFromParent();
    }

    // Export output from the proxy variable prior to "return" instruction or "emit" calls
    for (auto outputMap : m_outputProxyMap)
    {
        auto pOutput = cast<GlobalVariable>(outputMap.first);
        auto pProxy  = outputMap.second;

        MDNode* pMetaNode = pOutput->getMetadata(gSPIRVMD::InOut);
        LLPC_ASSERT(pMetaNode != nullptr);

        auto pMeta = mdconst::dyn_extract<Constant>(pMetaNode->getOperand(0));

        if ((m_shaderStage == ShaderStageVertex) || (m_shaderStage == ShaderStageTessEval) ||
            (m_shaderStage == ShaderStageFragment))
        {
            Value* pOutputValue = new LoadInst(pProxy, "", pRetInst);
            AddCallInstForOutputExport(pOutputValue, pMeta, nullptr, nullptr, nullptr, InvalidValue, pRetInst);
        }
        else if (m_shaderStage == ShaderStageGeometry)
        {
            for (auto pEmitCall : m_emitCalls)
            {
                uint32_t emitStreamId = 0;

                auto mangledName = pEmitCall->getCalledFunction()->getName();
                if (mangledName.startswith("_Z16EmitStreamVertex"))
                {
                    emitStreamId = cast<ConstantInt>(pEmitCall->getOperand(0))->getZExtValue();
                }
                else
                {
                    LLPC_ASSERT(mangledName.startswith("_Z10EmitVertex"));
                }

                // TODO: Multiple output streams are not supported.
                if (emitStreamId != 0)
                {
                    continue;
                }

                Value* pOutputValue = new LoadInst(pProxy, "", pEmitCall);
                AddCallInstForOutputExport(pOutputValue, pMeta, nullptr, nullptr, nullptr, emitStreamId, pEmitCall);
            }
        }
    }

    for (auto outputMap : m_outputProxyMap)
    {
        auto pOutput = cast<GlobalVariable>(outputMap.first);

        for (auto pUser = pOutput->user_begin(), pEnd = pOutput->user_end(); pUser != pEnd; ++pUser)
        {
            // NOTE: "Getelementptr" and "bitCast" will propogate the address space of pointer value (output variable)
            // to the element pointer value (destination). We have to clear the address space of this element pointer
            // value. The original pointer value has been lowered and therefore the address space is invalid now.
            Instruction* pInst = dyn_cast<Instruction>(*pUser);
            if (pInst != nullptr)
            {
                Type* pInstTy = pInst->getType();
                if (isa<PointerType>(pInstTy) && (pInstTy->getPointerAddressSpace() == SPIRAS_Output))
                {
                    LLPC_ASSERT(isa<GetElementPtrInst>(pInst) || isa<BitCastInst>(pInst));
                    Type* pNewInstTy = PointerType::get(pInstTy->getContainedType(0), SPIRAS_Private);
                    pInst->mutateType(pNewInstTy);
                }
            }
        }

        auto pProxy = outputMap.second;
        pOutput->mutateType(pProxy->getType()); // To clear address space for pointer to make replacement valid
        pOutput->replaceAllUsesWith(pProxy);
        pOutput->eraseFromParent();
    }
}

// =====================================================================================================================
// Does inplace lowering opertions for SPIR-V inputs/outputs, replaces "load" instructions with import calls and
// "store" instructions with export calls.
void SpirvLowerGlobal::LowerInOutInPlace()
{
    LLPC_ASSERT((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval));

    // Invoke handling of "load" and "store" instruction
    m_instVisitFlags.u32All = 0;
    m_instVisitFlags.checkLoad = true;
    if (m_shaderStage == ShaderStageTessControl)
    {
        m_instVisitFlags.checkStore = true;
    }
    visit(m_pModule);

    std::unordered_set<GetElementPtrInst*> getElemInsts;

    // Remove unnecessary "load" instructions
    for (auto pLoadInst : m_loadInsts)
    {
        GetElementPtrInst* pGetElemInst = dyn_cast<GetElementPtrInst>(pLoadInst->getOperand(0)); // Load source
        if (pGetElemInst != nullptr)
        {
            getElemInsts.insert(pGetElemInst);
        }

        LLPC_ASSERT(pLoadInst->use_empty());
        pLoadInst->dropAllReferences();
        pLoadInst->eraseFromParent();
    }

    // Remove unnecessary "getelementptr" instructions which are referenced by "load" instructions only
    for (auto pGetElemInst : getElemInsts)
    {
        if (pGetElemInst->use_empty())
        {
            pGetElemInst->dropAllReferences();
            pGetElemInst->eraseFromParent();
        }
    }

    m_loadInsts.clear();
    getElemInsts.clear();

    // Remove unnecessary "store" instructions
    for (auto pStoreInst : m_storeInsts)
    {
        GetElementPtrInst* pGetElemInst = dyn_cast<GetElementPtrInst>(pStoreInst->getOperand(1)); // Store destination
        if (pGetElemInst != nullptr)
        {
            getElemInsts.insert(pGetElemInst);
        }

        LLPC_ASSERT(pStoreInst->use_empty());
        pStoreInst->dropAllReferences();
        pStoreInst->eraseFromParent();
    }

    // Remove unnecessary "getelementptr" instructions which are referenced by "store" instructions only
    for (auto pGetElemInst : getElemInsts)
    {
        if (pGetElemInst->use_empty())
        {
            pGetElemInst->dropAllReferences();
            pGetElemInst->eraseFromParent();
        }
    }

    m_storeInsts.clear();
    getElemInsts.clear();

    // Remove inputs if they are lowered in-place
    if (m_lowerInputInPlace)
    {
        for (auto inputMap : m_inputProxyMap)
        {
            auto pInput = cast<GlobalVariable>(inputMap.first);
            LLPC_ASSERT(pInput->use_empty());
            pInput->eraseFromParent();
        }
    }

    // Remove outputs if they are lowered in-place
    if (m_lowerOutputInPlace)
    {
        for (auto outputMap : m_outputProxyMap)
        {
            auto pOutput = cast<GlobalVariable>(outputMap.first);
            LLPC_ASSERT(pOutput->use_empty());
            pOutput->eraseFromParent();
        }
    }
}

// =====================================================================================================================
// Inserts LLVM call instruction to import input/output.
Value* SpirvLowerGlobal::AddCallInstForInOutImport(
    Type*        pInOutTy,     // [in] Type of value imported from input/output
    uint32_t     addrSpace,    // Address space
    Constant*    pInOutMeta,   // [in] Metadata of this input/output
    Value*       pLocOffset,   // [in] Relative location offset, passed from aggregate type
    Value*       pElemIdx,     // [in] Element index used for element indexing, valid for tessellation shader (usually,
                               // it is vector component index, for built-in input/output, it could be element index
                               // of scalar array)
    Value*       pVertexIdx,   // [in] Input array outermost index used for vertex indexing, valid for tessellation
                               // shader and geometry shader
    uint32_t     interpLoc,    // Interpolation location, valid for fragment shader
                               // (use "InterpLocUnknown" as don't-care value)
    Value*       pSampleId,    // [in] Sample ID, used when the interpolation location is "InterpLocSample", valid for
                               // fragment shader
    Value*       pPixelOffset, // [in] Offset from the center of the pixel, used when interpolation location is
                               // "InterpLocCenter", valid for fragment shader
    Instruction* pInsertPos)   // [in] Where to insert this call
{
    LLPC_ASSERT((addrSpace == SPIRAS_Input) ||
                ((addrSpace == SPIRAS_Output) && (m_shaderStage == ShaderStageTessControl)));

    Value* pInOutValue = UndefValue::get(pInOutTy);

    ShaderInOutMetadata inOutMeta = {};

    if (pInOutTy->isArrayTy())
    {
        // Array type
        LLPC_ASSERT(pElemIdx == nullptr);

        LLPC_ASSERT(pInOutMeta->getNumOperands() == 3);
        uint32_t stride = cast<ConstantInt>(pInOutMeta->getOperand(0))->getZExtValue();
        inOutMeta.U32All = cast<ConstantInt>(pInOutMeta->getOperand(1))->getZExtValue();

        if (inOutMeta.IsBuiltIn)
        {
            LLPC_ASSERT(pLocOffset == nullptr);

            BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);

            if ((pVertexIdx == nullptr) && (m_shaderStage == ShaderStageGeometry) &&
                ((builtInId == BuiltInPerVertex)    || // GLSL style per-vertex data
                 (builtInId == BuiltInPosition)     || // HLSL style per-vertex data
                 (builtInId == BuiltInPointSize)    ||
                 (builtInId == BuiltInClipDistance) ||
                 (builtInId == BuiltInCullDistance)))
            {
                // NOTE: We are handling vertex indexing of built-in inputs of geometry shader. For tessellation
                // shader, vertex indexing is handled by "load"/"store" instruction lowering.
                LLPC_ASSERT(pVertexIdx == nullptr); // For per-vertex data, make a serial of per-vertex import calls.

                LLPC_ASSERT((m_shaderStage == ShaderStageGeometry) ||
                            (m_shaderStage == ShaderStageTessControl) ||
                            (m_shaderStage == ShaderStageTessEval));

                auto pElemMeta = cast<Constant>(pInOutMeta->getOperand(2));
                auto pElemTy   = pInOutTy->getArrayElementType();

                const uint64_t elemCount = pInOutTy->getArrayNumElements();
                for (uint32_t elemIdx = 0; elemIdx < elemCount; ++elemIdx)
                {
                    // Handle array elements recursively
                    pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), elemIdx);
                    auto pElem = AddCallInstForInOutImport(pElemTy,
                                                           addrSpace,
                                                           pElemMeta,
                                                           nullptr,
                                                           nullptr,
                                                           pVertexIdx,
                                                           interpLoc,
                                                           pSampleId,
                                                           pPixelOffset,
                                                           pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(elemIdx);
                    pInOutValue = InsertValueInst::Create(pInOutValue, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                // Normal built-ins without vertex indexing
                std::string builtInName = getNameMap(builtInId).map(builtInId);
                LLPC_ASSERT(builtInName.find("BuiltIn")  == 0);
                std::string instName =
                    (addrSpace == SPIRAS_Input) ? LlpcName::InputImportBuiltIn : LlpcName::OutputImportBuiltIn;
                instName += builtInName.substr(strlen("BuiltIn"));

                std::vector<Value*> args;
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), builtInId));

                if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
                {
                    // NOTE: For tessellation shader, we add element index as an addition parameter to do addressing for
                    // the input/output. Here, this is invalid value.
                    pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
                    args.push_back(pElemIdx);
                }

                if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval) ||
                    (m_shaderStage == ShaderStageGeometry))
                {
                    // NOTE: For gl_in[i].XXX/gl_out[i].XXX, we add vertex indexing as an additional parameter to do
                    // addressing for the input/output.
                    if (pVertexIdx == nullptr)
                    {
                        // When vertex indexing is not specified, we set it to don't-care value
                        pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
                    }
                    args.push_back(pVertexIdx);
                }
                else
                {
                    // Vertex indexing is not valid for other shader stages
                    LLPC_ASSERT(pVertexIdx == nullptr);
                }

                pInOutValue = EmitCall(m_pModule, instName, pInOutTy, args, NoAttrib, pInsertPos);
            }
        }
        else
        {
            auto pElemMeta = cast<Constant>(pInOutMeta->getOperand(2));
            auto pElemTy   = pInOutTy->getArrayElementType();

            const uint64_t elemCount = pInOutTy->getArrayNumElements();

            if ((pVertexIdx == nullptr) && (m_shaderStage == ShaderStageGeometry))
            {
                // NOTE: We are handling vertex indexing of generic inputs of geometry shader. For tessellation shader,
                // vertex indexing is handled by "load"/"store" instruction lowering.
                for (uint32_t elemIdx = 0; elemIdx < elemCount; ++elemIdx)
                {
                    pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), elemIdx);
                    auto pElem = AddCallInstForInOutImport(pElemTy,
                                                           addrSpace,
                                                           pElemMeta,
                                                           pLocOffset,
                                                           nullptr,
                                                           pVertexIdx,
                                                           InterpLocUnknown,
                                                           nullptr,
                                                           nullptr,
                                                           pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(elemIdx);
                    pInOutValue = InsertValueInst::Create(pInOutValue, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                // NOTE: If the relative location offset is not specified, initialize it to 0.
                if (pLocOffset == nullptr)
                {
                    pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
                }

                for (uint32_t elemIdx = 0; elemIdx < elemCount; ++elemIdx)
                {
                    // Handle array elements recursively

                    // elemLocOffset = locOffset + stride * elemIdx
                    Value* pElemLocOffset = BinaryOperator::CreateMul(ConstantInt::get(m_pContext->Int32Ty(), stride),
                                                                      ConstantInt::get(m_pContext->Int32Ty(), elemIdx),
                                                                      "",
                                                                      pInsertPos);
                    pElemLocOffset = BinaryOperator::CreateAdd(pLocOffset, pElemLocOffset, "", pInsertPos);

                    auto pElem = AddCallInstForInOutImport(pElemTy,
                                                           addrSpace,
                                                           pElemMeta,
                                                           pElemLocOffset,
                                                           pElemIdx,
                                                           pVertexIdx,
                                                           InterpLocUnknown,
                                                           nullptr,
                                                           nullptr,
                                                           pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(elemIdx);
                    pInOutValue = InsertValueInst::Create(pInOutValue, pElem, idxs, "", pInsertPos);
                }
            }
        }
    }
    else if (pInOutTy->isStructTy())
    {
        // Structure type
        LLPC_ASSERT(pElemIdx == nullptr);

        const uint64_t memberCount = pInOutTy->getStructNumElements();
        for (uint32_t memberIdx = 0; memberIdx < memberCount; ++memberIdx)
        {
            // Handle structure member recursively
            auto pMemberTy = pInOutTy->getStructElementType(memberIdx);
            auto pMemberMeta = cast<Constant>(pInOutMeta->getOperand(memberIdx));

            auto pMember = AddCallInstForInOutImport(pMemberTy,
                                                     addrSpace,
                                                     pMemberMeta,
                                                     pLocOffset,
                                                     nullptr,
                                                     pVertexIdx,
                                                     InterpLocUnknown,
                                                     nullptr,
                                                     nullptr,
                                                     pInsertPos);

            std::vector<uint32_t> idxs;
            idxs.push_back(memberIdx);
            pInOutValue = InsertValueInst::Create(pInOutValue, pMember, idxs, "", pInsertPos);
        }
    }
    else
    {
        std::vector<Value*> args;

        inOutMeta.U32All = cast<ConstantInt>(pInOutMeta)->getZExtValue();

        LLPC_ASSERT(inOutMeta.IsLoc || inOutMeta.IsBuiltIn);

        std::string instName;
        Value* pIJ = nullptr;

        if (interpLoc != InterpLocUnknown)
        {
            LLPC_ASSERT(m_shaderStage == ShaderStageFragment);

            // Add intrinsic to calculate I/J for interpolation function
            std::string evalInstName;
            std::vector<Value*> evalArgs;
            auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment);

            if (interpLoc == InterpLocCentroid)
            {
                evalInstName = LlpcName::InputImportBuiltIn;
                if (inOutMeta.InterpMode == InterpModeNoPersp)
                {
                    evalInstName += "InterpLinearCentroid";
                    evalArgs.push_back(ConstantInt::get(m_pContext->Int32Ty(), BuiltInInterpLinearCentroid));
                    pResUsage->builtInUsage.fs.noperspective = true;
                    pResUsage->builtInUsage.fs.centroid = true;
                }
                else
                {
                    evalInstName += "InterpPerspCentroid";
                    evalArgs.push_back(ConstantInt::get(m_pContext->Int32Ty(), BuiltInInterpPerspCentroid));
                    pResUsage->builtInUsage.fs.smooth = true;
                    pResUsage->builtInUsage.fs.centroid = true;
                }
            }
            else
            {
                evalInstName = LlpcName::InputInterpEval;
                if (interpLoc == InterpLocCenter)
                {
                    evalInstName += "offset";
                    evalArgs.push_back(pPixelOffset);
                }
                else
                {
                    evalInstName += "sample";
                    evalArgs.push_back(pSampleId);
                    pResUsage->builtInUsage.fs.runAtSampleRate = true;
                }

                if (inOutMeta.InterpMode == InterpModeNoPersp)
                {
                    evalInstName += ".noperspective";
                    pResUsage->builtInUsage.fs.noperspective = true;
                    pResUsage->builtInUsage.fs.center = true;
                }
                else
                {
                    pResUsage->builtInUsage.fs.smooth = true;
                    pResUsage->builtInUsage.fs.pullMode = true;
                }
            }

            pIJ = EmitCall(m_pModule, evalInstName, m_pContext->Floatx2Ty(), evalArgs, NoAttrib, pInsertPos);

            // Prepare arguments for input import call
            instName =  LlpcName::InputImportInterpolant;
            instName += GetTypeNameForScalarOrVector(pInOutTy);

            auto pLoc = ConstantInt::get(m_pContext->Int32Ty(), inOutMeta.Value);

            // NOTE: If the relative location offset is not specified, initialize it to 0.
            if (pLocOffset == nullptr)
            {
                pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
            }

            args.push_back(pLoc);
            args.push_back(pLocOffset);
        }
        else if (inOutMeta.IsBuiltIn)
        {
            instName = (addrSpace == SPIRAS_Input) ? LlpcName::InputImportBuiltIn : LlpcName::OutputImportBuiltIn;

            BuiltIn builtInId = static_cast<BuiltIn>(inOutMeta.Value);
            std::string builtInName = getNameMap(builtInId).map(builtInId);

            LLPC_ASSERT(builtInName.find("BuiltIn") == 0);
            instName += builtInName.substr(strlen("BuiltIn"));
            if (pElemIdx != nullptr)
            {
                // Add this suffix when element indexing is specified for built-in import
                instName += "." + GetTypeNameForScalarOrVector(pInOutTy);
            }

            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), builtInId));
        }
        else
        {
            instName = (addrSpace == SPIRAS_Input) ? LlpcName::InputImportGeneric : LlpcName::OutputImportGeneric;
            instName += GetTypeNameForScalarOrVector(pInOutTy);

            auto pLoc = ConstantInt::get(m_pContext->Int32Ty(), inOutMeta.Value);

            // NOTE: If the relative location offset is not specified, initialize it to 0.
            if (pLocOffset == nullptr)
            {
                pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
            }

            if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
            {
                // NOTE: For tessellation shader, we treats the location to two parts:
                // startLoc = loc + locOffset
                args.push_back(pLoc);
                args.push_back(pLocOffset);
            }
            else
            {
                auto pStartLoc = BinaryOperator::CreateAdd(pLoc, pLocOffset, "", pInsertPos);
                args.push_back(pStartLoc);
            }
        }

        if ((m_shaderStage == ShaderStageTessControl) ||
            (m_shaderStage == ShaderStageTessEval) ||
            (interpLoc != InterpLocUnknown))
        {
            // NOTE: For tessellation shader and fragment shader with interpolation functions, we add element indexing
            // as an addition parameter to do addressing for the input/output.
            if (pElemIdx == nullptr)
            {
                // When element indexing is not specified, we set it to don't-care value
                pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
            }
            args.push_back(pElemIdx);
        }
        else
        {
            // Element indexing is not valid for other shader stages
            LLPC_ASSERT(pElemIdx == nullptr);
        }

        if ((m_shaderStage == ShaderStageTessControl) ||
            (m_shaderStage == ShaderStageTessEval) ||
            (m_shaderStage == ShaderStageGeometry))
        {
            // NOTE: For tessellation shader and geometry shader, we add vertex indexing as an addition parameter to
            // do addressing for the input/output.
            if (pVertexIdx == nullptr)
            {
                // When vertex indexing is not specified, we set it to don't-care value
                pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
            }
            args.push_back(pVertexIdx);
        }
        else
        {
            // Vertex indexing is not valid for other shader stages
            LLPC_ASSERT(pVertexIdx == nullptr);
        }

        if (interpLoc != InterpLocUnknown)
        {
            // Add interpolation mode and calculated I/J for interpolant inputs of fragment shader
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), inOutMeta.InterpMode));
            args.push_back(pIJ);
        }
        else if ((m_shaderStage == ShaderStageFragment) && (inOutMeta.IsBuiltIn == false))
        {
            // Add interpolation mode and location for generic inputs of fragment shader
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), inOutMeta.InterpMode));
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), inOutMeta.InterpLoc));
        }

        //
        // VS:  @llpc.input.import.generic.%Type%(i32 location)
        //      @llpc.input.import.builtin.%BuiltIn%(i32 builtInId)
        //
        // TCS: @llpc.input.import.generic.%Type%(i32 location, i32 locOffset, i32 elemIdx, i32 vertexIdx)
        //      @llpc.input.import.builtin.%BuiltIn%.%Type%(i32 builtInId, i32 elemIdx, i32 vertexIdx)
        //
        //      @llpc.output.import.generic.%Type%(i32 location, i32 locOffset, i32 elemIdx, i32 vertexIdx)
        //      @llpc.output.import.builtin.%BuiltIn%.%Type%(i32 builtInId, i32 elemIdx, i32 vertexIdx)
        //
        //
        // TES: @llpc.input.import.generic.%Type%(i32 location, i32 locOffset, i32 elemIdx, i32 vertexIdx)
        //      @llpc.input.import.builtin.%BuiltIn%.%Type%(i32 builtInId, i32 elemIdx, i32 vertexIdx)

        // GS:  @llpc.input.import.generic.%Type%(i32 location, i32 vertexIdx)
        //      @llpc.input.import.builtin.%BuiltIn%(i32 builtInId, i32 vertexIdx)
        //
        // FS:  @llpc.input.import.generic.%Type%(i32 location, i32 interpMode, i32 interpLoc)
        //      @llpc.input.import.builtin.%BuiltIn%(i32 builtInId)
        //      @llpc.input.import.interpolant.%Type%(i32 location, i32 locOffset, i32 elemIdx, i32 interpMode, <2 x float> ij)
        //
        // CS:  @llpc.input.import.builtin.%BuiltIn%(i32 builtInId)
        //
        pInOutValue = EmitCall(m_pModule, instName, pInOutTy, args, NoAttrib, pInsertPos);
    }

    return pInOutValue;
}

// =====================================================================================================================
// Inserts LLVM call instruction to export output.
void SpirvLowerGlobal::AddCallInstForOutputExport(
    Value*       pOutputValue, // [in] Value exported to output
    Constant*    pOutputMeta,  // [in] Metadata of this output
    Value*       pLocOffset,   // [in] Relative location offset, passed from aggregate type
    Value*       pElemIdx,     // [in] Element index used for element indexing, valid for tessellation control shader
                               // (usually, it is vector component index, for built-in input/output, it could be
                               // element index of scalar array)
    Value*       pVertexIdx,   // [in] Output array outermost index used for vertex indexing, valid for tessellation
                               // control shader
    uint32_t     emitStreamId, // ID of emitted vertex stream, valid for geometry shader (0xFFFFFFFF for other stages)
    Instruction* pInsertPos)   // [in] Where to insert this call
{
    Type* pOutputTy = pOutputValue->getType();

    ShaderInOutMetadata outputMeta = {};

    if (pOutputTy->isArrayTy())
    {
        // Array type
        LLPC_ASSERT(pElemIdx == nullptr);

        LLPC_ASSERT(pOutputMeta->getNumOperands() == 3);
        uint32_t stride = cast<ConstantInt>(pOutputMeta->getOperand(0))->getZExtValue();
        outputMeta.U32All = cast<ConstantInt>(pOutputMeta->getOperand(1))->getZExtValue();

        if ((m_shaderStage == ShaderStageGeometry) && (emitStreamId != outputMeta.StreamId))
        {
            // NOTE: For geometry shader, if the output is not bound to this vertex stream, we skip processing.
            return;
        }

        if (outputMeta.IsBuiltIn)
        {
            BuiltIn builtInId = static_cast<BuiltIn>(outputMeta.Value);

            // NOTE: For tessellation shader, vertex indexing is handled by "load"/"store" instruction lowering.
            std::string instName = LlpcName::OutputExportBuiltIn;
            std::string builtInName = getNameMap(builtInId).map(builtInId);

            LLPC_ASSERT(builtInName.find("BuiltIn")  == 0);
            instName += builtInName.substr(strlen("BuiltIn"));

            std::vector<Value*> args;
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), builtInId));

            if (m_shaderStage == ShaderStageTessControl)
            {
                // NOTE: For tessellation control shader, we add element index as an addition parameter to do
                // addressing for the output. Here, this is invalid value.
                pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
                args.push_back(pElemIdx);

                // NOTE: For gl_out[i].XXX, we add vertex indexing as an additional parameter to do addressing
                // for the output.
                if (pVertexIdx == nullptr)
                {
                    // When vertex indexing is not specified, we set it to don't-care value
                    pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
                }
                args.push_back(pVertexIdx);
            }
            else
            {
                // Vertex indexing is not valid for other shader stages
                LLPC_ASSERT(pVertexIdx == nullptr);
            }

            if (m_shaderStage == ShaderStageGeometry)
            {
                // NOTE: For geometry shader, we add stream ID for outputs.
                LLPC_ASSERT(emitStreamId == outputMeta.StreamId);
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), emitStreamId));
            }
            else
            {
                // ID of emitted vertex stream is not valid for other shader stages
                LLPC_ASSERT(emitStreamId == InvalidValue);
            }

            args.push_back(pOutputValue);

            EmitCall(m_pModule, instName, m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        }
        else
        {
            // NOTE: If the relative location offset is not specified, initialize it to 0.
            if (pLocOffset == nullptr)
            {
                pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
            }

            auto pElemMeta = cast<Constant>(pOutputMeta->getOperand(2));
            auto pElemTy   = pOutputTy->getArrayElementType();

            const uint64_t elemCount = pOutputTy->getArrayNumElements();
            for (uint32_t elemIdx = 0; elemIdx < elemCount; ++elemIdx)
            {
                // Handle array elements recursively
                std::vector<uint32_t> idxs;
                idxs.push_back(elemIdx);
                Value* pElem = ExtractValueInst::Create(pOutputValue, idxs, "", pInsertPos);

                // elemLocOffset = locOffset + stride * elemIdx
                Value* pElemLocOffset = BinaryOperator::CreateMul(ConstantInt::get(m_pContext->Int32Ty(), stride),
                                                                  ConstantInt::get(m_pContext->Int32Ty(), elemIdx),
                                                                  "",
                                                                  pInsertPos);
                pElemLocOffset = BinaryOperator::CreateAdd(pLocOffset, pElemLocOffset, "", pInsertPos);

                AddCallInstForOutputExport(pElem,
                                           pElemMeta,
                                           pElemLocOffset,
                                           nullptr,
                                           pVertexIdx,
                                           emitStreamId,
                                           pInsertPos);
            }
        }
    }
    else if (pOutputTy->isStructTy())
    {
        // Structure type
        LLPC_ASSERT(pElemIdx == nullptr);

        const uint64_t memberCount = pOutputTy->getStructNumElements();
        for (uint32_t memberIdx = 0; memberIdx < memberCount; ++memberIdx)
        {
            // Handle structure member recursively
            auto pMemberTy = pOutputTy->getStructElementType(memberIdx);
            auto pMemberMeta = cast<Constant>(pOutputMeta->getOperand(memberIdx));

            std::vector<uint32_t> idxs;
            idxs.push_back(memberIdx);
            Value* pMember = ExtractValueInst::Create(pOutputValue, idxs, "", pInsertPos);

            AddCallInstForOutputExport(pMember, pMemberMeta, pLocOffset, nullptr, pVertexIdx, emitStreamId, pInsertPos);
        }
    }
    else
    {
        // Normal scalar or vector type
        std::vector<Value*> args;

        outputMeta.U32All = cast<ConstantInt>(pOutputMeta)->getZExtValue();

        if ((m_shaderStage == ShaderStageGeometry) && (emitStreamId != outputMeta.StreamId))
        {
            // NOTE: For geometry shader, if the output is not bound to this vertex stream, we skip processing.
            return;
        }

        LLPC_ASSERT(outputMeta.IsLoc || outputMeta.IsBuiltIn);

        std::string instName;
        if (outputMeta.IsBuiltIn)
        {
            instName = LlpcName::OutputExportBuiltIn;
            BuiltIn builtInId = static_cast<BuiltIn>(outputMeta.Value);
            std::string builtInName = getNameMap(builtInId).map(builtInId);

            LLPC_ASSERT(builtInName.find("BuiltIn")  == 0);
            instName += builtInName.substr(strlen("BuiltIn"));
            if (pElemIdx != nullptr)
            {
                // Add this suffix when element indexing is specified for built-in export
                instName += "." + GetTypeNameForScalarOrVector(pOutputTy);
            }

            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), builtInId));
        }
        else
        {
            instName = LlpcName::OutputExportGeneric;
            instName += GetTypeNameForScalarOrVector(pOutputTy);

            auto pLoc = ConstantInt::get(m_pContext->Int32Ty(), outputMeta.Value);

            // NOTE: If the relative location offset is not specified, initialize it to 0.
            if (pLocOffset == nullptr)
            {
                pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
            }

            if (m_shaderStage == ShaderStageTessControl)
            {
                // NOTE: For tessellation control shader, we treat the location as two parts:
                // startLoc = loc + locOffset
                args.push_back(pLoc);
                args.push_back(pLocOffset);
            }
            else
            {
                auto pStartLoc = BinaryOperator::CreateAdd(pLoc, pLocOffset, "", pInsertPos);
                args.push_back(pStartLoc);
            }
        }

        if (m_shaderStage == ShaderStageTessControl)
        {
            // NOTE: For tessellation control shader, we add element indexing as an addition parameter to do addressing
            // for the output.
            if (pElemIdx == nullptr)
            {
                // When element indexing is not specified, we set it to don't-care value
                pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
            }
            args.push_back(pElemIdx);

            // NOTE: For tessellation control shader, we add vertex indexing as an addition parameter to do addressing
            // for the output.
            if (pVertexIdx == nullptr)
            {
                // When vertex indexing is not specified, we set it to don't-care value
                pVertexIdx = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
            }
            args.push_back(pVertexIdx);
        }
        else
        {
            // Element and vertex indexing is not valid for other shader stages
            LLPC_ASSERT((pElemIdx == nullptr) && (pVertexIdx == nullptr));
        }

        if (m_shaderStage == ShaderStageGeometry)
        {
            // NOTE: For geometry shader, we add stream ID for outputs.
            LLPC_ASSERT(emitStreamId == outputMeta.StreamId);
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), emitStreamId));
        }
        else
        {
            // ID of emitted vertex stream is not valid for other shader stages
            LLPC_ASSERT(emitStreamId == InvalidValue);
        }

        args.push_back(pOutputValue);

        //
        // VS:  @llpc.output.export.generic.%Type%(i32 location, %Type% outputValue)
        //      @llpc.output.export.builtin.%BuiltIn%(i32 builtInId, %Type% outputValue)
        //
        // TCS: @llpc.output.export.generic.%Type%(i32 location, i32 locOffset, i32 elemIdx, i32 vertexIdx,
        //                                         %Type% outputValue)
        //      @llpc.output.export.builtin.%BuiltIn%.%Type%(i32 builtInId, i32 elemIdx, i32 vertexIdx,
        //                                                   %Type% outputValue)
        //
        // TES: @llpc.output.export.generic.%Type%(i32 location, %Type% outputValue)
        //      @llpc.output.export.builtin.%BuiltIn%.%Type%(i32 builtInId, %Type% outputValue)

        // GS:  @llpc.output.export.generic.%Type%(i32 location, i32 streamId, %Type% outputValue)
        //      @llpc.output.export.builtin.%BuiltIn%(i32 builtInId, i32 streamId, %Type% outputValue)
        //
        // FS:  @llpc.output.export.generic.%Type%(i32 location, %Type% outputValue)
        //      @llpc.output.export.builtin.%BuiltIn%(i32 builtInId, %Type% outputValue)
        //
        EmitCall(m_pModule, instName, m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }
}

// =====================================================================================================================
// Inserts instructions to load value from input/ouput member.
Value* SpirvLowerGlobal::LoadInOutMember(
    Type*                      pInOutTy,        // [in] Type of this input/output member
    uint32_t                   addrSpace,       // Address space
    const std::vector<Value*>& indexOperands,   // [in] Index operands
    uint32_t                   operandIdx,      // Index of the index operand in processing
    Constant*                  pInOutMeta,      // [in] Metadata of this input/output member
    Value*                     pLocOffset,      // [in] Relative location offset of this input/output member
    Value*                     pVertexIdx,      // [in] Input array outermost index used for vertex indexing
    uint32_t                   interpLoc,       // Interpolation location, valid for fragment shader
                                                // (use "InterpLocUnknown" as don't-care value)
    Value*                     pSampleId,       // [in] Sample ID, used when the interpolation location is
                                                // "InterpLocSample", valid for fragment shader
    Value*                     pPixelOffset,    // [in] Offset from the center of the pixel, used when interpolation
                                                //  location is "InterpLocCenter", valid for fragment shader
    Instruction*               pInsertPos)      // [in] Where to insert calculation instructions
{
    LLPC_ASSERT((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval));

    Value* pLoadValue = nullptr;

    if (operandIdx < indexOperands.size() - 1)
    {
        if (pInOutTy->isArrayTy())
        {
            // Array type
            LLPC_ASSERT(pInOutMeta->getNumOperands() == 3);
            ShaderInOutMetadata inOutMeta = {};
            inOutMeta.U32All = cast<ConstantInt>(pInOutMeta->getOperand(1))->getZExtValue();

            auto pElemMeta = cast<Constant>(pInOutMeta->getOperand(2));
            auto pElemTy   = pInOutTy->getArrayElementType();

            if (inOutMeta.IsBuiltIn)
            {
                LLPC_ASSERT(operandIdx + 1 == indexOperands.size() - 1);
                auto pElemIdx = indexOperands[operandIdx + 1];
                return AddCallInstForInOutImport(pElemTy,
                                                 addrSpace,
                                                 pElemMeta,
                                                 pLocOffset,
                                                 pElemIdx,
                                                 pVertexIdx,
                                                 interpLoc,
                                                 pSampleId,
                                                 pPixelOffset,
                                                 pInsertPos);
            }
            else
            {
                // NOTE: If the relative location offset is not specified, initialize it to 0.
                if (pLocOffset == nullptr)
                {
                    pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
                }

                // elemLocOffset = locOffset + stride * elemIdx
                uint32_t stride = cast<ConstantInt>(pInOutMeta->getOperand(0))->getZExtValue();
                auto pElemIdx = indexOperands[operandIdx + 1];
                Value* pElemLocOffset = BinaryOperator::CreateMul(ConstantInt::get(m_pContext->Int32Ty(), stride),
                                                                  pElemIdx,
                                                                  "",
                                                                  pInsertPos);
                pElemLocOffset = BinaryOperator::CreateAdd(pLocOffset, pElemLocOffset, "", pInsertPos);

                return LoadInOutMember(pElemTy,
                                       addrSpace,
                                       indexOperands,
                                       operandIdx + 1,
                                       pElemMeta,
                                       pElemLocOffset,
                                       pVertexIdx,
                                       interpLoc,
                                       pSampleId,
                                       pPixelOffset,
                                       pInsertPos);
            }
        }
        else if (pInOutTy->isStructTy())
        {
            // Structure type
            uint32_t memberIdx = cast<ConstantInt>(indexOperands[operandIdx + 1])->getZExtValue();

            auto pMemberTy = pInOutTy->getStructElementType(memberIdx);
            auto pMemberMeta = cast<Constant>(pInOutMeta->getOperand(memberIdx));

            return LoadInOutMember(pMemberTy,
                                   addrSpace,
                                   indexOperands,
                                   operandIdx + 1,
                                   pMemberMeta,
                                   pLocOffset,
                                   pVertexIdx,
                                   interpLoc,
                                   pSampleId,
                                   pPixelOffset,
                                   pInsertPos);
        }
        else if (pInOutTy->isVectorTy())
        {
            // Vector type
            auto pCompTy = pInOutTy->getVectorElementType();

            LLPC_ASSERT(operandIdx + 1 == indexOperands.size() - 1);
            auto pCompIdx = indexOperands[operandIdx + 1];

            return AddCallInstForInOutImport(pCompTy,
                                             addrSpace,
                                             pInOutMeta,
                                             pLocOffset,
                                             pCompIdx,
                                             pVertexIdx,
                                             interpLoc,
                                             pSampleId,
                                             pPixelOffset,
                                             pInsertPos);
        }
    }
    else
    {
        // Last index operand
        LLPC_ASSERT(operandIdx == indexOperands.size() - 1);
        return AddCallInstForInOutImport(pInOutTy,
                                         addrSpace,
                                         pInOutMeta,
                                         pLocOffset,
                                         nullptr,
                                         pVertexIdx,
                                         interpLoc,
                                         pSampleId,
                                         pPixelOffset,
                                         pInsertPos);
    }

    LLPC_NEVER_CALLED();
    return nullptr;
}

// =====================================================================================================================
// Inserts instructions to store value to ouput member.
void SpirvLowerGlobal::StoreOutputMember(
    Type*                      pOutputTy,       // [in] Type of this output member
    Value*                     pStoreValue,     // [in] Value stored to output member
    const std::vector<Value*>& indexOperands,   // [in] Index operands
    uint32_t                   operandIdx,      // Index of the index operand in processing
    Constant*                  pOutputMeta,     // [in] Metadata of this output member
    Value*                     pLocOffset,      // [in] Relative location offset of this output member
    Value*                     pVertexIdx,      // [in] Input array outermost index used for vertex indexing
    Instruction*               pInsertPos)      // [in] Where to insert store instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

    if (operandIdx < indexOperands.size() - 1)
    {
        if (pOutputTy->isArrayTy())
        {
            LLPC_ASSERT(pOutputMeta->getNumOperands() == 3);
            ShaderInOutMetadata outputMeta = {};
            outputMeta.U32All = cast<ConstantInt>(pOutputMeta->getOperand(1))->getZExtValue();

            auto pElemMeta = cast<Constant>(pOutputMeta->getOperand(2));
            auto pElemTy   = pOutputTy->getArrayElementType();

            if (outputMeta.IsBuiltIn)
            {
                LLPC_ASSERT(pLocOffset == nullptr);
                LLPC_ASSERT(operandIdx + 1 == indexOperands.size() - 1);

                auto pElemIdx = indexOperands[operandIdx + 1];
                return AddCallInstForOutputExport(pStoreValue,
                                                  pElemMeta,
                                                  nullptr,
                                                  pElemIdx,
                                                  pVertexIdx,
                                                  InvalidValue,
                                                  pInsertPos);
            }
            else
            {
                // NOTE: If the relative location offset is not specified, initialize it.
                if (pLocOffset == nullptr)
                {
                    pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
                }

                // elemLocOffset = locOffset + stride * elemIdx
                uint32_t stride = cast<ConstantInt>(pOutputMeta->getOperand(0))->getZExtValue();
                auto pElemIdx = indexOperands[operandIdx + 1];
                Value* pElemLocOffset = BinaryOperator::CreateMul(ConstantInt::get(m_pContext->Int32Ty(), stride),
                                                                  pElemIdx,
                                                                  "",
                                                                  pInsertPos);
                pElemLocOffset = BinaryOperator::CreateAdd(pLocOffset, pElemLocOffset, "", pInsertPos);

                return StoreOutputMember(pElemTy,
                                         pStoreValue,
                                         indexOperands,
                                         operandIdx + 1,
                                         pElemMeta,
                                         pElemLocOffset,
                                         pVertexIdx,
                                         pInsertPos);
            }
        }
        else if (pOutputTy->isStructTy())
        {
            // Structure type
            uint32_t memberIdx = cast<ConstantInt>(indexOperands[operandIdx + 1])->getZExtValue();

            auto pMemberTy = pOutputTy->getStructElementType(memberIdx);
            auto pMemberMeta = cast<Constant>(pOutputMeta->getOperand(memberIdx));

            return StoreOutputMember(pMemberTy,
                                     pStoreValue,
                                     indexOperands,
                                     operandIdx + 1,
                                     pMemberMeta,
                                     pLocOffset,
                                     pVertexIdx,
                                     pInsertPos);
        }
        else if (pOutputTy->isVectorTy())
        {
            // Vector type
            auto pCompTy = pOutputTy->getVectorElementType();

            LLPC_ASSERT(operandIdx + 1 == indexOperands.size() - 1);
            auto pCompIdx = indexOperands[operandIdx + 1];

            return AddCallInstForOutputExport(pStoreValue,
                                              pOutputMeta,
                                              pLocOffset,
                                              pCompIdx,
                                              pVertexIdx,
                                              InvalidValue,
                                              pInsertPos);
        }
    }
    else
    {
        // Last index operand
        LLPC_ASSERT(operandIdx == indexOperands.size() - 1);

        return AddCallInstForOutputExport(pStoreValue,
                                          pOutputMeta,
                                          pLocOffset,
                                          nullptr,
                                          pVertexIdx,
                                          InvalidValue,
                                          pInsertPos);
    }

    LLPC_NEVER_CALLED();
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for globals.
INITIALIZE_PASS(SpirvLowerGlobal, "spirv-lower-global",
                "Lower SPIR-V globals (global variables, inputs, and outputs)", false, false)
