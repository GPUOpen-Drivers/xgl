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
 * @file  llpcPatchPushConstOp.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchPushConstOp.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-push-const"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcIntrinsDefs.h"
#include "llpcPatchPushConstOp.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchPushConstOp::ID = 0;

// =====================================================================================================================
PatchPushConstOp::PatchPushConstOp()
    :
    Patch(ID)
{
    initializePatchPushConstOpPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool PatchPushConstOp::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Push-Const-Op\n");

    Patch::Init(&module);

    // Invoke handling of "call" instruction
    visit(m_pModule);

    // Remove unnecessary push constant load calls
    for (auto pCallInst: m_pushConstCalls)
    {
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    // Remove unnecessary push constant load functions
    for (auto pFunc : m_descLoadFuncs)
    {
        if (pFunc->user_empty())
        {
            pFunc->dropAllReferences();
            pFunc->eraseFromParent();
        }
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "call" instruction.
void PatchPushConstOp::visitCallInst(
    CallInst& callInst) // [in] "Call" instruction
{
    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    auto mangledName = pCallee->getName();

    if (mangledName.startswith(LlpcName::PushConstLoad))
    {
        auto pIntfData = m_pContext->GetShaderInterfaceData(m_shaderStage);
        auto pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);
        uint32_t pushConstNodeIdx = pIntfData->pushConst.resNodeIdx;
        LLPC_ASSERT(pushConstNodeIdx != InvalidValue);
        auto pPushConstNode = &pShaderInfo->pUserDataNodes[pushConstNodeIdx];

        if (pPushConstNode->offsetInDwords < pIntfData->spillTable.offsetInDwords)
        {
            auto pMemberOffsetInBytes = callInst.getOperand(0);
            auto pPushConst = GetFunctionArgument(m_pEntryPoint,
                                                  pIntfData->entryArgIdxs.resNodeValues[pushConstNodeIdx]);

            // Load push constant per dword
            Type* pLoadTy = callInst.getType();

            LLPC_ASSERT(pLoadTy->isVectorTy() &&
                        pLoadTy->getVectorElementType()->isIntegerTy() &&
                        (pLoadTy->getScalarSizeInBits() == 8));

            uint32_t dwordCount = pLoadTy->getVectorNumElements() / 4;

            Instruction* pInsertPos = &callInst;

            Value* pLoadValue = UndefValue::get(VectorType::get(m_pContext->Int32Ty(), dwordCount));
            for (uint32_t i = 0; i < dwordCount; ++i)
            {
                Value* pDwordIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                Value* pDestIdx = pDwordIdx;
                Value* pMemberDwordOffset = BinaryOperator::Create(Instruction::AShr,
                                                                   pMemberOffsetInBytes,
                                                                   ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                                   "",
                                                                   pInsertPos);
                pDwordIdx = BinaryOperator::Create(Instruction::Add, pDwordIdx, pMemberDwordOffset, "", pInsertPos);
                Value* pTmp = ExtractElementInst::Create(pPushConst, pDwordIdx, "", pInsertPos);
                pLoadValue = InsertElementInst::Create(pLoadValue, pTmp, pDestIdx, "", pInsertPos);
            }

            pLoadValue = new BitCastInst(pLoadValue,
                                         VectorType::get(m_pContext->Int8Ty(), pLoadTy->getVectorNumElements()),
                                         "",
                                         pInsertPos);

            callInst.replaceAllUsesWith(pLoadValue);

            m_pushConstCalls.insert(&callInst);
            m_descLoadFuncs.insert(pCallee);
        }
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patch operations for push constant operations.
INITIALIZE_PASS(PatchPushConstOp, "Patch-push-const",
                "Patch LLVM for push constant operations", false, false)
