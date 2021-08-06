/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  gpuTexDecoder.h
 * @brief Contains declaration of gpuTexDecoder classes.
 ***********************************************************************************************************************
 */
#pragma once
#include "pal.h"
#include "palSysMemory.h"
#include "palGpuMemory.h"
#include "palDevice.h"
#include "palPipeline.h"
#include "palMutex.h"
#include "palCmdBuffer.h"
#include "palPlatform.h"

namespace Pal
{
class IDevice;
class IPipeline;
class ICmdBuffer;
class IPlatform;
struct DeviceProperties;
};

namespace GpuTexDecoder
{
using Pal::uint8;
using Pal::uint16;
using Pal::uint32;
using Pal::uint64;

constexpr uint32 AstcInternalPipelineNodes = 6;

// Enum for internal texture format convert type
enum class InternalTexConvertCsType : uint32
{
     ConvertASTCToRGBA8,
     ConvertETC2ToRGBA8,
     Count
};

// Information to initialize a gpuTexDecoder device
struct DeviceInitInfo
{
    uint32            gpuIdx;                       // Client GPU index associated with this gpurt device
    void*             pClientUserData;              // User data pointer passed to internal pipeline create/destroy callbacks
    Pal::IDevice*     pPalDevice;
    Pal::IPlatform*   pPlatform;
    const Pal::DeviceProperties* pDeviceProperties; // Pointer to host PAL device properties
                                                    // (this pointer is retained).
};

struct CompileTimeConstants
{
    const uint32* pConstants;
    uint32        numConstants;
};

struct PipelineShaderCode
{
    const void* pSpvCode;   // Code in SPIR-V form
    uint32      spvSize;    // Size in bytes of SPIR-V code
};

enum class NodeType : uint32
{
    Buffer,
    TexBuffer,
    Image,
    Count
};

struct GpuDecodeMappingNode
{
    NodeType nodeType;
    uint32   binding;
    uint32   set;
    uint32   offsetInDwords;
    uint32   sizeInDwords;
};

struct PipelineBuildInfo
{
    GpuDecodeMappingNode*                pUserDataNodes;
    uint32                               nodeCount;
    PipelineShaderCode                   code;
    InternalTexConvertCsType             shaderType;
};

// Client-provided callback to build an internal compute pipeline.  This is called by gpuTexDecoder during initialization
// of a gpuTexDecoder device.
//
// The client must implement this function to successfully initialize gpuTexDecoder.
//
// @param initInfo         [in]  Information about the host device
// @param ppResultPipeline [out] Result PAL pipeline object pointer
// @param ppResultMemory   [out] Result PAL pipeline memory if different from pipeline pointer.  Optional.
//
// @returns Compilation success result.
extern Pal::Result ClientCreateInternalComputePipeline(
    const DeviceInitInfo&             initInfo,          // Information about the host device
    const CompileTimeConstants&       constInfo,         // CompileTime Specialization params
    const PipelineBuildInfo&          buildInfo,         // Pipeline Layout, shader code..
    Pal::IPipeline**                  ppResultPipeline,  // Result PAL pipeline object pointer
    void**                            ppResultMemory);   // (Optional) Result PAL pipeline memory, if different from obj

// =====================================================================================================================
// GPUTEXDECODER device
//
class Device
{
public:
    Device();
    ~Device();

    // Initializes the device
    //
    // @param info [in] Information required to initialize the device
    void Init(const DeviceInitInfo& info);

    Pal::Result GpuDecodeImage(
        InternalTexConvertCsType    type,
        Pal::ICmdBuffer*            pCmdBuffer,
        const Pal::IImage*          pSrcImage,
        const Pal::IImage*          pDstImage,
        uint32                      regionCount,
        Pal::ImageCopyRegion*       pPalImageRegions,
        const CompileTimeConstants& constInfo);

    Pal::Result GpuDecodeBuffer(
        InternalTexConvertCsType    type,
        Pal::ICmdBuffer*            pCmdBuffer,
        const Pal::IGpuMemory*      pSrcBufferMem,
        Pal::IImage*                pDstImage,
        uint32                      regionCount,
        Pal::MemoryImageCopyRegion* pPalBufferRegionsIn,
        const CompileTimeConstants& constInfo);

private:
    void CreateAstcUserData(
        InternalTexConvertCsType    type,
        uint32**                    ppUserData,
        uint32                      srdDwords);

    void BindPipeline(
        InternalTexConvertCsType    type,
        const CompileTimeConstants& constInfo);

    Pal::Result CreateTableMemory();

    Pal::Result SetupInternalTables(
        InternalTexConvertCsType    type,
        uint32**                    ppUserData);

    Pal::Result CreateGpuMemory(
        Pal::GpuMemoryRequirements* pMemReqs,
        Pal::IGpuMemory**           ppGpuMemory,
        Pal::gpusize*               pOffset);

    void CreateMemoryReqs(
        uint32                      bytesSize,
        uint32                      alignment,
        Pal::GpuMemoryRequirements* pMemReqs);

    uint32* CreateAndBindEmbeddedUserData(
        Pal::ICmdBuffer*  pCmdBuffer,
        uint32            sizeInDwords,
        uint32            entryToBind,
        uint32            bindNum) const;

    void BuildBufferViewInfo(
        uint32*                   pData,
        uint8                     count,
        Pal::gpusize              addr,
        Pal::gpusize              dataBytes,
        uint8                     stride,
        Pal::SwizzledFormat       swizzleFormat) const;

   void BuildImageViewInfo(
        Pal::ImageViewInfo*  pInfo,
        const Pal::IImage*   pImage,
        const Pal::SubresId& subresId,
        Pal::SwizzledFormat  swizzledFormat,
        bool                 isShaderWriteable) const;

    Pal::IPipeline* GetInternalPipeline(
        InternalTexConvertCsType    type,
        const CompileTimeConstants& constInfo) const;

    DeviceInitInfo      m_info;
    Pal::IGpuMemory*    m_pTableMemory;
    Pal::ICmdBuffer*    m_pPalCmdBuffer;      // The associated PAL cmdbuffer
    uint32              m_bufferViewSizeInDwords{0};
    uint32              m_imageViewSizeInDwords{0};
    uint32              m_srdDwords;
};
}
