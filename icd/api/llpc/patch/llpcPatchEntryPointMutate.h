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
 * @file  llpcPatchEntryPointMutate.h
 * @brief LLPC header file: contains declaration of class Llpc::PatchEntryPointMutate.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include "llpcPatch.h"

namespace Llpc
{

// =====================================================================================================================
// Represents the pass of LLVM patching opertions for entry-point mutation.
class PatchEntryPointMutate:
    public Patch,
    public llvm::InstVisitor<PatchEntryPointMutate>
{
public:
    PatchEntryPointMutate();

    virtual bool runOnModule(llvm::Module& module);

    // Pass creator, creates the pass of LLVM patching opertions for entry-point mutation
    static llvm::ModulePass* Create() { return new PatchEntryPointMutate(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(PatchEntryPointMutate);

    llvm::FunctionType* GenerateEntryPointType(uint64_t* pInRegMask) const;

    llvm::Value* InitPointerWithValue(llvm::Value*       pPtr,
                                      llvm::Value*       pLowValue,
                                      llvm::Value*       pHighValue,
                                      llvm::Type*        pCastTy,
                                      llvm::Instruction* pInsertPos) const;

    bool IsResourceMappingNodeActive(const ResourceMappingNode* pNode) const;

    llvm::Value* SetRingBufferDataFormat(llvm::Value* pBufDesc,
                                         uint32_t dataFormat,
                                         llvm::Instruction* pInsertPos) const;

    // -----------------------------------------------------------------------------------------------------------------

    // Reserved argument count for single DWORD descriptor table pointer
    static const uint32_t   TablePtrReservedArgCount = 2;

    bool    m_hasTs;    // Whether the pipeline has tessllation shader
    bool    m_hasGs;    // Whether the pipeline has geometry shader
};

} // Llpc
