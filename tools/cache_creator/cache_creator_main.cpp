/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Google LLC. All Rights Reserved.
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
#include "cache_creator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/FileSystem.h"
#include <cassert>

namespace {
llvm::cl::OptionCategory CacheCreatorCat("Cache Creator Options");

llvm::cl::list<std::string> InFiles(llvm::cl::Positional, llvm::cl::OneOrMore, llvm::cl::ValueRequired,
                                    llvm::cl::cat(CacheCreatorCat), llvm::cl::desc("<Input elf file(s)>"));
llvm::cl::opt<std::string> OutFileName("o", llvm::cl::desc("Output cache file"), llvm::cl::cat(CacheCreatorCat),
                                       llvm::cl::value_desc("filename.bin"), llvm::cl::Required);
llvm::cl::opt<uint32_t> DeviceId("device-id", llvm::cl::desc("Devide ID. This must match the target GPU."),
                                 llvm::cl::value_desc("number"), llvm::cl::cat(CacheCreatorCat), llvm::cl::Required);

// This UUID is generated in vk_physical_device.cpp::Initialize.
llvm::cl::opt<std::string>
    UuidStr("uuid",
            llvm::cl::desc(
                "Pipeline cache UUID for the specific driver and machine, e.g., 00000000-12345-6789-abcd-ef0000000042"),
            llvm::cl::value_desc("hex string"), llvm::cl::cat(CacheCreatorCat), llvm::cl::Required);

llvm::cl::opt<bool> Verbose("verbose", llvm::cl::desc("Enable verbose output"), llvm::cl::init(false),
                            llvm::cl::cat(CacheCreatorCat));

// =====================================================================================================================
// Returns an ostream for printing output info messages.
//
// @returns : Output stream writing to STDOUT if `--verbose` is enabled, or a stream that doesn't print anything if not
llvm::raw_ostream &infos() {
  if (Verbose)
    return llvm::outs();

  static llvm::raw_null_ostream blackHole;
  return blackHole;
}
} // namespace

namespace fs = llvm::sys::fs;

static llvm::Error getFileSizes(llvm::ArrayRef<std::string> filenames, llvm::MutableArrayRef<size_t> outFileSizes) {
  assert(filenames.size() == outFileSizes.size());
  for (auto &&nameSizePair : llvm::zip(filenames, outFileSizes)) {
    const std::string &filename = std::get<0>(nameSizePair);
    if (std::error_code err = fs::file_size(filename, std::get<1>(nameSizePair)))
      return llvm::createFileError(filename, llvm::createStringError(err, "Failed to read file size"));
  }
  return llvm::Error::success();
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  llvm::outs() << "NOTE: cache-creator is still under development. Things may not work as expected.\n\n";

  std::array<uint8_t, VK_UUID_SIZE> uuid = {};
  if (!cc::hexStringToUuid(UuidStr, uuid)) {
    llvm::errs() << "Failed to parse pipeline cache UUID (--uuid). See `cache-creator --help` for usage details.\n";
    return 2;
  }

  const size_t numFiles = InFiles.size();
  llvm::SmallVector<size_t> fileSizes(numFiles);
  if (auto err = getFileSizes(InFiles, fileSizes)) {
    llvm::errs() << err << "\n";
    llvm::consumeError(std::move(err));
    return 3;
  }

  const size_t cacheBlobSize = cc::RelocatableCacheCreator::CalculateAnticipatedCacheFileSize(fileSizes);
  infos() << "Num inputs: " << numFiles << ", anticipated cache size: " << cacheBlobSize << "\n";

  auto outFileBufferOrErr = llvm::FileOutputBuffer::create(OutFileName, cacheBlobSize);
  if (auto err = outFileBufferOrErr.takeError()) {
    llvm::errs() << "Failed to create the output file " << OutFileName << ". Error:\t" << err << "\n";
    llvm::consumeError(std::move(err));
    return 4;
  }

  // TODO(kuhar): Initialize the platform key properly by providing the `fingerprint` parameter instead of an empty
  // array. This is so that the cache can pass validation and be consumed by the ICD. Note that this also requires
  // ICD-side changes.
  auto cacheCreatorOrErr = cc::RelocatableCacheCreator::Create(
      DeviceId, uuid, {},
      llvm::makeMutableArrayRef((*outFileBufferOrErr)->getBufferStart(), (*outFileBufferOrErr)->getBufferSize()));
  if (auto err = cacheCreatorOrErr.takeError()) {
    llvm::errs() << "Error:\t" << err << "\n";
    llvm::consumeError(std::move(err));
    return 4;
  }
  cc::RelocatableCacheCreator &cacheCreator = *cacheCreatorOrErr;

  for (const std::string &filename : InFiles) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> inputBufferOrErr = llvm::MemoryBuffer::getFile(filename);
    if (std::error_code err = inputBufferOrErr.getError()) {
      llvm::errs() << "Failed to read input file " << filename << ": " << err.message() << "\n";
      return 3;
    }
    infos() << "Read: " << filename << "\n";

    if (auto err = cacheCreator.addElf(**inputBufferOrErr)) {
      llvm::errs() << "Error:\t" << err << "\n";
      llvm::consumeError(std::move(err));
      return 4;
    }
  }

  size_t actualNumEntries = 0;
  size_t actualCacheSize = 0;
  if (auto err = cacheCreator.finalize(&actualNumEntries, &actualCacheSize)) {
    llvm::errs() << "Error:\t" << err << "\n";
    llvm::consumeError(std::move(err));
    return 4;
  }
  infos() << "Num entries written: " << actualNumEntries << ", actual cache size: " << actualCacheSize << " B\n";

  if ((*outFileBufferOrErr)->commit()) {
    llvm::errs() << "Failed to commit the serialized cache to the output file\n";
    return 4;
  }
  llvm::outs() << "Cache successfully written to: " << (*outFileBufferOrErr)->getPath() << "\n";

  return 0;
}
