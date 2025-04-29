/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_compute_pipeline.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_utils.h"
#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#include "raytrace/vk_ray_tracing_pipeline.h"
#endif
#include "include/vk_sampler.h"

#include "palAutoBuffer.h"
#include "palInlineFuncs.h"
#include "palPipeline.h"
#include "palPipelineAbi.h"
#include "palPipelineAbiReader.h"
#include "palMetroHash.h"

#include <algorithm>

namespace vk
{

// The names of hardware shader stages used in PAL metadata, in Util::Abi::HardwareStage order.
static const char* HwStageNames[] =
{
    ".ls",
    ".hs",
    ".es",
    ".gs",
    ".vs",
    ".ps",
    ".cs"
};

// The names of api shader stages
static const char* ApiStageNames[] =
{
    ".cs",
    ".ts",
    ".vs",
    ".hs",
    ".ds",
    ".gs",
    ".ms",
    ".ps"
};

static constexpr struct
{
    Pal::ShaderStageFlagBits palFlagBits;
    Util::Abi::ApiShaderType abiType;
} IndexShaderStages[] =
{
    { Pal::ApiShaderStageCompute,                 Util::Abi::ApiShaderType::Cs },
    { Pal::ApiShaderStageTask,                    Util::Abi::ApiShaderType::Task },
    { Pal::ApiShaderStageVertex,                  Util::Abi::ApiShaderType::Vs },
    { Pal::ApiShaderStageHull,                    Util::Abi::ApiShaderType::Hs },
    { Pal::ApiShaderStageDomain,                  Util::Abi::ApiShaderType::Ds },
    { Pal::ApiShaderStageGeometry,                Util::Abi::ApiShaderType::Gs },
    { Pal::ApiShaderStageMesh,                    Util::Abi::ApiShaderType::Mesh },
    { Pal::ApiShaderStagePixel,                   Util::Abi::ApiShaderType::Ps },
};

static constexpr char shaderPreNameIntermediate[] = "Intermediate";
static constexpr char shaderPreNameISA[]          = "ISA";
static constexpr char shaderPreNameELF[]          = "ELF";

static_assert(VK_ARRAY_SIZE(HwStageNames) == static_cast<uint32_t>(Util::Abi::HardwareStage::Count),
    "Number of HwStageNames and PAL HW stages should match.");

// The number of executable statistics to return through
// the vkGetPipelineExecutableStatisticsKHR function
static constexpr uint32_t ExecutableStatisticsCount = 5;

// =====================================================================================================================
// Add binary data to this storage.
// To avoid redundant copies and memory allocation, it's expected that the calling code will allocate and prepare
// the binary. A Vulkan allocator must be used to allocate the memory at pData pointer.
// PipelineBinaryStorage will take ownership of the pointer and later free it in Free() call.
void Pipeline::InsertBinaryData(
    PipelineBinaryStorage*          pBinaryStorage,
    const uint32                    binaryIndex,
    const Util::MetroHash::Hash&    key,
    const size_t                    dataSize,
    const void*                     pData)
{
    VK_ASSERT(pBinaryStorage != nullptr);
    VK_ASSERT(binaryIndex < VK_ARRAY_SIZE(pBinaryStorage->binaryInfo));
    // Expect that each entry is added only once
    VK_ASSERT((pBinaryStorage->binaryInfo[binaryIndex].binaryHash.qwords[0] == 0) &&
              (pBinaryStorage->binaryInfo[binaryIndex].binaryHash.qwords[1] == 0));

    pBinaryStorage->binaryInfo[binaryIndex].binaryHash              = key;
    pBinaryStorage->binaryInfo[binaryIndex].pipelineBinary.codeSize = dataSize;
    pBinaryStorage->binaryInfo[binaryIndex].pipelineBinary.pCode    = pData;

    ++pBinaryStorage->binaryCount;
}

// =====================================================================================================================
// Frees the previously inserted pipeline binaries.
VkResult Pipeline::FreeBinaryStorage(
    const VkAllocationCallbacks*    pAllocator)
{
    VkResult result = VK_SUCCESS;

    if (m_pBinaryStorage != nullptr)
    {
        Pipeline::FreeBinaryStorage(m_pBinaryStorage, pAllocator);
        m_pBinaryStorage = nullptr;
    }
    else
    {
        result = VK_ERROR_UNKNOWN;
    }

    return result;
}

// =====================================================================================================================
// Frees the pipeline binaries.
void Pipeline::FreeBinaryStorage(
    PipelineBinaryStorage*          pBinaryStorage,
    const VkAllocationCallbacks*    pAllocator)
{
    VK_ASSERT(pBinaryStorage != nullptr);

    for (uint32_t binaryIndex = 0; binaryIndex < VK_ARRAY_SIZE(pBinaryStorage->binaryInfo); ++binaryIndex)
    {
        if (pBinaryStorage->binaryInfo[binaryIndex].pipelineBinary.pCode != nullptr)
        {
            auto pMemory = const_cast<void*>(pBinaryStorage->binaryInfo[binaryIndex].pipelineBinary.pCode);
            pAllocator->pfnFree(pAllocator->pUserData, pMemory);
        }
    }
}

// =====================================================================================================================
// Filter VkPipelineCreateFlags2KHR to only values used for pipeline caching
VkPipelineCreateFlags2KHR Pipeline::GetCacheIdControlFlags(
    VkPipelineCreateFlags2KHR in)
{
    // The following flags should NOT affect cache computation
    static constexpr VkPipelineCreateFlags2KHR CacheIdIgnoreFlags = { 0
        | VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR
        | VK_PIPELINE_CREATE_DERIVATIVE_BIT
        | VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT
        | VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT
        | VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT
    };

    return in & (~CacheIdIgnoreFlags);
}

// =====================================================================================================================
// Generates a hash using the contents of a VkSpecializationInfo struct
void Pipeline::GenerateHashFromSpecializationInfo(
    const VkSpecializationInfo& desc,
    Util::MetroHash128*         pHasher)
{
    pHasher->Update(desc.mapEntryCount);

    for (uint32_t i = 0; i < desc.mapEntryCount; i++)
    {
        pHasher->Update(desc.pMapEntries[i]);
    }

    pHasher->Update(desc.dataSize);

    if (desc.pData != nullptr)
    {
        pHasher->Update(reinterpret_cast<const uint8_t*>(desc.pData), desc.dataSize);
    }
}

// =====================================================================================================================
// Generates resource information of a pipeline
void Pipeline::BuildPipelineResourceLayout(
    const Device*                     pDevice,
    const PipelineLayout*             pPipelineLayout,
    VkPipelineBindPoint               pipelineBindPoint,
    VkPipelineCreateFlags2KHR         flags,
    PipelineResourceLayout*           pResourceLayout)
{
    if (pPipelineLayout != nullptr)
    {
        // Setup pipeline resource layout
        pResourceLayout->pPipelineLayout  = pPipelineLayout;

        pResourceLayout->userDataLayout   = pPipelineLayout->GetInfo().userDataLayout;
        pResourceLayout->userDataRegCount = pPipelineLayout->GetInfo().userDataRegCount;

        MappingBufferLayout* pBufferLayout = &pResourceLayout->mappingBufferLayout;

        pBufferLayout->mappingBufferSize = pPipelineLayout->GetPipelineInfo()->mappingBufferSize;
        pBufferLayout->numRsrcMapNodes   = pPipelineLayout->GetPipelineInfo()->numRsrcMapNodes;
        pBufferLayout->numUserDataNodes  = pPipelineLayout->GetPipelineInfo()->numUserDataNodes;

#if VKI_RAY_TRACING
        // Denotes if GpuRT resource mappings will need to be added to this pipeline layout
        pResourceLayout->hasRayTracing   = pPipelineLayout->GetPipelineInfo()->hasRayTracing;
#endif
    }
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Calculates the offset for the GpuRT user data constants
uint32_t Pipeline::GetDispatchRaysUserData(
    const PipelineResourceLayout* pResourceLayout)
{
    uint32_t              dispatchRaysUserData = 0;
    const UserDataLayout& userDataLayout       = pResourceLayout->userDataLayout;

    if (pResourceLayout->hasRayTracing)
    {
        dispatchRaysUserData = userDataLayout.common.dispatchRaysArgsPtrRegBase;
    }

    return dispatchRaysUserData;
}
#endif

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineShaderStageCreateInfo struct
void Pipeline::GenerateHashFromShaderStageCreateInfo(
    const VkPipelineShaderStageCreateInfo& desc,
    Util::MetroHash128*                    pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.stage);

    if (desc.pSpecializationInfo != nullptr)
    {
        GenerateHashFromSpecializationInfo(*desc.pSpecializationInfo, pHasher);
    }

    Pal::ShaderHash shaderModuleIdCodeHash;
    shaderModuleIdCodeHash.lower = 0;
    shaderModuleIdCodeHash.upper = 0;

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32_t>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO:
            {
                const auto* pRequiredSubgroupSizeInfo =
                    static_cast<const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo*>(pNext);
                pHasher->Update(pRequiredSubgroupSizeInfo->sType);
                pHasher->Update(pRequiredSubgroupSizeInfo->requiredSubgroupSize);

                break;
            }
            case VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO:
            {
                const auto* pShaderModuleCreateInfo = static_cast<const VkShaderModuleCreateInfo*>(pNext);
                {
                    Vkgc::BinaryData shaderBinary = {};
                    shaderBinary.codeSize = pShaderModuleCreateInfo->codeSize;
                    shaderBinary.pCode = pShaderModuleCreateInfo->pCode;

                    shaderModuleIdCodeHash = ShaderModule::BuildCodeHash(
                        shaderBinary.pCode,
                        shaderBinary.codeSize);
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT:
            {
                const auto* pPipelineShaderStageModuleIdentifierCreateInfoEXT =
                    static_cast<const VkPipelineShaderStageModuleIdentifierCreateInfoEXT*>(pNext);

                if (pPipelineShaderStageModuleIdentifierCreateInfoEXT->identifierSize > 0)
                {
                    VK_ASSERT(pPipelineShaderStageModuleIdentifierCreateInfoEXT->identifierSize ==
                              sizeof(shaderModuleIdCodeHash));

                    memcpy(&shaderModuleIdCodeHash.lower,
                           (pPipelineShaderStageModuleIdentifierCreateInfoEXT->pIdentifier),
                           sizeof(shaderModuleIdCodeHash.lower));

                    memcpy(&shaderModuleIdCodeHash.upper,
                           (pPipelineShaderStageModuleIdentifierCreateInfoEXT->pIdentifier +
                               sizeof(shaderModuleIdCodeHash.lower)),
                           sizeof(shaderModuleIdCodeHash.upper));
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT:
            {
                const auto* pPipelineRobustnessCreateInfoEXT =
                    static_cast<const VkPipelineRobustnessCreateInfoEXT*>(pNext);
                pHasher->Update(pPipelineRobustnessCreateInfoEXT->sType);
                pHasher->Update(pPipelineRobustnessCreateInfoEXT->storageBuffers);
                pHasher->Update(pPipelineRobustnessCreateInfoEXT->uniformBuffers);
                pHasher->Update(pPipelineRobustnessCreateInfoEXT->vertexInputs);
                pHasher->Update(pPipelineRobustnessCreateInfoEXT->images);

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }

    if (desc.module != VK_NULL_HANDLE)
    {
        pHasher->Update(ShaderModule::ObjectFromHandle(desc.module)->GetCodeHash(desc.pName));
    }
    else
    {
        // Code has from shader module id
        pHasher->Update(ShaderModule::GetCodeHash(shaderModuleIdCodeHash, desc.pName));
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a ShaderStageInfo struct
void Pipeline::GenerateHashFromShaderStageCreateInfo(
    const ShaderStageInfo& stageInfo,
    Util::MetroHash128*    pHasher)
{
    pHasher->Update(stageInfo.flags);
    pHasher->Update(stageInfo.stage);
    pHasher->Update(stageInfo.codeHash);
    pHasher->Update(stageInfo.waveSize);

    if (stageInfo.pSpecializationInfo != nullptr)
    {
        GenerateHashFromSpecializationInfo(*stageInfo.pSpecializationInfo, pHasher);
    }

}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineDynamicStateCreateInfo struct
// Pipeline compilation affected by: none
void Pipeline::GenerateHashFromDynamicStateCreateInfo(
    const VkPipelineDynamicStateCreateInfo& desc,
    Util::MetroHash128*                     pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.dynamicStateCount);

    for (uint32_t i = 0; i < desc.dynamicStateCount; i++)
    {
        pHasher->Update(desc.pDynamicStates[i]);
    }
}

// =====================================================================================================================
VkResult Pipeline::BuildShaderStageInfo(
    const Device*                          pDevice,
    const uint32_t                         stageCount,
    const VkPipelineShaderStageCreateInfo* pStages,
    uint32_t                               (*pfnGetOutputIdx)(const uint32_t inputIdx,
                                                              const uint32_t stageIdx),
    ShaderStageInfo*                       pShaderStageInfo,
    ShaderModuleHandle*                    pTempModules,
    PipelineCreationFeedback*              pFeedbacks)
{
    VkResult result = VK_SUCCESS;

    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    uint32_t maxOutIdx = 0;

    for (uint32_t stageIdx = 0; stageIdx < stageCount; ++stageIdx)
    {
        const VkPipelineShaderStageCreateInfo& stageInfo = pStages[stageIdx];
        const ShaderStage                      stage     = ShaderFlagBitToStage(stageInfo.stage);
        const uint32_t                         outIdx    = pfnGetOutputIdx(stageIdx, stage);

        maxOutIdx = Util::Max(maxOutIdx, outIdx + 1);

        PipelineShaderStageExtStructs extStructs = {};

        HandleShaderStageExtensionStructs(stageInfo.pNext, &extStructs);

        if (extStructs.pPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT != nullptr)
        {
            pShaderStageInfo[outIdx].waveSize =
                extStructs.pPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT->requiredSubgroupSize;
        }

        if (stageInfo.module != VK_NULL_HANDLE)
        {
            const ShaderModule* pModule = ShaderModule::ObjectFromHandle(stageInfo.module);

            pShaderStageInfo[outIdx].pModuleHandle = pModule->GetShaderModuleHandle();
            pShaderStageInfo[outIdx].codeHash      = pModule->GetCodeHash(stageInfo.pName);
            pShaderStageInfo[outIdx].codeSize      = pModule->GetCodeSize();
        }
        else
        {
            // Caller must make sure that pTempModules should be non-null if shader module may be deprecated.
            // Meanwhile, the memory pointed by pTempModules should be initialized to 0 and can take all the
            // newly-built modules in the pipeline (at most stageCount entries required).
            // Caller should release the newly-built temporary modules set in pNewModules manually after
            // creation of pipeline.
            VK_ASSERT(pTempModules != nullptr);

            ShaderModuleFlags         flags           = 0;
            Vkgc::BinaryData          shaderBinary    = {};
            Pal::ShaderHash           codeHash        = {};
            PipelineCreationFeedback* pShaderFeedback = (pFeedbacks == nullptr) ? nullptr : pFeedbacks + outIdx;

            if (extStructs.pShaderModuleCreateInfo != nullptr)
            {
                flags                 = ShaderModule::ConvertVkShaderModuleCreateFlags(
                    extStructs.pShaderModuleCreateInfo->flags);
                shaderBinary.codeSize = extStructs.pShaderModuleCreateInfo->codeSize;
                shaderBinary.pCode    = extStructs.pShaderModuleCreateInfo->pCode;

                codeHash = ShaderModule::BuildCodeHash(
                    shaderBinary.pCode,
                    shaderBinary.codeSize);

                if (shaderBinary.pCode != nullptr)
                {
                    result = pCompiler->BuildShaderModule(
                        pDevice,
                        flags | ShaderModuleForceUncached,
                        shaderBinary,
                        &pTempModules[outIdx]);

                    pTempModules[outIdx].codeHash          = codeHash;
                    pShaderStageInfo[outIdx].pModuleHandle = &pTempModules[outIdx];
                    pShaderStageInfo[outIdx].codeSize      = shaderBinary.codeSize;
                }
            }

            if (extStructs.pPipelineShaderStageModuleIdentifierCreateInfoEXT != nullptr)
            {
                // Get the 128 bit ShaderModule Hash
                VK_ASSERT(extStructs.pPipelineShaderStageModuleIdentifierCreateInfoEXT->identifierSize ==
                          sizeof(codeHash));

                memcpy(&codeHash.lower,
                       (extStructs.pPipelineShaderStageModuleIdentifierCreateInfoEXT->pIdentifier),
                       sizeof(codeHash.lower));

                memcpy(&codeHash.upper,
                       (extStructs.pPipelineShaderStageModuleIdentifierCreateInfoEXT->pIdentifier +
                       sizeof(codeHash.lower)),
                       sizeof(codeHash.upper));
            }

            if (result != VK_SUCCESS)
            {
                break;
            }

            pShaderStageInfo[outIdx].codeHash = ShaderModule::GetCodeHash(codeHash, stageInfo.pName);
        }

        pShaderStageInfo[outIdx].stage               = stage;
        pShaderStageInfo[outIdx].pEntryPoint         = stageInfo.pName;
        pShaderStageInfo[outIdx].flags               = stageInfo.flags;
        pShaderStageInfo[outIdx].pSpecializationInfo = stageInfo.pSpecializationInfo;
    }

    if (result != VK_SUCCESS)
    {
        FreeTempModules(pDevice, maxOutIdx + 1, pTempModules);
    }

    return result;
}

// =====================================================================================================================
void Pipeline::FreeTempModules(
    const Device*       pDevice,
    const uint32_t      maxStageCount,
    ShaderModuleHandle* pTempModules)
{
    if ((pTempModules != nullptr) && (maxStageCount > 0))
    {
        PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

        for (uint32_t i = 0; i < maxStageCount; ++i)
        {
            if (pCompiler->IsValidShaderModule(&pTempModules[i]))
            {
                pCompiler->FreeShaderModule(&pTempModules[i]);
            }
        }
    }
}

// =====================================================================================================================
void Pipeline::HandleExtensionStructs(
    const void*                         pNext,
    PipelineExtStructs*                 pExtStructs)
{
    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<int32>(pHeader->sType))
        {
        // Handle extension specific structures

        case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:
        {
            pExtStructs->pPipelineCreationFeedbackCreateInfoEXT =
                static_cast<const VkPipelineCreationFeedbackCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR:
        {
            pExtStructs->pPipelineBinaryInfoKHR = static_cast<const VkPipelineBinaryInfoKHR*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT:
        {
            pExtStructs->pPipelineRobustnessCreateInfoEXT =
                static_cast<const VkPipelineRobustnessCreateInfoEXT*>(pNext);
            break;
        }
        default:
            break;
        }
        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
void Pipeline::HandleShaderStageExtensionStructs(
    const void*                         pNext,
    PipelineShaderStageExtStructs*      pExtStructs)
{
    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<int32>(pHeader->sType))
        {
            // Handle extension specific structures

        case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT:
        {
            pExtStructs->pPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT =
                static_cast<const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO:
        {
            pExtStructs->pShaderModuleCreateInfo = static_cast<const VkShaderModuleCreateInfo*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT:
        {
            pExtStructs->pPipelineShaderStageModuleIdentifierCreateInfoEXT =
                static_cast<const VkPipelineShaderStageModuleIdentifierCreateInfoEXT*>(pNext);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT:
        {
            pExtStructs->pPipelineRobustnessCreateInfoEXT =
                static_cast<const VkPipelineRobustnessCreateInfoEXT*>(pNext);
            break;
        }
        default:
            break;
        }
        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
bool Pipeline::InitPipelineRobustness(
    const VkPipelineRobustnessCreateInfoEXT* pIncomingRobustness,
    VkPipelineRobustnessCreateInfoEXT*       pCurrentRobustness)
{
    bool result = false;

    if (pIncomingRobustness != nullptr)
    {
        *pCurrentRobustness = *(pIncomingRobustness);
        result = true;
    }
    else
    {
        // the DEFAULT enums tell the pipeline to use the robustness settings from the device.
        // if we receive a VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT during creation,
        // use those enums instead
        *pCurrentRobustness = { .sType          = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
                                .pNext          = nullptr,
                                .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
                                .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
                                .vertexInputs   = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
                                .images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT };
    }

    return result;
}

// =====================================================================================================================
void Pipeline::UpdatePipelineRobustnessBufferBehavior(
    const VkPipelineRobustnessBufferBehaviorEXT incomingRobustness,
    VkPipelineRobustnessBufferBehaviorEXT*      pCurrentRobustness)
{
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT) &&
        (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT))
    {
        *pCurrentRobustness = incomingRobustness;
    }
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT) &&
        ((*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT)))
    {
        *pCurrentRobustness = incomingRobustness;
    }
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT) &&
        ((*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT)))
    {
        *pCurrentRobustness = incomingRobustness;
    }
}

// =====================================================================================================================
void Pipeline::UpdatePipelineRobustnessImageBehavior(
    const VkPipelineRobustnessImageBehaviorEXT incomingRobustness,
    VkPipelineRobustnessImageBehaviorEXT*      pCurrentRobustness)
{
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT) &&
        (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT))
    {
        *pCurrentRobustness = incomingRobustness;
    }
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_EXT) &&
        ((*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT)))
    {
        *pCurrentRobustness = incomingRobustness;
    }
    if ((incomingRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT) &&
        ((*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT) ||
         (*pCurrentRobustness == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_EXT)))
    {
        *pCurrentRobustness = incomingRobustness;
    }
}

// =====================================================================================================================
void Pipeline::UpdatePipelineRobustness(
    const VkPipelineRobustnessCreateInfoEXT* pIncomingRobustness,
    VkPipelineRobustnessCreateInfoEXT*       pCurrentRobustness)
{
    UpdatePipelineRobustnessBufferBehavior(pIncomingRobustness->storageBuffers, &(pCurrentRobustness->storageBuffers));
    UpdatePipelineRobustnessBufferBehavior(pIncomingRobustness->uniformBuffers, &(pCurrentRobustness->uniformBuffers));
    UpdatePipelineRobustnessBufferBehavior(pIncomingRobustness->vertexInputs, &(pCurrentRobustness->vertexInputs));
    UpdatePipelineRobustnessImageBehavior(pIncomingRobustness->images, &(pCurrentRobustness->images));
}

// =====================================================================================================================
Pipeline::Pipeline(
    Device* const               pDevice,
#if VKI_RAY_TRACING
    bool                        hasRayTracing,
#endif
    VkPipelineBindPoint type)
    :
    m_pDevice(pDevice),
    m_userDataLayout(),
    m_palPipelineHash(0),
    m_staticStateMask(0),
    m_apiHash(0),
    m_type(type),
#if VKI_RAY_TRACING
    m_hasRayTracing(hasRayTracing),
    m_dispatchRaysUserDataOffset(0),
#endif
    m_pBinaryStorage(nullptr),
    m_pFormatStrings(nullptr)
{
    memset(m_pPalPipeline, 0, sizeof(m_pPalPipeline));
}

void Pipeline::Init(
    Pal::IPipeline**             pPalPipeline,
    const UserDataLayout*        pLayout,
    PipelineBinaryStorage*       pBinaryStorage,
    uint64_t                     staticStateMask,
#if VKI_RAY_TRACING
    uint32_t                     dispatchRaysUserDataOffset,
#endif
    const Util::MetroHash::Hash& cacheHash,
    uint64_t                     apiHash)
{
    m_pBinaryStorage             = pBinaryStorage;
    m_staticStateMask            = staticStateMask;
    m_cacheHash                  = cacheHash;
    m_apiHash                    = apiHash;
#if VKI_RAY_TRACING
    m_dispatchRaysUserDataOffset = dispatchRaysUserDataOffset;
#endif

    if (pLayout != nullptr)
    {
        m_userDataLayout = *pLayout;
    }
    else
    {
        memset(&m_userDataLayout, 0, sizeof(UserDataLayout));
    }

    if (pPalPipeline != nullptr)
    {
        m_palPipelineHash = pPalPipeline[DefaultDeviceIndex]->GetInfo().internalPipelineHash.unique;
        for (uint32_t devIdx = 0; devIdx < m_pDevice->NumPalDevices(); devIdx++)
        {
            m_pPalPipeline[devIdx] = pPalPipeline[devIdx];
        }
    }
    else
    {
        m_palPipelineHash = 0;
        for (uint32_t devIdx = 0; devIdx < m_pDevice->NumPalDevices(); devIdx++)
        {
            m_pPalPipeline[devIdx] = nullptr;
        }
    }
}

// =====================================================================================================================
// Destroy a pipeline object.
VkResult Pipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    // Destroy PAL objects
    for (uint32_t deviceIdx = 0;
         (deviceIdx < m_pDevice->NumPalDevices()) && (m_pPalPipeline[deviceIdx] != nullptr);
         deviceIdx++)
    {
        m_pPalPipeline[deviceIdx]->Destroy();
    }

    if (m_pBinaryStorage != nullptr)
    {
        FreeBinaryStorage(m_pBinaryStorage, pAllocator);
    }

    if (m_pFormatStrings != nullptr)
    {
        Util::Destructor(m_pFormatStrings);
        pDevice->VkInstance()->FreeMem(m_pFormatStrings);
    }

    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
bool Pipeline::GetBinary(
    Pal::ShaderType     shaderType,
    PipelineBinaryInfo* pBinaryInfo) const
{
    bool result = false;
    if (PalPipeline(DefaultDeviceIndex) != nullptr)
    {
        pBinaryInfo->binaryHash           = m_cacheHash;
        pBinaryInfo->pipelineBinary.pCode =
            PalPipeline(DefaultDeviceIndex)->GetCodeObjectWithShaderType(shaderType,
                                                                         &pBinaryInfo->pipelineBinary.codeSize);
        result = true;
    }
    return result;
}

// =====================================================================================================================
VkResult Pipeline::GetShaderDisassembly(
    const Device*                 pDevice,
    const Pal::IPipeline*         pPalPipeline,
    Util::Abi::PipelineSymbolType pipelineSymbolType,
    Pal::ShaderType               shaderType,
    size_t*                       pBufferSize,
    void*                         pBuffer) const
{
    PipelineBinaryInfo binaryInfo = {};

    bool hasPipelineBinary = GetBinary(shaderType, &binaryInfo);

    if (hasPipelineBinary == false)
    {
        // The pipeline binary will be null if the pipeline wasn't created with
        // VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR or for shader
        // module identifier
        return VK_ERROR_UNKNOWN;
    }

    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    Util::Abi::PipelineAbiReader abiReader(pDevice->VkInstance()->Allocator(),
        Util::Span<const void>{binaryInfo.pipelineBinary.pCode, binaryInfo.pipelineBinary.codeSize});

    VkResult    result    = VK_SUCCESS;
    Pal::Result palResult = abiReader.Init();

    if (palResult == Pal::Result::Success)
    {
        bool symbolValid = false;
        Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();
        Util::Abi::ApiShaderType      apiShaderType = Util::Abi::ApiShaderType::Cs;

        switch (shaderType)
        {
        case Pal::ShaderType::Compute:
            apiShaderType = Util::Abi::ApiShaderType::Cs;
            break;
        case Pal::ShaderType::Task:
            apiShaderType = Util::Abi::ApiShaderType::Task;
            break;
        case Pal::ShaderType::Vertex:
            apiShaderType = Util::Abi::ApiShaderType::Vs;
            break;
        case Pal::ShaderType::Hull:
            apiShaderType = Util::Abi::ApiShaderType::Hs;
            break;
        case Pal::ShaderType::Domain:
            apiShaderType = Util::Abi::ApiShaderType::Ds;
            break;
        case Pal::ShaderType::Geometry:
            apiShaderType = Util::Abi::ApiShaderType::Gs;
            break;
        case Pal::ShaderType::Mesh:
            apiShaderType = Util::Abi::ApiShaderType::Mesh;
            break;
        case Pal::ShaderType::Pixel:
            apiShaderType = Util::Abi::ApiShaderType::Ps;
            break;
        default:
            // Pal::ShaderType mapping to Util::Abi::ApiShaderType does not match!
            VK_NEVER_CALLED();
            break;
        }

        // Support returing AMDIL/LLVM-IR and ISA
        VK_ASSERT((pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly) ||
                  (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderAmdIl));

        uint32_t hwStage = 0;
        if (Util::BitMaskScanForward(&hwStage, apiToHwShader.apiShaders[static_cast<uint32_t>(apiShaderType)]))
        {
            const char* pSectionName = nullptr;

            if (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly)
            {
                palResult = abiReader.CopySymbol(
                                Util::Abi::GetSymbolForStage(
                                    Util::Abi::PipelineSymbolType::ShaderDisassembly,
                                    static_cast<Util::Abi::HardwareStage>(hwStage)),
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
                                    apiShaderType),
                                pBufferSize,
                                pBuffer);

                pSectionName = Util::Abi::AmdGpuCommentLlvmIrName;
                symbolValid  = palResult == Util::Result::Success;
            }

            if ((symbolValid == false) && (pSectionName != nullptr))
            {
                // NOTE: LLVM doesn't add disassemble symbol in ELF disassemble section, instead, it contains
                // the entry name in disassemble section. so we have to search the entry name to split per
                // stage disassemble info.
                const auto& elfReader = abiReader.GetElfReader();
                Util::ElfReader::SectionId disassemblySectionId = elfReader.FindSection(pSectionName);

                if (disassemblySectionId != 0)
                {
                    const char* pDisassemblySection = static_cast<const char*>(
                        elfReader.GetSectionData(disassemblySectionId));
                    size_t disassemblySectionLen = static_cast<size_t>(
                        elfReader.GetSection(disassemblySectionId).sh_size);
                    const char* pDisassemblySectionEnd = pDisassemblySection + disassemblySectionLen;

                    // Search disassemble code for input shader stage
                    const char* pSymbolName = Util::Abi::PipelineAbiSymbolNameStrings[
                        static_cast<uint32_t>(Util::Abi::GetSymbolForStage(
                            Util::Abi::PipelineSymbolType::ShaderMainEntry,
                            static_cast<Util::Abi::HardwareStage>(hwStage)))];
                    const size_t symbolNameLength = strlen(pSymbolName);
                    const char* pSymbolNameEnd = pSymbolName + symbolNameLength;

                    const char* ShaderSymbolPrefix = "_amdgpu_";
                    const char* ShaderSymbolSuffix =
                        (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly) ?
                            "_amdgpu_" : "; Function Attrs";
                    const char* ShaderSymbolSuffixEnd = ShaderSymbolSuffix + strlen(ShaderSymbolSuffix);

                    VK_ASSERT(strncmp(pSymbolName, ShaderSymbolPrefix, strlen(ShaderSymbolPrefix)) == 0);

                    const char* pSymbolBase = std::search(pDisassemblySection, pDisassemblySectionEnd,
                        pSymbolName, pSymbolNameEnd);
                    if (pSymbolBase != pDisassemblySectionEnd)
                    {
                        // Search the end of disassemble code
                        const char* pSymbolBody = pSymbolBase + symbolNameLength;
                        const char* pSymbolEnd  = std::search(pSymbolBody, pDisassemblySectionEnd,
                            ShaderSymbolSuffix, ShaderSymbolSuffixEnd);

                        const size_t symbolSize = pSymbolEnd - pSymbolBase;
                        symbolValid = true;

                        // Fill output
                        if (pBufferSize != nullptr)
                        {
                            *pBufferSize = symbolSize + 1;
                        }

                        if (pBuffer != nullptr)
                        {
                            // Copy disassemble code
                            memcpy(pBuffer, pSymbolBase, symbolSize);

                            // Add null terminator
                            static_cast<char*>(pBuffer)[symbolSize] = '\0';
                        }
                    }
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
static void ConvertShaderInfoStatistics(
    const Pal::ShaderStats&    palStats,
    VkShaderStatisticsInfoAMD* pStats)
{
    memset(pStats, 0, sizeof(*pStats));

    if (palStats.shaderStageMask & Pal::ApiShaderStageCompute)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageTask)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageVertex)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_VERTEX_BIT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageHull)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageDomain)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageGeometry)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStageMesh)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    if (palStats.shaderStageMask & Pal::ApiShaderStagePixel)
    {
        pStats->shaderStageMask |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    pStats->resourceUsage.numUsedVgprs             = palStats.common.numUsedVgprs;
    pStats->resourceUsage.numUsedSgprs             = palStats.common.numUsedSgprs;
    pStats->resourceUsage.ldsSizePerLocalWorkGroup = palStats.common.ldsSizePerThreadGroup;
    pStats->resourceUsage.ldsUsageSizeInBytes      = palStats.common.ldsUsageSizeInBytes;
    pStats->resourceUsage.scratchMemUsageInBytes   = palStats.common.scratchMemUsageInBytes;
    pStats->numAvailableVgprs                      = palStats.numAvailableVgprs;
    pStats->numAvailableSgprs                      = palStats.numAvailableSgprs;

    if (palStats.shaderStageMask & Pal::ApiShaderStageCompute)
    {
        pStats->computeWorkGroupSize[0] = palStats.cs.numThreadsPerGroup.x;
        pStats->computeWorkGroupSize[1] = palStats.cs.numThreadsPerGroup.y;
        pStats->computeWorkGroupSize[2] = palStats.cs.numThreadsPerGroup.z;
    }
}

// =====================================================================================================================
// Returns a bit mask based on Pal::ShaderStageFlagBits. Bit one means that the specified type of symbol derived from
// the corresponding shader is available in the pipeline binary.
uint32_t Pipeline::GetAvailableAmdIlSymbol(
    uint32_t shaderStageMask) const
{
    VK_ASSERT(shaderStageMask != 0);
    uint32_t shaderMask = 0;

    uint32_t firstShaderStage = 0;
    Util::BitMaskScanForward(&firstShaderStage, shaderStageMask);
    Pal::ShaderType shaderType = static_cast<Pal::ShaderType>(firstShaderStage);

    PipelineBinaryInfo binaryInfo = {};
    bool hasBinary = GetBinary(shaderType, &binaryInfo);
    if (hasBinary)
    {
        Util::Abi::PipelineAbiReader abiReader(m_pDevice->VkInstance()->Allocator(),
            Util::Span<const void>{binaryInfo.pipelineBinary.pCode, binaryInfo.pipelineBinary.codeSize});
        Pal::Result result = abiReader.Init();

        if (result == Pal::Result::Success)
        {
            const Util::Abi::ApiHwShaderMapping apiToHwShader = PalPipeline(DefaultDeviceIndex)->ApiHwShaderMapping();
            for (uint32_t i = 0; i < Util::ArrayLen32(IndexShaderStages); ++i)
            {
                const Pal::ShaderStageFlagBits palShaderType = IndexShaderStages[i].palFlagBits;
                const Util::Abi::ApiShaderType abiShaderType = IndexShaderStages[i].abiType;

                uint32_t hwStage = 0;

                if (((palShaderType & shaderStageMask) != 0) &&
                    (static_cast<uint32_t>(abiShaderType) != 0) &&
                    Util::BitMaskScanForward(&hwStage, apiToHwShader.apiShaders[static_cast<uint32_t>(abiShaderType)]))
                {
                    const Util::Elf::SymbolTableEntry* pSymbolEntry = nullptr;
                    const char* pSectionName                        = nullptr;

                    pSymbolEntry = abiReader.GetSymbolHeader(
                        Util::Abi::GetSymbolForStage(
                            Util::Abi::PipelineSymbolType::ShaderAmdIl,
                            abiShaderType));
                    pSectionName = Util::Abi::AmdGpuCommentLlvmIrName;

                    if ((pSymbolEntry != nullptr) ||
                        ((pSectionName != nullptr) && (abiReader.GetElfReader().FindSection(pSectionName) != 0)))
                    {
                        shaderMask |= palShaderType;
                    }
                }
            }
        }
    }

    return shaderMask;
}

// =====================================================================================================================
// Generate a cache ID using an elf hash as a baseline. Environment characteristics are added here that are not
// accounted for by the platform kay
void Pipeline::ElfHashToCacheId(
    const Device*                pDevice,
    uint32_t                     deviceIdx,
    const Util::MetroHash::Hash& elfHash,
    const PipelineOptimizerKey&  pipelineOptimizerKey,
    Util::MetroHash::Hash*       pCacheId
)
{
    Util::MetroHash::Hash settingsHash = pDevice->VkPhysicalDevice(deviceIdx)->GetSettingsLoader()->GetSettingsHash();

    Util::MetroHash128 hasher = {};
    hasher.Update(elfHash);
    hasher.Update(deviceIdx);
    hasher.Update(settingsHash);

    // Incorporate any tuning profiles
    pDevice->GetShaderOptimizer()->CalculateMatchingProfileEntriesHash(pipelineOptimizerKey, &hasher);

    // Calling `hasher.Update(uint32_t)` on uint32_t : 1 flag sends 31 always-zero bits to the hasher.
    // `utils::BitPacker` packs flag bits into single variable to save MetroHash128 a hashing cycle or two.
    utils::BitPacker flags;

    // Extensions and features whose enablement affects compiler inputs (and hence the binary)
    flags.Push(pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO));
    {
        flags.Push(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_PRIMITIVES_GENERATED_QUERY));
        flags.Push(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK));
        flags.Push(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT));
        flags.Push(pDevice->GetEnabledFeatures().scalarBlockLayout);
    }
    flags.Push(pDevice->GetEnabledFeatures().robustBufferAccess);
    flags.Push(pDevice->GetEnabledFeatures().robustUnboundVertexAttribute);
    flags.Push(pDevice->GetEnabledFeatures().robustBufferAccessExtended);
    flags.Push(pDevice->GetEnabledFeatures().robustImageAccessExtended);
    flags.Push(pDevice->GetEnabledFeatures().nullDescriptorExtended);
    flags.Push(pDevice->GetEnabledFeatures().pipelineRobustness);

#if VKI_RAY_TRACING
    if (pDevice->RayTrace() != nullptr)
    {
        // The accel struct tracker enable and the trace ray counter states get stored inside the ELF within
        // the static GpuRT flags. Needed for both TraceRay() and RayQuery().
        flags.Push(pDevice->RayTrace()->AccelStructTrackerEnabled(deviceIdx));
        hasher.Update(pDevice->RayTrace()->TraceRayCounterMode(deviceIdx));
    }
#endif

    hasher.Update(flags.Get());

    hasher.Finalize(pCacheId->bytes);
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    const VkAllocationCallbacks*                pAllocator)
{
    if (pipeline != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Pipeline::BaseObjectFromHandle(pipeline)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
// Implementation of vkGetShaderInfoAMD for VK_AMD_shader_info
VKAPI_ATTR VkResult VKAPI_CALL vkGetShaderInfoAMD(
    VkDevice               device,
    VkPipeline             pipeline,
    VkShaderStageFlagBits  shaderStage,
    VkShaderInfoTypeAMD    infoType,
    size_t*                pBufferSize,
    void*                  pBuffer)
{
    VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;

    const Device*         pDevice      = ApiDevice::ObjectFromHandle(device);
    const Pipeline*       pPipeline    = Pipeline::BaseObjectFromHandle(pipeline);
    const Pal::IPipeline* pPalPipeline = pPipeline->PalPipeline(DefaultDeviceIndex);

    if (pPipeline != nullptr)
    {
        Pal::ShaderType shaderType = VkToPalShaderType(shaderStage);

        if (infoType == VK_SHADER_INFO_TYPE_STATISTICS_AMD)
        {
            Pal::ShaderStats palStats = {};
            Pal::Result palResult = pPalPipeline->GetShaderStats(shaderType, &palStats, true);

            if ((palResult == Pal::Result::Success) ||
                (palResult == Pal::Result::ErrorInvalidMemorySize)) // This error is harmless and is a PAL bug w/around
            {
                if (pBufferSize != nullptr)
                {
                    *pBufferSize = sizeof(VkShaderStatisticsInfoAMD);
                }

                if (pBuffer != nullptr)
                {
                    VkShaderStatisticsInfoAMD* pStats = static_cast<VkShaderStatisticsInfoAMD*>(pBuffer);

                    ConvertShaderInfoStatistics(palStats, pStats);

                    const Pal::DeviceProperties& info = pDevice->GetPalProperties();

                    pStats->numPhysicalVgprs = info.gfxipProperties.shaderCore.vgprsPerSimd;
                    pStats->numPhysicalSgprs = info.gfxipProperties.shaderCore.sgprsPerSimd;
                }

                result = VK_SUCCESS;
            }
        }
        else if (infoType == VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD)
        {
            result = pPipeline->GetShaderDisassembly(
                pDevice,
                pPalPipeline,
                Util::Abi::PipelineSymbolType::ShaderDisassembly,
                shaderType,
                pBufferSize,
                pBuffer);
        }
        else if (infoType == VK_SHADER_INFO_TYPE_BINARY_AMD)
        {
            PipelineBinaryInfo binaryInfo = {};
            bool hasBinary = pPipeline->GetBinary(shaderType, &binaryInfo);

            if (hasBinary && (binaryInfo.pipelineBinary.pCode != nullptr))
            {
                if (pBuffer != nullptr)
                {
                    const size_t copySize = Util::Min(*pBufferSize, binaryInfo.pipelineBinary.codeSize);

                    memcpy(pBuffer, binaryInfo.pipelineBinary.pCode, copySize);

                    result = (copySize == binaryInfo.pipelineBinary.codeSize) ? VK_SUCCESS : VK_INCOMPLETE;
                }
                else
                {
                    *pBufferSize = binaryInfo.pipelineBinary.codeSize;

                    result = VK_SUCCESS;
                }
            }
        }
    }
    else
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
static void BuildPipelineNameDescription(
    const char*              pPreName,
    const char*              pShaderName,
    char*                    pName,
    char*                    pDescription,
    Util::Abi::HardwareStage hwStage,
    uint32_t                 palShaderMask)
{
    // Build a name and description string for the HW Shader
    char shaderName[VK_MAX_DESCRIPTION_SIZE];
    Util::Strncpy(shaderName, pPreName, VK_MAX_DESCRIPTION_SIZE);
    Util::Strncat(shaderName, VK_MAX_DESCRIPTION_SIZE, pShaderName);
    strncpy(pName, shaderName, VK_MAX_DESCRIPTION_SIZE);

    // Build the description string using the VkShaderStageFlagBits
    // that correspond to the HW Shader
    char shaderDescription[VK_MAX_DESCRIPTION_SIZE];

    // Beginning of the description
    Util::Strncpy(shaderDescription, "Executable handles following Vulkan stages: ", VK_MAX_DESCRIPTION_SIZE);

    if (palShaderMask & Pal::ApiShaderStageCompute)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_COMPUTE_BIT ");
    }

    if (palShaderMask & Pal::ApiShaderStageTask)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_TASK_BIT_EXT ");
    }

    if (palShaderMask & Pal::ApiShaderStageVertex)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_VERTEX_BIT ");
    }

    if (palShaderMask & Pal::ApiShaderStageHull)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ");
    }

    if (palShaderMask & Pal::ApiShaderStageDomain)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ");
    }

    if (palShaderMask & Pal::ApiShaderStageGeometry)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_GEOMETRY_BIT ");
    }

    if (palShaderMask & Pal::ApiShaderStageMesh)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_MESH_BIT_EXT ");
    }

    if (palShaderMask & Pal::ApiShaderStagePixel)
    {
        Util::Strncat(shaderDescription, VK_MAX_DESCRIPTION_SIZE, " VK_SHADER_STAGE_FRAGMENT_BIT ");
    }

    // Copy built string to the description with remainder of the string \0 filled.
    // Having the \0 to VK_MAX_DESCRIPTION_SIZE is a requirement to get the cts tests to pass.
    strncpy(pDescription, shaderDescription, VK_MAX_DESCRIPTION_SIZE);
}

// =====================================================================================================================
static uint32_t CountNumberOfHWStages(
    uint32_t*                            pHwStageMask,
    const Util::Abi::ApiHwShaderMapping& apiToHwShader)
{
    VK_ASSERT(pHwStageMask != nullptr);

    *pHwStageMask = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count); i++)
    {
         uint32_t hwStage = 0;
         if (Util::BitMaskScanForward(&hwStage, apiToHwShader.apiShaders[static_cast<uint32_t>(i)]))
         {
             *pHwStageMask |= (1 << hwStage);
         }
    }

    // The number of bits set in the HW Mask is the number HW shaders used
    return Util::CountSetBits(*pHwStageMask);
}

// =====================================================================================================================
// Get HW Stage for executable index
static Util::Abi::HardwareStage GetHwStageForExecutableIndex(
    uint32_t executableIndex,
    uint32_t hwStageMask)
{
    uint32_t hwStage = 0;
    for (uint32_t i = 0; i <= executableIndex; ++i)
    {
        Util::BitMaskScanForward(&hwStage, hwStageMask);
        hwStageMask &= ~(1 << hwStage);
    }

    // HW Stage should never exceed number of available HW Stages
    VK_ASSERT(hwStage < static_cast<uint32_t>(Util::Abi::HardwareStage::Count));

    return static_cast<Util::Abi::HardwareStage>(hwStage);
}

// =====================================================================================================================
// Convert from the HW Shader stage back to the corresponding API Stage
static Pal::ShaderType GetApiShaderFromHwShader(
    Util::Abi::HardwareStage             hwStage,
    const Util::Abi::ApiHwShaderMapping& apiToHwShader)
{
    Pal::ShaderType apiShaderType = Pal::ShaderType::Compute;
    for (uint32_t i = 0; i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count); ++i)
    {
        uint32_t apiHWStage = 0;
        Util::BitMaskScanForward(&apiHWStage, apiToHwShader.apiShaders[i]);

        if (apiToHwShader.apiShaders[i] & (1 << static_cast<uint32_t>(hwStage)))
        {
            switch (static_cast<Util::Abi::ApiShaderType>(i))
            {
            case Util::Abi::ApiShaderType::Cs:
                apiShaderType = Pal::ShaderType::Compute;
                break;
            case Util::Abi::ApiShaderType::Task:
                apiShaderType = Pal::ShaderType::Task;
                break;
            case Util::Abi::ApiShaderType::Vs:
                apiShaderType = Pal::ShaderType::Vertex;
                break;
            case Util::Abi::ApiShaderType::Hs:
                apiShaderType = Pal::ShaderType::Hull;
                break;
            case Util::Abi::ApiShaderType::Ds:
                apiShaderType = Pal::ShaderType::Domain;
                break;
            case Util::Abi::ApiShaderType::Gs:
                apiShaderType = Pal::ShaderType::Geometry;
                break;
            case Util::Abi::ApiShaderType::Mesh:
                apiShaderType = Pal::ShaderType::Mesh;
                break;
            case Util::Abi::ApiShaderType::Ps:
                apiShaderType = Pal::ShaderType::Pixel;
                break;
            default:
                // Util::Abi::ApiShaderType mapping to Pal::ShaderType does not match!
                VK_NEVER_CALLED();
                break;
            }
            break;
        }
    }

    return apiShaderType;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutablePropertiesKHR(
    VkDevice                                    device,
    const VkPipelineInfoKHR*                    pPipelineInfo,
    uint32_t*                                   pExecutableCount,
    VkPipelineExecutablePropertiesKHR*          pProperties)
{
    const Pipeline*                     pPipeline     = Pipeline::BaseObjectFromHandle(pPipelineInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

#if VKI_RAY_TRACING
    if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
    {
        const RayTracingPipeline* pRayTracingPipeline = static_cast<const RayTracingPipeline*>(pPipeline);

        return pRayTracingPipeline->GetPipelineExecutableProperties(pPipelineInfo,
                                                                pExecutableCount,
                                                                pProperties);
    }
#endif

    // Count the number of hardware stages that are used in this pipeline
    uint32_t hwStageMask = 0;
    uint32_t numHWStages = CountNumberOfHWStages(&hwStageMask, apiToHwShader);

    // If pProperties == nullptr the call to this function is just ment to return the number of executables
    // in the pipeline
    if (pProperties == nullptr)
    {
        *pExecutableCount = numHWStages;
        return VK_SUCCESS;
    }

    VkShaderStatisticsInfoAMD  vkShaderStats   = {};
    Pal::ShaderStats           palStats        = {};
    uint32_t                   outputCount     = 0;
    constexpr char             shaderPreName[] = "ShaderProperties";

    // Return the name / description for the pExecutableCount number of executables.
    uint32 i = 0;
    while (Util::BitMaskScanForward(&i, hwStageMask) && (outputCount < *pExecutableCount))
    {
        // Get an api shader type for the corresponding HW Shader
        Pal::ShaderType shaderType = GetApiShaderFromHwShader(static_cast<Util::Abi::HardwareStage>(i), apiToHwShader);

        // Get the shader stats from the shader in the pipeline
        Pal::Result palResult = pPalPipeline->GetShaderStats(shaderType, &palStats, true);

        // Covert to the pal statistics to VkShaderStatisticsInfoAMD
        ConvertShaderInfoStatistics(palStats, &vkShaderStats);

        // Set VkShaderStageFlagBits as an output property
        pProperties[outputCount].stages = vkShaderStats.shaderStageMask;

        // Convert HW Stage to API String Name
        Util::Abi::HardwareStage hwStage    = static_cast<Util::Abi::HardwareStage>(i);
        const char*              pHwStageString = HwStageNames[static_cast<uint32_t>(hwStage)];

        // Build the name and description of the output property
        BuildPipelineNameDescription(
            shaderPreName,
            pHwStageString,
            pProperties[outputCount].name,
            pProperties[outputCount].description,
            hwStage,
            palStats.shaderStageMask);

         // If this is a compute shader, report the workgroup size
         if (vkShaderStats.shaderStageMask & VK_SHADER_STAGE_COMPUTE_BIT)
         {
             pProperties[outputCount].subgroupSize = vkShaderStats.computeWorkGroupSize[0] *
                                                     vkShaderStats.computeWorkGroupSize[1] *
                                                     vkShaderStats.computeWorkGroupSize[2];
         }

         hwStageMask &= ~(1 << i);
         outputCount++;
     }

    // Write out the number of stages written
    *pExecutableCount = outputCount;

    // If the requested number of executables was less than the available number of hw stages, return Incomplete
    return (*pExecutableCount < numHWStages) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableStatisticsKHR(
    VkDevice                                    device,
    const VkPipelineExecutableInfoKHR*          pExecutableInfo,
    uint32_t*                                   pStatisticCount,
    VkPipelineExecutableStatisticKHR*           pStatistics)
{
    const Pipeline*                     pPipeline     = Pipeline::BaseObjectFromHandle(pExecutableInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

    // If pStatisticCount == nullptr the call to this function is just ment to return the number of statistics
    // for an executable in a pipeline.
    if (pStatistics == nullptr)
    {
        *pStatisticCount = ExecutableStatisticsCount;
        return VK_SUCCESS;
    }

    // Count the number of hardware stages that are used in this pipeline
    uint32_t hwStageMask = 0;
    uint32_t numHWStages = CountNumberOfHWStages(&hwStageMask, apiToHwShader);

    // The executable index should be less than the number of HW Stages.
    VK_ASSERT(pExecutableInfo->executableIndex < numHWStages);

    // Get hwStage for executable index
    Util::Abi::HardwareStage hwStage = GetHwStageForExecutableIndex(pExecutableInfo->executableIndex, hwStageMask);

    // Get an api shader type for the corresponding HW Shader
    Pal::ShaderType shaderType = GetApiShaderFromHwShader(hwStage, apiToHwShader);

    // Get the shader stats for the corresponding API stage
    VkShaderStatisticsInfoAMD  vkShaderStats = {};
    Pal::ShaderStats           palStats      = {};

    Pal::Result palResult = pPalPipeline->GetShaderStats(shaderType, &palStats, true);

    // Return error is the there are now statics for stage.
    if (palResult != Pal::Result::Success)
    {
        return VK_ERROR_UNKNOWN;
    }

    // Convert from PAL to VK statistics
    ConvertShaderInfoStatistics(palStats, &vkShaderStats);

    VkPipelineExecutableStatisticKHR executableStatics[ExecutableStatisticsCount] =
    {
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "numUsedVgprs",
         "Number of used VGPRs", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, {}},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "numUsedSgprs",
         "Number of used SGPRs", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, {}},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "ldsSizePerLocalWorkGroup",
         "LDS size per local workgroup", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, {}},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "ldsUsageSizeInBytes",
         "LDS usage size in Bytes", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, {}},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "scratchMemUsageInBytes",
         "Scratch memory usage in Bytes", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, {}}
    };

    // Number of used Vgprs
    executableStatics[0].value.u64 = vkShaderStats.resourceUsage.numUsedVgprs;

    // Number of used Sgprs
    executableStatics[1].value.u64 = vkShaderStats.resourceUsage.numUsedSgprs;

    // LDS Size Per Local WorkGroup
    executableStatics[2].value.u64 = vkShaderStats.resourceUsage.ldsSizePerLocalWorkGroup;

    // LDS usage size in Bytes
    executableStatics[3].value.u64 = vkShaderStats.resourceUsage.ldsUsageSizeInBytes;

    // Scratch memory usage in Bytes
    executableStatics[4].value.u64 = vkShaderStats.resourceUsage.scratchMemUsageInBytes;

    // Overwrite the number of written statistics
    *pStatisticCount = Util::Min(*pStatisticCount, static_cast<uint32_t>(ExecutableStatisticsCount));

    // Copy pStatisticCount number of statistics
    memcpy(pStatistics, executableStatics, (sizeof(VkPipelineExecutableStatisticKHR) * (*pStatisticCount)));

    // If the requested number of statistics was less than the available number of statics,
    // return Incomplete
    return ((*pStatisticCount) < ExecutableStatisticsCount) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableInternalRepresentationsKHR(
    VkDevice                                       device,
    const VkPipelineExecutableInfoKHR*             pExecutableInfo,
    uint32_t*                                      pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations)
{
    const Device*                       pDevice       = ApiDevice::ObjectFromHandle(device);
    const Pipeline*                     pPipeline     = Pipeline::BaseObjectFromHandle(pExecutableInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

#if VKI_RAY_TRACING
    if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
    {
        const RayTracingPipeline* pRayTracingPipeline = static_cast<const RayTracingPipeline*>(pPipeline);

        return pRayTracingPipeline->GetPipelineExecutableInternalRepresentations(pExecutableInfo,
                                                                        pInternalRepresentationCount,
                                                                        pInternalRepresentations);
    }
#endif

    // Count the number of hardware stages that are used in this pipeline
    uint32_t hwStageMask = 0;
    uint32_t numHWStages = CountNumberOfHWStages(&hwStageMask, apiToHwShader);

    // Get hwStage for executable index
    Util::Abi::HardwareStage hwStage = GetHwStageForExecutableIndex(pExecutableInfo->executableIndex, hwStageMask);

    // Convert from the HW Shader stage back to the corresponding API Stage
    Pal::ShaderType apiShaderType = GetApiShaderFromHwShader(hwStage, apiToHwShader);

    // Get the shader stats from the shader in the pipeline
    Pal::ShaderStats palStats  = {};
    Pal::Result      palResult = pPalPipeline->GetShaderStats(apiShaderType, &palStats, true);

    // Return (Number of Intermediate Shaders) + Number of HW ISA shaders
    uint32_t numberOfInternalRepresentations = (palStats.palShaderHash.lower > 0) ?
        (Util::CountSetBits(pPipeline->GetAvailableAmdIlSymbol(palStats.shaderStageMask)) + 1) : 0;

    if (pInternalRepresentations == nullptr)
    {
        // +1 for elf binary output
        *pInternalRepresentationCount = numberOfInternalRepresentations + 1;
        return VK_SUCCESS;
    }

    // Output the Intermediate API Shaders
    uint32_t outputCount   = 0;
    uint32_t apiShaderMask = pPipeline->GetAvailableAmdIlSymbol(palStats.shaderStageMask);

    uint32_t i = 0;
    while((Util::BitMaskScanForward(&i, apiShaderMask)) &&
          (outputCount < Util::Min(numberOfInternalRepresentations, *pInternalRepresentationCount)))
    {
         // Build the name and description of the output property for IL
         const char*              pApiString    = ApiStageNames[i];
         Pal::ShaderStageFlagBits palShaderMask = IndexShaderStages[i].palFlagBits;

         BuildPipelineNameDescription(
             shaderPreNameIntermediate,
             pApiString,
             pInternalRepresentations[outputCount].name,
             pInternalRepresentations[outputCount].description,
             static_cast<Util::Abi::HardwareStage>(i),
             static_cast<uint32_t>(palShaderMask));

         // Get the text based IL disassembly of the shader
         pPipeline->GetShaderDisassembly(
             pDevice,
             pPalPipeline,
             Util::Abi::PipelineSymbolType::ShaderAmdIl,
             apiShaderType,
             &(pInternalRepresentations[outputCount].dataSize),
             pInternalRepresentations[outputCount].pData);

         // Mark that the output IL disassembly is text formated
         pInternalRepresentations[outputCount].isText = VK_TRUE;

        apiShaderMask &= ~(1 << i);
        outputCount++;
    }

    // Output the ISA shaders
    if (outputCount < Util::Min(numberOfInternalRepresentations, *pInternalRepresentationCount))
    {
        // Build the name and description of the output property for ISA Shader
        const char* pApiString = HwStageNames[static_cast<uint32_t>(hwStage)];

        BuildPipelineNameDescription(
            shaderPreNameISA,
            pApiString,
            pInternalRepresentations[outputCount].name,
            pInternalRepresentations[outputCount].description,
            static_cast<Util::Abi::HardwareStage>(hwStage),
            palStats.shaderStageMask);

        // Get the text based ISA disassembly of the shader
        pPipeline->GetShaderDisassembly(
            pDevice,
            pPalPipeline,
            Util::Abi::PipelineSymbolType::ShaderDisassembly,
            apiShaderType,
            &(pInternalRepresentations[outputCount].dataSize),
            pInternalRepresentations[outputCount].pData);

        // Mark that the output ISA disassembly is text formated
        pInternalRepresentations[outputCount].isText =
            (pInternalRepresentations[outputCount].dataSize > 0) ? VK_TRUE : VK_FALSE;

        outputCount++;
    }

    // Add 1 for ELF internal representaion and output the elf binary
    numberOfInternalRepresentations++;
    if (*pInternalRepresentationCount == numberOfInternalRepresentations)
    {
        PipelineBinaryInfo binaryInfo = {};
        const char* pApiString = HwStageNames[static_cast<uint32_t>(hwStage)];

        bool hasPipelineBinary = pPipeline->GetBinary(apiShaderType, &binaryInfo);

        if (hasPipelineBinary == true)
        {
            VkResult tempResult = VK_ERROR_UNKNOWN;

            if ((pInternalRepresentations[outputCount].dataSize == 0)
                || (pInternalRepresentations[outputCount].pData == nullptr))
            {
                pInternalRepresentations[outputCount].dataSize = binaryInfo.pipelineBinary.codeSize + 1;
                tempResult = VK_SUCCESS;
            }
            else
            {
                VK_ASSERT(binaryInfo.pipelineBinary.pCode != nullptr);
                memcpy(pInternalRepresentations[outputCount].pData,
                        binaryInfo.pipelineBinary.pCode,
                        Util::Min(pInternalRepresentations[outputCount].dataSize, binaryInfo.pipelineBinary.codeSize));

                if (pInternalRepresentations[outputCount].dataSize > binaryInfo.pipelineBinary.codeSize)
                {
                    char *pBuffer = static_cast<char*>(pInternalRepresentations[outputCount].pData);
                    pBuffer[binaryInfo.pipelineBinary.codeSize] = '\0';
                    tempResult = VK_SUCCESS;
                }
            }

            BuildPipelineNameDescription(
                shaderPreNameELF,
                pApiString,
                pInternalRepresentations[outputCount].name,
                pInternalRepresentations[outputCount].description,
                static_cast<Util::Abi::HardwareStage>(hwStage),
                palStats.shaderStageMask);

            if (tempResult == VK_SUCCESS)
            {
                outputCount++;
            }
        }
        else
        {
            Util::Strncpy(pInternalRepresentations[outputCount].name, "ELF", VK_MAX_DESCRIPTION_SIZE);
            Util::Strncat(pInternalRepresentations[outputCount].name, VK_MAX_DESCRIPTION_SIZE, pApiString);
            Util::Strncpy(pInternalRepresentations[outputCount].description,
                                            "elf shader binary not ready", VK_MAX_DESCRIPTION_SIZE);
        }
    }

    // Write out the number of shader ouputs.
    *pInternalRepresentationCount = outputCount;

    // If the requested number of executables was less than the available number of hw stages, return Incomplete
    return (*pInternalRepresentationCount < numberOfInternalRepresentations) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetPipelineIndirectDeviceAddressNV(
    VkDevice                                        device,
    const VkPipelineIndirectDeviceAddressInfoNV*    pInfo)
{
    VK_NOT_IMPLEMENTED;
    return 0;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPipelineIndirectMemoryRequirementsNV(
    VkDevice                                        device,
    const VkComputePipelineCreateInfo*              pCreateInfo,
    VkMemoryRequirements2*                          pMemoryRequirements)
{
    VK_NOT_IMPLEMENTED;
}

} // namespace entry

} // namespace vk
