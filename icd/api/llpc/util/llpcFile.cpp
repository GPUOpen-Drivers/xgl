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
 * @brief LLPC source file: contains implementation of utility collection class Llpc::File.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-file"

#include "llpcFile.h"
#include <stdarg.h>
#include <sys/stat.h>

namespace Llpc
{

// =====================================================================================================================
// Opens a file stream for read, write or append access.
Result File::Open(
    const char*  pFilename,    // [in] Name of file to open
    uint32_t     accessFlags)  // ORed mask of FileAccessMode values describing how the file will be used
{
    Result result = Result::Success;

    if (m_pFileHandle != nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pFilename == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        char fileMode[5] = { };

        switch (accessFlags)
        {
        case FileAccessRead:
            fileMode[0] = 'r';
            break;
        case FileAccessWrite:
            fileMode[0] = 'w';
            break;
        case FileAccessAppend:
            fileMode[0] = 'a';
            break;
        case (FileAccessRead | FileAccessWrite):
            // NOTE: Both r+ and w+ modes might apply here: r+ requires that the file exists beforehand, while w+ does
            // not. w+ will create the file if it doesn't exist, like w,a,a+. w+, like w, will discard existing
            // contents of thefile. If we need to expose r+ mode, adding another flag to indicate 'don't overwrite the
            // file'.
            fileMode[0] = 'w';
            fileMode[1] = '+';
            break;
        case (FileAccessRead | FileAccessAppend):
            fileMode[0] = 'a';
            fileMode[1] = '+';
            break;
        case (FileAccessRead | FileAccessBinary):
            fileMode[0] = 'r';
            fileMode[1] = 'b';
            break;
        case (FileAccessWrite | FileAccessBinary) :
            fileMode[0] = 'w';
            fileMode[1] = 'b';
            break;
        case (FileAccessRead | FileAccessWrite | FileAccessBinary):
            fileMode[0] = 'w';
            fileMode[1] = 'b';
            fileMode[2] = '+';
            fileMode[3] = 'R';
            break;
        case (FileAccessRead | FileAccessAppend | FileAccessBinary):
            fileMode[0] = 'a';
            fileMode[1] = 'b';
            fileMode[2] = '+';
            fileMode[3] = 'R';
            break;
        case FileAccessReadUpdate:
            fileMode[0] = 'r';
            fileMode[1] = '+';
            break;
        case (FileAccessReadUpdate | FileAccessBinary):
            fileMode[0] = 'r';
            fileMode[1] = '+';
            fileMode[2] = 'b';
            break;
        default:
            LLPC_NEVER_CALLED();
            result = Result::ErrorInvalidValue;
            break;
        }

        if (result == Result::Success)
        {
#if defined(_WIN32)
            // MS compilers provide fopen_s, which is supposedly "safer" than traditional fopen.
            fopen_s(&m_pFileHandle, pFilename, &fileMode[0]);
#else
            // Just use the traditional fopen.
            m_pFileHandle = fopen(pFilename, &fileMode[0]);
#endif
            if (m_pFileHandle == nullptr)
            {
                result = Result::ErrorUnknown;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Closes the file handle if still open.
void File::Close()
{
    if (m_pFileHandle != nullptr)
    {
        fclose(m_pFileHandle);
        m_pFileHandle = nullptr;
    }
}

// =====================================================================================================================
// Writes a stream of bytes to the file.
Result File::Write(
    const void* pBuffer,     // [in] Buffer to write to the file
    size_t      bufferSize)  // Size of the buffer in bytes
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        if (fwrite(pBuffer, 1, bufferSize, m_pFileHandle) != bufferSize)
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
// Reads a stream of bytes from the file.
Result File::Read(
    void*   pBuffer,     // [out] Buffer to read the file into
    size_t  bufferSize,  // Size of buffer in bytes
    size_t* pBytesRead)  // [out] Number of bytes actually read (can be nullptr)
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        const size_t bytesRead = fread(pBuffer, 1, bufferSize, m_pFileHandle);

        if (bytesRead != bufferSize)
        {
            result = Result::ErrorUnknown;
        }

        if (pBytesRead != nullptr)
        {
            *pBytesRead = bytesRead;
        }
    }

    return result;
}

// =====================================================================================================================
// Reads a single line (until the next newline) of bytes from the file.
Result File::ReadLine(
    void*   pBuffer,     // [out] Buffer to read the file into
    size_t  bufferSize,  // Size of buffer in bytes
    size_t* pBytesRead)  // [out] Number of bytes actually read (can be nullptr)
{
    Result result = Result::ErrorInvalidValue;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        size_t bytesRead = 0;
        char* pCharBuffer = static_cast<char*>(pBuffer);

        while (bytesRead < bufferSize)
        {
            int32_t c = getc(m_pFileHandle);
            if (c == '\n')
            {
                result = Result::Success;
                break;
            }
            else if (c == EOF)
            {
                result = Result::ErrorUnknown;
                break;
            }
            pCharBuffer[bytesRead] = static_cast<char>(c);
            bytesRead++;
        }

        if (pBytesRead != nullptr)
        {
            *pBytesRead = bytesRead;
        }
    }

    return result;
}

// =====================================================================================================================
// Prints a formatted string to the file.
Result File::Printf(
    const char* pFormatStr,  // [in] Printf-style format string
    ...                      // Printf-style argument list
    ) const
{
    Result result = Result::ErrorUnavailable;

    if (m_pFileHandle != nullptr)
    {
        va_list argList;
        va_start(argList, pFormatStr);

#if defined(_WIN32)
        // MS compilers provide vfprintf_s, which is supposedly "safer" than traditional vfprintf.
        if (vfprintf_s(m_pFileHandle, pFormatStr, argList) != -1)
#else
        // Just use the traditional vfprintf.
        if (vfprintf(m_pFileHandle, pFormatStr, argList) >= 0)
#endif
        {
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorUnknown;
        }

        va_end(argList);
    }

    return result;
}

// =====================================================================================================================
// Prints a formatted string to the file.
Result File::VPrintf(
    const char* pFormatStr,  // [in] Printf-style format string
    va_list     argList)     // Pre-started variable argument list
{
    Result result = Result::ErrorUnavailable;

    if (m_pFileHandle != nullptr)
    {
#if defined(_WIN32)
        // MS compilers provide vfprintf_s, which is supposedly "safer" than traditional vfprintf.
        if (vfprintf_s(m_pFileHandle, pFormatStr, argList) != -1)
#else
        // Just use the traditional vfprintf.
        if (vfprintf(m_pFileHandle, pFormatStr, argList) >= 0)
#endif
        {
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
// Flushes pending I/O to the file.
Result File::Flush() const
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        fflush(m_pFileHandle);
    }

    return result;
}

// =====================================================================================================================
// Sets the file position to the beginning of the file.
void File::Rewind()
{
    if (m_pFileHandle != nullptr)
    {
        rewind(m_pFileHandle);
    }
}

// =====================================================================================================================
// Sets the file position to the beginning of the file.
void File::Seek(
    int32_t offset,         // Number of bytes to offset
    bool   fromOrigin)      // If true, the seek will be relative to the file origin;
                            // if false, it will be from the current position
{
    if (m_pFileHandle != nullptr)
    {
        int32_t ret = fseek(m_pFileHandle, offset, fromOrigin ? SEEK_SET : SEEK_CUR);

        LLPC_ASSERT(ret == 0);
    }
}

// =====================================================================================================================
// Returns true if a file with the given name exists.
size_t File::GetFileSize(
    const char* pFilename)     // [in] Name of the file to check
{
#if defined(_WIN32)
    // On MS compilers the function and structure to retrieve/store file status information is named '_stat' (with
    // underbar)...
    struct _stat fileStatus = { };
    const int32_t result = _stat(pFilename, &fileStatus);
#else
    // ...however, on other compilers, they are named 'stat' (no underbar).
    struct stat fileStatus = {};
    const int32_t result = stat(pFilename, &fileStatus);
#endif
    // If the function call to retrieve file status information fails (returns 0), then the file does not exist (or is
    // inaccessible in some other manner).
    return (result == 0) ? fileStatus.st_size : 0;
}

// =====================================================================================================================
// Returns true if a file with the given name exists.
bool File::Exists(
    const char* pFilename)      // [in] Name of the file to check
{
#if defined(_WIN32)
    // On MS compilers the function and structure to retrieve/store file status information is named '_stat' (with
    // underbar)...
    struct _stat fileStatus = { };
    const int32_t result = _stat(pFilename, &fileStatus);
#else
    // ...however, on other compilers, they are named 'stat' (no underbar).
    struct stat fileStatus = {};
    const int32_t result = stat(pFilename, &fileStatus);
#endif
    // If the function call to retrieve file status information fails (returns -1), then the file does not exist (or is
    // inaccessible in some other manner).
    return (result != -1);
}

} // Llpc
