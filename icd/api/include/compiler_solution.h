/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
class DeferredHostOperation;

#if VKI_RAY_TRACING
struct DeferredWorkload;
#endif

enum FreeCompilerBinary : uint32_t
{
    FreeWithCompiler          = 0,
    FreeWithInstanceAllocator,
    DoNotFree
};

// Represents the result of PipelineCompiler::BuildShaderModule
struct ShaderModuleHandle
{
    uint32_t* pRefCount;
    void*            pLlpcShaderModule; // Shader module handle from LLPC
    Vkgc::BinaryData elfPackage;        // Generated ElfPacekage from LLPC
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
// Pipeline compile and cache statistic info.
struct PipelineCompileCacheMatrix
{
    uint32_t    cacheAttempts;      // Number of attempted cache loads
    uint32_t    cacheHits;          // Number of cache hits
    uint32_t    totalBinaries;      // Total number of binaries compiled or fetched
    int64_t     totalTimeSpent;     // Accumulation of time spent either loading or compiling pipeline
};

// =====================================================================================================================
// Information required by the VB table manager that is defined by the graphics pipeline
struct VbBindingInfo
{
    uint32_t bindingTableSize;
    uint32_t bindingCount;

    struct
    {
        uint32_t slot;
        uint32_t byteStride;
    } bindings[Pal::MaxVertexBuffers];
};

constexpr uint32_t MaxPipelineInternalBufferCount = 4;
struct InternalBufferEntry
{
    uint32_t userDataOffset;
    uint32_t bufferOffset;
};

struct PipelineInternalBufferInfo
{
    uint32_t            internalBufferCount;
    InternalBufferEntry internalBufferEntries[MaxPipelineInternalBufferCount];
    uint32_t            dataSize;
    void* pData;
};

// =====================================================================================================================
// Represents pipeline metadata included in the pipeline ELF.
struct PipelineMetadata
{
#if VKI_RAY_TRACING
    bool                       rayQueryUsed;
#endif
    bool                       pointSizeUsed;
    VbBindingInfo              vbInfo;
    bool                       enableEarlyCompile;
    bool                       enableUberFetchShader;
    PipelineInternalBufferInfo internalBufferInfo;
};

// =====================================================================================================================
struct GraphicsPipelineBinaryCreateInfo
{
    Vkgc::GraphicsPipelineBuildInfo        pipelineInfo;
    void*                                  pTempBuffer;
    void*                                  pMappingBuffer;
    size_t                                 mappingBufferSize;
    VkPipelineCreateFlags                  flags;
    VkFormat                               dbFormat;
    PipelineOptimizerKey*                  pPipelineProfileKey;
    PipelineCompilerType                   compilerType;
    bool                                   linkTimeOptimization;
    Vkgc::BinaryData                       earlyElfPackage[ShaderStage::ShaderStageGfxCount];
    FreeCompilerBinary                     freeCompilerBinary;
    PipelineCreationFeedback               pipelineFeedback;
    PipelineCreationFeedback               stageFeedback[ShaderStage::ShaderStageGfxCount];
    uint32_t                               rasterizationStream;
    VkGraphicsPipelineLibraryFlagsEXT      libFlags;    // These flags indicate the section(s) included in pipeline
                                                        // (library).  Including the sections in the referenced
                                                        // libraries.
    PipelineMetadata*                      pBinaryMetadata;
};

// =====================================================================================================================
struct ComputePipelineBinaryCreateInfo
{
    Vkgc::ComputePipelineBuildInfo         pipelineInfo;
    void*                                  pTempBuffer;
    void*                                  pMappingBuffer;
    size_t                                 mappingBufferSize;
    VkPipelineCreateFlags                  flags;
    const PipelineOptimizerKey*            pPipelineProfileKey;
    PipelineCompilerType                   compilerType;
    FreeCompilerBinary                     freeCompilerBinary;
    PipelineCreationFeedback               pipelineFeedback;
    PipelineCreationFeedback               stageFeedback;
    PipelineMetadata*                      pBinaryMetadata;
};

#if VKI_RAY_TRACING
// =====================================================================================================================
struct RayTracingPipelineBinaryCreateInfo
{
    Vkgc::RayTracingPipelineBuildInfo      pipelineInfo;
    void*                                  pTempBuffer;
    void*                                  pMappingBuffer;
    size_t                                 mappingBufferSize;
    VkPipelineCreateFlags                  flags;
    const PipelineOptimizerKey*            pPipelineProfileKey;
    PipelineCompilerType                   compilerType;
    FreeCompilerBinary                     freeCompilerBinary;
    PipelineCreationFeedback               pipelineFeedback;
    uint32_t                               maxPayloadSize;
    uint32_t                               maxAttributeSize;
    bool                                   allowShaderInlining;
    DeferredWorkload*                      pDeferredWorkload;
};

// =====================================================================================================================
struct RayTracingPipelineBinary
{
    uint32_t                            maxFunctionCallDepth;
    bool                                hasTraceRay;
    uint32_t                            pipelineBinCount;
    Vkgc::BinaryData*                   pPipelineBins;
    Vkgc::RayTracingShaderGroupHandle   shaderGroupHandle;
    Vkgc::RayTracingShaderPropertySet   shaderPropSet;
    void*                               pElfCache;
};
#endif

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
        uint32_t     expectedEntries,
        ShaderCache* pShaderCache) = 0;

    virtual VkResult BuildShaderModule(
        const Device*                pDevice,
        VkShaderModuleCreateFlags    flags,
        size_t                       codeSize,
        const void*                  pCode,
        const bool                   adaptForFastLink,
        bool                         isInternal,
        ShaderModuleHandle*          pShaderModule,
        const PipelineOptimizerKey&  profileKey) = 0;

    virtual void TryEarlyCompileShaderModule(
        const Device*       pDevice,
        ShaderModuleHandle* pShaderModule) = 0;

    virtual void FreeShaderModule(ShaderModuleHandle* pShaderModule) = 0;

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
        int64_t*                          pCompileTime) = 0;

    virtual VkResult CreateGraphicsShaderBinary(
        const Device*                     pDevice,
        const ShaderStage                 stage,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        void*                             pPipelineDumpHandle,
        ShaderModuleHandle*               pShaderModule) = 0;

    virtual VkResult CreateComputePipelineBinary(
        Device*                     pDevice,
        uint32_t                    deviceIdx,
        PipelineCache*              pPipelineCache,
        ComputePipelineBinaryCreateInfo*  pCreateInfo,
        size_t*                     pPipelineBinarySize,
        const void**                ppPipelineBinary,
        void*                       pPipelineDumpHandle,
        uint64_t                    pipelineHash,
        Util::MetroHash::Hash*      pCacheId,
        int64_t*                    pCompileTime) = 0;

    virtual void FreeGraphicsPipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) = 0;

    virtual void FreeComputePipelineBinary(
        const void*                 pPipelineBinary,
        size_t                      binarySize) = 0;

#if VKI_RAY_TRACING
    virtual VkResult CreateRayTracingPipelineBinary(
        Device*                        pDevice,
        uint32_t                       deviceIdx,
        PipelineCache*                 pPipelineCache,
        RayTracingPipelineBinaryCreateInfo*  pCreateInfo,
        RayTracingPipelineBinary*      pPipelineBinary,
        void*                          pPipelineDumpHandle,
        uint64_t                       pipelineHash,
        Util::MetroHash::Hash*         pCacheId,
        int64_t*                       pCompileTime) = 0;

    virtual void FreeRayTracingPipelineBinary(
        RayTracingPipelineBinary* pPipelineBinary) = 0;
#endif

    static void DisableNggCulling(Vkgc::NggState* pNggState);

#if VKI_RAY_TRACING
    static void UpdateRayTracingFunctionNames(const Device* pDevice, Vkgc::RtState* pRtState);
    uint32_t GetRayTracingVgprLimit(bool isIndirect);
#endif

protected:
    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Vkgc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by Vkgc
    Pal::GfxIpLevel    m_gfxIpLevel;           // Graphics IP level
    static const char* GetShaderStageName(ShaderStage shaderStage);
private:

#if VKI_RAY_TRACING
    static void SetRayTracingFunctionName(const char* pSrc, char* pDest);
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(CompilerSolution);
};

}

#endif
