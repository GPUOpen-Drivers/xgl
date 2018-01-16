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
namespace MetroHash { struct Hash; };
class MetroHash64;

class PipelineDumper
{
public:
    static void DumpSpirvBinary(const char*                     pDumpDir,
                                const BinaryData*               pSpirvBin,
                                MetroHash::Hash*                pHash);

    static std::ofstream* BeginPipelineDump(const char*                      pDumpDir,
                                            const ComputePipelineBuildInfo*  pComputePipelineInfo,
                                            const GraphicsPipelineBuildInfo* pGraphicsPipelineInfo,
                                            const MetroHash::Hash*           pHash);

    static void EndPipelineDump(std::ofstream* pDumpFile);

    static void DumpPipelineBinary(std::ostream*                    pDumpFile,
                                   GfxIpVersion                     gfxIp,
                                   const BinaryData*                pPipelineBin);

    static uint64_t GetGraphicsPipelineHash(const GraphicsPipelineBuildInfo* pPipelineInfo);
    static uint64_t GetComputePipelineHash(const ComputePipelineBuildInfo* pPipelineInfo);

    static MetroHash::Hash GenerateHashForGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipeline);
    static MetroHash::Hash GenerateHashForComputePipeline(const ComputePipelineBuildInfo* pPipeline);

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
                                                MetroHash64*              pHasher);

    static void UpdateHashForResourceMappingNode(const ResourceMappingNode* pUserDataNode,
                                                 MetroHash64*               pHasher);
};

} // Llpc
