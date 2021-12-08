/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
    ".reserved", ///< @internal Reserved for future features.  Do not use!
    ".vs",
    ".hs",
    ".ds",
    ".gs",
    ".reserved", ///< @internal Reserved for future features.  Do not use!
    ".ps"
};

static const Pal::ShaderStageFlagBits IndexPalShaderStages[] =
{
    Pal::ApiShaderStageCompute,
    static_cast<Pal::ShaderStageFlagBits>(0x0), ///< @internal Reserved for future features.  Do not use!
    Pal::ApiShaderStageVertex,
    Pal::ApiShaderStageHull,
    Pal::ApiShaderStageDomain,
    Pal::ApiShaderStageGeometry,
    static_cast<Pal::ShaderStageFlagBits>(0x0), ///< @internal Reserved for future features.  Do not use!
    Pal::ApiShaderStagePixel
};

static constexpr char shaderPreNameIntermediate[] = "Intermediate";
static constexpr char shaderPreNameISA[]          = "ISA";

static_assert(VK_ARRAY_SIZE(HwStageNames) == static_cast<uint32_t>(Util::Abi::HardwareStage::Count),
    "Number of HwStageNames and PAL HW stages should match.");

// The number of executable statistics to return through
// the vkGetPipelineExecutableStatisticsKHR function
static constexpr uint32_t ExecutableStatisticsCount = 5;

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
// Generates a hash using the contents of a VkPipelineShaderStageCreateInfo struct
void Pipeline::GenerateHashFromShaderStageCreateInfo(
    const VkPipelineShaderStageCreateInfo& desc,
    Util::MetroHash128*                    pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.stage);
    if (desc.module != VK_NULL_HANDLE)
    {
        pHasher->Update(ShaderModule::ObjectFromHandle(desc.module)->GetCodeHash(desc.pName));
    }

    if (desc.pSpecializationInfo != nullptr)
    {
        GenerateHashFromSpecializationInfo(*desc.pSpecializationInfo, pHasher);
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
    const bool                             duplicateExistingModules,
    uint32_t                               (*pfnGetOutputIdx)(const uint32_t inputIdx,
                                                              const uint32_t stageIdx),
    ShaderStageInfo*                       pShaderStageInfo,
    ShaderModuleHandle*                    pTempModules)
{
    VkResult result = VK_SUCCESS;

    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    uint32_t numNewModules = 0;

    for (uint32_t i = 0; i < stageCount; ++i)
    {
        const VkPipelineShaderStageCreateInfo& stageInfo = pStages[i];
        const ShaderStage                      stage     = ShaderFlagBitToStage(stageInfo.stage);
        const uint32_t                         outIdx    = pfnGetOutputIdx(i, stage);

        if ((stageInfo.module != VK_NULL_HANDLE) && (duplicateExistingModules == false))
        {
            const ShaderModule* pModule = ShaderModule::ObjectFromHandle(stageInfo.module);

            pShaderStageInfo[outIdx].pModuleHandle = pModule->GetShaderModuleHandle();
            pShaderStageInfo[outIdx].codeHash      = pModule->GetCodeHash(stageInfo.pName);
            pShaderStageInfo[outIdx].codeSize      = pModule->GetCodeSize();
        }
        else
        {
            // Caller must make sure that pNewModules should be non-null if shader module may be deprecated.
            // Meanwhile, the memory pointed by pNewModules should be initialized to 0 and can take all the
            // newly-built modules in the pipeline (at most stageCount entries required).
            // Caller should release the newly-built temporary modules set in pNewModules manually after
            // creation of pipeline.
            VK_ASSERT(pTempModules != nullptr);

            VkShaderModuleCreateFlags flags    = 0;
            size_t                    codeSize = 0;
            const void*               pCode    = nullptr;

            if (stageInfo.module != VK_NULL_HANDLE)
            {
                // TODO: It's better to copy the compiled shader modules rather than compile them again.
                const ShaderModule* pModule = ShaderModule::ObjectFromHandle(stageInfo.module);
                codeSize = pModule->GetCodeSize();
                pCode    = pModule->GetCode();
            }
            else
            {
                EXTRACT_VK_STRUCTURES_0(
                    shaderModule,
                    ShaderModuleCreateInfo,
                    static_cast<const VkShaderModuleCreateInfo*>(stageInfo.pNext),
                    SHADER_MODULE_CREATE_INFO);

                VK_ASSERT(pShaderModuleCreateInfo != nullptr);

                flags    = pShaderModuleCreateInfo->flags;
                codeSize = pShaderModuleCreateInfo->codeSize;
                pCode    = pShaderModuleCreateInfo->pCode;
            }

            const Pal::ShaderHash codeHash = ShaderModule::BuildCodeHash(pCode, codeSize);

            result = pCompiler->BuildShaderModule(pDevice, flags, codeSize, pCode, &pTempModules[numNewModules]);

            if (result != VK_SUCCESS)
            {
                break;
            }

            pShaderStageInfo[outIdx].pModuleHandle = &pTempModules[numNewModules++];
            pShaderStageInfo[outIdx].codeHash      = ShaderModule::GetCodeHash(codeHash, stageInfo.pName);
            pShaderStageInfo[outIdx].codeSize      = codeSize;
        }

        pShaderStageInfo[outIdx].stage               = stage;
        pShaderStageInfo[outIdx].pEntryPoint         = stageInfo.pName;
        pShaderStageInfo[outIdx].flags               = stageInfo.flags;
        pShaderStageInfo[outIdx].pSpecializationInfo = stageInfo.pSpecializationInfo;

    }

    if (result != VK_SUCCESS)
    {
        for (uint32_t i = 0; i < numNewModules; ++i)
        {
            pCompiler->FreeShaderModule(&pTempModules[i]);
        }
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
            else
            {
                break;
            }
        }
    }
}

// =====================================================================================================================
Pipeline::Pipeline(
    Device* const       pDevice,
    VkPipelineBindPoint type)
    :
    m_pDevice(pDevice),
    m_userDataLayout(),
    m_palPipelineHash(0),
    m_staticStateMask(0),
    m_apiHash(0),
    m_type(type),
    m_pBinary(nullptr)
{
    memset(m_pPalPipeline, 0, sizeof(m_pPalPipeline));
}

void Pipeline::Init(
    Pal::IPipeline**      pPalPipeline,
    const PipelineLayout* pLayout,
    PipelineBinaryInfo*   pBinary,
    uint32_t              staticStateMask,
    uint64_t              apiHash)
{
    m_staticStateMask = staticStateMask;
    m_apiHash = apiHash;
    m_pBinary = pBinary;

    if (pLayout != nullptr)
    {
        m_userDataLayout = pLayout->GetInfo().userDataLayout;
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
Pipeline::~Pipeline()
{
    // Destroy PAL object
    for (uint32_t deviceIdx = 0; (deviceIdx < m_pDevice->NumPalDevices()) && (m_pPalPipeline[deviceIdx] != nullptr); deviceIdx++)
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
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
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
    const PipelineBinaryInfo* pPipelineBinary = GetBinary();

    if (pPipelineBinary == nullptr)
    {
        // The pipelineBinary will be null if the pipeline wasn't created with
        // VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR
        VK_NEVER_CALLED();

        return VK_ERROR_UNKNOWN;
    }

    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    Util::Abi::PipelineAbiReader abiReader(pDevice->VkInstance()->Allocator(), pPipelineBinary->pBinary);

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
            const Util::Elf::SymbolTableEntry* pSymbolEntry = nullptr;
            const char* pSectionName = nullptr;

            if (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderDisassembly)
            {
                pSymbolEntry = abiReader.GetPipelineSymbol(
                    Util::Abi::GetSymbolForStage(
                        Util::Abi::PipelineSymbolType::ShaderDisassembly,
                        static_cast<Util::Abi::HardwareStage>(hwStage)));
                pSectionName = Util::Abi::AmdGpuDisassemblyName;
            }
            else if (pipelineSymbolType == Util::Abi::PipelineSymbolType::ShaderAmdIl)
            {
                pSymbolEntry = abiReader.GetPipelineSymbol(
                    Util::Abi::GetSymbolForStage(
                        Util::Abi::PipelineSymbolType::ShaderAmdIl,
                        apiShaderType));
                pSectionName = Util::Abi::AmdGpuCommentLlvmIrName;
            }

            if (pSymbolEntry != nullptr)
            {
                palResult = abiReader.GetElfReader().CopySymbol(*pSymbolEntry, pBufferSize, pBuffer);
                symbolValid = palResult == Util::Result::Success;
            }
            else if (pSectionName != nullptr)
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
                Util::Abi::PipelineSymbolType::ShaderDisassembly,
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
    uint32_t numberOfInternalRepresentations = Util::CountSetBits(palStats.shaderStageMask) + 1;

    if (pInternalRepresentations == nullptr)
    {
        *pInternalRepresentationCount = numberOfInternalRepresentations;
        return VK_SUCCESS;
    }

    // Output the Intermediate API Shaders
    uint32_t outputCount   = 0;
    uint32_t apiShaderMask = palStats.shaderStageMask;

    uint32_t i = 0;
    while((Util::BitMaskScanForward(&i, apiShaderMask)) &&
          (outputCount < *pInternalRepresentationCount))
    {
         // Build the name and description of the output property for IL
         const char*              pApiString    = ApiStageNames[i];
         Pal::ShaderStageFlagBits palShaderMask = IndexPalShaderStages[i];

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
    if (outputCount < *pInternalRepresentationCount)
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
        pInternalRepresentations[outputCount].isText = VK_TRUE;

        outputCount++;
    }

    // Write out the number of shader ouputs.
    *pInternalRepresentationCount = outputCount;

    // If the requested number of executables was less than the available number of hw stages, return Incomplete
    return (*pInternalRepresentationCount < numberOfInternalRepresentations) ? VK_INCOMPLETE : VK_SUCCESS;
}

} // namespace entry

} // namespace vk
