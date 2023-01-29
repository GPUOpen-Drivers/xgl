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

#include "json_writer.h"

namespace vk
{
namespace utils
{
// =====================================================================================================================
JsonOutputStream::JsonOutputStream(
    const char* pFilePath)
    :
    m_pFilePath(pFilePath)
{
}

// =====================================================================================================================
JsonOutputStream::~JsonOutputStream()
{
    if (m_file.IsOpen())
    {
        m_file.Close();
    }
}

// =====================================================================================================================
Util::Result JsonOutputStream::OpenFile(
    const char* pFilePath)
{
    Util::Result result = m_file.Open(pFilePath, Util::FileAccessAppend);

    return result;
}

// =====================================================================================================================
void JsonOutputStream::WriteString(
    const char*  pString,
    Util::uint32 length)
{
    Util::Result result = Util::Result::Success;

    // If we've already opened the dump file, just write directly to it
    if (m_file.IsOpen())
    {
        m_file.Write(pString, length);
    }
    else
    {
        result = OpenFile(m_pFilePath);
        if (result == Util::Result::Success)
        {
             m_file.Write(pString, length);
        }
    }
}

// =====================================================================================================================
void JsonOutputStream::WriteCharacter(
    char character)
{
    Util::Result result = Util::Result::Success;

    // If we've already opened the dump file, just write directly to it
    if (m_file.IsOpen())
    {
        m_file.Write(&character, 1);
    }
    else
    {
        result = OpenFile(m_pFilePath);
        if (result == Util::Result::Success)
        {
             m_file.Write(&character, 1);
        }
    }
}

}

}

