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
 * @file  llpcVertexFetch.h
 * @brief LLPC header file: contains declaration of class Llpc::VertexFetch.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcInternal.h"
#include "llpcIntrinsDefs.h"

namespace Llpc
{

class Context;

// Represents vertex format info corresponding to vertex attribute format (VkFormat).
struct VertexFormatInfo
{
    VkFormat        format;         // Vertex attribute format
    BufNumFormat    nfmt;           // Numeric format of vertex buffer
    BufDataFormat   dfmt;           // Data format of vertex buffer
    uint32_t        numChannels;    // Valid number of channels
};

// Represents vertex component info corresponding to to vertex data format (BufDataFormat).
//
// NOTE: This info is used by vertex fetch instructions. We split vertex fetch into its per-component fetches when
// the original vertex fetch does not match the hardware requirements (such as vertex attribute offset, vertex
// attribute stride, etc..)
struct VertexCompFormatInfo
{
    uint32_t        vertexByteSize; // Byte size of the vertex
    uint32_t        compByteSize;   // Byte size of each individual component
    uint32_t        compCount;      // Component count
    BufDataFormat   compDfmt;       // Equivalent data format of each component
};

// =====================================================================================================================
// Represents the manager of vertex fetch operations.
class VertexFetch
{
public:
    VertexFetch(llvm::Module* pModule);

    static const VertexFormatInfo* GetVertexFormatInfo(VkFormat format);

    llvm::Value* Run(llvm::Type* pInputTy, uint32_t location, llvm::Instruction* pInsertPos);

    // Gets variable corresponding to vertex index
    llvm::Value* GetVertexIndex() { return m_pVertexIndex; }

    // Gets variable corresponding to instance index
    llvm::Value* GetInstanceIndex() { return m_pInstanceIndex; }

private:
    LLPC_DISALLOW_DEFAULT_CTOR(VertexFetch);
    LLPC_DISALLOW_COPY_AND_ASSIGN(VertexFetch);

    static const VertexCompFormatInfo* GetVertexComponentFormatInfo(uint32_t dfmt);

    llvm::Value* LoadVertexBufferDescriptor(uint32_t binding, llvm::Instruction* pInsertPos) const;

    void ExtractVertexInputInfo(uint32_t                                  location,
                                const VkVertexInputBindingDescription**   ppBinding,
                                const VkVertexInputAttributeDescription** ppAttrib) const;

    void AddVertexFetchInst(llvm::Value*       pVbDesc,
                            uint32_t           numChannels,
                            llvm::Value*       pVbIndex,
                            uint32_t           offset,
                            uint32_t           stride,
                            uint32_t           dfmt,
                            uint32_t           nfmt,
                            llvm::Instruction* pInsertPos,
                            llvm::Value**      ppFetch) const;

    bool NeedPostShuffle(VkFormat format, std::vector<llvm::Constant*>& shuffleMask) const;

    bool NeedPatchA2S(VkFormat format) const;

    bool NeedSecondVertexFetch(VkFormat format) const;

    // -----------------------------------------------------------------------------------------------------------------

    llvm::Module*   m_pModule;          // LLVM module
    Context*        m_pContext;         // LLPC context

    const VkPipelineVertexInputStateCreateInfo*   m_pVertexInput; // Vertex input info

    llvm::Value*    m_pVertexIndex;     // Vertex index
    llvm::Value*    m_pInstanceIndex;   // Instance index

    static const VertexFormatInfo       m_vertexFormatInfo[];     // Info table of vertex format
    static const VertexCompFormatInfo   m_vertexCompFormatInfo[]; // Info table of vertex component format

    // Default values for vertex fetch (<4 x i32> or <8 x i32>)
    struct
    {
        llvm::Constant*   pInt;       // < 0, 0, 0, 1 >
        llvm::Constant*   pInt64;     // < 0, 0, 0, 0, 0, 0, 0, 1 >
        llvm::Constant*   pFloat;     // < 0, 0, 0, 0x3F800000 >
        llvm::Constant*   pDouble;    // < 0, 0, 0, 0, 0, 0, 0, 0x3FF00000 >
    } m_fetchDefaults;
};

} // Llpc
