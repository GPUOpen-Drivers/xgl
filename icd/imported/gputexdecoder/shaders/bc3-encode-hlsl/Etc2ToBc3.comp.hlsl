//===============================================================================
// Copyright (c) 2022    Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//===============================================================================
#ifndef ASPM_HLSL
#define ASPM_HLSL
#endif

#define CMP_USE_LOWQUALITY

#include "bcn_common_kernel.h"

#define MAX_USED_THREAD     16  // pixels in a BC (block compressed) block
#define BLOCK_IN_GROUP      4   // the number of BC blocks a thread group processes = 64 / 16 = 4
#define THREAD_GROUP_SIZE   64  // 4 blocks

static const int4 etc2_alpha_modifier_table[16] = {
    int4(2, 5, 8, 14),
    int4(2, 6, 9, 12),
    int4(1, 4, 7, 12),
    int4(1, 3, 5, 12),
    int4(2, 5, 7, 11),
    int4(2, 6, 8, 10),
    int4(3, 6, 7, 10),
    int4(2, 4, 7, 10),
    int4(1, 5, 7, 9),
    int4(1, 4, 7, 9),
    int4(1, 3, 7, 9),
    int4(1, 4, 6, 9),
    int4(2, 3, 6, 9),
    int4(0, 1, 2, 9),
    int4(3, 5, 7, 8),
    int4(2, 4, 6, 8)
};

static const int etc2_distance_table[8] = { 3, 6, 11, 16, 23, 32, 41, 64 };

static const int2 etc1_color_modifier_table[8] = {
    int2(2, 8),
    int2(5, 17),
    int2(9, 29),
    int2(13, 42),
    int2(18, 60),
    int2(24, 80),
    int2(33, 106),
    int2(47, 183)
};

// Compressed Output Data
RWTexture2D<uint4> uOutput    : register( u0 );
Texture2D<uint4> uInput       : register( t1 );
Buffer<uint4> etc2SrcBuffer2D : register( t2 );


struct BufferCopyData {
    int4 offset;
    int4 extent;
    int4 pitch;
};

struct ImageCopyData {
    int4 srcOffset;
    int4 dstOffset;
    int4 extent;
};

[[vk::push_constant]]
cbuffer constants {
    BufferCopyData copyData_bufferData : packoffset(c0);
    ImageCopyData copyData_imageData   : packoffset(c3);
    int copyData_alphaBits             : packoffset(c6);
    int copyData_eacComps              : packoffset(c6.y);
    int copyData_eacSignedFlag         : packoffset(c6.z);
    uint copyData_isBufferSrc          : packoffset(c6.w);
};

uint bitfieldUExtract(uint Base, uint Offset, uint Count) {
    uint Mask = Count == 32 ? 0xffffffff : ((1 << Count) - 1);
    return (Base >> Offset) & Mask;
}

int bitfieldSExtract(int Base, int Offset, int Count)
{
    int Mask = Count == 32 ? -1 : ((1 << Count) - 1);
    int Masked = (Base >> Offset) & Mask;
    int ExtendShift = (32 - Count) & 31;
    return (Masked << ExtendShift) >> ExtendShift;
}

uint flip_endian(uint v)
{
    uint4 words = v.xxxx >> uint4(0u, 8u, 16u, 24u);
    words &= uint4(255u, 255u, 255u, 255u);
    return (((words.x << 24u) | (words.y << 16u)) | (words.z << 8u)) | (words.w << 0u);
}

uint2 flip_endian(uint2 v)
{
    uint param = v.y;
    uint param_1 = v.x;
    return uint2(flip_endian(param), flip_endian(param_1));
}

int decode_eac_signed(uint2 payload, int linear_pixel)
{
    int bit_offset = 45 - (3 * linear_pixel);
    int temp = int(payload.y);
    int base = bitfieldSExtract(temp, 24, 8) * 8;
    int multiplier = max((int(bitfieldUExtract(payload.y, 20, 4)) * 8), 1);
    int table = int(bitfieldUExtract(payload.y, 16, 4));
    int lsb_index = int(bitfieldUExtract(payload[bit_offset >> 5], bit_offset & 31, 2));
    bit_offset += 2;
    int msb = int((payload[bit_offset >> 5] >> uint(bit_offset & 31)) & 1u);
    int _mod = etc2_alpha_modifier_table[table][lsb_index] ^ (msb - 1);
    int a = base + (_mod * multiplier);
    a += 1023;
    return clamp(a, 0, 2047);
}

uint decode_eac(uint2 payload, int linear_pixel)
{
    int bit_offset = 45 - (3 * linear_pixel);
    int base = (int(bitfieldUExtract(payload.y, 24, 8)) * 8) + 4;
    int multiplier = max((int(bitfieldUExtract(payload.y, 20, 4)) * 8), 1);
    int table = int(bitfieldUExtract(payload.y, 16, 4));
    int lsb_index = int(bitfieldUExtract(payload[bit_offset >> 5], bit_offset & 31, 2));
    bit_offset += 2;
    int msb = int((payload[bit_offset >> 5] >> uint(bit_offset & 31)) & 1u);
    int _mod = etc2_alpha_modifier_table[table][lsb_index] ^ (msb - 1);
    int a = base + (_mod * multiplier);
    return uint(clamp(a, 0, 2047));
}

uint decode_etc2_alpha(uint2 payload, int linear_pixel)
{
    int bit_offset = 45 - (3 * linear_pixel);
    int base = int(bitfieldUExtract(payload.y, 24, 8));
    int multiplier = int(bitfieldUExtract(payload.y, 20, 4));
    int table = int(bitfieldUExtract(payload.y, 16, 4));
    int lsb_index = int(bitfieldUExtract(payload[bit_offset >> 5], bit_offset & 31, 2));
    bit_offset += 2;
    int msb = int((payload[bit_offset >> 5] >> uint(bit_offset & 31)) & 1u);
    int _mod = etc2_alpha_modifier_table[table][lsb_index] ^ (msb - 1);
    int a = base + (_mod * multiplier);
    return uint(clamp(a, 0, 255));
}

groupshared float4 shared_temp[THREAD_GROUP_SIZE];

[numthreads(4, 4, 4)]
void TransformEtc2ToBc3(uint GI : SV_GroupIndex, uint3 groupID : SV_GroupID, uint3 threadID : SV_GroupThreadID)
{
    int2 coord = int2(groupID.xy) * int2(8, 8);
    coord.x += (4 * (int(threadID.z) & 1));
    coord.y += (2 * (int(threadID.z) & 2));
    coord   += int2(threadID.xy);
    int2 tile_coord  = coord >> int2(2, 2);
    int2 pixel_coord = coord & int2(3, 3);
    int linear_pixel = (4 * pixel_coord.x) + pixel_coord.y;

    if (copyData_isBufferSrc != 0u)
    {
        if (any(bool2(tile_coord.x >= copyData_bufferData.extent.x, tile_coord.y >= copyData_bufferData.extent.y)))
        {
            return;
        }
    }
    else
    {
        if (any(bool2(tile_coord.x >= copyData_imageData.extent.x, tile_coord.y >= copyData_imageData.extent.y)))
        {
            return;
        }
    }

    uint4 payload;

    if (copyData_isBufferSrc != 0u)
    {
        int p = (tile_coord.y * copyData_bufferData.pitch.x) + tile_coord.x;
        payload = etc2SrcBuffer2D.Load(p);
    }
    else
    {
        payload = uInput.Load(int3(tile_coord + copyData_imageData.srcOffset.xy, 0));
    }

    float4 pixelColor;

    //EAC path
    if (copyData_eacComps != 0)
    {
        if (copyData_eacSignedFlag == 1)
        {
            uint2 param = payload.xy;
            uint2 param_1 = flip_endian(param);
            int param_2 = linear_pixel;
            int r = decode_eac_signed(param_1, param_2);
            if (copyData_eacComps == 1)
            {
                pixelColor = float4((float(r) / 1023.0f) - 1.0f, 0.0f, 0.0f, 1.0f);
            }
            else
            {
                if (copyData_eacComps == 2)
                {
                    uint2 param_3 = payload.zw;
                    uint2 param_4 = flip_endian(param_3);
                    int param_5 = linear_pixel;
                    int g = decode_eac_signed(param_4, param_5);
                    pixelColor = float4((float(r) / 1023.0f) - 1.0f, (float(g) / 1023.0f) - 1.0f, 0.0f, 1.0f);
                }
            }
        }
        else
        {
            if (copyData_eacSignedFlag == 0)
            {
                uint2 param_6 = payload.xy;
                uint2 param_7 = flip_endian(param_6);
                int param_8 = linear_pixel;
                uint r_1 = decode_eac(param_7, param_8);
                if (copyData_eacComps == 1)
                {
                    pixelColor = float4(float(r_1) / 2047.0f, 0.0f, 0.0f, 1.0f);
                }
                else
                {
                    if (copyData_eacComps == 2)
                    {
                        uint2 param_9 = payload.zw;
                        uint2 param_10 = flip_endian(param_9);
                        int param_11 = linear_pixel;
                        uint g_1 = decode_eac(param_10, param_11);
                        pixelColor = float4(float(r_1) / 2047.0f, float(g_1) / 2047.0f, 0.0f, 1.0f);
                    }
                }
            }
        }
    }
    else // Etc2/1 path
    {
        uint alpha_result;
        uint2 color_payload;
        if (copyData_alphaBits == 8)
        {
            uint2 param_12 = payload.xy;
            uint2 alpha_payload = flip_endian(param_12);
            uint2 param_13 = alpha_payload;
            int param_14 = linear_pixel;
            alpha_result = decode_etc2_alpha(param_13, param_14);
            uint2 param_15 = payload.zw;
            color_payload = flip_endian(param_15);
        }
        else
        {
            uint2 param_16 = payload.xy;
            color_payload = flip_endian(param_16);
            alpha_result = 255u;
        }
        uint flip = color_payload.y & 1u;
        uint subblock = (uint(pixel_coord[flip]) & 2u) >> 1u;
        bool etc1_compat = false;
        bool punchthrough;

        if (copyData_alphaBits == 1)
        {
            punchthrough = (color_payload.y & 2u) == 0u;
        }
        else
        {
            punchthrough = false;
        }

        bool _602 = copyData_alphaBits != 1;
        bool _609;
        if (_602)
        {
            _609 = (color_payload.y & 2u) == 0u;
        }
        else
        {
            _609 = _602;
        }
        int3 base_rgb;
        uint3 rgb_result;
        if (_609)
        {
            etc1_compat = true;
            base_rgb = int3(color_payload.yyy >> (uint3(28u, 20u, 12u) - (4u * subblock).xxx));
            base_rgb &= int3(15, 15, 15);
            base_rgb *= int3(17, 17, 17);
        }
        else
        {
            int r_2 = int(bitfieldUExtract(color_payload.y, 27, 5));
            int rd = bitfieldSExtract(int(color_payload.y), 24, 3);
            int g_2 = int(bitfieldUExtract(color_payload.y, 19, 5));
            int gd = bitfieldSExtract(int(color_payload.y), 16, 3);
            int b = int(bitfieldUExtract(color_payload.y, 11, 5));
            int bd = bitfieldSExtract(int(color_payload.y), 8, 3);
            int r1 = r_2 + rd;
            int g1 = g_2 + gd;
            int b1 = b + bd;
            if (uint(r1) > 31u)
            {
                int r1_1 = int(bitfieldUExtract(color_payload.y, 24, 2)) | (int(bitfieldUExtract(color_payload.y, 27, 2)) << 2);
                int g1_1 = int(bitfieldUExtract(color_payload.y, 20, 4));
                int b1_1 = int(bitfieldUExtract(color_payload.y, 16, 4));
                int r2 = int(bitfieldUExtract(color_payload.y, 12, 4));
                int g2 = int(bitfieldUExtract(color_payload.y, 8, 4));
                int b2 = int(bitfieldUExtract(color_payload.y, 4, 4));
                uint da = (bitfieldUExtract(color_payload.y, 2, 2) << uint(1)) | (color_payload.y & 1u);
                int dist = etc2_distance_table[da];
                int msb = int((color_payload.x >> uint(15 + linear_pixel)) & 2u);
                int lsb = int((color_payload.x >> uint(linear_pixel)) & 1u);
                int index = msb | lsb;
                if (punchthrough)
                {
                    punchthrough = index == 2;
                }
                if (index == 0)
                {
                    rgb_result = uint3(uint(r1_1), uint(g1_1), uint(b1_1));
                    rgb_result *= uint3(17u, 17u, 17u);
                }
                else
                {
                    int _mod = 2 - index;
                    int3 rgb = (int3(r2, g2, b2) * int3(17, 17, 17)) + (_mod * dist).xxx;
                    rgb_result = uint3(clamp(rgb, int3(0, 0, 0), int3(255, 255, 255)));
                }
            }
            else
            {
                if (uint(g1) > 31u)
                {
                    int r1_2 = int(bitfieldUExtract(color_payload.y, 27, 4));
                    int g1_2 = (int(bitfieldUExtract(color_payload.y, 24, 3)) << 1) | int((color_payload.y >> 20u) & 1u);
                    int b1_2 = int(bitfieldUExtract(color_payload.y, 15, 3)) | int((color_payload.y >> 16u) & 8u);
                    int r2_1 = int(bitfieldUExtract(color_payload.y, 11, 4));
                    int g2_1 = int(bitfieldUExtract(color_payload.y, 7, 4));
                    int b2_1 = int(bitfieldUExtract(color_payload.y, 3, 4));
                    uint da_1 = color_payload.y & 4u;
                    uint db = color_payload.y & 1u;
                    uint d = da_1 + (2u * db);
                    d += uint((((r1_2 * 65536) + (g1_2 * 256)) + b1_2) >= (((r2_1 * 65536) + (g2_1 * 256)) + b2_1));
                    int dist_1 = etc2_distance_table[d];
                    int msb_1 = int((color_payload.x >> uint(15 + linear_pixel)) & 2u);
                    int lsb_1 = int((color_payload.x >> uint(linear_pixel)) & 1u);
                    if (punchthrough)
                    {
                        punchthrough = (msb_1 + lsb_1) == 2;
                    }
                    int3 _919;
                    if (msb_1 != 0)
                    {
                        _919 = int3(r2_1, g2_1, b2_1);
                    }
                    else
                    {
                        _919 = int3(r1_2, g1_2, b1_2);
                    }
                    int3 base = _919;
                    base *= int3(17, 17, 17);
                    int _mod_1 = 1 - (2 * lsb_1);
                    base += (_mod_1 * dist_1).xxx;
                    rgb_result = uint3(clamp(base, int3(0, 0, 0), int3(255, 255, 255)));
                }
                else
                {
                    if (uint(b1) > 31u)
                    {
                        int r_3 = int(bitfieldUExtract(color_payload.y, 25, 6));
                        int g_3 = int(bitfieldUExtract(color_payload.y, 17, 6)) | (int(color_payload.y >> uint(18)) & 64);
                        int b_1 = (int(bitfieldUExtract(color_payload.y, 7, 3)) | (int(bitfieldUExtract(color_payload.y, 11, 2)) << 3)) | (int(color_payload.y >> uint(11)) & 32);
                        int rh = int(color_payload.y & 1u) | (int(bitfieldUExtract(color_payload.y, 2, 5)) << 1);
                        int rv = int(bitfieldUExtract(color_payload.x, 13, 6));
                        int gh = int(bitfieldUExtract(color_payload.x, 25, 7));
                        int gv = int(bitfieldUExtract(color_payload.x, 6, 7));
                        int bh = int(bitfieldUExtract(color_payload.x, 19, 6));
                        int bv = int(bitfieldUExtract(color_payload.x, 0, 6));
                        r_3 = (r_3 << 2) | (r_3 >> 4);
                        rh = (rh << 2) | (rh >> 4);
                        rv = (rv << 2) | (rv >> 4);
                        g_3 = (g_3 << 1) | (g_3 >> 6);
                        gh = (gh << 1) | (gh >> 6);
                        gv = (gv << 1) | (gv >> 6);
                        b_1 = (b_1 << 2) | (b_1 >> 4);
                        bh = (bh << 2) | (bh >> 4);
                        bv = (bv << 2) | (bv >> 4);
                        int3 rgb_1 = int3(r_3, g_3, b_1);
                        int3 dx = int3(rh, gh, bh) - rgb_1;
                        int3 dy = int3(rv, gv, bv) - rgb_1;
                        dx *= pixel_coord.x.xxx;
                        dy *= pixel_coord.y.xxx;
                        rgb_1 += (((dx + dy) + int3(2, 2, 2)) >> int3(2, 2, 2));
                        rgb_1 = clamp(rgb_1, int3(0, 0, 0), int3(255, 255, 255));
                        rgb_result = uint3(rgb_1);
                        punchthrough = false;
                    }
                    else
                    {
                        etc1_compat = true;
                        base_rgb = int3(r_2, g_2, b) + (int(subblock).xxx * int3(rd, gd, bd));
                        base_rgb = (base_rgb << int3(3, 3, 3)) | (base_rgb >> int3(2, 2, 2));
                    }
                }
            }
        }
        if (etc1_compat)
        {
            uint etc1_table_index = bitfieldUExtract(color_payload.y, 5 - (3 * int(subblock != 0u)), 3);
            int msb_2 = int((color_payload.x >> uint(15 + linear_pixel)) & 2u);
            int lsb_2 = int((color_payload.x >> uint(linear_pixel)) & 1u);
            int sgn = 1 - msb_2;
            if (punchthrough)
            {
                sgn *= lsb_2;
                punchthrough = (msb_2 + lsb_2) == 2;
            }
            int offset = etc1_color_modifier_table[etc1_table_index][lsb_2] * sgn;
            base_rgb = clamp(base_rgb + offset.xxx, int3(0, 0, 0), int3(255, 255, 255));
            rgb_result = uint3(base_rgb);
        }
        if ((copyData_alphaBits == 1) && punchthrough)
        {
            rgb_result = uint3(0u, 0u, 0u);
            alpha_result = 0u;
        }
        pixelColor = float4(float3(rgb_result), float(alpha_result)) / 255.0f.xxxx;
    }

    // we process 4 BC blocks per thread group
    uint blockInGroup   = GI / MAX_USED_THREAD;                // what BC block this thread is on within this thread group
    uint pixelBase      = blockInGroup * MAX_USED_THREAD;      // the first id of the pixel in this BC block in this thread group
    uint pixelInBlock   = GI - pixelBase;                      // id of the pixel in this BC block

    if (pixelInBlock < 16)
    {
        // load pixels (0..1)
        shared_temp[GI] = pixelColor;
    }

    GroupMemoryBarrierWithGroupSync();

    // Process and save
    if (pixelInBlock == 0)
    {
        float3 blockRGB[16];
        float  blockA[16];
        for (int i = 0; i < 16; i++ )
        {
            blockRGB[i].x   = shared_temp[pixelBase + i].x;
            blockRGB[i].y   = shared_temp[pixelBase + i].y;
            blockRGB[i].z   = shared_temp[pixelBase + i].z;
            blockA[i]       = shared_temp[pixelBase + i].w;
        }

        int2 offset;

        if (copyData_isBufferSrc != 0u)
        {
            offset = copyData_bufferData.offset.xy;
        }
        else
        {
            offset = copyData_imageData.dstOffset.xy;

        }

        uOutput[tile_coord + offset] = CompressBlockBC3_UNORM(blockRGB,blockA, 0.5f, false);
    }
}
