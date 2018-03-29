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
 * @file  llpcSpirvLowerGlobal.h
 * @brief LLPC header file: contains declaration of class Llpc::SpirvLowerGlobal.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include <unordered_map>
#include <unordered_set>
#include <list>
#include "SPIRVInternal.h"
#include "llpcSpirvLower.h"

namespace Llpc
{

// =====================================================================================================================
// Represents the pass of SPIR-V lowering opertions for globals (global variables, inputs, and outputs).
class SpirvLowerGlobal:
    public SpirvLower,
    public llvm::InstVisitor<SpirvLowerGlobal>
{
public:
    SpirvLowerGlobal();

    virtual bool runOnModule(llvm::Module& module);
    virtual void visitReturnInst(llvm::ReturnInst& retInst);
    virtual void visitCallInst(llvm::CallInst& callInst);
    virtual void visitLoadInst(llvm::LoadInst& loadInst);
    virtual void visitStoreInst(llvm::StoreInst& storeInst);

    // Pass creator, creates the pass of SPIR-V lowering opertions for globals
    static llvm::ModulePass* Create() { return new SpirvLowerGlobal(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(SpirvLowerGlobal);

    void MapGlobalVariableToProxy(llvm::GlobalVariable* pGlobalVar);
    void MapInputToProxy(llvm::GlobalVariable* pInput);
    void MapOutputToProxy(llvm::GlobalVariable* pInput);
    void RemoveConstantExpr();

    void LowerGlobalVar();
    void LowerInput();
    void LowerOutput();
    void LowerInOutInPlace();

    llvm::Value* AddCallInstForInOutImport(llvm::Type*        pInOutTy,
                                           uint32_t           addrSpace,
                                           llvm::Constant*    pInOutMeta,
                                           llvm::Value*       pStartLoc,
                                           llvm::Value*       pCompIdx,
                                           llvm::Value*       pVertexIdx,
                                           uint32_t           interpLoc,
                                           Value*             pInterpInfo,
                                           llvm::Instruction* pInsertPos);

    void AddCallInstForOutputExport(llvm::Value*       pOutputValue,
                                    llvm::Constant*    pOutputMeta,
                                    llvm::Value*       pLocOffset,
                                    llvm::Value*       pElemIdx,
                                    llvm::Value*       pVertexIdx,
                                    uint32_t           emitStreamId,
                                    llvm::Instruction* pInsertPos);

    llvm::Value* LoadInOutMember(llvm::Type*                      pInOutTy,
                                 uint32_t                         addrSpace,
                                 const std::vector<llvm::Value*>& indexOperands,
                                 uint32_t                         operandIdx,
                                 llvm::Constant*                  pInOutMeta,
                                 llvm::Value*                     pLocOffset,
                                 llvm::Value*                     pVertexIdx,
                                 uint32_t                         interpLoc,
                                 llvm::Value*                     pInterpInfo,
                                 llvm::Instruction*               pInsertPos);

    void StoreOutputMember(llvm::Type*                      pOutputTy,
                           llvm::Value*                     pStoreValue,
                           const std::vector<llvm::Value*>& indexOperands,
                           uint32_t                         operandIdx,
                           llvm::Constant*                  pOutputMeta,
                           llvm::Value*                     pLocOffset,
                           llvm::Value*                     pVertexIdx,
                           llvm::Instruction*               pInsertPos);

    // -----------------------------------------------------------------------------------------------------------------

    std::unordered_map<llvm::Value*, llvm::Value*>  m_globalVarProxyMap;    // Proxy map for lowering global variables
    std::unordered_map<llvm::Value*, llvm::Value*>  m_inputProxyMap;        // Proxy map for lowering inputs

    // NOTE: Here we use list to store pairs of output proxy mappings. This is because we want output patching to be
    // "ordered" (resulting LLVM IR for the patching always be consistent).
    std::list<std::pair<llvm::Value*, llvm::Value*> >  m_outputProxyMap;    // Proxy list for lowering outputs

    llvm::BasicBlock*   m_pRetBlock;    // The return block of entry point

    bool    m_lowerInputInPlace;    // Whether to lower input inplace
    bool    m_lowerOutputInPlace;   // Whether to lower output inplace

    // Flags controlling how to behave when visting the instructions
    union
    {
        struct
        {
            uint32_t checkEmitCall    : 1;  // Whether to check "emit" calls
            uint32_t checkInterpCall  : 1;  // Whether to check interpolation calls
            uint32_t checkReturn      : 1;  // Whether to check "return" instructions
            uint32_t checkLoad        : 1;  // Whether to check "load" instructions
            uint32_t checkStore       : 1;  // Whether to check "store" instructions

            uint32_t unused           : 27;
        };

        uint32_t u32All;

    } m_instVisitFlags;

    std::unordered_set<llvm::ReturnInst*>  m_retInsts;      // "Return" instructions to be removed
    std::unordered_set<llvm::CallInst*>    m_emitCalls;     // "Call" instructions to emit vertex (geometry shader)
    std::unordered_set<llvm::LoadInst*>    m_loadInsts;     // "Load" instructions to be removed
    std::unordered_set<llvm::StoreInst*>   m_storeInsts;    // "Store" instructions to be removed
    std::unordered_set<llvm::CallInst*>    m_interpCalls;   // "Call" instruction to do input interpolation
                                                            // (fragment shader)
};

} // Llpc
