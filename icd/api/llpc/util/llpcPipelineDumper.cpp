/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcPipelineDumper.cpp
 * @breif LLPC source file: contains implementation of LLPC pipline dump utility.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-pipeline-dumper"

#include "llvm/Support/Mutex.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcPipelineDumper.h"
#include "llpcMetroHash.h"
#include "llpcGfx6Chip.h"
#include "llpcGfx9Chip.h"
#include "llpcCompiler.h"

using namespace llvm;

namespace Llpc
{

// Forward declaration
std::ostream& operator<<(std::ostream& out, VkVertexInputRate  inputRate);
std::ostream& operator<<(std::ostream& out, VkFormat           format);
std::ostream& operator<<(std::ostream& out, VkPrimitiveTopology topology);
std::ostream& operator<<(std::ostream& out, enum class ResourceMappingNodeType type);
template std::ostream& operator<<(std::ostream& out, ElfReader<Elf64>& reader);
template raw_ostream& operator<<(raw_ostream& out, ElfReader<Elf64>& reader);

// Mutex for pipeline dump
static ManagedStatic<sys::Mutex> s_dumpMutex;

// =====================================================================================================================
// Gets the file name of SPIR-V binary according the specified shader hash.
std::string PipelineDumper::GetSpirvBinaryFileName(
     const MetroHash::Hash* pHash)       // [in] Shader hash code
{
    uint64_t hashCode64 = MetroHash::Compact64(pHash);
    char     fileName[64] = {};
    auto     length = snprintf(fileName, 64, "Shader_0x%016" PRIX64 ".spv", hashCode64);
    return std::string(fileName);
}

// =====================================================================================================================
// Gets the file name of pipeline info file according to the specified pipeline build info and pipeline hash.
std::string PipelineDumper::GetPipelineInfoFileName(
    const ComputePipelineBuildInfo*  pComputePipelineInfo,   // [in] Info of the compute pipeline to be built
    const GraphicsPipelineBuildInfo* pGraphicsPipelineInfo,  // [in] Info of the graphics pipeline to be built
    const MetroHash::Hash*           pHash)                  // [in] Pipeline hash code
{
    uint64_t        hashCode64 = MetroHash::Compact64(pHash);
    char            fileName[64] = {};
    if (pComputePipelineInfo != nullptr)
    {
        auto length = snprintf(fileName, 64, "PipelineCs_0x%016" PRIX64, hashCode64);
    }
    else
    {
        LLPC_ASSERT(pGraphicsPipelineInfo != nullptr);
        const char* pFileNamePrefix = nullptr;
        if (pGraphicsPipelineInfo->tes.pModuleData != nullptr && pGraphicsPipelineInfo->gs.pModuleData != nullptr)
        {
             pFileNamePrefix = "PipelineGsTess";
        }
        else if (pGraphicsPipelineInfo->gs.pModuleData != nullptr)
        {
             pFileNamePrefix = "PipelineGs";
        }
        else if (pGraphicsPipelineInfo->tes.pModuleData != nullptr)
        {
             pFileNamePrefix = "PipelineTess";
        }
        else
        {
            pFileNamePrefix = "PipelineVsFs";
        }

        auto length = snprintf(fileName, 64, "%s_0x%016" PRIX64, pFileNamePrefix, hashCode64);
    }

    return std::string(fileName);
}

// =====================================================================================================================
// Begins to dump graphics/compute pipeline info.
std::ofstream* PipelineDumper::BeginPipelineDump(
    const char*                      pDumpDir,               // [in] Directory of pipeline dump
    const ComputePipelineBuildInfo*  pComputePipelineInfo,   // [in] Info of the compute pipeline to be built
    const GraphicsPipelineBuildInfo* pGraphicsPipelineInfo,  // [in] Info of the graphics pipeline to be built
    const MetroHash::Hash*           pHash)                  // [in] Pipeline hash code
{
    std::error_code errCode;

    std::string dumpFileName = pDumpDir;
    dumpFileName += "/";
    dumpFileName += GetPipelineInfoFileName(pComputePipelineInfo, pGraphicsPipelineInfo, pHash);
    dumpFileName += ".pipe";

    // Open dump file
    s_dumpMutex->lock();
    auto pDumpFile = new std::ofstream(dumpFileName.c_str());
    if (pDumpFile->bad())
    {
        delete pDumpFile;
        pDumpFile = nullptr;
        s_dumpMutex->unlock();
    }

    // Dump pipeline input info
    if (pComputePipelineInfo)
    {
        DumpComputePipelineInfo(pDumpFile, pComputePipelineInfo);
    }

    if (pGraphicsPipelineInfo)
    {
        DumpGraphicsPipelineInfo(pDumpFile, pGraphicsPipelineInfo);
    }
    return pDumpFile;
}

// =====================================================================================================================
// Ends to dump graphics/compute pipeline info.
void PipelineDumper::EndPipelineDump(
    std::ofstream* pDumpFile) // [in] Dump file
{
    delete pDumpFile;
    s_dumpMutex->unlock();
}

// =====================================================================================================================
// Dumps resource mapping node to dumpFile.
void PipelineDumper::DumpResourceMappingNode(
    const ResourceMappingNode* pUserDataNode,    // [in] User data nodes to be dumped
    const char*                pPrefix,          // [in] Prefix string for each line
    std::ostream&              dumpFile)         // [out] dump file
{
    dumpFile << pPrefix << ".type = " << pUserDataNode->type << "\n";
    dumpFile << pPrefix << ".offsetInDwords = " << pUserDataNode->offsetInDwords << "\n";
    dumpFile << pPrefix << ".sizeInDwords = " << pUserDataNode->sizeInDwords << "\n";

    switch (pUserDataNode->type)
    {
    case ResourceMappingNodeType::DescriptorResource:
    case ResourceMappingNodeType::DescriptorSampler:
    case ResourceMappingNodeType::DescriptorCombinedTexture:
    case ResourceMappingNodeType::DescriptorTexelBuffer:
    case ResourceMappingNodeType::DescriptorBuffer:
    case ResourceMappingNodeType::DescriptorFmask:
    case ResourceMappingNodeType::DescriptorBufferCompact:
        {
            dumpFile << pPrefix << ".set = " << pUserDataNode->srdRange.set << "\n";
            dumpFile << pPrefix << ".binding = " << pUserDataNode->srdRange.binding << "\n";
            break;
        }
    case ResourceMappingNodeType::DescriptorTableVaPtr:
        {
            char prefixBuf[256];
            int32_t length = 0;
            for (uint32_t i = 0; i < pUserDataNode->tablePtr.nodeCount; ++i)
            {
                length = snprintf(prefixBuf, 256, "%s.next[%u]", pPrefix, i);
                DumpResourceMappingNode(pUserDataNode->tablePtr.pNext + i, prefixBuf, dumpFile);
            }
            break;
        }
    case ResourceMappingNodeType::IndirectUserDataVaPtr:
        {
            dumpFile << pPrefix << ".indirectUserDataCount = " << pUserDataNode->userDataPtr.sizeInDwords << "\n";
            break;
        }
    case ResourceMappingNodeType::PushConst:
        {
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Dumps pipeline shader info to file.
void PipelineDumper::DumpPipelineShaderInfo(
    ShaderStage               stage,       // Shader stage
    const PipelineShaderInfo* pShaderInfo, // [in] Shader info of specified shader stage
    std::ostream&             dumpFile)    // [out] dump file
{
    const ShaderModuleData* pModuleData = reinterpret_cast<const ShaderModuleData*>(pShaderInfo->pModuleData);

    // Output shader binary file
    dumpFile << "[" << GetShaderStageAbbreviation(stage) << "SpvFile]\n";
    dumpFile << "fileName = " << GetSpirvBinaryFileName(&pModuleData->hash) << "\n\n";

    dumpFile << "[" << GetShaderStageAbbreviation(stage) << "Info]\n";
    // Output entry point
    if (pShaderInfo->pEntryTarget != nullptr)
    {
         dumpFile << "entryPoint = " << pShaderInfo->pEntryTarget << "\n";
    }

    // Output specialize info
    if (pShaderInfo->pSpecializatonInfo)
    {
        auto pSpecializatonInfo = pShaderInfo->pSpecializatonInfo;
        for (uint32_t i = 0; i < pSpecializatonInfo->mapEntryCount; ++i)
        {
            dumpFile << "specConst.mapEntry[" << i << "].constantID = " << pSpecializatonInfo->pMapEntries[i].constantID << "\n";
            dumpFile << "specConst.mapEntry[" << i << "].offset = " << pSpecializatonInfo->pMapEntries[i].offset << "\n";
            dumpFile << "specConst.mapEntry[" << i << "].size = " << pSpecializatonInfo->pMapEntries[i].size << "\n";
        }
        const uint32_t* pData = reinterpret_cast<const uint32_t*>(pSpecializatonInfo->pData);
        for (uint32_t i = 0; i < pSpecializatonInfo->dataSize / sizeof(uint32_t); ++i)
        {
            if ((i % 8) == 0)
            {
                dumpFile << "specConst.uintData = ";
            }
            dumpFile << pData[i];
            if ((i % 8) == 7)
            {
                dumpFile << "\n";
            }
            else
            {
                dumpFile << ", ";
            }
        }
        dumpFile << "\n\n";
    }

    // Output descriptor range value
    if (pShaderInfo->descriptorRangeValueCount > 0)
    {
        for (uint32_t i = 0; i < pShaderInfo->descriptorRangeValueCount; ++i)
        {
            auto pDescriptorRangeValue = &pShaderInfo->pDescriptorRangeValues[i];
            dumpFile << "descriptorRangeValue[" << i << "].type = " << pDescriptorRangeValue->type << "\n";
            dumpFile << "descriptorRangeValue[" << i << "].set = " << pDescriptorRangeValue->set << "\n";
            dumpFile << "descriptorRangeValue[" << i << "].binding = " << pDescriptorRangeValue->binding << "\n";
            for (uint32_t j = 0; j < pDescriptorRangeValue->arraySize; ++j)
            {
                dumpFile << "descriptorRangeValue[" << i << "].uintData = ";
                const uint32_t DescriptorSizeInDw = 4;

                for (uint32_t k = 0; k < DescriptorSizeInDw -1; ++k)
                {
                     dumpFile << pDescriptorRangeValue->pValue[i] << ", ";
                }
                dumpFile << pDescriptorRangeValue->pValue[DescriptorSizeInDw - 1] << "\n";
            }
        }
        dumpFile << "\n\n";
    }

    // Output resource node mapping
    if (pShaderInfo->userDataNodeCount > 0)
    {
        char prefixBuff[64];
        for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
        {
            auto pUserDataNode = &pShaderInfo->pUserDataNodes[i];
            auto length = snprintf(prefixBuff, 64, "userDataNode[%u]", i);
            DumpResourceMappingNode(pUserDataNode, prefixBuff, dumpFile);
        }
        dumpFile << "\n\n";
    }
}

// =====================================================================================================================
// Dumps SPIR-V shader binary to extenal file
void PipelineDumper::DumpSpirvBinary(
    const char*                     pDumpDir,     // [in] Directory of pipeline dump
    const BinaryData*               pSpirvBin,    // [in] SPIR-V binary
    MetroHash::Hash*                pHash)        // [in] Pipeline hash code
{
    std::error_code errCode = {};

    std::string pathName = pDumpDir;
    pathName += "/";
    pathName += GetSpirvBinaryFileName(pHash);

    // Open dumpfile
    std::ofstream dumpFile(pathName.c_str(), std::ios_base::binary | std::ios_base::out);
    if (dumpFile.bad() == false)
    {
        dumpFile.write(reinterpret_cast<const char*>(pSpirvBin->pCode), pSpirvBin->codeSize);
    }
}

// =====================================================================================================================
// Disassembles pipeline binary and dumps it to pipeline info file.
void PipelineDumper::DumpPipelineBinary(
    std::ostream*                    pDumpFile,              // [in] Directory of pipeline dump
    GfxIpVersion                     gfxIp,                  // Graphics IP version info
    const BinaryData*                pPipelineBin)           // [in] Pipeline binary (ELF)
{
    ElfReader<Elf64> reader(gfxIp);
    size_t codeSize = pPipelineBin->codeSize;
    auto result = reader.ReadFromBuffer(pPipelineBin->pCode, &codeSize);
    LLPC_ASSERT(result == Result::Success);

    *pDumpFile << "\n[CompileLog]\n";
    *pDumpFile << reader;
}

// =====================================================================================================================
// Dumps LLPC version info to file
void PipelineDumper::DumpVersionInfo(
    std::ostream&                  dumpFile)      // [out] dump file
{
    dumpFile << "[Version]\n";
    dumpFile << "version = " << Version << "\n\n";
}

// =====================================================================================================================
// Dumps compute pipeline state info to file.
void PipelineDumper::DumpComputeStateInfo(
    const ComputePipelineBuildInfo* pPipelineInfo,  // [in] Info of the graphics pipeline to be built
    std::ostream&                   dumpFile)      // [out] dump file
{
    dumpFile << "[ComputePipelineState]\n";

    // Output pipeline states
    dumpFile << "deviceIndex = " << pPipelineInfo->deviceIndex << "\n";
}

// =====================================================================================================================
// Dumps compute pipeline information to file.
void PipelineDumper::DumpComputePipelineInfo(
    std::ostream*                   pDumpFile,         // [in] Pipeline dump file
    const ComputePipelineBuildInfo* pPipelineInfo)     // [in] Info of the compute pipeline to be built
{
    DumpVersionInfo(*pDumpFile);

    // Output shader info
    DumpPipelineShaderInfo(ShaderStageCompute, &pPipelineInfo->cs, *pDumpFile);
    DumpComputeStateInfo(pPipelineInfo, *pDumpFile);

    pDumpFile->flush();
}

// =====================================================================================================================
// Dumps graphics pipeline state info to file.
void PipelineDumper::DumpGraphicsStateInfo(
    const GraphicsPipelineBuildInfo* pPipelineInfo, // [in] Info of the graphics pipeline to be built
    std::ostream&                    dumpFile)      // [out] dump file
{
    dumpFile << "[GraphicsPipelineState]\n";

    // Output pipeline states
    dumpFile << "topology = " << pPipelineInfo->iaState.topology << "\n";
    dumpFile << "patchControlPoints = " << pPipelineInfo->iaState.patchControlPoints << "\n";
    dumpFile << "deviceIndex = " << pPipelineInfo->iaState.deviceIndex << "\n";
    dumpFile << "disableVertexReuse = " << pPipelineInfo->iaState.disableVertexReuse << "\n";
    dumpFile << "switchWinding = " << pPipelineInfo->iaState.switchWinding << "\n";
    dumpFile << "depthClipEnable = " << pPipelineInfo->vpState.depthClipEnable << "\n";

    dumpFile << "rasterizerDiscardEnable = " << pPipelineInfo->rsState.rasterizerDiscardEnable << "\n";
    dumpFile << "perSampleShading = " << pPipelineInfo->rsState.perSampleShading << "\n";
    dumpFile << "numSamples = " << pPipelineInfo->rsState.numSamples << "\n";
    dumpFile << "samplePatternIdx = " << pPipelineInfo->rsState.samplePatternIdx << "\n";
    dumpFile << "usrClipPlaneMask = " << static_cast<uint32_t>(pPipelineInfo->rsState.usrClipPlaneMask) << "\n";

    dumpFile << "alphaToCoverageEnable = " << pPipelineInfo->cbState.alphaToCoverageEnable << "\n";
    dumpFile << "dualSourceBlendEnable = " << pPipelineInfo->cbState.dualSourceBlendEnable << "\n";

    for (uint32_t i = 0; i < MaxColorTargets; ++i)
    {
        if (pPipelineInfo->cbState.target[i].format != VK_FORMAT_UNDEFINED)
        {
            auto pCbTarget = &pPipelineInfo->cbState.target[i];
            dumpFile << "colorBuffer[" << i << "].format = " << pCbTarget->format << "\n";
            dumpFile << "colorBuffer[" << i << "].blendEnable = " << pCbTarget->blendEnable << "\n";
            dumpFile << "colorBuffer[" << i << "].blendSrcAlphaToColor = " << pCbTarget->blendSrcAlphaToColor << "\n";
        }
    }
    dumpFile << "\n\n";

    // Output vertex input state
    if (pPipelineInfo->pVertexInput &&
        (pPipelineInfo->pVertexInput->vertexBindingDescriptionCount > 0))
    {
        dumpFile << "[VertexInputState]\n";
        for (uint32_t i = 0; i < pPipelineInfo->pVertexInput->vertexBindingDescriptionCount; ++i)
        {
            auto pBinding = &pPipelineInfo->pVertexInput->pVertexBindingDescriptions[i];
            dumpFile << "binding[" << i << "].binding = " << pBinding->binding << "\n";
            dumpFile << "binding[" << i << "].stride = " << pBinding->stride << "\n";
            dumpFile << "binding[" << i << "].inputRate = " << pBinding->inputRate << "\n";
        }

        for (uint32_t i = 0; i < pPipelineInfo->pVertexInput->vertexAttributeDescriptionCount; ++i)
        {
            auto pAttrib = &pPipelineInfo->pVertexInput->pVertexAttributeDescriptions[i];
            dumpFile << "attribute[" << i << "].location = " << pAttrib->location << "\n";
            dumpFile << "attribute[" << i << "].binding = " << pAttrib->binding << "\n";
            dumpFile << "attribute[" << i << "].format = " << pAttrib->format << "\n";
            dumpFile << "attribute[" << i << "].offset = " << pAttrib->offset << "\n";
        }
    }
}

// =====================================================================================================================
// Dumps graphics pipeline build info to file.
void PipelineDumper::DumpGraphicsPipelineInfo(
    std::ostream*                    pDumpFile,       // [in] Pipeline dump file
    const GraphicsPipelineBuildInfo* pPipelineInfo)   // [in] Info of the graphics pipeline to be built
{
    DumpVersionInfo(*pDumpFile);
    // Dump pipeline
    const PipelineShaderInfo* shaderInfo[ShaderStageGfxCount] =
    {
        &pPipelineInfo->vs,
        &pPipelineInfo->tcs,
        &pPipelineInfo->tes,
        &pPipelineInfo->gs,
        &pPipelineInfo->fs,
    };

    for (uint32_t stage = 0; stage < ShaderStageGfxCount; ++stage)
    {
        const PipelineShaderInfo* pShaderInfo = shaderInfo[stage];
        if (pShaderInfo->pModuleData == nullptr)
        {
            continue;
        }
        DumpPipelineShaderInfo(static_cast<ShaderStage>(stage), pShaderInfo, *pDumpFile);
    }

    DumpGraphicsStateInfo(pPipelineInfo, *pDumpFile);

    pDumpFile->flush();
}

// =====================================================================================================================
// Outputs text with specified range to output stream.
template <class OStream>
void OutputText(
    const uint8_t* pData,    // [in] Text data
    uint32_t       startPos, // Starting position
    uint32_t       endPos,   // End position
    OStream&       out)      // [out] Output stream
{
    if (endPos > startPos)
    {
        // NOTE: We have to replace last character with null terminator and restore it afterwards. Otherwise, the
        // text print will be incorrect.
        uint8_t lastChar = pData[endPos - 1];
        const_cast<uint8_t*>(pData)[endPos - 1] = '\0';

        // Output text
        const char* pText = reinterpret_cast<const char*>(pData + startPos);
        out << pText << lastChar << "\n";

        // Restore last character
        const_cast<uint8_t*>(pData)[endPos - 1] = lastChar;
    }
}

// =====================================================================================================================
// Outputs binary data with specified range to output stream.
template<class OStream>
void OutputBinary(
    const uint8_t* pData,     // [in] Binary data
    uint32_t       startPos,  // Starting position
    uint32_t       endPos,    // End position
    OStream&       out)       // [out] Output stream
{
    const uint32_t* pStartPos = reinterpret_cast<const uint32_t*>(pData + startPos);
    int32_t dwordCount = (endPos - startPos) / sizeof(uint32_t);
    char formatBuf[256];
    for (int32_t i = 0; i < dwordCount; ++i)
    {
        if (i % 8 == 0)
        {
            out << "        ";
        }
        auto length = snprintf(formatBuf, sizeof(formatBuf), "%08X", pStartPos[i]);
        out << formatBuf;

        if (i % 8 == 7)
        {
            out << "\n";
        }
        else
        {
            out << " ";
        }
    }

    if ((endPos > startPos) && (endPos - startPos) % sizeof(uint32_t))
    {
        int32_t padPos = dwordCount * sizeof(uint32_t);
        for (int32_t i = padPos; i < endPos; ++i)
        {
            auto length = snprintf(formatBuf, sizeof(formatBuf), "%02X", pData[i]);
            out << formatBuf;
        }
    }

    if ((dwordCount % 8) != 0)
    {
        out << "\n";
    }
}

// =====================================================================================================================
//  Dumps ELF package to out stream
template<class OStream, class Elf>
OStream& operator<<(
    OStream&          out,      // [out] Output stream
    ElfReader<Elf>&   reader)   // [in] ELF object
{
    GfxIpVersion gfxIp = reader.GetGfxIpVersion();

    uint32_t sectionCount = reader.GetSectionCount();
    char formatBuf[256];

    for (uint32_t secIdx = 0; secIdx < sectionCount; ++secIdx)
    {
        typename ElfReader<Elf>::ElfSectionBuffer* pSection = nullptr;
        Result result = reader.GetSectionDataBySectionIndex(secIdx, &pSection);
        LLPC_ASSERT(result == Result::Success);
        if ((strcmp(pSection->pName, ShStrTabName) == 0) ||
            (strcmp(pSection->pName, StrTabName) == 0) ||
            (strcmp(pSection->pName, SymTabName) == 0))
        {
            // Output system section
            out << pSection->pName << " (size = " << pSection->secHead.sh_size << " bytes)\n";
        }
        else if (strcmp(pSection->pName, NoteName) == 0)
        {
            // Output .note section
            out << pSection->pName << " (size = " << pSection->secHead.sh_size << " bytes)\n";
            uint32_t offset = 0;
            const uint32_t noteHeaderSize = sizeof(NoteHeader);
            while (offset < pSection->secHead.sh_size)
            {
                const NoteHeader* pNode = reinterpret_cast<const NoteHeader*>(pSection->pData + offset);
                switch (pNode->type)
                {
                case Util::Abi::PipelineAbiNoteType::HsaIsa:
                    {
                        out << "    HsaIsa                       (name = "
                            << pNode->name << "  size = "<< pNode->descSize << ")\n";

                        auto pGpu = reinterpret_cast<const Util::Abi::AbiAmdGpuVersionNote*>(
                            pSection->pData + offset + noteHeaderSize);

                        out << "        vendorName  = " << pGpu->vendorName << "\n";
                        out << "        archName    = " << pGpu->archName << "\n";
                        out << "        gfxIp       = " << pGpu->gfxipMajorVer << "."
                                                        << pGpu->gfxipMinorVer << "."
                                                        << pGpu->gfxipStepping << "\n";
                        break;
                    }
                case Util::Abi::PipelineAbiNoteType::AbiMinorVersion:
                    {
                        out << "    AbiMinorVersion              (name = "
                            << pNode->name << "  size = " << pNode->descSize << ")\n";

                        auto pCodeVersion = reinterpret_cast<const Util::Abi::AbiMinorVersionNote *>(
                            pSection->pData + offset + noteHeaderSize);
                        out << "        minor = " << pCodeVersion->minorVersion << "\n";
                        break;
                    }
                case Util::Abi::PipelineAbiNoteType::PalMetadata:
                    {
                        out << "    PalMetadata                  (name = "
                            << pNode->name << "  size = " << pNode->descSize << ")\n";

                        const uint32_t configCount = pNode->descSize / sizeof(Util::Abi::PalMetadataNoteEntry);
                        auto pConfig = reinterpret_cast<const Util::Abi::PalMetadataNoteEntry*>(
                            pSection->pData + offset + noteHeaderSize);

                        for (uint32_t i = 0; i < configCount; ++i)
                        {
                            const char* pRegName = nullptr;
                            if (gfxIp.major <= 8)
                            {
                                pRegName = Gfx6::GetRegisterNameString(gfxIp, pConfig[i].key * 4);
                            }
                            else
                            {
#ifdef LLPC_BUILD_GFX9
                                pRegName = Gfx9::GetRegisterNameString(gfxIp, pConfig[i].key * 4);
#else
                                pRegName = "UNKNOWN";
#endif
                            }
                            auto length = snprintf(formatBuf,
                                                   sizeof(formatBuf),
                                                   "        %-45s = 0x%08X\n",
                                                   pRegName,
                                                   pConfig[i].value);
                            out << formatBuf;
                        }
                        break;
                    }
                default:
                    {
                        out << "    unknown note type " << (uint32_t)pNode->type << "\n";
                        break;
                    }
                }
                offset += noteHeaderSize + Pow2Align(pNode->descSize, sizeof(uint32_t));
                LLPC_ASSERT(offset <= pSection->secHead.sh_size);
            }
        }
        else if (strcmp(pSection->pName, RelocName) == 0)
        {
            // Output .reloc section
            out << pSection->pName << " (size = " << pSection->secHead.sh_size << " bytes)\n";
            const uint32_t relocCount = reader.GetRelocationCount();
            for (uint32_t i = 0; i < relocCount; ++i)
            {
                ElfReloc reloc = {};
                reader.GetRelocation(i, &reloc);
                ElfSymbol elfSym = {};
                reader.GetSymbol(reloc.symIdx, &elfSym);
                auto length = snprintf(formatBuf, sizeof(formatBuf), "    %-35s", elfSym.pSymName);
                out << "#" << i << "    " << formatBuf
                    << "    offset = " << reloc.offset << "\n";
            }
        }
        else if (strncmp(pSection->pName, AmdGpuConfigName, sizeof(AmdGpuConfigName) - 1) == 0)
        {
            // Output .AMDGPU.config section
            const uint32_t configCount = pSection->secHead.sh_size / sizeof(uint32_t) / 2;
            const uint32_t* pConfig = reinterpret_cast<const uint32_t*>(pSection->pData);
            out << pSection->pName << " (" << configCount << " registers)\n";

            for (uint32_t i = 0; i < configCount; ++i)
            {
                const char* pRegName = nullptr;
                if (gfxIp.major <= 8)
                {
                    pRegName = Gfx6::GetRegisterNameString(gfxIp, pConfig[2 * i]);
                }
                else
                {
#ifdef LLPC_BUILD_GFX9
                    pRegName = Gfx9::GetRegisterNameString(gfxIp, pConfig[2 * i]);
#else
                    pRegName = "UNKNOWN";
#endif
                }
                auto length = snprintf(formatBuf, sizeof(formatBuf), "        %-45s = 0x%08X\n", pRegName, pConfig[2 * i + 1]);
                out << formatBuf;
            }
        }
        else if ((strncmp(pSection->pName, AmdGpuDisasmName, sizeof(AmdGpuDisasmName) - 1) == 0) ||
                 (strncmp(pSection->pName, AmdGpuCsdataName, sizeof(AmdGpuCsdataName) - 1) == 0))
        {
            // Output text based sections
            out << pSection->pName << " (size = " << pSection->secHead.sh_size << " bytes)\n";

            std::vector<ElfSymbol> symbols;
            reader.GetSymbolsBySectionIndex(secIdx, symbols);
            uint32_t symIdx = 0;
            uint32_t startPos = 0;
            uint32_t endPos = 0;
            while (startPos < pSection->secHead.sh_size)
            {
                if (symIdx < symbols.size())
                {
                    endPos = symbols[symIdx].value;
                }
                else
                {
                    endPos = pSection->secHead.sh_size;
                }

                OutputText(pSection->pData, startPos, endPos, out);

                if (symIdx < symbols.size())
                {
                    out << "    " << symbols[symIdx].pSymName
                        << " (offset = " << symbols[symIdx].value << "  size = " << symbols[symIdx].size << ")\n";
                }
                ++symIdx;
                startPos = endPos;
            }
        }
        else
        {
            // Output binary based sections
            out << (pSection->pName[0] == 0 ? "(null)" : pSection->pName)
                << " (size = " << pSection->secHead.sh_size << " bytes)\n";

            std::vector<ElfSymbol> symbols;
            reader.GetSymbolsBySectionIndex(secIdx, symbols);
            uint32_t symIdx = 0;
            uint32_t startPos = 0;
            uint32_t endPos = 0;
            while (startPos < pSection->secHead.sh_size)
            {
                if (symIdx < symbols.size())
                {
                    endPos = symbols[symIdx].value;
                }
                else
                {
                    endPos = pSection->secHead.sh_size;
                }

                OutputBinary(pSection->pData, startPos, endPos, out);

                if (symIdx < symbols.size())
                {
                    out << "    " << symbols[symIdx].pSymName
                        << " (offset = " << symbols[symIdx].value << "  size = " << symbols[symIdx].size << ")\n";
                }
                ++symIdx;
                startPos = endPos;
            }
        }
        out << "\n";
    }

    return out;
}

// =====================================================================================================================
// Assistant macros for pipeline dump
#define CASE_CLASSENUM_TO_STRING(TYPE, ENUM) \
    case TYPE::ENUM: pString = #ENUM; break;
#define CASE_ENUM_TO_STRING(ENUM) \
    case ENUM: pString = #ENUM; break;

// =====================================================================================================================
// Translates enum "VkVertexInputRate" to string and output to ostream.
std::ostream& operator<<(
    std::ostream&       out,        // [out] Output stream
    VkVertexInputRate  inputRate)   // Vertex input rate
{
    const char* pString = nullptr;
    switch (inputRate)
    {
    CASE_ENUM_TO_STRING(VK_VERTEX_INPUT_RATE_VERTEX)
    CASE_ENUM_TO_STRING(VK_VERTEX_INPUT_RATE_INSTANCE)
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }
    return out << pString;
}

// =====================================================================================================================
// Translates enum "ResourceMappingNodeType" to string and output to ostream.
std::ostream& operator<<(
    std::ostream&                       out,   // [out] Output stream
    enum class ResourceMappingNodeType type)  // Resource map node type
{
    const char* pString = nullptr;
    switch (type)
    {
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorResource)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorSampler)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorCombinedTexture)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorTexelBuffer)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorFmask)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorBuffer)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorTableVaPtr)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, IndirectUserDataVaPtr)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, PushConst)
    CASE_CLASSENUM_TO_STRING(ResourceMappingNodeType, DescriptorBufferCompact)
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }
    return out << pString;
}

// =====================================================================================================================
// Translates enum "VkPrimitiveTopology" to string and output to ostream.
std::ostream& operator<<(
    std::ostream&       out,       // [out] Output stream
    VkPrimitiveTopology topology)  // Primitive topology
{
    const char* pString = nullptr;
    switch (topology)
    {
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
    CASE_ENUM_TO_STRING(VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }

    return out << pString;
}

// =====================================================================================================================
// Translates enum "VkFormat" to string and output to ostream.
std::ostream& operator<<(
    std::ostream&       out,     // [out] Output stream
    VkFormat           format)  // Resource format
{
    const char* pString = nullptr;
    switch (format)
    {
    CASE_ENUM_TO_STRING(VK_FORMAT_UNDEFINED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R4G4_UNORM_PACK8)
    CASE_ENUM_TO_STRING(VK_FORMAT_R4G4B4A4_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_B4G4R4A4_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_R5G6B5_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_B5G6R5_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_R5G5B5A1_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_B5G5R5A1_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_A1R5G5B5_UNORM_PACK16)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R8G8B8A8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_B8G8R8A8_SRGB)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_UNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_SNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_USCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_SSCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_UINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_SINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A8B8G8R8_SRGB_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_UNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_SNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_USCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_SSCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_UINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2R10G10B10_SINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_SNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_USCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_SSCALED_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_UINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_A2B10G10R10_SINT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_SNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_USCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_SSCALED)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R16G16B16A16_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32A32_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32A32_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R32G32B32A32_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64A64_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64A64_SINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_R64G64B64A64_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_D16_UNORM)
    CASE_ENUM_TO_STRING(VK_FORMAT_X8_D24_UNORM_PACK32)
    CASE_ENUM_TO_STRING(VK_FORMAT_D32_SFLOAT)
    CASE_ENUM_TO_STRING(VK_FORMAT_S8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_D16_UNORM_S8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_D24_UNORM_S8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_D32_SFLOAT_S8_UINT)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC1_RGB_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC1_RGB_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC1_RGBA_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC2_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC2_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC3_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC3_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC4_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC4_SNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC5_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC5_SNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC6H_UFLOAT_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC6H_SFLOAT_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC7_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_BC7_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_EAC_R11_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_EAC_R11_SNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_EAC_R11G11_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_EAC_R11G11_SNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_4x4_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_5x4_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_5x4_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_5x5_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_5x5_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_6x5_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_6x5_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_6x6_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_6x6_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x5_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x5_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x6_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x6_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x8_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_8x8_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x5_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x5_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x6_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x6_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x8_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x8_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x10_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_10x10_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_12x10_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_12x10_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_12x12_UNORM_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG)
    CASE_ENUM_TO_STRING(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG)
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }
    return out << pString;
}

} // Llpc
