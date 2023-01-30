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
**************************************************************************************************
* @file  json_writer.h
* @brief A simple JSON writer.
**************************************************************************************************
*/
#ifndef __JSON_WRITER_H__
#define __JSON_WRITER_H__
#pragma once

#include <stdint.h>
#include "palUtil.h"
#include "palFile.h"
#include "palJsonWriter.h"

namespace Util
{
class JsonStream;
class File;
}

namespace vk
{

namespace utils
{
// =====================================================================================================================
// JSON stream that records the text stream using an output file.
class JsonOutputStream : public Util::JsonStream
{
public:
    explicit JsonOutputStream(const char* pFilePath);
    virtual ~JsonOutputStream();

    virtual void WriteString(const char* pString, Util::uint32 length) override;
    virtual void WriteCharacter(char character) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(JsonOutputStream);

    Util::Result OpenFile(const char* pFilePath);

    bool IsFileOpen() const { return m_file.IsOpen(); }

    Util::File     m_file;       // The text stream is being written here.
    const char*    m_pFilePath;

};
}

}
#endif
