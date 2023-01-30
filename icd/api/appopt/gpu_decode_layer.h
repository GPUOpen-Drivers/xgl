/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  gpu_decode_layer.h
* @brief Declaration of gpu decode layer for compressed texture/image
***********************************************************************************************************************
*/

#if VKI_GPU_DECOMPRESS
#ifndef __GPU_DECODE_LAYER_H
#define __GPU_DECODE_LAYER_H

#pragma once
#include "palImage.h"
#include "palHashMapImpl.h"
#include "palHashSetImpl.h"
#include "opt_layer.h"
#include "include/vk_alloccb.h"
#include "include/vk_conv.h"
#include "include/vk_private_data_slot.h"

#include "gpuTexDecoder.h"

namespace vk
{
struct StagingResourcePair
{
    VkImage image;
    VkDeviceMemory memory;
};

// =====================================================================================================================
// GPU Decoder  Wrapper
class GpuDecoderLayer final : public OptLayer
{
public:
    GpuDecoderLayer(Device* pDevice);
    ~GpuDecoderLayer();
    VkResult Init(Device* pDevice);

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;
    GpuTexDecoder::Device* GetTexDecoder()
    {
        return m_pGpuTexDecoder;
    }

    bool IsAstcSrgbaFormat(VkFormat format)
    {
         return Formats::IsASTCFormat(format) &&
            (static_cast<uint32_t>(format) % 2 == 0);
    }

    VkImage CreateStagingImage(Device* device, VkImage dstImage);
    void ClearStagingResources(VkImage image);

    typedef Util::HashMap<VkImage, StagingResourcePair, PalAllocator> ImageResourcePairMap;
    typedef Util::HashSet<VkImage, PalAllocator> DecodedImagesSet;

    void AddDecodedImage(VkImage image) { m_decodedImages.Insert(image); }
    bool IsImageDecoded(VkImage image) { return m_decodedImages.Contains(image); }
    bool RemoveDecodedImage(VkImage image) { return m_decodedImages.Erase(image); }

private:
    Device*                   m_pDevice;
    GpuTexDecoder::Device*    m_pGpuTexDecoder;
    ImageResourcePairMap      m_cachedStagingRes;
    DecodedImagesSet          m_decodedImages;

    uint32_t FindMemoryType(Device* device, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuDecoderLayer);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECODER_WAPPER_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::gpuDecoderWapper::func_name;

#define DECODER_WAPPER_OVERRIDE_ENTRY(entry_name) DECODER_WAPPER_OVERRIDE_ALIAS(entry_name, entry_name)

#define DECODER_WAPPER_CALL_NEXT_LAYER(entry_name) \
    pDecodeWrapper->GetNextLayer()->GetEntryPoints().entry_name

} // namespace vk
#endif /* __GPU_DECODE_LAYER_H */
#endif /* VKI_GPU_DECOMPRESS */
