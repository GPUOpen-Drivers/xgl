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
 * @file  llpcPatchInOutImportExport.h
 * @brief LLPC header file: contains declaration of class Llpc::PatchInOutImportExport.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include "SPIRVInternal.h"
#include "llpcIntrinsDefs.h"
#include "llpcPatch.h"

namespace Llpc
{

class FragColorExport;
class VertexFetch;
// =====================================================================================================================
// Represents the pass of LLVM patching opertions for input import and output export.
class PatchInOutImportExport:
    public Patch,
    public llvm::InstVisitor<PatchInOutImportExport>
{
public:
    PatchInOutImportExport();
    virtual ~PatchInOutImportExport();

    virtual bool runOnModule(llvm::Module& module);
    virtual void visitCallInst(llvm::CallInst& callInst);
    virtual void visitReturnInst(llvm::ReturnInst& retInst);

    // Pass creator, creates the pass of LLVM patching opertions for input import and output export
    static llvm::ModulePass* Create() { return new PatchInOutImportExport(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(PatchInOutImportExport);

    llvm::Value* PatchVsGenericInputImport(llvm::Type*        pInputTy,
                                           uint32_t           location,
                                           uint32_t           compIdx,
                                           llvm::Instruction* pInsertPos);
    llvm::Value* PatchTcsGenericInputImport(llvm::Type*        pInputTy,
                                            uint32_t           location,
                                            llvm::Value*       pLocOffset,
                                            llvm::Value*       pCompIdx,
                                            llvm::Value*       pVertexIdx,
                                            llvm::Instruction* pInsertPos);
    llvm::Value* PatchTesGenericInputImport(llvm::Type*        pInputTy,
                                            uint32_t           location,
                                            llvm::Value*       pLocOffset,
                                            llvm::Value*       pCompIdx,
                                            llvm::Value*       pVertexIdx,
                                            llvm::Instruction* pInsertPos);
    llvm::Value* PatchGsGenericInputImport(llvm::Type*        pInputTy,
                                           uint32_t           location,
                                           uint32_t           compIdx,
                                           llvm::Value*       pVertexIdx,
                                           llvm::Instruction* pInsertPos);
    llvm::Value* PatchFsGenericInputImport(llvm::Type*        pInputTy,
                                           uint32_t           location,
                                           Value*             pLocOffset,
                                           Value*             pCompIdx,
                                           Value*             pAuxInterpValue,
                                           uint32_t           interpMode,
                                           uint32_t           interpLoc,
                                           llvm::Instruction* pInsertPos);

    llvm::Value* PatchTcsGenericOutputImport(llvm::Type*        pOutputTy,
                                             uint32_t           location,
                                             llvm::Value*       pLocOffset,
                                             llvm::Value*       pCompIdx,
                                             llvm::Value*       pVertexIdx,
                                             llvm::Instruction* pInsertPos);

    void PatchVsGenericOutputExport(llvm::Value*       pOutput,
                                    uint32_t           location,
                                    uint32_t           compIdx,
                                    llvm::Instruction* pInsertPos);
    void PatchTcsGenericOutputExport(llvm::Value*       pOutput,
                                     uint32_t           location,
                                     llvm::Value*       pLocOffset,
                                     llvm::Value*       pCompIdx,
                                     llvm::Value*       pVertexIdx,
                                     llvm::Instruction* pInsertPos);
    void PatchTesGenericOutputExport(llvm::Value*       pOutput,
                                     uint32_t           location,
                                     uint32_t           compIdx,
                                     llvm::Instruction* pInsertPos);
    void PatchGsGenericOutputExport(llvm::Value*       pOutput,
                                    uint32_t           location,
                                    uint32_t           compIdx,
                                    uint32_t           streamId,
                                    llvm::Instruction* pInsertPos);
    void PatchFsGenericOutputExport(llvm::Value*       pOutput,
                                    uint32_t           location,
                                    uint32_t           compIdx,
                                    llvm::Instruction* pInsertPos);

    llvm::Value* PatchVsBuiltInInputImport(llvm::Type* pInputTy, uint32_t builtInId, llvm::Instruction* pInsertPos);
    llvm::Value* PatchTcsBuiltInInputImport(llvm::Type*        pInputTy,
                                            uint32_t           builtInId,
                                            llvm::Value*       pElemIdx,
                                            llvm::Value*       pVertexIdx,
                                            llvm::Instruction* pInsertPos);
    llvm::Value* PatchTesBuiltInInputImport(llvm::Type*        pInputTy,
                                            uint32_t           builtInId,
                                            llvm::Value*       pElemIdx,
                                            llvm::Value*       pVertexIdx,
                                            llvm::Instruction* pInsertPos);
    llvm::Value* PatchGsBuiltInInputImport(llvm::Type*        pInputTy,
                                           uint32_t           builtInId,
                                           llvm::Value*       pVertexIdx,
                                           llvm::Instruction* pInsertPos);
    llvm::Value* PatchFsBuiltInInputImport(llvm::Type* pInputTy, uint32_t builtInId, llvm::Instruction* pInsertPos);
    llvm::Value* PatchCsBuiltInInputImport(llvm::Type* pInputTy, uint32_t builtInId, llvm::Instruction* pInsertPos);

    llvm::Value* PatchTcsBuiltInOutputImport(llvm::Type*        pOutputTy,
                                             uint32_t           builtInId,
                                             llvm::Value*       pElemIdx,
                                             llvm::Value*       pVertexIdx,
                                             llvm::Instruction* pInsertPos);

    void PatchVsBuiltInOutputExport(llvm::Value* pOutput, uint32_t builtInId, llvm::Instruction* pInsertPos);
    void PatchTcsBuiltInOutputExport(llvm::Value*       pOutput,
                                     uint32_t           builtInId,
                                     llvm::Value*       pElemIdx,
                                     llvm::Value*       pVertexIdx,
                                     llvm::Instruction* pInsertPos);
    void PatchTesBuiltInOutputExport(llvm::Value* pOutput, uint32_t builtInId, llvm::Instruction* pInsertPos);
    void PatchGsBuiltInOutputExport(llvm::Value*       pOutput,
                                    uint32_t           builtInId,
                                    uint32_t           streamId,
                                    llvm::Instruction* pInsertPos);
    void PatchFsBuiltInOutputExport(llvm::Value* pOutput, uint32_t builtInId, llvm::Instruction* pInsertPos);

    void PatchCopyShaderGenericOutputExport(llvm::Value* pOutput, uint32_t location, llvm::Instruction* pInsertPos);
    void PatchCopyShaderBuiltInOutputExport(llvm::Value* pOutput, uint32_t builtInId, llvm::Instruction* pInsertPos);

    void StoreValueToEsGsRing(llvm::Value*        pStoreValue,
                              uint32_t            location,
                              uint32_t            compIdx,
                              llvm::Instruction*  pInsertPos);

    llvm::Value* LoadValueFromEsGsRing(llvm::Type*         pLoadType,
                                       uint32_t            location,
                                       uint32_t            compIdx,
                                       llvm::Value*        pVertexIdx,
                                       llvm::Instruction*  pInsertPos);

    void StoreValueToGsVsRingBuffer(llvm::Value*        pStoreValue,
                                    uint32_t            location,
                                    uint32_t            compIdx,
                                    llvm::Instruction*  pInsertPos);

    llvm::Value* CalcEsGsRingOffsetForOutput(uint32_t           location,
                                             uint32_t           compIdx,
                                             llvm::Value*       pEsGsOffset,
                                             llvm::Instruction* pInsertPos);

    llvm::Value* CalcEsGsRingOffsetForInput(uint32_t           location,
                                            uint32_t           compIdx,
                                            llvm::Value*       pVertexIdx,
                                            llvm::Instruction* pInsertPos);

    llvm::Value* CalcGsVsRingOffsetForOutput(uint32_t           location,
                                             uint32_t           compIdx,
                                             llvm::Value*       pVertexIdx,
                                             llvm::Value*       pGsVsOffset,
                                             llvm::Instruction* pInsertPos);

    llvm::Value* ReadValueFromLds(bool isOutput, llvm::Type* pReadTy, llvm::Value* pLdsOffset, llvm::Instruction* pInsertPos);
    void WriteValueToLds(llvm::Value* pWriteValue, llvm::Value* pLdsOffset, llvm::Instruction* pInsertPos);

    llvm::Value* CalcTessFactorOffset(bool isOuter, llvm::Value* pElemIdx, llvm::Instruction* pInsertPos);

    void StoreTessFactorToBuffer(const std::vector<llvm::Value*>& tessFactors,
                                 llvm::Value*                     pTessFactorOffset,
                                 llvm::Instruction*               pInsertPos);

    void CreateTessBufferStoreFunction();

    uint32_t CalcPatchCountPerThreadGroup(uint32_t inVertexCount,
                                          uint32_t inVertexStride,
                                          uint32_t outVertexCount,
                                          uint32_t outVertexStride,
                                          uint32_t patchConstCount) const;

    llvm::Value* CalcLdsOffsetForVsOutput(Type*              pOutputTy,
                                          uint32_t           location,
                                          uint32_t           compIdx,
                                          llvm::Instruction* pInsertPos);

    llvm::Value* CalcLdsOffsetForTcsInput(Type*              pInputTy,
                                          uint32_t           location,
                                          llvm::Value*       pLocOffset,
                                          llvm::Value*       pCompIdx,
                                          llvm::Value*       pVertexIdx,
                                          llvm::Instruction* pInsertPos);

    llvm::Value* CalcLdsOffsetForTcsOutput(Type*              pOutputTy,
                                           uint32_t           location,
                                           llvm::Value*       pLocOffset,
                                           llvm::Value*       pCompIdx,
                                           llvm::Value*       pVertexIdx,
                                           llvm::Instruction* pInsertPos);

    llvm::Value* CalcLdsOffsetForTesInput(Type*              pInputTy,
                                          uint32_t           location,
                                          llvm::Value*       pLocOffset,
                                          llvm::Value*       pCompIdx,
                                          llvm::Value*       pVertexIdx,
                                          llvm::Instruction* pInsertPos);

    void AddExportInstForGenericOutput(llvm::Value*       pOutput,
                                       uint32_t           location,
                                       uint32_t           compIdx,
                                       llvm::Instruction* pInsertPos);
    void AddExportInstForBuiltInOutput(llvm::Value* pOutput, uint32_t builtInId, llvm::Instruction* pInsertPos);

    // -----------------------------------------------------------------------------------------------------------------

    GfxIpVersion            m_gfxIp;                    // Graphics IP version info

    VertexFetch*            m_pVertexFetch;             // Vertex fetch manager
    FragColorExport*        m_pFragColorExport;         // Fragment color export manager

    llvm::CallInst*         m_pLastExport;              // Last "export" intrinsic for which "done" flag is valid

    llvm::Value*            m_pClipDistance;            // Correspond to "out float gl_ClipDistance[]"
    llvm::Value*            m_pCullDistance;            // Correspond to "out float gl_CullDistance[]"
    llvm::Value*            m_pPrimitiveId;             // Correspond to "out int gl_PrimitiveID"
    // NOTE: For GFX6, gl_FragDepth, gl_FragStencilRef and gl_SampleMask[] are exported at the same time with one "EXP"
    // instruction. Thus, the export is delayed.
    llvm::Value*            m_pFragDepth;               // Correspond to "out float gl_FragDepth"
    llvm::Value*            m_pFragStencilRef;          // Correspond to "out int gl_FragStencilRef"
    llvm::Value*            m_pSampleMask;              // Correspond to "out int gl_SampleMask[]"
    // NOTE: For GFX9, gl_ViewportIndex and gl_Layer are packed with one channel (gl_ViewpoertInex is 16-bit high part
    // and gl_Layer is 16-bit low part). Thus, the export is delayed with them merged together.
    llvm::Value*            m_pViewportIndex;           // Correspond to "out int gl_ViewportIndex"
    llvm::Value*            m_pLayer;                   // Correspond to "out int gl_Layer"

    bool                    m_hasTs;                    // Whether the pipeline has tessellation shaders

    bool                    m_hasGs;                    // Whether the pipeline has geometry shader

    GlobalVariable*         m_pLds;                     // Global variable to model LDS
    llvm::Value*            m_pThreadId;                // Thread ID

    std::vector<Value*>     m_expFragColors[MaxColorTargets]; // Exported fragment colors

    std::vector<llvm::CallInst*> m_importCalls; // List of "call" instructions to import inputs
    std::vector<llvm::CallInst*> m_exportCalls; // List of "call" instructions to export outputs
};

} // Llpc
