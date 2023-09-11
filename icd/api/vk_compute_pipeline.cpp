/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_cmdbuffer.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_conv.h"
#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_memory.h"
#include "include/vk_pipeline.h"

#include "palPipeline.h"
#include "palPipelineAbi.h"
#include "palPipelineAbiReader.h"
#include "palMetroHash.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
// Generates the API PSO hash using the contents of the VkComputePipelineCreateInfo struct
// Pipeline compilation affected by:
//     - pCreateInfo->pStage
//     - pCreateInfo->layout
void ComputePipeline::BuildApiHash(
    const VkComputePipelineCreateInfo*    pCreateInfo,
    PipelineCreateFlags                   flags,
    const ComputePipelineShaderStageInfo& stageInfo,
    Util::MetroHash::Hash*                pElfHash,
    uint64_t*                             pApiHash)
{
    // Set up the ELF hash, which is used for indexing the pipeline cache
    Util::MetroHash128 elfHasher = {};

    // Hash only flags needed for pipeline caching
    elfHasher.Update(GetCacheIdControlFlags(flags));

    GenerateHashFromShaderStageCreateInfo(stageInfo.stage, &elfHasher);

    if (pCreateInfo->layout != VK_NULL_HANDLE)
    {
        elfHasher.Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());
    }
    elfHasher.Finalize(reinterpret_cast<uint8_t*>(pElfHash));

    // Set up the API hash, which gets passed down to RGP traces as 64 bits
    Util::MetroHash::Hash apiHash128 = {};
    Util::MetroHash128    apiHasher  = {};

    apiHasher.Update(*pElfHash);

    // Hash flags not accounted for in the elf hash
    apiHasher.Update(flags);

    if (((pCreateInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) != 0) &&
        (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(ComputePipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);
    apiHasher.Finalize(reinterpret_cast<uint8_t*>(&apiHash128));

    *pApiHash = Util::MetroHash::Compact64(&apiHash128);
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
void ComputePipeline::ConvertComputePipelineInfo(
    Device*                               pDevice,
    const VkComputePipelineCreateInfo*    pIn,
    const ComputePipelineShaderStageInfo& stageInfo,
    CreateInfo*                           pOutInfo)
{
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pOutInfo->pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }
}

// =====================================================================================================================
void ComputePipeline::FetchPalMetadata(
    PalAllocator* pAllocator,
    const void*   pBinary,
    uint32_t*     pOrigThreadgroupDims)
{
    Util::Abi::PipelineAbiReader abiReader(pAllocator, pBinary);

    Util::Result result = abiReader.Init();
    if (result == Util::Result::Success)
    {
        Util::MsgPackReader              metadataReader;
        Util::PalAbi::CodeObjectMetadata metadata;
        result = abiReader.GetMetadata(&metadataReader, &metadata);

        if (result == Util::Result::Success)
        {
            const auto& csStage = metadata.pipeline.hardwareStage[static_cast<uint32_t>(Util::Abi::HardwareStage::Cs)];

            const uint32_t* pThreadgroupDims = (csStage.origThreadgroupDimensions[0] != 0) ?
                csStage.origThreadgroupDimensions : csStage.threadgroupDimensions;

            pOrigThreadgroupDims[0] = pThreadgroupDims[0];
            pOrigThreadgroupDims[1] = pThreadgroupDims[1];
            pOrigThreadgroupDims[2] = pThreadgroupDims[2];
        }
    }

    VK_ASSERT(result == Util::Result::Success);
}

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* const                        pDevice,
    Pal::IPipeline**                     pPalPipeline,
    const PipelineLayout*                pPipelineLayout,
    const ImmedInfo&                     immedInfo,
#if VKI_RAY_TRACING
    bool                                 hasRayTracing,
    uint32_t                             dispatchRaysUserDataOffset,
#endif
    const uint32_t*                      pOrigThreadgroupDims,
    uint64_t                             staticStateMask,
    const Util::MetroHash::Hash&         cacheHash,
    uint64_t                             apiHash)
    :
    Pipeline(
        pDevice,
#if VKI_RAY_TRACING
        hasRayTracing,
#endif
        VK_PIPELINE_BIND_POINT_COMPUTE),
    m_info(immedInfo)
{
    Pipeline::Init(
        pPalPipeline,
        pPipelineLayout,
        staticStateMask,
#if VKI_RAY_TRACING
        dispatchRaysUserDataOffset,
#endif
        cacheHash,
        apiHash);

    m_origThreadgroupDims[0] = pOrigThreadgroupDims[0];
    m_origThreadgroupDims[1] = pOrigThreadgroupDims[1];
    m_origThreadgroupDims[2] = pOrigThreadgroupDims[2];
}

// =====================================================================================================================
VkResult ComputePipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
// Create a compute pipeline object.
VkResult ComputePipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkComputePipelineCreateInfo*      pCreateInfo,
    PipelineCreateFlags                     flags,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    // Setup PAL create info from Vulkan inputs
    size_t                          pipelineBinarySizes[MaxPalDevices] = {};
    const void*                     pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash           cacheId[MaxPalDevices]             = {};
    PipelineCompiler*               pDefaultCompiler                   = pDevice->GetCompiler(DefaultDeviceIndex);
    const RuntimeSettings&          settings                           = pDevice->GetRuntimeSettings();
    ComputePipelineBinaryCreateInfo binaryCreateInfo                   = {};
    PipelineOptimizerKey            pipelineOptimizerKey               = {};
    ShaderOptimizerKey              shaderOptimizerKey                 = {};
    ShaderModuleHandle              tempModule                         = {};
    VkResult                        result                             = VK_SUCCESS;
    PipelineMetadata                binaryMetadata                     = {};

    ComputePipelineShaderStageInfo shaderInfo = {};
    result = BuildShaderStageInfo(pDevice,
        1,
        &pCreateInfo->stage,
        false,
        [](const uint32_t inputIdx, const uint32_t stageIdx)
        {
            return 0u;
        },
        &shaderInfo.stage,
        &tempModule,
        pPipelineCache,
        &binaryCreateInfo.stageFeedback);

    Util::MetroHash::Hash elfHash    = {};
    uint64_t              apiPsoHash = {};
    BuildApiHash(pCreateInfo, flags, shaderInfo, &elfHash, &apiPsoHash);

    binaryCreateInfo.apiPsoHash = apiPsoHash;

    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo = nullptr;
    pDefaultCompiler->GetPipelineCreationFeedback(static_cast<const VkStructHeader*>(pCreateInfo->pNext),
        &pPipelineCreationFeedbackCreateInfo);

    const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
        ShaderModule::GetFirstValidShaderData(shaderInfo.stage.pModuleHandle));

    // Set up the PipelineProfileKey for applying tuning parameters
    pipelineOptimizerKey.shaderCount = 1;
    pipelineOptimizerKey.pShaders    = &shaderOptimizerKey;

    pDevice->GetShaderOptimizer()->CreateShaderOptimizerKey(pModuleData,
                                                            shaderInfo.stage.codeHash,
                                                            Vkgc::ShaderStage::ShaderStageCompute,
                                                            shaderInfo.stage.codeSize,
                                                            &shaderOptimizerKey);

    // Load or create the pipeline binary
    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

    for (uint32_t deviceIdx = 0; (deviceIdx < pDevice->NumPalDevices()) && (result == VK_SUCCESS); ++deviceIdx)
    {
        bool isUserCacheHit     = false;
        bool isInternalCacheHit = false;

        {
            Util::Result cacheResult = Util::Result::NotFound;

            ElfHashToCacheId(
                pDevice,
                deviceIdx,
                elfHash,
                pDevice->VkPhysicalDevice(deviceIdx)->GetSettingsLoader()->GetSettingsHash(),
                pipelineOptimizerKey,
                &cacheId[deviceIdx]
            );

            bool forceCompilation = settings.enablePipelineDump;

            if (forceCompilation == false)
            {
                // Search the pipeline binary cache
                cacheResult = pDevice->GetCompiler(deviceIdx)->GetCachedPipelineBinary(
                    &cacheId[deviceIdx],
                    pPipelineBinaryCache,
                    &pipelineBinarySizes[deviceIdx],
                    &pPipelineBinaries[deviceIdx],
                    &isUserCacheHit,
                    &isInternalCacheHit,
                    &binaryCreateInfo.freeCompilerBinary,
                    &binaryCreateInfo.pipelineFeedback);
            }

            // Compile if unable to retrieve from cache
            if (cacheResult != Util::Result::Success)
            {
                if (binaryCreateInfo.pTempBuffer == nullptr)
                {
                    result = pDefaultCompiler->ConvertComputePipelineInfo(
                        pDevice,
                        pCreateInfo,
                        &shaderInfo,
                        &pipelineOptimizerKey,
                        &binaryMetadata,
                        &binaryCreateInfo,
                        flags);
                }

                if (result == VK_SUCCESS)
                {
                    result = pDevice->GetCompiler(deviceIdx)->CreateComputePipelineBinary(
                        pDevice,
                        deviceIdx,
                        pPipelineCache,
                        &binaryCreateInfo,
                        &pipelineBinarySizes[deviceIdx],
                        &pPipelineBinaries[deviceIdx],
                        &cacheId[deviceIdx]);
                }

                if (result == VK_SUCCESS)
                {
                    result = pDefaultCompiler->WriteBinaryMetadata(
                        pDevice,
                        binaryCreateInfo.compilerType,
                        &binaryCreateInfo.freeCompilerBinary,
                        &pPipelineBinaries[deviceIdx],
                        &pipelineBinarySizes[deviceIdx],
                        binaryCreateInfo.pBinaryMetadata);
                }
            }
            else if (deviceIdx == DefaultDeviceIndex)
            {
                pDefaultCompiler->ReadBinaryMetadata(
                    pDevice,
                    pPipelineBinaries[DefaultDeviceIndex],
                    pipelineBinarySizes[DefaultDeviceIndex],
                    &binaryMetadata);
            }
        }

        // Add to any cache layer where missing
        if (result == VK_SUCCESS)
        {
            pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                &cacheId[deviceIdx],
                pPipelineBinaryCache,
                pipelineBinarySizes[deviceIdx],
                pPipelineBinaries[deviceIdx],
                isUserCacheHit,
                isInternalCacheHit);
        }
    }

    CreateInfo localPipelineInfo = {};

    if (result == VK_SUCCESS)
    {
        ConvertComputePipelineInfo(pDevice, pCreateInfo, shaderInfo, &localPipelineInfo);

        // Override pipeline creation parameters based on pipeline profile
        pDevice->GetShaderOptimizer()->OverrideComputePipelineCreateInfo(
            pipelineOptimizerKey,
            &localPipelineInfo.immedInfo.computeShaderInfo);
    }

    // Get the pipeline and shader size from PAL and allocate memory.
    size_t pipelineSize = 0;
    void* pSystemMem = nullptr;

    Pal::Result palResult = Pal::Result::Success;

    if (result == VK_SUCCESS)
    {
        localPipelineInfo.pipeline.flags.clientInternal = false;
        localPipelineInfo.pipeline.pipelineBinarySize   = pipelineBinarySizes[DefaultDeviceIndex];
        localPipelineInfo.pipeline.pPipelineBinary      = pPipelineBinaries[DefaultDeviceIndex];

        pipelineSize =
            pDevice->PalDevice(DefaultDeviceIndex)->GetComputePipelineSize(localPipelineInfo.pipeline, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        pSystemMem = pDevice->AllocApiObject(
            pAllocator,
            sizeof(ComputePipeline) + (pipelineSize * pDevice->NumPalDevices()));

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Create the PAL pipeline object.
    Pal::IPipeline* pPalPipeline[MaxPalDevices] = {};

    if (result == VK_SUCCESS)
    {
        void* pPalMem = Util::VoidPtrInc(pSystemMem, sizeof(ComputePipeline));

        for (uint32_t deviceIdx = 0;
            ((deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
            deviceIdx++)
        {
            VK_ASSERT(pipelineSize ==
                pDevice->PalDevice(deviceIdx)->GetComputePipelineSize(localPipelineInfo.pipeline, nullptr));

            // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
            // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
            if (pPipelineBinaries[deviceIdx] != nullptr)
            {
                localPipelineInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[deviceIdx];
                localPipelineInfo.pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
            }

            palResult = pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                localPipelineInfo.pipeline,
                Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                &pPalPipeline[deviceIdx]);

#if ICD_GPUOPEN_DEVMODE_BUILD
            // Temporarily reinject post Pal pipeline creation (when the internal pipeline hash is available).
            // The reinjection cache layer can be linked back into the pipeline cache chain once the
            // Vulkan pipeline cache key can be stored (and read back) inside the ELF as metadata.
            if ((pDevice->VkInstance()->GetDevModeMgr() != nullptr) &&
                (palResult == Util::Result::Success))
            {
                const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                palResult = pDevice->GetCompiler(deviceIdx)->RegisterAndLoadReinjectionBinary(
                    &info.internalPipelineHash,
                    &cacheId[deviceIdx],
                    &localPipelineInfo.pipeline.pipelineBinarySize,
                    &localPipelineInfo.pipeline.pPipelineBinary,
                    pPipelineCache);

                if (palResult == Util::Result::Success)
                {
                    pPalPipeline[deviceIdx]->Destroy();

                    palResult = pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
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
#if VKI_RAY_TRACING
        // If pModuleData is null this means the pipeline is being created with a null shader and
        // using the shader module identifier.
        bool hasRayTracing = (pModuleData != nullptr) ?
            pModuleData->usage.enableRayQuery :
            binaryMetadata.rayQueryUsed;

        uint32_t dispatchRaysUserDataOffset = localPipelineInfo.pLayout->GetDispatchRaysUserData();
#endif

        uint32_t origThreadgroupDims[3];
        FetchPalMetadata(pDevice->VkInstance()->Allocator(),
                         pPipelineBinaries[DefaultDeviceIndex],
                         origThreadgroupDims);

        // On success, wrap it up in a Vulkan object and return.
        VK_PLACEMENT_NEW(pSystemMem) ComputePipeline(pDevice,
            pPalPipeline,
            localPipelineInfo.pLayout,
            localPipelineInfo.immedInfo,
#if VKI_RAY_TRACING
            hasRayTracing,
            dispatchRaysUserDataOffset,
#endif
            origThreadgroupDims,
            localPipelineInfo.staticStateMask,
            cacheId[DefaultDeviceIndex],
            apiPsoHash);

        *pPipeline = ComputePipeline::HandleFromVoidPointer(pSystemMem);
        if (settings.enableDebugPrintf)
        {
            ComputePipeline* pComputePipeline = static_cast<ComputePipeline*>(pSystemMem);
            pComputePipeline->ClearFormatString();
            DebugPrintf::DecodeFormatStringsFromElf(pDevice,
                                                    pipelineBinarySizes[DefaultDeviceIndex],
                                                    static_cast<const char*>(pPipelineBinaries[DefaultDeviceIndex]),
                                                    pComputePipeline->GetFormatStrings());
        }
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            // Internal memory allocation failed, free PAL event object if it gets created
            if (pPalPipeline[deviceIdx] != nullptr)
            {
                pPalPipeline[deviceIdx]->Destroy();
            }
        }
    }

    // Free the temporary newly-built shader modules
    FreeTempModules(pDevice, 1, &tempModule);

    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            pDevice->GetCompiler(deviceIdx)->FreeComputePipelineBinary(
                &binaryCreateInfo, pPipelineBinaries[deviceIdx], pipelineBinarySizes[deviceIdx]);
        }
    }

    pDefaultCompiler->FreeComputePipelineCreateInfo(&binaryCreateInfo);

    // Something went wrong with creating the PAL object. Free memory and return error.
    if (result != VK_SUCCESS)
    {
        // Free system memory for pipeline object
        pDevice->FreeApiObject(pAllocator, pSystemMem);
    }

    if (result == VK_SUCCESS)
    {
        const Device::DeviceFeatures& deviceFeatures = pDevice->GetEnabledFeatures();

        uint64_t durationTicks = Util::GetPerfCpuTime() - startTimeTicks;
        uint64_t duration      = vk::utils::TicksToNano(durationTicks);

        binaryCreateInfo.pipelineFeedback.feedbackValid = true;
        binaryCreateInfo.pipelineFeedback.duration      = duration;

        pDefaultCompiler->SetPipelineCreationFeedbackInfo(
            pPipelineCreationFeedbackCreateInfo,
            0,
            nullptr,
            &binaryCreateInfo.pipelineFeedback,
            &binaryCreateInfo.stageFeedback);

        if (deviceFeatures.gpuMemoryEventHandler)
        {
            size_t numEntries = 0;
            Util::Vector<Pal::GpuMemSubAllocInfo, 1, PalAllocator> palSubAllocInfos(pDevice->VkInstance()->Allocator());
            const auto* pPipelineObject = ComputePipeline::ObjectFromHandle(*pPipeline);

            pPipelineObject->PalPipeline(DefaultDeviceIndex)->QueryAllocationInfo(&numEntries, nullptr);

            palSubAllocInfos.Resize(numEntries);

            pPipelineObject->PalPipeline(DefaultDeviceIndex)->QueryAllocationInfo(&numEntries, &palSubAllocInfos[0]);

            for (size_t i = 0; i < numEntries; ++i)
            {
                // Report the Pal suballocation for this pipeline to device_memory_report
                pDevice->VkInstance()->GetGpuMemoryEventHandler()->ReportDeferredPalSubAlloc(
                    pDevice,
                    palSubAllocInfos[i].address,
                    palSubAllocInfos[i].offset,
                    ComputePipeline::IntValueFromHandle(*pPipeline),
                    VK_OBJECT_TYPE_PIPELINE);
            }
       }

        // The hash is same as pipline dump file name, we can easily analyze further.
        AmdvlkLog(settings.logTagIdMask, PipelineCompileTime, "0x%016llX-%llu", apiPsoHash, duration);
    }

    return result;
}

// =====================================================================================================================
void ComputePipeline::BindToCmdBuffer(
    CmdBuffer*                           pCmdBuffer,
    const Pal::DynamicComputeShaderInfo& computeShaderInfo) const
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};

    params.pipelineBindPoint = Pal::PipelineBindPoint::Compute;
    params.cs                = computeShaderInfo;
    params.apiPsoHash        = m_apiHash;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        params.pPipeline = m_pPalPipeline[deviceIdx];

        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
        uint32_t debugPrintfRegBase = (m_userDataLayout.scheme == PipelineLayoutScheme::Compact) ?
            m_userDataLayout.compact.debugPrintfRegBase : m_userDataLayout.indirect.debugPrintfRegBase;
        pCmdBuffer->GetDebugPrintf()->BindPipeline(m_pDevice,
                                                   this,
                                                   deviceIdx,
                                                   pCmdBuffer->PalCmdBuffer(deviceIdx),
                                                   static_cast<uint32_t>(Pal::PipelineBindPoint::Compute),
                                                   debugPrintfRegBase);
    }
}

// =====================================================================================================================
void ComputePipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint       = Pal::PipelineBindPoint::Compute;
    params.apiPsoHash = Pal::InternalApiPsoHash;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

} // namespace vk
