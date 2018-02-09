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
 * @file  llpcCopyShader.h
 * @brief LLPC header file: contains declaration of class Llpc::CopyShader.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcElf.h"
#include "llpcInternal.h"

namespace Llpc
{

class Context;

// Count of user SGPRs used in copy shader
static const uint32_t CopyShaderUserSgprCount = 3;

// User SGPR index for ES-GS LDS size used in copy shader
static const uint32_t CopyShaderUserSgprIdxEsGsLdsSize = 2;

// =====================================================================================================================
// Represents the manager of copy shader generation.
class CopyShader
{
public:
    CopyShader(Context* pContext);

    Result Run(llvm::Module** ppModule);

private:
    LLPC_DISALLOW_DEFAULT_CTOR(CopyShader);
    LLPC_DISALLOW_COPY_AND_ASSIGN(CopyShader);

    Result LoadLibrary(llvm::Module*& pModule);
    void ExportOutput();
    Result DoPatch();

    llvm::Value* CalcGsVsRingOffsetForInput(uint32_t           location,
                                            uint32_t           compIdx,
                                            llvm::Instruction* pInsertPos);

    llvm::Value* LoadValueFromGsVsRingBuffer(uint32_t           location,
                                             uint32_t           compIdx,
                                             llvm::Instruction* pInsertPos);

    void ExportGenericOutput(llvm::Value* pOutputValue, uint32_t location, llvm::Instruction* pInsertPos);
    void ExportBuiltInOutput(llvm::Value* pOutputValue, spv::BuiltIn builtInId, llvm::Instruction* pInsertPos);

    // -----------------------------------------------------------------------------------------------------------------

    // Low part of global internal table pointer
    static const uint32_t EntryArgIdxInternalTablePtrLow = 0;

    // Start offset of currently-processed vertex in GS-VS ring buffer
    static const uint32_t EntryArgIdxVertexOffset = 3;

    llvm::Module*           m_pModule;                      // LLVM module for copy shader
    Context*                m_pContext;                     // LLPC context
    llvm::Function*         m_pEntryPoint;                  // Entry point of copy shader module
    llvm::GlobalVariable*   m_pLds;                         // Global variable to model LDS
};

} // Llpc
