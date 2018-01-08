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
 * @file  llpcSpirvLowerBufferOp.h
 * @brief LLPC header file: contains declaration of class Llpc::SpirvLowerBufferOp.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include <unordered_set>
#include "llpcSpirvLower.h"

namespace Llpc
{

// =====================================================================================================================
// Represents the pass of SPIR-V lowering opertions for buffer operations (load and store).
class SpirvLowerBufferOp:
    public SpirvLower,
    public llvm::InstVisitor<SpirvLowerBufferOp>
{
public:
    SpirvLowerBufferOp();

    virtual bool runOnModule(llvm::Module& module);
    virtual void visitCallInst(llvm::CallInst& callInst);
    virtual void visitLoadInst(llvm::LoadInst& loadInst);
    virtual void visitStoreInst(llvm::StoreInst& storeInst);

    // Pass creator, creates the pass of SPIR-V lowering opertions for buffer operations
    static llvm::ModulePass* Create() { return new SpirvLowerBufferOp(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(SpirvLowerBufferOp);

    llvm::Value* CalcBlockOffset(const llvm::Type*                pBlockTy,
                                 const std::vector<llvm::Value*>& indexOperands,
                                 uint32_t                         operandIdx,
                                 llvm::Instruction*               pInsertPos,
                                 uint32_t*                        pStride);

    llvm::Value* CalcBlockMemberOffset(const llvm::Type*                pBlockMemberTy,
                                       const std::vector<llvm::Value*>& indexOperands,
                                       uint32_t                         operandIdx,
                                       llvm::Constant*                  pMeta,
                                       llvm::Instruction*               pInsertPos,
                                       llvm::Constant**                 ppResultMeta);

    llvm::Value* AddBufferLoadInst(llvm::Type*        pLoadTy,
                                   uint32_t           descSet,
                                   uint32_t           binding,
                                   bool               isPushConst,
                                   llvm::Value*       pBlockOffset,
                                   llvm::Value*       pBlockMemberOffset,
                                   llvm::Constant*    pBlockMemberMeta,
                                   llvm::Instruction* pInsertPos);

    void AddBufferStoreInst(llvm::Value*        pStoreValue,
                            uint32_t            descSet,
                            uint32_t            binding,
                            llvm::Value*        pBlockOffset,
                            llvm::Value*        pBlockMemberOffset,
                            llvm::Constant*     pBlockMemberMeta,
                            llvm::Instruction*  pInsertPos);

    llvm::Value* AddBufferAtomicInst(std::string                      atomicOpName,
                                     llvm::Type*                      pDataTy,
                                     const std::vector<llvm::Value*>& data,
                                     uint32_t                         descSet,
                                     uint32_t                         binding,
                                     llvm::Value*                     pBlockOffset,
                                     llvm::Value*                     pBlockMemberOffset,
                                     llvm::Constant*                  pBlockMemberMeta,
                                     llvm::Instruction*               pInsertPos);

    llvm::Value* TransposeMatrix(llvm::Value* pMatrix, llvm::Instruction* pInsertPos);

    llvm::Value* LoadEntireBlock(llvm::GlobalVariable*      pBlock,
                                 llvm::Type*                pLoadTy,
                                 std::vector<llvm::Value*>& indexOperands,
                                 llvm::Instruction*         pInsertPos);

    void StoreEntireBlock(llvm::GlobalVariable*      pBlock,
                          llvm::Value*               pStoreValue,
                          std::vector<llvm::Value*>& indexOperands,
                          llvm::Instruction*         pInsertPos);

    // -----------------------------------------------------------------------------------------------------------------

    std::unordered_set<llvm::Instruction*>   m_loadInsts;  // List of "load" instructions
    std::unordered_set<llvm::Instruction*>   m_storeInsts; // List of "store" instructions
    std::unordered_set<llvm::Instruction*>   m_callInsts;  // List of "call" instructions (array length or atomic operations)
};

} // Llpc
