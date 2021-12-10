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
#pragma once

#include "include/binary_cache_serialization.h"

// These Xlib defines conflict with LLVM.
#undef Bool
#undef Status

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace cc {

// =====================================================================================================================
//
// This API allows to analyze and print XGL's PipelineBinaryCache blobs. It is not meant to work with other
// Vulkan Pipeline Cache blob formats.
//
// PipelineBinaryCache consist of 3 parts:
// -  Public Vulkan Pipeline Cache header
// -  Private PipelineBinaryCache header
// -  Sequence of PipelineBinaryCache entries
//
// For detailed information about PipelineBinaryCache structure, see
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html and
// `xgl/icd/api/include/binary_cache_serialization.h`.
//
// =====================================================================================================================

// Represents printable information about the public Vulkan Pipeline Cache header.
struct PublicVkHeaderInfo {
  const vk::PipelineCacheHeaderData *publicHeader;
  size_t trailingSpaceBeforePrivateBlob;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const PublicVkHeaderInfo &info);

// Represents printable information about the private Pipeline Binary Cache header.
struct BinaryCachePrivateHeaderInfo {
  const vk::PipelineBinaryCachePrivateHeader *privateHeader;
  size_t contentBlobSize;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const BinaryCachePrivateHeaderInfo &info);

using MD5DigestStr = llvm::SmallString<32>;

// Represents printable information about a Pipeline Binary Cache entry, its location within the cache blob, and
// calculated MD5 sum of the entry content.
struct BinaryCacheEntryInfo {
  // We do not store the header as `vk::BinaryCacheEntry *` because the address may not be properly aligned.
  const uint8_t *entryHeader;
  vk::BinaryCacheEntry entryHeaderData;
  size_t idx;
  llvm::ArrayRef<uint8_t> entryBlob;
  MD5DigestStr entryMD5Sum;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const BinaryCacheEntryInfo &info);

// Given a directory, returns a map from ELF MD5 sums their paths. Files without the '.elf' extension ignored.
llvm::StringMap<std::string> mapMD5SumsToElfFilePath(llvm::Twine dir);

// Analyzes given PipelineBinaryCache file. It it valid to use it with invalid or partially-valid cache blobs.
// Note: Member functions do not have to be called in any particular order.
class CacheBlobInfo {
public:
  static llvm::Expected<CacheBlobInfo> create(llvm::MemoryBufferRef cacheBlob);

  llvm::Expected<PublicVkHeaderInfo> readPublicVkHeaderInfo() const;

  llvm::Expected<BinaryCachePrivateHeaderInfo> readBinaryCachePrivateHeaderInfo() const;

  llvm::Error readBinaryCacheEntriesInfo(llvm::SmallVectorImpl<BinaryCacheEntryInfo> &entriesInfoOut) const;

  llvm::Expected<size_t> getPrivateHeaderOffset() const;
  llvm::Expected<size_t> getCacheContentOffset() const;

private:
  CacheBlobInfo(llvm::MemoryBufferRef cacheBlob) : m_cacheBlob(cacheBlob) {}

  llvm::MemoryBufferRef m_cacheBlob;
};

} // namespace cc
