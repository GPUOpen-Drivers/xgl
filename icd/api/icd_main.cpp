/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
************************************************************************************************************************
* @file  icdMain.cpp
* @brief Implements the Vulkan ICD's DllMain entrypoint.
************************************************************************************************************************
*/

#if defined(__unix__) & (__GNUC__ == 5)

#include <ostream>
#include <istream>
#include <string>
#include <stdarg.h>
#include <stdio.h>

namespace std {

    // Fix comptibility issue in GCC5 build.
    // std::bitset and many others start to refer to __throw_out_of_range_fmt in stdc++ 3.4.20.
    void __attribute__((weak)) __throw_out_of_range_fmt(char const* fmt, ...)
    {
        va_list ap;
        char buf[1024];

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        buf[sizeof(buf) - 1] = 0;
        va_end(ap);

        __throw_range_error(buf);
    }

    // Fix comptibility issue in GCC5+boost 1.64 build.
    __attribute__((weak)) runtime_error::runtime_error(char const* s)
    : runtime_error(std::string(s))
    {
    }

    __attribute__((weak)) length_error::length_error(char const* s)
    : length_error(std::string(s))
    {
    }

    __attribute__((weak)) logic_error::logic_error(char const* s)
    : logic_error(std::string(s))
    {
    }

}

#endif
