/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPipeline.h"
#include "palShaderCache.h"
#include "palAutoBuffer.h"

// Temporary includes to support legacy path ELF building
#include "palPipelineAbi.h"
#include "palElfProcessorImpl.h"

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
    m_pLayout(pLayout),
    m_pBinary(pBinary)
{
    memset(m_pPalPipeline, 0, sizeof(m_pPalPipeline));
    memcpy(m_pPalPipeline, pPalPipeline, sizeof(pPalPipeline[0]) * pDevice->NumPalDevices());
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

    const Pipeline* pPipeline = Pipeline::ObjectFromHandle(pipeline);
    const Pal::IPipeline* pPalPipeline = pPipeline->PalPipeline();

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

                    ApiDevice::ObjectFromHandle(device)->VkPhysicalDevice()->PalDevice()->GetProperties(&info);

                    pStats->numPhysicalVgprs = info.gfxipProperties.shaderCore.vgprsPerSimd;
                    pStats->numPhysicalSgprs = info.gfxipProperties.shaderCore.sgprsPerSimd;
                }

                result = VK_SUCCESS;
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

// =====================================================================================================================
// This is a temporary function to infer a mockup of a PAL ABI ELF binary out of a previously-created PAL Pipeline
// object.
//
// !!! This function only necessary until LLPC/SCPC compiler interfaces are in place that can produce full ELF       !!!
// !!! binaries.                                                                                                     !!!
//
// This binary is not usable to create new PAL IPipelines but it should contain just enough information that
// an external tool which queries it via VK_AMD_shader_info can feed it to an external disassembler object.
void Pipeline::CreateLegacyPathElfBinary(
    Device*         pDevice,
    bool            graphicsPipeline,
    Pal::IPipeline* pPalPipeline,
    size_t*         pPipelineBinarySize,
    void**          ppPipelineBinary)
{
    Pal::Result result = Pal::Result::Success;

    using namespace Util::Elf;

    const Pal::DeviceProperties& deviceProps = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    // Query shader code for each shader stage (not all of these are always available)
    constexpr uint32_t ShaderTypeCount = 6;

    PalAllocator* pAllocator             = pDevice->VkInstance()->Allocator();
    size_t shaderSizes[ShaderTypeCount]  = {};
    size_t entryOffsets[ShaderTypeCount] = {};
    size_t textSize                      = 0;
    void*  pTextData                     = nullptr;
    uint32_t symbolCount                 = 0;

    // Calculate the sizes of each shader stage and the "entry point offsets" within the .text section.
    for (uint32_t stage = 0; stage < ShaderTypeCount; ++stage)
    {
        Pal::ShaderType shaderType = static_cast<Pal::ShaderType>(stage);

        pPalPipeline->GetShaderCode(shaderType, &shaderSizes[stage], nullptr);

        if (shaderSizes[stage] != 0)
        {
            size_t alignedOffset = Util::Pow2Align(textSize, 256);

            entryOffsets[stage] = alignedOffset;

            textSize = entryOffsets[stage] + shaderSizes[stage];

            symbolCount++;
        }
    }

    // Allocate memory for text section
    if (textSize > 0)
    {
        pTextData = pDevice->VkInstance()->AllocMem(
            textSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
    }

    // Extract the compiled code for each stage
    if (pTextData != nullptr)
    {
        memset(pTextData, 0, textSize);

        for (uint32_t stage = 0; stage < ShaderTypeCount; ++stage)
        {
            if (shaderSizes[stage] != 0)
            {
                pPalPipeline->GetShaderCode(
                    static_cast<Pal::ShaderType>(stage),
                    &shaderSizes[stage],
                    Util::VoidPtrInc(pTextData, entryOffsets[stage]));
            }
        }
    }

    ElfProcessor<PalAllocator> elf(pAllocator);
    result = elf.Init();

    if (result == Pal::Result::Success)
    {
        // Set some random basic information
        elf.SetClass(ElfClass64);
        elf.SetEndianness(ElfLittleEndian);

        // Add the .text section
        Section<PalAllocator>* pTextSection = nullptr;

        if (pTextData != nullptr)
        {
            pTextSection = elf.GetSections()->Add(SectionType::Text);

            if (pTextSection != nullptr)
            {
                pTextSection->SetData(pTextData, textSize);
            }
        }

        // Add a symbol table section that just contains the entry point offsets
        if ((pTextSection != nullptr) && (symbolCount > 0))
        {
            auto* pStrTabSection = elf.GetSections()->Add(SectionType::StrTab);
            auto* pSymTabSection = elf.GetSections()->Add(SectionType::SymTab);

            if ((pStrTabSection != nullptr) && (pSymTabSection != nullptr))
            {
                pSymTabSection->SetLink(pStrTabSection);

                StringProcessor<PalAllocator> stringProcessor(pStrTabSection, pAllocator);
                SymbolProcessor<PalAllocator> symbolProcessor(pSymTabSection, &stringProcessor, pAllocator);

                for (uint32_t stage = 0; stage < ShaderTypeCount; ++stage)
                {
                    if (shaderSizes[stage] != 0)
                    {
                        const Pal::ShaderType shaderType = static_cast<Pal::ShaderType>(stage);

                        // This mapping from shader stage/type to PAL ABI pipeline symbol type is completely made-up and
                        // inaccurate, but it's the best we can do.
                        //
                        // PAL ABI dictates that the logical unit of code is the whole pipeline.  This makes sense in
                        // the context of newer gfxips where HW shader stages are merging.
                        //
                        // The entry points in the PAL ABI are defined in terms of HW shader stages of the current
                        // gfxip, and that information is lost by PAL's HW abstraction.  The real ELF created by the
                        // compiler interface has proper knowledge of all the HW stage entry point offsets.
                        //
                        // It is probably correct for CS and VS+PS cases, but it'll most likely be wrong when GS/TCS/TES
                        // is involved, and also with NGG.
                        Util::Abi::PipelineSymbolType symbolType = Util::Abi::PipelineSymbolType::Unknown;

                        switch (shaderType)
                        {
                        case Pal::ShaderType::Compute:
                            symbolType = Util::Abi::PipelineSymbolType::CsMainEntry;
                            break;
                        case Pal::ShaderType::Vertex:
                            symbolType = Util::Abi::PipelineSymbolType::VsMainEntry;
                            break;
                        case Pal::ShaderType::Hull:
                            symbolType = Util::Abi::PipelineSymbolType::HsMainEntry;
                            break;
                        case Pal::ShaderType::Domain:
                            symbolType = Util::Abi::PipelineSymbolType::EsMainEntry;
                            break;
                        case Pal::ShaderType::Geometry:
                            symbolType = Util::Abi::PipelineSymbolType::GsMainEntry;
                            break;
                        case Pal::ShaderType::Pixel:
                            symbolType = Util::Abi::PipelineSymbolType::PsMainEntry;
                            break;
                        }

                        if (symbolType != Util::Abi::PipelineSymbolType::Unknown)
                        {
                            symbolProcessor.Add(
                                Util::Abi::PipelineAbiSymbolNameStrings[static_cast<size_t>(symbolType)],
                                SymbolTableEntryBinding::Local,
                                SymbolTableEntryType::Func,
                                pTextSection->GetIndex(),
                                entryOffsets[stage],
                                shaderSizes[stage]);
                        }
                    }
                }
            }
        }

        auto* pNoteSection = elf.GetSections()->Add(SectionType::Note);

        if (pNoteSection != nullptr)
        {
            // Add a .note identifying the GPU IP version.  This code is basically ripped from the LLPC ELF generation
            NoteProcessor<PalAllocator> noteProcessor(pNoteSection, pAllocator);

            Util::Abi::AbiAmdGpuVersionNote gpuVersionNote = {};

            switch (deviceProps.gfxLevel)
            {
            case Pal::GfxIpLevel::GfxIp6:
                gpuVersionNote.gfxipMajorVer = 6;
                gpuVersionNote.gfxipMinorVer = 0;
                break;
            case Pal::GfxIpLevel::GfxIp7:
                gpuVersionNote.gfxipMajorVer = 7;
                gpuVersionNote.gfxipMinorVer = 0;
                break;
            case Pal::GfxIpLevel::GfxIp8:
                gpuVersionNote.gfxipMajorVer = 8;
                gpuVersionNote.gfxipMinorVer = 0;
                break;
            case Pal::GfxIpLevel::GfxIp8_1:
                gpuVersionNote.gfxipMajorVer = 8;
                gpuVersionNote.gfxipMinorVer = 1;
                break;
            case Pal::GfxIpLevel::GfxIp9:
                gpuVersionNote.gfxipMajorVer = 9;
                gpuVersionNote.gfxipMinorVer = 0;
                break;
            default:
                VK_NEVER_CALLED();
                break;
            }

            gpuVersionNote.gfxipStepping  = deviceProps.gfxStepping;

            gpuVersionNote.vendorNameSize = sizeof(Util::Abi::AmdGpuVendorName);
            gpuVersionNote.archNameSize   = sizeof(Util::Abi::AmdGpuArchName);

            memcpy(gpuVersionNote.vendorName, Util::Abi::AmdGpuVendorName, sizeof(Util::Abi::AmdGpuVendorName));
            memcpy(gpuVersionNote.archName,   Util::Abi::AmdGpuArchName, sizeof(Util::Abi::AmdGpuArchName));

            // The empty spaces in the note strings here are because of a bug in the PAL ELF writer's alignment code.
            // We really want to send empty strings, which translates to a 4 byte padded string, but they apply padding
            // twice which hits an assert inside their code.

            noteProcessor.Add(
                static_cast<uint32_t>(Util::Abi::PipelineAbiNoteType::HsaIsa),
                "   ",
                &gpuVersionNote,
                sizeof(gpuVersionNote));

            // Add a .note identifying PAL version information.  Also ripped from the LLPC code.
            Util::Abi::AbiMinorVersionNote ntAbiMinorVersion = {};

            ntAbiMinorVersion.minorVersion = Util::Abi::ElfAbiMinorVersion;

            noteProcessor.Add(
                static_cast<uint32_t>(Util::Abi::PipelineAbiNoteType::AbiMinorVersion),
                "   ",
                &ntAbiMinorVersion,
                sizeof(ntAbiMinorVersion));
        }

        elf.Finalize();

        const size_t elfSize = elf.GetRequiredBufferSizeBytes();

        if (elfSize > 0)
        {
            void* pElf = pDevice->VkInstance()->AllocMem(
                elfSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pElf != nullptr)
            {
                elf.SaveToBuffer(pElf);

                *ppPipelineBinary = pElf;
                *pPipelineBinarySize = elfSize;
            }
        }
    }

    if (pTextData != nullptr)
    {
        pDevice->VkInstance()->FreeMem(pTextData);
    }
}

} // namespace vk
