/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  compiler_solution_llpc.h
* @brief Contains declaration of ComplerSolutionLlpc
***********************************************************************************************************************
*/
#ifndef __COMPILER_SOLUTION_LLPC_H__
#define __COMPILER_SOLUTION_LLPC_H__
#include "include/compiler_solution.h"

namespace vk
{

// =====================================================================================================================
// Compiler solution for LLPC
class CompilerSolutionLlpc : public CompilerSolution
{
public:
    CompilerSolutionLlpc(PhysicalDevice* pPhysicalDevice);
    virtual ~CompilerSolutionLlpc();

public:
    // Overidded functions
    virtual VkResult Initialize();

    virtual void Destroy();

    virtual size_t GetShaderCacheSize(PipelineCompilerType cacheType);

    virtual VkResult CreateShaderCache(
        const void*  pInitialData,
        size_t       initialDataSize,
        void*        pShaderCacheMem,
        bool         isScpcInternalCache,
        ShaderCache* pShaderCache);

    virtual VkResult BuildShaderModule(
        const Device*                pDevice,
        size_t                       codeSize,
        const void*                  pCode,
        ShaderModuleHandle*          pShaderModule,
        const Util::MetroHash::Hash& hash);

    virtual void FreeShaderModule(ShaderModuleHandle* pShaderModule);

    virtual VkResult CreateGraphicsPipelineBinary(
        Device*                     pDevice,
        uint32_t                    deviceIdx,
        PipelineCache*              pPipelineCache,
        GraphicsPipelineCreateInfo* pCreateInfo,
        size_t*                     pPipelineBinarySize,
        const void**                ppPipelineBinary,
        uint32_t                    rasterizationStream,
        Llpc::PipelineShaderInfo**  ppShadersInfo,
        void*                       pPipelineDumpHandle,
        uint64_t                    pipelineHash,
        int64_t*                    pCompileTime);

    virtual VkResult CreateComputePipelineBinary(
        Device*                     pDevice,
        uint32_t                    deviceIdx,
        PipelineCache*              pPipelineCache,
        ComputePipelineCreateInfo*  pCreateInfo,
        size_t*                     pPipelineBinarySize,
        const void**                ppPipelineBinary,
        void*                       pPipelineDumpHandle,
        uint64_t                    pipelineHash,
        int64_t*                    pCompileTime);

    virtual void FreeGraphicsPipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize);

    virtual void FreeComputePipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize);
private:
    VkResult CreateLlpcCompiler();

private:
    Llpc::ICompiler*    m_pLlpc;               // LLPC compiler object

};

}

#endif
