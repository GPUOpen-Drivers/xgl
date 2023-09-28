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

#include "devmode/devmode_mgr.h"

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_memory.h"
#include "include/vk_deferred_operation.h"
#include "include/vk_utils.h"

#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/ray_tracing_device.h"
#include "ray_tracing_util.h"

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
            VK_ASSERT((pCreateInfo->pStages[apiGroupInfo.generalShader].stage &
                       (VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                        VK_SHADER_STAGE_MISS_BIT_KHR |
                        VK_SHADER_STAGE_CALLABLE_BIT_KHR)) != 0);
            stages |= pCreateInfo->pStages[apiGroupInfo.generalShader].stage;
            break;
        case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
            VK_ASSERT(apiGroupInfo.intersectionShader != VK_SHADER_UNUSED_KHR);
            VK_ASSERT(pCreateInfo->pStages[apiGroupInfo.intersectionShader].stage ==
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
            stages |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            // falls through
        case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
            if (apiGroupInfo.closestHitShader != VK_SHADER_UNUSED_KHR)
            {
                VK_ASSERT(pCreateInfo->pStages[apiGroupInfo.closestHitShader].stage ==
                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
                stages |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            }
            if (apiGroupInfo.anyHitShader != VK_SHADER_UNUSED_KHR)
            {
                VK_ASSERT(pCreateInfo->pStages[apiGroupInfo.anyHitShader].stage ==
                          VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
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
    PipelineCreateFlags                      flags,
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

        if (pCreateInfo->pLibraryInterface != nullptr)
        {
            GenerateHashFromRayTracingPipelineInterfaceCreateInfo(*pCreateInfo->pLibraryInterface, &apiHasher);
        }
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
    CreateInfo*                              pOutInfo)
{
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pOutInfo->pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    pOutInfo->immedInfo.computeShaderInfo.maxWavesPerCu        = settings.maxWavesPerCu;
    pOutInfo->immedInfo.computeShaderInfo.maxThreadGroupsPerCu = settings.maxThreadGroupsPerCu;
    pOutInfo->immedInfo.computeShaderInfo.tgScheduleCountPerCu = settings.tgScheduleCountPerCu;
}

// =====================================================================================================================
PipelineImplCreateInfo::PipelineImplCreateInfo(
    Device* const   pDevice)
    : m_stageCount(0),
        m_stageList(pDevice->VkInstance()->Allocator()),
        m_groupCount(0),
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
    m_elfHash{},
    m_captureReplayVaMappingBufferInfo{}
{
    memset(m_pShaderGroupHandles, 0, sizeof(Vkgc::RayTracingShaderIdentifier*) * MaxPalDevices);
    memset(m_pShaderGroupStackSizes, 0, sizeof(ShaderGroupStackSizes*) * MaxPalDevices);
    memset(m_traceRayGpuVas, 0, sizeof(gpusize) * MaxPalDevices);
    memset(m_defaultPipelineStackSizes, 0, sizeof(uint32_t) * MaxPalDevices);
}

// =====================================================================================================================
void RayTracingPipeline::Init(
    Pal::IPipeline**                     ppPalPipeline,
    uint32_t                             shaderLibraryCount,
    Pal::IShaderLibrary**                ppPalShaderLibrary,
    const PipelineLayout*                pPipelineLayout,
    const ShaderOptimizerKey*            pShaderOptKeys,
    const ImmedInfo&                     immedInfo,
    uint64_t                             staticStateMask,
    uint32_t                             nativeShaderCount,
    uint32_t                             totalShaderCount,
    uint32_t                             shaderGroupCount,
    Vkgc::RayTracingShaderIdentifier*    pShaderGroupHandles[MaxPalDevices],
    ShaderGroupStackSizes*               pShaderGroupStackSizes[MaxPalDevices],
    ShaderGroupInfo*                     pShaderGroupInfos,
    uint32_t                             attributeSize,
    gpusize                              traceRayGpuVas[MaxPalDevices],
    uint32_t                             dispatchRaysUserDataOffset,
    const Util::MetroHash::Hash&         cacheHash,
    uint64_t                             apiHash,
    const Util::MetroHash::Hash&         elfHash)
{
    Pipeline::Init(
        ppPalPipeline,
        pPipelineLayout,
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
}

// =====================================================================================================================
VkResult RayTracingPipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
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
    PipelineCreateFlags                      flags,
    const VkAllocationCallbacks*             pAllocator,
    DeferredWorkload*                        pDeferredWorkload)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    // Setup PAL create info from Vulkan inputs
    CreateInfo                         localPipelineInfo      = {};
    RayTracingPipelineBinaryCreateInfo binaryCreateInfo       = {};
    VkResult                           result                 = VkResult::VK_SUCCESS;
    const RuntimeSettings&             settings               = m_pDevice->GetRuntimeSettings();

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

        // If rtEnableCompilePipelineLibrary is false, the library shaders have been included in pCreateInfo.
        const bool hasLibraries = settings.rtEnableCompilePipelineLibrary &&
            ((pCreateInfo->pLibraryInfo != nullptr) && (pCreateInfo->pLibraryInfo->libraryCount > 0));

        PipelineCompiler* pDefaultCompiler = m_pDevice->GetCompiler(DefaultDeviceIndex);

        Util::MetroHash::Hash elfHash    = {};
        uint64_t              apiPsoHash = {};
        BuildApiHash(pCreateInfo, flags, &elfHash, &apiPsoHash);

        binaryCreateInfo.pDeferredWorkload = pDeferredWorkload;
        binaryCreateInfo.apiPsoHash        = apiPsoHash;

        const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo = nullptr;
        pDefaultCompiler->GetPipelineCreationFeedback(static_cast<const VkStructHeader*>(pCreateInfo->pNext),
                                                      &pPipelineCreationFeedbackCreateInfo);

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

                result = BuildShaderStageInfo(m_pDevice,
                                              nativeShaderCount,
                                              pCreateInfo->pStages,
                                              false,
                                              [](const uint32_t inputIdx, const uint32_t stageIdx)
                                              {
                                                  return inputIdx;
                                              },
                                              shaderInfo.pStages,
                                              pTempModules,
                                              pPipelineCache,
                                              nullptr);
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        if (result == VK_SUCCESS)
        {
            uint32_t shaderIdx       = 0;
            optimizerKey.shaderCount = totalShaderCount;

            for (; shaderIdx < nativeShaderCount; ++shaderIdx)
            {
                const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
                    ShaderModule::GetFirstValidShaderData(shaderInfo.pStages[shaderIdx].pModuleHandle));

                m_pDevice->GetShaderOptimizer()->CreateShaderOptimizerKey(
                    pModuleData,
                    shaderInfo.pStages[shaderIdx].codeHash,
                    shaderInfo.pStages[shaderIdx].stage,
                    shaderInfo.pStages[shaderIdx].codeSize,
                    &optimizerKey.pShaders[shaderIdx]);
            }

            if (hasLibraries)
            {
                for (uint32_t libraryIdx = 0; libraryIdx < pCreateInfo->pLibraryInfo->libraryCount; ++libraryIdx)
                {
                    const auto pLibrary = RayTracingPipeline::ObjectFromHandle(
                        pCreateInfo->pLibraryInfo->pLibraries[libraryIdx]);
                    const auto shaderCount = pLibrary->GetTotalShaderCount();

                    memcpy(&optimizerKey.pShaders[shaderIdx],
                           pLibrary->GetShaderOptKeys(),
                           sizeof(ShaderOptimizerKey) * shaderCount);
                    shaderIdx += shaderCount;
                }
            }

            VK_ASSERT(shaderIdx == totalShaderCount);
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

                    const auto   pImportedShaderLibrary = pPipelineLib->PalShaderLibrary(deviceIdx);
                    const auto   pImportedFuncInfoList  = pImportedShaderLibrary->GetShaderLibFunctionList();
                    const uint32 numFunctions           = pImportedShaderLibrary->GetShaderLibFunctionCount();

                    // We only use one shader library per collection function
                    VK_ASSERT((pImportedFuncInfoList != nullptr) && (numFunctions == 1));

                    // update group count
                    pipelineLibGroupCount += pPipelineLib->GetShaderGroupCount();
                }
            }
        }

        const uint32_t totalGroupCount = pipelineCreateInfo.groupCount + pipelineLibGroupCount;

        RayTracingPipelineBinary          pipelineBinary[MaxPalDevices] = {};
        Vkgc::RayTracingShaderIdentifier* pShaderGroups [MaxPalDevices] = {};

        if (totalGroupCount > 0)
        {
            const size_t shaderGroupArraySize = totalGroupCount * GpuRt::RayTraceShaderIdentifierByteSize;

            pShaderGroups[0] = static_cast<Vkgc::RayTracingShaderIdentifier*>(
                pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    shaderGroupArraySize * m_pDevice->NumPalDevices(),
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

            if (pShaderGroups[0] == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            if (pipelineCreateInfo.groupCount > 0)
            {
                pipelineBinary[0].shaderGroupHandle.shaderHandles     = pShaderGroups[0];
                pipelineBinary[0].shaderGroupHandle.shaderHandleCount = pipelineCreateInfo.groupCount;
            }

            for (uint32_t deviceIdx = 1; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
            {
                pShaderGroups[deviceIdx] = pShaderGroups[deviceIdx - 1] + totalGroupCount;
                if (pipelineCreateInfo.groupCount > 0)
                {
                    pipelineBinary[deviceIdx].shaderGroupHandle.shaderHandles     = pShaderGroups[deviceIdx];
                    pipelineBinary[deviceIdx].shaderGroupHandle.shaderHandleCount = pipelineCreateInfo.groupCount;
                }
            }
        }

        const uint32_t                  maxFunctionCount  = pipelineCreateInfo.stageCount + 1;
        Pal::ShaderLibraryFunctionInfo* pIndirectFuncInfo = nullptr;
        uint32_t*                       pShaderNameMap    = nullptr;
        VkDeviceSize*                   pShaderStackSize  = nullptr;
        bool*                           pTraceRayUsage    = nullptr;
        void*                           pTempBuffer       = nullptr;

        {
            // Allocate temp buffer for shader name and indirect functions
            const uint32_t maxPipelineBinaryCount = maxFunctionCount + 1;

            auto placement = utils::PlacementHelper<6>(
                nullptr,

                utils::PlacementElement<Vkgc::RayTracingShaderProperty>{
                    &pipelineBinary[0].shaderPropSet.shaderProps,
                    maxFunctionCount * m_pDevice->NumPalDevices()},

                utils::PlacementElement<Vkgc::BinaryData>{
                    &pipelineBinary[0].pPipelineBins,
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

                pipelineBinary[0].shaderPropSet.shaderCount = maxFunctionCount;
                pipelineBinary[0].pipelineBinCount          = maxPipelineBinaryCount;

                for (uint32_t deviceIdx = 1; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
                {
                    const auto  pBinary    = &pipelineBinary[deviceIdx];
                    const auto& prevBinary = pipelineBinary[deviceIdx - 1];

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

        Util::MetroHash::Hash cacheId[MaxPalDevices] = {};

        const auto pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                      : nullptr;

        for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < m_pDevice->NumPalDevices()); deviceIdx++)
        {
            bool isUserCacheHit     = false;
            bool isInternalCacheHit = false;

            // PAL Pipeline caching
            Util::Result cacheResult = Util::Result::NotFound;

            ElfHashToCacheId(
                m_pDevice,
                deviceIdx,
                elfHash,
                m_pDevice->VkPhysicalDevice(deviceIdx)->GetSettingsLoader()->GetSettingsHash(),
                optimizerKey,
                &cacheId[deviceIdx]
            );

            bool forceCompilation = m_pDevice->GetRuntimeSettings().enablePipelineDump;
            if (forceCompilation == false)
            {
                Vkgc::BinaryData cachedBinData = {};

                // Search the pipeline binary cache.
                cacheResult = pDefaultCompiler->GetCachedPipelineBinary(
                    &cacheId[deviceIdx],
                    pPipelineBinaryCache,
                    &cachedBinData.codeSize,
                    &cachedBinData.pCode,
                    &isUserCacheHit,
                    &isInternalCacheHit,
                    &binaryCreateInfo.freeCompilerBinary,
                    &binaryCreateInfo.pipelineFeedback);

                // Found the pipeline; Add it to any cache layers where it's missing.
                if (cacheResult == Util::Result::Success)
                {
                    m_pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                        &cacheId[deviceIdx],
                        pPipelineBinaryCache,
                        cachedBinData.codeSize,
                        cachedBinData.pCode,
                        isUserCacheHit,
                        isInternalCacheHit);

                    // Unpack the cached blob into separate binaries.
                    pDefaultCompiler->ExtractRayTracingPipelineBinary(
                        &cachedBinData,
                        &pipelineBinary[deviceIdx]);
                }
            }

            // Compile if unable to retrieve from cache.
            if (cacheResult != Util::Result::Success)
            {
                result = pDefaultCompiler->ConvertRayTracingPipelineInfo(
                    m_pDevice,
                    &pipelineCreateInfo,
                    flags,
                    &shaderInfo,
                    &optimizerKey,
                    &binaryCreateInfo);

                if (result == VK_SUCCESS)
                {
                    result = pDefaultCompiler->CreateRayTracingPipelineBinary(
                        m_pDevice,
                        deviceIdx,
                        pPipelineCache,
                        &binaryCreateInfo,
                        &pipelineBinary[deviceIdx],
                        &cacheId[deviceIdx]);
                }

                // Add the pipeline to any cache layer where it's missing.
                if (result == VK_SUCCESS)
                {
                    Vkgc::BinaryData cachedBinData = {};

                    // Join the binaries into a single blob.
                    pDefaultCompiler->BuildRayTracingPipelineBinary(
                        &pipelineBinary[deviceIdx],
                        &cachedBinData);

                    if (cachedBinData.pCode != nullptr)
                    {
                        m_pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                            &cacheId[deviceIdx],
                            pPipelineBinaryCache,
                            cachedBinData.codeSize,
                            cachedBinData.pCode,
                            isUserCacheHit,
                            isInternalCacheHit);

                        m_pDevice->VkInstance()->FreeMem(const_cast<void*>(cachedBinData.pCode));
                    }
                }
            }

            // Copy shader groups if compiler doesn't use pre-allocated buffer.
            const auto& groupHandle = pipelineBinary[deviceIdx].shaderGroupHandle;
            if (groupHandle.shaderHandles != pShaderGroups[deviceIdx])
            {
                memcpy(
                    pShaderGroups[deviceIdx],
                    groupHandle.shaderHandles,
                    sizeof(Vkgc::RayTracingShaderIdentifier) * groupHandle.shaderHandleCount);
            }
        }

        m_hasTraceRay = pipelineBinary[DefaultDeviceIndex].hasTraceRay;

        uint32_t funcCount = 0;
        if (result == VK_SUCCESS)
        {
            const auto pShaderProp = &pipelineBinary[DefaultDeviceIndex].shaderPropSet.shaderProps[0];
            const uint32_t shaderCount = pipelineBinary[DefaultDeviceIndex].shaderPropSet.shaderCount;
            for (uint32_t i = 0; i < shaderCount; i++)
            {
                if (pShaderProp[i].shaderId != RayTracingInvalidShaderId)
                {
                    ++funcCount;
                }
            }

            ConvertRayTracingPipelineInfo(m_pDevice, &pipelineCreateInfo, &localPipelineInfo);

            // Override pipeline creation parameters based on pipeline profile
            m_pDevice->GetShaderOptimizer()->OverrideComputePipelineCreateInfo(
                optimizerKey,
                &localPipelineInfo.immedInfo.computeShaderInfo);
        }

        size_t pipelineSize      = 0;
        size_t shaderLibrarySize = 0;
        void*  pSystemMem        = nullptr;

        // Create the PAL pipeline object.
        Pal::IShaderLibrary**   ppShaderLibraries                     = nullptr;
        ShaderGroupInfo*        pShaderGroupInfos                     = nullptr;
        Pal::IPipeline*         pPalPipeline          [MaxPalDevices] = {};
        ShaderGroupStackSizes*  pShaderGroupStackSizes[MaxPalDevices] = {};
        gpusize                 traceRayGpuVas        [MaxPalDevices] = {};

        size_t pipelineMemSize              = 0;
        size_t shaderLibraryMemSize         = 0;
        size_t shaderLibraryPalMemSize      = 0;
        size_t shaderGroupStackSizesMemSize = 0;
        size_t shaderGroupInfosMemSize      = 0;

        const size_t shaderOptKeysSize = optimizerKey.shaderCount * sizeof(ShaderOptimizerKey);

        if (result == VK_SUCCESS)
        {
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

            const size_t totalSize =
                pipelineMemSize +
                shaderLibraryMemSize +
                shaderLibraryPalMemSize +
                shaderGroupStackSizesMemSize +
                shaderGroupInfosMemSize +
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

            PopulateShaderGroupInfos(pCreateInfo, pShaderGroupInfos, totalGroupCount);

            // Transfer shader optimizer keys to permanent storage.
            memcpy(pShaderOptKeys, optimizerKey.pShaders, shaderOptKeysSize);
            optimizerKey.pShaders = static_cast<ShaderOptimizerKey*>(pShaderOptKeys);

            for (uint32_t deviceIdx = 0;
                ((deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
                deviceIdx++)
            {
                const auto pBinaries               = pipelineBinary[deviceIdx].pPipelineBins;
                const auto ppDeviceShaderLibraries = ppShaderLibraries + deviceIdx * funcCount;
                void*      pDeviceShaderLibraryMem = Util::VoidPtrInc(pPalShaderLibraryMem,
                                                                      deviceIdx * funcCount * shaderLibrarySize);

                VK_ASSERT(pipelineSize ==
                    m_pDevice->PalDevice(deviceIdx)->GetComputePipelineSize(localPipelineInfo.pipeline, nullptr));

                // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
                // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
                if (pBinaries[0].pCode != nullptr)
                {
                    localPipelineInfo.pipeline.flags.clientInternal = false;
                    localPipelineInfo.pipeline.pipelineBinarySize   = pBinaries[0].codeSize;
                    localPipelineInfo.pipeline.pPipelineBinary      = pBinaries[0].pCode;
                    localPipelineInfo.pipeline.maxFunctionCallDepth = pipelineBinary[deviceIdx].maxFunctionCallDepth;
                }

                // Copy indirect function info
                uint32_t       funcIndex           = 0;
                const auto     pShaderProp         = &pipelineBinary[deviceIdx].shaderPropSet.shaderProps[0];
                const uint32_t traceRayShaderIndex = pipelineBinary[deviceIdx].shaderPropSet.traceRayIndex;
                const uint32_t shaderCount         = pipelineBinary[deviceIdx].shaderPropSet.shaderCount;

                for (uint32_t i = 0; i < shaderCount; i++)
                {
                    if (pShaderProp[i].shaderId != RayTracingInvalidShaderId)
                    {
                        pIndirectFuncInfo[funcIndex].pSymbolName = pShaderProp[i].name;
                        pIndirectFuncInfo[funcIndex].gpuVirtAddr = 0;
                        pTraceRayUsage[funcIndex]                = pShaderProp[i].hasTraceRay;
                        pShaderNameMap[i]                        = funcIndex;
                        ++funcIndex;
                    }
                }
                VK_ASSERT(funcIndex == funcCount);

                if (result == VK_SUCCESS)
                {
                    palResult = m_pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                        localPipelineInfo.pipeline,
                        Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                        &pPalPipeline[deviceIdx]);
                }

                // The size of stack is per native thread. So that stack size have to be multiplied by 2
                // if a Wave64 shader that needs scratch buffer is used.
                uint32_t stackSizeFactor = 0;
                if (palResult == Util::Result::Success)
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
                    for (uint32_t i = 0; i < funcCount; ++i)
                    {
                        VK_ASSERT((pBinaries[i + 1].pCode != nullptr) && (pBinaries[i + 1].codeSize != 0));
                        Pal::ShaderLibraryCreateInfo shaderLibraryCreateInfo = {};
                        shaderLibraryCreateInfo.pCodeObject = pBinaries[i + 1].pCode;
                        shaderLibraryCreateInfo.codeObjectSize = pBinaries[i + 1].codeSize;
                        shaderLibraryCreateInfo.pFuncList = &pIndirectFuncInfo[i];
                        shaderLibraryCreateInfo.funcCount = 1;

                        palResult = m_pDevice->PalDevice(deviceIdx)->CreateShaderLibrary(
                            shaderLibraryCreateInfo,
                            Util::VoidPtrInc(pDeviceShaderLibraryMem, shaderLibrarySize * i),
                            &ppDeviceShaderLibraries[i]);
                    }

                    if (palResult == Util::Result::Success)
                    {
                        palResult = pPalPipeline[deviceIdx]->LinkWithLibraries(ppDeviceShaderLibraries, funcCount);
                    }

                    // Used by calculation of default pipeline stack size
                    uint32_t rayGenStackMax       = 0;
                    uint32_t anyHitStackMax       = 0;
                    uint32_t closestHitStackMax   = 0;
                    uint32_t missStackMax         = 0;
                    uint32_t intersectionStackMax = 0;
                    uint32_t callableStackMax     = 0;

                    if ((palResult == Util::Result::Success) && ((funcCount > 0) || hasLibraries))
                    {
                        pShaderGroupStackSizes[deviceIdx]
                            = static_cast<ShaderGroupStackSizes*>(
                                Util::VoidPtrInc(pShaderGroupsStackSizesMem,
                                                 deviceIdx * totalGroupCount * sizeof(ShaderGroupStackSizes)));
                        memset(pShaderStackSize, 0xff, sizeof(VkDeviceSize)* maxFunctionCount);

                        auto GetFuncStackSize = [&](uint32_t shaderIdx) -> VkDeviceSize
                        {
                            VkDeviceSize stackSize = 0;
                            if (shaderIdx != VK_SHADER_UNUSED_KHR)
                            {
                                const uint32_t funcIdx = pShaderNameMap[shaderIdx];
                                if (funcIdx < funcCount)
                                {
                                    if (pShaderStackSize[funcIdx] == ~0ULL)
                                    {
                                        Pal::ShaderLibStats shaderStats = {};
                                        ppDeviceShaderLibraries[funcIdx]->GetShaderFunctionStats(
                                            pIndirectFuncInfo[funcIdx].pSymbolName,
                                            &shaderStats);
                                        pShaderStackSize[funcIdx] = shaderStats.stackFrameSizeInBytes *
                                                                    stackSizeFactor;

                                        if (pTraceRayUsage[funcIdx])
                                        {
                                            const uint32_t traceRayFuncIdx = pShaderNameMap[traceRayShaderIndex];
                                            if ((pShaderStackSize[traceRayFuncIdx] == ~0ULL) &&
                                                (ppDeviceShaderLibraries[traceRayFuncIdx] != nullptr))
                                            {
                                                Pal::ShaderLibStats traceRayShaderStats = {};
                                                ppDeviceShaderLibraries[traceRayFuncIdx]->GetShaderFunctionStats(
                                                    pIndirectFuncInfo[traceRayFuncIdx].pSymbolName,
                                                    &traceRayShaderStats);
                                                pShaderStackSize[traceRayFuncIdx] =
                                                    traceRayShaderStats.stackFrameSizeInBytes * stackSizeFactor;
                                            }
                                            pShaderStackSize[funcIdx] += pShaderStackSize[traceRayFuncIdx];
                                        }
                                    }
                                    VK_ASSERT(pShaderStackSize[funcIdx] != ~0ULL);
                                    stackSize = pShaderStackSize[funcIdx];
                                }
                            }
                            return stackSize;
                        };

                        for (uint32_t groupIdx = 0; groupIdx < m_createInfo.GetGroupCount(); ++groupIdx)
                        {
                            const auto& groupInfo          = m_createInfo.GetGroupList().At(groupIdx);
                            const auto  pCurrentStackSizes = &pShaderGroupStackSizes[deviceIdx][groupIdx];

                            switch (groupInfo.type)
                            {
                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
                                pCurrentStackSizes->generalSize = GetFuncStackSize(groupInfo.generalShader);
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

                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
                                pCurrentStackSizes->anyHitSize     = GetFuncStackSize(groupInfo.anyHitShader);
                                pCurrentStackSizes->closestHitSize = GetFuncStackSize(groupInfo.closestHitShader);
                                anyHitStackMax = Util::Max(
                                    anyHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->anyHitSize));
                                closestHitStackMax = Util::Max(
                                    closestHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->closestHitSize));
                                break;

                            case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
                                pCurrentStackSizes->anyHitSize       = GetFuncStackSize(groupInfo.anyHitShader);
                                pCurrentStackSizes->closestHitSize   = GetFuncStackSize(groupInfo.closestHitShader);
                                pCurrentStackSizes->intersectionSize = GetFuncStackSize(groupInfo.intersectionShader);
                                anyHitStackMax = Util::Max(
                                    anyHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->anyHitSize));
                                closestHitStackMax = Util::Max(
                                    closestHitStackMax, static_cast<uint32_t>(pCurrentStackSizes->closestHitSize));
                                intersectionStackMax = Util::Max(
                                    intersectionStackMax, static_cast<uint32_t>(pCurrentStackSizes->intersectionSize));
                                break;

                            default:
                                VK_NEVER_CALLED();
                                break;
                            }
                        }
                    }

                    if (funcCount > 0)
                    {
                        for (uint32_t i = 0; i < pipelineCreateInfo.groupCount; ++i)
                        {
                            const auto pGroup = &pShaderGroups[deviceIdx][i];
                            bool       found = false;
                            found |= MapShaderIdToShaderHandle(pIndirectFuncInfo,
                                                               pShaderNameMap,
                                                               shaderCount,
                                                               pShaderProp,
                                                               &pGroup->shaderId);
                            found |= MapShaderIdToShaderHandle(pIndirectFuncInfo,
                                                               pShaderNameMap,
                                                               shaderCount,
                                                               pShaderProp,
                                                               &pGroup->intersectionId);
                            found |= MapShaderIdToShaderHandle(pIndirectFuncInfo,
                                                               pShaderNameMap,
                                                               shaderCount,
                                                               pShaderProp,
                                                               &pGroup->anyHitId);
                            VK_ASSERT(found && "Failed to map shader to gpu address");
                        }
                    }

                    // now appending the pipeline library data
                    gpusize      pipelineLibTraceRayVa  = 0;
                    bool         pipelineHasTraceRay    = false;
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

                            // Link all shader libraries from imported pipeline library
                            const auto ppImortedShaderLibraries = pPipelineLib->GetShaderLibraries();
                            const uint32 importedLibraryCount =
                                pPipelineLib->GetShaderLibraryCount() / m_pDevice->NumPalDevices();
                            const auto ppImportedDeviceShaderLibraries =
                                ppImortedShaderLibraries + deviceIdx * importedLibraryCount;

                            palResult = pPalPipeline[deviceIdx]->LinkWithLibraries(ppImportedDeviceShaderLibraries,
                                                                                   importedLibraryCount);

                            if (palResult == Util::Result::Success)
                            {
                                // update group count
                                const uint32_t pipelineGroupCount   = pPipelineLib->GetShaderGroupCount();
                                const auto pPipelineLibShaderGroups = pPipelineLib->GetShaderGroupHandles(deviceIdx);
                                const auto pLibGroupInfos           = pPipelineLib->GetShaderGroupInfos();

                                // update pipelineLibHasTraceRay and pipelineLibTraceRayVa
                                pipelineHasTraceRay = pPipelineLib->CheckHasTraceRay();
                                if (pipelineHasTraceRay)
                                {
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

                                    switch (pLibGroupInfos[libGroupIdx].type)
                                    {
                                    case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
                                        pStackSizes->generalSize =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_GENERAL_KHR);
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
                                        pStackSizes->intersectionSize =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_INTERSECTION_KHR);
                                        intersectionStackMax = Util::Max(
                                            intersectionStackMax,
                                            static_cast<uint32_t>(pStackSizes->intersectionSize));
                                        //
                                        // falls through
                                        //
                                    case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
                                        pStackSizes->anyHitSize =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_ANY_HIT_KHR);
                                        pStackSizes->closestHitSize =
                                            pPipelineLib->GetRayTracingShaderGroupStackSize(
                                                deviceIdx,
                                                libGroupIdx,
                                                VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR);
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
                        }
                    }

                    // Calculate the default pipeline size via spec definition
                    m_defaultPipelineStackSizes[deviceIdx] =
                        rayGenStackMax +
                        (Util::Min(1U, m_createInfo.GetMaxRecursionDepth()) *
                         Util::Max(closestHitStackMax, missStackMax, intersectionStackMax + anyHitStackMax)) +
                        (Util::Max(0U, m_createInfo.GetMaxRecursionDepth()) *
                         Util::Max(closestHitStackMax, missStackMax)) +
                        (2 * callableStackMax);

                    // TraceRay is the last function in function list, record it regardless we are building library or
                    // not, so that a pipeline will get its own TraceRayGpuVa correctly.
                    if (funcCount > 0)
                    {
                        const auto traceRayFuncIndex = funcCount - 1;
                        traceRayGpuVas[deviceIdx]    = pIndirectFuncInfo[traceRayFuncIndex].gpuVirtAddr;
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
#if ICD_GPUOPEN_DEVMODE_BUILD
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
#endif
            }

            result = PalToVkResult(palResult);
        }

        if (result == VK_SUCCESS)
        {
            uint32_t dispatchRaysUserDataOffset = localPipelineInfo.pLayout->GetDispatchRaysUserData();

            Init(pPalPipeline,
                 funcCount * m_pDevice->NumPalDevices(),
                 ppShaderLibraries,
                 localPipelineInfo.pLayout,
                 optimizerKey.pShaders,
                 localPipelineInfo.immedInfo,
                 localPipelineInfo.staticStateMask,
                 nativeShaderCount,
                 totalShaderCount,
                 totalGroupCount,
                 pShaderGroups,
                 pShaderGroupStackSizes,
                 pShaderGroupInfos,
                 binaryCreateInfo.maxAttributeSize,
                 traceRayGpuVas,
                 dispatchRaysUserDataOffset,
                 cacheId[DefaultDeviceIndex],
                 apiPsoHash,
                 elfHash);
            if (settings.enableDebugPrintf)
            {
                ClearFormatString();
                for (uint32_t i = 0; i < pipelineBinary[DefaultDeviceIndex].pipelineBinCount; ++i)
                {
                    DebugPrintf::DecodeFormatStringsFromElf(
                        m_pDevice,
                        pipelineBinary[DefaultDeviceIndex].pPipelineBins[i].codeSize,
                        static_cast<const char*>(pipelineBinary[DefaultDeviceIndex].pPipelineBins[i].pCode),
                        GetFormatStrings());
                }
            }
        }
        else
        {
            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                // Internal memory allocation failed, free PAL event object if it gets created
                if (pPalPipeline[deviceIdx] != nullptr)
                {
                    pPalPipeline[deviceIdx]->Destroy();
                }
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
        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
        {
            m_pDevice->GetCompiler(deviceIdx)->FreeRayTracingPipelineBinary(
                &binaryCreateInfo,
                &pipelineBinary[deviceIdx]);
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

            pDefaultCompiler->SetPipelineCreationFeedbackInfo(
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
        uint32_t index = Util::AtomicIncrement(&pState->nextPending) - 1;

        const bool firstThread = (index == 0);

        // Run in a loop until we've processed all pipeline create infos. Parallel joins in their own loops can
        // consume iterations. A single "main" thread per pipeline is sent out here. These threads will not return
        // untill the pipeline has been fully created (unlike the helper worker threads).
        while (index < pState->infoCount)
        {
            VkResult                                 localResult = VK_SUCCESS;
            const VkRayTracingPipelineCreateInfoKHR* pCreateInfo = &pState->pInfos[index];
            PipelineCreateFlags                      flags       =
                Device::GetPipelineCreateFlags(pCreateInfo);

            if (pState->skipRemaining == VK_FALSE)
            {
                RayTracingPipeline* pPipeline = RayTracingPipeline::ObjectFromHandle(pState->pPipelines[index]);

                localResult = pPipeline->CreateImpl(pState->pPipelineCache,
                                                    pCreateInfo,
                                                    flags,
                                                    pState->pAllocator,
                                                    pOperation->Workload(index));

#if ICD_GPUOPEN_DEVMODE_BUILD
                if (localResult == VK_SUCCESS)
                {
                    DevModeMgr* pDevMgr = pDevice->VkInstance()->GetDevModeMgr();

                    if (pDevMgr != nullptr)
                    {
                        pDevMgr->PipelineCreated(pDevice, pPipeline);

                        if (pPipeline->IsInlinedShaderEnabled() == false)
                        {
                            pDevMgr->ShaderLibrariesCreated(pDevice, pPipeline);
                        }
                    }
                }
#endif
            }

            if (localResult != VK_SUCCESS)
            {
                Util::AtomicCompareAndSwap(&pState->finalResult,
                                           static_cast<uint32_t>(VK_SUCCESS),
                                           static_cast<uint32_t>(localResult));

                if (pCreateInfo->flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
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
            PipelineCreateFlags                      flags       =
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

        uint32_t debugPrintfRegBase = (m_userDataLayout.scheme == PipelineLayoutScheme::Compact) ?
            m_userDataLayout.compact.debugPrintfRegBase : m_userDataLayout.indirect.debugPrintfRegBase;
        pCmdBuffer->GetDebugPrintf()->BindPipeline(m_pDevice,
                                                   this,
                                                   deviceIdx,
                                                   pPalCmdBuf,
                                                   static_cast<uint32_t>(Pal::PipelineBindPoint::Compute),
                                                   debugPrintfRegBase);

        // Upload internal buffer data
        if (m_captureReplayVaMappingBufferInfo.dataSize > 0)
        {
            Pal::gpusize gpuAddress = {};
            uint32_t dwordSize = m_captureReplayVaMappingBufferInfo.dataSize / sizeof(uint32_t);
            uint32_t* pCpuAddr = pPalCmdBuf->CmdAllocateEmbeddedData(dwordSize, 1, &gpuAddress);
            memcpy(pCpuAddr, m_captureReplayVaMappingBufferInfo.pData, m_captureReplayVaMappingBufferInfo.dataSize);

            uint32_t rtCaptureReplayConstBufRegBase = (m_userDataLayout.scheme == PipelineLayoutScheme::Compact) ?
                m_userDataLayout.compact.rtCaptureReplayConstBufRegBase :
                m_userDataLayout.indirect.rtCaptureReplayConstBufRegBase;

            pPalCmdBuf->CmdSetUserData(Pal::PipelineBindPoint::Compute,
                                       rtCaptureReplayConstBufRegBase,
                                       2,
                                       reinterpret_cast<uint32_t*>(&gpuAddress));
        }
    }
}

// =====================================================================================================================
void RayTracingPipeline::BindNullPipeline(
    CmdBuffer* pCmdBuffer)
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint       = Pal::PipelineBindPoint::Compute;
    params.apiPsoHash              = Pal::InternalApiPsoHash;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

// =====================================================================================================================
bool RayTracingPipeline::MapShaderIdToShaderHandle(
    Pal::ShaderLibraryFunctionInfo*   pIndirectFuncList,
    uint32_t*                         pShaderNameMap,
    uint32_t                          shaderPropCount,
    Vkgc::RayTracingShaderProperty*   pShaderProp,
    uint64_t*                         pShaderId)
{
    bool found = false;
    if (*pShaderId != RayTracingInvalidShaderId)
    {
        for (uint32_t i = 0; i < shaderPropCount; i++)
        {
            if (pShaderProp[i].shaderId == *pShaderId)
            {
                auto pIndirectFunc = &pIndirectFuncList[pShaderNameMap[i]];
                VK_ASSERT(pIndirectFunc->pSymbolName == &pShaderProp[i].name[0]);
                uint64_t gpuVirtAddr = pIndirectFunc->gpuVirtAddr;
                if (pShaderProp[i].onlyGpuVaLo)
                {
                    gpuVirtAddr = gpuVirtAddr & 0xffffffff;
                }
                *pShaderId = gpuVirtAddr | pShaderProp[i].shaderIdExtraBits;
                found = true;
                break;
            }
        }
    }
    return found;
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
VkDeviceSize RayTracingPipeline::GetRayTracingShaderGroupStackSize(
    uint32_t                            deviceIndex,
    uint32_t                            group,
    VkShaderGroupShaderKHR              groupShader) const
{
    VkDeviceSize stackSize = 0;

    if ((group < GetShaderGroupCount()) && (IsInlinedShaderEnabled() == false))
    {
        switch (groupShader)
        {
        case VK_SHADER_GROUP_SHADER_GENERAL_KHR:
            stackSize = m_pShaderGroupStackSizes[deviceIndex][group].generalSize;
            break;
        case VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR:
            stackSize = m_pShaderGroupStackSizes[deviceIndex][group].closestHitSize;
            break;
        case VK_SHADER_GROUP_SHADER_ANY_HIT_KHR:
            stackSize = m_pShaderGroupStackSizes[deviceIndex][group].anyHitSize;
            break;
        case VK_SHADER_GROUP_SHADER_INTERSECTION_KHR:
            stackSize = m_pShaderGroupStackSizes[deviceIndex][group].intersectionSize;
            break;
        default:
            VK_NEVER_CALLED();
            break;
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

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
    if (settings.rtEnableCompilePipelineLibrary == false)
    {
        // if the library contains other library,
        // and driver decided not to compile pipeline library as a shader library
        // then needs to merge them first
        for (uint32 i = 0; ((pCreateInfoIn->pLibraryInfo != nullptr) &&
                            (i < pCreateInfoIn->pLibraryInfo->libraryCount)); ++i)
        {
            VkPipeline pipeline = pCreateInfoIn->pLibraryInfo->pLibraries[i];
            RayTracingPipeline* pPipelineLib = RayTracingPipeline::ObjectFromHandle(pipeline);

            if (pPipelineLib != nullptr)
            {
                const PipelineImplCreateInfo& createInfo = pPipelineLib->GetCreateInfo();
                uint32 libStageCount = createInfo.GetStageCount();
                uint32 libGroupCount = createInfo.GetGroupCount();
                const ShaderStageList& libStageList = createInfo.GetStageList();
                const ShaderGroupList& libGroupList = createInfo.GetGroupList();

                // Merge library createInfo with pipeline createInfo
                VK_ASSERT(libStageCount == libGroupCount);
                for (uint32 cnt = 0; cnt < libGroupCount; cnt++)
                {
                    const uint32 shaderNdx = stageCount + cnt;
                    VkPipelineShaderStageCreateInfo stageCreateInfo = libStageList.At(cnt);
                    VkRayTracingShaderGroupCreateInfoKHR groupInfo  = libGroupList.At(cnt);

                    switch (stageCreateInfo.stage)
                    {
                    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                    case VK_SHADER_STAGE_MISS_BIT_KHR:
                    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                        groupInfo.generalShader = UpdateShaderGroupIndex(groupInfo.generalShader, shaderNdx);
                        break;
                    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
                        groupInfo.anyHitShader = UpdateShaderGroupIndex(groupInfo.anyHitShader, shaderNdx);
                        break;
                    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                        groupInfo.closestHitShader = UpdateShaderGroupIndex(groupInfo.closestHitShader, shaderNdx);
                        break;
                    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
                        groupInfo.intersectionShader = UpdateShaderGroupIndex(groupInfo.intersectionShader, shaderNdx);
                        break;
                    default:
                        break;
                    }

                    m_createInfo.AddToStageList(stageCreateInfo);
                    m_createInfo.AddToGroupList(groupInfo);
                }

                stageCount += libStageCount;
                groupCount += libGroupCount;
                uint32_t libMaxRecursionDepth = createInfo.GetMaxRecursionDepth();

                maxRecursionDepth = Util::Max(pCreateInfoIn->maxPipelineRayRecursionDepth, libMaxRecursionDepth);
            }
        }
    }
    // Will need to repack things together after integrate the library data
    m_createInfo.SetStageCount(stageCount);
    m_createInfo.SetGroupCount(groupCount);
    m_createInfo.SetMaxRecursionDepth(maxRecursionDepth);
}

// =====================================================================================================================
// Returns literal constants for driver stubs required by GPURT.
void RayTracingPipeline::ConvertStaticPipelineFlags(
    const Device* pDevice,
    uint32_t*     pStaticFlags,
    uint32_t*     pTriangleCompressMode,
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

    *pTriangleCompressMode = static_cast<uint32_t>(ConvertGpuRtTriCompressMode(settings.rtTriangleCompressionMode));

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
void RayTracingPipeline::GetDispatchSize(
    uint32_t* pDispatchSizeX,
    uint32_t* pDispatchSizeY,
    uint32_t* pDispatchSizeZ,
    uint32_t  width,
    uint32_t  height,
    uint32_t  depth) const
{
    VK_ASSERT((pDispatchSizeX != nullptr) && (pDispatchSizeY != nullptr) && (pDispatchSizeZ != nullptr));

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (settings.rtFlattenThreadGroupSize == 0)
    {
        *pDispatchSizeX = Util::RoundUpQuotient(width,  settings.rtThreadGroupSizeX);
        *pDispatchSizeY = Util::RoundUpQuotient(height, settings.rtThreadGroupSizeY);
        *pDispatchSizeZ = Util::RoundUpQuotient(depth,  settings.rtThreadGroupSizeZ);
    }
    else
    {
        uint32_t dispatchSize = 0;

        if ((width > 1) && (height > 1))
        {
            const uint32_t tileHeight = settings.rtFlattenThreadGroupSize / RayTracingTileWidth;
            const uint32_t paddedWidth = Util::Pow2Align(width, RayTracingTileWidth);
            const uint32_t paddedHeight = Util::Pow2Align(height, tileHeight);

            dispatchSize = Util::RoundUpQuotient(paddedWidth * paddedHeight, settings.rtFlattenThreadGroupSize);
        }
        else
        {
            dispatchSize = Util::RoundUpQuotient(width * height, settings.rtFlattenThreadGroupSize);
        }

        *pDispatchSizeX = dispatchSize;
        *pDispatchSizeY = depth;
        *pDispatchSizeZ = 1;
    }
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

    return pPipeline->GetRayTracingShaderGroupStackSize(DefaultDeviceIndex, group, groupShader);
}

}; // namespace entry
}; // namespace vk
