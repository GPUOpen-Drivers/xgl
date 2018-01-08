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
 * @file  llpcSpirvLower.h
 * @brief LLPC header file: contains declaration of class Llpc::SpirvLower.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/Pass.h"

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcInternal.h"

namespace llvm
{

class PassRegistry;
void initializeSpirvLowerAccessChainPass(PassRegistry&);
void initializeSpirvLowerAggregateLoadStorePass(PassRegistry&);
void initializeSpirvLowerAlgebraTransformPass(PassRegistry&);
void initializeSpirvLowerBufferOpPass(PassRegistry&);
void initializeSpirvLowerConstImmediateStorePass(PassRegistry&);
void initializeSpirvLowerDynIndexPass(PassRegistry&);
void initializeSpirvLowerGlobalPass(PassRegistry&);
void initializeSpirvLowerImageOpPass(PassRegistry&);
void initializeSpirvLowerOptPass(PassRegistry&);
void initializeSpirvLowerResourceCollectPass(PassRegistry&);

} // llvm

namespace Llpc
{

class Context;

// =====================================================================================================================
// Represents the pass of SPIR-V lowering operations, as the base class.
class SpirvLower: public llvm::ModulePass
{
public:
    explicit SpirvLower(char& Pid)
        :
        llvm::ModulePass(Pid),
        m_pModule(nullptr),
        m_pContext(nullptr),
        m_shaderStage(ShaderStageInvalid),
        m_pEntryPoint(nullptr)
    {
    }

    static Result Run(llvm::Module* pModule);

protected:
    void Init(llvm::Module* pModule);

    // -----------------------------------------------------------------------------------------------------------------

    llvm::Module*   m_pModule;      // LLVM module to be run on
    Context*        m_pContext;     // Associated LLPC context of the LLVM module that passes run on
    ShaderStage     m_shaderStage;  // Shader stage
    llvm::Function* m_pEntryPoint;  // Entry point of input module

private:
    LLPC_DISALLOW_DEFAULT_CTOR(SpirvLower);
    LLPC_DISALLOW_COPY_AND_ASSIGN(SpirvLower);
};

} // Llpc
