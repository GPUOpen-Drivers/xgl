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
 * @file  llpcMd5.h
 * @brief LLPC header file: contains definitions of utility collection MD5.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpc.h"
#include "llpcDebug.h"

namespace Llpc
{

// Namespace containing functions that provide support for MD5 checksums.
//
// Code adapted from: http://www.fourmilab.ch/md5/
//
// This code implements the MD5 message-digest algorithm.  The algorithm is due to Ron Rivest.  This code was written
// by Colin Plumb in 1993, no copyright is claimed.  This code is in the public domain; do with it what you wish.
//
// Equivalent code is available from RSA Data Security, Inc.  This code has been tested against that, and is
// equivalent, except that you don't need to include two pages of legalese with every copy.
namespace Md5
{

// Output hash value generated from the MD5 checksum.
struct Hash
{
    uint32_t hashValue[4];  // Output hash value.
};

// Working context for the MD5 checksum algorithm.
struct Context
{
    uint32_t buf[4];   // Working buffer.
    uint32_t bits[2];  // Bit count.
    uint8_t  in[64];   // Hash Value.
};

// Generates an MD5 hash from the specified memory buffer.
extern Hash GenerateHashFromBuffer(const void* pBuffer, size_t bufLen);

// Initializes an MD5 context to be used for incremental hashing of several buffers via Update().
extern void Init(Context* pCtx);

// Updates the specified MD5 context based on the data in the specified buffer.
extern void Update(Context* pCtx, const void* pBuf, size_t bufLen);

// Outputs the final MD5 hash after a series of Update() calls.
extern void Final(Context* pCtx, Hash* pHash);

// Compacts a 128-bit MD5 checksum into a 64-bit one by XOR'ing the low and high 64-bits together to create a more
// manageable 64-bit identifier based on the checksum.
inline uint64_t Compact64(
    const Hash* pHash)    // [in] pHash 128-bit MD5 hash to be compacted.
{
    return (static_cast<uint64_t>(pHash->hashValue[3] ^ pHash->hashValue[1]) |
           (static_cast<uint64_t>(pHash->hashValue[2] ^ pHash->hashValue[0]) << 32));
}

// Compacts a 128-bit MD5 checksum into a 32-bit one by XOR'ing each 32-bit chunk together to create a more manageable
// 32-bit identifier based on the checksum.
inline uint32_t Compact32(
    const Hash* pHash)  // [in] pHash 128-bit MD5 hash to be compacted.
{
    return pHash->hashValue[3] ^ pHash->hashValue[2] ^ pHash->hashValue[1] ^ pHash->hashValue[0];
}

// Updates the specified MD5 context based on the input data
template <class T>
void Update(
    Context* pCtx,  // [in] MD5 context to be updated
    const T& data)  // Input data to be hashed in the MD5 context
{
    Update(pCtx, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
}

} // Md5

} // Util
