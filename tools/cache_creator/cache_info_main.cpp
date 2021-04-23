/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Google LLC. All Rights Reserved.
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

#include "cache_info.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cassert>

namespace {
llvm::cl::OptionCategory CacheInfoCat("Cache Info Options");

llvm::cl::opt<std::string> InFile(llvm::cl::Positional, llvm::cl::ValueRequired, llvm::cl::cat(CacheInfoCat),
                                  llvm::cl::desc("<Input cache_file.bin>"));
llvm::cl::opt<std::string> ElfSourceDir("elf-source-dir", llvm::cl::desc("(Optional) Directory with source ELF files"),
                                        llvm::cl::cat(CacheInfoCat), llvm::cl::value_desc("directory"));

int reportAndConsumeError(llvm::Error err, int exitCode) {
  llvm::errs() << "[ERROR]: " << err << "\n";
  llvm::consumeError(std::move(err));
  return exitCode;
}
} // namespace

namespace fs = llvm::sys::fs;

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> inputBufferOrErr = llvm::MemoryBuffer::getFile(InFile);
  if (auto err = inputBufferOrErr.getError()) {
    llvm::errs() << "Failed to read input file " << InFile << ": " << err.message() << "\n";
    return 3;
  }
  llvm::outs() << "Read: " << InFile << ", " << (*inputBufferOrErr)->getBufferSize() << " B\n\n";

  auto blobInfoOrErr = cc::CacheBlobInfo::create(**inputBufferOrErr);
  if (auto err = blobInfoOrErr.takeError())
    return reportAndConsumeError(std::move(err), 4);
  const cc::CacheBlobInfo &blobInfo = *blobInfoOrErr;

  auto publicHeaderInfoOrErr = blobInfo.readPublicVkHeaderInfo();
  if (auto err = publicHeaderInfoOrErr.takeError())
    return reportAndConsumeError(std::move(err), 4);
  llvm::outs() << *publicHeaderInfoOrErr << "\n";

  auto privateHeaderInfoOrErr = blobInfo.readBinaryCachePrivateHeaderInfo();
  if (auto err = privateHeaderInfoOrErr.takeError())
    return reportAndConsumeError(std::move(err), 4);
  llvm::outs() << *privateHeaderInfoOrErr << "\n";

  llvm::StringMap<std::string> elfMD5ToFilePath;
  if (!ElfSourceDir.empty()) {
    llvm::SmallVector<char> rawElfSourceDir;
    if (std::error_code err = fs::real_path(ElfSourceDir, rawElfSourceDir, /* expand_tilde = */ true)) {
      llvm::errs() << "[ERROR]: --elf-source-dir: " << ElfSourceDir << "could not be expanded: " << err.message()
                   << "\n";
      return 3;
    }
    llvm::StringRef elfSourceDirReal(rawElfSourceDir.begin(), rawElfSourceDir.size());

    if (!fs::is_directory(elfSourceDirReal)) {
      llvm::errs() << "[ERROR]: --elf-source-dir: " << elfSourceDirReal << " is not a directory!\n";
      return 3;
    }
    elfMD5ToFilePath = cc::mapMD5SumsToElfFilePath(elfSourceDirReal);
  }

  llvm::SmallVector<cc::BinaryCacheEntryInfo> entries;
  if (auto cacheEntriesReadErr = blobInfo.readBinaryCacheEntriesInfo(entries))
    return reportAndConsumeError(std::move(cacheEntriesReadErr), 4);

  llvm::outs() << "=== Cache Content Info ===\n"
               << "total num entries: " << entries.size() << "\n"
               << "entry header length: " << sizeof(vk::BinaryCacheEntry) << "\n\n";

  for (const cc::BinaryCacheEntryInfo &entryInfo : entries) {
    llvm::StringRef sourceFilePath = "<none>";
    auto sourceElfIt = elfMD5ToFilePath.find(entryInfo.entryMD5Sum);
    if (sourceElfIt != elfMD5ToFilePath.end())
      sourceFilePath = sourceElfIt->second;

    llvm::outs() << entryInfo << "\tmatched source file:\t" << sourceFilePath << "\n\n";
  }

  llvm::outs() << "\n=== Cache Info analysis finished ===\n";
  return 0;
}
