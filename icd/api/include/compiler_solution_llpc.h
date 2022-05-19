/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "llpc.h"

namespace vk
{

// =====================================================================================================================
// Compiler solution for LLPC
class CompilerSolutionLlpc final : public CompilerSolution
{
public:
    CompilerSolutionLlpc(PhysicalDevice* pPhysicalDevice);
    virtual ~CompilerSolutionLlpc();

public:
    // Overridden functions
    virtual VkResult Initialize(Vkgc::GfxIpVersion gfxIp, Pal::GfxIpLevel gfxIpLevel, Vkgc::ICache* pCache) override;

    virtual void Destroy() override;

    virtual size_t GetShaderCacheSize(PipelineCompilerType cacheType) override;

    virtual VkResult CreateShaderCache(
        const void*  pInitialData,
        size_t       initialDataSize,
        void*        pShaderCacheMem,
        uint32_t     expectedEntries,
        ShaderCache* pShaderCache) override;

    virtual VkResult BuildShaderModule(
        const Device*                pDevice,
        VkShaderModuleCreateFlags    flags,
        size_t                       codeSize,
        const void*                  pCode,
        const bool                   adaptForFaskLink,
        ShaderModuleHandle*          pShaderModule,
        const Util::MetroHash::Hash& hash) override;

    virtual void TryEarlyCompileShaderModule(
        const Device*       pDevice,
        ShaderModuleHandle* pModule) override { }

    virtual void FreeShaderModule(ShaderModuleHandle* pShaderModule) override;

    virtual VkResult CreateGraphicsPipelineBinary(
        Device*                           pDevice,
        uint32_t                          deviceIdx,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        size_t*                           pPipelineBinarySize,
        const void**                      ppPipelineBinary,
        Vkgc::PipelineShaderInfo**        ppShadersInfo,
        void*                             pPipelineDumpHandle,
        uint64_t                          pipelineHash,
        Util::MetroHash::Hash*            pCacheId,
        int64_t*                          pCompileTime) override;

    virtual VkResult CreateGraphicsShaderBinary(
        const Device*                           pDevice,
        const ShaderStage                       stage,
        const GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        ShaderModuleHandle*                     pShaderModule) override { return VK_SUCCESS; }

    virtual VkResult CreateComputePipelineBinary(
        Device*                          pDevice,
        uint32_t                         deviceIdx,
        PipelineCache*                   pPipelineCache,
        ComputePipelineBinaryCreateInfo* pCreateInfo,
        size_t*                          pPipelineBinarySize,
        const void**                     ppPipelineBinary,
        void*                            pPipelineDumpHandle,
        uint64_t                         pipelineHash,
        Util::MetroHash::Hash*           pCacheId,
        int64_t*                         pCompileTime) override;

    virtual void FreeGraphicsPipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) override;

    virtual void FreeComputePipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(CompilerSolutionLlpc);

    VkResult CreateLlpcCompiler(Vkgc::ICache* pCache);

    void UpdateStageCreationFeedback(
        PipelineCreationFeedback*       pStageFeedback,
        const Vkgc::PipelineShaderInfo& shader,
        const Llpc::CacheAccessInfo*    pStageCacheAccesses,
        ShaderStage                     stage);

private:
    Llpc::ICompiler*    m_pLlpc;               // LLPC compiler object

};

}

#endif
