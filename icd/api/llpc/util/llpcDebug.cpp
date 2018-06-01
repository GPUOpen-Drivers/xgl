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
 * @file  llpcDebug.cpp
 * @brief LLPC source file: contains implementation of LLPC debug utility functions.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-debug"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include "llpc.h"
#include "llpcCompiler.h"
#include "llpcDebug.h"
#include "llpcElf.h"
#include "llpcGfx6Chip.h"
#include "llpcGfx9Chip.h"
#include "llpcInternal.h"
#include "llpcMetroHash.h"

namespace llvm
{

namespace cl
{

// -enable-outs: enable general message output (to stdout or external file).
opt<bool> EnableOuts(
    "enable-outs",
    desc("Enable general message output (to stdout or external file) (default: true)"),
    init(true));

// -enable-errs: enable error message output (to stderr or external file).
opt<bool> EnableErrs(
    "enable-errs",
    desc("Enable error message output (to stdout or external file) (default: true)"),
    init(true));

// -log-file-dbgs: name of the file to log info from dbg()
opt<std::string> LogFileDbgs("log-file-dbgs",
                             desc("Name of the file to log info from dbgs()"),
                             value_desc("filename"),
                             init("llpcLog.txt"));

// -log-file-outs: name of the file to log info from LLPC_OUTS() and LLPC_ERRS()
opt<std::string> LogFileOuts("log-file-outs",
                             desc("Name of the file to log info from LLPC_OUTS() and LLPC_ERRS()"),
                             value_desc("filename"),
                             init(""));

} // cl

} // llvm

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// Gets the value of option "allow-out".
bool EnableOuts()
{
    return cl::EnableOuts;
}

// =====================================================================================================================
// Gets the value of option "allow-err".
bool EnableErrs()
{
    return cl::EnableErrs;
}

// =====================================================================================================================
// Redirects the output of logs. It affects the behavior of llvm::outs(), dbgs() and errs().
//
// NOTE: This function redirects log output by modify the underlying static raw_fd_ostream object in outs() and errs().
// With this method, we can redirect logs in all environments, include both standalone compiler and Vulkan ICD. and we
// can restore the output on all platform, which is very useful when app crashes or hits an assert.
// CAUTION: The behavior isn't changed if app outputs logs to STDOUT or STDERR directly.
void RedirectLogOutput(
    bool              restoreToDefault,   // Restore the default behavior of outs() and errs() if it is true
    uint32_t          optionCount,        // Count of compilation-option strings
    const char*const* pOptions)            // [in] An array of compilation-option strings
{
    static raw_fd_ostream* pDbgFile = nullptr;
    static raw_fd_ostream* pOutFile = nullptr;
    static uint8_t  dbgFileBak[sizeof(raw_fd_ostream)] = {};
    static uint8_t  outFileBak[sizeof(raw_fd_ostream)] = {};

    if (restoreToDefault)
    {
        // Restore default raw_fd_ostream objects
        if (pDbgFile != nullptr)
        {
            memcpy(&errs(), dbgFileBak, sizeof(raw_fd_ostream));
            pDbgFile->close();
            pDbgFile = nullptr;
        }

        if (pOutFile != nullptr)
        {
            memcpy(&outs(), outFileBak, sizeof(raw_fd_ostream));
            pOutFile->close();
            pOutFile = nullptr;
        }
    }
    else
    {
        // Redirect errs() for dbgs()
        if (cl::LogFileDbgs.empty() == false)
        {
            // NOTE: Checks whether errs() is needed in compiliation
            // Until now, option -debug, -debug-only and -print-* need use debug output
            bool needDebugOut = ::llvm::DebugFlag;
            for (uint32_t i = 1; (needDebugOut == false) && (i < optionCount); ++i)
            {
                StringRef option = pOptions[i];
                if (option.startswith("-debug") || option.startswith("-print"))
                {
                    needDebugOut = true;
                }
            }

            if (needDebugOut)
            {
                std::error_code errCode;

                static raw_fd_ostream dbgFile(cl::LogFileDbgs.c_str(), errCode, sys::fs::F_Text);
                LLPC_ASSERT(!errCode);
                if (pDbgFile == nullptr)
                {
                    dbgFile.SetUnbuffered();
                    memcpy(dbgFileBak, &errs(), sizeof(raw_fd_ostream));
                    memcpy(&errs(), &dbgFile, sizeof(raw_fd_ostream));
                    pDbgFile = &dbgFile;
                }
            }
        }

        // Redirect outs() for LLPC_OUTS() and LLPC_ERRS()
        if ((cl::EnableOuts || cl::EnableErrs) && (cl::LogFileOuts.empty() == false))
        {
            if ((cl::LogFileOuts == cl::LogFileDbgs) && (pDbgFile != nullptr))
            {
                 memcpy(outFileBak, &outs(), sizeof(raw_fd_ostream));
                 memcpy(&outs(), pDbgFile, sizeof(raw_fd_ostream));
                 pOutFile = pDbgFile;
            }
            else
            {
                std::error_code errCode;

                static raw_fd_ostream outFile(cl::LogFileOuts.c_str(), errCode, sys::fs::F_Text);
                LLPC_ASSERT(!errCode);
                if (pOutFile == nullptr)
                {
                    outFile.SetUnbuffered();
                    memcpy(outFileBak, &outs(), sizeof(raw_fd_ostream));
                    memcpy(&outs(), &outFile, sizeof(raw_fd_ostream));
                    pOutFile = &outFile;
                }
            }
        }
    }
}

// =====================================================================================================================
// Enables/disables the output for debugging. TRUE for enable, FALSE for disable.
void EnableDebugOutput(
    bool restore) // Whether to enable debug output
{
    static raw_null_ostream nullStream;
    static uint8_t  dbgStream[sizeof(raw_fd_ostream)] = {};

    if (restore)
    {
        // Restore default raw_fd_ostream objects
        memcpy(&errs(), dbgStream, sizeof(raw_fd_ostream));
    }
    else
    {
        // Redirect errs() for dbgs()
         memcpy(dbgStream, &errs(), sizeof(raw_fd_ostream));
         memcpy(&errs(), &nullStream, sizeof(nullStream));
    }
}

} // Llpc
