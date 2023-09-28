/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if VKI_RAY_TRACING
#include "gpurt/gpurt.h"
#endif

#include "include/compiler_solution_llpc.h"

#include "include/vk_shader_code.h"
#include "include/vk_conv.h"
#include "include/defer_compile_thread.h"

namespace vk
{

class PipelineLayout;
class PipelineCache;
class ShaderModule;
class PipelineCompiler;
struct PipelineInternalBufferInfo;
struct ShaderModuleHandle;

class PipelineBinaryCache;

// =====================================================================================================================
// The shader stages of Pre-Rasterization Shaders section
constexpr uint32_t PrsShaderMask =
    (0
    | (1 << ShaderStage::ShaderStageTask)
    | (1 << ShaderStage::ShaderStageVertex)
    | (1 << ShaderStage::ShaderStageTessControl)
    | (1 << ShaderStage::ShaderStageTessEval)
    | (1 << ShaderStage::ShaderStageGeometry)
    | (1 << ShaderStage::ShaderStageMesh)
    );

// =====================================================================================================================
// The shader stages of Fragment Shader (Post-Rasterization) section
constexpr uint32_t FgsShaderMask = (1 << ShaderStage::ShaderStageFragment);

// =====================================================================================================================
struct ShaderStageInfo
{
    ShaderStage                                        stage;
    const ShaderModuleHandle*                          pModuleHandle;
    Pal::ShaderHash                                    codeHash;            // This hash includes entry point info
    size_t                                             codeSize;
    const char*                                        pEntryPoint;
    VkPipelineShaderStageCreateFlags                   flags;
    const VkSpecializationInfo*                        pSpecializationInfo;
    size_t                                             waveSize;
};

// =====================================================================================================================
struct GraphicsPipelineShaderStageInfo
{
    ShaderStageInfo stages[ShaderStage::ShaderStageGfxCount];
};

// =====================================================================================================================
struct ComputePipelineShaderStageInfo
{
    ShaderStageInfo stage;
};

#if VKI_RAY_TRACING
// =====================================================================================================================
struct RayTracingPipelineShaderStageInfo
{
    uint32_t         stageCount;
    ShaderStageInfo* pStages;
};
#endif

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
        uint32_t                     expectedEntries,
        void*                        pShaderCacheMem,
        ShaderCache*                 pShaderCache);

    size_t GetShaderCacheSize(PipelineCompilerType cacheType);

    PipelineCompilerType GetShaderCacheType();

    PipelineBinaryCache* GetBinaryCache() const { return m_pBinaryCache; }

    void ApplyPipelineOptions(
        const Device*          pDevice,
        VkPipelineCreateFlags  flags,
        Vkgc::PipelineOptions* pOptions
    );

    VkResult BuildShaderModule(
        const Device*                   pDevice,
        const VkShaderModuleCreateFlags flags,
        const VkShaderModuleCreateFlags internalShaderFlags,
        size_t                          codeSize,
        const void*                     pCode,
        const bool                      adaptForFastLink,
        bool                            isInternal,
        PipelineBinaryCache*            pBinaryCache,
        PipelineCreationFeedback*       pFeedback,
        ShaderModuleHandle*             pShaderModule);

    void TryEarlyCompileShaderModule(
        const Device*       pDevice,
        ShaderModuleHandle* pModule);

    bool IsValidShaderModule(
        const ShaderModuleHandle* pShaderModule) const;

    void FreeShaderModule(
        ShaderModuleHandle* pShaderModule);

    VkResult CreateGraphicsPipelineBinary(
        Device*                           pDevice,
        uint32_t                          deviceIndex,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        const PipelineCreateFlags         flags,
        size_t*                           pPipelineBinarySize,
        const void**                      ppPipelineBinary,
        Util::MetroHash::Hash*            pCacheId);

    VkResult CreateGraphicsShaderBinary(
        const Device*                     pDevice,
        PipelineCache*                    pPipelineCache,
        const ShaderStage                 stage,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        ShaderModuleHandle*               pModule);

    VkResult CreateColorExportShaderLibrary(
        const Device*                     pDevice,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        const VkAllocationCallbacks*      pAllocator,
        Pal::IShaderLibrary**             ppColExpLib);

    VkResult CreateGraphicsShaderLibrary(
        const Device*                     pDevice,
        const Vkgc::BinaryData            shaderBinary,
        const VkAllocationCallbacks*      pAllocator,
        Pal::IShaderLibrary**             ppShaderLibrary);

    static void FreeGraphicsShaderBinary(
        ShaderModuleHandle*               pShaderModule);

    VkResult CreateComputePipelineBinary(
        Device*                           pDevice,
        uint32_t                          deviceIndex,
        PipelineCache*                    pPipelineCache,
        ComputePipelineBinaryCreateInfo*  pInfo,
        size_t*                           pPipelineBinarySize,
        const void**                      ppPipelineBinary,
        Util::MetroHash::Hash*            pCacheId);

    static void GetPipelineCreationFeedback(
        const VkStructHeader*                           pHeader,
        const VkPipelineCreationFeedbackCreateInfoEXT** ppPipelineCreationFeadbackCreateInfo);

    static void UpdatePipelineCreationFeedback(
        VkPipelineCreationFeedbackEXT*  pPipelineCreationFeedback,
        const PipelineCreationFeedback* pFeedbackFromCompiler);

    static VkResult SetPipelineCreationFeedbackInfo(
        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo,
        uint32_t                                       stageCount,
        const VkPipelineShaderStageCreateInfo*         pStages,
        const PipelineCreationFeedback*                pPipelineFeedback,
        const PipelineCreationFeedback*                pStageFeedback);

    VkResult ConvertGraphicsPipelineInfo(
        const Device*                                   pDevice,
        const VkGraphicsPipelineCreateInfo*             pIn,
        PipelineCreateFlags                             flags,
        const GraphicsPipelineShaderStageInfo*          pShaderInfo,
        const PipelineLayout*                           pPipelineLayout,
        PipelineOptimizerKey*                           pPipelineProfileKey,
        PipelineMetadata*                               pBinaryMetadata,
        GraphicsPipelineBinaryCreateInfo*               pCreateInfo);

    VkResult ConvertComputePipelineInfo(
        const Device*                                   pDevice,
        const VkComputePipelineCreateInfo*              pIn,
        const ComputePipelineShaderStageInfo*           pShaderInfo,
        const PipelineOptimizerKey*                     pPipelineProfileKey,
        PipelineMetadata*                               pBinaryMetadata,
        ComputePipelineBinaryCreateInfo*                pInfo,
        PipelineCreateFlags                             flags);

    void FreeComputePipelineBinary(
        ComputePipelineBinaryCreateInfo* pCreateInfo,
        const void*                      pPipelineBinary,
        size_t                           binarySize);

    void FreeGraphicsPipelineBinary(
        const PipelineCompilerType        compilerType,
        const FreeCompilerBinary          freeCompilerBinary,
        const void*                       pPipelineBinary,
        size_t                            binarySize);

    void FreeComputePipelineCreateInfo(ComputePipelineBinaryCreateInfo* pCreateInfo);

    void FreeGraphicsPipelineCreateInfo(GraphicsPipelineBinaryCreateInfo* pCreateInfo, bool keepConvertTempMem);

#if VKI_RAY_TRACING

    VkResult ConvertRayTracingPipelineInfo(
        const Device*                            pDevice,
        const VkRayTracingPipelineCreateInfoKHR* pIn,
        PipelineCreateFlags                      flags,
        const RayTracingPipelineShaderStageInfo* pShaderInfo,
        const PipelineOptimizerKey*              pPipelineProfileKey,
        RayTracingPipelineBinaryCreateInfo*      pCreateInfo);

    VkResult CreateRayTracingPipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIdx,
        PipelineCache*                      pPipelineCache,
        RayTracingPipelineBinaryCreateInfo* pCreateInfo,
        RayTracingPipelineBinary*           pPipelineBinary,
        Util::MetroHash::Hash*              pCacheId);

    void FreeRayTracingPipelineBinary(
        RayTracingPipelineBinaryCreateInfo* pCreateInfo,
        RayTracingPipelineBinary*           pPipelineBinary);

    void FreeRayTracingPipelineCreateInfo(RayTracingPipelineBinaryCreateInfo* pCreateInfo);

    void SetRayTracingState(
        const Device*                       pDevice,
        Vkgc::RtState*                      pRtState,
        uint32_t                            createFlags);

    void ExtractRayTracingPipelineBinary(
        Vkgc::BinaryData*                   pBinary,
        RayTracingPipelineBinary*           pPipelineBinary);

    bool BuildRayTracingPipelineBinary(
        const RayTracingPipelineBinary*     pPipelineBinary,
        Vkgc::BinaryData*                   pBinary);

#endif

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
        ShaderStage                      stage,
        VkPipelineShaderStageCreateFlags flags,
        Vkgc::PipelineShaderOptions*     pShaderOptions) const;

    Vkgc::GfxIpVersion& GetGfxIp() { return m_gfxIp; }

    void DestroyPipelineBinaryCache();

    void BuildPipelineInternalBufferData(
        const PipelineLayout*             pPipelineLayout,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo);

    void GetComputePipelineCacheId(
        uint32_t                         deviceIdx,
        ComputePipelineBinaryCreateInfo* pCreateInfo,
        uint64_t                         pipelineHash,
        const Util::MetroHash::Hash&     settingsHash,
        Util::MetroHash::Hash*           pCacheId);

    void GetGraphicsPipelineCacheId(
        uint32_t                          deviceIdx,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        uint64_t                          pipelineHash,
        const Util::MetroHash::Hash&      settingsHash,
        Util::MetroHash::Hash*            pCacheId);

    void GetColorExportShaderCacheId(
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        Util::MetroHash::Hash*            pCacheId);

#if VKI_RAY_TRACING
    void GetRayTracingPipelineCacheId(
        uint32_t                            deviceIdx,
        uint32_t                            numDevices,
        RayTracingPipelineBinaryCreateInfo* pCreateInfo,
        uint64_t                            pipelineHash,
        const Util::MetroHash::Hash&        settingsHash,
        Util::MetroHash::Hash*              pCacheId);
#endif

    static void BuildNggState(
        const Device*                     pDevice,
        const VkShaderStageFlagBits       activeStages,
        const bool                        isConservativeOverestimation,
        const bool                        unrestrictedPrimitiveTopology,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo);

    static void BuildPipelineShaderInfo(
        const Device*                                 pDevice,
        const ShaderStageInfo*                        pShaderInfoIn,
        Vkgc::PipelineShaderInfo*                     pShaderInfoOut,
        Vkgc::PipelineOptions*                        pPipelineOptions,
        const PipelineOptimizerKey*                   pOptimizerKey,
        Vkgc::NggState*                               pNggState
        );

    void ExecuteDeferCompile(
        DeferredCompileWorkload* pWorkload);

    Util::Result GetCachedPipelineBinary(
        const Util::MetroHash::Hash* pCacheId,
        const PipelineBinaryCache*   pPipelineBinaryCache,
        size_t*                      pPipelineBinarySize,
        const void**                 ppPipelineBinary,
        bool*                        pIsUserCacheHit,
        bool*                        pIsInternalCacheHit,
        FreeCompilerBinary*          pFreeCompilerBinary,
        PipelineCreationFeedback*    pPipelineFeedback);

    void CachePipelineBinary(
        const Util::MetroHash::Hash* pCacheId,
        PipelineBinaryCache*         pPipelineBinaryCache,
        size_t                       pipelineBinarySize,
        const void*                  pPipelineBinary,
        bool                         isUserCacheHit,
        bool                         isInternalCacheHit);

    template<class PipelineBuildInfo>
    bool ReplacePipelineBinary(
        const PipelineBuildInfo* pPipelineBuildInfo,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary,
        uint64_t                 hashCode64);

    static size_t GetMaxUberFetchShaderInternalDataSize();

    static size_t GetUberFetchShaderInternalDataSize(const VkPipelineVertexInputStateCreateInfo* pVertexInput);

    uint32_t BuildUberFetchShaderInternalData(
        uint32_t                                     vertexBindingDescriptionCount,
        const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
        uint32_t                                     vertexAttributeDescriptionCount,
        const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions,
        void*                                        pUberFetchShaderInternalData);

    uint32_t BuildUberFetchShaderInternalData(
        const VkPipelineVertexInputStateCreateInfo* pVertexInput,
        bool                                        dynamicStride,
        void*                                       pUberFetchShaderInternalData) const;

    static void ReadBinaryMetadata(
        const Device*     pDevice,
        const void*       pElfBinary,
        const size_t      binarySize,
        PipelineMetadata* pMetadata);

    static VkResult WriteBinaryMetadata(
        const Device*                     pDevice,
        PipelineCompilerType              compilerType,
        FreeCompilerBinary*               pFreeCompilerBinary,
        const void**                      ppElfBinary,
        size_t*                           pBinarySize,
        PipelineMetadata*                 pMetadata);

    static void DumpCacheMatrix(
        PhysicalDevice*             pPhysicalDevice,
        const char*                 pPrefixStr,
        uint32_t                    countHint,
        PipelineCompileCacheMatrix* pCacheMatrix);

    static void GetElfCacheMetricString(
        PipelineCompileCacheMatrix* pCacheMatrix,
        const char*                 pPrefixStr,
        char*                       pOutStr,
        size_t                      outStrSize);

    void GetElfCacheMetricString(char* pOutStr, size_t outStrSize)
    {
        GetElfCacheMetricString(&m_pipelineCacheMatrix, "", pOutStr, outStrSize);
    }
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineCompiler);

    static bool IsDefaultPipelineMetadata(
        const PipelineMetadata* pPipelineMetadata);

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

#if VKI_RAY_TRACING
    bool ReplaceRayTracingPipelineBinary(
        RayTracingPipelineBinaryCreateInfo* pCreateInfo,
        RayTracingPipelineBinary*           pPipelineBinary,
        uint64_t                            hashCode64);

    size_t GetRayTracingPipelineMetaSize(const RayTracingPipelineBinary* pPipelineBinary) const;
#endif

    VkResult LoadShaderModuleFromCache(
        const Device*                   pDevice,
        const VkShaderModuleCreateFlags flags,
        const uint32_t                  compilerMask,
        const Util::MetroHash::Hash&    uniqueHash,
        PipelineBinaryCache*            pBinaryCache,
        PipelineCreationFeedback*       pFeedback,
        ShaderModuleHandle*             pShaderModule);

    void StoreShaderModuleToCache(
        const Device*                   pDevice,
        const VkShaderModuleCreateFlags flags,
        const uint32_t                  compilerMask,
        const Util::MetroHash::Hash&    uniqueHash,
        PipelineBinaryCache*            pBinaryCache,
        ShaderModuleHandle*             pShaderModule);

    Util::MetroHash::Hash GetShaderModuleCacheHash(
        const VkShaderModuleCreateFlags flags,
        const uint32_t                  compilerMask,
        const Util::MetroHash::Hash&    uniqueHash);

    template<class VertexInputBinding, class VertexInputAttribute, class VertexInputDivisor>
    uint32_t BuildUberFetchShaderInternalDataImp(
        uint32_t                    vertexBindingDescriptionCount,
        const VertexInputBinding*   pVertexBindingDescriptions,
        uint32_t                    vertexAttributeDescriptionCount,
        const VertexInputAttribute* pVertexAttributeDescriptions,
        uint32_t                    vertexDivisorDescriptionCount,
        const VertexInputDivisor*   pVertexDivisorDescriptions,
        bool                        isDynamicStride,
        void*                       pUberFetchShaderInternalData) const;
    // -----------------------------------------------------------------------------------------------------------------

    typedef Util::HashMap<Util::MetroHash::Hash, ShaderModuleHandle, PalAllocator, Util::JenkinsHashFunc>
        ShaderModuleHandleMap;

    typedef Util::HashMap<Util::MetroHash::Hash, Pal::IShaderLibrary*, PalAllocator, Util::JenkinsHashFunc>
        ColorExportShaderMap;

    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Vkgc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by Vkgcf
    DeferCompileManager m_deferCompileMgr;     // Defer compile thread manager
    CompilerSolutionLlpc m_compilerSolutionLlpc;

    PipelineBinaryCache* m_pBinaryCache;       // Pipeline binary cache object

    // Compile statistic metrics
    PipelineCompileCacheMatrix     m_pipelineCacheMatrix;

    Util::Mutex                    m_cacheLock;

    UberFetchShaderFormatInfoMap   m_uberFetchShaderInfoFormatMap;  // Uber fetch shader format info map

    ShaderModuleHandleMap          m_shaderModuleHandleMap;

    ColorExportShaderMap           m_colorExportShaderMap;

}; // class PipelineCompiler

} // namespce vk
