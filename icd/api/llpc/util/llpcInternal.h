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
 * @file  llpcInternal.h
 * @brief LLPC header file: contains LLPC internal-use definitions (including data types and utility functions).
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "spirv.hpp"
#include "llpc.h"
#include "llpcUtil.h"

namespace llvm { class Module; }
namespace Llpc { class Context; }

// Internally defined SPIR-V semantics (internal-use)
namespace spv
{

// Built-ins for fragment input interpolation (I/J)
static const BuiltIn BuiltInInterpPerspSample    = static_cast<BuiltIn>(0x10000000);
static const BuiltIn BuiltInInterpPerspCenter    = static_cast<BuiltIn>(0x10000001);
static const BuiltIn BuiltInInterpPerspCentroid  = static_cast<BuiltIn>(0x10000002);
static const BuiltIn BuiltInInterpPullMode       = static_cast<BuiltIn>(0x10000003);
static const BuiltIn BuiltInInterpLinearSample   = static_cast<BuiltIn>(0x10000004);
static const BuiltIn BuiltInInterpLinearCenter   = static_cast<BuiltIn>(0x10000005);
static const BuiltIn BuiltInInterpLinearCentroid = static_cast<BuiltIn>(0x10000006);

// Built-ins for sample position emulation
static const BuiltIn BuiltInSamplePosOffset      = static_cast<BuiltIn>(0x10000007);
static const BuiltIn BuiltInNumSamples           = static_cast<BuiltIn>(0x10000008);
static const BuiltIn BuiltInSamplePatternIdx     = static_cast<BuiltIn>(0x10000009);
static const BuiltIn BuiltInWaveId               = static_cast<BuiltIn>(0x1000000A);

// Execution model: copy shader
static const ExecutionModel ExecutionModelCopyShader = static_cast<ExecutionModel>(1024);

} // spv

namespace Llpc
{

namespace LlpcName
{
    const static char InputImportGeneric[]            = "llpc.input.import.generic.";
    const static char InputImportBuiltIn[]            = "llpc.input.import.builtin.";
    const static char InputImportInterpolant[]        = "llpc.input.import.interpolant.";
    const static char OutputImportGeneric[]           = "llpc.output.import.generic.";
    const static char OutputImportBuiltIn[]           = "llpc.output.import.builtin.";
    const static char OutputExportGeneric[]           = "llpc.output.export.generic.";
    const static char OutputExportBuiltIn[]           = "llpc.output.export.builtin.";
    const static char InputInterpEval[]               = "llpc.input.interpolate.evalij.";
    const static char BufferCallPrefix[]              = "llpc.buffer.";
    const static char BufferAtomic[]                  = "llpc.buffer.atomic.";
    const static char BufferLoad[]                    = "llpc.buffer.load.";
    const static char BufferLoadUniform[]             = "llpc.buffer.load.uniform.";
    const static char BufferStore[]                   = "llpc.buffer.store.";
    const static char BufferArrayLength[]             = "llpc.buffer.arraylength";
    const static char InlineConstLoadUniform[]        = "llpc.inlineconst.load.uniform.";
    const static char InlineConstLoad[]               = "llpc.inlineconst.load.";
    const static char PushConstLoad[]                 = "llpc.pushconst.load.";
    const static char TfBufferStore[]                 = "llpc.tfbuffer.store.f32";

    const static char DescriptorLoadPrefix[]          = "llpc.descriptor.load.";
    const static char DescriptorLoadResource[]        = "llpc.descriptor.load.resource";
    const static char DescriptorLoadSampler[]         = "llpc.descriptor.load.sampler";
    const static char DescriptorLoadFmask[]           = "llpc.descriptor.load.fmask";
    const static char DescriptorLoadBuffer[]          = "llpc.descriptor.load.buffer";
    const static char DescriptorLoadAddress[]         = "llpc.descriptor.load.address";
    const static char DescriptorLoadInlineBuffer[]    = "llpc.descriptor.load.inlinebuffer";
    const static char DescriptorLoadTexelBuffer[]     = "llpc.descriptor.load.texelbuffer";
    const static char DescriptorLoadSpillTable[]      = "llpc.descriptor.load.spilltable";
    const static char DescriptorLoadGsVsRingBuffer[]  = "llpc.descriptor.load.gsvsringbuffer";

    const static char ImageCallPrefix[]               = "llpc.image.";

    const static char GlobalProxyPrefix[]             = "__llpc_global_proxy_";
    const static char InputProxyPrefix[]              = "__llpc_input_proxy_";
    const static char OutputProxyPrefix[]             = "__llpc_output_proxy_";

    // Names of entry-points for merged shader
    const static char LsEntryPoint[]                  = "llpc.ls.main";
    const static char HsEntryPoint[]                  = "llpc.hs.main";
    const static char EsEntryPoint[]                  = "llpc.es.main";
    const static char GsEntryPoint[]                  = "llpc.gs.main";

} // LlpcName

// Maximum count of input/output locations that a shader stage (except fragment shader outputs) is allowed to specify
static const uint32_t MaxInOutLocCount = 32;

// Maximum array size of gl_ClipDistance[] and gl_CullDistance[]
static const uint32_t MaxClipCullDistanceCount = 8;

// Threshold of inline pass
static const int32_t InlineThreshold = (INT32_MAX / 100);

// Internal resource table's virtual descriptor sets
static const uint32_t InternalResourceTable  = 0x10000000;
static const uint32_t InternalPerShaderTable = 0x10000001;

// Internal resource table's virtual bindings
static const uint32_t SI_DRV_TABLE_SCRATCH_GFX_SRD_OFFS = 0;
static const uint32_t SI_DRV_TABLE_SCRATCH_CS_SRD_OFFS  = 1;
static const uint32_t SI_DRV_TABLE_ES_RING_OUT_OFFS     = 2;
static const uint32_t SI_DRV_TABLE_GS_RING_IN_OFFS      = 3;
static const uint32_t SI_DRV_TABLE_GS_RING_OUT0_OFFS    = 4;
static const uint32_t SI_DRV_TABLE_GS_RING_OUT1_OFFS    = 5;
static const uint32_t SI_DRV_TABLE_GS_RING_OUT2_OFFS    = 6;
static const uint32_t SI_DRV_TABLE_GS_RING_OUT3_OFFS    = 7;
static const uint32_t SI_DRV_TABLE_VS_RING_IN_OFFS      = 8;
static const uint32_t SI_DRV_TABLE_TF_BUFFER_OFFS       = 9;
static const uint32_t SI_DRV_TABLE_HS_BUFFER0_OFFS      = 10;
static const uint32_t SI_DRV_TABLE_OFF_CHIP_PARAM_CACHE = 11;
static const uint32_t SI_DRV_TABLE_SAMPLEPOS            = 12;

// No attribute
static const std::vector<llvm::Attribute::AttrKind>   NoAttrib;

// Gets the entry point (valid for AMD GPU) of a LLVM module.
llvm::Function* GetEntryPoint(llvm::Module* pModule);

// Emits a LLVM function call (inserted before the specified instruction), builds it automically based on return type
// and its parameters.
llvm::Value* EmitCall(llvm::Module*                             pModule,
                      llvm::StringRef                           instName,
                      llvm::Type*                               pRetTy,
                      llvm::ArrayRef<llvm::Value *>             args,
                      llvm::ArrayRef<llvm::Attribute::AttrKind> attribs,
                      llvm::Instruction*                        pInsertPos);

// Emits a LLVM function call (inserted at the end of the specified basic block), builds it automically based on return
// type and its parameters.
llvm::Value* EmitCall(llvm::Module*                             pModule,
                      llvm::StringRef                           instName,
                      llvm::Type*                               pRetTy,
                      llvm::ArrayRef<llvm::Value *>             args,
                      llvm::ArrayRef<llvm::Attribute::AttrKind> attribs,
                      llvm::BasicBlock*                         pInsertAtEnd);

// Gets LLVM-style name for scalar or vector type.
std::string GetTypeNameForScalarOrVector(llvm::Type* pTy);

// Gets the shader stage from the specified LLVM module.
ShaderStage GetShaderStageFromModule(llvm::Module* pModule);

// Gets the argument from the specified function according to the argument index.
llvm::Value* GetFunctionArgument(llvm::Function* pFunc, uint32_t idx);

// Checks if one type can be bitcasted to the other (type1 -> type2).
bool CanBitCast(const llvm::Type* pTy1, const llvm::Type* pTy2);

// Checks whether input binary data is SPIR-V binary
bool IsSpirvBinary(const BinaryData*  pShaderBin);

// Checks whether input binary data is LLVM bitcode
bool IsLlvmBitcode(const BinaryData*  pShaderBin);

// Gets the shader stage mask from the SPIR-V binary according to the specified entry-point.
uint32_t GetStageMaskFromSpirvBinary(const BinaryData* pSpvBin, const char* pEntryName);

// Verifies if the SPIR-V binary is valid and is supported
Result VerifySpirvBinary(const BinaryData* pSpvBin);

// Checks if the specified value actually represents a don't-care value (0xFFFFFFFF).
bool IsDontCareValue(llvm::Value* pValue);

// Translates an integer to 32-bit integer regardless of its initial bit width.
llvm::Value* ToInt32Value(Llpc::Context* pContext, llvm::Value* pValue, llvm::Instruction* pInsertPos);

// Retrieves the frequency of the performance counter for CPU times.
int64_t GetPerfFrequency();

// Retrieves the current value of the performance counter.
int64_t GetPerfCpuTime();

// =====================================================================================================================
// Represents the result of CPU time profiling.
struct TimeProfileResult
{
    int64_t translateTime;    // Translate time
    int64_t lowerTime;        // SPIR-V Lower phase time
    int64_t patchTime;        // LLVM patch pahse time
    int64_t lowerOptTime;     // General optimization time of SPIR-V lower phase
    int64_t patchLinkTime;    // Library link time of LLVM patch phase
    int64_t codeGenTime;      // Code generation time
};

// =====================================================================================================================
// Helper class for time profiling
class TimeProfiler
{
public:
    TimeProfiler(int64_t* pAccumTime)
    {
        m_pAccumTime = pAccumTime;
        m_startTime = GetPerfCpuTime();
    }

    ~TimeProfiler()
    {
        *m_pAccumTime += (GetPerfCpuTime() - m_startTime);
    }

    int64_t m_startTime;     // Start time
    int64_t* m_pAccumTime;   // Pointer to accumulated time
};

} // Llpc

// Initialize optimizer
void InitOptimizer();

// Do optimization for the specified LLVM mode, codes are ported from LLVM "opt.exe"
bool OptimizeModule(llvm::Module* M);

