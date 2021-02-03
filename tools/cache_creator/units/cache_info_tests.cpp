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
#include "units/doctest.h"
#include "llvm/Support/MD5.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cassert>

static bool consumeErrorToBool(llvm::Error err) {
  const bool isError = bool(err);
  if (isError) {
    llvm::errs() << err << "\n";
    llvm::errs().flush();
  }
  llvm::consumeError(std::move(err));
  return isError;
}

template <typename T> static bool consumeErrorToBool(llvm::Expected<T> &valueOrErr) {
  return consumeErrorToBool(valueOrErr.takeError());
}

TEST_CASE("Empty cache blob") {
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::StringRef{}, "empty", false);
  assert(blobPtr);
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(consumeErrorToBool(blobInfoOrErr));
}

TEST_CASE("Large zero blob") {
  llvm::SmallVector<uint8_t> zeros(512u);
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(zeros), "zeros", false);
  assert(blobPtr);
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(!consumeErrorToBool(blobInfoOrErr));

  // This should fail as the self-declared header length is 0.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  CHECK(consumeErrorToBool(publicHeaderInfoOrErr));

  // This should fail as the self-declared header length is 0.
  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  CHECK(consumeErrorToBool(privateHeaderOffsetOrErr));
}

TEST_CASE("Public header only") {
  vk::PipelineCacheHeaderData publicHeader = {};
  publicHeader.headerLength = sizeof(publicHeader);
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(reinterpret_cast<const char *>(&publicHeader), sizeof(publicHeader)), "public_header", false);
  assert(blobPtr);

  // These should fail as the blob is not big enough to contain both a public and a private header.
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(consumeErrorToBool(blobInfoOrErr));
}

TEST_CASE("Public header length too long") {
  llvm::SmallVector<uint8_t> buffer(vk::VkPipelineCacheHeaderDataSize + sizeof(vk::PipelineBinaryCachePrivateHeader));
  auto *publicHeader = new (buffer.data()) vk::PipelineCacheHeaderData();
  publicHeader->headerLength = vk::VkPipelineCacheHeaderDataSize + 1;

  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(buffer), "public_header_len", false);
  assert(blobPtr);

  // This should succeed because we don't parse the public header at this point and don't know the public header length.
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(!consumeErrorToBool(blobInfoOrErr));

  // This should succeed as the public header can be fully parsed.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  CHECK(!consumeErrorToBool(publicHeaderInfoOrErr));
  CHECK(publicHeaderInfoOrErr->publicHeader == publicHeader);
  CHECK(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob == 1);

  // This should fail as the self-declared header length is too long to fit the private header.
  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  CHECK(consumeErrorToBool(privateHeaderOffsetOrErr));

  // This should fail as the self-declared header length is too long to fit the private header.
  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  CHECK(consumeErrorToBool(privateHeaderInfoOrErr));
}

TEST_CASE("Valid blob w/o entries") {
  llvm::SmallVector<uint8_t> buffer(vk::VkPipelineCacheHeaderDataSize + sizeof(vk::PipelineBinaryCachePrivateHeader));
  auto *publicHeader = new (buffer.data()) vk::PipelineCacheHeaderData();
  auto *privateHeader = new (buffer.data() + vk::VkPipelineCacheHeaderDataSize) vk::PipelineBinaryCachePrivateHeader();
  publicHeader->headerLength = vk::VkPipelineCacheHeaderDataSize;

  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(buffer), "valid_no_entries", false);
  assert(blobPtr);

  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(!consumeErrorToBool(blobInfoOrErr));

  // These should succeed as both headers can be fully parsed.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  CHECK(!consumeErrorToBool(publicHeaderInfoOrErr));
  CHECK(publicHeaderInfoOrErr->publicHeader == publicHeader);
  CHECK(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob == 0);

  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  CHECK(!consumeErrorToBool(privateHeaderOffsetOrErr));
  CHECK(*privateHeaderOffsetOrErr == vk::VkPipelineCacheHeaderDataSize);

  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  CHECK(!consumeErrorToBool(privateHeaderInfoOrErr));
  CHECK(privateHeaderInfoOrErr->privateHeader == privateHeader);
  CHECK(privateHeaderInfoOrErr->contentBlobSize == 0);

  auto contentOffsetOrErr = blobInfoOrErr->getCacheContentOffset();
  CHECK(!consumeErrorToBool(contentOffsetOrErr));
  CHECK(*contentOffsetOrErr == buffer.size());

  // This should succeed as it is valid to have an empty cache content, i.e., zero entries.
  llvm::SmallVector<cc::BinaryCacheEntryInfo, 0> entries;
  auto err = blobInfoOrErr->readBinaryCacheEntriesInfo(entries);
  CHECK(!consumeErrorToBool(std::move(err)));
  CHECK(entries.empty());
}

static cc::MD5DigestStr calculateMD5Sum(llvm::ArrayRef<uint8_t> data) {
  llvm::MD5 md5;
  md5.update(data);
  llvm::MD5::MD5Result result = {};
  md5.final(result);
  return result.digest();
}

TEST_CASE("Valid blob w/ one entry") {
  const size_t trailingSpace = 16;
  const size_t entrySize = sizeof(uint32_t);
  const size_t bufferSize = vk::VkPipelineCacheHeaderDataSize + trailingSpace +
                            sizeof(vk::PipelineBinaryCachePrivateHeader) + sizeof(vk::BinaryCacheEntry) + entrySize;
  llvm::SmallVector<uint8_t> buffer(bufferSize);
  uint8_t *currData = buffer.data();

  auto *publicHeader = new (currData) vk::PipelineCacheHeaderData();
  publicHeader->headerLength = vk::VkPipelineCacheHeaderDataSize + trailingSpace;
  currData += publicHeader->headerLength;

  auto *privateHeader = new (currData) vk::PipelineBinaryCachePrivateHeader();
  currData += sizeof(vk::PipelineBinaryCachePrivateHeader);

  auto *entryHeader = new (currData) vk::BinaryCacheEntry();
  entryHeader->dataSize = entrySize;
  currData += sizeof(vk::BinaryCacheEntry);

  auto *entryContent = new (currData) uint32_t(42);
  llvm::ArrayRef<uint8_t> entryBlob(currData, entrySize);

  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(buffer), "valid_one_entry", false);
  assert(blobPtr);

  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  CHECK(!consumeErrorToBool(blobInfoOrErr));

  // These should all succeed as both headers can be fully parsed and there's one valid entry.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  CHECK(!consumeErrorToBool(publicHeaderInfoOrErr));
  CHECK(publicHeaderInfoOrErr->publicHeader == publicHeader);
  CHECK(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob == trailingSpace);

  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  CHECK(!consumeErrorToBool(privateHeaderInfoOrErr));
  CHECK(privateHeaderInfoOrErr->privateHeader == privateHeader);
  CHECK(privateHeaderInfoOrErr->contentBlobSize == (sizeof(vk::BinaryCacheEntry) + entrySize));

  llvm::SmallVector<cc::BinaryCacheEntryInfo, 1> entries;
  auto err = blobInfoOrErr->readBinaryCacheEntriesInfo(entries);
  CHECK(!consumeErrorToBool(std::move(err)));
  CHECK(entries.size() == 1);

  cc::BinaryCacheEntryInfo &entry = entries.front();
  CHECK(entry.entryHeader == entryHeader);
  CHECK(entry.idx == 0);
  CHECK(entry.entryBlob.data() == entryBlob.data());
  CHECK(entry.entryBlob.size() == entryBlob.size());
  CHECK(entry.entryMD5Sum == calculateMD5Sum(entryBlob));
}
