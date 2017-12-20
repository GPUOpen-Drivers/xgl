/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  llpcContext.h
 * @brief LLPC header file: contains declaration of class Llpc::Context.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"

#include <unordered_map>
#include <unordered_set>
#include "spirv.hpp"

#include "llpcPipelineContext.h"

namespace Llpc
{

// =====================================================================================================================
// Represents LLPC context for pipeline compilation. Derived from the base class llvm::LLVMContext.
class Context : public llvm::LLVMContext
{
public:
    Context(GfxIpVersion gfxIp);
    ~Context();

    // Checks whether this context is in use.
    bool IsInUse() const { return m_isInUse; }

    // Set context in-use flag.
    void SetInUse(bool inUse) { m_isInUse = inUse; }

    // Attaches pipeline context to LLPC context.
    void AttachPipelineContext(PipelineContext* pPipelineContext)
    {
        m_pPipelineContext = pPipelineContext;
    }

    // Gets pipeline context.
    PipelineContext* GetPipelineContext() const
    {
        return m_pPipelineContext;
    }

    // Gets the library that is responsible for GLSL emulation.
    llvm::Module* GetGlslEmuLibrary() const
    {
        return m_pGlslEmuLib.get();
    }

    // Gets the library that is responsible for GLSL emulation with LLVM native instructions and intrinsics.
    llvm::Module* GetNativeGlslEmuLibrary() const
    {
        return m_pNativeGlslEmuLib.get();
    }

    // Gets pre-constructed LLVM types
    llvm::Type* BoolTy() const { return m_tys.pBoolTy; }
    llvm::Type* Int8Ty() const { return m_tys.pInt8Ty; }
    llvm::Type* Int16Ty() const { return m_tys.pInt16Ty; }
    llvm::Type* Int32Ty() const { return m_tys.pInt32Ty; }
    llvm::Type* Int64Ty()  const { return m_tys.pInt64Ty; }
    llvm::Type* Float16Ty() const { return m_tys.pFloat16Ty; }
    llvm::Type* FloatTy() const { return m_tys.pFloatTy; }
    llvm::Type* DoubleTy() const { return m_tys.pDoubleTy; }
    llvm::Type* VoidTy() const { return m_tys.pVoidTy; }

    llvm::Type* Int32x2Ty() const { return m_tys.pInt32x2Ty; }
    llvm::Type* Int32x3Ty() const { return m_tys.pInt32x3Ty; }
    llvm::Type* Int32x4Ty() const { return m_tys.pInt32x4Ty; }
    llvm::Type* Int32x6Ty() const { return m_tys.pInt32x6Ty; }
    llvm::Type* Int32x8Ty() const { return m_tys.pInt32x8Ty; }
    llvm::Type* Float16x2Ty() const { return m_tys.pFloat16x2Ty; }
    llvm::Type* Floatx2Ty() const { return m_tys.pFloatx2Ty; }
    llvm::Type* Floatx3Ty() const { return m_tys.pFloatx3Ty; }
    llvm::Type* Floatx4Ty() const { return m_tys.pFloatx4Ty; }

    // Gets IDs of pre-declared LLVM metadata
    uint32_t MetaIdInvariantLoad() const { return m_metaIds.invariantLoad; }
    uint32_t MetaIdRange() const { return m_metaIds.range; }
    uint32_t MetaIdUniform() const { return m_metaIds.uniform; }

    std::unique_ptr<llvm::Module> LoadLibary(const BinaryData* pLib);

    // Wrappers of interfaces of pipeline context
    ResourceUsage* GetShaderResourceUsage(ShaderStage shaderStage)
    {
        return m_pPipelineContext->GetShaderResourceUsage(shaderStage);
    }

    InterfaceData* GetShaderInterfaceData(ShaderStage shaderStage)
    {
        return m_pPipelineContext->GetShaderInterfaceData(shaderStage);
    }

    bool IsGraphics() const
    {
        return m_pPipelineContext->IsGraphics();
    }

    const PipelineShaderInfo* GetPipelineShaderInfo(ShaderStage shaderStage) const
    {
        return m_pPipelineContext->GetPipelineShaderInfo(shaderStage);
    }

    const void* GetPipelineBuildInfo() const
    {
        return m_pPipelineContext->GetPipelineBuildInfo();
    }

    uint32_t GetShaderStageMask() const
    {
        return m_pPipelineContext->GetShaderStageMask();
    }

    uint32_t GetActiveShaderStageCount() const
    {
        return m_pPipelineContext->GetActiveShaderStageCount();
    }

    ShaderStage GetPrevShaderStage(ShaderStage shaderStage) const
    {
        return m_pPipelineContext->GetPrevShaderStage(shaderStage);
    }

    ShaderStage GetNextShaderStage(ShaderStage shaderStage) const
    {
        return m_pPipelineContext->GetNextShaderStage(shaderStage);
    }

    const char* GetGpuNameString() const
    {
        return m_pPipelineContext->GetGpuNameString();
    }

    const char* GetGpuNameAbbreviation() const
    {
        return m_pPipelineContext->GetGpuNameAbbreviation();
    }

    GfxIpVersion GetGfxIpVersion() const
    {
        return m_pPipelineContext->GetGfxIpVersion();
    }

    const GpuProperty* GetGpuProperty() const
    {
        return m_pPipelineContext->GetGpuProperty();
    }

    llvm::MDNode* GetEmptyMetadataNode()
    {
        return m_pEmptyMetaNode;
    }

    uint32_t GetDescriptorTablePtrHigh()
    {
        return m_pPipelineContext->GetDescriptorTablePtrHigh();
    }

    void AutoLayoutDescriptor(ShaderStage shaderStage)
    {
        return m_pPipelineContext->AutoLayoutDescriptor(shaderStage);
    }

    bool IsTessOffChip() const
    {
        return m_pPipelineContext->IsTessOffChip();
    }

    bool CanGsOnChip()
    {
        return m_pPipelineContext->CanGsOnChip();
    };

    bool IsGsOnChip()
    {
        return m_pPipelineContext->IsGsOnChip();
    }

    void SetGsOnChip(bool gsOnChip)
    {
        m_pPipelineContext->SetGsOnChip(gsOnChip);
    }

#ifdef LLPC_BUILD_GFX9
    void DoUserDataNodeMerge()
    {
        m_pPipelineContext->DoUserDataNodeMerge();
    }
#endif

    uint64_t GetPiplineHashCode() const
    {
        return m_pPipelineContext->GetPiplineHashCode();
    }

    uint64_t GetShaderHashCode(ShaderStage shaderStage) const
    {
        return m_pPipelineContext->GetShaderHashCode(shaderStage);
    }

private:
    LLPC_DISALLOW_DEFAULT_CTOR(Context);
    LLPC_DISALLOW_COPY_AND_ASSIGN(Context);

    // -----------------------------------------------------------------------------------------------------------------

     GfxIpVersion                  m_gfxIp;             // Graphics IP version info
     PipelineContext*              m_pPipelineContext;  // Pipeline-specific context
     std::unique_ptr<llvm::Module> m_pGlslEmuLib;       // LLVM library for GLSL emulation
     std::unique_ptr<llvm::Module> m_pNativeGlslEmuLib; // Native LLVM library for GLSL emulation
     bool                          m_isInUse;           // Whether this context is in use

    llvm::MDNode*       m_pEmptyMetaNode;   // Empty metadata node

    // Pre-constructed LLVM types
    struct
    {
        llvm::Type* pBoolTy;      // Bool
        llvm::Type* pInt8Ty;      // Int8
        llvm::Type* pInt16Ty;     // Int16
        llvm::Type* pInt32Ty;     // Int32
        llvm::Type* pInt64Ty;     // Int64
        llvm::Type* pFloat16Ty;   // Float16
        llvm::Type* pFloatTy;     // Float
        llvm::Type* pDoubleTy;    // Double
        llvm::Type* pVoidTy;      // Void

        llvm::Type* pInt32x2Ty;   // Int32 x 2
        llvm::Type* pInt32x3Ty;   // Int32 x 3
        llvm::Type* pInt32x4Ty;   // Int32 x 4
        llvm::Type* pInt32x6Ty;   // Int32 x 6
        llvm::Type* pInt32x8Ty;   // Int32 x 8
        llvm::Type* pFloat16x2Ty; // Float16 x 2
        llvm::Type* pFloatx2Ty;   // Float x 2
        llvm::Type* pFloatx3Ty;   // Float x 3
        llvm::Type* pFloatx4Ty;   // Float x 4
    } m_tys;

    // IDs of pre-declared LLVM metadata
    struct
    {
        uint32_t invariantLoad;   // "invariant.load"
        uint32_t range;           // "range"
        uint32_t uniform;         // "amdgpu.uniform"
    } m_metaIds;

    // GLSL emulation libraries
    static const uint8_t GlslEmuLib[];
    static const uint8_t GlslEmuLibGfx6[];
#ifdef LLPC_BUILD_GFX9
    static const uint8_t GlslEmuLibGfx9[];
#endif

};

} // Llpc
