/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcUtil.cpp
 * @brief LLPC source file: contains implementation of LLPC internal types and utility functions
 * (independent of LLVM use).
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-util"

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcElf.h"
#include "llpcUtil.h"
#include "palPipelineAbi.h"

namespace Llpc
{

// =====================================================================================================================
// Gets the name string of shader stage.
const char* GetShaderStageName(
    ShaderStage shaderStage)  // Shader stage
{
    const char* pName = nullptr;

    if (shaderStage == ShaderStageCopyShader)
    {
        pName = "copy";
    }
    else
    {
        LLPC_ASSERT(shaderStage < ShaderStageCount);

        static const char* ShaderStageNames[] =
        {
            "vertex",
            "tessellation control",
            "tessellation evaluation",
            "geometry",
            "fragment",
            "compute",
        };

        pName = ShaderStageNames[static_cast<uint32_t>(shaderStage)];
    }

    return pName;
}

// =====================================================================================================================
// Gets name string of the abbreviation for the specified shader stage
const char* GetShaderStageAbbreviation(
    ShaderStage shaderStage,  // Shader stage
    bool        upper)        // Whether to use uppercase for the abbreviation (default is lowercase)
{
    const char* pAbbr = nullptr;

    if (shaderStage == ShaderStageCopyShader)
    {
        pAbbr = upper ? "COPY" : "Copy";
    }
    else
    {
        LLPC_ASSERT(shaderStage < ShaderStageCount);

        if (upper)
        {
            static const char* ShaderStageAbbrs[] =
            {
                "VS",
                "TCS",
                "TES",
                "GS",
                "FS",
                "CS",
            };

            pAbbr = ShaderStageAbbrs[static_cast<uint32_t>(shaderStage)];
        }
        else
        {
            static const char* ShaderStageAbbrs[] =
            {
                "Vs",
                "Tcs",
                "Tes",
                "Gs",
                "Fs",
                "Cs",
            };

            pAbbr = ShaderStageAbbrs[static_cast<uint32_t>(shaderStage)];
        }
    }

    return pAbbr;
}

// =====================================================================================================================
// Gets the symbol name for .text section.
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForTextSection(
    ShaderStage stage,      // Shader stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::LsMainEntry)];
        }
        else if (hasGs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::EsMainEntry)];
        }
        else
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        }
        break;
    case ShaderStageTessControl:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
           static_cast<uint32_t>(Util::Abi::PipelineSymbolType::HsMainEntry)];
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::EsMainEntry)];
        }
        else
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        }
        break;
    case ShaderStageGeometry:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
           static_cast<uint32_t>(Util::Abi::PipelineSymbolType::GsMainEntry)];
        break;
    case ShaderStageFragment:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::PsMainEntry)];
        break;
    case ShaderStageCompute:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::CsMainEntry)];
        break;
    case ShaderStageCopyShader:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// =====================================================================================================================
// Gets the symbol name for .AMDGPU.disasm section.
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForDisasmSection(
    ShaderStage stage,      // Shader Stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = DebugSymNames::LsDisasm;
        }
        else if (hasGs)
        {
            pName = DebugSymNames::EsDisasm;
        }
        else
        {
            pName = DebugSymNames::VsDisasm;
        }
        break;
    case ShaderStageTessControl:
        pName = DebugSymNames::HsDisasm;
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = DebugSymNames::EsDisasm;
        }
        else
        {
            pName = DebugSymNames::VsDisasm;
        }
        break;
    case ShaderStageGeometry:
        pName = DebugSymNames::GsDisasm;
        break;
    case ShaderStageFragment:
        pName = DebugSymNames::PsDisasm;
        break;
    case ShaderStageCompute:
        pName = DebugSymNames::CsDisasm;
        break;
    case ShaderStageCopyShader:
        pName = DebugSymNames::VsDisasm;
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// =====================================================================================================================
// Gets the symbol name for .AMDGPU.csdata section
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForCsdataSection(
    ShaderStage stage,      // Shader stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = DebugSymNames::LsCsdata;
        }
        else if (hasGs)
        {
            pName = DebugSymNames::EsCsdata;
        }
        else
        {
            pName = DebugSymNames::VsCsdata;
        }
        break;
    case ShaderStageTessControl:
        pName = DebugSymNames::HsCsdata;
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = DebugSymNames::EsCsdata;
        }
        else
        {
            pName = DebugSymNames::VsCsdata;
        }
        break;
    case ShaderStageGeometry:
        pName = DebugSymNames::GsCsdata;
        break;
    case ShaderStageFragment:
        pName = DebugSymNames::PsCsdata;
        break;
    case ShaderStageCompute:
        pName = DebugSymNames::CsCsdata;
        break;
    case ShaderStageCopyShader:
        pName = DebugSymNames::VsCsdata;
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// =====================================================================================================================
// Translates shader stage to corresponding stage mask.
uint32_t ShaderStageToMask(
    ShaderStage stage)  // Shader stage
{
    LLPC_ASSERT((stage < ShaderStageCount) || (stage == ShaderStageCopyShader));
    return (1 << stage);
}

} // Llpc
