/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
struct GraphicsPipelineLibraryInfo;
struct GraphicsPipelineExtStructs;

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
constexpr uint32_t VsFsStageMask     = (1 << ShaderStage::ShaderStageVertex) | (1 << ShaderStage::ShaderStageFragment);
constexpr uint32_t VsGsFsStageMask   = VsFsStageMask | (1 << ShaderStage::ShaderStageGeometry);
constexpr uint32_t VsTessFsStageMask = VsFsStageMask |
    (1 << ShaderStage::ShaderStageTessControl) | (1 << ShaderStage::ShaderStageTessEval);
constexpr uint32_t VsTessGsFsStageMask = VsTessFsStageMask | (1 << ShaderStage::ShaderStageGeometry);

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
/// Determines whether the given stage info is from shader module identifier.
inline bool IsShaderModuleIdentifier(const Vkgc::PipelineShaderInfo& stageInfo)
{
    return (stageInfo.pModuleData == nullptr) &&
        ((stageInfo.options.clientHash.lower != 0) ||
        (stageInfo.options.clientHash.upper != 0));
}

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
        const Device*             pDevice,
        VkPipelineCreateFlags2KHR flags,
        Vkgc::PipelineOptions*    pOptions
    );

    VkResult BuildShaderModule(
        const Device*                   pDevice,
        const VkShaderModuleCreateFlags flags,
        const VkShaderModuleCreateFlags internalShaderFlags,
        const Vkgc::BinaryData&         shaderBinary,
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
        const VkPipelineCreateFlags2KHR   flags,
        Vkgc::BinaryData*                 pPipelineBinary,
        Util::MetroHash::Hash*            pCacheId);

    VkResult CreateGraphicsShaderBinary(
        const Device*                     pDevice,
        PipelineCache*                    pPipelineCache,
        GraphicsLibraryType               gplType,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        GplModuleState*                   pModuleState);

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

    void FreeGplModuleState(
        GplModuleState* pModuleState);

    VkResult CreateComputePipelineBinary(
        Device*                           pDevice,
        uint32_t                          deviceIndex,
        PipelineCache*                    pPipelineCache,
        ComputePipelineBinaryCreateInfo*  pInfo,
        Vkgc::BinaryData*                 pPipelineBinary,
        Util::MetroHash::Hash*            pCacheId);

    static void InitPipelineCreationFeedback(
        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo);

    static void UpdatePipelineCreationFeedback(
        VkPipelineCreationFeedbackEXT*  pPipelineCreationFeedback,
        const PipelineCreationFeedback* pFeedbackFromCompiler);

    static VkResult SetPipelineCreationFeedbackInfo(
        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo,
        uint32_t                                       stageCount,
        const VkPipelineShaderStageCreateInfo*         pStages,
        const PipelineCreationFeedback*                pPipelineFeedback,
        const PipelineCreationFeedback*                pStageFeedback);

    VkResult ConvertGraphicsPipelineInfo(
        Device*                                         pDevice,
        const VkGraphicsPipelineCreateInfo*             pIn,
        const GraphicsPipelineExtStructs&               extStructs,
        const GraphicsPipelineLibraryInfo&              libInfo,
        VkPipelineCreateFlags2KHR                       flags,
        const GraphicsPipelineShaderStageInfo*          pShaderInfo,
        const PipelineLayout*                           pPipelineLayout,
        const PipelineOptimizerKey*                     pPipelineProfileKey,
        PipelineMetadata*                               pBinaryMetadata,
        GraphicsPipelineBinaryCreateInfo*               pCreateInfo);

    VkResult BuildGplFastLinkCreateInfo(
        Device*                                         pDevice,
        const VkGraphicsPipelineCreateInfo*             pIn,
        const GraphicsPipelineExtStructs&               extStructs,
        VkPipelineCreateFlags2KHR                       flags,
        const GraphicsPipelineLibraryInfo&              libInfo,
        const PipelineLayout*                           pPipelineLayout,
        PipelineMetadata*                               pBinaryMetadata,
        GraphicsPipelineBinaryCreateInfo*               pCreateInfo);

    VkResult ConvertComputePipelineInfo(
        const Device*                                   pDevice,
        const VkComputePipelineCreateInfo*              pIn,
        const ComputePipelineShaderStageInfo*           pShaderInfo,
        const PipelineOptimizerKey*                     pPipelineProfileKey,
        PipelineMetadata*                               pBinaryMetadata,
        ComputePipelineBinaryCreateInfo*                pInfo,
        VkPipelineCreateFlags2KHR                       flags);

    void FreeComputePipelineBinary(
        ComputePipelineBinaryCreateInfo* pCreateInfo,
        const Vkgc::BinaryData&          pipelineBinary);

    void FreeGraphicsPipelineBinary(
        const PipelineCompilerType        compilerType,
        const FreeCompilerBinary          freeCompilerBinary,
        const Vkgc::BinaryData&           pipelineBinary);

    void FreeComputePipelineCreateInfo(ComputePipelineBinaryCreateInfo* pCreateInfo);

    void FreeGraphicsPipelineCreateInfo(
        Device*                           pDevice,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo,
        bool                              keepConvertTempMem,
        bool                              keepInternalMem);

#if VKI_RAY_TRACING

    VkResult ConvertRayTracingPipelineInfo(
        const Device*                            pDevice,
        const VkRayTracingPipelineCreateInfoKHR* pIn,
        VkPipelineCreateFlags2KHR                flags,
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
    PipelineCompilerType CheckCompilerType(
        const PipelineBuildInfo* pPipelineBuildInfo,
        uint64_t                 preRasterHash,
        uint64_t                 fragmentHash);

    uint32_t GetCompilerCollectionMask();

    CompilerSolution* GetSolution(PipelineCompilerType type)
    {
        CompilerSolution* pSolution = nullptr;
        pSolution = &m_compilerSolutionLlpc;
        return pSolution;
    }

    void ApplyDefaultShaderOptions(
        ShaderStage                      stage,
        VkPipelineShaderStageCreateFlags flags,
        Vkgc::PipelineShaderOptions*     pShaderOptions) const;

    Vkgc::GfxIpVersion& GetGfxIp() { return m_gfxIp; }

    void DestroyPipelineBinaryCache();

    void BuildPipelineInternalBufferData(
        const PipelineLayout*             pPipelineLayout,
        bool                              needCache,
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
        Vkgc::BinaryData*            pPipelineBinary,
        bool*                        pIsUserCacheHit,
        bool*                        pIsInternalCacheHit,
        FreeCompilerBinary*          pFreeCompilerBinary,
        PipelineCreationFeedback*    pPipelineFeedback);

    void CachePipelineBinary(
        const Util::MetroHash::Hash* pCacheId,
        PipelineBinaryCache*         pPipelineBinaryCache,
        Vkgc::BinaryData*            pPipelineBinary,
        bool                         isUserCacheHit,
        bool                         isInternalCacheHit);

    template<class PipelineBuildInfo>
    static bool ReplacePipelineBinary(
        const PhysicalDevice*    pPhysicalDevice,
        const PipelineBuildInfo* pPipelineBuildInfo,
        Vkgc::BinaryData*        pPipelineBinary,
        uint64_t                 hashCode64);

    static size_t GetMaxUberFetchShaderInternalDataSize();

    static size_t GetUberFetchShaderInternalDataSize(const VkPipelineVertexInputStateCreateInfo* pVertexInput);

    uint32_t BuildUberFetchShaderInternalData(
        uint32_t                                     vertexBindingDescriptionCount,
        const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
        uint32_t                                     vertexAttributeDescriptionCount,
        const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions,
        void*                                        pUberFetchShaderInternalData,
        bool                                         isOffsetMode);

    uint32_t BuildUberFetchShaderInternalData(
        const VkPipelineVertexInputStateCreateInfo* pVertexInput,
        bool                                        dynamicStride,
        bool                                        isOffsetMode,
        void*                                       pUberFetchShaderInternalData) const;

    static void ReadBinaryMetadata(
        const Device*           pDevice,
        const Vkgc::BinaryData& elfBinary,
        PipelineMetadata*       pMetadata);

    static VkResult WriteBinaryMetadata(
        const Device*               pDevice,
        PipelineCompilerType        compilerType,
        FreeCompilerBinary*         pFreeCompilerBinary,
        Vkgc::BinaryData*           pElfBinary,
        PipelineMetadata*           pMetadata);

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
    static VkResult UploadInternalBufferData(
        Device*                           pDevice,
        GraphicsPipelineBinaryCreateInfo* pCreateInfo);

    static void DumpPipelineMetadata(
        void*                   pPipelineDumpHandle,
        const PipelineMetadata* pBinaryMetadata);

    void DumpPipeline(
        const RuntimeSettings&         settings,
        const Vkgc::PipelineBuildInfo& pipelineInfo,
        uint64_t                       apiPsoHash,
        uint32_t                       binaryCount,
        const Vkgc::BinaryData*        pElfBinary,
        VkResult                       result);

    static void InitPipelineDumpOption(
        Vkgc::PipelineDumpOptions* pDumpOptions,
        const RuntimeSettings&     settings,
        char*                      pBuffer,
        PipelineCompilerType       type);
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineCompiler);

    static bool IsDefaultPipelineMetadata(
        const PipelineMetadata* pPipelineMetadata);

    void DropPipelineBinaryInst(
        Device*                 pDevice,
        const RuntimeSettings&  settings,
        const Vkgc::BinaryData& pipelineBinary);

    void ReplacePipelineIsaCode(
        Device*                 pDevice,
        uint64_t                pipelineHash,
        uint32_t                pipelineIndex,
        const Vkgc::BinaryData& pipelineBinary);

    bool LoadReplaceShaderBinary(
        uint64_t          shaderHash,
        Vkgc::BinaryData* pBinary);

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
        const VkShaderModuleCreateFlags flags,
        const VkShaderModuleCreateFlags internalShaderFlags,
        const uint32_t                  compilerMask,
        const Util::MetroHash::Hash&    uniqueHash,
        ShaderModuleHandle*             pShaderModule);

    void StoreShaderModuleToCache(
        const VkShaderModuleCreateFlags flags,
        const VkShaderModuleCreateFlags internalShaderFlags,
        const uint32_t                  compilerMask,
        const Util::MetroHash::Hash&    uniqueHash,
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
        bool                        isOffsetMode,
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
