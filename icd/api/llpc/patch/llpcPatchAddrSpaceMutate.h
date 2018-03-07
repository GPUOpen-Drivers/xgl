/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcPatchAddrSpaceMutate.h
 * @brief LLPC header file: contains declaration of class Llpc::PatchAddrSpaceMutate.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcPatch.h"

namespace Llpc
{

// =====================================================================================================================
// Represents the pass of LLVM patch operations of mutating address spaces from SPIRAS to AMDGPU.
class PatchAddrSpaceMutate: public Patch
{
public:
    PatchAddrSpaceMutate();

    virtual bool runOnModule(llvm::Module& module);

    // Pass creator, creates the pass of LLVM patching operations of mutating address spaces from SPIRAS to AMDGPU
    static llvm::ModulePass* Create() { return new PatchAddrSpaceMutate(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(PatchAddrSpaceMutate);

    void ProcessFunction(llvm::Function* pFunc);
    llvm::Type* MapType(llvm::Type* pOldType);

    // -----------------------------------------------------------------------------------------------------------------

    llvm::SmallVector<unsigned, 9> m_addrSpaceMap; // Address space mapping (from SPIRAS to AMDGPU)
    std::map<llvm::Type*, llvm::Type*> m_typeMap; // Type mapping (from SPIRAS to AMDGPU)
    std::map<llvm::GlobalValue*, llvm::GlobalValue*> m_globalMap; // Global mapping, for any global whose type needed
                                                                  // to be changed
};

} // Llpc
