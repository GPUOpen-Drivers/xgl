/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
class PipelineCompiler;
struct PipelineInternalBufferInfo;

#if VKI_RAY_TRACING
class LlpcHelperThreadProvider : public Llpc::IHelperThreadProvider
{
public:
    struct HelperThreadProviderPayload
    {
        Llpc::IHelperThreadProvider* pHelperProvider;
        Llpc::IHelperThreadProvider::ThreadFunction* pFunction;
        void* pPayload;
    };

    LlpcHelperThreadProvider(DeferredWorkload* pDeferredWorkload) : m_pDeferredWorkload(pDeferredWorkload) {}
    ~LlpcHelperThreadProvider() {}

    virtual void SetTasks(ThreadFunction* pFunction, uint32_t numTasks, void* pPayload) override;

    virtual bool GetNextTask(uint32_t* pTaskIndex) override;

    virtual void TaskCompleted() override;

    virtual void WaitForTasks() override;

private:
    DeferredWorkload* m_pDeferredWorkload;
    HelperThreadProviderPayload m_payload;
};
#endif

// =====================================================================================================================
// Compiler solution for LLPC
class CompilerSolutionLlpc final : public CompilerSolution
{
public:
    CompilerSolutionLlpc(PhysicalDevice* pPhysicalDevice);
    virtual ~CompilerSolutionLlpc();

public:
    // Overridden functions
    virtual VkResult Initialize(
        Vkgc::GfxIpVersion   gfxIp,
        Pal::GfxIpLevel      gfxIpLevel,
        PipelineBinaryCache* pCache) override;

    virtual void Destroy() override;

    virtual VkResult BuildShaderModule(
        const Device*                pDevice,
        VkShaderModuleCreateFlags    flags,
        VkShaderModuleCreateFlags    internalShaderFlags,
        const Vkgc::BinaryData&      shaderBinary,
        ShaderModuleHandle*          pShaderModule,
        const PipelineOptimizerKey&  profileKey) override;

    virtual void TryEarlyCompileShaderModule(
        const Device*       pDevice,
        ShaderModuleHandle* pModule) override { }

    virtual void FreeShaderModule(ShaderModuleHandle* pShaderModule) override;

    virtual VkResult CreateGraphicsPipelineBinary(
        const Device*                     pDevice,
        uint32_t                          deviceIdx,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        Vkgc::BinaryData*                 pPipelineBinary,
        Vkgc::PipelineShaderInfo**        ppShadersInfo,
        void*                             pPipelineDumpHandle,
        uint64_t                          pipelineHash,
        Util::MetroHash::Hash*            pCacheId,
        int64_t*                          pCompileTime) override;

    virtual VkResult CreateGraphicsShaderBinary(
        const Device*                     pDevice,
        PipelineCache*                    pPipelineCache,
        GraphicsLibraryType               gplType,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        void*                             pPipelineDumpHandle,
        GplModuleState*                   pModuleState) override;

    virtual VkResult CreateColorExportBinary(
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        void*                             pPipelineDumpHandle,
        Vkgc::BinaryData*                 pOutputPackage) override;

    virtual VkResult CreateComputePipelineBinary(
        Device*                          pDevice,
        uint32_t                         deviceIdx,
        PipelineCache*                   pPipelineCache,
        ComputePipelineBinaryCreateInfo* pCreateInfo,
        Vkgc::BinaryData*                pPipelineBinary,
        void*                            pPipelineDumpHandle,
        uint64_t                         pipelineHash,
        Util::MetroHash::Hash*           pCacheId,
        int64_t*                         pCompileTime) override;

    virtual void FreeGraphicsPipelineBinary(
        const Vkgc::BinaryData& pipelineBinary) override;

    virtual void FreeComputePipelineBinary(
        const Vkgc::BinaryData& pipelineBinary) override;

#if VKI_RAY_TRACING
    virtual VkResult CreateRayTracingPipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIdx,
        PipelineCache*                      pPipelineCache,
        RayTracingPipelineBinaryCreateInfo* pCreateInfo,
        RayTracingPipelineBinary*           pPipelineBinary,
        void*                               pPipelineDumpHandle,
        uint64_t                            pipelineHash,
        Util::MetroHash::Hash*              pCacheId,
        int64_t*                            pCompileTime) override;

    virtual void FreeRayTracingPipelineBinary(
        RayTracingPipelineBinary* pPipelineBinary) override;
#endif

    virtual void BuildPipelineInternalBufferData(
        const PipelineCompiler*           pCompiler,
        const uint32_t                    uberFetchConstBufRegBase,
        const uint32_t                    specConstBufVertexRegBase,
        const uint32_t                    specConstBufFragmentRegBase,
        bool                              needCache,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo) override;

    virtual bool IsGplFastLinkCompatible(
        const Device*                           pDevice,
        uint32_t                                deviceIdx,
        const GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        const GraphicsPipelineLibraryInfo&      libInfo) override;

    virtual Vkgc::BinaryData ExtractPalElfBinary(const Vkgc::BinaryData& shaderBinary) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(CompilerSolutionLlpc);

    typedef LlpcShaderLibraryBlobHeader ShaderLibraryBlobHeader;

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
