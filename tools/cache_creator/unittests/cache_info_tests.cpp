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
#include "llvm/Support/MD5.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include <cassert>

namespace {

using llvm::Failed;
using llvm::Succeeded;

TEST(CacheInfoTest, EmptyCacheBlob) {
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::StringRef{}, "empty", false);
  assert(blobPtr);
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  EXPECT_THAT_EXPECTED(blobInfoOrErr, Failed());
}

TEST(CacheInfoTest, LargeZeroBlob) {
  llvm::SmallVector<uint8_t> zeros(512u);
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(zeros), "zeros", false);
  assert(blobPtr);
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  ASSERT_THAT_EXPECTED(blobInfoOrErr, Succeeded());

  // This should fail as the self-declared header length is 0.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  EXPECT_THAT_EXPECTED(publicHeaderInfoOrErr, Failed());

  // This should fail as the self-declared header length is 0.
  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  EXPECT_THAT_EXPECTED(privateHeaderOffsetOrErr, Failed());
}

TEST(CacheInfoTest, PublicHeaderOnly) {
  vk::PipelineCacheHeaderData publicHeader = {};
  publicHeader.headerLength = sizeof(publicHeader);
  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(reinterpret_cast<const char *>(&publicHeader), sizeof(publicHeader)), "public_header", false);
  assert(blobPtr);

  // These should fail as the blob is not big enough to contain both a public and a private header.
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  EXPECT_THAT_EXPECTED(blobInfoOrErr, Failed());
}

TEST(CacheInfoTest, PublicHeaderLengthTooLong) {
  llvm::SmallVector<uint8_t> buffer(vk::VkPipelineCacheHeaderDataSize + sizeof(vk::PipelineBinaryCachePrivateHeader));
  auto *publicHeader = new (buffer.data()) vk::PipelineCacheHeaderData();
  publicHeader->headerLength = vk::VkPipelineCacheHeaderDataSize + 1;

  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(buffer), "public_header_len", false);
  assert(blobPtr);

  // This should succeed because we don't parse the public header at this point and don't know the public header length.
  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  ASSERT_THAT_EXPECTED(blobInfoOrErr, Succeeded());

  // This should succeed as the public header can be fully parsed.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  ASSERT_THAT_EXPECTED(publicHeaderInfoOrErr, Succeeded());
  EXPECT_EQ(publicHeaderInfoOrErr->publicHeader, publicHeader);
  EXPECT_EQ(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob, 1u);

  // This should fail as the self-declared header length is too long to fit the private header.
  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  EXPECT_THAT_EXPECTED(privateHeaderOffsetOrErr, Failed());

  // This should fail as the self-declared header length is too long to fit the private header.
  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  EXPECT_THAT_EXPECTED(privateHeaderInfoOrErr, Failed());
}

TEST(CacheInfoTest, ValidBlobNoEntries) {
  llvm::SmallVector<uint8_t> buffer(vk::VkPipelineCacheHeaderDataSize + sizeof(vk::PipelineBinaryCachePrivateHeader));
  auto *publicHeader = new (buffer.data()) vk::PipelineCacheHeaderData();
  auto *privateHeader = new (buffer.data() + vk::VkPipelineCacheHeaderDataSize) vk::PipelineBinaryCachePrivateHeader();
  publicHeader->headerLength = vk::VkPipelineCacheHeaderDataSize;

  auto blobPtr = llvm::MemoryBuffer::getMemBuffer(llvm::toStringRef(buffer), "valid_no_entries", false);
  assert(blobPtr);

  auto blobInfoOrErr = cc::CacheBlobInfo::create(*blobPtr);
  ASSERT_THAT_EXPECTED(blobInfoOrErr, Succeeded());

  // These should succeed as both headers can be fully parsed.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  ASSERT_THAT_EXPECTED(publicHeaderInfoOrErr, Succeeded());
  EXPECT_EQ(publicHeaderInfoOrErr->publicHeader, publicHeader);
  EXPECT_EQ(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob, 0u);

  auto privateHeaderOffsetOrErr = blobInfoOrErr->getPrivateHeaderOffset();
  ASSERT_THAT_EXPECTED(privateHeaderOffsetOrErr, Succeeded());
  EXPECT_EQ(*privateHeaderOffsetOrErr, vk::VkPipelineCacheHeaderDataSize);

  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  ASSERT_THAT_EXPECTED(privateHeaderInfoOrErr, Succeeded());
  EXPECT_EQ(privateHeaderInfoOrErr->privateHeader, privateHeader);
  EXPECT_EQ(privateHeaderInfoOrErr->contentBlobSize, 0u);

  auto contentOffsetOrErr = blobInfoOrErr->getCacheContentOffset();
  ASSERT_THAT_EXPECTED(contentOffsetOrErr, Succeeded());
  EXPECT_EQ(*contentOffsetOrErr, buffer.size());

  // This should succeed as it is valid to have an empty cache content, i.e., zero entries.
  llvm::SmallVector<cc::BinaryCacheEntryInfo, 0> entries;
  auto err = blobInfoOrErr->readBinaryCacheEntriesInfo(entries);
  EXPECT_THAT_ERROR(std::move(err), Succeeded());
  EXPECT_TRUE(entries.empty());
}

cc::MD5DigestStr calculateMD5Sum(llvm::ArrayRef<uint8_t> data) {
  llvm::MD5 md5;
  md5.update(data);
  llvm::MD5::MD5Result result = {};
  md5.final(result);
  return result.digest();
}

TEST(CacheInfoTest, DISABLED_ValidBlobOneEntry) {
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
  ASSERT_THAT_EXPECTED(blobInfoOrErr, Succeeded());

  // These should all succeed as both headers can be fully parsed and there's one valid entry.
  auto publicHeaderInfoOrErr = blobInfoOrErr->readPublicVkHeaderInfo();
  ASSERT_THAT_EXPECTED(publicHeaderInfoOrErr, Succeeded());
  EXPECT_EQ(publicHeaderInfoOrErr->publicHeader, publicHeader);
  EXPECT_EQ(publicHeaderInfoOrErr->trailingSpaceBeforePrivateBlob, trailingSpace);

  auto privateHeaderInfoOrErr = blobInfoOrErr->readBinaryCachePrivateHeaderInfo();
  ASSERT_THAT_EXPECTED(privateHeaderInfoOrErr, Succeeded());
  EXPECT_EQ(privateHeaderInfoOrErr->privateHeader, privateHeader);
  EXPECT_EQ(privateHeaderInfoOrErr->contentBlobSize, sizeof(vk::BinaryCacheEntry) + entrySize);

  llvm::SmallVector<cc::BinaryCacheEntryInfo, 1> entries;
  auto err = blobInfoOrErr->readBinaryCacheEntriesInfo(entries);
  ASSERT_THAT_ERROR(std::move(err), Succeeded());
  EXPECT_EQ(entries.size(), 1u);

  cc::BinaryCacheEntryInfo &entry = entries.front();
  EXPECT_EQ(entry.entryHeader, entryHeader);
  EXPECT_EQ(entry.idx, 0u);
  EXPECT_EQ(entry.entryBlob.data(), entryBlob.data());
  EXPECT_EQ(entry.entryBlob.size(), entryBlob.size());
  EXPECT_EQ(entry.entryMD5Sum, calculateMD5Sum(entryBlob));
}

} // namespace
