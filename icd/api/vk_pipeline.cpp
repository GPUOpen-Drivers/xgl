/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_layout.h"

#include "palAutoBuffer.h"
#include "palInlineFuncs.h"
#include "palPipeline.h"
#include "palPipelineAbi.h"
#include "palPipelineAbiProcessorImpl.h"
#include "palMetroHash.h"

namespace vk
{

// ShaderType to string conversion table.
const char* ApiShaderTypeStrings[] =
{
    "CS",
    "VS",
    "HS",
    "DS",
    "GS",
    "PS",
};

static_assert(VK_ARRAY_SIZE(ApiShaderTypeStrings) == Pal::NumShaderTypes,
    "Number of PAL/API shader types should match.");

// The number of executable statistics to return through
// the vkGetPipelineExecutableStatisticsKHR function
static constexpr uint32_t ExecutableStatisticsCount = 5;

// =====================================================================================================================
// Generates a hash using the contents of a VkSpecializationInfo struct
void Pipeline::GenerateHashFromSpecializationInfo(
    Util::MetroHash128*         pHasher,
    const VkSpecializationInfo& desc)
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
// Generates a hash using the contents of a VkPipelineShaderStageCreateInfo struct
void Pipeline::GenerateHashFromShaderStageCreateInfo(
    Util::MetroHash128*                    pHasher,
    const VkPipelineShaderStageCreateInfo& desc)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.stage);
    pHasher->Update(ShaderModule::ObjectFromHandle(desc.module)->GetCodeHash(desc.pName));

    if (desc.pSpecializationInfo != nullptr)
    {
        GenerateHashFromSpecializationInfo(pHasher, *desc.pSpecializationInfo);
    }

}

// =====================================================================================================================
Pipeline::Pipeline(
    Device* const         pDevice,
    Pal::IPipeline**      pPalPipeline,
    const PipelineLayout* pLayout,
    PipelineBinaryInfo*   pBinary,
    uint32_t              staticStateMask)
    :
    m_pDevice(pDevice),
    m_userDataLayout(pLayout->GetInfo().userDataLayout),
    m_staticStateMask(staticStateMask),
    m_apiHash(0),
    m_pBinary(pBinary)
{
    memset(m_pPalPipeline, 0, sizeof(m_pPalPipeline));

    m_palPipelineHash = pPalPipeline[DefaultDeviceIndex]->GetInfo().internalPipelineHash.unique;

    for (uint32_t devIdx = 0; devIdx < pDevice->NumPalDevices(); devIdx++)
    {
        m_pPalPipeline[devIdx] = pPalPipeline[devIdx];
    }
}

// =====================================================================================================================
Pipeline::~Pipeline()
{
    // Destroy PAL object
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        m_pPalPipeline[deviceIdx]->Destroy();
    }
}

// =====================================================================================================================
// Destroy a pipeline object.
VkResult Pipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    // Free binary if it exists
    if (m_pBinary != nullptr)
    {
        m_pBinary->Destroy(pAllocator);
    }

    // Call destructor
    this->~Pipeline();

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Pipeline::GetShaderDisassembly(
    const Device*         pDevice,
    const Pal::IPipeline* pPalPipeline,
    Pal::ShaderType       shaderType,
    size_t*               pBufferSize,
    void*                 pBuffer) const
{
    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());

    const PipelineBinaryInfo* pPipelineBinary = GetBinary();

    if (pPipelineBinary == nullptr)
    {
        // The pipelineBinary will be null if the pipeline wasn't created with
        // VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR
        VK_NEVER_CALLED();

        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkResult    result    = VK_SUCCESS;
    Pal::Result palResult = abiProcessor.LoadFromBuffer(pPipelineBinary->pBinary, pPipelineBinary->binaryByteSize);

    if (palResult == Pal::Result::Success)
    {
        bool symbolValid = false;
        Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

        static_assert(((static_cast<uint32_t>(Util::Abi::ApiShaderType::Cs) == static_cast<uint32_t>(Pal::ShaderType::Compute)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Vs) == static_cast<uint32_t>(Pal::ShaderType::Vertex)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Hs) == static_cast<uint32_t>(Pal::ShaderType::Hull)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Ds) == static_cast<uint32_t>(Pal::ShaderType::Domain)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Gs) == static_cast<uint32_t>(Pal::ShaderType::Geometry)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Ps) == static_cast<uint32_t>(Pal::ShaderType::Pixel)) &&
            (static_cast<uint32_t>(Util::Abi::ApiShaderType::Count) == Pal::NumShaderTypes)),
            "Util::Abi::ApiShaderType to Pal::ShaderType mapping does not match!");

        uint32_t hwStage = 0;
        if (Util::BitMaskScanForward(&hwStage, apiToHwShader.apiShaders[static_cast<uint32_t>(shaderType)]))
        {
            Util::Abi::PipelineSymbolEntry symbol = {};
            const void* pDisassemblySection = nullptr;
            size_t      disassemblySectionLen = 0;

            symbolValid = abiProcessor.HasPipelineSymbolEntry(
                Util::Abi::GetSymbolForStage(
                    Util::Abi::PipelineSymbolType::ShaderDisassembly,
                    static_cast<Util::Abi::HardwareStage>(hwStage)),
                &symbol);

            abiProcessor.GetDisassembly(&pDisassemblySection, &disassemblySectionLen);

            if (symbolValid)
            {
                if (pBufferSize != nullptr)
                {
                    *pBufferSize = static_cast<size_t>(symbol.size);
                }

                if (pBuffer != nullptr)
                {
                    VK_ASSERT((symbol.size + symbol.value) <= disassemblySectionLen);

                    // Copy disassemble code
                    memcpy(pBuffer,
                           Util::VoidPtrInc(pDisassemblySection, static_cast<size_t>(symbol.value)),
                           static_cast<size_t>(symbol.size));
                }
            }
            else if (pDisassemblySection != nullptr)
            {
                // NOTE: LLVM doesn't add disassemble symbol in ELF disassemble section, instead, it contains
                // the entry name in disassemble section. so we have to search the entry name to split per
                // stage disassemble info.
                char* pDisassembly = const_cast<char*>(static_cast<const char*>(pDisassemblySection));

                // Force disassemble string to be a C-style string
                char disassemblyEnd = pDisassembly[disassemblySectionLen - 1];
                pDisassembly[disassemblySectionLen - 1] = 0;

                // Search disassemble code for input shader stage
                const char* pSymbolName = Util::Abi::PipelineAbiSymbolNameStrings[
                    static_cast<uint32_t>(Util::Abi::GetSymbolForStage(
                        Util::Abi::PipelineSymbolType::ShaderMainEntry,
                        static_cast<Util::Abi::HardwareStage>(hwStage)))];
                const size_t symbolNameLength = strlen(pSymbolName);
                const char* ShaderSymbolPrefix = "_amdgpu_";

                VK_ASSERT(strncmp(pSymbolName, ShaderSymbolPrefix, strlen(ShaderSymbolPrefix)) == 0);
                VK_ASSERT(strlen(pDisassembly) + 1 == disassemblySectionLen);

                const char* pSymbolBase = strstr(reinterpret_cast<const char*>(pDisassemblySection), pSymbolName);
                if (pSymbolBase != nullptr)
                {
                    // Search the end of disassemble code
                    const char* pSymbolBody = pSymbolBase + symbolNameLength;
                    const char* pSymbolEnd = strstr(pSymbolBody, ShaderSymbolPrefix);

                    size_t symbolSize = (pSymbolEnd == nullptr) ?
                        disassemblySectionLen - (pSymbolBase - pDisassembly) :
                        (pSymbolEnd - pSymbolBase);
                    symbolValid = true;

                    // Restore the last character
                    pDisassembly[disassemblySectionLen - 1] = disassemblyEnd;

                    // Fill output
                    if (pBufferSize != nullptr)
                    {
                        *pBufferSize = static_cast<size_t>(symbolSize);
                    }

                    if (pBuffer)
                    {
                        // Copy disassemble code
                        memcpy(pBuffer, pSymbolBase, symbolSize);
                    }
                }
                else
                {
                    // Restore the last character
                    pDisassembly[disassemblySectionLen - 1] = disassemblyEnd;
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
PipelineBinaryInfo* PipelineBinaryInfo::Create(
    size_t                       size,
    const void*                  pBinary,
    const VkAllocationCallbacks* pAllocator)
{
    PipelineBinaryInfo* pInfo = nullptr;

    if ((pBinary != nullptr) && (size > 0))
    {
        void* pStorage = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(PipelineBinaryInfo) + size,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pStorage != nullptr)
        {
            pInfo = VK_PLACEMENT_NEW(pStorage) PipelineBinaryInfo();

            pInfo->binaryByteSize = size;
            pInfo->pBinary        = Util::VoidPtrInc(pStorage, sizeof(PipelineBinaryInfo));

            memcpy(pInfo->pBinary, pBinary, size);
        }
    }

    return pInfo;
}

// =====================================================================================================================
void PipelineBinaryInfo::Destroy(
    const VkAllocationCallbacks* pAllocator)
{
    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);
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
        pStats->computeWorkGroupSize[0] = palStats.cs.numThreadsPerGroupX;
        pStats->computeWorkGroupSize[1] = palStats.cs.numThreadsPerGroupY;
        pStats->computeWorkGroupSize[2] = palStats.cs.numThreadsPerGroupZ;
    }
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

        Pipeline::ObjectFromHandle(pipeline)->Destroy(pDevice, pAllocCB);
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
    const Pipeline*       pPipeline    = Pipeline::ObjectFromHandle(pipeline);
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

                    Pal::DeviceProperties info;

                    pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalDevice()->GetProperties(&info);

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
                shaderType,
                pBufferSize,
                pBuffer);
        }
        else if (infoType == VK_SHADER_INFO_TYPE_BINARY_AMD)
        {
            const PipelineBinaryInfo* pBinary = pPipeline->GetBinary();

            if (pBinary != nullptr)
            {
                if (pBuffer != nullptr)
                {
                    const size_t copySize = Util::Min(*pBufferSize, pBinary->binaryByteSize);

                    memcpy(pBuffer, pBinary->pBinary, copySize);

                    result = (copySize == pBinary->binaryByteSize) ? VK_SUCCESS : VK_INCOMPLETE;
                }
                else
                {
                    *pBufferSize = pBinary->binaryByteSize;

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
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutablePropertiesKHR(
    VkDevice                                    device,
    const VkPipelineInfoKHR*                    pPipelineInfo,
    uint32_t*                                   pExecutableCount,
    VkPipelineExecutablePropertiesKHR*          pProperties)
{
    const Pipeline*                     pPipeline     = Pipeline::ObjectFromHandle(pPipelineInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

    uint32_t numStages = 0;
    for (uint32 i = 0; i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count); i++)
    {
        numStages += (apiToHwShader.apiShaders[i] != 0) ? 1 : 0;
    }

    if (pProperties == nullptr)
    {
        *pExecutableCount = numStages;
        return VK_SUCCESS;
    }

    uint32_t outputCount = 0;
    for (uint32 i = 0;
         ((i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count)) && (outputCount < *pExecutableCount));
         i++)
    {
        if (apiToHwShader.apiShaders[i] != 0)
        {
            VkShaderStatisticsInfoAMD  vkShaderStats = {};
            Pal::ShaderStats           palStats      = {};

            Pal::Result palResult = pPalPipeline->GetShaderStats(
                static_cast<Pal::ShaderType>(i),
                &palStats,
                true);

            ConvertShaderInfoStatistics(palStats, &vkShaderStats);

            // API String
            const char* apiString = ApiShaderTypeStrings[static_cast<uint32_t>(i)];
            strncpy(pProperties[outputCount].name,        apiString, VK_MAX_DESCRIPTION_SIZE);
            strncpy(pProperties[outputCount].description, apiString, VK_MAX_DESCRIPTION_SIZE);

            // Set VkShaderStageFlagBits
            pProperties[outputCount].stages = vkShaderStats.shaderStageMask;

            // Add subgroup size for Compute
            if (vkShaderStats.shaderStageMask & VK_SHADER_STAGE_COMPUTE_BIT)
            {
                pProperties[outputCount].subgroupSize = vkShaderStats.computeWorkGroupSize[0] *
                                                        vkShaderStats.computeWorkGroupSize[1] *
                                                        vkShaderStats.computeWorkGroupSize[2];
            }

            outputCount++;
        }
    }

    // Write out the number of stages written
    *pExecutableCount = outputCount;

    return (*pExecutableCount < numStages) ? VK_INCOMPLETE : VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableStatisticsKHR(
    VkDevice                                    device,
    const VkPipelineExecutableInfoKHR*          pExecutableInfo,
    uint32_t*                                   pStatisticCount,
    VkPipelineExecutableStatisticKHR*           pStatistics)
{
    const Pipeline*                     pPipeline     = Pipeline::ObjectFromHandle(pExecutableInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

    if (pStatistics == nullptr)
    {
        // Returning to statics value per shader executable
        *pStatisticCount = ExecutableStatisticsCount;
        return VK_SUCCESS;
    }

    uint32_t index = 0;
    uint32_t apiStages[static_cast<uint32_t>(Util::Abi::ApiShaderType::Count)] = {};
    for (uint32 i = 0; i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count); i++)
    {
        if (apiToHwShader.apiShaders[i] != 0)
        {
            apiStages[index] = i;
            index++;
        }
    }

    VkShaderStatisticsInfoAMD  vkShaderStats = {};
    Pal::ShaderStats           palStats      = {};

    Pal::Result palResult = pPalPipeline->GetShaderStats(
        static_cast<Pal::ShaderType>(apiStages[pExecutableInfo->executableIndex]),
        &palStats,
        true);

    if (palResult != Pal::Result::Success)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    ConvertShaderInfoStatistics(palStats, &vkShaderStats);

    VkPipelineExecutableStatisticKHR executableStatics[ExecutableStatisticsCount] =
    {
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "numUsedVgprs",
         "Number of used VGPRs", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, false},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "numUsedSgprs",
         "Number of used SGPRs", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, false},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "ldsSizePerLocalWorkGroup",
         "LDS size per local workgroup", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, false},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "ldsUsageSizeInBytes",
         "LDS usage size in Bytes", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, false},
        {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, 0, "scratchMemUsageInBytes",
         "Scratch memory usage in Bytes", VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR, false}
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
    const Pipeline*                     pPipeline     = Pipeline::ObjectFromHandle(pExecutableInfo->pipeline);
    const Pal::IPipeline*               pPalPipeline  = pPipeline->PalPipeline(DefaultDeviceIndex);
    const Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

    if (pInternalRepresentations == nullptr)
    {
        *pInternalRepresentationCount = 1;
        return VK_SUCCESS;
    }

    if (*pInternalRepresentationCount == 0)
    {
        return VK_INCOMPLETE;
    }

    uint32_t index = 0;
    uint32_t apiStages[static_cast<uint32_t>(Util::Abi::ApiShaderType::Count)] = {};
    for (uint32 i = 0; i < static_cast<uint32_t>(Util::Abi::ApiShaderType::Count); i++)
    {
        if (apiToHwShader.apiShaders[i] != 0)
        {
            apiStages[index] = i;
            index++;
        }
    }

    // Get the ABI HW Shader
    uint32_t vkStage = apiStages[pExecutableInfo->executableIndex];

    // API String
    const char* apiString = ApiShaderTypeStrings[static_cast<uint32_t>(vkStage)];
    strncpy(pInternalRepresentations[0].name,        apiString, VK_MAX_DESCRIPTION_SIZE);
    strncpy(pInternalRepresentations[0].description, apiString, VK_MAX_DESCRIPTION_SIZE);
    pInternalRepresentations[0].isText = VK_TRUE;

    VkResult result = pPipeline->GetShaderDisassembly(
        pDevice,
        pPalPipeline,
        static_cast<Pal::ShaderType>(vkStage),
        &(pInternalRepresentations[0].dataSize),
        pInternalRepresentations[0].pData);

    // Update the number of representations written
    *pInternalRepresentationCount = 1;

    return result;
}

} // namespace entry

} // namespace vk
