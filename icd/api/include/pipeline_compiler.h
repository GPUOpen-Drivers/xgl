/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  pipeline_compiler.h
 * @brief Contains declaration of Vulkan pipeline compiler
 ***********************************************************************************************************************
 */

#pragma once

#include "include/khronos/vulkan.h"
#include "include/compiler_solution.h"
#include "include/shader_cache.h"

#include "include/compiler_solution_llpc.h"

#include "include/vk_shader_code.h"

namespace vk
{

class PipelineLayout;
class PipelineCache;
class ShaderModule;
class PipelineCompiler;
struct VbBindingInfo;
struct ShaderModuleHandle;

class PipelineBinaryCache;

// =====================================================================================================================
class PipelineCompiler
{
public:
    PipelineCompiler(PhysicalDevice* pPhysicalDevice);

    ~PipelineCompiler();

    VkResult Initialize();

    void Destroy();

    VkResult CreateShaderCache(
        const void*                  pInitialData,
        size_t                       initialDataSize,
        void*                        pShaderCacheMem,
        ShaderCache*                 pShaderCache);

    size_t GetShaderCacheSize(PipelineCompilerType cacheType);

    PipelineCompilerType GetShaderCacheType();

    PipelineBinaryCache* GetBinaryCache() const { return m_pBinaryCache; }

    void ApplyPipelineOptions(
        const Device*          pDevice,
        VkPipelineCreateFlags  flags,
        Vkgc::PipelineOptions* pOptions);

    VkResult BuildShaderModule(
        const Device*             pDevice,
        VkShaderModuleCreateFlags flags,
        size_t                    codeSize,
        const void*               pCode,
        ShaderModuleHandle*       pModule);

    virtual VkResult CreatePartialPipelineBinary(
        uint32_t                             deviceIdx,
        void*                                pShaderModuleData,
        Vkgc::ShaderModuleEntryData*         pShaderModuleEntryData,
        const Vkgc::ResourceMappingRootNode* pResourceMappingNode,
        uint32_t                             mappingNodeCount,
        Vkgc::ColorTarget*                   pColorTarget);

    VkResult CreateGraphicsPipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        GraphicsPipelineCreateInfo*         pCreateInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary,
        uint32_t                            rasterizationStream,
        Util::MetroHash::Hash*              pCacheId);

    VkResult CreateComputePipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        ComputePipelineCreateInfo*          pInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary,
        Util::MetroHash::Hash*              pCacheId);

    VkResult SetPipelineCreationFeedbackInfo(
        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo,
        const PipelineCreationFeedback*                pPipelineFeedback);

    VkResult ConvertGraphicsPipelineInfo(
        Device*                                         pDevice,
        const VkGraphicsPipelineCreateInfo*             pIn,
        GraphicsPipelineCreateInfo*                     pInfo,
        VbBindingInfo*                                  pVbInfo,
        const VkPipelineCreationFeedbackCreateInfoEXT** ppPipelineCreationFeadbackCreateInfo);

    VkResult ConvertComputePipelineInfo(
        Device*                                         pDevice,
        const VkComputePipelineCreateInfo*              pIn,
        ComputePipelineCreateInfo*                      pInfo,
        const VkPipelineCreationFeedbackCreateInfoEXT** ppPipelineCreationFeadbackCreateInfo);

    void FreeShaderModule(ShaderModuleHandle* pShaderModule);

    void FreeComputePipelineBinary(
        ComputePipelineCreateInfo* pCreateInfo,
        const void*                pPipelineBinary,
        size_t                     binarySize);

    void FreeGraphicsPipelineBinary(
        GraphicsPipelineCreateInfo* pCreateInfo,
        const void*                pPipelineBinary,
        size_t                     binarySize);

    void FreeComputePipelineCreateInfo(ComputePipelineCreateInfo* pCreateInfo);

    void FreeGraphicsPipelineCreateInfo(GraphicsPipelineCreateInfo* pCreateInfo);

#if ICD_GPUOPEN_DEVMODE_BUILD
    Util::Result RegisterAndLoadReinjectionBinary(
        const Pal::PipelineHash*     pInternalPipelineHash,
        const Util::MetroHash::Hash* pCacheId,
        size_t*                      pBinarySize,
        const void**                 ppPipelineBinary,
        PipelineCache*               pPipelineCache = nullptr);
#endif

    template<class PipelineBuildInfo>
    PipelineCompilerType CheckCompilerType(const PipelineBuildInfo* pPipelineBuildInfo);

    uint32_t GetCompilerCollectionMask();

    void ApplyDefaultShaderOptions(
        ShaderStage                  stage,
        Vkgc::PipelineShaderOptions* pShaderOptions
    ) const;

    VK_INLINE Vkgc::GfxIpVersion& GetGfxIp() { return m_gfxIp; }

    void GetElfCacheMetricString(char* pOutStr, size_t outStrSize);

    void DestroyPipelineBinaryCache();

private:

    void ApplyProfileOptions(
        Device*                      pDevice,
        ShaderStage                  stage,
        ShaderModule*                pShaderModule,
        Vkgc::PipelineOptions*       pPipelineOptions,
        Vkgc::PipelineShaderInfo*    pShaderInfo,
        PipelineOptimizerKey*        pProfileKey,
        Vkgc::NggState*              pNggState
    );

    template<class PipelineBuildInfo>
    bool ReplacePipelineBinary(
        const PipelineBuildInfo* pPipelineBuildInfo,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary,
        uint64_t                 hashCode64);

    void DropPipelineBinaryInst(
        Device*                pDevice,
        const RuntimeSettings& settings,
        const void*            pPipelineBinary,
        size_t                 pipelineBinarySize);

    void ReplacePipelineIsaCode(
        Device*                pDevice,
        uint64_t               pipelineHash,
        uint32_t               pipelineIndex,
        const void*            pPipelineBinary,
        size_t                 pipelineBinarySize);

    bool LoadReplaceShaderBinary(
        uint64_t shaderHash,
        size_t*  pCodeSize,
        void**   ppCode);

    bool ReplacePipelineShaderModule(
        const Device*             pDevice,
        PipelineCompilerType      compilerType,
        Vkgc::PipelineShaderInfo* pShaderInfo,
        ShaderModuleHandle*       pShaderModule);

    Util::Result GetCachedPipelineBinary(
        const Util::MetroHash::Hash* pCacheId,
        const PipelineBinaryCache*   pPipelineBinaryCache,
        size_t*                      pPipelineBinarySize,
        const void**                 ppPipelineBinary,
        bool*                        pIsUserCacheHit,
        bool*                        pIsInternalCacheHit,
        bool*                        pFreeWithCompiler,
        PipelineCreationFeedback*    pPipelineFeedback);

    // -----------------------------------------------------------------------------------------------------------------

    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Vkgc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by Vkgcf

    CompilerSolutionLlpc m_compilerSolutionLlpc;

    PipelineBinaryCache* m_pBinaryCache;       // Pipeline binary cache object

    // Metrics
    uint32_t             m_cacheAttempts;      // Number of attempted cache loads
    uint32_t             m_cacheHits;          // Number of cache hits
    uint32_t             m_totalBinaries;      // Total number of binaries compiled or fetched
    int64_t              m_totalTimeSpent;     // Accumulation of time spent either loading or compiling pipeline
                                               // binaries

    void GetPipelineCreationInfoNext(
        const VkStructHeader*                             pHeader,
        const VkPipelineCreationFeedbackCreateInfoEXT**   ppPipelineCreationFeadbackCreateInfo);

    static VkPipelineCreateFlags GetCacheIdControlFlags(
        VkPipelineCreateFlags in);
}; // class PipelineCompiler

} // namespce vk
