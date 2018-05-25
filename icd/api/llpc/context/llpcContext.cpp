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
 * @file  llpcContext.cpp
 * @brief LLPC source file: contains implementation of class Llpc::Context.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-context"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llpcCompiler.h"
#include "llpcContext.h"
#include "llpcMetroHash.h"
#include "llpcPassNonNativeFuncRemove.h"
#include "llpcShaderCache.h"
#include "llpcShaderCacheManager.h"

namespace llvm
{

namespace cl
{

// -enable-cache-emu-lib-context: enable the cache of context of GLSL emulation library to file.
static opt<uint32_t> EnableCacheEmuLibContext("enable-cache-emu-lib-context",
                                         desc("Enable the cache of context of GLSL emulation library to file"),
                                         init(0));

} // cl

} // llvm

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
const uint8_t Context::GlslEmuLib[]=
{
    #include "./generate/g_llpcGlslEmuLib.h"
};

const uint8_t Context::GlslEmuLibGfx8[] =
{
#include "./generate/gfx8/g_llpcGlslEmuLibGfx8.h"
};

const uint8_t Context::GlslEmuLibGfx9[]=
{
    #include "./generate/gfx9/g_llpcGlslEmuLibGfx9.h"
};

// =====================================================================================================================
Context::Context(
    GfxIpVersion gfxIp) // Graphics IP version info
    :
    LLVMContext(),
    m_gfxIp(gfxIp)
{
    std::vector<Metadata*> emptyMeta;
    m_pEmptyMetaNode = MDNode::get(*this, emptyMeta);

    // Initialize pre-constructed LLVM derived types
    m_tys.pBoolTy     = Type::getInt1Ty(*this);
    m_tys.pInt8Ty     = Type::getInt8Ty(*this);
    m_tys.pInt16Ty    = Type::getInt16Ty(*this);
    m_tys.pInt32Ty    = Type::getInt32Ty(*this);
    m_tys.pInt64Ty    = Type::getInt64Ty(*this);
    m_tys.pFloat16Ty  = Type::getHalfTy(*this);
    m_tys.pFloatTy    = Type::getFloatTy(*this);
    m_tys.pDoubleTy   = Type::getDoubleTy(*this);
    m_tys.pVoidTy     = Type::getVoidTy(*this);

    m_tys.pInt32x2Ty    = VectorType::get(m_tys.pInt32Ty, 2);
    m_tys.pInt32x3Ty    = VectorType::get(m_tys.pInt32Ty, 3);
    m_tys.pInt32x4Ty    = VectorType::get(m_tys.pInt32Ty, 4);
    m_tys.pInt32x6Ty    = VectorType::get(m_tys.pInt32Ty, 6);
    m_tys.pInt32x8Ty    = VectorType::get(m_tys.pInt32Ty, 8);
    m_tys.pFloat16x2Ty  = VectorType::get(m_tys.pFloat16Ty, 2);
    m_tys.pFloat16x4Ty  = VectorType::get(m_tys.pFloat16Ty, 4);
    m_tys.pFloatx2Ty    = VectorType::get(m_tys.pFloatTy, 2);
    m_tys.pFloatx3Ty    = VectorType::get(m_tys.pFloatTy, 3);
    m_tys.pFloatx4Ty    = VectorType::get(m_tys.pFloatTy, 4);

    // Initialize IDs of pre-declared LLVM metadata
    m_metaIds.invariantLoad = getMDKindID("invariant.load");
    m_metaIds.range         = getMDKindID("range");
    m_metaIds.uniform       = getMDKindID("amdgpu.uniform");

    ShaderEntryState glslEmuLibEntryState = {};
    CacheEntryHandle hGlslEmuLibEntry = nullptr;
    ShaderEntryState nativeGlslEmuLibEntryState = {};
    CacheEntryHandle hNativeGlslEmuLibEntry = nullptr;
    // Initialize shader cache
    ShaderCacheCreateInfo    createInfo = {};
    ShaderCacheAuxCreateInfo auxCreateInfo = {};
    auxCreateInfo.pExecutableName = "__LLPC_CONTEXT_CACHE__";
    auxCreateInfo.pCacheFilePath = getenv("AMD_SHADER_DISK_CACHE_PATH");

    if (auxCreateInfo.pCacheFilePath == nullptr)
    {
#ifdef WIN_OS
        auxCreateInfo.pCacheFilePath = getenv("LOCALAPPDATA");
#else
        auxCreateInfo.pCacheFilePath = getenv("HOME");
#endif
    }

    if (cl::EnableCacheEmuLibContext == 1)
    {
        auxCreateInfo.shaderCacheMode = ShaderCacheEnableOnDisk;
    }
    else if (cl::EnableCacheEmuLibContext == 2)
    {
        auxCreateInfo.shaderCacheMode = ShaderCacheEnableOnDiskReadOnly;
    }
    else
    {
        auxCreateInfo.shaderCacheMode = ShaderCacheEnableRuntime;
    }

    auto contextCache = ShaderCacheManager::GetShaderCacheManager()->GetShaderCacheObject(&createInfo, &auxCreateInfo);
    MetroHash64 emuLibhasher;
    emuLibhasher.Update(gfxIp);
    MetroHash::Hash emuLibHash = {};
    emuLibhasher.Finalize(emuLibHash.bytes);
    glslEmuLibEntryState = contextCache->FindShader(emuLibHash, true, &hGlslEmuLibEntry);
    if (glslEmuLibEntryState == ShaderEntryState::Ready)
    {
        BinaryData libBin = {};
        auto result = contextCache->RetrieveShader(hGlslEmuLibEntry, &libBin.pCode, &libBin.codeSize);
        m_pGlslEmuLib = LoadLibary(&libBin);
    }

    bool isNativeLib = true;
    MetroHash64 nativeEmuLibHasher;
    nativeEmuLibHasher.Update(gfxIp);
    nativeEmuLibHasher.Update(isNativeLib);
    MetroHash::Hash nativeEmuLibHash = {};
    nativeEmuLibHasher.Finalize(nativeEmuLibHash.bytes);

    nativeGlslEmuLibEntryState = contextCache->FindShader(nativeEmuLibHash, true, &hNativeGlslEmuLibEntry);
    if (nativeGlslEmuLibEntryState == ShaderEntryState::Ready)
    {
        BinaryData libBin = {};
        auto result = contextCache->RetrieveShader(hNativeGlslEmuLibEntry, &libBin.pCode, &libBin.codeSize);
        m_pNativeGlslEmuLib = LoadLibary(&libBin);
    }

    // Load external LLVM libraries
    if ((m_pNativeGlslEmuLib == nullptr) || (m_pGlslEmuLib == nullptr))
    {
        BinaryData libBin = {};
        libBin.codeSize = sizeof(GlslEmuLib);
        libBin.pCode    = GlslEmuLib;
        m_pGlslEmuLib = LoadLibary(&libBin);
        LLPC_ASSERT(m_pGlslEmuLib != nullptr);

        // Link GFX-independent and GFX-dependent libraries together
        EnableDebugOutput(false);

        std::unique_ptr<Module> pGlslEmuLibGfx;
        if (gfxIp.major >= 8)
        {
            libBin.codeSize = sizeof(GlslEmuLibGfx8);
            libBin.pCode = GlslEmuLibGfx8;
            pGlslEmuLibGfx = LoadLibary(&libBin);
            LLPC_ASSERT(pGlslEmuLibGfx != nullptr);
            if (Linker::linkModules(*m_pGlslEmuLib, std::move(pGlslEmuLibGfx), Linker::OverrideFromSrc))
            {
                LLPC_ERRS("Fails to link LLVM libraries together\n");
            }
        }

        if (gfxIp.major >= 9)
        {
            libBin.codeSize = sizeof(GlslEmuLibGfx9);
            libBin.pCode    = GlslEmuLibGfx9;
            pGlslEmuLibGfx = LoadLibary(&libBin);
            LLPC_ASSERT(pGlslEmuLibGfx != nullptr);
            if (Linker::linkModules(*m_pGlslEmuLib, std::move(pGlslEmuLibGfx), Linker::OverrideFromSrc))
            {
                LLPC_ERRS("Fails to link LLVM libraries together\n");
            }
        }

        // Do function inlining
        {
            legacy::PassManager passMgr;

            passMgr.add(createFunctionInliningPass(InlineThreshold));

            if (passMgr.run(*m_pGlslEmuLib) == false)
            {
                LLPC_NEVER_CALLED();
            }
        }

        if (hGlslEmuLibEntry && (glslEmuLibEntryState == ShaderEntryState::Compiling))
        {
            SmallString<1024> glslEmuLibBin;
            raw_svector_ostream libBinStream(glslEmuLibBin);
            WriteBitcodeToFile(*m_pGlslEmuLib, libBinStream);
            contextCache->InsertShader(hGlslEmuLibEntry, glslEmuLibBin.data(), glslEmuLibBin.size());
            hGlslEmuLibEntry = nullptr;
        }

        // Remove non-native function for native lib
        {
            m_pNativeGlslEmuLib = CloneModule(*m_pGlslEmuLib.get());
            legacy::PassManager passMgr;
            passMgr.add(PassNonNativeFuncRemove::Create());

            if (passMgr.run(*m_pNativeGlslEmuLib) == false)
            {
                LLPC_NEVER_CALLED();
            }
        }

        if (hNativeGlslEmuLibEntry &&  (nativeGlslEmuLibEntryState == ShaderEntryState::Compiling))
        {
            SmallString<1024> nativeGlslEmuLibBin;
            raw_svector_ostream libBinStream(nativeGlslEmuLibBin);
            WriteBitcodeToFile(*m_pNativeGlslEmuLib, libBinStream);
            contextCache->InsertShader(hNativeGlslEmuLibEntry, nativeGlslEmuLibBin.data(), nativeGlslEmuLibBin.size());
        }

        EnableDebugOutput(true);
    }
}

// =====================================================================================================================
Context::~Context()
{
}

// =====================================================================================================================
// Loads library from external LLVM library.
std::unique_ptr<Module> Context::LoadLibary(
    const BinaryData* pLib)     // [in] Bitcodes of external LLVM library
{
    auto pMemBuffer = MemoryBuffer::getMemBuffer(
        StringRef(static_cast<const char*>(pLib->pCode), pLib->codeSize), "", false);

    Expected<std::unique_ptr<Module>> moduleOrErr =
        getLazyBitcodeModule(pMemBuffer->getMemBufferRef(), *this);

    std::unique_ptr<Module> pLibModule = nullptr;
    if (!moduleOrErr)
    {
        Error error = moduleOrErr.takeError();
        LLPC_ERRS("Fails to load LLVM bitcode \n");
    }
    else
    {
        pLibModule = std::move(*moduleOrErr);
        if (llvm::Error errCode = pLibModule->materializeAll())
        {
            LLPC_ERRS("Fails to materialize \n");
            pLibModule = nullptr;
        }
    }

    return std::move(pLibModule);
}

// =====================================================================================================================
// Sets triple and data layout in specified module from the context's target machine.
void Context::SetModuleTargetMachine(
    Module* pModule)  // [in/out] Module to modify
{
    pModule->setTargetTriple(GetTargetMachine()->getTargetTriple().getTriple());
    pModule->setDataLayout(GetTargetMachine()->createDataLayout());
}

} // Llpc
