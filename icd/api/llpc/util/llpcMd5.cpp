/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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
 * @file  llpcMd5.cpp
 * @breif LLPC source file: contains implementation of utility collection MD5.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-md5"

#include "llpcDebug.h"
#include "llpcInternal.h"
#include "llpcMd5.h"

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

// This is the central step in the MD5 algorithm.
#define MD5STEP(f, w, x, y, z, data, s) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

namespace Llpc
{

namespace Md5
{

static void Transform(uint32_t* pBuf, uint32_t* pIn);
static void ByteReverse(uint8_t* pBuf, uint32_t longs);

// =====================================================================================================================
// Generates a checksum on the specified buffer using the MD5 algorithm.
Hash GenerateHashFromBuffer(
    const void*  pBuffer,     // [in] pBuffer Buffer in memory to be hashed
    size_t       bufLen)      // Size of pBuffer, in bytes
{
    Context ctx;
    Init(&ctx);

    Update(&ctx, static_cast<const uint8_t*>(pBuffer), bufLen);

    Hash hash;
    Final(&ctx, &hash);

    return hash;
}

// =====================================================================================================================
// Initializes the context for the MD5 algorithm using some magic values.
//
// NOTE: Must be called before Update() or Final().
void Init(
    Context* pCtx) // [in] MD5 context to be initialized
{
    pCtx->buf[0] = 0x67452301;
    pCtx->buf[1] = 0xEFCDAB89;
    pCtx->buf[2] = 0x98BADCFE;
    pCtx->buf[3] = 0x10325476;

    pCtx->bits[0] = 0;
    pCtx->bits[1] = 0;
}

// =====================================================================================================================
// Updates the context to reflect the concatenation of another buffer full of data for the algorithm.
void Update(
    Context*       pCtx,       // [in,out] MD5 context to be updated
    const void*    pBuf,       // [in] Buffer in memory to be hashed in the MD5 context
    size_t         bufLen)     // Size of pBuf, in bytes
{
    bool breakVal = false;

    uint32_t value = pCtx->bits[0];
    pCtx->bits[0] = value + static_cast<uint32_t>(bufLen << 3);
    if (pCtx->bits[0] < value)
    {
        pCtx->bits[1]++;
    }
    pCtx->bits[1] += static_cast<uint32_t>(bufLen >> 29);

    value = (value >> 3) & 0x3F;

    if (value)
    {
        uint8_t* pWorkingBuffer = static_cast<uint8_t*>(pCtx->in + value);

        value = 64 - value;

        if (bufLen < value)
        {
            memcpy(pWorkingBuffer, pBuf, bufLen);
            breakVal = true;
        }
        else
        {
            memcpy(pWorkingBuffer, pBuf, value);
            ByteReverse(&pCtx->in[0], 16);
            Transform(&pCtx->buf[0], reinterpret_cast<uint32_t*>(&pCtx->in[0]));
            pBuf = VoidPtrInc(pBuf, value);
            bufLen -= value;
        }
    }

    if (breakVal == false)
    {
        while (bufLen >= 64)
        {
            memcpy(&pCtx->in[0], pBuf, 64);
            ByteReverse(&pCtx->in[0], 16);
            Transform(&pCtx->buf[0], reinterpret_cast<uint32_t*>(&pCtx->in[0]));
            pBuf = VoidPtrInc(pBuf, 64);
            bufLen -= 64;
        }

        memcpy(&pCtx->in[0], pBuf, bufLen);
    }
}

// =====================================================================================================================
// Finalizes the context and outputs the checksum.
void Final(
    Context* pCtx,  // [in] MD5 context that has been accumulating a hash via calls to Update()
    Hash*    pHash) // [out] 128-bit hash value
{
    uint32_t count = (pCtx->bits[0] >> 3) & 0x3F;

    uint8_t* pBuf = pCtx->in + count;
    *pBuf++ = 0x80;

    count = 64 - 1 - count;

    if (count < 8)
    {
        memset(pBuf, 0, count);
        ByteReverse(&pCtx->in[0], 16);
        Transform(&pCtx->buf[0], reinterpret_cast<uint32_t*>(&pCtx->in[0]));
        memset(&pCtx->in[0], 0, 56);
    }
    else
    {
        memset(pBuf, 0, count - 8);
    }

    ByteReverse(&pCtx->in[0], 14);

    reinterpret_cast<uint32_t*>(&pCtx->in[0])[14] = pCtx->bits[0];
    reinterpret_cast<uint32_t*>(&pCtx->in[0])[15] = pCtx->bits[1];

    Transform(&pCtx->buf[0], reinterpret_cast<uint32_t*>(&pCtx->in[0]));
    ByteReverse(reinterpret_cast<uint8_t*>(&pCtx->buf[0]), 4);
    memcpy(pHash->hashValue, &pCtx->buf[0], 16);
}

// =====================================================================================================================
// Performs the actual checksumming on the input data.
void Transform(
    uint32_t*  pBuf,  // [in,out] Working buffer
    uint32_t*  pIn)   // [in] Buffer of the current checksum information
{
    uint32_t a = pBuf[0];
    uint32_t b = pBuf[1];
    uint32_t c = pBuf[2];
    uint32_t d = pBuf[3];

    MD5STEP(F1, a, b, c, d, pIn[0] + 0xD76AA478, 7);
    MD5STEP(F1, d, a, b, c, pIn[1] + 0xE8C7B756, 12);
    MD5STEP(F1, c, d, a, b, pIn[2] + 0x242070DB, 17);
    MD5STEP(F1, b, c, d, a, pIn[3] + 0xC1BDCEEE, 22);
    MD5STEP(F1, a, b, c, d, pIn[4] + 0xF57C0FAF, 7);
    MD5STEP(F1, d, a, b, c, pIn[5] + 0x4787C62A, 12);
    MD5STEP(F1, c, d, a, b, pIn[6] + 0xA8304613, 17);
    MD5STEP(F1, b, c, d, a, pIn[7] + 0xFD469501, 22);
    MD5STEP(F1, a, b, c, d, pIn[8] + 0x698098D8, 7);
    MD5STEP(F1, d, a, b, c, pIn[9] + 0x8B44F7AF, 12);
    MD5STEP(F1, c, d, a, b, pIn[10] + 0xFFFF5BB1, 17);
    MD5STEP(F1, b, c, d, a, pIn[11] + 0x895CD7BE, 22);
    MD5STEP(F1, a, b, c, d, pIn[12] + 0x6B901122, 7);
    MD5STEP(F1, d, a, b, c, pIn[13] + 0xFD987193, 12);
    MD5STEP(F1, c, d, a, b, pIn[14] + 0xA679438E, 17);
    MD5STEP(F1, b, c, d, a, pIn[15] + 0x49B40821, 22);

    MD5STEP(F2, a, b, c, d, pIn[1] + 0xF61E2562, 5);
    MD5STEP(F2, d, a, b, c, pIn[6] + 0xC040B340, 9);
    MD5STEP(F2, c, d, a, b, pIn[11] + 0x265E5A51, 14);
    MD5STEP(F2, b, c, d, a, pIn[0] + 0xE9B6C7AA, 20);
    MD5STEP(F2, a, b, c, d, pIn[5] + 0xD62F105D, 5);
    MD5STEP(F2, d, a, b, c, pIn[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, pIn[15] + 0xD8A1E681, 14);
    MD5STEP(F2, b, c, d, a, pIn[4] + 0xE7D3FBC8, 20);
    MD5STEP(F2, a, b, c, d, pIn[9] + 0x21E1CDE6, 5);
    MD5STEP(F2, d, a, b, c, pIn[14] + 0xC33707D6, 9);
    MD5STEP(F2, c, d, a, b, pIn[3] + 0xF4D50D87, 14);
    MD5STEP(F2, b, c, d, a, pIn[8] + 0x455A14ED, 20);
    MD5STEP(F2, a, b, c, d, pIn[13] + 0xA9E3E905, 5);
    MD5STEP(F2, d, a, b, c, pIn[2] + 0xFCEFA3F8, 9);
    MD5STEP(F2, c, d, a, b, pIn[7] + 0x676F02D9, 14);
    MD5STEP(F2, b, c, d, a, pIn[12] + 0x8D2A4C8A, 20);

    MD5STEP(F3, a, b, c, d, pIn[5] + 0xFFFA3942, 4);
    MD5STEP(F3, d, a, b, c, pIn[8] + 0x8771F681, 11);
    MD5STEP(F3, c, d, a, b, pIn[11] + 0x6D9D6122, 16);
    MD5STEP(F3, b, c, d, a, pIn[14] + 0xFDE5380C, 23);
    MD5STEP(F3, a, b, c, d, pIn[1] + 0xA4BEEA44, 4);
    MD5STEP(F3, d, a, b, c, pIn[4] + 0x4BDECFA9, 11);
    MD5STEP(F3, c, d, a, b, pIn[7] + 0xF6BB4B60, 16);
    MD5STEP(F3, b, c, d, a, pIn[10] + 0xBEBFBC70, 23);
    MD5STEP(F3, a, b, c, d, pIn[13] + 0x289B7EC6, 4);
    MD5STEP(F3, d, a, b, c, pIn[0] + 0xEAA127FA, 11);
    MD5STEP(F3, c, d, a, b, pIn[3] + 0xD4EF3085, 16);
    MD5STEP(F3, b, c, d, a, pIn[6] + 0x04881D05, 23);
    MD5STEP(F3, a, b, c, d, pIn[9] + 0xD9D4D039, 4);
    MD5STEP(F3, d, a, b, c, pIn[12] + 0xE6DB99E5, 11);
    MD5STEP(F3, c, d, a, b, pIn[15] + 0x1FA27CF8, 16);
    MD5STEP(F3, b, c, d, a, pIn[2] + 0xC4AC5665, 23);

    MD5STEP(F4, a, b, c, d, pIn[0] + 0xF4292244, 6);
    MD5STEP(F4, d, a, b, c, pIn[7] + 0x432AFF97, 10);
    MD5STEP(F4, c, d, a, b, pIn[14] + 0xAB9423A7, 15);
    MD5STEP(F4, b, c, d, a, pIn[5] + 0xFC93A039, 21);
    MD5STEP(F4, a, b, c, d, pIn[12] + 0x655B59C3, 6);
    MD5STEP(F4, d, a, b, c, pIn[3] + 0x8F0CCC92, 10);
    MD5STEP(F4, c, d, a, b, pIn[10] + 0xFFEFF47D, 15);
    MD5STEP(F4, b, c, d, a, pIn[1] + 0x85845DD1, 21);
    MD5STEP(F4, a, b, c, d, pIn[8] + 0x6FA87E4F, 6);
    MD5STEP(F4, d, a, b, c, pIn[15] + 0xFE2CE6E0, 10);
    MD5STEP(F4, c, d, a, b, pIn[6] + 0xA3014314, 15);
    MD5STEP(F4, b, c, d, a, pIn[13] + 0x4E0811A1, 21);
    MD5STEP(F4, a, b, c, d, pIn[4] + 0xF7537E82, 6);
    MD5STEP(F4, d, a, b, c, pIn[11] + 0xBD3AF235, 10);
    MD5STEP(F4, c, d, a, b, pIn[2] + 0x2AD7D2BB, 15);
    MD5STEP(F4, b, c, d, a, pIn[9] + 0xEB86D391, 21);

    pBuf[0] += a;
    pBuf[1] += b;
    pBuf[2] += c;
    pBuf[3] += d;
}

// =====================================================================================================================
// Reverses the bytes on little-endian machine.  If the machine is big-endian, this function does nothing.
void ByteReverse(
    uint8_t* pBuf,   // [in,out] Buffer to reverse
    uint32_t longs)  // Number of dwords to reverse
{
#if defined(LITTLEENDIAN_CPU)
    uint32_t value;
    do
    {
        value = (pBuf[3] << 24) | (pBuf[2] << 16) | (pBuf[1] << 8) | pBuf[0];
        *reinterpret_cast<uint32_t*>(pBuf)= value;
        pBuf += 4;
    } while (--longs);
#else
    // No reversal is necessary on big-endian architecture.
#endif
}

} // Md5

} // Llpc
