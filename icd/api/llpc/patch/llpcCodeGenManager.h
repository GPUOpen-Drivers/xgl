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
 * @file  llpcCodeGenManager.h
 * @brief LLPC header file: contains declaration of class Llpc::CodeGenManager.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include "llpc.h"
#include "llpcDebug.h"
#include "llpcElf.h"

namespace Llpc
{

namespace Gfx6
{
    struct PipelineVsFsRegConfig;
    struct PipelineCsRegConfig;
}

class Context;

// Represents data entry in a ELF section, including associated ELF symbols.
struct ElfDataEntry
{
    const void* pData;           // Data in the section
    uint32_t    offset;          // Offset of the data
    uint32_t    size;            // Size of the data
    uint32_t    padSize;         // Padding size of the data
    const char* pSymName;        // Name of associated ELF symbol
};

// =====================================================================================================================
// Represents the manager of GPU ISA code generation.
class CodeGenManager
{
public:
    static Result CreateTargetMachine(Context* pContext);

    static Result GenerateCode(llvm::Module* pModule, llvm::raw_pwrite_stream& outStream, std::string& errMsg);

private:
    LLPC_DISALLOW_DEFAULT_CTOR(CodeGenManager);
    LLPC_DISALLOW_COPY_AND_ASSIGN(CodeGenManager);

    static void DiagnosticHandler(const llvm::DiagnosticInfo& diagInfo, void* pContext);

    static void FatalErrorHandler(void* userData, const std::string& reason, bool gen_crash_diag);

    static Result AddAbiMetadata(Context* pContext, llvm::Module* pModule);

    static Result BuildGraphicsPipelineRegConfig(Context*            pContext,
                                                 uint8_t**           ppConfig,
                                                 size_t*             pConfigSize);

    static Result BuildComputePipelineRegConfig(Context*            pContext,
                                                uint8_t**           ppConfig,
                                                size_t*             pConfigSize);
};

} // Llpc
