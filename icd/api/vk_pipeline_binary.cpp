/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_pipeline_binary.h"
#include "include/vk_device.h"
#include "include/vk_pipeline.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_graphics_pipeline.h"
#include "include/graphics_pipeline_common.h"
#if VKI_RAY_TRACING
#include "raytrace/vk_ray_tracing_pipeline.h"
#endif

#include "palPlatformKey.h"

namespace vk
{
// =====================================================================================================================
PipelineBinary::PipelineBinary(
    const Util::MetroHash::Hash&                binaryKey,
    const Vkgc::BinaryData&                     binaryData)
    :
    m_binaryKey(binaryKey),
    m_binaryData(binaryData)
{
}

// =====================================================================================================================
// Create a pipeline binary object.
VkResult PipelineBinary::Create(
    Device*                                     pDevice,
    const Util::MetroHash::Hash&                binaryKey,
    const Vkgc::BinaryData&                     binaryData,
    const bool                                  isGplPipelineFromInternalCache,
    const GraphicsLibraryType                   gplType,
    const Util::MetroHash::Hash*                pElfHash,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineBinaryKHR*                        pPipelineBinary)
{
    VK_ASSERT(pPipelineBinary != nullptr);

    VkResult           result       = VK_SUCCESS;
    size_t             storageSize  = sizeof(PipelineBinary) + binaryData.codeSize;
    constexpr uint32_t MetadataSize = sizeof(PipelineBinaryGplMetadata);

    if (isGplPipelineFromInternalCache)
    {
        storageSize += MetadataSize;
    }

    void* pMemory = pDevice->AllocApiObject(pAllocator, storageSize);

    if (pMemory == nullptr)
    {
        result           = VK_ERROR_OUT_OF_HOST_MEMORY;
        *pPipelineBinary = VK_NULL_HANDLE;
    }
    else
    {
        void*  pCode    = Util::VoidPtrInc(pMemory, sizeof(PipelineBinary));
        size_t codeSize = binaryData.codeSize;

        if (isGplPipelineFromInternalCache)
        {
            // GPL binaries used to create PipelineBinary objects that are first retained with the pipeline have
            // metadata packed with them when they are retained.  Metadata doesn't need to be added here unless
            // the GPL binaries come from internal cache, where the associated metadata needed for PipelineBinary
            // objects is not packed together with the binary at the time that it is stored to cache.
            VK_ASSERT ((gplType != GraphicsLibraryCount) && (pElfHash != nullptr));

            // Store GPL metadata needed for pipeline binary with the GPL binary
            *static_cast<PipelineBinaryGplMetadata*>(pCode) =
                {
                    .gplType = static_cast<GraphicsLibraryType>(gplType),
                    .elfHash = *pElfHash
                };
            pCode = Util::VoidPtrInc(pCode, MetadataSize);

            codeSize += MetadataSize;
        }

        memcpy(pCode, binaryData.pCode, binaryData.codeSize);

        Vkgc::BinaryData objectBinaryData
        {
            .codeSize = codeSize,
            .pCode    = Util::VoidPtrInc(pMemory, sizeof(PipelineBinary))
        };

        PipelineBinary* pObject = VK_PLACEMENT_NEW(pMemory) PipelineBinary(binaryKey, objectBinaryData);

        *pPipelineBinary = PipelineBinary::HandleFromVoidPointer(pObject);
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineBinary::CreatePipelineBinaries(
    Device*                                     pDevice,
    const VkPipelineBinaryCreateInfoKHR*        pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineBinaryHandlesInfoKHR*             pBinaries)
{
    VK_ASSERT((pCreateInfo != nullptr) && (pBinaries != nullptr));

    VkResult finalResult = VK_SUCCESS;

    if (pCreateInfo->pKeysAndDataInfo != nullptr)
    {
        if (pBinaries->pPipelineBinaries == nullptr)
        {
            pBinaries->pipelineBinaryCount = pCreateInfo->pKeysAndDataInfo->binaryCount;
        }
        else
        {
            uint32 binariesCopiedCount = 0;

            for (uint32_t binaryIndex = 0;
                 (binaryIndex < pCreateInfo->pKeysAndDataInfo->binaryCount);
                 ++binaryIndex)
            {
                Util::MetroHash::Hash binaryKey = {};
                ReadFromPipelineBinaryKey(pCreateInfo->pKeysAndDataInfo->pPipelineBinaryKeys[binaryIndex], &binaryKey);

                const Vkgc::BinaryData binaryData =
                    {
                        .codeSize = pCreateInfo->pKeysAndDataInfo->pPipelineBinaryData[binaryIndex].dataSize,
                        .pCode    = pCreateInfo->pKeysAndDataInfo->pPipelineBinaryData[binaryIndex].pData
                    };

                VkResult result = PipelineBinary::Create(
                    pDevice,
                    binaryKey,
                    binaryData,
                    false,
                    GraphicsLibraryCount,
                    nullptr,
                    pAllocator,
                    &pBinaries->pPipelineBinaries[binaryIndex]);

                if (result == VK_SUCCESS)
                {
                    ++binariesCopiedCount;
                }
                else if (finalResult == VK_SUCCESS)
                {
                    // Keep the first failed result, but attempt to create the remaining pipeline binaries
                    finalResult = result;
                }
            }

            pBinaries->pipelineBinaryCount = binariesCopiedCount;

        }
    }
    else if (pCreateInfo->pipeline != VK_NULL_HANDLE)
    {
        const PipelineBinaryStorage* pBinaryStorage =
            Pipeline::BaseObjectFromHandle(pCreateInfo->pipeline)->GetBinaryStorage();

        if (pBinaryStorage != nullptr)
        {
            if (pBinaries->pPipelineBinaries == nullptr)
            {
                pBinaries->pipelineBinaryCount = pBinaryStorage->binaryCount;
            }
            else
            {
                uint32 binariesCopiedCount = 0;

                for (uint32_t binaryIndex = 0;
                     (binaryIndex < pBinaries->pipelineBinaryCount);
                     ++binaryIndex)
                {
                    VkResult result = PipelineBinary::Create(
                        pDevice,
                        pBinaryStorage->binaryInfo[binaryIndex].binaryHash,
                        pBinaryStorage->binaryInfo[binaryIndex].pipelineBinary,
                        false,
                        GraphicsLibraryCount,
                        nullptr,
                        pAllocator,
                        &pBinaries->pPipelineBinaries[binaryIndex]);

                    if (result == VK_SUCCESS)
                    {
                        ++binariesCopiedCount;
                    }
                    else if (finalResult == VK_SUCCESS)
                    {
                        // Keep the first failed result, but attempt to create the remaining pipeline binaries
                        finalResult = result;
                    }
                }

                pBinaries->pipelineBinaryCount = binariesCopiedCount;
            }
        }
        else
        {
            // Pipeline didn't enable VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR.
            finalResult = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    else if (pCreateInfo->pPipelineCreateInfo != nullptr)
    {
        // Determine if this pPipelineCreateInfo is for a GPL pipeline, and if so, generate a gplMask
        uint32_t   gplMask       = 0;
        const bool isGplPipeline = GenerateGplMask(pDevice, pCreateInfo, &gplMask);

        if (pBinaries->pPipelineBinaries == nullptr)
        {
            // Report the binary count for the provided pipeline create info

            if (isGplPipeline)
            {
                // GPL pipelines require a binary per library
                pBinaries->pipelineBinaryCount = Util::CountSetBits(gplMask);
            }
            else
            {
                // Monolithic pipelines require a binary per device
                pBinaries->pipelineBinaryCount = pDevice->NumPalDevices();
            }
        }
        else
        {
            // Create pipeline binaries from internal cache entries.
            Util::MetroHash::Hash elfHash                              = {};
            Util::MetroHash::Hash cacheIds[MaxPipelineBinaryInfoCount] = {};
            uint32_t              binariesCreatedCount                 = 0;

            // Calculate the cacheIds for this pPipelineCreateInfo
            PipelineBinary::CreateCacheId(pDevice, pCreateInfo->pPipelineCreateInfo, &elfHash, cacheIds);

            // Perform cache lookups and create pipeline binaries using binaries retrieved from cache
            if (isGplPipeline)
            {
                finalResult = PipelineBinary::CreateGplPipelineBinaryFromCache(
                    pDevice,
                    pAllocator,
                    cacheIds,
                    &elfHash,
                    gplMask,
                    pBinaries,
                    &binariesCreatedCount);
            }
            else
            {
                finalResult = PipelineBinary::CreateMonolithicPipelineBinaryFromCache(
                    pDevice,
                    pAllocator,
                    cacheIds,
                    pBinaries,
                    &binariesCreatedCount);
            }

            VK_ASSERT((binariesCreatedCount == pBinaries->pipelineBinaryCount) || (finalResult != VK_SUCCESS));

            pBinaries->pipelineBinaryCount = binariesCreatedCount;
        }
    }
    else
    {
        finalResult = VK_ERROR_INITIALIZATION_FAILED;
        VK_NEVER_CALLED();
    }

    return finalResult;
}

// =====================================================================================================================
bool PipelineBinary::GenerateGplMask(
    const Device*                               pDevice,
    const VkPipelineBinaryCreateInfoKHR*        pCreateInfo,
    uint32_t*                                   pGplMask)
{
    bool isGplPipeline = false;

    // Check if the provided pipeline create info is for a graphics pipeline
    if ((static_cast<const VkStructHeader*>(pCreateInfo->pPipelineCreateInfo->pNext)->sType) ==
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)
    {
        const VkGraphicsPipelineCreateInfo* pGraphicsCreateInfo =
            static_cast<const VkGraphicsPipelineCreateInfo*>(pCreateInfo->pPipelineCreateInfo->pNext);

        const VkPipelineCreateFlags2KHR flags = Device::GetPipelineCreateFlags(pGraphicsCreateInfo);

        // Check if the graphics pipeline create info is for a GPL
        if ((flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR) != 0)
        {
            isGplPipeline = true;

            GraphicsPipelineExtStructs  extStructs = {};
            GraphicsPipelineLibraryInfo libInfo    = {};

            GraphicsPipelineCommon::HandleExtensionStructs(pGraphicsCreateInfo, &extStructs);
            GraphicsPipelineCommon::ExtractLibraryInfo(pDevice, pGraphicsCreateInfo, extStructs, flags, &libInfo);

            // Calculate a mask for the requested GPL library types
            if ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)
            {
                *pGplMask |= (1 << GraphicsLibraryPreRaster);
            }

            if ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0)
            {
                *pGplMask |= (1 << GraphicsLibraryFragment);
            }
        }
    }

    return isGplPipeline;
}

// =====================================================================================================================
VkResult PipelineBinary::CreateGplPipelineBinaryFromCache(
    Device*                                     pDevice,
    const VkAllocationCallbacks*                pAllocator,
    const Util::MetroHash::Hash*                pCacheIds,
    const Util::MetroHash::Hash*                pElfHash,
    const uint32_t                              gplMask,
    VkPipelineBinaryHandlesInfoKHR*             pBinaries,
    uint32_t*                                   pBinariesCreatedCount)
{
    VkResult finalResult = VK_SUCCESS;

    PipelineCompiler*          pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);
    const PipelineCompilerType compilerType     =
        pDefaultCompiler->CheckCompilerType<Vkgc::GraphicsPipelineBuildInfo>(nullptr, 0, 0);
    CompilerSolution*          pSolution    = pDefaultCompiler->GetSolution(compilerType);
    Vkgc::BinaryData           pipelineBinary   = {};

    for (uint32_t gplType = 0; gplType < GraphicsLibraryCount; ++gplType)
    {
        if (Util::TestAnyFlagSet(gplMask, 1 << gplType))
        {
            bool hitCache    = false;
            bool hitAppCache = false;

            pSolution->LoadShaderBinaryFromCache(
                nullptr,
                &pCacheIds[gplType],
                &pipelineBinary,
                &hitCache,
                &hitAppCache);

            if (hitCache || hitAppCache)
            {
                VkResult result = PipelineBinary::Create(
                    pDevice,
                    pCacheIds[gplType],
                    pipelineBinary,
                    true,
                    static_cast<GraphicsLibraryType>(gplType),
                    pElfHash,
                    pAllocator,
                    &pBinaries->pPipelineBinaries[*pBinariesCreatedCount]);

                if (result == VK_SUCCESS)
                {
                    *pBinariesCreatedCount += 1;
                }
                else if (finalResult == VK_SUCCESS)
                {
                    // Keep the first failed result, but attempt to create remaining pipeline binaries
                    finalResult = result;
                }
            }
            else
            {
                finalResult = VK_PIPELINE_BINARY_MISSING_KHR;
            }
        }
    }

    return finalResult;
}

// =====================================================================================================================
VkResult PipelineBinary::CreateMonolithicPipelineBinaryFromCache(
    Device*                                     pDevice,
    const VkAllocationCallbacks*                pAllocator,
    const Util::MetroHash::Hash*                pCacheIds,
    VkPipelineBinaryHandlesInfoKHR*             pBinaries,
    uint32_t*                                   pBinariesCreatedCount)
{
    VkResult           finalResult         = VK_SUCCESS;
    PipelineCompiler*  pDefaultCompiler    = pDevice->GetCompiler(DefaultDeviceIndex);
    Vkgc::BinaryData   pipelineBinary      = {};
    bool               isUserCacheHit      = false;
    bool               isInternalCacheHit  = false;
    FreeCompilerBinary freeCompilerBinary  = FreeWithCompiler;

    Util::Result cacheResult = pDefaultCompiler->GetCachedPipelineBinary(
        &pCacheIds[0],
        nullptr,                // pPipelineBinaryCache
        &pipelineBinary,
        &isUserCacheHit,
        &isInternalCacheHit,
        &freeCompilerBinary,
        nullptr);               // pPipelineFeedback

    if (cacheResult == Util::Result::Success)
    {
        for (uint32_t binaryIndex = 0; binaryIndex < pBinaries->pipelineBinaryCount; ++binaryIndex)
        {
            VkResult result = PipelineBinary::Create(
                pDevice,
                pCacheIds[binaryIndex],
                pipelineBinary,
                false,
                GraphicsLibraryCount,
                nullptr,
                pAllocator,
                &pBinaries->pPipelineBinaries[binaryIndex]);

            if (result == VK_SUCCESS)
            {
                *pBinariesCreatedCount += 1;
            }
            else if (finalResult == VK_SUCCESS)
            {
                // Keep the first failed result, but attempt to create remaining pipeline binaries
                finalResult = result;
            }
        }
    }
    else
    {
        finalResult = VK_PIPELINE_BINARY_MISSING_KHR;
    }

    return finalResult;
}

// =====================================================================================================================
VkResult PipelineBinary::DestroyPipelineBinary(
    Device*                                     pDevice,
    const VkAllocationCallbacks*                pAllocator)
{
    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult PipelineBinary::GetPipelineKey(
    const Device*                               pDevice,
    const VkPipelineCreateInfoKHR*              pPipelineCreateInfo,
    VkPipelineBinaryKeyKHR*                     pPipelineBinaryKey)
{
    VkResult result = VK_SUCCESS;

    if (pPipelineCreateInfo == nullptr)
    {
        // Return a common key that applies to all pipelines. If it's changed, it invalidates all other
        // pipeline-specific keys.
        const Util::IPlatformKey* pPlatformKey = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPlatformKey();

        // If this fails, then we probably need to compute the key in some other way, or we risk collisions.
        VK_ASSERT(pPlatformKey->GetKeySize() <= VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR);

        WriteToPipelineBinaryKey(
            pPlatformKey->GetKey(),
            pPlatformKey->GetKeySize(),
            pPipelineBinaryKey);
    }
    else
    {
        Util::MetroHash::Hash elfHash                              = {};
        Util::MetroHash::Hash cacheIds[MaxPipelineBinaryInfoCount] = {};

        result = PipelineBinary::CreateCacheId(pDevice, pPipelineCreateInfo, &elfHash, cacheIds);

        if (result == VK_SUCCESS)
        {
            Util::MetroHash128 hasher = {};

            // Make the pipeline key consist of all the cacheIds associated with the pipeline, which makes it unique
            // from the individual cacheIds generated per binary.
            for (uint32_t i = 0; i < MaxPipelineBinaryInfoCount; ++i)
            {
                if ((cacheIds[i].qwords[0] != 0) || (cacheIds[i].qwords[1] != 0))
                {
                    hasher.Update(cacheIds[i]);
                }
            }

            Util::MetroHash::Hash pipelineKey = {};

            hasher.Finalize(pipelineKey.bytes);

            WriteToPipelineBinaryKey(
                pipelineKey.bytes,
                sizeof(pipelineKey.bytes),
                pPipelineBinaryKey);
        }
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineBinary::CreateCacheId(
    const Device*                               pDevice,
    const VkPipelineCreateInfoKHR*              pPipelineCreateInfo,
    Util::MetroHash::Hash*                      pElfHash,
    Util::MetroHash::Hash*                      pCacheIds)
{
    VK_ASSERT(pPipelineCreateInfo != nullptr);
    VkResult result = VK_SUCCESS;

    switch (static_cast<const VkStructHeader*>(pPipelineCreateInfo->pNext)->sType)
    {
    case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
    {
        const auto pComputeCreateInfo = static_cast<const VkComputePipelineCreateInfo*>(pPipelineCreateInfo->pNext);

        const VkPipelineCreateFlags2KHR flags = Device::GetPipelineCreateFlags(pComputeCreateInfo);

        ComputePipelineBinaryCreateInfo binaryCreateInfo               = {};
        PipelineOptimizerKey            pipelineOptimizerKey           = {};
        ShaderOptimizerKey              shaderOptimizerKey             = {};
        ShaderModuleHandle              tempModule                     = {};
        ComputePipelineShaderStageInfo  shaderInfo                     = {};
        uint64                          apiPsoHash                     = 0;

        result = ComputePipeline::CreateCacheId(
            pDevice,
            pComputeCreateInfo,
            flags,
            &shaderInfo,
            &binaryCreateInfo,
            &shaderOptimizerKey,
            &pipelineOptimizerKey,
            &apiPsoHash,
            &tempModule,
            pCacheIds);

        break;
    }
    case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
    {
        const auto pGraphicsCreateInfo =
            static_cast<const VkGraphicsPipelineCreateInfo*>(pPipelineCreateInfo->pNext);

        const VkPipelineCreateFlags2KHR flags = Device::GetPipelineCreateFlags(pGraphicsCreateInfo);

        GraphicsPipelineExtStructs       extStructs                                            = {};
        GraphicsPipelineLibraryInfo      libInfo                                               = {};
        GraphicsPipelineBinaryCreateInfo binaryCreateInfo                                      = {};
        PipelineOptimizerKey             pipelineOptimizerKey                                  = {};
        ShaderOptimizerKey               shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
        ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount]         = {};
        GraphicsPipelineShaderStageInfo  shaderStageInfo                                       = {};
        PipelineMetadata                 binaryMetadata                                        = {};
        uint64                           apiPsoHash                                            = 0;

        GraphicsPipelineCommon::HandleExtensionStructs(pGraphicsCreateInfo, &extStructs);
        GraphicsPipelineCommon::ExtractLibraryInfo(pDevice, pGraphicsCreateInfo, extStructs, flags, &libInfo);

        const PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pGraphicsCreateInfo->layout);

        if (pPipelineLayout == nullptr)
        {
            pPipelineLayout = pDevice->GetNullPipelineLayout();
        }

        if ((flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) != 0)
        {
            // Set this here to be consistent between pipeline_binary and pipeline library creation.
            binaryCreateInfo.pipelineInfo.iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }

        result = GraphicsPipelineCommon::CreateCacheId(
            pDevice,
            pGraphicsCreateInfo,
            extStructs,
            libInfo,
            pPipelineLayout,
            flags,
            &shaderStageInfo,
            &binaryCreateInfo,
            shaderOptimizerKeys,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &apiPsoHash,
            pElfHash,
            tempModules,
            pCacheIds);

        break;
    }
#if VKI_RAY_TRACING
    case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
    {
        const auto pRayTracingCreateInfo =
            static_cast<const VkRayTracingPipelineCreateInfoKHR*>(pPipelineCreateInfo->pNext);

        const VkPipelineCreateFlags2KHR flags = Device::GetPipelineCreateFlags(pRayTracingCreateInfo);

        RayTracingPipelineShaderStageInfo shaderInfo        = {};
        PipelineOptimizerKey              optimizerKey      = {};
        ShaderModuleHandle*               pTempModules      = nullptr;
        uint64                            apiPsoHash        = 0;
        Util::MetroHash::Hash             elfHash           = {};

        // If rtEnableCompilePipelineLibrary is false, the library shaders are included in pRayTracingCreateInfo.
        const bool hasLibraries =
            pDevice->GetRuntimeSettings().rtEnableCompilePipelineLibrary &&
            ((pRayTracingCreateInfo->pLibraryInfo != nullptr)            &&
             (pRayTracingCreateInfo->pLibraryInfo->libraryCount > 0));

        void*          pShaderTempBuffer = nullptr;
        const uint32_t nativeShaderCount = pRayTracingCreateInfo->stageCount;
        uint32_t       totalShaderCount  = pRayTracingCreateInfo->stageCount;

        if (hasLibraries)
        {
            for (uint32_t libraryIdx = 0;
                 libraryIdx < pRayTracingCreateInfo->pLibraryInfo->libraryCount;
                 ++libraryIdx)
            {
                auto pLibrary = RayTracingPipeline::ObjectFromHandle(
                    pRayTracingCreateInfo->pLibraryInfo->pLibraries[libraryIdx]);

                VK_ASSERT(pLibrary->GetType() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

                totalShaderCount += pLibrary->GetTotalShaderCount();
            }
        }

            if (totalShaderCount > 0)
            {
                const size_t storageSize = (sizeof(ShaderStageInfo)    * nativeShaderCount) +
                                           (sizeof(ShaderModuleHandle) * nativeShaderCount) +
                                           (sizeof(ShaderOptimizerKey) * totalShaderCount);

                pShaderTempBuffer = pDevice->VkInstance()->AllocMem(
                    storageSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pShaderTempBuffer != nullptr)
            {
                void* pMemory = pShaderTempBuffer;

                memset(pMemory, 0, storageSize);

                shaderInfo.pStages = static_cast<ShaderStageInfo*>(pMemory);
                pMemory = Util::VoidPtrInc(pMemory, sizeof(ShaderStageInfo) * nativeShaderCount);

                pTempModules = static_cast<ShaderModuleHandle*>(pMemory);
                pMemory = Util::VoidPtrInc(pMemory, sizeof(ShaderModuleHandle) * nativeShaderCount);

                optimizerKey.pShaders = static_cast<ShaderOptimizerKey*>(pMemory);

                shaderInfo.stageCount = nativeShaderCount;
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            if (result == VK_SUCCESS)
            {
                optimizerKey.shaderCount = totalShaderCount;

                result = RayTracingPipeline::CreateCacheId(
                    pDevice,
                    pRayTracingCreateInfo,
                    flags,
                    hasLibraries,
                    &shaderInfo,
                    &optimizerKey,
                    &apiPsoHash,
                    &elfHash,
                    pTempModules,
                    pCacheIds);

                // Free the temporary memory for shader modules
                Pipeline::FreeTempModules(pDevice, nativeShaderCount, pTempModules);

                // Free the temporary memory for creating cacheIds
                if (pShaderTempBuffer != nullptr)
                {
                    pDevice->VkInstance()->FreeMem(pShaderTempBuffer);
                }
            }
        }

        break;
    }
#endif
    default:
        // Unexpected header
        result = VK_ERROR_UNKNOWN;

        VK_NEVER_CALLED();
        break;
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineBinary::GetPipelineBinaryData(
    VkPipelineBinaryKeyKHR*                     pPipelineBinaryKey,
    size_t*                                     pPipelineBinaryDataSize,
    void*                                       pPipelineBinaryData)
{
    VK_ASSERT(pPipelineBinaryDataSize != nullptr);

    VkResult result = VK_SUCCESS;

    if (pPipelineBinaryData != nullptr)
    {
        if (*pPipelineBinaryDataSize < m_binaryData.codeSize)
        {
            result = VK_ERROR_NOT_ENOUGH_SPACE_KHR;
        }
        else
        {
            memcpy(pPipelineBinaryData, m_binaryData.pCode, m_binaryData.codeSize);
        }
    }

    // Must be written in all cases
    *pPipelineBinaryDataSize = m_binaryData.codeSize;
    WriteToPipelineBinaryKey(&m_binaryKey, sizeof(m_binaryKey), pPipelineBinaryKey);

    return result;
}

// =====================================================================================================================
VkResult PipelineBinary::ReleaseCapturedPipelineData(
    Device*                                     pDevice,
    Pipeline*                                   pPipeline,
    const VkAllocationCallbacks*                pAllocator)
{
    return pPipeline->FreeBinaryStorage(pAllocator);
}

// =====================================================================================================================
// A helper to write a pipeline binary key
void PipelineBinary::WriteToPipelineBinaryKey(
    const void*                                 pSrcData,
    const size_t                                dataSize,
    VkPipelineBinaryKeyKHR*                     pDstKey)
{
    VK_ASSERT(pDstKey != nullptr);
    VK_ASSERT(dataSize <= sizeof(pDstKey->key));

    pDstKey->keySize = static_cast<uint32_t>(dataSize);
    memcpy(pDstKey->key, pSrcData, dataSize);
    memset(&pDstKey->key[dataSize], 0, sizeof(pDstKey->key) - dataSize);
}

// =====================================================================================================================
// A helper to convert a pipeline binary key to MetroHash::Hash.
void PipelineBinary::ReadFromPipelineBinaryKey(
    const VkPipelineBinaryKeyKHR&               inKey,
    Util::MetroHash::Hash*                      pOutKey)
{
    VK_ASSERT(pOutKey != nullptr);

    constexpr auto OutKeySize = static_cast<uint32_t>(sizeof(pOutKey->bytes));

    VK_ASSERT(inKey.keySize >= OutKeySize);

    memcpy(pOutKey->bytes, inKey.key, OutKeySize);
}

} // namespace vk
