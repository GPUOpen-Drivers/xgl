/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "devmode/devmode_mgr.h"

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline_binary.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_memory.h"
#include "include/vk_deferred_operation.h"
#include "include/vk_utils.h"

#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/ray_tracing_device.h"
#include "ray_tracing_util.h"

#include "palPipelineAbiReader.h"
#include "palShaderLibrary.h"
#include "palPipeline.h"
#include "palMetroHash.h"
#include "palVectorImpl.h"

#include "gpurt/gpurt.h"
#include "vkgcDefs.h"

namespace vk
{

// =====================================================================================================================
// Populates our internal ShaderGroupInfo structs from parameters passed down through the API
static void PopulateShaderGroupInfos(
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
    ShaderGroupInfo*                         pShaderGroupInfos,
    uint32_t                                 shaderGroupCount)
{
    uint32_t groupIdx = 0;

    for (; groupIdx < pCreateInfo->groupCount; ++groupIdx)
    {
        const VkRayTracingShaderGroupCreateInfoKHR& apiGroupInfo = pCreateInfo->pGroups[groupIdx];
        VkShaderStageFlags                          stages       = {};

        switch (apiGroupInfo.type)
        {
        case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
            VK_ASSERT(apiGroupInfo.generalShader != VK_SHADER_UNUSED_KHR);
            // m_stageList includes stages from API and its libs
            stages |= pCreateInfo->pStages[apiGroupInfo.generalShader].stage;
            break;
        case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
            VK_ASSERT(apiGroupInfo.intersectionShader != VK_SHADER_UNUSED_KHR);
            stages |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            [[fallthrough]];
        case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
            if (apiGroupInfo.closestHitShader != VK_SHADER_UNUSED_KHR)
            {
                stages |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            }
            if (apiGroupInfo.anyHitShader != VK_SHADER_UNUSED_KHR)
            {
                stages |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            }
            break;
        default:
            break;
        }

        pShaderGroupInfos[groupIdx].type   = apiGroupInfo.type;
        pShaderGroupInfos[groupIdx].stages = stages;
    }

    // Copy group infos from pipeline libraries being imported
    if (pCreateInfo->pLibraryInfo != nullptr)
    {
        for (uint32_t libraryIdx = 0; libraryIdx < pCreateInfo->pLibraryInfo->libraryCount; ++libraryIdx)
        {
            VkPipeline             libraryHandle      = pCreateInfo->pLibraryInfo->pLibraries[libraryIdx];
            RayTracingPipeline*    pLibrary           = RayTracingPipeline::ObjectFromHandle(libraryHandle);
            const ShaderGroupInfo* pLibraryGroupInfos = pLibrary->GetShaderGroupInfos();
            uint32_t               libraryGroupCount  = pLibrary->GetShaderGroupCount();

            memcpy(&pShaderGroupInfos[groupIdx], pLibraryGroupInfos, sizeof(ShaderGroupInfo) * libraryGroupCount);

            groupIdx += libraryGroupCount;
        }
    }

    VK_ASSERT(groupIdx == shaderGroupCount);
}

// =====================================================================================================================
// Generates a hash using the contents of a VkRayTracingShaderGroupCreateInfoKHR struct
static void GenerateHashFromRayTracingShaderGroupCreateInfo(
    const VkRayTracingShaderGroupCreateInfoKHR& desc,
    Util::MetroHash128*                         pHasher)
{
    pHasher->Update(desc.type);
    switch (desc.type)
    {
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
        pHasher->Update(desc.generalShader);
        break;
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
        pHasher->Update(desc.anyHitShader);
        pHasher->Update(desc.closestHitShader);
        break;
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
        pHasher->Update(desc.anyHitShader);
        pHasher->Update(desc.closestHitShader);
        pHasher->Update(desc.intersectionShader);
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    bool hasReplayHandle = (desc.pShaderGroupCaptureReplayHandle != nullptr);
    pHasher->Update(hasReplayHandle);
}

// =====================================================================================================================
// Generates a hash using the contents of a VkRayTracingPipelineInterfaceCreateInfoKHR struct
static void GenerateHashFromRayTracingPipelineInterfaceCreateInfo(
    const VkRayTracingPipelineInterfaceCreateInfoKHR& desc,
    Util::MetroHash128*                               pHasher)
{
    pHasher->Update(desc.maxPipelineRayPayloadSize);
    pHasher->Update(desc.maxPipelineRayHitAttributeSize);
}

// =====================================================================================================================
// Generates the API PSO hash using the contents of the VkRayTracingPipelineCreateInfoKHR struct
// Pipeline compilation affected by:
//     - pCreateInfo->flags
//     - pCreateInfo->stageCount
//     - pCreateInfo->pStages
//     - pCreateInfo->groupCount
//     - pCreateInfo->pGroups
//     - pCreateInfo->maxPipelineRayRecursionDepth
//     - pCreateInfo->layout
void RayTracingPipeline::BuildApiHash(
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
    VkPipelineCreateFlags2KHR                flags,
    Util::MetroHash::Hash*                   pElfHash,
    uint64_t*                                pApiHash)
{
    Util::MetroHash128 elfHasher = {};
    Util::MetroHash128 apiHasher = {};

    // Hash only flags needed for pipeline caching
    elfHasher.Update(GetCacheIdControlFlags(flags));

    // All flags (including ones not accounted for in the elf hash)
    apiHasher.Update(flags);

    elfHasher.Update(pCreateInfo->stageCount);
    for (uint32_t i = 0; i < pCreateInfo->stageCount; ++i)
    {
        GenerateHashFromShaderStageCreateInfo(pCreateInfo->pStages[i], &elfHasher);
    }

    elfHasher.Update(pCreateInfo->groupCount);
    for (uint32_t i = 0; i < pCreateInfo->groupCount; ++i)
    {
        GenerateHashFromRayTracingShaderGroupCreateInfo(pCreateInfo->pGroups[i], &elfHasher);
    }

    if (pCreateInfo->pLibraryInfo != nullptr)
    {
        elfHasher.Update(pCreateInfo->pLibraryInfo->libraryCount);
        for (uint32_t i = 0; i < pCreateInfo->pLibraryInfo->libraryCount; ++i)
        {
            auto pLibraryPipeline = RayTracingPipeline::ObjectFromHandle(pCreateInfo->pLibraryInfo->pLibraries[i]);
            elfHasher.Update(pLibraryPipeline->GetElfHash());
            apiHasher.Update(pLibraryPipeline->GetApiHash());
        }
    }

    const bool hasLibraries =
        ((pCreateInfo->pLibraryInfo != nullptr) && (pCreateInfo->pLibraryInfo->libraryCount > 0));
    const bool isLibrary    = Util::TestAnyFlagSet(flags, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);

    // pLibraryInterface must be populated (per spec) if the pipeline is a library or has libraries
    VK_ASSERT((pCreateInfo->pLibraryInterface != nullptr) || ((isLibrary || hasLibraries) == false));

    if (pCreateInfo->pLibraryInterface != nullptr)
    {
        GenerateHashFromRayTracingPipelineInterfaceCreateInfo(*pCreateInfo->pLibraryInterface, &elfHasher);
    }

    elfHasher.Update(pCreateInfo->maxPipelineRayRecursionDepth);
    elfHasher.Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());

    if (pCreateInfo->pDynamicState != nullptr)
    {
        GenerateHashFromDynamicStateCreateInfo(*pCreateInfo->pDynamicState, &apiHasher);
    }

    if (((flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) != 0) &&
        (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(RayTracingPipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);

    // Finalize ELF hash (and add it to the API hash)
    elfHasher.Finalize(reinterpret_cast<uint8_t*>(pElfHash));
    apiHasher.Update(*pElfHash);

    // Finalize API hash
    Util::MetroHash::Hash apiHashFull;
    apiHasher.Finalize(reinterpret_cast<uint8_t*>(&apiHashFull));
    *pApiHash = Util::MetroHash::Compact64(&apiHashFull);
}

// =====================================================================================================================
// Converts Vulkan ray tracing pipeline parameters to an internal structure
void RayTracingPipeline::ConvertRayTracingPipelineInfo(
    Device*                                  pDevice,
    const VkRayTracingPipelineCreateInfoKHR* pIn,
    VkPipelineCreateFlags2KHR                flags,
    CreateInfo*                              pOutInfo)
{
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);

    Pipeline::BuildPipelineResourceLayout(pDevice,
                                         PipelineLayout::ObjectFromHandle(pIn->layout),
                                         VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                         flags,
                                         &pOutInfo->resourceLayout);

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    pOutInfo->immedInfo.computeShaderInfo.maxWavesPerCu        = settings.maxWavesPerCu;
    pOutInfo->immedInfo.computeShaderInfo.maxThreadGroupsPerCu = settings.maxThreadGroupsPerCu;
    pOutInfo->immedInfo.computeShaderInfo.tgScheduleCountPerCu = settings.tgScheduleCountPerCu;
}

// =====================================================================================================================
PipelineImplCreateInfo::PipelineImplCreateInfo(
    Device* const   pDevice)
    : m_stageCount(0),
      m_totalStageCount(0),
      m_stageList(pDevice->VkInstance()->Allocator()),
      m_groupCount(0),
      m_totalGroupCount(0),
      m_groupList(pDevice->VkInstance()->Allocator()),
      m_maxRecursionDepth(0)
{
}

// =====================================================================================================================
PipelineImplCreateInfo::~PipelineImplCreateInfo()
{
    m_stageList.Clear();
    m_groupList.Clear();
}

// =====================================================================================================================
void PipelineImplCreateInfo::AddToStageList(
    const VkPipelineShaderStageCreateInfo& stageInfo)
{
    m_stageList.PushBack(stageInfo);
}

// =====================================================================================================================
void PipelineImplCreateInfo::AddToGroupList(
    const VkRayTracingShaderGroupCreateInfoKHR& groupInfo)
{
    m_groupList.PushBack(groupInfo);
}

// =====================================================================================================================
RayTracingPipeline::RayTracingPipeline(
    Device* const   pDevice)
    :
    Pipeline(
        pDevice,
        true,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR),
    m_info(),
    m_pShaderOptKeys(nullptr),
    m_shaderGroupCount(0),
    m_pShaderGroupInfos(nullptr),
    m_nativeShaderCount(0),
    m_totalShaderCount(0),
    m_attributeSize(0),
    m_shaderLibraryCount(0),
    m_ppShaderLibraries(nullptr),
    m_createInfo(pDevice),
    m_hasTraceRay(false),
    m_isCps(false),
    m_elfHash{},
    m_captureReplayVaMappingBufferInfo{},
    m_pShaderProperty(nullptr),
    m_compiledShaderCount(0),
    m_shaderStageDataList
    {
        pDevice->VkInstance()->Allocator(),
#if VKI_BUILD_MAX_NUM_GPUS > 1
        pDevice->VkInstance()->Allocator(),
        pDevice->VkInstance()->Allocator(),
        pDevice->VkInstance()->Allocator()
#endif
    },
    m_shaderStageDataCount(0),
    m_totalShaderLibraryList
    {
        pDevice->VkInstance()->Allocator(),
#if VKI_BUILD_MAX_NUM_GPUS > 1
        pDevice->VkInstance()->Allocator(),
        pDevice->VkInstance()->Allocator(),
        pDevice->VkInstance()->Allocator()
#endif
    }
{
    memset(m_pShaderGroupHandles, 0, sizeof(Vkgc::RayTracingShaderIdentifier*) * MaxPalDevices);
    memset(m_pShaderGroupStackSizes, 0, sizeof(ShaderGroupStackSizes*) * MaxPalDevices);
    memset(m_traceRayGpuVas, 0, sizeof(gpusize) * MaxPalDevices);
    memset(m_defaultPipelineStackSizes, 0, sizeof(Pal::CompilerStackSizes) * MaxPalDevices);
    memset(m_librarySummary, 0, sizeof(BinaryData) * MaxPalDevices);
}

// =====================================================================================================================
void RayTracingPipeline::Init(
    Pal::IPipeline**                     ppPalPipeline,
    uint32_t                             shaderLibraryCount,
    Pal::IShaderLibrary**                ppPalShaderLibrary,
    const UserDataLayout*                pLayout,
    PipelineBinaryStorage*               pBinaryStorage,
    const ShaderOptimizerKey*            pShaderOptKeys,
    const ImmedInfo&                     immedInfo,
    uint64_t                             staticStateMask,
    uint32_t                             nativeShaderCount,
    uint32_t                             totalShaderCount,
    uint32_t                             shaderGroupCount,
    Vkgc::RayTracingShaderIdentifier*    pShaderGroupHandles[MaxPalDevices],
    ShaderGroupStackSizes*               pShaderGroupStackSizes[MaxPalDevices],
    ShaderGroupInfo*                     pShaderGroupInfos,
    const BinaryData*                    pLibrarySummary,
    uint32_t                             attributeSize,
    gpusize                              traceRayGpuVas[MaxPalDevices],
    uint32_t                             dispatchRaysUserDataOffset,
    const Util::MetroHash::Hash&         cacheHash,
    uint64_t                             apiHash,
    const Util::MetroHash::Hash&         elfHash)
{
    Pipeline::Init(
        ppPalPipeline,
        pLayout,
        pBinaryStorage,
        staticStateMask,
        dispatchRaysUserDataOffset,
        cacheHash,
        apiHash);

    m_info               = immedInfo;
    m_pShaderOptKeys     = pShaderOptKeys;
    m_attributeSize      = attributeSize;
    m_nativeShaderCount  = nativeShaderCount;
    m_totalShaderCount   = totalShaderCount;
    m_shaderGroupCount   = shaderGroupCount;
    m_shaderLibraryCount = shaderLibraryCount;
    m_ppShaderLibraries  = ppPalShaderLibrary;
    m_pShaderGroupInfos  = pShaderGroupInfos;
    m_elfHash            = elfHash;

    memcpy(m_pShaderGroupHandles, pShaderGroupHandles, sizeof(Vkgc::RayTracingShaderIdentifier*) * MaxPalDevices);
    memcpy(m_pShaderGroupStackSizes, pShaderGroupStackSizes, sizeof(ShaderGroupStackSizes*) * MaxPalDevices);
    memcpy(m_traceRayGpuVas, traceRayGpuVas, sizeof(gpusize) * MaxPalDevices);
    memcpy(m_librarySummary, pLibrarySummary, sizeof(BinaryData) * MaxPalDevices);
}

// =====================================================================================================================
VkResult RayTracingPipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    if (m_pShaderProperty != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, m_pShaderProperty);
    }

    for (uint32_t i = 0; i < m_shaderLibraryCount; ++i)
    {
        m_ppShaderLibraries[i]->Destroy();
    }

    //Free the created shader groups
    if (m_shaderGroupCount > 0)
    {
        if (m_pShaderGroupHandles[0] != nullptr)
        {
            pAllocator->pfnFree(pAllocator->pUserData, m_pShaderGroupHandles[0]);
        }
    }

    if (m_librarySummary[0].pCode != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, const_cast<void*>(m_librarySummary[0].pCode));
    }

    if (m_captureReplayVaMappingBufferInfo.pData != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, m_captureReplayVaMappingBufferInfo.pData);
    }

    // This memory chunk contains the shader libraries and Pal::IPipeline objects. It should be destroyed after
    // Pipeline::Destroy is called.
    void* pShaderLibMem = m_ppShaderLibraries;

    VkResult result = Pipeline::Destroy(pDevice, pAllocator);

    if (pShaderLibMem != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pShaderLibMem);
    }

    return result;
}

// =====================================================================================================================
// Create a ray tracing pipeline object.
VkResult RayTracingPipeline::CreateImpl(
    PipelineCache*                           pPipelineCache,
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
    VkPipelineCreateFlags2KHR                flags,
    const VkAllocationCallbacks*             pAllocator,
    DeferredWorkload*                        pDeferredWorkload)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    // Setup PAL create info from Vulkan inputs
    CreateInfo                         localPipelineInfo      = {};
    RayTracingPipelineBinaryCreateInfo binaryCreateInfo       = {};
    VkResult                           result                 = VkResult::VK_SUCCESS;
    const RuntimeSettings&             settings               = m_pDevice->GetRuntimeSettings();
    RayTracingPipelineExtStructs       extStructs             = {};
    VkPipelineRobustnessCreateInfoEXT  pipelineRobustness     = {};

    HandleExtensionStructs(pCreateInfo, &extStructs);

    bool usePipelineRobustness = InitPipelineRobustness(
        extStructs.pPipelineRobustnessCreateInfoEXT,
        &pipelineRobustness);

    // If VkPipelineRobustnessCreateInfoEXT is specified for both a pipeline and a shader stage,
    // the VkPipelineRobustnessCreateInfoEXT specified for the shader stage will take precedence.
    for (uint32_t stageIdx = 0; stageIdx < pCreateInfo->stageCount; ++stageIdx)
    {
        PipelineShaderStageExtStructs shaderStageExtStructs = {};
        HandleShaderStageExtensionStructs(pCreateInfo->pStages[stageIdx].pNext, &shaderStageExtStructs);

        if (shaderStageExtStructs.pPipelineRobustnessCreateInfoEXT != nullptr)
        {
            UpdatePipelineRobustness(shaderStageExtStructs.pPipelineRobustnessCreateInfoEXT, &pipelineRobustness);
            usePipelineRobustness = true;
        }
    }

    if (usePipelineRobustness)
    {
        extStructs.pPipelineRobustnessCreateInfoEXT =
            static_cast<const VkPipelineRobustnessCreateInfoEXT*>(&pipelineRobustness);
    }

    UpdatePipelineImplCreateInfo(pCreateInfo);

    if ((Util::TestAnyFlagSet(flags, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR)) &&
        (settings.rtEnableCompilePipelineLibrary == false))
    {
        // The 1st attempt is to keep all library create info during library creation time,
        // and append the stage / groups to main pipeline.
        //
        // ToDo: Revisit this implementation to either compile library into indirect call,
        // or implement as a "mixed mode".
        result = VkResult::VK_SUCCESS;
    }
    else
    {
        // Possible there might be pipeline library integrated.
        // Repack createInfo together before compile and linking.
        const VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo =
        {
            pCreateInfo->sType,
            pCreateInfo->pNext,
            pCreateInfo->flags,
            m_createInfo.GetStageCount(),
            m_createInfo.GetStageList().Data(),
            m_createInfo.GetGroupCount(),
            m_createInfo.GetGroupList().Data(),
            m_createInfo.GetMaxRecursionDepth(),
            pCreateInfo->pLibraryInfo,
            pCreateInfo->pLibraryInterface,
            pCreateInfo->pDynamicState,
            pCreateInfo->layout,
            pCreateInfo->basePipelineHandle,
            pCreateInfo->basePipelineIndex
        };

        ConvertRayTracingPipelineInfo(m_pDevice,
                                      &pipelineCreateInfo,
                                      flags,
                                      &localPipelineInfo);

        // If rtEnableCompilePipelineLibrary is false, the library shaders have been included in pCreateInfo.
        const bool hasLibraries = settings.rtEnableCompilePipelineLibrary &&
            ((pCreateInfo->pLibraryInfo != nullptr) && (pCreateInfo->pLibraryInfo->libraryCount > 0));

        PipelineCompiler* pDefaultCompiler = m_pDevice->GetCompiler(DefaultDeviceIndex);

        binaryCreateInfo.pDeferredWorkload = pDeferredWorkload;

        auto pPipelineCreationFeedbackCreateInfo = extStructs.pPipelineCreationFeedbackCreateInfoEXT;

        PipelineCompiler::InitPipelineCreationFeedback(pPipelineCreationFeedbackCreateInfo);
        bool                     binariesProvided                = false;
        Util::MetroHash::Hash    cacheId[MaxPalDevices]          = {};
        Vkgc::BinaryData         providedBinaries[MaxPalDevices] = {};

        auto pPipelineBinaryInfoKHR = extStructs.pPipelineBinaryInfoKHR;

        if (pPipelineBinaryInfoKHR != nullptr)
        {
            if (pPipelineBinaryInfoKHR->binaryCount > 0)
            {
                VK_ASSERT(pPipelineBinaryInfoKHR->binaryCount == m_pDevice->NumPalDevices());
                binariesProvided = true;
            }

            for (uint32_t binaryIndex = 0;
                (binaryIndex < pPipelineBinaryInfoKHR->binaryCount) && (result == VK_SUCCESS);
                ++binaryIndex)
            {
                const auto pBinary = PipelineBinary::ObjectFromHandle(
                    pPipelineBinaryInfoKHR->pPipelineBinaries[binaryIndex]);

                cacheId[binaryIndex]          = pBinary->BinaryKey();
                providedBinaries[binaryIndex] = pBinary->BinaryData();
            }
        }

        RayTracingPipelineShaderStageInfo shaderInfo        = {};
        PipelineOptimizerKey              optimizerKey      = {};
        ShaderModuleHandle*               pTempModules      = nullptr;
        void*                             pShaderTempBuffer = nullptr;

        const uint32_t nativeShaderCount = pCreateInfo->stageCount;
        uint32_t       totalShaderCount  = nativeShaderCount;

        if (hasLibraries)
        {
            for (uint32_t libraryIdx = 0; libraryIdx < pCreateInfo->pLibraryInfo->libraryCount; ++libraryIdx)
            {
                auto pLibrary = RayTracingPipeline::ObjectFromHandle(
                    pCreateInfo->pLibraryInfo->pLibraries[libraryIdx]);

                VK_ASSERT(pLibrary->GetType() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

                totalShaderCount += pLibrary->GetTotalShaderCount();
            }
        }

        if (totalShaderCount > 0)
        {
            auto placement = utils::PlacementHelper<3>(
                nullptr,
                utils::PlacementElement<ShaderStageInfo>   {&shaderInfo.pStages,    nativeShaderCount},
                utils::PlacementElement<ShaderModuleHandle>{&pTempModules,          nativeShaderCount},
                utils::PlacementElement<ShaderOptimizerKey>{&optimizerKey.pShaders, totalShaderCount});

            pShaderTempBuffer = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                placement.SizeOf(),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pShaderTempBuffer != nullptr)
            {
                memset(pShaderTempBuffer, 0, placement.SizeOf());
                placement.FixupPtrs(pShaderTempBuffer);

                shaderInfo.stageCount = nativeShaderCount;
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        // Allocate buffer for shader groups
        uint32_t pipelineLibGroupCount = 0;

        if (hasLibraries)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
            {
                for (uint32_t libIdx = 0; libIdx < pCreateInfo->pLibraryInfo->libraryCount; ++libIdx)
                {
                    VkPipeline          pLibraries   = pCreateInfo->pLibraryInfo->pLibraries[libIdx];
                    RayTracingPipeline* pPipelineLib = RayTracingPipeline::ObjectFromHandle(pLibraries);

                    if (pPipelineLib == nullptr)
                    {
                        continue;
                    }

                    if (pPipelineLib->GetShaderLibraryCount() > 0)
                    {
                        const auto pImportedShaderLibrary = pPipelineLib->PalShaderLibrary(deviceIdx);
                        const auto pImportedFuncInfoList  = pImportedShaderLibrary->GetShaderLibFunctionInfos().Data();
                        const auto numFunctions           = pImportedShaderLibrary->
                                                            GetShaderLibFunctionInfos().NumElements();

                        // We only use one shader library per collection function
                        VK_ASSERT((pImportedFuncInfoList != nullptr) && (numFunctions == 1));
                    }

                    // update group count
                    pipelineLibGroupCount += pPipelineLib->GetShaderGroupCount();
                }
            }
        }

        const uint32_t totalGroupCount = pipelineCreateInfo.groupCount + pipelineLibGroupCount;

        RayTracingPipelineBinary          pipelineBinaries[MaxPalDevices] = {};
        Vkgc::RayTracingShaderIdentifier* pShaderGroups [MaxPalDevices] = {};
        BinaryData                        librarySummaries[MaxPalDevices] = {};

        if (totalGroupCount > 0)
        {
            const size_t shaderGroupArraySize = totalGroupCount * GpuRt::RayTraceShaderIdentifierByteSize;

            pShaderGroups[0] = static_cast<Vkgc::RayTracingShaderIdentifier*>(
                pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    shaderGroupArraySize * m_pDevice->NumPalDevices(),
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
            memset(pShaderGroups[0], 0, shaderGroupArraySize * m_pDevice->NumPalDevices());

            if (pShaderGroups[0] == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                if (pipelineCreateInfo.groupCount > 0)
                {
                    pipelineBinaries[0].shaderGroupHandle.shaderHandles     = pShaderGroups[0];
                    pipelineBinaries[0].shaderGroupHandle.shaderHandleCount = pipelineCreateInfo.groupCount;
                }

                for (uint32_t deviceIdx = 1; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
                {
                    pShaderGroups[deviceIdx] = pShaderGroups[deviceIdx - 1] + totalGroupCount;
                    if (pipelineCreateInfo.groupCount > 0)
                    {
                        pipelineBinaries[deviceIdx].shaderGroupHandle.shaderHandles     = pShaderGroups[deviceIdx];
                        pipelineBinaries[deviceIdx].shaderGroupHandle.shaderHandleCount = pipelineCreateInfo.groupCount;
                    }
                }
            }
        }

        const uint32_t                  maxFunctionCount  = pipelineCreateInfo.stageCount + 1;
        Pal::ShaderLibraryFunctionInfo* pIndirectFuncInfo = nullptr;
        uint32_t*                       pShaderNameMap    = nullptr;
        VkDeviceSize*                   pShaderStackSize  = nullptr;
        bool*                           pTraceRayUsage    = nullptr;
        void*                           pTempBuffer       = nullptr;

        if (result == VK_SUCCESS)
        {
            // Allocate temp buffer for shader name and indirect functions
            const uint32_t maxPipelineBinaryCount = maxFunctionCount + 1;

            auto placement = utils::PlacementHelper<6>(
                nullptr,

                utils::PlacementElement<Vkgc::RayTracingShaderProperty>{
                    &pipelineBinaries[0].shaderPropSet.shaderProps,
                    maxFunctionCount * m_pDevice->NumPalDevices()},

                utils::PlacementElement<Vkgc::BinaryData>{
                    &pipelineBinaries[0].pPipelineBins,
                    maxPipelineBinaryCount * m_pDevice->NumPalDevices()},

                utils::PlacementElement<Pal::ShaderLibraryFunctionInfo>{&pIndirectFuncInfo, maxFunctionCount},
                utils::PlacementElement<uint32_t>                      {&pShaderNameMap,    maxFunctionCount},
                utils::PlacementElement<VkDeviceSize>                  {&pShaderStackSize,  maxFunctionCount},
                utils::PlacementElement<bool>                          {&pTraceRayUsage,    maxFunctionCount});

            pTempBuffer = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                placement.SizeOf(),
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pTempBuffer != nullptr)
            {
                memset(pTempBuffer, 0, placement.SizeOf());
                placement.FixupPtrs(pTempBuffer);

                pipelineBinaries[0].shaderPropSet.shaderCount = maxFunctionCount;
                pipelineBinaries[0].pipelineBinCount          = maxPipelineBinaryCount;

                for (uint32_t deviceIdx = 1; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
                {
                    const auto  pBinary    = &pipelineBinaries[deviceIdx];
                    const auto& prevBinary = pipelineBinaries[deviceIdx - 1];

                    pBinary->pipelineBinCount = maxPipelineBinaryCount;
                    pBinary->pPipelineBins    = prevBinary.pPipelineBins + maxPipelineBinaryCount;

                    pBinary->shaderPropSet.shaderCount = maxFunctionCount;
                    pBinary->shaderPropSet.shaderProps = prevBinary.shaderPropSet.shaderProps + maxFunctionCount;
                }
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
        const auto pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                      : nullptr;

        // Build API and ELF hashes
        Util::MetroHash::Hash elfHash    = {};
        uint64_t              apiPsoHash = {};

        if (result == VK_SUCCESS)
        {
            optimizerKey.shaderCount = totalShaderCount;

            if (binariesProvided == false)
            {
                result = CreateCacheId(
                    m_pDevice,
                    pCreateInfo,
                    flags,
                    hasLibraries,
                    &shaderInfo,
                    &optimizerKey,
                    &apiPsoHash,
                    &elfHash,
                    pTempModules,
                    cacheId);

                binaryCreateInfo.apiPsoHash = apiPsoHash;
            }
        }

        bool                  storeBinaryToPipeline = false;
        bool                  storeBinaryToCache    = true;
        PipelineBinaryStorage binaryStorage         = {};

        storeBinaryToPipeline = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) != 0;
        storeBinaryToCache    = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) == 0;

        for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < m_pDevice->NumPalDevices()); deviceIdx++)
        {
            bool isUserCacheHit     = false;
            bool isInternalCacheHit = false;

            // PAL Pipeline caching
            Util::Result cacheResult = Util::Result::NotFound;

            bool forceCompilation = false;
            if (forceCompilation == false)
            {
                Vkgc::BinaryData cachedBinData = {};

                if (binariesProvided == false)
                {
                    // Search the pipeline binary cache.
                    cacheResult = pDefaultCompiler->GetCachedPipelineBinary(
                        &cacheId[deviceIdx],
                        pPipelineBinaryCache,
                        &cachedBinData,
                        &isUserCacheHit,
                        &isInternalCacheHit,
                        &binaryCreateInfo.freeCompilerBinary,
                        &binaryCreateInfo.pipelineFeedback);

                    // Found the pipeline; Add it to any cache layers where it's missing.
                    if (cacheResult == Util::Result::Success)
                    {
                        if (storeBinaryToCache)
                        {
                            m_pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                                &cacheId[deviceIdx],
                                pPipelineBinaryCache,
                                &cachedBinData,
                                isUserCacheHit,
                                isInternalCacheHit);
                        }

                        if (storeBinaryToPipeline)
                        {
                            // Store single packed blob of binaries from cache instead of separate binaries.
                            void* pMemory = pAllocator->pfnAllocation(
                                pAllocator->pUserData,
                                cachedBinData.codeSize,
                                VK_DEFAULT_MEM_ALIGN,
                                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);     // retained in the pipeline object

                            if (pMemory != nullptr)
                            {
                                memcpy(
                                    pMemory,
                                    cachedBinData.pCode,
                                    cachedBinData.codeSize);

                                InsertBinaryData(
                                    &binaryStorage,
                                    deviceIdx,
                                    cacheId[deviceIdx],
                                    cachedBinData.codeSize,
                                    pMemory);
                            }
                            else
                            {
                                result = VK_ERROR_OUT_OF_HOST_MEMORY;
                            }
                        }
                    }
                }
                else
                {
                    cachedBinData = providedBinaries[deviceIdx];
                    cacheResult   = Util::Result::Success;
                }

                if (cacheResult == Util::Result::Success)
                {
                    // Unpack the cached blob into separate binaries.
                    pDefaultCompiler->ExtractRayTracingPipelineBinary(
                        &cachedBinData,
                        &pipelineBinaries[deviceIdx]);
                }
            }

            if (cacheResult != Util::Result::Success)
            {
                if ((settings.ignoreFlagFailOnPipelineCompileRequired == false) &&
                    (flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR))
                {
                    result = VK_PIPELINE_COMPILE_REQUIRED_EXT;
                }
            }

            bool shouldConvert = (pCreateInfo != nullptr) &&
                (settings.enablePipelineDump ||
                 (cacheResult != Util::Result::Success));

            VkResult convertResult = VK_ERROR_UNKNOWN;
            if (shouldConvert)
            {
                convertResult = pDefaultCompiler->ConvertRayTracingPipelineInfo(
                    m_pDevice,
                    &pipelineCreateInfo,
                    extStructs,
                    flags,
                    &shaderInfo,
                    &localPipelineInfo.resourceLayout,
                    &optimizerKey,
                    &binaryCreateInfo);
                result = (result == VK_SUCCESS) ? convertResult : result;
            }

            if ((result == VK_SUCCESS) &&
                (convertResult == VK_SUCCESS) &&
                (cacheResult != Util::Result::Success))
            {
                for (uint32_t i = 0; i < binaryCreateInfo.pipelineInfo.shaderCount; i++)
                {
                    if (IsShaderModuleIdentifier(binaryCreateInfo.pipelineInfo.pShaders[i]))
                    {
                        result = VK_ERROR_UNKNOWN;
                        break;
                    }
                }
            }

            if (settings.enablePipelineDump && (convertResult == VK_SUCCESS))
            {
                if ((cacheResult == Util::Result::Success) || (result != VK_SUCCESS))
                {
                    Vkgc::PipelineBuildInfo pipelineInfo = {};
                    pipelineInfo.pRayTracingInfo = &binaryCreateInfo.pipelineInfo;
                    pDefaultCompiler->DumpPipeline(
                        m_pDevice->GetRuntimeSettings(),
                        pipelineInfo,
                        binaryCreateInfo.apiPsoHash,
                        pipelineBinaries[deviceIdx].pipelineBinCount,
                        pipelineBinaries[deviceIdx].pPipelineBins,
                        result);
                }
            }

            // Compile if unable to retrieve from cache.
            if ((result == VK_SUCCESS) && (cacheResult != Util::Result::Success))
            {
                result = pDefaultCompiler->CreateRayTracingPipelineBinary(
                    m_pDevice,
                    deviceIdx,
                    pPipelineCache,
                    &binaryCreateInfo,
                    &pipelineBinaries[deviceIdx],
                    &cacheId[deviceIdx]);

                // Add the pipeline to any cache layer where it's missing.
                if (result == VK_SUCCESS)
                {
                    Vkgc::BinaryData cachedBinData = {};

                    // Join the binaries into a single blob.
                    pDefaultCompiler->BuildRayTracingPipelineBinary(
                        &pipelineBinaries[deviceIdx],
                        &cachedBinData);

                    if (cachedBinData.pCode != nullptr)
                    {
                        if (storeBinaryToCache)
                        {
                            m_pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                                &cacheId[deviceIdx],
                                pPipelineBinaryCache,
                                &cachedBinData,
                                isUserCacheHit,
                                isInternalCacheHit);
                        }

                        if (storeBinaryToPipeline)
                        {
                            // Store compiled binaries packed into a single blob instead of separately.
                            void* pMemory = pAllocator->pfnAllocation(
                                pAllocator->pUserData,
                                cachedBinData.codeSize,
                                VK_DEFAULT_MEM_ALIGN,
                                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);     // retained in the pipeline object

                            if (pMemory != nullptr)
                            {
                                memcpy(
                                    pMemory,
                                    cachedBinData.pCode,
                                    cachedBinData.codeSize);

                                InsertBinaryData(
                                    &binaryStorage,
                                    deviceIdx,
                                    cacheId[deviceIdx],
                                    cachedBinData.codeSize,
                                    pMemory);
                            }
                            else
                            {
                                result = VK_ERROR_OUT_OF_HOST_MEMORY;
                            }
                        }
                        else
                        {
                            m_pDevice->VkInstance()->FreeMem(const_cast<void*>(cachedBinData.pCode));
                        }
                    }
                }
            }

            // insert spirvs into the pipeline binary
            if ((result == VK_SUCCESS) && (pDefaultCompiler->IsEmbeddingSpirvRequired()))
            {
                result = pDefaultCompiler->InsertSpirvsInRtPipeline(
                    m_pDevice,
                    pCreateInfo->pStages,
                    pCreateInfo->stageCount,
                    &binaryCreateInfo,
                    &pipelineBinaries[deviceIdx]);
            }

            if (totalGroupCount > 0)
            {
                // Copy shader groups if compiler doesn't use pre-allocated buffer.
                const auto& groupHandle = pipelineBinaries[deviceIdx].shaderGroupHandle;
                if (groupHandle.shaderHandles != pShaderGroups[deviceIdx])
                {
                    memcpy(
                        pShaderGroups[deviceIdx],
                        groupHandle.shaderHandles,
                        sizeof(Vkgc::RayTracingShaderIdentifier) * groupHandle.shaderHandleCount);
                }
            }
        }

        m_hasTraceRay = pipelineBinaries[DefaultDeviceIndex].hasTraceRay;
        m_isCps = pipelineBinaries[DefaultDeviceIndex].isCps;
        bool hasKernelEntry = pipelineBinaries[DefaultDeviceIndex].hasKernelEntry;

        uint32_t funcCount = 0;
        if (result == VK_SUCCESS)
        {
            const auto pShaderProp = &pipelineBinaries[DefaultDeviceIndex].shaderPropSet.shaderProps[0];
            const uint32_t shaderCount = pipelineBinaries[DefaultDeviceIndex].shaderPropSet.shaderCount;
            for (uint32_t i = 0; i < shaderCount; i++)
            {
                if (pShaderProp[i].shaderId != RayTracingInvalidShaderId)
                {
                    ++funcCount;
                }
            }

            // Inline pipelines have only one shader. Indirect pipelines have +1 for launch shader
            m_compiledShaderCount = 1 + funcCount;
            m_pShaderProperty = static_cast<Vkgc::RayTracingShaderProperty*>(pAllocator->pfnAllocation(
                                                    pAllocator->pUserData,
                                                    sizeof(Vkgc::RayTracingShaderProperty) * m_compiledShaderCount,
                                                    VK_DEFAULT_MEM_ALIGN,
                                                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
            if (m_pShaderProperty == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                memset(m_pShaderProperty, 0, sizeof(Vkgc::RayTracingShaderProperty) * m_compiledShaderCount);
                if (funcCount > 1) // Indirect call
                {
                    Util::Strncpy(m_pShaderProperty[0].name, "launchRay", Vkgc::RayTracingMaxShaderNameLength);
                    for (uint32_t i = 0; i < funcCount; i++)
                    {
                        if (pShaderProp[i].shaderId != RayTracingInvalidShaderId)
                        {
                            Util::Strncpy(m_pShaderProperty[i+1].name, pShaderProp[i].name,
                                                Vkgc::RayTracingMaxShaderNameLength);
                        }
                    }
                }
                else
                {
                    Util::Strncpy(m_pShaderProperty[0].name, "ray-gen", Vkgc::RayTracingMaxShaderNameLength);
                }
            }

            // Override pipeline creation parameters based on pipeline profile
            m_pDevice->GetShaderOptimizer()->OverrideComputePipelineCreateInfo(
                optimizerKey,
                &localPipelineInfo.immedInfo.computeShaderInfo);
        }

        if (result == VK_SUCCESS)
        {
            size_t totalLibrarySummariesSize = 0;

            for (uint32_t deviceIdx = 0; deviceIdx != MaxPalDevices; ++deviceIdx)
            {
                const auto& librarySummary = pipelineBinaries[deviceIdx].librarySummary;
                totalLibrarySummariesSize += Pow2Align(librarySummary.codeSize, 8);
            }

            if (totalLibrarySummariesSize != 0)
            {
                void* pBuffer = pAllocator->pfnAllocation(pAllocator->pUserData,
                                                          totalLibrarySummariesSize,
                                                          VK_DEFAULT_MEM_ALIGN,
                                                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

                if (pBuffer == nullptr)
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
                else
                {
                    size_t offset = 0;

                    for (uint32_t deviceIdx = 0; deviceIdx != MaxPalDevices; ++deviceIdx)
                    {
                        const auto& librarySummary = pipelineBinaries[deviceIdx].librarySummary;
                        librarySummaries[deviceIdx].pCode = VoidPtrInc(pBuffer, offset);
                        librarySummaries[deviceIdx].codeSize = librarySummary.codeSize;
                        memcpy(VoidPtrInc(pBuffer, offset), librarySummary.pCode, librarySummary.codeSize);
                        offset += Pow2Align(librarySummary.codeSize, 8);
                    }
                }
            }
        }

        size_t pipelineSize      = 0;
        size_t shaderLibrarySize = 0;
        void*  pSystemMem        = nullptr;

        // Create the PAL pipeline object.
        Pal::IShaderLibrary**   ppShaderLibraries                     = nullptr;
        ShaderGroupInfo*        pShaderGroupInfos                     = nullptr;
        PipelineBinaryStorage*  pPermBinaryStorage                    = nullptr;
        Pal::IPipeline*         pPalPipeline          [MaxPalDevices] = {};
        ShaderGroupStackSizes*  pShaderGroupStackSizes[MaxPalDevices] = {};
        gpusize                 traceRayGpuVas        [MaxPalDevices] = {};

        size_t pipelineMemSize              = 0;
        size_t shaderLibraryMemSize         = 0;
        size_t shaderLibraryPalMemSize      = 0;
        size_t shaderGroupStackSizesMemSize = 0;
        size_t shaderGroupInfosMemSize      = 0;
        size_t binaryStorageSize            = 0;

        const size_t shaderOptKeysSize = optimizerKey.shaderCount * sizeof(ShaderOptimizerKey);

        if (result == VK_SUCCESS)
        {
            const auto pBinaries = pipelineBinaries[DefaultDeviceIndex].pPipelineBins;

            // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
            // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
            if (pBinaries[0].pCode != nullptr)
            {
                localPipelineInfo.pipeline.flags.clientInternal = false;
#if VKI_BUILD_GFX12
                localPipelineInfo.pipeline.flags.reverseWorkgroupOrder = m_pDevice->GetShaderOptimizer()->
                    OverrideReverseWorkgroupOrder(Vkgc::ShaderStage::ShaderStageCompute,
                                                  optimizerKey);
                localPipelineInfo.pipeline.groupLaunchGuarantee =
                    static_cast<Pal::TriState>(settings.csGroupLaunchGuarantee);
#endif
                localPipelineInfo.pipeline.pipelineBinarySize   = pBinaries[0].codeSize;
                localPipelineInfo.pipeline.pPipelineBinary      = pBinaries[0].pCode;
                localPipelineInfo.pipeline.maxFunctionCallDepth =
                                pipelineBinaries[DefaultDeviceIndex].maxFunctionCallDepth;

                CsDispatchInterleaveSize interleaveSize = m_pDevice->GetShaderOptimizer()->
                    OverrideDispatchInterleaveSize(Vkgc::ShaderStage::ShaderStageCompute, optimizerKey);

                localPipelineInfo.pipeline.interleaveSize = (interleaveSize != CsDispatchInterleaveSizeDefault) ?
                    ConvertDispatchInterleaveSize(interleaveSize) :
                    ConvertDispatchInterleaveSize(settings.rtCsDispatchInterleaveSize);
            }

            // Get the pipeline and shader size from PAL and allocate memory.
            pipelineSize =
                m_pDevice->PalDevice(DefaultDeviceIndex)->GetComputePipelineSize(localPipelineInfo.pipeline, nullptr);

            Pal::ShaderLibraryCreateInfo dummyLibraryCreateInfo = {};
            shaderLibrarySize =
                m_pDevice->PalDevice(DefaultDeviceIndex)->GetShaderLibrarySize(dummyLibraryCreateInfo, nullptr);

            pipelineMemSize              = pipelineSize * m_pDevice->NumPalDevices();
            shaderLibraryPalMemSize      = shaderLibrarySize * funcCount * m_pDevice->NumPalDevices();
            shaderLibraryMemSize         = sizeof(Pal::IShaderLibrary*) * funcCount * m_pDevice->NumPalDevices();
            shaderGroupInfosMemSize      = sizeof(ShaderGroupInfo) * totalGroupCount;
            shaderGroupStackSizesMemSize = (((funcCount > 0) || hasLibraries) ? 1 : 0) *
                                           sizeof(ShaderGroupStackSizes) * totalGroupCount * m_pDevice->NumPalDevices();
            binaryStorageSize            = (storeBinaryToPipeline ? 1 : 0 ) * sizeof(PipelineBinaryStorage);

            const size_t totalSize =
                pipelineMemSize +
                shaderLibraryMemSize +
                shaderLibraryPalMemSize +
                shaderGroupStackSizesMemSize +
                shaderGroupInfosMemSize +
                binaryStorageSize +
                shaderOptKeysSize;

            pSystemMem = pAllocator->pfnAllocation(
                pAllocator->pUserData,
                totalSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            if (pSystemMem == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                memset(pSystemMem, 0, totalSize);
            }
        }

        if (result == VK_SUCCESS)
        {
            Pal::Result palResult = Pal::Result::Success;

            // ppShaderLibraries will be later used (via ~RayTracingPipeline()) to free pSystemMem
            ppShaderLibraries = static_cast<Pal::IShaderLibrary**>(pSystemMem);
            pShaderGroupInfos = static_cast<ShaderGroupInfo*>(Util::VoidPtrInc(ppShaderLibraries,
                                                                               shaderLibraryMemSize));

            void* pShaderOptKeys             = Util::VoidPtrInc(pShaderGroupInfos, shaderGroupInfosMemSize);
            void* pPalMem                    = Util::VoidPtrInc(pShaderOptKeys, shaderOptKeysSize);
            void* pPalShaderLibraryMem       = Util::VoidPtrInc(pPalMem, pipelineMemSize);
            void* pShaderGroupsStackSizesMem = Util::VoidPtrInc(pPalShaderLibraryMem, shaderLibraryPalMemSize);

            PopulateShaderGroupInfos(&pipelineCreateInfo, pShaderGroupInfos, totalGroupCount);

            if (storeBinaryToPipeline)
            {
                pPermBinaryStorage = static_cast<PipelineBinaryStorage*>(
                    Util::VoidPtrInc(pShaderGroupsStackSizesMem, shaderGroupStackSizesMemSize));

                // Simply copy the existing allocations to the new struct.
                *pPermBinaryStorage = binaryStorage;
            }

            // Transfer shader optimizer keys to permanent storage.
            memcpy(pShaderOptKeys, optimizerKey.pShaders, shaderOptKeysSize);
            optimizerKey.pShaders = static_cast<ShaderOptimizerKey*>(pShaderOptKeys);

            for (uint32_t deviceIdx = 0;
                ((deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
                deviceIdx++)
            {
                const auto pBinaries               = pipelineBinaries[deviceIdx].pPipelineBins;
                const auto ppDeviceShaderLibraries = ppShaderLibraries + deviceIdx * funcCount;
                void*      pDeviceShaderLibraryMem = Util::VoidPtrInc(pPalShaderLibraryMem,
                                                                      deviceIdx * funcCount * shaderLibrarySize);

                VK_ASSERT(pipelineSize ==
                    m_pDevice->PalDevice(deviceIdx)->GetComputePipelineSize(localPipelineInfo.pipeline, nullptr));

                // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
                // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
                if (hasKernelEntry)
                {
                    localPipelineInfo.pipeline.flags.clientInternal = false;
#if VKI_BUILD_GFX12
                    localPipelineInfo.pipeline.flags.reverseWorkgroupOrder = m_pDevice->GetShaderOptimizer()->
                        OverrideReverseWorkgroupOrder(Vkgc::ShaderStage::ShaderStageCompute,
                                                      optimizerKey);
                    localPipelineInfo.pipeline.groupLaunchGuarantee =
                        static_cast<Pal::TriState>(settings.csGroupLaunchGuarantee);
#endif
                    localPipelineInfo.pipeline.pipelineBinarySize   = pBinaries[0].codeSize;
                    localPipelineInfo.pipeline.pPipelineBinary      = pBinaries[0].pCode;
                    localPipelineInfo.pipeline.maxFunctionCallDepth = pipelineBinaries[deviceIdx].maxFunctionCallDepth;

                    CsDispatchInterleaveSize interleaveSize = m_pDevice->GetShaderOptimizer()->
                        OverrideDispatchInterleaveSize(Vkgc::ShaderStage::ShaderStageCompute, optimizerKey);

                    localPipelineInfo.pipeline.interleaveSize = (interleaveSize != CsDispatchInterleaveSizeDefault) ?
                        ConvertDispatchInterleaveSize(interleaveSize) :
                        ConvertDispatchInterleaveSize(settings.rtCsDispatchInterleaveSize);
                }

                // Copy indirect function info
                uint32_t       funcIndex           = 0;
                const auto     pShaderProp         = &pipelineBinaries[deviceIdx].shaderPropSet.shaderProps[0];
                const uint32_t traceRayShaderIndex = pipelineBinaries[deviceIdx].shaderPropSet.traceRayIndex;
                const uint32_t shaderCount         = pipelineBinaries[deviceIdx].shaderPropSet.shaderCount;

                for (uint32_t i = 0; i < shaderCount; i++)
                {
                    if (pShaderProp[i].shaderId != RayTracingInvalidShaderId)
                    {
                        pTraceRayUsage[funcIndex]                = pShaderProp[i].hasTraceRay;
                        pShaderNameMap[i]                        = funcIndex;
                        ++funcIndex;
                    }
                }
                VK_ASSERT(funcIndex == funcCount);

                if ((result == VK_SUCCESS) && hasKernelEntry)
                {
                    palResult = m_pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                        localPipelineInfo.pipeline,
                        Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                        &pPalPipeline[deviceIdx]);
                }

                // The size of stack is per native thread. So that stack size have to be multiplied by 2
                // if a Wave64 shader that needs scratch buffer is used.
                uint32_t stackSizeFactor = 1;
                if ((palResult == Util::Result::Success) && hasKernelEntry)
                {
                    Pal::ShaderStats shaderStats = {};
                    palResult = pPalPipeline[deviceIdx]->GetShaderStats(Pal::ShaderType::Compute,
                                                                        &shaderStats,
                                                                        false);
                    stackSizeFactor = (shaderStats.common.flags.isWave32 == 0) ? 2 : 1;
                }

                // Create shader library and remap shader ID to indirect function GPU Va
                if (palResult == Util::Result::Success)
                {
                    uint32_t binaryOffset = hasKernelEntry ? 1 : 0;
                    for (uint32_t i = 0; (i < funcCount) && (palResult == Util::Result::Success); ++i)
                    {
                        VK_ASSERT((pBinaries[i + binaryOffset].pCode != nullptr) &&
                                  (pBinaries[i + binaryOffset].codeSize != 0));
                        Pal::ShaderLibraryCreateInfo shaderLibraryCreateInfo = {};
                        shaderLibraryCreateInfo.pCodeObject = pBinaries[i + binaryOffset].pCode;
                        shaderLibraryCreateInfo.codeObjectSize = pBinaries[i + binaryOffset].codeSize;

                        palResult = m_pDevice->PalDevice(deviceIdx)->CreateShaderLibrary(
                            shaderLibraryCreateInfo,
                            Util::VoidPtrInc(pDeviceShaderLibraryMem, shaderLibrarySize * i),
                            &ppDeviceShaderLibraries[i]);

                        if (palResult == Util::Result::Success)
                        {
                            const auto pFunctionInfo = ppDeviceShaderLibraries[i]->GetShaderLibFunctionInfos().Data();
                            pIndirectFuncInfo[i].symbolName  = pFunctionInfo[0].symbolName;
                            pIndirectFuncInfo[i].gpuVirtAddr = pFunctionInfo[0].gpuVirtAddr;

                            m_totalShaderLibraryList[deviceIdx].PushBack(ppDeviceShaderLibraries[i]);
                        }
                    }

                    m_shaderStageDataList[deviceIdx].Resize(totalShaderCount);
                    // Used by calculation of default pipeline stack size
                    uint32_t rayGenStackMax       = 0;
                    uint32_t anyHitStackMax       = 0;
                    uint32_t closestHitStackMax   = 0;
                    uint32_t missStackMax         = 0;
                    uint32_t intersectionStackMax = 0;
                    uint32_t callableStackMax     = 0;
                    uint32_t backendStackSizeMax  = 0;
                    uint32_t traceRayStackSize    = 0;

                    if ((palResult == Util::Result::Success) && ((funcCount > 0) || hasLibraries))
                    {
                        pShaderGroupStackSizes[deviceIdx]
                            = static_cast<ShaderGroupStackSizes*>(
                                Util::VoidPtrInc(pShaderGroupsStackSizesMem,
                                    deviceIdx * totalGroupCount * sizeof(ShaderGroupStackSizes)));
                        memset(pShaderStackSize, 0xff, sizeof(VkDeviceSize) * maxFunctionCount);

                        auto GetFuncStackSize = [&](uint32_t shaderIdx) -> VkDeviceSize
                        {
                            auto UpdateLibStackSizes = [&](uint32_t libIdx)
                            {
                                auto pShaderLibrary = ppDeviceShaderLibraries[libIdx];
                                if (CheckIsCps())
                                {
                                    auto libFuncList = pShaderLibrary->GetShaderLibFunctionInfos();

                                    pShaderStackSize[libIdx] = 0;
                                    for (size_t i = 0; i < libFuncList.NumElements(); i++)
                                    {
                                        Pal::ShaderLibStats shaderStats = {};
                                        pShaderLibrary->GetShaderFunctionStats(
                                            libFuncList.Data()[i].symbolName,
                                            &shaderStats);
                                        pShaderStackSize[libIdx] += shaderStats.cpsStackSizes.frontendSize;
                                        // NOTE: Backend stack size is determined across all shaders (functions), no
                                        // need to record for each.
                                        backendStackSizeMax =
                                            Util::Max(backendStackSizeMax, shaderStats.cpsStackSizes.backendSize);
                                    }
                                }
                                else
                                {
                                    Pal::ShaderLibStats shaderStats = {};
                                    pShaderLibrary->GetShaderFunctionStats(
                                        pIndirectFuncInfo[libIdx].symbolName,
                                        &shaderStats);
                                    pShaderStackSize[libIdx] = shaderStats.stackFrameSizeInBytes;
                                }
                                pShaderStackSize[libIdx] *= stackSizeFactor;
                            };

                            VkDeviceSize stackSize = 0;
                            if (shaderIdx != VK_SHADER_UNUSED_KHR)
                            {
                                const uint32_t funcIdx = pShaderNameMap[shaderIdx];
                                if (funcIdx < funcCount)
                                {
                                    if (pShaderStackSize[funcIdx] == ~0ULL)
                                    {
                                        UpdateLibStackSizes(funcIdx);
                                    }
                                    VK_ASSERT(pShaderStackSize[funcIdx] != ~0ULL);
                                    stackSize = pShaderStackSize[funcIdx];
                                }
                            }
                            return stackSize;
                        };

                        if (m_hasTraceRay)
                        {
                            traceRayStackSize = GetFuncStackSize(traceRayShaderIndex);
                        }

                        const uint32_t stageShaderCount = pipelineCreateInfo.stageCount;
                        for (uint32_t sIdx = 0; sIdx < shaderCount; ++sIdx)
                        {
                            if (pShaderProp[sIdx].shaderId != RayTracingInvalidShaderId)
                            {
                                uint32_t shaderIdx = pShaderNameMap[sIdx];

                                auto pIndirectFunc = &pIndirectFuncInfo[shaderIdx];
                                if (shaderIdx < stageShaderCount)
                                {
                                    ShaderStageData shaderStageData = {};
                                    // shader stage has Trace ray status
                                    shaderStageData.hasTraceRay = pShaderProp[sIdx].hasTraceRay;

                                    // shader stage stack size
                                    shaderStageData.stackSize = GetFuncStackSize(sIdx);

                                    // shader stage GPU virtual address
                                    VK_ASSERT(strncmp(pIndirectFunc->symbolName.Data(),
                                        &pShaderProp[sIdx].name[0],
                                        pIndirectFunc->symbolName.Length()) == 0);

                                    uint64_t gpuVirtAddr = pIndirectFunc->gpuVirtAddr;
                                    if (pShaderProp[sIdx].onlyGpuVaLo)
                                    {
                                        gpuVirtAddr = gpuVirtAddr & 0xffffffff;
                                    }
                                    const uint64_t shaderVirtAddr = gpuVirtAddr | pShaderProp[sIdx].shaderIdExtraBits;
                                    shaderStageData.gpuVirtAddress = shaderVirtAddr;

                                    m_shaderStageDataList[deviceIdx][shaderIdx] = shaderStageData;
                                }
                            }
                        }

                        m_shaderStageDataCount = totalShaderCount;
                    }

                    // now appending the pipeline library data
                    gpusize      pipelineLibTraceRayVa  = 0;
                    bool         pipelineHasTraceRay    = false;
                    uint32_t     shaderStageCount       = pipelineCreateInfo.stageCount;

                    // append pipeline library group stack size to the main pipeline group stack size
                    // first appending all the groups of pLibraries[0], then all the groups of pLibraries[1], etc
                    // with no gap in between
                    if ((palResult == Util::Result::Success) && hasLibraries)
                    {
                        uint32_t mixedGroupCount = pipelineCreateInfo.groupCount;
                        // Create shader library and remap shader ID to indirect function GPU Va
                        // If pipeline including pipeline libraries, import the libraries here as well
                        for (uint32_t libIdx = 0;
                            ((libIdx < pCreateInfo->pLibraryInfo->libraryCount) && (palResult == Util::Result::Success));
                            ++libIdx)
                        {
                            const auto pLibraries             = pCreateInfo->pLibraryInfo->pLibraries[libIdx];
                            const auto pPipelineLib           = RayTracingPipeline::ObjectFromHandle(pLibraries);

                            if (palResult == Util::Result::Success)
                            {
                                // update group count
                                const uint32_t pipelineGroupCount   = pPipelineLib->GetShaderGroupCount();
                                const auto pPipelineLibShaderGroups = pPipelineLib->GetShaderGroupHandles(deviceIdx);
                                const auto pLibGroupInfos           = pPipelineLib->GetShaderGroupInfos();

                                if (CheckIsCps())
                                {
                                    // Shaders in a library may require more backend stack than the main pipeline, we
                                    // need to reserve enough space when setting up continuation stack pointer in order
                                    // not to collide with backend stack.
                                    uint32_t libBackendSize =
                                        pPipelineLib->GetDefaultPipelineStackSizes(deviceIdx).backendSize;
                                    backendStackSizeMax = Util::Max(backendStackSizeMax, libBackendSize);
                                }

                                // update pipelineHasTraceRay and pipelineLibTraceRayVa
                                if (pPipelineLib->CheckHasTraceRay())
                                {
                                    pipelineHasTraceRay   = true;
                                    pipelineLibTraceRayVa = pPipelineLib->GetTraceRayGpuVa(deviceIdx);
                                }

                                // map the GPU Va from Pipeline Library to local pShaderGroups
                                for (uint32_t libGroupIdx = 0; libGroupIdx < pipelineGroupCount; ++libGroupIdx)
                                {
                                    // append the pipeline library GPUVa after main pipeline
                                    const uint32_t groupIdx = mixedGroupCount + libGroupIdx;
                                    const auto  pStackSizes = &pShaderGroupStackSizes[deviceIdx][groupIdx];
                                    const auto  pGroup      = &pShaderGroups[deviceIdx][groupIdx];
                                    const auto& stages      = pLibGroupInfos[libGroupIdx].stages;

                                    pGroup->shaderId       = pPipelineLibShaderGroups[libGroupIdx].shaderId;
                                    pGroup->anyHitId       = pPipelineLibShaderGroups[libGroupIdx].anyHitId;
                                    pGroup->intersectionId = pPipelineLibShaderGroups[libGroupIdx].intersectionId;
                                    pGroup->padding        = pPipelineLibShaderGroups[libGroupIdx].padding;

                                    ShaderStackSize stackSizes = {};
                                    switch (pLibGroupInfos[libGroupIdx].type)
                                    {
                                    case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
                                        stackSizes =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_GENERAL_KHR,
                                                traceRayStackSize);

                                        pStackSizes->generalSize = stackSizes.size;
                                        pStackSizes->metadata.generalSizeNeedAddTraceRay = stackSizes.needAddTraceRay;

                                        if ((stages & VK_SHADER_STAGE_RAYGEN_BIT_KHR) != 0)
                                        {
                                            rayGenStackMax = Util::Max(
                                                rayGenStackMax,
                                                static_cast<uint32_t>(pStackSizes->generalSize));
                                        }
                                        else if ((stages & VK_SHADER_STAGE_MISS_BIT_KHR) != 0)
                                        {
                                            missStackMax = Util::Max(
                                                missStackMax,
                                                static_cast<uint32_t>(pStackSizes->generalSize));
                                        }
                                        else if ((stages & VK_SHADER_STAGE_CALLABLE_BIT_KHR) != 0)
                                        {
                                            callableStackMax = Util::Max(
                                                callableStackMax,
                                                static_cast<uint32_t>(pStackSizes->generalSize));
                                        }
                                        else
                                        {
                                            VK_NEVER_CALLED();
                                        }
                                        break;

                                    case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
                                        stackSizes =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_INTERSECTION_KHR,
                                                traceRayStackSize);
                                        pStackSizes->intersectionSize = stackSizes.size;
                                        pStackSizes->metadata.intersectionSizeNeedAddTraceRay =
                                            stackSizes.needAddTraceRay;

                                        intersectionStackMax = Util::Max(
                                            intersectionStackMax,
                                            static_cast<uint32_t>(pStackSizes->intersectionSize));
                                        [[fallthrough]];

                                    case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
                                        stackSizes =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_ANY_HIT_KHR,
                                                traceRayStackSize);
                                        pStackSizes->anyHitSize = stackSizes.size;
                                        pStackSizes->metadata.anyHitSizeNeedAddTraceRay = stackSizes.needAddTraceRay;

                                        stackSizes =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR,
                                                traceRayStackSize);

                                        pStackSizes->closestHitSize = stackSizes.size;
                                        pStackSizes->metadata.closestHitSizeNeedAddTraceRay =
                                            stackSizes.needAddTraceRay;

                                        anyHitStackMax = Util::Max(
                                            anyHitStackMax,
                                            static_cast<uint32_t>(pStackSizes->anyHitSize));
                                        closestHitStackMax = Util::Max(
                                            closestHitStackMax,
                                            static_cast<uint32_t>(pStackSizes->closestHitSize));
                                        break;

                                    default:
                                        break;
                                    }
                                }

                                mixedGroupCount = mixedGroupCount + pipelineGroupCount;
                            }

                            // Merge all shader libraries from imported pipeline library
                            const auto& importedShaderLibs      = pPipelineLib->GetTotalShaderLibraryList(deviceIdx);
                            const uint32_t importedLibsCount    = importedShaderLibs.NumElements();
                            for (uint32_t libShaderIdx = 0; libShaderIdx < importedLibsCount; ++libShaderIdx)
                            {
                                m_totalShaderLibraryList[deviceIdx].PushBack(importedShaderLibs.At(libShaderIdx));
                            }

                            // Merge all shader stage data from imported pipeline library
                            const auto& libShaderDataList = pPipelineLib->GetShaderStageDataList(deviceIdx);
                            const uint32_t libStageCount  = pPipelineLib->GetShaderStageDataCount();
                            for (uint32_t lsIdx = 0; lsIdx < libStageCount; ++lsIdx)
                            {
                                m_shaderStageDataList[deviceIdx][shaderStageCount + lsIdx] =
                                    libShaderDataList.At(lsIdx);
                            }
                            shaderStageCount += libStageCount;
                        }

                        VK_ASSERT(m_shaderStageDataCount == shaderStageCount);
                    }

                    if (m_totalShaderLibraryList[deviceIdx].NumElements() > 0)
                    {
                        // Patch GPU virtual address
                        for (uint32_t grpIdx = 0; grpIdx < pipelineCreateInfo.groupCount; ++grpIdx)
                        {
                            const auto pGroup = &pShaderGroups[deviceIdx][grpIdx];
                            pGroup->padding = 0;

                            uint64_t* pGroupShaderIds[] = { &pGroup->shaderId, &pGroup->intersectionId, &pGroup->anyHitId };
                            for (uint32_t idx = 0; idx < sizeof(pGroupShaderIds) / sizeof(uint64_t*); ++idx)
                            {
                                if (*pGroupShaderIds[idx] != RayTracingInvalidShaderId)
                                {
                                    const uint64_t groupShaderId = *pGroupShaderIds[idx] - 1;

                                    VK_ASSERT(groupShaderId < m_shaderStageDataList[deviceIdx].NumElements());
                                    auto shaderStageInfo = m_shaderStageDataList[deviceIdx].At(groupShaderId);
                                    *pGroupShaderIds[idx] = shaderStageInfo.gpuVirtAddress;
                                }
                            }
                        }

                        // Patch stack size
                        auto GetTraceRayUsage = [&](uint32_t shaderIdx) -> bool
                        {
                            if (shaderIdx != VK_SHADER_UNUSED_KHR)
                            {
                                auto stageData = m_shaderStageDataList[deviceIdx].At(shaderIdx);
                                return stageData.hasTraceRay;
                            }
                            return false;
                        };

                        auto GetStackSizeFromList = [&](uint32_t shaderIdx) -> VkDeviceSize
                        {
                            if (shaderIdx != VK_SHADER_UNUSED_KHR)
                            {
                                auto stageData = m_shaderStageDataList[deviceIdx].At(shaderIdx);
                                return stageData.stackSize;
                            }
                            return 0;
                        };

                        for (uint32_t groupIdx = 0; groupIdx < m_createInfo.GetGroupCount(); ++groupIdx)
                        {
                            const auto& groupInfo = m_createInfo.GetGroupList().At(groupIdx);
                            const auto  pCurrentStackSizes = &pShaderGroupStackSizes[deviceIdx][groupIdx];

                            switch (groupInfo.type)
                            {
                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
                                pCurrentStackSizes->generalSize = GetStackSizeFromList(groupInfo.generalShader);

                                if (GetTraceRayUsage(groupInfo.generalShader))
                                {
                                    if (m_hasTraceRay)
                                    {
                                        pCurrentStackSizes->generalSize += traceRayStackSize;
                                    }
                                    else
                                    {
                                        pCurrentStackSizes->metadata.generalSizeNeedAddTraceRay = 1;
                                    }
                                }

                                switch (m_createInfo.GetStageList().At(groupInfo.generalShader).stage)
                                {
                                case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                                    rayGenStackMax = Util::Max(
                                        rayGenStackMax,
                                        static_cast<uint32_t>(pCurrentStackSizes->generalSize));
                                    break;
                                case VK_SHADER_STAGE_MISS_BIT_KHR:
                                    missStackMax = Util::Max(
                                        missStackMax,
                                        static_cast<uint32_t>(pCurrentStackSizes->generalSize));
                                    break;
                                case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                                    callableStackMax = Util::Max(
                                        callableStackMax,
                                        static_cast<uint32_t>(pCurrentStackSizes->generalSize));
                                    break;
                                default:
                                    VK_NEVER_CALLED();
                                    break;
                                }
                                break;

                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
                                pCurrentStackSizes->intersectionSize = GetStackSizeFromList(groupInfo.intersectionShader);

                                if (GetTraceRayUsage(groupInfo.intersectionShader))
                                {
                                    if (m_hasTraceRay)
                                    {
                                        pCurrentStackSizes->intersectionSize += traceRayStackSize;
                                    }
                                    else
                                    {
                                        pCurrentStackSizes->metadata.intersectionSizeNeedAddTraceRay = 1;
                                    }
                                }

                                intersectionStackMax = Util::Max(
                                    intersectionStackMax, static_cast<uint32_t>(pCurrentStackSizes->intersectionSize));
                                [[fallthrough]];

                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
                                pCurrentStackSizes->anyHitSize = GetStackSizeFromList(groupInfo.anyHitShader);
                                pCurrentStackSizes->closestHitSize = GetStackSizeFromList(groupInfo.closestHitShader);

                                if (GetTraceRayUsage(groupInfo.anyHitShader))
                                {
                                    if (m_hasTraceRay)
                                    {
                                        pCurrentStackSizes->anyHitSize += traceRayStackSize;
                                    }
                                    else
                                    {
                                        pCurrentStackSizes->metadata.anyHitSizeNeedAddTraceRay = 1;
                                    }
                                }

                                if (GetTraceRayUsage(groupInfo.closestHitShader))
                                {
                                    if (m_hasTraceRay)
                                    {
                                        pCurrentStackSizes->closestHitSize += traceRayStackSize;
                                    }
                                    else
                                    {
                                        pCurrentStackSizes->metadata.closestHitSizeNeedAddTraceRay = 1;
                                    }
                                }

                                anyHitStackMax = Util::Max(
                                    anyHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->anyHitSize));
                                closestHitStackMax = Util::Max(
                                    closestHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->closestHitSize));
                                break;

                            default:
                                VK_NEVER_CALLED();
                                break;
                            }
                        }
                    }

                    if ((palResult == Util::Result::Success) && hasKernelEntry)
                    {
                        palResult = pPalPipeline[deviceIdx]->LinkWithLibraries(
                            m_totalShaderLibraryList[deviceIdx].Data(),
                            m_totalShaderLibraryList[deviceIdx].NumElements());
                    }

                    // Calculate the default pipeline size via spec definition
                    uint32_t defaultPipelineStackSize =
                        rayGenStackMax +
                        (Util::Min(1U, m_createInfo.GetMaxRecursionDepth()) *
                         Util::Max(closestHitStackMax, missStackMax, intersectionStackMax + anyHitStackMax)) +
                        (Util::Max(0U, m_createInfo.GetMaxRecursionDepth()) *
                         Util::Max(closestHitStackMax, missStackMax)) +
                        (2 * callableStackMax);

                    if (CheckIsCps())
                    {
                        // The size we calculated above is frontend stack size for continuations.
                        m_defaultPipelineStackSizes[deviceIdx].frontendSize = defaultPipelineStackSize;
                        m_defaultPipelineStackSizes[deviceIdx].backendSize = backendStackSizeMax;
                    }
                    else
                    {
                        m_defaultPipelineStackSizes[deviceIdx].backendSize = defaultPipelineStackSize;
                    }

                    // TraceRay is the last function in function list, record it regardless we are building library or
                    // not, so that a pipeline will get its own TraceRayGpuVa correctly.
                    if (m_hasTraceRay && (funcCount > 0))
                    {
                        const auto traceRayFuncIndex = funcCount - 1;
                        traceRayGpuVas[deviceIdx] =
                            pIndirectFuncInfo[traceRayFuncIndex].gpuVirtAddr |
                            pShaderProp[traceRayFuncIndex].shaderIdExtraBits;
                    }
                    else if (pipelineHasTraceRay)
                    {
                        traceRayGpuVas[deviceIdx] = pipelineLibTraceRayVa;
                    }

                    if ((m_createInfo.GetGroupCount() > 0) &&
                        (m_createInfo.GetGroupList().At(0).pShaderGroupCaptureReplayHandle != nullptr))
                    {
                        // Replaying in indirect mode, the replayer will upload VAs that is calculated when capturing to
                        // SBT, we need to map them to new VAs newly generated which are actually in used.
                        // Group count has to match for us to do a one-on-one mapping.
                        VK_ASSERT(totalGroupCount == (m_createInfo.GetGroupCount() + pipelineLibGroupCount));
                        result = ProcessCaptureReplayHandles(pShaderGroups[DefaultDeviceIndex],
                                                             pCreateInfo->pLibraryInfo,
                                                             pAllocator);
                    }
                }
                // Temporarily reinject post Pal pipeline creation (when the internal pipeline hash is available).
                // The reinjection cache layer can be linked back into the pipeline cache chain once the
                // Vulkan pipeline cache key can be stored (and read back) inside the ELF as metadata.
                if ((m_pDevice->VkInstance()->GetDevModeMgr() != nullptr) &&
                    (palResult == Util::Result::Success))
                {
                    const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                    palResult = m_pDevice->GetCompiler(deviceIdx)->RegisterAndLoadReinjectionBinary(
                        &info.internalPipelineHash,
                        &cacheId[deviceIdx],
                        &localPipelineInfo.pipeline.pipelineBinarySize,
                        &localPipelineInfo.pipeline.pPipelineBinary);

                    if (palResult == Util::Result::Success)
                    {
                        pPalPipeline[deviceIdx]->Destroy();

                        palResult = m_pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                            localPipelineInfo.pipeline,
                            Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                            &pPalPipeline[deviceIdx]);

                    }
                    else if (palResult == Util::Result::NotFound)
                    {
                        // If a replacement was not found, proceed with the original
                        palResult = Util::Result::Success;
                    }
                }
            }

            result = PalToVkResult(palResult);
        }

        if (result == VK_SUCCESS)
        {
            uint32_t dispatchRaysUserDataOffset = GetDispatchRaysUserData(&localPipelineInfo.resourceLayout);

            Init(hasKernelEntry ? pPalPipeline : nullptr,
                 funcCount * m_pDevice->NumPalDevices(),
                 ppShaderLibraries,
                 &localPipelineInfo.resourceLayout.userDataLayout,
                 pPermBinaryStorage,
                 optimizerKey.pShaders,
                 localPipelineInfo.immedInfo,
                 localPipelineInfo.staticStateMask,
                 nativeShaderCount,
                 totalShaderCount,
                 totalGroupCount,
                 pShaderGroups,
                 pShaderGroupStackSizes,
                 pShaderGroupInfos,
                 librarySummaries,
                 binaryCreateInfo.maxAttributeSize,
                 traceRayGpuVas,
                 dispatchRaysUserDataOffset,
                 cacheId[DefaultDeviceIndex],
                 apiPsoHash,
                 elfHash);
            if (m_pDevice->GetEnabledFeatures().enableDebugPrintf)
            {
                ClearFormatString();
                for (uint32_t i = 0; i < pipelineBinaries[DefaultDeviceIndex].pipelineBinCount; ++i)
                {
                    DebugPrintf::DecodeFormatStringsFromElf(
                        m_pDevice,
                        pipelineBinaries[DefaultDeviceIndex].pPipelineBins[i].codeSize,
                        static_cast<const char*>(pipelineBinaries[DefaultDeviceIndex].pPipelineBins[i].pCode),
                        GetFormatStrings());
                }
            }
        }
        else
        {
            // Free the binaries only if we failed to create the pipeline.
            FreeBinaryStorage(&binaryStorage, pAllocator);

            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                // Internal memory allocation failed, free PAL event object if it gets created
                if (pPalPipeline[deviceIdx] != nullptr)
                {
                    pPalPipeline[deviceIdx]->Destroy();
                }
            }

            if (pShaderGroups[0] != nullptr)
            {
                pAllocator->pfnFree(pAllocator->pUserData, pShaderGroups[0]);
            }

            if (librarySummaries[0].pCode != nullptr)
            {
                pAllocator->pfnFree(pAllocator->pUserData, const_cast<void*>(librarySummaries[0].pCode));
            }
        }

        // Free the temporary memory for shader modules
        if (totalShaderCount > 0)
        {
            // Free the temporary newly-built shader modules
            FreeTempModules(m_pDevice, nativeShaderCount, pTempModules);

            if (pShaderTempBuffer != nullptr)
            {
                pAllocator->pfnFree(pAllocator->pUserData, pShaderTempBuffer);
            }
        }

        // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
        if (binariesProvided == false)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                m_pDevice->GetCompiler(deviceIdx)->FreeRayTracingPipelineBinary(
                    &binaryCreateInfo,
                    &pipelineBinaries[deviceIdx]);
            }
        }

        pAllocator->pfnFree(pAllocator->pUserData, pTempBuffer);

        pDefaultCompiler->FreeRayTracingPipelineCreateInfo(&binaryCreateInfo);

        // Something went wrong with creating the PAL object. Free memory and return error.
        if (result != VK_SUCCESS)
        {
            // Free system memory for pipeline object
            pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);
        }

        if (result == VK_SUCCESS)
        {
            uint64_t durationTicks = Util::GetPerfCpuTime() - startTimeTicks;
            uint64_t duration      = vk::utils::TicksToNano(durationTicks);

            binaryCreateInfo.pipelineFeedback.feedbackValid = true;
            binaryCreateInfo.pipelineFeedback.duration      = duration;

            PipelineCompiler::SetPipelineCreationFeedbackInfo(
                pPipelineCreationFeedbackCreateInfo,
                0,
                NULL,
                &binaryCreateInfo.pipelineFeedback,
                NULL);

            // The hash is same as pipline dump file name, we can easily analyze further.
            AmdvlkLog(settings.logTagIdMask, PipelineCompileTime, "0x%016llX-%llu", apiPsoHash, duration);
        }

    }

    return result;
}

// =====================================================================================================================
VkResult RayTracingPipeline::CreateCacheId(
    const Device*                               pDevice,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfo,
    VkPipelineCreateFlags2KHR                   flags,
    const bool                                  hasLibraries,
    RayTracingPipelineShaderStageInfo*          pShaderInfo,
    PipelineOptimizerKey*                       pPipelineOptimizerKey,
    uint64_t*                                   pApiPsoHash,
    Util::MetroHash::Hash*                      pElfHash,
    ShaderModuleHandle*                         pTempModules,
    Util::MetroHash::Hash*                      pCacheIds)
{
    VkResult result = VK_SUCCESS;

    // 1. Build shader stage info
    if (pPipelineOptimizerKey->shaderCount > 0)
    {
        result = BuildShaderStageInfo(
            pDevice,
            pCreateInfo->stageCount,
            pCreateInfo->pStages,
            [](const uint32_t inputIdx, const uint32_t stageIdx)
            {
                return inputIdx;
            },
            pShaderInfo->pStages,
            pTempModules,
            nullptr);
    }

    if (result == VK_SUCCESS)
    {
        // 2. Build ShaderOptimizer pipeline key
        uint32_t shaderIdx = 0;

        for (; shaderIdx < pCreateInfo->stageCount; ++shaderIdx)
        {
            const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
                ShaderModule::GetFirstValidShaderData(pShaderInfo->pStages[shaderIdx].pModuleHandle));

            pDevice->GetShaderOptimizer()->CreateShaderOptimizerKey(
                pModuleData,
                pShaderInfo->pStages[shaderIdx].codeHash,
                pShaderInfo->pStages[shaderIdx].stage,
                pShaderInfo->pStages[shaderIdx].codeSize,
                &pPipelineOptimizerKey->pShaders[shaderIdx]);
        }

        if (hasLibraries)
        {
            for (uint32_t libraryIdx = 0; libraryIdx < pCreateInfo->pLibraryInfo->libraryCount; ++libraryIdx)
            {
                const auto pLibrary = RayTracingPipeline::ObjectFromHandle(
                    pCreateInfo->pLibraryInfo->pLibraries[libraryIdx]);
                const auto shaderCount = pLibrary->GetTotalShaderCount();

                memcpy(&pPipelineOptimizerKey->pShaders[shaderIdx],
                       pLibrary->GetShaderOptKeys(),
                       sizeof(ShaderOptimizerKey) * shaderCount);
                shaderIdx += shaderCount;
            }
        }

        VK_ASSERT(shaderIdx == pPipelineOptimizerKey->shaderCount);

        // 3. Build API and ELF hashes
        BuildApiHash(pCreateInfo, flags, pElfHash, pApiPsoHash);

        // 4. Build Cache IDs
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            ElfHashToCacheId(
                pDevice,
                deviceIdx,
                *pElfHash,
                *pPipelineOptimizerKey,
                &pCacheIds[deviceIdx]
                );
        }
    }

    return result;
}

// =====================================================================================================================
static int32_t DeferredCreateRayTracingPipelineCallback(
    Device*                pDevice,
    DeferredHostOperation* pOperation,
    DeferredCallbackType   type)
{
    int32_t result;
    DeferredHostOperation::RayTracingPipelineCreateState* pState = pOperation->RayTracingPipelineCreate();

    switch (type)
    {
    case DeferredCallbackType::Join:
    {
        if (pState->nextPending < pState->infoCount)
        {
            uint32_t index = Util::AtomicIncrement(&pState->nextPending) - 1;

            // Run in a loop until we've processed all pipeline create infos. Parallel joins in their own loops can
            // consume iterations. A single "main" thread per pipeline is sent out here. These threads will not return
            // untill the pipeline has been fully created (unlike the helper worker threads).
            while (index < pState->infoCount)
            {
                VkResult                                 localResult = VK_SUCCESS;
                const VkRayTracingPipelineCreateInfoKHR* pCreateInfo = &pState->pInfos[index];
                VkPipelineCreateFlags2KHR                flags       = Device::GetPipelineCreateFlags(pCreateInfo);

                if (pState->skipRemaining == VK_FALSE)
                {
                    RayTracingPipeline* pPipeline = RayTracingPipeline::ObjectFromHandle(pState->pPipelines[index]);

                    localResult = pPipeline->CreateImpl(pState->pPipelineCache,
                                                        pCreateInfo,
                                                        flags,
                                                        pState->pAllocator,
                                                        pOperation->Workload(index));

                    if (localResult == VK_SUCCESS)
                    {
                        IDevMode* pDevMode = pDevice->VkInstance()->GetDevModeMgr();

                        if (pDevMode != nullptr)
                        {
                            pDevMode->PipelineCreated(pDevice, pPipeline);

                            if (pPipeline->IsInlinedShaderEnabled() == false)
                            {
                                pDevMode->ShaderLibrariesCreated(pDevice, pPipeline);
                            }
                        }
                    }
                }

                if (localResult != VK_SUCCESS)
                {
                    Util::AtomicCompareAndSwap(&pState->finalResult,
                                               static_cast<uint32_t>(VK_SUCCESS),
                                               static_cast<uint32_t>(localResult));

                    if (flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
                    {
                        Util::AtomicCompareAndSwap(&pState->skipRemaining,
                                                   VK_FALSE,
                                                   VK_TRUE);
                    }
                }

                // If the workloads for this pipeline are still pending (after creation), then no-op them at this point
                Util::AtomicCompareAndSwap(&pOperation->Workload(index)->totalInstances,
                                           UINT_MAX,
                                           0);

                Util::AtomicIncrement(&pState->completed);

                index = Util::AtomicIncrement(&pState->nextPending) - 1;
            }
        }

        // Helper worker threads go through here. They assist the main pipeline threads. Currently, the only workloads
        // we have are the compute pipeline library creations. Helper threads return when no work is available to
        // execute.
        for (uint32_t workloadIdx = 0; workloadIdx < pOperation->WorkloadCount(); ++workloadIdx)
        {
            DeferredHostOperation::ExecuteWorkload(pOperation->Workload(workloadIdx));
        }

        // At least one joining thread is responsible for signaling when full operation is complete. In this case,
        // return VK_SUCCESS when all pipelines are created.
        if (pState->completed == pState->infoCount)
        {
            result = VK_SUCCESS;
        }
        else
        {
            result = VK_THREAD_DONE_KHR;

            // Return VK_THREAD_IDLE_KHR if workloads still remain
            for (uint32_t workloadIdx = 0; workloadIdx < pOperation->WorkloadCount(); ++workloadIdx)
            {
                const DeferredWorkload* pWorkload = pOperation->Workload(workloadIdx);
                uint32_t totalInstances = pWorkload->totalInstances;

                if ((totalInstances == UINT_MAX) || (pWorkload->nextInstance < totalInstances))
                {
                    result = VK_THREAD_IDLE_KHR;
                    break;
                }
            }
        }

        break;
    }
    case DeferredCallbackType::GetMaxConcurrency:
    {
        uint32_t maxConcurrency = pState->infoCount - Util::Min(pState->nextPending, pState->infoCount);

        for (uint32_t workloadIdx = 0; workloadIdx < pOperation->WorkloadCount(); ++workloadIdx)
        {
            const DeferredWorkload* pWorkload = pOperation->Workload(workloadIdx);
            uint32_t totalInstances = pWorkload->totalInstances;

            uint32_t workloadConcurrency = (totalInstances == UINT_MAX) ? pWorkload->maxInstances :
                (totalInstances - Util::Min(pWorkload->nextInstance, totalInstances));

            // Subtract one, as it will be executed on the pipeline main thread
            maxConcurrency += Util::Max(workloadConcurrency, static_cast<uint32_t>(1)) - 1;
        }

        result = maxConcurrency;

        break;
    }
    case DeferredCallbackType::GetResult:
    {
        if (pState->completed < pState->infoCount)
        {
            result = static_cast<int32_t>(VK_NOT_READY);
        }
        else
        {
            result = static_cast<int32_t>(pState->finalResult);
        }

        break;
    }
    default:
        VK_NEVER_CALLED();
        result = 0;
        break;
    }

    return result;
}

// =====================================================================================================================
// Gets the code object binary according to binary index
void RayTracingPipeline::GetPipelineBinaryByIndex(
    uint32_t        index,
    void*           pBinary,
    uint32_t*       pSize
    ) const
{
    if (index < m_compiledShaderCount)
    {
        if (index == 0)
        {
            m_pPalPipeline[0]->GetCodeObject(pSize, pBinary);
        }
        else
        {
            m_ppShaderLibraries[index - 1]->GetCodeObject(pSize, pBinary);
        }
    }
}

// =====================================================================================================================
void RayTracingPipeline::GetShaderDescriptionByStage(
    char*              pDescription,
    const uint32_t     index,
    const uint32_t     binaryCount
    ) const
{
    // Beginning of the description
    Util::Strncpy(pDescription, "Executable handles following Vulkan stages: ", VK_MAX_DESCRIPTION_SIZE);

    if (index == 0) // First shader is the compute launch shader
    {
        if (binaryCount == 1) // Inline shader
        {
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, "Inline Raygen Shader");
        }
        else
        {
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, "Internal Launch Shader");
        }
    }
    else if (index == (binaryCount - 1)) // Last shader is the internal traceray shader
    {
        Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, "Internal Intersection Shader");
    }
    else
    {
        const ShaderGroupInfo* pShaderGroupInfo = GetShaderGroupInfos();
        VkShaderStageFlags shaderStage = pShaderGroupInfo[index - 1].stages;

        switch (shaderStage)
        {
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_RAYGEN_BIT_KHR ");
            break;
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_MISS_BIT_KHR ");
            break;
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_ANY_HIT_BIT_KHR ");
            break;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR ");
            break;
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_INTERSECTION_BIT_KHR ");
            break;
        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_CALLABLE_BIT_KHR ");
            break;
        default:
            Util::Strncat(pDescription, VK_MAX_DESCRIPTION_SIZE, " unsupported shader ");
            break;
        }
    }
}

// =====================================================================================================================
// Gets the shader property information
VkResult RayTracingPipeline::GetPipelineExecutableProperties(
    const VkPipelineInfoKHR*                    pPipelineInfo,
    uint32_t*                                   pExecutableCount,
    VkPipelineExecutablePropertiesKHR*          pProperties
    ) const
{
    const uint32_t numHWStages = GetPipelineBinaryCount();

    if (pProperties == nullptr)
    {
        *pExecutableCount = numHWStages;
    }
    else
    {
        const ShaderGroupInfo* pShaderGroupInfo = GetShaderGroupInfos();

        if (*pExecutableCount > numHWStages)
        {
            *pExecutableCount = numHWStages;
        }

        if ((*pExecutableCount == 1) && (numHWStages == 1)) //Inline shader
        {
            pProperties[0].stages = pShaderGroupInfo[0].stages;
            Util::Strncpy(pProperties[0].name, "ShaderProperties.ray-gen", VK_MAX_DESCRIPTION_SIZE);
            Util::Strncpy(pProperties[0].description,
                          "Executable handles following Vulkan stages: Raygen Shader",
                          VK_MAX_DESCRIPTION_SIZE);
        }
        else
        {
            for (uint32_t i = 0; i < *pExecutableCount; i++)
            {
                const Vkgc::RayTracingShaderProperty* pProperty = GetPipelineBinaryPropset(i);
                Util::Strncpy(pProperties[i].name, "ShaderProperties.", VK_MAX_DESCRIPTION_SIZE);
                Util::Strncat(pProperties[i].name, VK_MAX_DESCRIPTION_SIZE, pProperty->name);

                if (i == 0) // First shader is the compute launch shader
                {
                    pProperties[i].stages = VK_SHADER_STAGE_COMPUTE_BIT;
                }
                else if (i == (numHWStages - 1)) // Last shader is the internal traceray shader
                {
                    pProperties[i].stages = VK_SHADER_STAGE_COMPUTE_BIT;
                }
                else
                {
                    pProperties[i].stages = pShaderGroupInfo[i-1].stages;
                }
                GetShaderDescriptionByStage(pProperties[i].description, i, numHWStages);
            }
        }
    }

    return (*pExecutableCount < numHWStages) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
VkResult RayTracingPipeline::GetRayTracingShaderDisassembly(
    Util::Abi::PipelineSymbolType pipelineSymbolType,
    size_t                        binarySize,
    const void*                   pBinaryCode,
    size_t*                       pBufferSize,
    void*                         pBuffer
    ) const
{
    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    Util::Abi::PipelineAbiReader abiReader(m_pDevice->VkInstance()->Allocator(),
                                           Util::Span<const void>{pBinaryCode, binarySize});

    VkResult    result    = VK_SUCCESS;
    Pal::Result palResult = abiReader.Init();

    if (palResult == Pal::Result::Success)
    {
        bool symbolValid = false;

        // Support returing AMDIL/LLVM-IR and ISA
        VK_ASSERT((pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly) ||
                  (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderAmdIl));

        const char* pSectionName = nullptr;

        if (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly)
        {
            palResult = abiReader.CopySymbol(
                            Util::Abi::GetSymbolForStage(
                                Util::Abi::PipelineSymbolType::ShaderDisassembly,
                                Util::Abi::HardwareStage::Cs),
                            pBufferSize,
                            pBuffer);

            pSectionName = Util::Abi::AmdGpuDisassemblyName;
            symbolValid  = palResult == Util::Result::Success;
        }
        else if (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderAmdIl)
        {
            palResult = abiReader.CopySymbol(
                            Util::Abi::GetSymbolForStage(
                                Util::Abi::PipelineSymbolType::ShaderAmdIl,
                                Util::Abi::ApiShaderType::Cs),
                            pBufferSize,
                            pBuffer);

            pSectionName = Util::Abi::AmdGpuCommentLlvmIrName;
            symbolValid  = palResult == Util::Result::Success;
        }

        if ((symbolValid == false) && (pSectionName != nullptr))
        {
            const auto& elfReader = abiReader.GetElfReader();
            Util::ElfReader::SectionId disassemblySectionId = elfReader.FindSection(pSectionName);

            if (disassemblySectionId != 0)
            {
                const char* pDisassemblySection = static_cast<const char*>(
                    elfReader.GetSectionData(disassemblySectionId));
                size_t disassemblySectionLen = static_cast<size_t>(
                    elfReader.GetSection(disassemblySectionId).sh_size);

                symbolValid = true;

                // Fill output
                if (pBufferSize != nullptr)
                {
                    *pBufferSize = disassemblySectionLen + 1;
                }

                if (pBuffer != nullptr)
                {
                    memcpy(pBuffer, pDisassemblySection,
                            Util::Min(*pBufferSize, disassemblySectionLen));
                    if (*pBufferSize > disassemblySectionLen)
                    {
                        // Add null terminator
                        static_cast<char*>(pBuffer)[disassemblySectionLen] = '\0';
                    }
                }

                if (*pBufferSize < disassemblySectionLen)
                {
                    symbolValid = false;
                }
            }
        }

        result = symbolValid ? VK_SUCCESS : VK_INCOMPLETE;
    }
    else
    {
        VK_ASSERT(palResult == Pal::Result::ErrorInvalidMemorySize);

        result = VK_INCOMPLETE;
    }

    return result;
}

// =====================================================================================================================
// Gets the shader binary/disassembler info
VkResult RayTracingPipeline::GetPipelineExecutableInternalRepresentations(
    const VkPipelineExecutableInfoKHR*             pExecutableInfo,
    uint32_t*                                      pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations
    ) const
{
    constexpr uint32_t NumberOfInternalRepresentations = 2; // ELF binary and ISA disassembly
    const uint32_t index = pExecutableInfo->executableIndex;
    const uint32_t binaryCount = GetPipelineBinaryCount();

    if (pInternalRepresentations == nullptr)
    {
        *pInternalRepresentationCount = NumberOfInternalRepresentations;
    }
    else // Output the shader
    {
        if ((index < binaryCount) && (*pInternalRepresentationCount > 0))
        {
            uint32_t binarySize = 0;
            uint32_t entry = 0;
            void* pBinaryCode = nullptr;
            const Vkgc::RayTracingShaderProperty* pProperty = GetPipelineBinaryPropset(index);

            // ELF format binary back
            GetPipelineBinaryByIndex(index, nullptr, &binarySize);
            if ((pInternalRepresentations[entry].dataSize == 0) ||
                (pInternalRepresentations[entry].pData == nullptr))
            {
                pInternalRepresentations[entry].dataSize = binarySize + 1;
            }
            else
            {
                if (pInternalRepresentations[entry].dataSize >= binarySize)
                {
                    pBinaryCode = pInternalRepresentations[entry].pData;
                    GetPipelineBinaryByIndex(index, pBinaryCode, &binarySize);
                    // Add null terminator
                    if (pInternalRepresentations[entry].dataSize > binarySize)
                    {
                        static_cast<char*>(pInternalRepresentations[entry].pData)[binarySize] = '\0';
                    }

                    GetShaderDescriptionByStage(pInternalRepresentations[entry].description,
                                                index, binaryCount);
                    Util::Strncpy(pInternalRepresentations[entry].name, "ELF.", VK_MAX_DESCRIPTION_SIZE);
                    Util::Strncat(pInternalRepresentations[entry].name,
                                    VK_MAX_DESCRIPTION_SIZE, pProperty->name);
                }
                else
                {
                    *pInternalRepresentationCount = 0; // ELF binary is incompleted
                }
            }
            entry++;

            // If the first entry is incomplete/invalid, we will ginore the second internal representation
            // and return VK_INCOMPLETE with pInternalRepresentationCount = 0 directly
            // If the first entry is valid, and the second internal representation is incomplete/invalid
            // return VK_INCOMPLETE with pInternalRepresentationCount--
            if ((*pInternalRepresentationCount == NumberOfInternalRepresentations) &&
                    (pBinaryCode != nullptr))
            {
                // Get the text based ISA disassembly of the shader
                VkResult result = GetRayTracingShaderDisassembly(
                                        Util::Abi::PipelineSymbolType::ShaderDisassembly,
                                        static_cast<size_t>(binarySize),
                                        pBinaryCode,
                                        &(pInternalRepresentations[entry].dataSize),
                                        pInternalRepresentations[entry].pData);

                if (result == VK_SUCCESS)
                {
                    GetShaderDescriptionByStage(pInternalRepresentations[entry].description,
                                                    index, binaryCount);
                    Util::Strncpy(pInternalRepresentations[entry].name, "ISA.", VK_MAX_DESCRIPTION_SIZE);
                    Util::Strncat(pInternalRepresentations[entry].name, VK_MAX_DESCRIPTION_SIZE, pProperty->name);
                }
                else
                {
                    *pInternalRepresentationCount -= 1; // ISA disassembly is incompleted
                }
            }
        }
        else
        {
            *pInternalRepresentationCount = 0;
        }
    }

    // If the requested number of executables was less than the available number of hw stages, return Incomplete
    return (*pInternalRepresentationCount < NumberOfInternalRepresentations) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
// Gets the code object prop according to binary index
const Vkgc::RayTracingShaderProperty* RayTracingPipeline::GetPipelineBinaryPropset(
    uint32_t    index
    ) const
{
    const Vkgc::RayTracingShaderProperty* pRayTracingShaderProperty = nullptr;

    if ((m_pShaderProperty != nullptr) && (index < m_compiledShaderCount))
    {
        pRayTracingShaderProperty = &(m_pShaderProperty[index]);
    }

    return pRayTracingShaderProperty;
}

// =====================================================================================================================
// Create or defer an array of ray tracing pipelines
VkResult RayTracingPipeline::Create(
    Device*                                  pDevice,
    DeferredHostOperation*                   pDeferredOperation,
    PipelineCache*                           pPipelineCache,
    uint32_t                                 count,
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
    const VkAllocationCallbacks*             pAllocator,
    VkPipeline*                              pPipelines)
{
    VkResult finalResult = VK_SUCCESS;
    void*    pObjMem = nullptr;

    DeferredHostOperation::RayTracingPipelineCreateState* pState = nullptr;

    if (pDeferredOperation != nullptr)
    {
        pState = pDeferredOperation->RayTracingPipelineCreate();

        pState->nextPending   = 0;
        pState->completed     = 0;
        pState->finalResult   = static_cast<uint32_t>(VK_SUCCESS);
        pState->skipRemaining = VK_FALSE;

        pState->pPipelineCache = pPipelineCache;
        pState->infoCount      = 0;
        pState->pInfos         = pCreateInfos;
        pState->pAllocator     = pAllocator;
        pState->pPipelines     = pPipelines;

        finalResult = pDeferredOperation->GenerateWorkloads(count);

        if (finalResult == VK_SUCCESS)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                DeferredWorkload* pWorkload = pDeferredOperation->Workload(i);

                pWorkload->totalInstances = UINT_MAX;
                pWorkload->maxInstances = pCreateInfos[i].stageCount + 2;
            }
        }
    }

    if (finalResult == VK_SUCCESS)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            VkResult                                 localResult = VK_SUCCESS;
            const VkRayTracingPipelineCreateInfoKHR* pCreateInfo = &pCreateInfos[i];
            VkPipelineCreateFlags2KHR                flags       =
                Device::GetPipelineCreateFlags(pCreateInfo);

            pObjMem = pDevice->AllocApiObject(
                pAllocator,
                sizeof(RayTracingPipeline));

            if (pObjMem == nullptr)
            {
                localResult = VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            if (localResult == VK_SUCCESS)
            {
                VK_PLACEMENT_NEW(pObjMem) RayTracingPipeline(
                    pDevice);

                pPipelines[i] = RayTracingPipeline::HandleFromVoidPointer(pObjMem);

                if (pState != nullptr)
                {
                    ++pState->infoCount;
                }
                else
                {
                    localResult = ObjectFromHandle(pPipelines[i])->CreateImpl(pPipelineCache,
                        pCreateInfo,
                        flags,
                        pAllocator,
                        nullptr);
                }
            }

            if (localResult != VK_SUCCESS)
            {
                // Free system memory for pipeline object
                if (pPipelines[i] != VK_NULL_HANDLE)
                {
                    ObjectFromHandle(pPipelines[i])->Destroy(pDevice,
                        pAllocator);
                    pPipelines[i] = VK_NULL_HANDLE;
                }

                // In case of failure, VK_NULL_HANDLE must be set
                VK_ASSERT(pPipelines[i] == VK_NULL_HANDLE);

                // Capture the first failure result and save it to be returned
                finalResult = (finalResult != VK_SUCCESS) ? finalResult : localResult;

                if (flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
                {
                    break;
                }
            }
        }
    }

    if (pState != nullptr)
    {
        pDeferredOperation->SetOperation(&DeferredCreateRayTracingPipelineCallback);

        finalResult = (finalResult != VK_SUCCESS) ? finalResult : VK_OPERATION_DEFERRED_KHR;
    }

    return finalResult;
}

// =====================================================================================================================
void RayTracingPipeline::BindToCmdBuffer(
    CmdBuffer*                           pCmdBuffer,
    const Pal::DynamicComputeShaderInfo& computeShaderInfo
    ) const
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};

    params.pipelineBindPoint = Pal::PipelineBindPoint::Compute;
    params.cs                = computeShaderInfo;
    params.apiPsoHash        = m_apiHash;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        params.pPipeline = m_pPalPipeline[deviceIdx];

        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        pPalCmdBuf->CmdBindPipeline(params);

        pCmdBuffer->GetDebugPrintf()->BindPipeline(m_pDevice,
                                                   this,
                                                   deviceIdx,
                                                   pPalCmdBuf,
                                                   static_cast<uint32_t>(Pal::PipelineBindPoint::Compute),
                                                   m_userDataLayout.common.debugPrintfRegBase);

        pCmdBuffer->UpdateLargestPipelineStackSizes(deviceIdx, GetDefaultPipelineStackSizes(deviceIdx));

        // Upload internal buffer data
        if (m_captureReplayVaMappingBufferInfo.dataSize > 0)
        {
            Pal::gpusize gpuAddress = {};
            uint32_t dwordSize = m_captureReplayVaMappingBufferInfo.dataSize / sizeof(uint32_t);
            uint32_t* pCpuAddr = pPalCmdBuf->CmdAllocateEmbeddedData(dwordSize, 1, &gpuAddress);
            memcpy(pCpuAddr, m_captureReplayVaMappingBufferInfo.pData, m_captureReplayVaMappingBufferInfo.dataSize);

            const uint32_t rtCaptureReplayConstBufRegBase = m_userDataLayout.common.rtCaptureReplayConstBufRegBase;

            pPalCmdBuf->CmdSetUserData(Pal::PipelineBindPoint::Compute,
                                       rtCaptureReplayConstBufRegBase,
                                       2,
                                       reinterpret_cast<uint32_t*>(&gpuAddress));
        }
    }
}

// =====================================================================================================================
void RayTracingPipeline::GetRayTracingShaderGroupHandles(
    uint32_t                                    deviceIndex,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData)
{
    const void* pRecords      = static_cast<const void*>(GetShaderGroupHandles(deviceIndex));
    const size_t recordSize   = GpuRt::RayTraceShaderIdentifierByteSize;
    const uint32_t maxGroups  = GetShaderGroupCount();
    const uint32_t copyGroups = Util::Min((firstGroup < maxGroups) ? (maxGroups - firstGroup) : 0, groupCount);
    const size_t   copySize   = Util::Min(dataSize, recordSize * copyGroups);

    Util::FastMemCpy(pData, Util::VoidPtrInc(pRecords, recordSize * firstGroup), copySize);
}

// =====================================================================================================================
ShaderStackSize RayTracingPipeline::GetRayTracingShaderGroupStackSize(
    uint32_t                            deviceIndex,
    uint32_t                            group,
    VkShaderGroupShaderKHR              groupShader,
    VkDeviceSize                        traceRaySize) const
{
    ShaderStackSize stackSize = {};
    if ((group < GetShaderGroupCount()) && (IsInlinedShaderEnabled() == false))
    {
        switch (groupShader)
        {
        case VK_SHADER_GROUP_SHADER_GENERAL_KHR:
            stackSize.size = m_pShaderGroupStackSizes[deviceIndex][group].generalSize;
            break;
        case VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR:
            stackSize.size = m_pShaderGroupStackSizes[deviceIndex][group].closestHitSize;
            break;
        case VK_SHADER_GROUP_SHADER_ANY_HIT_KHR:
            stackSize.size = m_pShaderGroupStackSizes[deviceIndex][group].anyHitSize;
            break;
        case VK_SHADER_GROUP_SHADER_INTERSECTION_KHR:
            stackSize.size = m_pShaderGroupStackSizes[deviceIndex][group].intersectionSize;
            break;
        default:
            VK_NEVER_CALLED();
            break;
        }

        if (m_pShaderGroupStackSizes[deviceIndex][group].metadata.u32All != 0)
        {
            stackSize.size += traceRaySize;
            stackSize.needAddTraceRay = true;
        }
    }

    return stackSize;
}

// =====================================================================================================================
void RayTracingPipeline::UpdatePipelineImplCreateInfo(
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfoIn)
{
    uint32 stageCount  = pCreateInfoIn->stageCount;
    for (uint32 i = 0; i < stageCount; i++)
    {
        m_createInfo.AddToStageList(pCreateInfoIn->pStages[i]);
    }

    uint32 groupCount = pCreateInfoIn->groupCount;
    for (uint32 i = 0; i < groupCount; ++i)
    {
        m_createInfo.AddToGroupList(pCreateInfoIn->pGroups[i]);
    }

    uint32 maxRecursionDepth = pCreateInfoIn->maxPipelineRayRecursionDepth;

    // Set count of stages and groups from API.
    m_createInfo.SetStageCount(stageCount);
    m_createInfo.SetGroupCount(groupCount);
    m_createInfo.SetMaxRecursionDepth(maxRecursionDepth);

    // m_stageList and m_groupList include elements from API and its libs.
    for (uint32 i = 0; ((pCreateInfoIn->pLibraryInfo != nullptr) &&
                        (i < pCreateInfoIn->pLibraryInfo->libraryCount)); ++i)
    {
        VkPipeline pipeline = pCreateInfoIn->pLibraryInfo->pLibraries[i];
        RayTracingPipeline* pPipelineLib = RayTracingPipeline::ObjectFromHandle(pipeline);

        if (pPipelineLib != nullptr)
        {
            const PipelineImplCreateInfo& createInfo = pPipelineLib->GetCreateInfo();
            uint32 libStageCount = createInfo.GetTotalStageCount();
            uint32 libGroupCount = createInfo.GetTotalGroupCount();
            const ShaderStageList& libStageList = createInfo.GetStageList();
            const ShaderGroupList& libGroupList = createInfo.GetGroupList();

            // Merge library createInfo with pipeline createInfo
            for (uint32 cnt = 0; cnt < libGroupCount; cnt++)
            {
                VkRayTracingShaderGroupCreateInfoKHR groupInfo  = libGroupList.At(cnt);
                m_createInfo.AddToGroupList(groupInfo);
            }

            for (uint32 cnt = 0; cnt < libStageCount; cnt++)
            {
                VkPipelineShaderStageCreateInfo stageCreateInfo = libStageList.At(cnt);
                m_createInfo.AddToStageList(stageCreateInfo);
            }

            stageCount += libStageCount;
            groupCount += libGroupCount;
            uint32_t libMaxRecursionDepth = createInfo.GetMaxRecursionDepth();

            maxRecursionDepth = Util::Max(pCreateInfoIn->maxPipelineRayRecursionDepth, libMaxRecursionDepth);
        }
    }
    // Set count of stages and groups from API and its libs.
    m_createInfo.SetTotalStageCount(stageCount);
    m_createInfo.SetTotalGroupCount(groupCount);

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (settings.rtEnableCompilePipelineLibrary == false)
    {
        m_createInfo.SetStageCount(stageCount);
        m_createInfo.SetGroupCount(groupCount);
        m_createInfo.SetMaxRecursionDepth(maxRecursionDepth);
    }
}

// =====================================================================================================================
// Returns literal constants for driver stubs required by GPURT.
void RayTracingPipeline::ConvertStaticPipelineFlags(
    const Device* pDevice,
    uint32_t*     pStaticFlags,
    uint32_t*     pCounterMode,
    uint32_t      pipelineFlags
)
{
    const RuntimeSettings& settings        = pDevice->GetRuntimeSettings();
    GpuRt::TraceRayCounterMode counterMode = pDevice->RayTrace()->TraceRayCounterMode(DefaultDeviceIndex);

    uint32_t staticFlags = pDevice->RayTrace()->GpuRt(DefaultDeviceIndex)->GetStaticPipelineFlags(
        Util::TestAnyFlagSet(pipelineFlags, VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR),
        Util::TestAnyFlagSet(pipelineFlags, VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR),
        pDevice->RayTrace()->AccelStructTrackerEnabled(DefaultDeviceIndex),
        (counterMode != GpuRt::TraceRayCounterMode::TraceRayCounterDisable));

    *pStaticFlags = staticFlags;

    *pCounterMode = static_cast<uint32_t>(counterMode);
}

// =====================================================================================================================
uint32_t RayTracingPipeline::UpdateShaderGroupIndex(
    uint32_t shader,
    uint32_t idx)
{
    return  (shader == VK_SHADER_UNUSED_KHR) ? VK_SHADER_UNUSED_KHR : idx;
}

// =====================================================================================================================
uint32_t RayTracingPipeline::PersistentDispatchSize(
    uint32_t width,
    uint32_t height,
    uint32_t depth
    ) const
{
    const Pal::DispatchDims dispatchSize = GetDispatchSize({ .x = width, .y = height, .z = depth });

    // Groups needed to cover the x, y, and z dimension of a persistent dispatch
    // For large dispatches, this will be limited by the size of the GPU because we want just enough groups to fill it
    // For small dispatches, there will be even fewer groups; don't launch groups that will have nothing to do
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
    const Pal::DeviceProperties& deviceProp = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
    const auto& props = deviceProp.gfxipProperties.shaderCore;
    const uint32 rayDispatchMaxGroups = settings.rtPersistentDispatchRays
                                            ? (props.numAvailableCus * props.numSimdsPerCu * props.numWavefrontsPerSimd)
                                            : 0;
    const uint32 persistentDispatchSize = Util::Min(rayDispatchMaxGroups,
                                                    (dispatchSize.x * dispatchSize.y * dispatchSize.z));

    return persistentDispatchSize;
}

// =====================================================================================================================
Pal::DispatchDims RayTracingPipeline::GetDispatchSize(
    Pal::DispatchDims size) const
{
    Pal::DispatchDims dispatchSize = {};

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    // NOTE: For CPS, we only support flatten thread group so far.
    const uint32_t flattenThreadGroupSize =
        CheckIsCps() ? settings.dispatchRaysThreadGroupSize : settings.rtFlattenThreadGroupSize;

    if (flattenThreadGroupSize == 0)
    {
        dispatchSize.x = Util::RoundUpQuotient(size.x, settings.rtThreadGroupSizeX);
        dispatchSize.y = Util::RoundUpQuotient(size.y, settings.rtThreadGroupSizeY);
        dispatchSize.z = Util::RoundUpQuotient(size.z, settings.rtThreadGroupSizeZ);
    }
    else
    {
        uint32_t x = 0;

        if ((size.x > 1) && (size.y > 1))
        {
            const uint32_t tileHeight = flattenThreadGroupSize / RayTracingTileWidth;
            const uint32_t paddedWidth = Util::Pow2Align(size.x, RayTracingTileWidth);
            const uint32_t paddedHeight = Util::Pow2Align(size.y, tileHeight);

            x = Util::RoundUpQuotient(paddedWidth * paddedHeight, flattenThreadGroupSize);
        }
        else
        {
            x = Util::RoundUpQuotient(size.x * size.y, flattenThreadGroupSize);
        }

        dispatchSize.x = x;
        dispatchSize.y = size.z;
        dispatchSize.z = 1;
    }

    return dispatchSize;
}

// =====================================================================================================================
// Processes capture replay group handle by doing:
//     1. Build a captured VA -> replaying VA mapping buffer.
//     2. Replace group handles with captured ones (to match spec behavior that vkGetRayTracingShaderGroupHandlesKHR
//        should return the captured handles.
VkResult RayTracingPipeline::ProcessCaptureReplayHandles(
    Vkgc::RayTracingShaderIdentifier*     pShaderGroupHandles,
    const VkPipelineLibraryCreateInfoKHR* pLibraryInfo,
    const VkAllocationCallbacks*          pAllocator)
{
    VkResult result = VK_SUCCESS;

    // Newly created group handles should have the same layout as captured group handles
    uint32_t groupCount = m_createInfo.GetGroupCount();

    // Calculate total data size
    uint32_t totalEntryCount = 0;
    Util::Vector<Vkgc::RayTracingCaptureReplayVaMappingEntry, 16, PalAllocator>
        entries(m_pDevice->VkInstance()->Allocator());

    // Use the first entry to store total number of entries.
    entries.PushBack({});

    for (uint32_t i = 0; i < groupCount; i++)
    {
        auto capturedGroupHandle =
            static_cast<const Vkgc::RayTracingShaderIdentifier*>(
                m_createInfo
                .GetGroupList()
                .At(i)
                .pShaderGroupCaptureReplayHandle);
        if (pShaderGroupHandles[i].shaderId != RayTracingInvalidShaderId)
        {
            VK_ASSERT(capturedGroupHandle->shaderId != RayTracingInvalidShaderId);
            entries.PushBack({ capturedGroupHandle->shaderId, pShaderGroupHandles[i].shaderId });
            totalEntryCount++;
        }

        if (pShaderGroupHandles[i].anyHitId != RayTracingInvalidShaderId)
        {
            VK_ASSERT(capturedGroupHandle->anyHitId != RayTracingInvalidShaderId);
            entries.PushBack({ capturedGroupHandle->anyHitId, pShaderGroupHandles[i].anyHitId });
            totalEntryCount++;
        }

        if (pShaderGroupHandles[i].intersectionId != RayTracingInvalidShaderId)
        {
            VK_ASSERT(capturedGroupHandle->intersectionId != RayTracingInvalidShaderId);
            entries.PushBack({ capturedGroupHandle->intersectionId, pShaderGroupHandles[i].intersectionId });
            totalEntryCount++;
        }

        // NOTE: Per spec, vkGetRayTracingShaderGroupHandlesKHR should return identical handles when replaying as
        // capturing. Here we replay the generated handles with given ones, this should be safe as they are unused
        // elsewhere than vkGetRayTracingShaderGroupHandlesKHR.
        pShaderGroupHandles[i].shaderId = capturedGroupHandle->shaderId;
        pShaderGroupHandles[i].anyHitId = capturedGroupHandle->anyHitId;
        pShaderGroupHandles[i].intersectionId = capturedGroupHandle->intersectionId;
    }

    // If there is any library, merge its mapping buffer info into this one.
    if ((m_pDevice->GetRuntimeSettings().rtEnableCompilePipelineLibrary == true) &&
        (pLibraryInfo != nullptr))
    {
        for (uint32_t i = 0; i < pLibraryInfo->libraryCount; i++)
        {
            RayTracingPipeline* pPipelineLib = RayTracingPipeline::ObjectFromHandle(pLibraryInfo->pLibraries[i]);
            if (pPipelineLib == nullptr)
            {
                continue;
            }

            CaptureReplayVaMappingBufferInfo libMappingBufferInfo = pPipelineLib->GetCaptureReplayVaMappingBufferInfo();
            if (libMappingBufferInfo.dataSize > 0)
            {
                Vkgc::RayTracingCaptureReplayVaMappingEntry* libEntries =
                    reinterpret_cast<Vkgc::RayTracingCaptureReplayVaMappingEntry*>(libMappingBufferInfo.pData);
                uint32_t libEntryCount = static_cast<uint32_t>(libEntries->capturedGpuVa);
                ++libEntries;
                for (uint32_t libEntryIdx = 0; libEntryIdx < libEntryCount; libEntryIdx++)
                {
                    entries.PushBack(libEntries[libEntryIdx]);
                    totalEntryCount++;
                }
            }
        }
    }

    entries.At(0).capturedGpuVa = totalEntryCount;
    totalEntryCount++;

    m_captureReplayVaMappingBufferInfo.pData = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        totalEntryCount * sizeof(Vkgc::RayTracingCaptureReplayVaMappingEntry),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (m_captureReplayVaMappingBufferInfo.pData == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        m_captureReplayVaMappingBufferInfo.dataSize =
            totalEntryCount * sizeof(Vkgc::RayTracingCaptureReplayVaMappingEntry);
        memcpy(m_captureReplayVaMappingBufferInfo.pData, entries.Data(), m_captureReplayVaMappingBufferInfo.dataSize);
    }

    return result;
}

// =====================================================================================================================
void RayTracingPipeline::HandleExtensionStructs(
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
    RayTracingPipelineExtStructs*            pExtStructs)
{
    // Handle common extension structs
    Pipeline::HandleExtensionStructs(pCreateInfo->pNext, pExtStructs);

    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<int32>(pHeader->sType))
        {
        // Handle extension specific structures

        default:
            break;
        }
        pNext = pHeader->pNext;
    }
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingShaderGroupHandlesKHR(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData)
{
    RayTracingPipeline* pPipeline = RayTracingPipeline::ObjectFromHandle(pipeline);

    pPipeline->GetRayTracingShaderGroupHandles(DefaultDeviceIndex, firstGroup, groupCount, dataSize, pData);

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkDeviceSize VKAPI_CALL vkGetRayTracingShaderGroupStackSizeKHR(
    VkDevice                                           device,
    VkPipeline                                         pipeline,
    uint32_t                                           group,
    VkShaderGroupShaderKHR                             groupShader)
{
    RayTracingPipeline* pPipeline = RayTracingPipeline::ObjectFromHandle(pipeline);

    ShaderStackSize stackSize = pPipeline->GetRayTracingShaderGroupStackSize(DefaultDeviceIndex, group, groupShader, 0);

    return stackSize.size;
}

}; // namespace entry
}; // namespace vk
