/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineBinaryKHR*                        pPipelineBinary)
{
    VK_ASSERT(pPipelineBinary != nullptr);

    VkResult result = VK_SUCCESS;

    PipelineBinary* pObject = nullptr;
    uint8_t*        pCode   = nullptr;

    auto placement = utils::PlacementHelper<2>(
        nullptr,
        utils::PlacementElement<PipelineBinary>{&pObject},
        utils::PlacementElement<uint8_t>       {&pCode, binaryData.codeSize});

    void* pMemory = pDevice->AllocApiObject(pAllocator, placement.SizeOf());

    if (pMemory == nullptr)
    {
        result           = VK_ERROR_OUT_OF_HOST_MEMORY;
        *pPipelineBinary = VK_NULL_HANDLE;
    }
    else
    {
        placement.FixupPtrs(pMemory);
        VK_ASSERT(pObject == pMemory);

        memcpy(pCode, binaryData.pCode, binaryData.codeSize);

        Vkgc::BinaryData objectBinaryData
        {
            .codeSize = binaryData.codeSize,
            .pCode    = pCode
        };

        VK_PLACEMENT_NEW(pObject) PipelineBinary(binaryKey, objectBinaryData);

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

                const auto binaryData = Vkgc::BinaryData
                {
                    .codeSize = pCreateInfo->pKeysAndDataInfo->pPipelineBinaryData[binaryIndex].dataSize,
                    .pCode    = pCreateInfo->pKeysAndDataInfo->pPipelineBinaryData[binaryIndex].pData
                };

                VkResult result = PipelineBinary::Create(
                    pDevice,
                    binaryKey,
                    binaryData,
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
        const auto pBinaryStorage = Pipeline::BaseObjectFromHandle(pCreateInfo->pipeline)->GetBinaryStorage();

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
        // Generate the key for the provided pipeline create info
        VkPipelineBinaryKeyKHR binaryKey = {};
        PipelineBinary::GetPipelineKey(pDevice, pCreateInfo->pPipelineCreateInfo, &binaryKey);

        // Query the pipeline binary cache using the generated key.
        bool                  isUserCacheHit     = false;
        bool                  isInternalCacheHit = false;
        Util::MetroHash::Hash key                = {};
        Vkgc::BinaryData      pipelineBinary     = {};
        FreeCompilerBinary    freeCompilerBinary = FreeWithCompiler;

        ReadFromPipelineBinaryKey(binaryKey, &key);

        Util::Result cacheResult = pDevice->GetCompiler(DefaultDeviceIndex)->GetCachedPipelineBinary(
            &key,
            nullptr,                // pPipelineBinaryCache
            &pipelineBinary,
            &isUserCacheHit,
            &isInternalCacheHit,
            &freeCompilerBinary,
            nullptr);               // pPipelineFeedback

        if (cacheResult == Util::Result::Success)
        {
            if (pBinaries->pPipelineBinaries == nullptr)
            {
                // Cached binaries are monolithic, not GPL libraries
                pBinaries->pipelineBinaryCount = pDevice->NumPalDevices();
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
                        key,
                        pipelineBinary,
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
            finalResult = VK_PIPELINE_BINARY_MISSING_KHR;
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
        const auto pPlatformKey = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPlatformKey();

        // If this fails, then we probably need to compute the key in some other way, or we risk collisions.
        VK_ASSERT(pPlatformKey->GetKeySize() <= VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR);

        WriteToPipelineBinaryKey(
            pPlatformKey->GetKey(),
            pPlatformKey->GetKeySize(),
            pPipelineBinaryKey);
    }
    else
    {
        Util::MetroHash::Hash cacheId[MaxPipelineBinaryInfoCount] = {};

        switch (static_cast<const VkStructHeader*>(pPipelineCreateInfo->pNext)->sType)
        {
        case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
        {
            const auto pComputeCreateInfo = static_cast<const VkComputePipelineCreateInfo*>(pPipelineCreateInfo->pNext);
            const auto flags              = Device::GetPipelineCreateFlags(pComputeCreateInfo);

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
                cacheId);

            break;
        }
        case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
        {
            const auto pGraphicsCreateInfo =
                static_cast<const VkGraphicsPipelineCreateInfo*>(pPipelineCreateInfo->pNext);

            const auto flags               = Device::GetPipelineCreateFlags(pGraphicsCreateInfo);

            GraphicsPipelineExtStructs       extStructs                                            = {};
            GraphicsPipelineLibraryInfo      libInfo                                               = {};
            GraphicsPipelineBinaryCreateInfo binaryCreateInfo                                      = {};
            PipelineOptimizerKey             pipelineOptimizerKey                                  = {};
            ShaderOptimizerKey               shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
            ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount]         = {};
            GraphicsPipelineShaderStageInfo  shaderStageInfo                                       = {};
            uint64                           apiPsoHash                                            = 0;

            GraphicsPipelineCommon::HandleExtensionStructs(pGraphicsCreateInfo, &extStructs);
            GraphicsPipelineCommon::ExtractLibraryInfo(pDevice, pGraphicsCreateInfo, extStructs, flags, &libInfo);

            result = GraphicsPipelineCommon::CreateCacheId(
                pDevice,
                pGraphicsCreateInfo,
                extStructs,
                libInfo,
                flags,
                &shaderStageInfo,
                &binaryCreateInfo,
                shaderOptimizerKeys,
                &pipelineOptimizerKey,
                &apiPsoHash,
                tempModules,
                cacheId);

            break;
        }
#if VKI_RAY_TRACING
        case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
        {
            const auto pRayTracingCreateInfo =
                static_cast<const VkRayTracingPipelineCreateInfoKHR*>(pPipelineCreateInfo->pNext);

            const auto flags = Device::GetPipelineCreateFlags(pRayTracingCreateInfo);

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
                auto placement = utils::PlacementHelper<3>(
                    nullptr,
                    utils::PlacementElement<ShaderStageInfo>   {&shaderInfo.pStages,    nativeShaderCount},
                    utils::PlacementElement<ShaderModuleHandle>{&pTempModules,          nativeShaderCount},
                    utils::PlacementElement<ShaderOptimizerKey>{&optimizerKey.pShaders, totalShaderCount});

                pShaderTempBuffer = pDevice->VkInstance()->AllocMem(
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
                        cacheId);

                    // Free the temporary memory for shader modules
                    Pipeline::FreeTempModules(pDevice, nativeShaderCount, pTempModules);

                    // Free the temporary memory for creating cacheId
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

        if (result == VK_SUCCESS)
        {
            WriteToPipelineBinaryKey(
                cacheId[0].bytes,
                sizeof(cacheId[0].bytes),
                pPipelineBinaryKey);
        }
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
            WriteToPipelineBinaryKey(&m_binaryKey, sizeof(m_binaryKey), pPipelineBinaryKey);

            memcpy(pPipelineBinaryData, m_binaryData.pCode, m_binaryData.codeSize);
        }
    }

    // Must be written in all cases
    *pPipelineBinaryDataSize = m_binaryData.codeSize;

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
