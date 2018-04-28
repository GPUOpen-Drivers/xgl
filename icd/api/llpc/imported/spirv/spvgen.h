/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  spvgen.h
 * @brief SPVGEN header file: contains the definition and the wrap implementation of SPIR-V generator entry-points
 ***********************************************************************************************************************
 */
#pragma once

#define SPVGEN_VERSION  0x10000
#define SPVGEN_REVISION 4

#ifndef SH_IMPORT_EXPORT
    #ifdef _WIN32
        #define SPVAPI __cdecl
        #ifdef SH_EXPORTING
            #define SH_IMPORT_EXPORT __declspec(dllexport)
        #else
            #define SH_IMPORT_EXPORT __declspec(dllimport)
        #endif
    #else
        #define SH_IMPORT_EXPORT
        #define SPVAPI
    #endif
#endif

enum SpvGenVersion
{
    SpvGenVersionGlslang,
    SpvGenVersionSpirv,
    SpvGenVersionStd450,
    SpvGenVersionExtAmd,
    SpvGenVersionCount,
};

enum VfxDocType
{
    VfxDocTypeRender,
    VfxDocTypePipeline
};

struct VfxRenderState;
struct  VfxPipelineState;

typedef struct VfxRenderState* VfxRenderStatePtr;
typedef struct VfxPipelineState* VfxPipelineStatePtr;

#ifdef SH_EXPORTING

#ifdef __cplusplus
extern "C"{
#endif
bool SH_IMPORT_EXPORT spvCompileAndLinkProgramFromFile(
    int             fileNum,
    const char*     fileList[],
    void**          pProgram,
    const char**    ppLog);

bool SH_IMPORT_EXPORT spvCompileAndLinkProgram(
    int                sourceStringCount[EShLangCount],
    const char* const* sourceList[EShLangCount],
    void**             pProgram,
    const char**       ppLog);

void SH_IMPORT_EXPORT spvDestroyProgram(
    void* hProgram);

int SH_IMPORT_EXPORT spvGetSpirvBinaryFromProgram(
    void*                hProgram,
    EShLanguage          stage,
    const unsigned int** ppData);

int SH_IMPORT_EXPORT spvAssembleSpirv(
    const char*   pSpvText,
    unsigned int  bufSize,
    unsigned int* pBuffer,
    const char**  ppLog);

bool SH_IMPORT_EXPORT spvDisassembleSpirv(
    unsigned int size,
    const void*  pSpvToken,
    unsigned int bufSize,
    char*        pBuffer);

bool SH_IMPORT_EXPORT spvValidateSpirv(
    unsigned int size,
    const void*  pSpvToken,
    unsigned int logSize,
    char*        pLog);

bool SH_IMPORT_EXPORT spvOptimizeSpirv(
    unsigned int   size,
    const void*    pSpvToken,
    int            optionCount,
    const char*    options[],
    unsigned int*  pBufSize,
    void**         ppOptBuf,
    unsigned int   logSize,
    char*          pLog);

void SH_IMPORT_EXPORT spvFreeBuffer(
    void* pBuffer);

bool SH_IMPORT_EXPORT spvGetVersion(
    SpvGenVersion version,
    unsigned int* pVersion,
    unsigned int* pReversion);

bool SH_IMPORT_EXPORT vfxParseFile(
    const char*  pFilename,
    unsigned int numMacro,
    const char*  pMacros[],
    VfxDocType   type,
    void**       ppDoc,
    const char** ppErrorMsg);

void SH_IMPORT_EXPORT vfxCloseDoc(
    void* pDoc);

void SH_IMPORT_EXPORT vfxGetRenderDoc(
    void*              pDoc,
    VfxRenderStatePtr* pRenderState);

void SH_IMPORT_EXPORT vfxGetPipelineDoc(
    void*                pDoc,
    VfxPipelineStatePtr* pPipelineState);

void SH_IMPORT_EXPORT vfxPrintDoc(
    void*                pDoc);

#ifdef __cplusplus
}
#endif

#else

typedef enum {
    EShLangVertex,
    EShLangTessControl,
    EShLangTessEvaluation,
    EShLangGeometry,
    EShLangFragment,
    EShLangCompute,
    EShLangCount,
} EShLanguage;

// =====================================================================================================================
// SPIR-V generator entrypoints declaration
typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvCompileAndLinkProgramFromFile)(
    int             fileNum,
    const char*     fileList[],
    void**          pProgram,
    const char**    ppLog);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvCompileAndLinkProgram)(
    int                sourceStringCount[EShLangCount],
    const char* const* sourceList[EShLangCount],
    void**             pProgram,
    const char**       ppLog);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_spvDestroyProgram)(void* hProgram);

typedef int SH_IMPORT_EXPORT (SPVAPI* PFN_spvGetSpirvBinaryFromProgram)(
    void*                hProgram,
    EShLanguage          stage,
    const unsigned int** ppData);

typedef int SH_IMPORT_EXPORT (SPVAPI* PFN_spvAssembleSpirv)(
    const char*     pSpvText,
    unsigned int    codeBufSize,
    unsigned int*   pSpvCodeBuf,
    const char**    ppLog);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvDisassembleSpirv)(
    unsigned int        size,
    const void*         pSpvCode,
    unsigned int        textBufSize,
    char*               pSpvTextBuf);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvValidateSpirv)(
    unsigned int        size,
    const void*         pSpvToken,
    unsigned int        bufSize,
    char*               pLog);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvOptimizeSpirv)(
    unsigned int   size,
    const void*    pSpvToken,
    int            optionCount,
    const char*    options[],
    unsigned int*  pBufSize,
    void**         ppOptBuf,
    unsigned int   logSize,
    char*          pLog);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_spvFreeBuffer)(
    void* pBuffer);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_spvGetVersion)(
    SpvGenVersion  version,
     unsigned int* pVersion,
     unsigned int* pReversion);

typedef bool SH_IMPORT_EXPORT (SPVAPI* PFN_vfxParseFile)(
    const char*  pFilename,
    unsigned int numMacro,
    const char*  pMacros[],
    VfxDocType   type,
    void**       ppDoc,
    const char** ppErrorMsg);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_vfxCloseDoc)(
    void* pDoc);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_vfxGetRenderDoc)(
    void*              pDoc,
    VfxRenderStatePtr* pRenderState);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_vfxGetPipelineDoc)(
    void*                pDoc,
    VfxPipelineStatePtr* pPipelineState);

typedef void SH_IMPORT_EXPORT (SPVAPI* PFN_vfxPrintDoc)(
    void*                pDoc);

// =====================================================================================================================
// SPIR-V generator entry-points
#define DECL_EXPORT_FUNC(func) \
  extern PFN_##func g_pfn##func

DECL_EXPORT_FUNC(spvCompileAndLinkProgramFromFile);
DECL_EXPORT_FUNC(spvCompileAndLinkProgram);
DECL_EXPORT_FUNC(spvDestroyProgram);
DECL_EXPORT_FUNC(spvGetSpirvBinaryFromProgram);
DECL_EXPORT_FUNC(spvAssembleSpirv);
DECL_EXPORT_FUNC(spvDisassembleSpirv);
DECL_EXPORT_FUNC(spvValidateSpirv);
DECL_EXPORT_FUNC(spvOptimizeSpirv);
DECL_EXPORT_FUNC(spvFreeBuffer);
DECL_EXPORT_FUNC(spvGetVersion);
DECL_EXPORT_FUNC(vfxParseFile);
DECL_EXPORT_FUNC(vfxCloseDoc);
DECL_EXPORT_FUNC(vfxGetRenderDoc);
DECL_EXPORT_FUNC(vfxGetPipelineDoc);
DECL_EXPORT_FUNC(vfxPrintDoc);

#endif

#ifdef SPVGEN_STATIC_LIB

#define DEFI_EXPORT_FUNC(func) \
  PFN_##func g_pfn##func = nullptr

DEFI_EXPORT_FUNC(spvCompileAndLinkProgramFromFile);
DEFI_EXPORT_FUNC(spvCompileAndLinkProgram);
DEFI_EXPORT_FUNC(spvDestroyProgram);
DEFI_EXPORT_FUNC(spvGetSpirvBinaryFromProgram);
DEFI_EXPORT_FUNC(spvAssembleSpirv);
DEFI_EXPORT_FUNC(spvDisassembleSpirv);
DEFI_EXPORT_FUNC(spvValidateSpirv);
DEFI_EXPORT_FUNC(spvOptimizeSpirv);
DEFI_EXPORT_FUNC(spvFreeBuffer);
DEFI_EXPORT_FUNC(spvGetVersion);
DEFI_EXPORT_FUNC(vfxParseFile);
DEFI_EXPORT_FUNC(vfxCloseDoc);
DEFI_EXPORT_FUNC(vfxGetRenderDoc);
DEFI_EXPORT_FUNC(vfxGetPipelineDoc);
DEFI_EXPORT_FUNC(vfxPrintDoc);

// SPIR-V generator Windows implementation
#ifdef _WIN32

#include <windows.h>
// SPIR-V generator Windows DLL name
#ifdef UNICODE
static const wchar_t* SpvGeneratorName = L"spvgen.dll";
#else
static const char* SpvGeneratorName = "spvgen.dll";
#endif

#define INITFUNC(func) \
  g_pfn##func = reinterpret_cast<PFN_##func>(GetProcAddress(hModule, #func));\
  if (g_pfn##func == NULL)\
  {\
      success = false;\
  }

#define INIT_OPT_FUNC(func) \
  g_pfn##func = reinterpret_cast<PFN_##func>(GetProcAddress(hModule, #func));

#else

#include <dlfcn.h>
#include <stdio.h>
static const char* SpvGeneratorName = "spvgen.so";

#define INITFUNC(func) \
  g_pfn##func = reinterpret_cast<PFN_##func>(dlsym(hModule, #func));\
  if (g_pfn##func == NULL)\
  {\
      success = false;\
  }

#define INIT_OPT_FUNC(func) \
  g_pfn##func = reinterpret_cast<PFN_##func>(dlsym(hModule, #func));

#endif // _WIN32

// =====================================================================================================================
// Initialize SPIR-V generator entry-points
bool InitSpvGen()
{
    bool success = true;
#ifdef _WIN32
    HMODULE hModule = LoadLibrary(SpvGeneratorName);
#else
    void* hModule = dlopen(SpvGeneratorName, RTLD_GLOBAL | RTLD_NOW);
#endif

    if (hModule != NULL)
    {
        INITFUNC(spvCompileAndLinkProgramFromFile);
        INITFUNC(spvCompileAndLinkProgram);
        INITFUNC(spvDestroyProgram);
        INITFUNC(spvGetSpirvBinaryFromProgram);
        INITFUNC(spvAssembleSpirv);
        INITFUNC(spvDisassembleSpirv);
        INITFUNC(spvValidateSpirv);
        INITFUNC(spvOptimizeSpirv);
        INITFUNC(spvFreeBuffer);
        INIT_OPT_FUNC(spvGetVersion);
        INITFUNC(vfxParseFile);
        INITFUNC(vfxCloseDoc);
        INITFUNC(vfxGetRenderDoc);
        INITFUNC(vfxGetPipelineDoc);
        INIT_OPT_FUNC(vfxPrintDoc);
    }
    else
    {
#ifndef _WIN32
        fprintf(stderr, "Failed: %s\n", dlerror());
#endif
        success = false;
    }
    return success;
}

#endif

#ifndef SH_EXPORTING

#define spvCompileAndLinkProgramFromFile g_pfnspvCompileAndLinkProgramFromFile
#define spvCompileAndLinkProgram         g_pfnspvCompileAndLinkProgram
#define spvDestroyProgram                g_pfnspvDestroyProgram
#define spvGetSpirvBinaryFromProgram     g_pfnspvGetSpirvBinaryFromProgram
#define spvAssembleSpirv                 g_pfnspvAssembleSpirv
#define spvDisassembleSpirv              g_pfnspvDisassembleSpirv
#define spvValidateSpirv                 g_pfnspvValidateSpirv
#define spvOptimizeSpirv                 g_pfnspvOptimizeSpirv
#define spvFreeBuffer                    g_pfnspvFreeBuffer
#define spvGetVersion                    g_pfnspvGetVersion
#define vfxParseFile                     g_pfnvfxParseFile
#define vfxCloseDoc                      g_pfnvfxCloseDoc
#define vfxGetRenderDoc                  g_pfnvfxGetRenderDoc
#define vfxGetPipelineDoc                g_pfnvfxGetPipelineDoc
#define vfxPrintDoc                      g_pfnvfxPrintDoc

#endif

