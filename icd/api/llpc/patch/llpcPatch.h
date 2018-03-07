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
 * @file  llpcPatch.h
 * @brief LLPC header file: contains declaration of class Llpc::Patch.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/Pass.h"

#include "llpc.h"
#include "llpcDebug.h"

namespace llvm
{

class PassRegistry;

void initializePatchAddrSpaceMutatePass(PassRegistry&);
void initializePatchBufferOpPass(PassRegistry&);
void initializePatchDeadFuncRemovePass(PassRegistry&);
void initializePatchDescriptorLoadPass(PassRegistry&);
void initializePatchEntryPointMutatePass(PassRegistry&);
void initializePatchExternalLibLinkPass(PassRegistry&);
void initializePatchImageOpPass(PassRegistry&);
void initializePatchInOutImportExportPass(PassRegistry&);
void initializePatchPushConstOpPass(PassRegistry&);
void initializePatchResourceCollectPass(PassRegistry&);

} // llvm

namespace Llpc
{

class Context;

// =====================================================================================================================
// Represents the pass of LLVM patching operations, as the base class.
class Patch: public llvm::ModulePass
{
public:
    explicit Patch(char& Pid)
        :
        llvm::ModulePass(Pid),
        m_pModule(nullptr),
        m_pContext(nullptr),
        m_shaderStage(ShaderStageInvalid),
        m_pEntryPoint(nullptr)
    {
    }
    virtual ~Patch() {}

    static Result PreRun(llvm::Module* pModule);
    static Result Run(llvm::Module* pModule);

protected:
    void Init(llvm::Module* pModule);

    // -----------------------------------------------------------------------------------------------------------------

    llvm::Module*   m_pModule;      // LLVM module to be run on
    Context*        m_pContext;     // Associated LLPC context of the LLVM module that passes run on
    ShaderStage     m_shaderStage;  // Shader stage
    llvm::Function* m_pEntryPoint;  // Entry-point

private:
    LLPC_DISALLOW_DEFAULT_CTOR(Patch);
    LLPC_DISALLOW_COPY_AND_ASSIGN(Patch);
};

} // Llpc
