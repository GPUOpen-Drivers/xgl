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
* @file  compiler_solution.cpp
* @brief Contains implementation of CompilerSolution
***********************************************************************************************************************
*/
#include "include/compiler_solution.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline_cache.h"

#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#endif

#include <climits>
#include <cmath>

namespace vk
{
    // =====================================================================================================================
CompilerSolution::CompilerSolution(
    PhysicalDevice* pPhysicalDevice)
    :
    m_pPhysicalDevice(pPhysicalDevice),
    m_gplCacheMatrix{}
{

}

// =====================================================================================================================
CompilerSolution::~CompilerSolution()
{

}

// =====================================================================================================================
// Initialize CompilerSolution class
VkResult CompilerSolution::Initialize(
    Vkgc::GfxIpVersion   gfxIp,
    Pal::GfxIpLevel      gfxIpLevel,
    PipelineBinaryCache* pCache)
{
    m_gfxIp        = gfxIp;
    m_gfxIpLevel   = gfxIpLevel;
    m_pBinaryCache = pCache;
    return VK_SUCCESS;
}

// =====================================================================================================================
// Gets the name string of shader stage.
const char* CompilerSolution::GetShaderStageName(
    ShaderStage shaderStage)
{
    const char* pName = nullptr;

    VK_ASSERT(shaderStage < ShaderStageCount);

    static const char* ShaderStageNames[] =
    {
        "Task    ",
        "Vertex  ",
        "Tessellation control",
        "Tessellation evaluation",
        "Geometry",
        "Mesh    ",
        "Fragment",
        "Compute ",
#if VKI_RAY_TRACING
        "Raygen",
        "Intersect",
        "Anyhit",
        "Closesthit",
        "Miss",
        "Callable"
#endif
    };

    pName = ShaderStageNames[static_cast<uint32_t>(shaderStage)];

    return pName;
}

// =====================================================================================================================
// Gets the name string of graphics library type.
const char* CompilerSolution::GetGraphicsLibraryName(
    GraphicsLibraryType libraryType)
{
    const char* pName = nullptr;

    VK_ASSERT(libraryType < GraphicsLibraryCount);

    static const char* GraphicsLibraryTypeNames[] =
    {
        "PreRasterLib",
        "FragmentLib",
        "ColorExportLib"
    };

    pName = GraphicsLibraryTypeNames[static_cast<uint32_t>(libraryType)];

    return pName;
}

// =====================================================================================================================
// Helper to disable all NGG culling options
void CompilerSolution::DisableNggCulling(
    Vkgc::NggState* pNggState)
{
    pNggState->enableBackfaceCulling     = false;
    pNggState->enableFrustumCulling      = false;
    pNggState->enableBoxFilterCulling    = false;
    pNggState->enableSphereCulling       = false;
    pNggState->enableSmallPrimFilter     = false;
    pNggState->enableCullDistanceCulling = false;
}

// =====================================================================================================================
void CompilerSolution::LoadShaderBinaryFromCache(
    PipelineCache*               pPipelineCache,
    const Util::MetroHash::Hash* pCacheId,
    Vkgc::BinaryData*            pCacheBinary,
    bool*                        pHitCache,
    bool*                        pHitAppCache)
{
    Util::Result result = Util::Result::NotFound;
    if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
    {
        auto pAppCache = pPipelineCache->GetPipelineCache();
        result = pAppCache->LoadPipelineBinary(pCacheId, &pCacheBinary->codeSize, &pCacheBinary->pCode);
    }

    *pHitAppCache = (result == Util::Result::Success);
    if (result != Util::Result::Success)
    {
        if (m_pBinaryCache != nullptr)
        {
            result = m_pBinaryCache->LoadPipelineBinary(pCacheId, &pCacheBinary->codeSize, &pCacheBinary->pCode);
        }
    }

    m_gplCacheMatrix.cacheAttempts++;
    if (result == Util::Result::Success)
    {
        m_gplCacheMatrix.cacheHits++;
    }

    *pHitCache = (result == Util::Result::Success);
}

// =====================================================================================================================
template<class ShaderLibraryBlobHeader>
void CompilerSolution::StoreShaderBinaryToCache(
    PipelineCache*                       pPipelineCache,
    const Util::MetroHash::Hash*         pCacheId,
    const ShaderLibraryBlobHeader*       pHeader,
    const void*                          pBlob,
    const void*                          pFragmentMeta,
    bool                                 hitCache,
    bool                                 hitAppCache,
    Vkgc::BinaryData*                    pCacheBinary)
{
    // Update app pipeline cache when cache is  available and flag hitAppCache is false
    bool updateAppCache = (hitAppCache == false) &&
                          (pPipelineCache != nullptr) &&
                          (pPipelineCache->GetPipelineCache() != nullptr);

    bool updateBinaryCache = false;
    if (m_pBinaryCache != nullptr)
    {
        if (hitAppCache)
        {
            Util::QueryResult queryResult = {};
            updateBinaryCache =
                (m_pBinaryCache->QueryPipelineBinary(pCacheId, 0, &queryResult) != Util::Result::Success);
        }
        else
        {
            updateBinaryCache = (hitCache == false);
        }
    }

    if (updateBinaryCache || updateAppCache || (pCacheBinary->pCode == nullptr))
    {
        if (((pHeader->binaryLength > 0) || (pHeader->requireFullPipeline)) && (pCacheBinary->codeSize == 0))
        {
            size_t cacheSize = sizeof(ShaderLibraryBlobHeader) + pHeader->binaryLength + pHeader->fragMetaLength;

            void* pBuffer = m_pPhysicalDevice->VkInstance()->AllocMem(
                cacheSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            if (pBuffer != nullptr)
            {
                memcpy(pBuffer, pHeader, sizeof(ShaderLibraryBlobHeader));
                if (pBlob != nullptr)
                {
                    memcpy(Util::VoidPtrInc(pBuffer, sizeof(ShaderLibraryBlobHeader)), pBlob, pHeader->binaryLength);
                }
                if (pFragmentMeta != nullptr)
                {
                    memcpy(Util::VoidPtrInc(pBuffer, sizeof(ShaderLibraryBlobHeader) + pHeader->binaryLength),
                          pFragmentMeta,
                          pHeader->fragMetaLength);
                }
                pCacheBinary->codeSize = cacheSize;
                pCacheBinary->pCode = pBuffer;
            }
        }

        if (pCacheBinary->codeSize > 0)
        {
            if (updateBinaryCache)
            {
                m_pBinaryCache->StorePipelineBinary(pCacheId, pCacheBinary->codeSize, pCacheBinary->pCode);
            }

            if (updateAppCache)
            {
                pPipelineCache->GetPipelineCache()->StorePipelineBinary(
                    pCacheId, pCacheBinary->codeSize, pCacheBinary->pCode);
            }
        }
    }
}

template void CompilerSolution::StoreShaderBinaryToCache<LlpcShaderLibraryBlobHeader>(
    PipelineCache*                       pPipelineCache,
    const Util::MetroHash::Hash*         pCacheId,
    const LlpcShaderLibraryBlobHeader*   pHeader,
    const void*                          pBlob,
    const void*                          pFragmentMeta,
    bool                                 hitCache,
    bool                                 hitAppCache,
    Vkgc::BinaryData*                    pCacheBinary);

#if VKI_RAY_TRACING
// =====================================================================================================================
// Parse and update RayTracingFunctionName of funcType
void CompilerSolution::SetRayTracingFunctionName(
    const char* pSrc,
    char*       pDest)
{
    VK_ASSERT(pSrc != nullptr);
    VK_ASSERT(pDest != nullptr);

    // format is "\01?RayQueryProceed1_1@@YA_NURayQueryInternal@@IV?$vector@I$02@@@Z"
    // input  "\01?RayQueryProceed1_1@@YA_NURayQueryInternal@@IV?$vector@I$02@@@Z"
    // output "RayQueryProceed1_1"
    const char* pTemp = pSrc + 2;

    const char* pPostfix = strstr(pTemp, "@@");
    if (pPostfix != nullptr)
    {
        size_t sz = strlen(pTemp) - strlen(pPostfix);
        strncpy(pDest, pTemp, sz);
        pDest[sz] = '\0';
    }
    else
    {
        VK_ASSERT(false);
    }
}

// =====================================================================================================================
// Parse and update RayTracingFunctionName of all funcTypes
void CompilerSolution::UpdateRayTracingFunctionNames(
    const Device*          pDevice,
    Pal::RayTracingIpLevel rayTracingIp,
    Vkgc::RtState*         pRtState)
{
    VkResult result              = VK_ERROR_UNKNOWN;
    GpuRt::IDevice* pGpurtDevice = pDevice->RayTrace()->GpuRt(DefaultDeviceIndex);

    if (pGpurtDevice != nullptr)
    {
        const RuntimeSettings& settings = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();
        auto pTable                     = &pRtState->gpurtFuncTable;

        GpuRt::EntryFunctionTable entryFuncTable = {};
        result = PalToVkResult(pGpurtDevice->QueryRayTracingEntryFunctionTable(rayTracingIp, &entryFuncTable));

        VK_ASSERT(result == VK_SUCCESS);

        SetRayTracingFunctionName(
            entryFuncTable.traceRay.pTraceRay, pTable->pFunc[Vkgc::RT_ENTRY_TRACE_RAY]);
        SetRayTracingFunctionName(
            entryFuncTable.traceRay.pTraceRayUsingHitToken, pTable->pFunc[Vkgc::RT_ENTRY_TRACE_RAY_HIT_TOKEN]);
        SetRayTracingFunctionName(
            entryFuncTable.rayQuery.pTraceRayInline, pTable->pFunc[Vkgc::RT_ENTRY_TRACE_RAY_INLINE]);
        SetRayTracingFunctionName(
            entryFuncTable.rayQuery.pProceed, pTable->pFunc[Vkgc::RT_ENTRY_RAY_QUERY_PROCEED]);
        SetRayTracingFunctionName(
            entryFuncTable.intrinsic.pGetInstanceID, pTable->pFunc[Vkgc::RT_ENTRY_INSTANCE_ID]);
        SetRayTracingFunctionName(
            entryFuncTable.intrinsic.pGetInstanceIndex, pTable->pFunc[Vkgc::RT_ENTRY_INSTANCE_INDEX]);
        SetRayTracingFunctionName(
            entryFuncTable.intrinsic.pGetObjectToWorldTransform,
            pTable->pFunc[Vkgc::RT_ENTRY_OBJECT_TO_WORLD_TRANSFORM]);
        SetRayTracingFunctionName(
            entryFuncTable.intrinsic.pGetWorldToObjectTransform,
            pTable->pFunc[Vkgc::RT_ENTRY_WORLD_TO_OBJECT_TRANSFORM]);
        SetRayTracingFunctionName(entryFuncTable.intrinsic.pFetchTrianglePositionFromNodePointer,
                                  pTable->pFunc[Vkgc::RT_ENTRY_FETCH_HIT_TRIANGLE_FROM_NODE_POINTER]);
        SetRayTracingFunctionName(entryFuncTable.intrinsic.pFetchTrianglePositionFromRayQuery,
                                  pTable->pFunc[Vkgc::RT_ENTRY_FETCH_HIT_TRIANGLE_FROM_RAY_QUERY]);
        SetRayTracingFunctionName(entryFuncTable.rayQuery.pGet64BitInstanceNodePtr,
                                  pTable->pFunc[Vkgc::RT_ENTRY_GET_INSTANCE_NODE]);
    }
}

uint32_t CompilerSolution::GetRayTracingVgprLimit(
    bool isIndirect)
{
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    uint32_t               vgprLimit = 0;

    if (isIndirect)
    {
        if (settings.rtIndirectVgprLimit == UINT_MAX)
        {
            const auto *pProps = &m_pPhysicalDevice->PalProperties().gfxipProperties.shaderCore;

            const uint32 targetNumWavesPerSimd =
                Util::Max(1U, static_cast<uint32>(
                    round(settings.indirectCallTargetOccupancyPerSimd * pProps->numWavefrontsPerSimd)));

            const uint32 targetNumVgprsPerWave =
                Util::RoundDownToMultiple(pProps->vgprsPerSimd / targetNumWavesPerSimd, pProps->vgprAllocGranularity);

            vgprLimit = Util::Min(pProps->numAvailableVgprs, targetNumVgprsPerWave);
        }
        else
        {
            vgprLimit = settings.rtIndirectVgprLimit;
        }
    }
    else
    {
        vgprLimit = settings.rtUnifiedVgprLimit;
    }

    return vgprLimit;
}
#endif
}
