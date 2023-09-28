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
* @file  compiler_solution.cpp
* @brief Contains implementation of CompilerSolution
***********************************************************************************************************************
*/
#include "include/compiler_solution.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"

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
    : m_pPhysicalDevice(pPhysicalDevice)
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
    m_gfxIp      = gfxIp;
    m_gfxIpLevel = gfxIpLevel;

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
    const Device*   pDevice,
    Vkgc::RtState*  pRtState)
{
    VkResult result              = VK_ERROR_UNKNOWN;
    GpuRt::IDevice* pGpurtDevice = pDevice->RayTrace()->GpuRt(DefaultDeviceIndex);

    if (pGpurtDevice != nullptr)
    {
        const RuntimeSettings& settings = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings();
        auto pTable                     = &pRtState->gpurtFuncTable;

        Pal::RayTracingIpLevel rayTracingIp =
            pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.rayTracingIp;

        // Optionally, override RTIP level based on software emulation setting
        switch (settings.emulatedRtIpLevel)
        {
        case EmulatedRtIpLevelNone:
            break;
        case HardwareRtIpLevel1_1:
        case EmulatedRtIpLevel1_1:
            rayTracingIp = Pal::RayTracingIpLevel::RtIp1_1;
            break;
#if VKI_BUILD_GFX11
        case EmulatedRtIpLevel2_0:
            rayTracingIp = Pal::RayTracingIpLevel::RtIp2_0;
            break;
#endif
        default:
            VK_ASSERT(false);
            break;
        }

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
