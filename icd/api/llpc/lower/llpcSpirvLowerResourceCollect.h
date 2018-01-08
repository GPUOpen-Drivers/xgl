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
 * @file  llpcSpirvLowerResourceCollect.h
 * @brief LLPC header file: contains declaration of class Llpc::SpirvLowerResourceCollect.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/IR/InstVisitor.h"

#include "SPIRVInternal.h"
#include "llpc.h"
#include "llpcSpirvLower.h"

namespace Llpc
{

struct DescriptorBinding;
struct ResourceUsage;

// =====================================================================================================================
// Represents the pass of SPIR-V lowering opertions for resource collecting.
class SpirvLowerResourceCollect:
    public SpirvLower,
    public llvm::InstVisitor<SpirvLowerResourceCollect>
{
public:
    SpirvLowerResourceCollect();

    virtual bool runOnModule(llvm::Module& module);

    // Pass creator, creates the pass of SPIR-V lowering opertions for resource collecting
    static llvm::ModulePass* Create() { return new SpirvLowerResourceCollect(); }

    // -----------------------------------------------------------------------------------------------------------------

    static char ID;   // ID of this pass

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(SpirvLowerResourceCollect);

    uint32_t GetFlattenArrayElementCount(const llvm::Type* pTy) const;
    const llvm::Type* GetFlattenArrayElementType(const llvm::Type* pTy) const;

    void CollectExecutionModeUsage();
    void CollectDescriptorUsage(uint32_t descSet, uint32_t binding, const DescriptorBinding* pBinding);
    void CollectInOutUsage(const llvm::Type* pInOutTy, llvm::Constant* pInOutMeta, SPIRAddressSpace addrSpace);
    void CollectVertexInputUsage(const llvm::Type* pVertexTy, bool signedness, uint32_t startLoc, uint32_t locCount);

    // -----------------------------------------------------------------------------------------------------------------

    ResourceUsage*  m_pResUsage;    // Resource usage of the shader stage
};

} // Llpc
