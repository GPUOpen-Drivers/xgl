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
 * @file  llpcPipelineDumper.h
 * @brief LLPC header file: contains definitions of LLPC pipline dump utility
 ***********************************************************************************************************************
 */
#pragma once

#include <fstream>
#include <llpc.h>

namespace Llpc
{

struct ComputePipelineBuildInfo;
struct GraphicsPipelineBuildInfo;
struct BinaryData;
struct PipelineDumpFile;
namespace MetroHash { struct Hash; };
class MetroHash64;

// Enumerates which types of pipeline dump are disable
enum PipelineDumpFilters : uint32_t
{
    PipelineDumpFilterNone = 0x00, // Do not disable any pipeline type
    PipelineDumpFilterCs   = 0x01, // Disable pipeline dump for Cs
    PipelineDumpFilterNgg  = 0x02, // Disable pipeline dump for NGG
    PipelineDumpFilterGs   = 0x04, // Disable pipeline dump for Gs
    PipelineDumpFilterTess = 0x08, // Disable pipeline dump for Tess
    PipelineDumpFilterVsPs = 0x10, // Disable pipeline dump for VsPs
};

class PipelineDumper
{
public:
    static void DumpSpirvBinary(const char*                     pDumpDir,
                                const BinaryData*               pSpirvBin,
                                MetroHash::Hash*                pHash);

    static PipelineDumpFile* BeginPipelineDump(const PipelineDumpOptions*       pDumpOptions,
                                               const ComputePipelineBuildInfo*  pComputePipelineInfo,
                                               const GraphicsPipelineBuildInfo* pGraphicsPipelineInfo,
                                               const MetroHash::Hash*           pHash);

    static void EndPipelineDump(PipelineDumpFile* pDumpFile);

    static void DumpPipelineBinary(PipelineDumpFile*                pBinaryFile,
                                   GfxIpVersion                     gfxIp,
                                   const BinaryData*                pPipelineBin);

    static MetroHash::Hash GenerateHashForGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipeline, bool isCacheHash);
    static MetroHash::Hash GenerateHashForComputePipeline(const ComputePipelineBuildInfo* pPipeline, bool isCacheHash);

private:
    static std::string GetSpirvBinaryFileName(const MetroHash::Hash* pHash);
    static std::string GetPipelineInfoFileName(const ComputePipelineBuildInfo*  pComputePipelineInfo,
                                               const GraphicsPipelineBuildInfo* pGraphicsPipelineInfo,
                                               const MetroHash::Hash*                 pHash);

    static void DumpComputePipelineInfo(std::ostream*                   pDumpFile,
                                       const ComputePipelineBuildInfo* pPipelineInfo);
    static void DumpGraphicsPipelineInfo(std::ostream*                    pDumpFile,
                                         const GraphicsPipelineBuildInfo* pPipelineInfo);

    static void DumpVersionInfo(std::ostream&                  dumpFile);
    static void DumpPipelineShaderInfo(ShaderStage               stage,
                                       const PipelineShaderInfo* pShaderInfo,
                                       std::ostream&             dumpFile);
    static void DumpResourceMappingNode(const ResourceMappingNode* pUserDataNode,
                                        const char*                pPrefix,
                                        std::ostream&              dumpFile);
    static void DumpComputeStateInfo(const ComputePipelineBuildInfo* pPipelineInfo,
                                     std::ostream&                   dumpFile);
     static void DumpGraphicsStateInfo(const GraphicsPipelineBuildInfo* pPipelineInfo,
                                      std::ostream&                    dumpFile);

    static void UpdateHashForPipelineShaderInfo(ShaderStage               stage,
                                                const PipelineShaderInfo* pShaderInfo,
                                                bool                      isCacheHash,
                                                MetroHash64*              pHasher);

    static void UpdateHashForResourceMappingNode(const ResourceMappingNode* pUserDataNode,
                                                 MetroHash64*               pHasher);
};

} // Llpc
