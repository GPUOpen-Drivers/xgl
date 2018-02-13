/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  amdllpc.cpp
 * @brief LLPC source file: contains implementation of LLPC standalone tool.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "amd-llpc"

#ifdef WIN_OS
    // NOTE: Disable Windows-defined min()/max() because we use STL-defined std::min()/std::max() in LLPC.
    #define NOMINMAX
#endif

#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/ToolOutputFile.h"

#if defined(LLPC_MEM_TRACK_LEAK) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
    #include <crtdbg.h>
#else
    #ifdef WIN_OS
        #include <io.h>
        #include <signal.h>
    #endif
#endif

// NOTE: To enable VLD, please add option BUILD_WIN_VLD=1 in build option.To run amdllpc with VLD enabled,
// please copy vld.ini and all files in.\winVisualMemDetector\bin\Win64 to current directory of amdllpc.
#ifdef BUILD_WIN_VLD
    #include "vld.h"
#endif

#ifndef LLPC_ENABLE_SPIRV_OPT
    #define SPVGEN_STATIC_LIB   1
#endif
#include "spvgen.h"
#include "vfx.h"

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcElf.h"
#include "llpcInternal.h"

using namespace llvm;
using namespace Llpc;

namespace llvm
{

namespace cl
{

// Represents options of LLPC standalone tool.

// -gfxip: graphics IP version
static opt<std::string> GfxIp("gfxip", desc("Graphics IP version"), value_desc("major.minor.step"), init("8.0.0"));

// Input sources
static opt<std::string> InFile1(Positional, desc("<first source>"),  init("-"));
static opt<std::string> InFile2(Positional, desc("<second source>"), init("-"));
static opt<std::string> InFile3(Positional, desc("<third source>"),  init("-"));
static opt<std::string> InFile4(Positional, desc("<fourth source>"), init("-"));
static opt<std::string> InFile5(Positional, desc("<fifth source>"),  init("-"));

// -o: output
static opt<std::string> OutFile("o", desc("Output file"), value_desc("filename"));

// -l: link pipeline
static opt<bool>        ToLink("l", desc("Link pipeline and generate ISA codes"), init(true));

// -val: validate input SPIR-V binary or text
static opt<bool>        Validate("val", desc("Validate input SPIR-V binary or text"), init(true));

// -entry-target: name string of entry target (for multiple entry-points)
static opt<std::string> EntryTarget("entry-target",
                                    desc("Name string of entry target"),
                                    value_desc("entryname"),
                                    init("main"));

// -ignore-color-attachment-formats: ignore color attachment formats
static opt<bool> IgnoreColorAttachmentFormats("ignore-color-attachment-formats",
                                              desc("Ignore color attachment formats"), init(false));

#ifdef WIN_OS
// -assert-to-msgbox: pop message box when an assert is hit, only valid in Windows
static opt<bool>        AssertToMsgBox("assert-to-msgbox", desc("Pop message box when assert is hit"));
#endif

} // cl

} // llvm

// Represents allowed extensions of LLPC source files.
namespace LlpcExt
{

const char SpirvBin[]       = ".spv";
const char LlvmBin[]        = ".bc";
const char IsaBin[]         = ".isa";
const char SpirvText[]      = ".spvas";
const char GlslTextVs[]     = ".vert";
const char GlslTextTcs[]    = ".tesc";
const char GlslTextTes[]    = ".tese";
const char GlslTextGs[]     = ".geom";
const char GlslTextFs[]     = ".frag";
const char GlslTextCs[]     = ".comp";
const char PipelineInfo[]   = ".pipe";
const char LlvmIr[]         = ".ll";

} // LlpcExt

// Represents global compilation info of LLPC standalone tool (as tool context).
struct CompileInfo
{
    GfxIpVersion                gfxIp;                          // Graphics IP version info
    VkFlags                     stageMask;                      // Shader stage mask
    BinaryData                  spirvBin[ShaderStageCount];     // SPIR-V binary codes

    ShaderModuleBuildInfo       shaderInfo[ShaderStageCount];   // Info to build shader modules
    ShaderModuleBuildOut        shaderOut[ShaderStageCount];    // Output of building shader modules
    void*                       shaderBuf[ShaderStageCount];    // Allocation buffer of building shader modules

    GraphicsPipelineBuildInfo   gfxPipelineInfo;                // Info to build graphics pipeline
    GraphicsPipelineBuildOut    gfxPipelineOut;                 // Output of building graphics pipeline
    ComputePipelineBuildInfo    compPipelineInfo;               // Info to build compute pipeline
    ComputePipelineBuildOut     compPipelineOut;                // Output of building compute pipeline
    void*                       pPipelineBuf;                   // Alllocation buffer of building pipeline
    void*                       pPipelineInfoFile;              // VFX-style file containing pipeline info
};

// =====================================================================================================================
// Translates GLSL source language to corresponding shader stage.
static ShaderStage SourceLangToShaderStage(
    EShLanguage sourceLang) // GLSL source language
{
    static_assert(EShLangVertex         == 0, "Unexpected value!");
    static_assert(EShLangTessControl    == 1, "Unexpected value!");
    static_assert(EShLangTessEvaluation == 2, "Unexpected value!");
    static_assert(EShLangGeometry       == 3, "Unexpected value!");
    static_assert(EShLangFragment       == 4, "Unexpected value!");
    static_assert(EShLangCompute        == 5, "Unexpected value!");
    static_assert(EShLangCount          == 6, "Unexpected value!");

    return static_cast<ShaderStage>(sourceLang);
}

// =====================================================================================================================
// Performs initialization work for LLPC standalone tool.
static Result Init(
    int32_t      argc,          // Count of arguments
    char*        argv[],        // [in] List of arguments
    ICompiler**  ppCompiler,    // [out] Created LLPC compiler object
    CompileInfo* pCompileInfo)  // [out] Compilation info of LLPC standalone tool
{
    Result result = Result::Success;

#ifndef LLPC_ENABLE_SPIRV_OPT
    if (InitSpvGen() == false)
    {
        printf("Fail to load spvgen.dll and do initialization, can only compile SPIR-V binary\n");
    }
#endif

    if (result == Result::Success)
    {
        // NOTE: For testing consistency, these options should be kept the same as those of Vulkan
        // ICD (Device::InitLlpcCompiler()). Here, we check the specified options from command line.
        // For each default option that is missing, we add it manually. This code to check whether
        // the same option has been specified is not completely foolproof because it does not know
        // which arguments are not option names.
        static const char* defaultOptions[] =
        {
            // Name                      Option
            "-gfxip",                    "-gfxip=8.0.0",
            "-O",                        "-O3",
            "-pragma-unroll-threshold",  "-pragma-unroll-threshold=4096",
            "-unroll-allow-partial",     "-unroll-allow-partial",
            "-lower-dyn-index",          "-lower-dyn-index",
            "-simplifycfg-sink-common",  "-simplifycfg-sink-common=false",
            "-amdgpu-vgpr-index-mode",   "-amdgpu-vgpr-index-mode",         // force VGPR indexing on GFX8
        };

        // Build new arguments, starting with those supplied in command line
        std::vector<const char*> newArgs;
        GfxIpVersion gfxIp = {8, 0, 0};
        for (int32_t i = 0; i < argc; ++i)
        {
            newArgs.push_back(argv[i]);
        }

        static const size_t defaultOptionCount = sizeof(defaultOptions) / (2 * sizeof(defaultOptions[0]));
        for (uint32_t optionIdx = 0; optionIdx != defaultOptionCount; ++optionIdx)
        {
            const char* pName = defaultOptions[2 * optionIdx];
            const char* pOption = defaultOptions[2 * optionIdx + 1];
            size_t nameLen = strlen(pName);
            bool found = false;
            const char* pArg = nullptr;
            for (int32_t i = 1; i < argc; ++i)
            {
                pArg = argv[i];
                if ((strncmp(pArg, pName, nameLen) == 0) &&
                    ((pArg[nameLen] == '\0') || (pArg[nameLen] == '=') || (isdigit((int32_t)pArg[nameLen]))))
                {
                    found = true;
                    break;
                }
            }

            if (found == false)
            {
                newArgs.push_back(pOption);
            }
            else if (optionIdx == 0) // Find option -gfxip
            {
                size_t argLen = strlen(pArg);
                if ((argLen > nameLen) && pArg[nameLen] == '=')
                {
                    // Extract tokens of graphics IP version info (delimiter is ".")
                    const uint32_t len = argLen - nameLen - 1;
                    char* pGfxIp = new char[len + 1];
                    memcpy(pGfxIp, &pArg[nameLen + 1], len);
                    pGfxIp[len] = '\0';

                    char* tokens[3] = {}; // Format: major.minor.step
                    char* pToken = std::strtok(pGfxIp, ".");
                    for (uint32_t i = 0; (i < 3) && (pToken != nullptr); ++i)
                    {
                        tokens[i] = pToken;
                        pToken = std::strtok(nullptr, ".");
                    }

                    gfxIp.major    = (tokens[0] != nullptr) ? std::strtoul(tokens[0], nullptr, 10) : 0;
                    gfxIp.minor    = (tokens[1] != nullptr) ? std::strtoul(tokens[1], nullptr, 10) : 0;
                    gfxIp.stepping = (tokens[2] != nullptr) ? std::strtoul(tokens[2], nullptr, 10) : 0;

                    delete[] pGfxIp;
                }
            }
        }

        result = ICompiler::Create(gfxIp, newArgs.size(), &newArgs[0], ppCompiler);

        if (result == Result::Success)
        {
            pCompileInfo->gfxIp = gfxIp;
        }
    }

    return result;
}

// =====================================================================================================================
// Performs cleanup work for LLPC standalone tool.
static void Cleanup(
    ICompiler*   pCompiler,     // [in,out] LLPC compiler object
    CompileInfo* pCompileInfo)  // [in,out] Compilation info of LLPC standalone tool
{
    for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
    {
        if (pCompileInfo->stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage)))
        {
            // NOTE: We do not have to free SPIR-V binary for pipeline info file.
            // It will be freed when we close the VFX doc.
            if (pCompileInfo->pPipelineInfoFile == nullptr)
            {
                delete[] reinterpret_cast<const char*>(pCompileInfo->spirvBin[stage].pCode);
            }
            free(pCompileInfo->shaderBuf[stage]);
        }
    }

    free(pCompileInfo->pPipelineBuf);

    if (pCompileInfo->pPipelineInfoFile)
    {
        vfxCloseDoc(pCompileInfo->pPipelineInfoFile);
    }

    memset(pCompileInfo, 0, sizeof(*pCompileInfo));
    pCompiler->Destroy();
}

// =====================================================================================================================
// Callback function to allocate buffer for building shader module and building pipeline.
void* VKAPI_CALL AllocateBuffer(
    void*  pInstance,   // [in] Dummy instance object, unused
    void*  pUserData,   // [in] User data
    size_t size)        // Requested allocation size
{
    void* pAllocBuf = malloc(size);
    memset(pAllocBuf, 0, size);

    void** ppOutBuf = reinterpret_cast<void**>(pUserData);
    *ppOutBuf = pAllocBuf;
    return pAllocBuf;
}

// =====================================================================================================================
// Checks whether the specified file name represents a GLSL source text file (.vert, .tesc, .tese, .geom, .frag, or
// .comp).
static bool IsGlslTextFile(
    const std::string& fileName)    // [in] File name to check
{
    bool isGlslText = false;

    std::string extName;
    size_t extPos = fileName.find_last_of(".");
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if (extName.empty() == false)
    {
        if ((extName == LlpcExt::GlslTextVs)  ||
            (extName == LlpcExt::GlslTextTcs) ||
            (extName == LlpcExt::GlslTextTes) ||
            (extName == LlpcExt::GlslTextGs)  ||
            (extName == LlpcExt::GlslTextFs)  ||
            (extName == LlpcExt::GlslTextCs))
        {
            isGlslText = true;
        }
    }

    return isGlslText;
}

// =====================================================================================================================
// Checks whether the specified file name represents a SPRI-V assembly text file (.spvas).
static bool IsSpirvTextFile(
    const std::string& fileName)
{
    bool isSpirvText = false;

    size_t extPos = fileName.find_last_of(".");
    std::string extName;
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if ((extName.empty() == false) && (extName == LlpcExt::SpirvText))
    {
        isSpirvText = true;
    }

    return isSpirvText;
}

// =====================================================================================================================
// Checks whether the specified file name represents a SPRI-V binary file (.spv).
static bool IsSpirvBinaryFile(
    const std::string& fileName) // [in] File name to check
{
    bool isSpirvBin = false;

    size_t extPos = fileName.find_last_of(".");
    std::string extName;
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if ((extName.empty() == false) && (extName == LlpcExt::SpirvBin))
    {
        isSpirvBin = true;
    }

    return isSpirvBin;
}

// =====================================================================================================================
// Checks whether the specified file name represents a LLPC piepline info file (.pipe).
static bool IsPipelineInfoFile(
    const std::string& fileName) // [in] File name to check
{
    bool isPipelineInfo = false;

    size_t extPos = fileName.find_last_of(".");
    std::string extName;
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if ((extName.empty() == false) && (extName == LlpcExt::PipelineInfo))
    {
        isPipelineInfo = true;
    }

    return isPipelineInfo;
}

// =====================================================================================================================
// Checks whether the specified file name represents a LLVM IR file (.ll).
static bool IsLlvmIrFile(
    const std::string& fileName) // [in] File name to check
{
    bool isLlvmIr = false;

    size_t extPos = fileName.find_last_of(".");
    std::string extName;
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if ((extName.empty() == false) && (extName == LlpcExt::LlvmIr))
    {
        isLlvmIr = true;
    }

    return isLlvmIr;
}

// =====================================================================================================================
// Gets GLSL source language from file extension.
static EShLanguage GetGlslSourceLang(
    const std::string& fileName)    // [in] Name of GLSL source text file
{
    EShLanguage sourceLang = EShLangCount;

    std::string extName;

    size_t extPos = fileName.find_last_of(".");
    if (extPos != std::string::npos)
    {
        extName = fileName.substr(extPos, fileName.size() - extPos);
    }

    if (extName == LlpcExt::GlslTextVs)
    {
        sourceLang = EShLangVertex;
    }
    else if (extName == LlpcExt::GlslTextTcs)
    {
        sourceLang = EShLangTessControl;
    }
    else if (extName == LlpcExt::GlslTextTes)
    {
        sourceLang = EShLangTessEvaluation;
    }
    else if (extName == LlpcExt::GlslTextGs)
    {
        sourceLang = EShLangGeometry;
    }
    else if (extName == LlpcExt::GlslTextFs)
    {
        sourceLang = EShLangFragment;
    }
    else if (extName == LlpcExt::GlslTextCs)
    {
        sourceLang = EShLangCompute;
    }

    return sourceLang;
}

// =====================================================================================================================
// Gets SPIR-V binary codes from the specified binary file.
static Result GetSpirvBinaryFromFile(
    const std::string& spvBinFile,  // [in] SPIR-V binary file
    BinaryData*        pSpvBin)     // [out] SPIR-V binary codes
{
    Result result = Result::Success;

    FILE* pBinFile = fopen(spvBinFile.c_str(), "rb");
    if (pBinFile == nullptr)
    {
        LLPC_ERRS("Fails to open SPIR-V binary file: " << spvBinFile << "\n");
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        fseek(pBinFile, 0, SEEK_END);
        size_t binSize = ftell(pBinFile);
        fseek(pBinFile, 0, SEEK_SET);

        char* pBin= new char[binSize];
        LLPC_ASSERT(pBin != nullptr);
        memset(pBin, 0, binSize);
        binSize = fread(pBin, 1, binSize, pBinFile);

        pSpvBin->codeSize = binSize;
        pSpvBin->pCode    = pBin;

        fclose(pBinFile);
    }

    return result;
}

// =====================================================================================================================
// GLSL compiler, compiles GLSL source text file (input) to SPIR-V binary file (output).
static Result CompileGlsl(
    const std::string& inFile,      // [in] Input file, GLSL source text
    ShaderStage*       pStage,      // [out] Shader stage
    std::string&       outFile)     // [out] Output file, SPIR-V binary
{
    Result result = Result::Success;

    EShLanguage lang = GetGlslSourceLang(inFile);
    *pStage = SourceLangToShaderStage(lang);

    FILE* pInFile = fopen(inFile.c_str(), "r");
    if (pInFile == nullptr)
    {
        LLPC_ERRS("Fails to open input file: " << inFile << "\n");
        result = Result::ErrorUnavailable;
    }

    FILE* pOutFile = nullptr;
    if (result == Result::Success)
    {
        outFile = sys::path::filename(inFile).str() + LlpcExt::SpirvBin;

        pOutFile = fopen(outFile.c_str(), "wb");
        if (pOutFile == nullptr)
        {
            LLPC_ERRS("Fails to open output file: " << outFile << "\n");
            result = Result::ErrorUnavailable;
        }
    }

    if (result == Result::Success)
    {
        fseek(pInFile, 0, SEEK_END);
        size_t textSize = ftell(pInFile);
        fseek(pInFile, 0, SEEK_SET);

        char* pGlslText = new char[textSize + 1];
        LLPC_ASSERT(pGlslText != nullptr);
        memset(pGlslText, 0, textSize + 1);
        auto readSize = fread(pGlslText, 1, textSize, pInFile);
        pGlslText[readSize] = 0;

        LLPC_OUTS("===============================================================================\n");
        LLPC_OUTS("// GLSL sources: " << inFile << "\n\n");
        LLPC_OUTS(pGlslText);
        LLPC_OUTS("\n\n");

        int32_t sourceStringCount[EShLangCount] = {};
        const char*const* sourceList[EShLangCount] = {};
        sourceStringCount[lang] = 1;
        sourceList[lang] = &pGlslText;

        void* pProgram = nullptr;
        const char* pLog = nullptr;
        bool compileResult = spvCompileAndLinkProgram(sourceStringCount, sourceList, &pProgram, &pLog);

        LLPC_OUTS("// GLSL program compile/link log\n");

        if (compileResult)
        {
            const uint32_t* pSpvBin = nullptr;
            uint32_t binSize = spvGetSpirvBinaryFromProgram(pProgram, lang, &pSpvBin);
            fwrite(pSpvBin, 1, binSize, pOutFile);

            textSize = binSize * 10 + 1024;
            char* pSpvText = new char[textSize];
            LLPC_ASSERT(pSpvText != nullptr);
            memset(pSpvText, 0, textSize);
            LLPC_OUTS("\nSPIR-V disassembly: " << outFile << "\n");
            spvDisassembleSpirv(binSize, pSpvBin, textSize, pSpvText);
            LLPC_OUTS(pSpvText << "\n");
            delete[] pSpvText;
        }
        else
        {
            LLPC_ERRS("Fail to compile GLSL sources\n\n" << pLog << "\n");
            result = Result::ErrorInvalidShader;
        }

        delete[] pGlslText;

        fclose(pInFile);
        fclose(pOutFile);
    }

    return result;
}

// =====================================================================================================================
// SPIR-V assembler, converts SPIR-V assembly text file (input) to SPIR-V binary file (output).
static Result AssembleSpirv(
    const std::string& inFile,  // [in] Input file, SPIR-V assembly text
    std::string&       outFile) // [out] Output file, SPIR-V binary
{
    Result result = Result::Success;

    FILE* pInFile = fopen(inFile.c_str(), "r");
    if (pInFile == nullptr)
    {
        LLPC_ERRS("Fails to open input file: " << inFile << "\n");
        result = Result::ErrorUnavailable;
    }

    FILE* pOutFile = nullptr;
    if (result == Result::Success)
    {
        outFile = sys::path::stem(sys::path::filename(inFile)).str() + LlpcExt::SpirvBin;

        pOutFile = fopen(outFile.c_str(), "wb");
        if (pOutFile == nullptr)
        {
            LLPC_ERRS("Fails to open output file: " << outFile << "\n");
            result = Result::ErrorUnavailable;
        }
    }

    if (result == Result::Success)
    {
        fseek(pInFile, 0, SEEK_END);
        size_t textSize = ftell(pInFile);
        fseek(pInFile, 0, SEEK_SET);

        char* pSpvText = new char[textSize + 1];
        LLPC_ASSERT(pSpvText != nullptr);
        memset(pSpvText, 0, textSize + 1);

        size_t realSize = fread(pSpvText, 1, textSize, pInFile);
        int32_t binSize = realSize * 4 + 1024; // Estimated SPIR-V binary size
        uint32_t* pSpvBin = new uint32_t[binSize / sizeof(uint32_t)];
        LLPC_ASSERT(pSpvBin != nullptr);

        const char* pLog = nullptr;
        binSize = spvAssembleSpirv(pSpvText, binSize, pSpvBin, &pLog);
        if (binSize < 0)
        {
            LLPC_ERRS("Fails to assemble SPIR-V: \n" << pLog << "\n");
            result = Result::ErrorInvalidShader;
        }
        else
        {
            fwrite(pSpvBin, 1, binSize, pOutFile);

            LLPC_OUTS("===============================================================================\n");
            LLPC_OUTS("// SPIR-V disassembly: " << inFile << "\n");
            LLPC_OUTS(pSpvText);
            LLPC_OUTS("\n\n");
        }

        fclose(pInFile);
        fclose(pOutFile);

        delete[] pSpvText;
        delete[] pSpvBin;
    }

    return result;
}

// =====================================================================================================================
// Decodes the binary after building a pipeline and outputs the decoded info.
static Result DecodePipelineBinary(
    const BinaryData* pPipelineBin, // [in] Pipeline binary
    CompileInfo*      pCompileInfo, // [in,out] Compilation info of LLPC standalone tool
    bool              isGraphics)   // Whether it is graphics pipeline
{
    Result result = Result::Success;

    ElfReader<Elf64> reader(pCompileInfo->gfxIp);
    size_t readSize = 0;
    result = reader.ReadFromBuffer(pPipelineBin->pCode, &readSize);

    if (result == Result::Success)
    {
        LLPC_OUTS("===============================================================================\n");
        LLPC_OUTS("// LLPC final ELF info\n");
        LLPC_OUTS(reader);
    }

    return result;
}

// =====================================================================================================================
// Builds shader module based on the specified SPIR-V binary.
static Result BuildShaderModules(
    const ICompiler* pCompiler,     // [in] LLPC compiler object
    CompileInfo*     pCompileInfo)  // [in,out] Compilation info of LLPC standalone tool
{
    Result result = Result::Success;

    for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
    {
        if (pCompileInfo->stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage)))
        {
            ShaderModuleBuildInfo* pShaderInfo = &pCompileInfo->shaderInfo[stage];
            ShaderModuleBuildOut*  pShaderOut  = &pCompileInfo->shaderOut[stage];

            pShaderInfo->pInstance      = nullptr; // Dummy, unused
            pShaderInfo->pUserData      = &pCompileInfo->shaderBuf[stage];
            pShaderInfo->pfnOutputAlloc = AllocateBuffer;
            pShaderInfo->shaderBin      = pCompileInfo->spirvBin[stage];

            result = pCompiler->BuildShaderModule(pShaderInfo, pShaderOut);
            if ((result != Result::Success) && (result != Result::Delayed))
            {
                LLPC_ERRS("Fails to build "
                          << GetShaderStageName(static_cast<ShaderStage>(stage))
                          << " shader module: " << "\n");
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Builds pipeline and do linking.
static Result BuildPipeline(
    ICompiler*    pCompiler,     // [in] LLPC compiler object
    CompileInfo*  pCompileInfo)  // [in,out] Compilation info of LLPC standalone tool
{
    Result result = Result::Success;

    BinaryData pipelinBin = {};

    bool isGraphics = (pCompileInfo->stageMask & ShaderStageToMask(ShaderStageCompute)) ? false : true;
    if (isGraphics)
    {
        // Build graphics pipeline
        GraphicsPipelineBuildInfo* pPipelineInfo = &pCompileInfo->gfxPipelineInfo;
        GraphicsPipelineBuildOut*  pPipelineOut  = &pCompileInfo->gfxPipelineOut;

        // Fill pipeline shader info
        PipelineShaderInfo* shaderInfo[ShaderStageGfxCount] =
        {
            &pPipelineInfo->vs,
            &pPipelineInfo->tcs,
            &pPipelineInfo->tes,
            &pPipelineInfo->gs,
            &pPipelineInfo->fs,
        };

        for (uint32_t stage = 0; stage < ShaderStageGfxCount; ++stage)
        {
            if (pCompileInfo->stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage)))
            {
                PipelineShaderInfo*         pShaderInfo = shaderInfo[stage];
                const ShaderModuleBuildOut* pShaderOut  = &pCompileInfo->shaderOut[stage];

                if (pShaderInfo->pEntryTarget == nullptr)
                {
                    // If entry target is not specified, use the one from command line option
                    pShaderInfo->pEntryTarget = cl::EntryTarget.c_str();
                }
                pShaderInfo->pModuleData  = pShaderOut->pModuleData;
            }
        }

        pPipelineInfo->pInstance      = nullptr; // Dummy, unused
        pPipelineInfo->pUserData      = &pCompileInfo->pPipelineBuf;
        pPipelineInfo->pfnOutputAlloc = AllocateBuffer;

        // NOTE: If number of patch control points is not specified, we set it to 3.
        if (pPipelineInfo->iaState.patchControlPoints == 0)
        {
            pPipelineInfo->iaState.patchControlPoints = 3;
        }

        result = pCompiler->BuildGraphicsPipeline(pPipelineInfo, pPipelineOut);
        if (result == Result::Success)
        {
            result = DecodePipelineBinary(&pPipelineOut->pipelineBin, pCompileInfo, true);
        }
    }
    else
    {
        // Build compute pipeline
        ComputePipelineBuildInfo* pPipelineInfo = &pCompileInfo->compPipelineInfo;
        ComputePipelineBuildOut*  pPipelineOut  = &pCompileInfo->compPipelineOut;

        PipelineShaderInfo*         pShaderInfo = &pPipelineInfo->cs;
        const ShaderModuleBuildOut* pShaderOut  = &pCompileInfo->shaderOut[ShaderStageCompute];

        if (pShaderInfo->pEntryTarget == nullptr)
        {
            // If entry target is not specified, use the one from command line option
            pShaderInfo->pEntryTarget = cl::EntryTarget.c_str();
        }
        pShaderInfo->pModuleData  = pShaderOut->pModuleData;

        pPipelineInfo->pInstance      = nullptr; // Dummy, unused
        pPipelineInfo->pUserData      = &pCompileInfo->pPipelineBuf;
        pPipelineInfo->pfnOutputAlloc = AllocateBuffer;

        result = pCompiler->BuildComputePipeline(pPipelineInfo, pPipelineOut);
        if (result == Result::Success)
        {
            result = DecodePipelineBinary(&pPipelineOut->pipelineBin, pCompileInfo, false);
        }
    }

    return result;
}

// =====================================================================================================================
// Outputs ELF binary to the specified file.
static Result OutputElf(
    CompileInfo*       pCompileInfo,  // [in] Compilation info of LLPC standalone tool
    const std::string& outFile)       // [in] Name of the file to output ELF binary
{
    Result result = Result::Success;
    FILE* pOutFile = fopen(outFile.c_str(), "wb");
    if (pOutFile == nullptr)
    {
        LLPC_ERRS("Failed to open output file: " << outFile << "\n");
        result = Result::ErrorUnavailable;
    }
    if (result == Result::Success)
    {
        const BinaryData* pPipelineBin = (pCompileInfo->stageMask & ShaderStageToMask(ShaderStageCompute)) ?
                                             &pCompileInfo->compPipelineOut.pipelineBin :
                                             &pCompileInfo->gfxPipelineOut.pipelineBin;
        if (fwrite(pPipelineBin->pCode, 1, pPipelineBin->codeSize, pOutFile) != pPipelineBin->codeSize)
        {
            result = Result::ErrorUnavailable;
        }
        if (fclose(pOutFile))
        {
            result = Result::ErrorUnavailable;
        }
        if (result != Result::Success)
        {
            LLPC_ERRS("Failed to write output file: " << outFile << "\n");
        }
    }
    return result;
}

#ifdef WIN_OS
// =====================================================================================================================
// Callback function for SIGABRT.
extern "C" void LlpcSignalAbortHandler(
    int signal)  // Signal type
{
    if (signal == SIGABRT)
    {
        RedirectLogOutput(true, 0, nullptr); // Restore redirecting to show crash in console window
        LLVM_BUILTIN_TRAP;
    }
}
#endif

#if defined(LLPC_MEM_TRACK_LEAK) && defined(_DEBUG)
// =====================================================================================================================
// Enable VC run-time based memory leak detection.
static void EnableMemoryLeakDetection()
{
   // Retrieve the state of CRT debug reporting:
   int32_t dbgFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Append custom flags to enable memory leak checks:
   dbgFlag |= _CRTDBG_LEAK_CHECK_DF;
   dbgFlag |= _CRTDBG_ALLOC_MEM_DF;

   // Update the run-time settings:
   _CrtSetDbgFlag (dbgFlag);
}
#endif

// =====================================================================================================================
// Main function of LLPC standalone tool, entry-point.
//
// Returns 0 if successful. Other numeric values indicate failure.
int32_t main(
    int32_t argc,       // Count of arguments
    char*   argv[])     // [in] List of arguments
{
    Result result = Result::Success;

    ICompiler*  pCompiler   = nullptr;
    CompileInfo compileInfo = {};

    //
    // Initialization
    //

    // TODO: CRT based Memory leak detection is conflict with stack trace now, we only can enable one of them.
#if defined(LLPC_MEM_TRACK_LEAK) && defined(_DEBUG)
    EnableMemoryLeakDetection();
#else
    EnablePrettyStackTrace();
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    PrettyStackTraceProgram X(argc, argv);

#ifdef WIN_OS
    signal(SIGABRT, LlpcSignalAbortHandler);
#endif
#endif

    result = Init(argc, argv, &pCompiler, &compileInfo);

#ifdef WIN_OS
    if (cl::AssertToMsgBox)
    {
        _set_error_mode(_OUT_TO_MSGBOX);
    }
#endif

    constexpr uint32_t MaxFileCount = ShaderStageGfxCount;

    std::string inFiles[MaxFileCount] =
    {
        cl::InFile1,
        cl::InFile2,
        cl::InFile3,
        cl::InFile4,
        cl::InFile5,
    };
    std::string outFile = cl::OutFile;

    //
    // Translate sources to SPIR-V binary
    //
    for (uint32_t i = 0; (i < MaxFileCount) && (result == Result::Success); ++i)
    {
        std::string spvBinFile;

        if (inFiles[i] == "-")
        {
            // Corresponding source file is not specified, ignore
            continue;
        }

        if (IsGlslTextFile(inFiles[i]))
        {
            // GLSL source text
            ShaderStage stage = ShaderStageInvalid;
            result = CompileGlsl(inFiles[i], &stage, spvBinFile);
            if (result == Result::Success)
            {
                compileInfo.stageMask |= ShaderStageToMask(stage);
                result = GetSpirvBinaryFromFile(spvBinFile, &compileInfo.spirvBin[stage]);
            }
        }
        else if (IsSpirvTextFile(inFiles[i]) || IsSpirvBinaryFile(inFiles[i]))
        {
            // SPIR-V assembly text or SPIR-V binary
            if (IsSpirvTextFile(inFiles[i]))
            {
                result = AssembleSpirv(inFiles[i], spvBinFile);
            }
            else
            {
                spvBinFile = inFiles[i];
            }

            BinaryData spvBin = {};

            if (result == Result::Success)
            {
                result = GetSpirvBinaryFromFile(spvBinFile, &spvBin);
            }

            if ((result == Result::Success) && cl::Validate)
            {
                char log[1024] = {};
                if (spvValidateSpirv != nullptr)
                {
                    if (spvValidateSpirv(spvBin.codeSize, spvBin.pCode, sizeof(log), log) == false)
                    {
                        LLPC_ERRS("Fails to validate SPIR-V: \n" << log << "\n");
                        result = Result::ErrorInvalidShader;
                    }
                }
            }

            if (result == Result::Success)
            {
                uint32_t stageMask = GetStageMaskFromSpirvBinary(&spvBin, cl::EntryTarget.c_str());
                if (stageMask != 0)
                {
                    for (uint32_t stage = ShaderStageVertex; stage < ShaderStageCount; ++stage)
                    {
                        if (stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage)))
                        {
                            compileInfo.spirvBin[stage] = spvBin;
                            compileInfo.stageMask |= ShaderStageToMask(static_cast<ShaderStage>(stage));
                            break;
                        }
                    }
                }
                else
                {
                    result = Result::ErrorUnavailable;
                }
            }

        }
        else if (IsPipelineInfoFile(inFiles[i]))
        {
            const char* pLog = nullptr;
            bool vfxResult = vfxParseFile(inFiles[i].c_str(),
                                          0,
                                          nullptr,
                                          VfxDocTypePipeline,
                                          &compileInfo.pPipelineInfoFile,
                                          &pLog);
            if (vfxResult)
            {
                VfxPipelineStatePtr pPipelineState = nullptr;
                vfxGetPipelineDoc(compileInfo.pPipelineInfoFile, &pPipelineState);

                if (pPipelineState->version != Llpc::Version)
                {
                    LLPC_ERRS("Version incompatible, SPVGEN::Version = " << pPipelineState->version <<
                              " AMDLLPC::Version = " << Llpc::Version << "\n");
                    result = Result::ErrorInvalidShader;
                }
                else
                {
                    compileInfo.compPipelineInfo = pPipelineState->compPipelineInfo;
                    compileInfo.gfxPipelineInfo = pPipelineState->gfxPipelineInfo;
                    if (cl::IgnoreColorAttachmentFormats)
                    {
                        // NOTE: When this option is enabled, we set color attachment format to
                        // R8G8B8A8_SRGB for color target 0. Also, for other color targets, if the
                        // formats are not UNDEFINED, we set them to R8G8B8A8_SRGB as well.
                        for (uint32_t target = 0; target < MaxColorTargets; ++target)
                        {
                            if ((target == 0) ||
                                (compileInfo.gfxPipelineInfo.cbState.target[target].format != VK_FORMAT_UNDEFINED))
                            {
                                compileInfo.gfxPipelineInfo.cbState.target[target].format = VK_FORMAT_R8G8B8A8_SRGB;
                            }
                        }
                    }

                    for (uint32_t stage = 0; stage < ShaderStageCount; ++stage)
                    {
                        if (pPipelineState->stages[stage].dataSize > 0)
                        {
                            compileInfo.spirvBin[stage].codeSize = pPipelineState->stages[stage].dataSize;
                            compileInfo.spirvBin[stage].pCode = pPipelineState->stages[stage].pData;
                            compileInfo.stageMask |= ShaderStageToMask(static_cast<ShaderStage>(stage));

                            uint32_t binSize =  pPipelineState->stages[stage].dataSize;
                            uint32_t textSize = binSize * 10 + 1024;
                            char* pSpvText = new char[textSize];
                            LLPC_ASSERT(pSpvText != nullptr);
                            memset(pSpvText, 0, textSize);
                            LLPC_OUTS("\nSPIR-V disassembly for " <<
                                      GetShaderStageName(static_cast<ShaderStage>(stage)) << "\n");
                            spvDisassembleSpirv(binSize, compileInfo.spirvBin[stage].pCode, textSize, pSpvText);
                            LLPC_OUTS(pSpvText << "\n");
                            delete[] pSpvText;
                        }
                    }
                }
            }
            else
            {
                 LLPC_ERRS("Failed to parse input file: " << inFiles[i] << "\n" << pLog << "\n");
                 result = Result::ErrorInvalidShader;
            }
        }
        else if (IsLlvmIrFile(inFiles[i]))
        {
            LLVMContext context;
            SMDiagnostic errDiag;

            // Load LLVM IR
            std::unique_ptr<Module> pModule =
                parseAssemblyFile(inFiles[i], errDiag, context, nullptr, false);
            if (pModule.get() == nullptr)
            {
                std::string errMsg;
                raw_string_ostream errStream(errMsg);
                errDiag.print(inFiles[i].c_str(), errStream);
                LLPC_ERRS(errMsg);
                result = Result::ErrorInvalidShader;
            }

            // Verify LLVM module
            std::string errMsg;
            raw_string_ostream errStream(errMsg);
            if ((result == Result::Success) && verifyModule(*pModule.get(), &errStream))
            {
                LLPC_ERRS("File " << inFiles[i] << " parsed, but fail to verify the module: " << errMsg << "\n");
                result = Result::ErrorInvalidShader;
            }

            // Check the shader stage of input module
            ShaderStage shaderStage = ShaderStageInvalid;
            if (result == Result::Success)
            {
                shaderStage = GetShaderStageFromModule(pModule.get());
                if (shaderStage == ShaderStageInvalid)
                {
                    LLPC_ERRS("File " << inFiles[i] << ": Fail to determine shader stage\n");
                    result = Result::ErrorInvalidShader;
                }
            }

            if (result == Result::Success)
            {
                // Translate LLVM module to LLVM bitcode
                llvm::SmallString<1024> bitcodeBuf;
                raw_svector_ostream bitcodeStream(bitcodeBuf);
                WriteBitcodeToFile(pModule.get(), bitcodeStream);

                void* pCode = new uint8_t[bitcodeBuf.size()];
                memcpy(pCode, bitcodeBuf.data(), bitcodeBuf.size());
                compileInfo.spirvBin[shaderStage].codeSize = bitcodeBuf.size();
                compileInfo.spirvBin[shaderStage].pCode = pCode;
                compileInfo.stageMask |= ShaderStageToMask(static_cast<ShaderStage>(shaderStage));
            }
        }
    }

    //
    // Build shader modules
    //
    if ((result == Result::Success) && (compileInfo.stageMask != 0))
    {
        result = BuildShaderModules(pCompiler, &compileInfo);
    }

    //
    // Build pipeline
    //
    if ((result == Result::Success) && cl::ToLink)
    {
        result = BuildPipeline(pCompiler, &compileInfo);
        if ((result == Result::Success) && (outFile.empty() == false))
        {
            result = OutputElf(&compileInfo, outFile);
        }
    }

    //
    // Clean up
    //
    Cleanup(pCompiler, &compileInfo);
    pCompiler = nullptr;

    if (result == Result::Success)
    {
        LLPC_OUTS("\n=====  AMDLLPC SUCCESS  =====\n");
    }

    return (result == Result::Success) ? 0 : 1;
}
