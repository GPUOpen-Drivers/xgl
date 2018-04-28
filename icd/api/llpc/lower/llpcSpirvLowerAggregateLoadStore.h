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
 * @file  llpcSpirvLowerAggregateLoadStore.h
 * @brief LLPC header file: contains declaration of class Llpc::SpirvLowerAggregateLoadStore.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include <unordered_set>
#include "llpcSpirvLower.h"

namespace Llpc
{

// =====================================================================================================================
// Represents the pass of SPIR-V lowering opertions for load and store operation on aggregate type.
class SpirvLowerAggregateLoadStore:
    public SpirvLower,
    public llvm::InstVisitor<SpirvLowerAggregateLoadStore>
{
public:
    SpirvLowerAggregateLoadStore();

    virtual bool runOnModule(llvm::Module& module);
    virtual void visitLoadInst(llvm::LoadInst& loadInst);
    virtual void visitStoreInst(llvm::StoreInst& storeInst);

    // Pass creator, creates the pass of SPIR-V lowering opertions for load and store operations on aggregate type
    static llvm::ModulePass* Create() { return new SpirvLowerAggregateLoadStore(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(SpirvLowerAggregateLoadStore);

    void ExpandStoreInst(llvm::Value*           pStoreValue,
                         llvm::Value*           pStorePtr,
                         llvm::Type*            pStoreTy,
                         std::vector<uint32_t>& idx,
                         llvm::Instruction*     pInsertPos);

    llvm::Value* ExpandLoadInst(llvm::Value*           pLoadValue,
                                llvm::Value*           pLoadPtr,
                                llvm::Type*            pLoadTy,
                                std::vector<uint32_t>& idxStack,
                                llvm::Instruction*     pInsertPos);
    // -----------------------------------------------------------------------------------------------------------------

    std::unordered_set<llvm::Instruction*>   m_loadInsts;  // List of "load" instructions
    std::unordered_set<llvm::Instruction*>   m_storeInsts; // List of "store" instructions
};

} // Llpc
