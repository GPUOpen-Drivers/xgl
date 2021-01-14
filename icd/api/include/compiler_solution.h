/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  compiler_solution.h
* @brief Contains declaration of ComplerSolution
***********************************************************************************************************************
*/
#ifndef __COMPILER_SOLUTION_H__
#define __COMPILER_SOLUTION_H__

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_utils.h"

#include "vkgcDefs.h"

#include "include/app_shader_optimizer.h"

#include "palMetroHash.h"

namespace vk
{

class PhysicalDevice;
class PipelineCache;
class ShaderCache;

// Represents the result of PipelineCompiler::BuildShaderModule
struct ShaderModuleHandle
{
    void*   pLlpcShaderModule;   // Shader module handle from LLPC
};

// =====================================================================================================================
// Pipeline Creation feedback info.
struct PipelineCreationFeedback
{
    bool        feedbackValid;
    bool        hitApplicationCache;
    uint64_t    duration;
};

// =====================================================================================================================
// Represents pipeline metadata included in the pipeline ELF.
struct PipelineMetadata
{
    bool     reservedForFutureHw1;
    bool     reservedForFutureHw2;
    uint16_t reservedForFutureHw3;
    uint16_t reservedForFutureHw4;
};

// =====================================================================================================================
struct GraphicsPipelineCreateInfo
{
    Vkgc::GraphicsPipelineBuildInfo        pipelineInfo;
    void*                                  pTempBuffer;
    void*                                  pMappingBuffer;
    size_t                                 mappingBufferSize;
    VkPipelineCreateFlags                  flags;
    VkFormat                               dbFormat;
    PipelineOptimizerKey                   pipelineProfileKey;
    PipelineCompilerType                   compilerType;
    bool                                   freeWithCompiler;
    Util::MetroHash::Hash                  basePipelineHash;
    PipelineCreationFeedback               pipelineFeedback;
    PipelineCreationFeedback               stageFeedback[ShaderStage::ShaderStageGfxCount];
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
    Vkgc::ResourceMappingData              resourceMapping;
#endif
};

// =====================================================================================================================
struct ComputePipelineCreateInfo
{
    Vkgc::ComputePipelineBuildInfo         pipelineInfo;
    void*                                  pTempBuffer;
    void*                                  pMappingBuffer;
    size_t                                 mappingBufferSize;
    VkPipelineCreateFlags                  flags;
    PipelineOptimizerKey                   pipelineProfileKey;
    PipelineCompilerType                   compilerType;
    bool                                   freeWithCompiler;
    Util::MetroHash::Hash                  basePipelineHash;
    PipelineCreationFeedback               pipelineFeedback;
    PipelineCreationFeedback               stageFeedback;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
    Vkgc::ResourceMappingData              resourceMapping;
#endif
};

// =====================================================================================================================
// Base class for compiler solution
class CompilerSolution
{
public:
    CompilerSolution(PhysicalDevice* pPhysicalDevice);
    virtual ~CompilerSolution();

    virtual VkResult Initialize(Vkgc::GfxIpVersion gfxIp, Pal::GfxIpLevel gfxIpLevel, Vkgc::ICache* pCache) = 0;

    virtual void Destroy() = 0;

    virtual size_t GetShaderCacheSize(PipelineCompilerType cacheType) = 0;

    virtual VkResult CreateShaderCache(
        const void*  pInitialData,
        size_t       initialDataSize,
        void*        pShaderCacheMem,
        ShaderCache* pShaderCache) = 0;

    virtual VkResult BuildShaderModule(
        const Device*                pDevice,
        VkShaderModuleCreateFlags    flags,
        size_t                       codeSize,
        const void*                  pCode,
        ShaderModuleHandle*          pShaderModule,
        const Util::MetroHash::Hash& hash) = 0;

    virtual void FreeShaderModule(ShaderModuleHandle* pShaderModule) = 0;

    virtual VkResult CreatePartialPipelineBinary(
        uint32_t                             deviceIdx,
        void*                                pShaderModuleData,
        Vkgc::ShaderModuleEntryData*         pShaderModuleEntryData,
        const Vkgc::ResourceMappingRootNode* pResourceMappingNode,
        uint32_t                             mappingNodeCount,
        Vkgc::ColorTarget*                   pColorTarget) = 0;

    virtual VkResult CreateGraphicsPipelineBinary(
        Device*                     pDevice,
        uint32_t                    deviceIdx,
        PipelineCache*              pPipelineCache,
        GraphicsPipelineCreateInfo* pCreateInfo,
        size_t*                     pPipelineBinarySize,
        const void**                ppPipelineBinary,
        uint32_t                    rasterizationStream,
        Vkgc::PipelineShaderInfo**  ppShadersInfo,
        void*                       pPipelineDumpHandle,
        uint64_t                    pipelineHash,
        int64_t*                    pCompileTime) = 0;

    virtual VkResult CreateComputePipelineBinary(
        Device*                     pDevice,
        uint32_t                    deviceIdx,
        PipelineCache*              pPipelineCache,
        ComputePipelineCreateInfo*  pCreateInfo,
        size_t*                     pPipelineBinarySize,
        const void**                ppPipelineBinary,
        void*                       pPipelineDumpHandle,
        uint64_t                    pipelineHash,
        int64_t*                    pCompileTime) = 0;

    virtual void FreeGraphicsPipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) = 0;

    virtual void FreeComputePipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) = 0;

protected:
    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Vkgc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by Vkgc
    Pal::GfxIpLevel    m_gfxIpLevel;           // Graphics IP level
    static const char* GetShaderStageName(ShaderStage shaderStage);
};

}

#endif
