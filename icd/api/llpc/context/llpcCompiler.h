/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  llpcCompiler.h
 * @brief LLPC header file: contains declaration of class Llpc::Compiler.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcElf.h"
#include "llpcInternal.h"
#include "llpcMd5.h"
#include "llpcShaderCache.h"

namespace Llpc
{

// Forward declaration
class Context;

// =====================================================================================================================
// Enumerates types of shader binary.
enum class BinaryType : uint32_t
{
    Unknown = 0,  // Invalid type
    Spirv,       // SPIR-V binary
    LlvmBc,       // LLVM bitcode
    Elf,          // ELF
};

// =====================================================================================================================
// Represents output data of building a shader module.
struct ShaderModuleData
{
    BinaryType      binType;    // Shader binary type
    BinaryData      binCode;    // Shader binary data
    Md5::Hash       hash;       // Shader hash code
};

// Represents the properties of GPU device.
struct GpuProperty
{
    uint32_t numShaderEngines;                  // Number of shader engines present
    uint32_t waveSize;                          // Wavefront size
    uint32_t ldsSizePerCu;                      // LDS size per compute unit
    uint32_t ldsSizePerThreadGroup;             // LDS size per thread group
    uint32_t gsOnChipDefaultPrimsPerSubgroup;   // Default target number of primitives per subgroup for GS on-chip mode.
    uint32_t gsOnChipDefaultLdsSizePerSubgroup; // Default value for the maximum LDS size per subgroup for
                                                // GS on-chip mode (in DWORDs).
    uint32_t ldsSizeDwordGranularity;           // Granularity used to interpretate the LDS_SIZE register field

    //TODO: Setup gsPrimBufferDepth from hardware config option, will be done in another change.
    uint32_t gsPrimBufferDepth;     // Comes from the hardware GPU__GC__GSPRIM_BUFF_DEPTH configuration option

    uint32_t maxUserDataCount;                  // Max allowed count of user data SGPRs
};

// =====================================================================================================================
// Represents LLPC pipeline compiler.
class Compiler: public ICompiler
{
public:
    Compiler(const char* pClient, GfxIpVersion gfxIp);
    ~Compiler();

    virtual void VKAPI_CALL Destroy();

    virtual Result BuildShaderModule(const ShaderModuleBuildInfo* pShaderInfo,
                                     ShaderModuleBuildOut*        pShaderOut) const;

    virtual Result BuildGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipelineInfo,
                                         GraphicsPipelineBuildOut*        pPipelineOut);

    virtual Result BuildComputePipeline(const ComputePipelineBuildInfo* pPipelineInfo,
                                        ComputePipelineBuildOut*        pPipelineOut);

    // Gets the count of compiler instance.
    static uint32_t GetInstanceCount() { return m_instanceCount; }

    virtual uint64_t GetGraphicsPipelineHash(const GraphicsPipelineBuildInfo* pPipelineInfo) const;
    virtual uint64_t GetComputePipelineHash(const ComputePipelineBuildInfo* pPipelineInfo) const;
    virtual Result CreateShaderCache(const ShaderCacheCreateInfo* pCreateInfo, IShaderCache** ppShaderCache);

    virtual void DumpGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipelineInfo) const;
    virtual void DumpComputePipeline(const ComputePipelineBuildInfo* pPipelineInfo) const;

private:
    LLPC_DISALLOW_DEFAULT_CTOR(Compiler);
    LLPC_DISALLOW_COPY_AND_ASSIGN(Compiler);

    Result TranslateSpirvToLlvm(const BinaryData*            pSpirvBin,
                                ShaderStage                  shaderStage,
                                const char*                  pEntryTarget,
                                const VkSpecializationInfo*  pSpecializationInfo,
                                llvm::LLVMContext*           pContext,
                                llvm::Module**               ppModule) const;

    Md5::Hash GenerateHashForGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipeline) const;
    Md5::Hash GenerateHashForComputePipeline(const ComputePipelineBuildInfo* pPipeline) const;

    void UpdateHashForPipelineShaderInfo(ShaderStage               shaderStage,
                                         const PipelineShaderInfo* pShaderInfo,
                                         Md5::Context*             pMd5Context) const;

    void UpdateHashForResourceMappingNode(const ResourceMappingNode* pUserDataNode,
                                          Md5::Context*              pMd5Context) const;

    Result ValidatePipelineShaderInfo(ShaderStage shaderStage, const PipelineShaderInfo* pShaderInfo) const;

    Result BuildNullFs(Context* pContext, std::unique_ptr<llvm::Module>& pNullFsModule) const;
    Result BuildCopyShader(Context* pContext, ElfPackage* pCopyShaderElf) const;

    Result ReplaceShader(const ShaderModuleData* pOrigModuleData, ShaderModuleData** ppModuleData) const;

    void InitGpuProperty();
    void DumpTimeProfilingResult(const Md5::Hash* pHash);

    Context* AcquireContext();
    void ReleaseContext(Context* pContext);

    void DumpTimeProfileResult();

    Result OptimizeSpirv(const BinaryData* pSpirvBinIn, BinaryData* pSpirvBinOut) const;
    void CleanOptimizedSpirv(BinaryData* pSpirvBin) const;

    // -----------------------------------------------------------------------------------------------------------------

    const char*const    m_pClientName;      // Name of the client who calls LLPC
    GfxIpVersion        m_gfxIp;            // Graphics IP version info
    static uint32_t     m_instanceCount;    // The count of compiler instance
    ShaderCache         m_shaderCache;      // Shader cache
    GpuProperty         m_gpuProperty;      // GPU property
    llvm::sys::Mutex    m_contextPoolMutex; // Mutex for context pool access
    std::vector<Context*> m_contextPool;    // Context pool
};

} // Llpc
