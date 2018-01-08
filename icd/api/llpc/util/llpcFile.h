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
 * @file  llpcFile.h
 * @brief LLPC header file: contains definitions of utility collection class Llpc::File.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpc.h"
#include "llpcDebug.h"
#include <cstdio>

namespace Llpc
{

// Enumerates access modes that may be required on an opened file. Can be bitwise ORed together to specify multiple
// simultaneous modes.
enum FileAccessMode : uint32_t
{
    FileAccessRead   = 0x1,         ///< Read access.
    FileAccessWrite  = 0x2,         ///< Write access.
    FileAccessAppend = 0x4,         ///< Append access.
    FileAccessBinary = 0x8,         ///< Binary access.
    FileAccessReadUpdate = 0x10,    ///< Read&Update access.
};

// =====================================================================================================================
// Exposes simple file I/O functionality by encapsulating standard C runtime file I/O functions like fopen, fwrite, etc.
class File
{
public:
    File() : m_pFileHandle(nullptr) { }

    // Closes the file if it is still open.
    ~File() { Close(); }

    static size_t GetFileSize(const char* pFilename);
    static bool Exists(const char* pFilename);

    Result Open(const char* pFilename, uint32_t accessFlags);
    void Close();
    Result Write(const void* pBuffer, size_t bufferSize);
    Result Read(void* pBuffer, size_t bufferSize, size_t* pBytesRead);
    Result ReadLine(void* pBuffer, size_t bufferSize, size_t* pBytesRead);
    Result Printf(const char* pFormatStr, ...) const;
    Result VPrintf(const char* pFormatStr, va_list argList);
    Result Flush() const;
    void Rewind();
    void Seek(int32_t offset, bool fromOrigin);

    // Returns true if the file is presently open.
    bool IsOpen() const { return (m_pFileHandle != nullptr); }
    // Gets handle of the file
    const std::FILE* GetHandle() const { return m_pFileHandle; }

private:
    std::FILE* m_pFileHandle;      // File handle
};

} // Llpc
