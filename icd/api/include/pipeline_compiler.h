/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_shader_code.h"

#include "llpc.h"
#include "include/app_shader_optimizer.h"

#define ICD_BUILD_MULIT_COMPILER 0

#if ICD_BUILD_MULIT_COMPILER
#include "palHashSet.h"
#endif

namespace vk
{

class PhysicalDevice;
class PipelineLayout;
class PipelineCache;
class ShaderModule;
class PipelineCompiler;
struct VbBindingInfo;
struct ShaderModuleHandle;

// Enumerates the compiler types
enum PipelineCompilerType : uint32_t
{
    PipelineCompilerTypeLlpc,  // Use shader compiler provided by LLPC
};

// Represents the result of PipelineCompiler::BuildShaderModule
struct ShaderModuleHandle
{
    void*   pLlpcShaderModule;   // Shader module handle from LLPC
};

// Unified shader cache interface
class ShaderCache
{
public:
    union ShaderCachePtr
    {
        Llpc::IShaderCache* pLlpcShaderCache; // Pointer to LLPC shader cache object
    };

    ShaderCache();

    void Init(PipelineCompilerType cacheType, ShaderCachePtr cachePtr);

    VkResult Serialize(void* pBlob, size_t* pSize);

    VkResult Merge(uint32_t srcCacheCount, const ShaderCachePtr* ppSrcCaches);

    void Destroy(PipelineCompiler* pCompiler);

    PipelineCompilerType GetCacheType() const { return m_cacheType; }

    ShaderCachePtr GetCachePtr() { return m_cache; }

private:
    PipelineCompilerType  m_cacheType;
    ShaderCachePtr        m_cache;
};

// =====================================================================================================================
class PipelineCompiler
{
public:
    struct GraphicsPipelineCreateInfo
    {
        Llpc::GraphicsPipelineBuildInfo        pipelineInfo;
        VkPipelineCreateFlags                  flags;
        void*                                  pMappingBuffer;
        size_t                                 tempBufferStageSize;
        VkFormat                               dbFormat;
        PipelineOptimizerKey                   pipelineProfileKey;
        PipelineCompilerType                   compilerType;
    };

    struct ComputePipelineCreateInfo
    {
        Llpc::ComputePipelineBuildInfo         pipelineInfo;
        VkPipelineCreateFlags                  flags;
        void*                                  pMappingBuffer;
        size_t                                 tempBufferStageSize;
        PipelineOptimizerKey                   pipelineProfileKey;
        PipelineCompilerType                   compilerType;
    };

    PipelineCompiler(PhysicalDevice* pPhysicalDevice);

    ~PipelineCompiler();

    VkResult Initialize();

    void Destroy();

    VkResult CreateShaderCache(
        const void*                  pInitialData,
        size_t                       initialDataSize,
        void*                        pShaderCacheMem,
        bool                         isScpcInternalCache,
        ShaderCache*                 pShaderCache);

    size_t GetShaderCacheSize(PipelineCompilerType cacheType);

    PipelineCompilerType GetShaderCacheType();

    VkResult BuildShaderModule(
        const Device*       pDevice,
        size_t              codeSize,
        const void*         pCode,
        ShaderModuleHandle* pModule);

    VkResult CreateGraphicsPipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        GraphicsPipelineCreateInfo*         pCreateInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary,
        uint32_t                            rasterizationStream);

    VkResult CreateComputePipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        ComputePipelineCreateInfo*          pInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary);

    VkResult ConvertGraphicsPipelineInfo(
        Device*                             pDevice,
        const VkGraphicsPipelineCreateInfo* pIn,
        GraphicsPipelineCreateInfo*         pInfo,
        VbBindingInfo*                      pVbInfo);

    VkResult ConvertComputePipelineInfo(
        Device*                             pDevice,
        const VkComputePipelineCreateInfo*  pIn,
        ComputePipelineCreateInfo*          pInfo);

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

    template<class PipelineBuildInfo>
    PipelineCompilerType CheckCompilerType(const PipelineBuildInfo* pPipelineBuildInfo);

    uint32_t GetCompilerCollectionMask();

    static const char* GetShaderStageName(ShaderStage shaderStage);

    void ApplyDefaultShaderOptions(
        Llpc::PipelineShaderOptions* pShaderOptions
    ) const;

private:
    VkResult CreateLlpcCompiler();

    void ApplyProfileOptions(
        Device*                   pDevice,
        ShaderStage               stage,
        ShaderModule*             pShaderModule,
        Llpc::PipelineShaderInfo* pShaderInfo,
        PipelineOptimizerKey*     pProfileKey
    );

    template<class PipelineBuildInfo>
    bool ReplacePipelineBinary(
        const PipelineBuildInfo* pPipelineBuildInfo,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary);

    void DropPipelineBinaryInst(
        Device*                pDevice,
        const RuntimeSettings& settings,
        const void*            pPipelineBinary,
        size_t                 pipelineBinarySize);

    // -----------------------------------------------------------------------------------------------------------------

    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Llpc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by LLPC

    Llpc::ICompiler*    m_pLlpc;               // LLPC compiler object

#if ICD_BUILD_MULIT_COMPILER
    // Store the hash list read from file when enable
    // LlpcModeMixScpcHashList or LlpcModeMixLlpcHashList or LlpcModeMixRcpcHashList
    Util::HashSet<uint64_t, PalAllocator>            m_llpcModeMixHashList;
#endif

}; // class PipelineCompiler

} // namespce vk
