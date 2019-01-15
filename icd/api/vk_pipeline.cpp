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

namespace vk
{

// =====================================================================================================================
Pipeline::Pipeline(
    Device* const         pDevice,
    Pal::IPipeline**      pPalPipeline,
    const PipelineLayout* pLayout,
    PipelineBinaryInfo*     pBinary)
    :
    m_pDevice(pDevice),
    m_UserDataLayout(pLayout->GetInfo().userDataLayout),
    m_pBinary(pBinary)
{
    memset(m_pPalPipeline, 0, sizeof(m_pPalPipeline));
    memset(m_palPipelineHash, 0, sizeof(m_palPipelineHash));

    for (uint32_t devIdx = 0; devIdx < pDevice->NumPalDevices(); devIdx++)
    {
        m_pPalPipeline[devIdx]      = pPalPipeline[devIdx];
        m_palPipelineHash[devIdx]   = pPalPipeline[devIdx]->GetInfo().pipelineHash;
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

    const Device*   pDevice   = ApiDevice::ObjectFromHandle(device);
    const Pipeline* pPipeline = Pipeline::ObjectFromHandle(pipeline);
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
            // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
            // instructions by examining the symbol table entry for that shader's entrypoint.
            Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());

            const PipelineBinaryInfo* pPipelineBinary = pPipeline->GetBinary();

            Pal::Result palResult = abiProcessor.LoadFromBuffer(pPipelineBinary->pBinary, pPipelineBinary->binaryByteSize);

            if (palResult == Pal::Result::Success)
            {
                bool symbolValid  = false;
                Util::Abi::ApiHwShaderMapping apiToHwShader = pPalPipeline->ApiHwShaderMapping();

                static_assert(((static_cast<uint32_t>(Util::Abi::ApiShaderType::Cs)    == static_cast<uint32_t>(Pal::ShaderType::Compute))  &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Vs)    == static_cast<uint32_t>(Pal::ShaderType::Vertex))   &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Hs)    == static_cast<uint32_t>(Pal::ShaderType::Hull))     &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Ds)    == static_cast<uint32_t>(Pal::ShaderType::Domain))   &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Gs)    == static_cast<uint32_t>(Pal::ShaderType::Geometry)) &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Ps)    == static_cast<uint32_t>(Pal::ShaderType::Pixel))    &&
                               (static_cast<uint32_t>(Util::Abi::ApiShaderType::Count) == Pal::NumShaderTypes)),
                               "Util::Abi::ApiShaderType to Pal::ShaderType mapping does not match!");

                uint32_t hwStage = 0;
                if (Util::BitMaskScanForward(&hwStage, apiToHwShader.apiShaders[static_cast<uint32_t>(shaderType)]))
                {
                    Util::Abi::PipelineSymbolEntry symbol = {};
                    const void* pDisassemblySection   = nullptr;
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

} // namespace entry

} // namespace vk
