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
 * @file  llpcSpirvLowerAlgebraTransform.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerAlgebraTransform.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-algebra-transform"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SPIRVInternal.h"
#include "llpcSpirvLowerAlgebraTransform.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerAlgebraTransform::ID = 0;

// =====================================================================================================================
SpirvLowerAlgebraTransform::SpirvLowerAlgebraTransform()
    :
    SpirvLower(ID),
    m_changed(false)
{
    initializeSpirvLowerAlgebraTransformPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerAlgebraTransform::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{

    DEBUG(dbgs() << "Run the pass Spirv-Lower-Algebra-Transform\n");

    SpirvLower::Init(&module);
    m_changed = false;

    visit(m_pModule);

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return m_changed;
}

// =====================================================================================================================
// Visits binary operator instruction.
void SpirvLowerAlgebraTransform::visitBinaryOperator(
    llvm::BinaryOperator& binaryOp)  // Binary operator instructions
{
    Instruction::BinaryOps opCode = binaryOp.getOpcode();

    auto pSrc1 = binaryOp.getOperand(0);
    auto pSrc2 = binaryOp.getOperand(1);
    bool src1IsConstZero = isa<ConstantAggregateZero>(pSrc1) ||
                          (isa<ConstantFP>(pSrc1) && cast<ConstantFP>(pSrc1)->isZero());
    bool src2IsConstZero = isa<ConstantAggregateZero>(pSrc2) ||
                          (isa<ConstantFP>(pSrc2) && cast<ConstantFP>(pSrc2)->isZero());
    Value* pDest = nullptr;

    switch (opCode)
    {
    case Instruction::FAdd:
        {
            if (src1IsConstZero)
            {
                pDest = pSrc2;
            }
            else if (src2IsConstZero)
            {
                pDest = pSrc1;
            }
            break;
        }
    case Instruction::FMul:
        {
            if (src1IsConstZero)
            {
                pDest = pSrc1;
            }
            else if (src2IsConstZero)
            {
                pDest = pSrc2;
            }
            break;
        }
    case Instruction::FDiv:
        {
            if (src1IsConstZero)
            {
                pDest = pSrc1;
            }
            break;
        }
    case Instruction::FSub:
        {
            if (src2IsConstZero)
            {
                pDest = pSrc1;
            }
            break;
        }
    default:
        {
            break;
        }
    }

    if (pDest != nullptr)
    {
        m_changed = true;
        binaryOp.replaceAllUsesWith(pDest);
        binaryOp.dropAllReferences();
        binaryOp.eraseFromParent();
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for algebraic transformation.
INITIALIZE_PASS(SpirvLowerAlgebraTransform, "spirv-lower-algebra-transform",
                "Lower SPIR-V algebraic transforms", false, false)
