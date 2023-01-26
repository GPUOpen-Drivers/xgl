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
#include "cache_creator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MD5.h"
#include <cassert>
#include <cinttypes>

namespace {

// =====================================================================================================================
// Annotates the base error with the blob identifier.
//
// @param blob : Cache blob
// @param err : Base error
// @returns : Error annotated with the blob identifier
llvm::Error createBlobError(llvm::MemoryBufferRef blob, llvm::Error err) {
  return llvm::createFileError(blob.getBufferIdentifier(), std::move(err));
}

// =====================================================================================================================
// Creates a string error referencing the blob identifier.
//
// @param blob : Cache blob.
// @param format : Format specifier (printf-style)
// @param vals : Values for the format specifier
// @returns : Error annotated with the blob identifier
template <typename... Ts>
llvm::Error createBlobError(llvm::MemoryBufferRef blob, const char *format, const Ts &...vals) {
  return createBlobError(blob, llvm::createStringError(std::errc::state_not_recoverable, format, vals...));
}

} // namespace

namespace cc {

// =====================================================================================================================
// Creates a CacheBlobInfo object.
//
// @param cacheBlob : Pipeline Cache blob
// @returns : CacheBlobInfo object on success, error if the cacheBlob cannot be a valid PipelineBinaryCache blob
llvm::Expected<CacheBlobInfo> CacheBlobInfo::create(llvm::MemoryBufferRef cacheBlob) {
  constexpr size_t minCacheBlobSize = vk::VkPipelineCacheHeaderDataSize + sizeof(vk::PipelineBinaryCachePrivateHeader);
  const size_t bufferSize = cacheBlob.getBufferSize();

  if (bufferSize < minCacheBlobSize) {
    return createBlobError(cacheBlob, "Input buffer too small to be a valid Pipeline Binary Cache blob: %zu B < %zu B",
                           bufferSize, minCacheBlobSize);
  }

  assert(cacheBlob.getBufferStart());
  return CacheBlobInfo{cacheBlob};
}

// =====================================================================================================================
// Reads the public Vulkan Pipeline Cache header.
//
// @returns : PublicVkHeaderInfo object on success, error if the cache blob does not have a valid public header
llvm::Expected<PublicVkHeaderInfo> CacheBlobInfo::readPublicVkHeaderInfo() const {
  PublicVkHeaderInfo res = {};
  res.publicHeader = reinterpret_cast<const vk::PipelineCacheHeaderData *>(m_cacheBlob.getBufferStart());

  const uint32_t headerLength = res.publicHeader->headerLength;
  if (headerLength < vk::VkPipelineCacheHeaderDataSize) {
    return createBlobError(m_cacheBlob,
                           "Vulkan Pipeline Cache header length too small to be a valid header: %zu B < %zu B",
                           size_t(headerLength), vk::VkPipelineCacheHeaderDataSize);
  }
  if (headerLength >= m_cacheBlob.getBufferSize()) {
    return createBlobError(m_cacheBlob, "Vulkan Pipeline Cache header length greater than blob size: %zu B >= %zu B",
                           size_t(headerLength), m_cacheBlob.getBufferSize());
  }

  res.trailingSpaceBeforePrivateBlob = headerLength - vk::VkPipelineCacheHeaderDataSize;
  return res;
}

// =====================================================================================================================
// Finds the start offset of the private PipelineBinaryCache header.
//
// @returns : Private header offset on success, error if the cache blob does not have a valid private header
llvm::Expected<size_t> CacheBlobInfo::getPrivateHeaderOffset() const {
  auto *publicHeader = reinterpret_cast<const vk::PipelineCacheHeaderData *>(m_cacheBlob.getBufferStart());
  size_t privateHeaderOffset = publicHeader->headerLength;

  if (privateHeaderOffset < vk::VkPipelineCacheHeaderDataSize)
    return createBlobError(m_cacheBlob, "Vulkan Pipeline Cache header length less than expected");

  if (privateHeaderOffset + sizeof(vk::PipelineBinaryCachePrivateHeader) > m_cacheBlob.getBufferSize())
    return createBlobError(m_cacheBlob, "Insufficient buffer size for the Pipeline Binary Cache private header");

  // Make sure that this is an AMD pipeline cache blob. If not, we cannot read the private header.
  if (publicHeader->vendorID != AMDVendorId)
    return createBlobError(m_cacheBlob, "Vendor is not AMD. Unknown cache blob format.");

  return privateHeaderOffset;
}

// =====================================================================================================================
// Reads the private PipelineBinaryCache header.
//
// @returns : BinaryCachePrivateHeader info on success, error if the cache blob does not have a valid private header
llvm::Expected<BinaryCachePrivateHeaderInfo> CacheBlobInfo::readBinaryCachePrivateHeaderInfo() const {
  auto privateHeaderOffsetOrErr = getPrivateHeaderOffset();
  if (auto err = privateHeaderOffsetOrErr.takeError())
    return std::move(err);

  BinaryCachePrivateHeaderInfo res = {};
  res.privateHeader = reinterpret_cast<const vk::PipelineBinaryCachePrivateHeader *>(m_cacheBlob.getBufferStart() +
                                                                                     *privateHeaderOffsetOrErr);
  res.contentBlobSize =
      m_cacheBlob.getBufferSize() - (*privateHeaderOffsetOrErr + sizeof(vk::PipelineBinaryCachePrivateHeader));
  return res;
}

// =====================================================================================================================
// Finds the start offset of the cache content.
//
// @returns : Cache content offset on success, error if the cache blob does not have any valid content (not even empty)
llvm::Expected<size_t> CacheBlobInfo::getCacheContentOffset() const {
  auto privateHeaderOffsetOrErr = getPrivateHeaderOffset();
  if (auto err = privateHeaderOffsetOrErr.takeError())
    return std::move(err);

  return *privateHeaderOffsetOrErr + sizeof(vk::PipelineBinaryCachePrivateHeader);
}

// =====================================================================================================================
// Reads all PipelineBinaryCache entries. For each entry, calculates information about its location within the cache
// blob, and computes the MD5 sum of the entry's content.
//
// @param [out] entriesInfoOut : The cache entries found
// @returns : Error if the cache blob does not have a valid content section, success otherwise
llvm::Error
CacheBlobInfo::readBinaryCacheEntriesInfo(llvm::SmallVectorImpl<BinaryCacheEntryInfo> &entriesInfoOut) const {
  auto contentOffsetOrErr = getCacheContentOffset();
  if (auto err = contentOffsetOrErr.takeError())
    return err;

  constexpr size_t EntrySize = sizeof(vk::BinaryCacheEntry);
  const uint8_t *const blobStart = reinterpret_cast<const uint8_t *>(m_cacheBlob.getBufferStart());
  const uint8_t *const blobEnd = reinterpret_cast<const uint8_t *>(m_cacheBlob.getBufferEnd());
  const uint8_t *currData = blobStart + *contentOffsetOrErr;

  for (size_t entryIdx = 0; currData < blobEnd; ++entryIdx) {
    const size_t currEntryOffset = currData - blobStart;
    if (currData + EntrySize > blobEnd) {
      return createBlobError(m_cacheBlob, "Insufficient buffer size for cache entry header #%zu at offset %zu",
                             entryIdx, currEntryOffset);
    }

    BinaryCacheEntryInfo currEntryInfo = {};
    currEntryInfo.entryHeader = currData;
    memcpy(&currEntryInfo.entryHeaderData, currData, EntrySize);
    currEntryInfo.idx = entryIdx;

    const size_t currEntryBlobSize = currEntryInfo.entryHeaderData.dataSize;
    if (currData + EntrySize + currEntryBlobSize > blobEnd) {
      return createBlobError(m_cacheBlob, "Insufficient buffer size for cache entry content #%zu at offset %zu",
                             entryIdx, currEntryOffset);
    }

    currData += EntrySize;
    currEntryInfo.entryBlob = {currData, currEntryBlobSize};
    currData += currEntryBlobSize;

    llvm::MD5 md5;
    md5.update(currEntryInfo.entryBlob);
    llvm::MD5::MD5Result result = {};
    md5.final(result);
    currEntryInfo.entryMD5Sum = result.digest();

    entriesInfoOut.push_back(std::move(currEntryInfo));
  }

  return llvm::Error::success();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const PublicVkHeaderInfo &info) {
  assert(info.publicHeader);
  const vk::PipelineCacheHeaderData &header = *info.publicHeader;

  os << "=== Vulkan Pipeline Cache Header ===\n"
     << "header length:\t\t" << header.headerLength << "\n"
     << "header version:\t\t" << header.headerVersion << "\n"
     << "vendor ID:\t\t" << llvm::format("0x%" PRIx32, header.vendorID) << "\n"
     << "device ID:\t\t" << llvm::format("0x%" PRIx32, header.deviceID) << "\n"
     << "pipeline cache UUID:\t" << cc::uuidToHexString(header.UUID) << "\n"
     << "trailing space:\t" << info.trailingSpaceBeforePrivateBlob << "\n";
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const BinaryCachePrivateHeaderInfo &info) {
  assert(info.privateHeader);
  const vk::PipelineBinaryCachePrivateHeader &header = *info.privateHeader;

  os << "=== Pipeline Binary Cache Private Header ===\n"
     << "header length:\t" << sizeof(header) << "\n"
     << "hash ID:\t" << llvm::format_bytes(header.hashId, std::nullopt, sizeof(header.hashId)) << "\n"
     << "content size:\t" << info.contentBlobSize << "\n";
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const BinaryCacheEntryInfo &info) {
  assert(info.entryHeader);
  const vk::BinaryCacheEntry &header = info.entryHeaderData;

  os << "\t*** Entry " << info.idx << " ***\n"
     << "\thash ID:\t\t"
     << "0x" << llvm::format_hex_no_prefix(header.hashId.qwords[0], sizeof(uint64_t) * 2) << " 0x"
     << llvm::format_hex_no_prefix(header.hashId.qwords[1], sizeof(uint64_t) * 2) << '\n'
     << "\tdata size:\t\t" << header.dataSize << "\n"
     << "\tcalculated MD5 sum:\t" << info.entryMD5Sum << "\n";
  return os;
}

// =====================================================================================================================
// Returns a map from the MD5 sum of a files contents to the file's path for every '.elf' file in `dir` or any of its
// subdirectories.
// If there are multiple '.elf' files sharing the same MD5, a single (arbitrary) file path is kept as the value of that
// map entry.
//
// @param dir : The directory to search
// @returns : Map from ELF MD5 sums to their paths on disk
llvm::StringMap<std::string> mapMD5SumsToElfFilePath(llvm::Twine dir) {
  namespace fs = llvm::sys::fs;
  llvm::StringMap<std::string> md5ToElfPath;

  std::error_code ec{};
  for (fs::recursive_directory_iterator it{dir, ec}, e{}; it != e && !ec; it.increment(ec)) {
    const std::string &path(it->path());
    if (!llvm::StringRef(path).endswith(".elf") || fs::is_directory(path))
      continue;

    llvm::ErrorOr<llvm::MD5::MD5Result> elfMD5OrErr = fs::md5_contents(path);
    if (std::error_code err = elfMD5OrErr.getError()) {
      llvm::errs() << "[WARN]: Can not read source ELF file " << path << ": " << err.message() << "\n";
      continue;
    }

    md5ToElfPath.insert({elfMD5OrErr->digest(), path});
  }

  return md5ToElfPath;
}

} // namespace cc
