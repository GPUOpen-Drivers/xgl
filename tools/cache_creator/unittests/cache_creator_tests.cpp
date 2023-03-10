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
#include "cache_creator.h"
#include "llvm/BinaryFormat/MsgPackDocument.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include <array>

namespace {

using llvm::Failed;
using llvm::Succeeded;

TEST(CacheCreatorTest, PlaceholderTestPass) {
  EXPECT_TRUE(true);
}

using UuidArray = std::array<uint8_t, 16>;

TEST(CacheCreatorTest, BasicUuidToString) {
  UuidArray uuid;

  uuid.fill(0);
  EXPECT_EQ(cc::uuidToHexString(uuid), "00000000-0000-0000-0000-000000000000");

  uuid[0] = 16;
  EXPECT_EQ(cc::uuidToHexString(uuid), "10000000-0000-0000-0000-000000000000");

  uuid[0] = 255;
  EXPECT_EQ(cc::uuidToHexString(uuid), "ff000000-0000-0000-0000-000000000000");

  uuid[0] = 0;
  uuid.back() = 1;
  EXPECT_EQ(cc::uuidToHexString(uuid), "00000000-0000-0000-0000-000000000001");

  uuid.back() = 15;
  EXPECT_EQ(cc::uuidToHexString(uuid), "00000000-0000-0000-0000-00000000000f");

  uuid.back() = 255;
  EXPECT_EQ(cc::uuidToHexString(uuid), "00000000-0000-0000-0000-0000000000ff");

  uuid[0] = 255;
  EXPECT_EQ(cc::uuidToHexString(uuid), "ff000000-0000-0000-0000-0000000000ff");
}

TEST(CacheCreatorTest, BasicHexStringToUuid) {
  UuidArray allZeros = {};
  UuidArray allOnes = {};
  allOnes.fill(255);

  UuidArray out = {};
  EXPECT_TRUE(cc::hexStringToUuid("00000000-0000-0000-0000-000000000000", out));
  EXPECT_EQ(out, allZeros);

  EXPECT_TRUE(cc::hexStringToUuid("10000000-0000-0000-0000-000000000000", out));
  EXPECT_EQ(out.front(), 16);
  EXPECT_EQ(out.back(), 0);

  EXPECT_TRUE(cc::hexStringToUuid("f0000000-0000-0000-0000-000000000000", out));
  EXPECT_EQ(out.front(), 240);
  EXPECT_EQ(out.back(), 0);

  EXPECT_TRUE(cc::hexStringToUuid("ff000000-0000-0000-0000-000000000000", out));
  EXPECT_EQ(out.front(), 255);
  EXPECT_EQ(out.back(), 0);

  EXPECT_TRUE(cc::hexStringToUuid("00000000-0000-0000-0000-000000000001", out));
  EXPECT_EQ(out.front(), 0);
  EXPECT_EQ(out.back(), 1);

  EXPECT_TRUE(cc::hexStringToUuid("00000000-0000-0000-0000-00000000000f", out));
  EXPECT_EQ(out.front(), 0);
  EXPECT_EQ(out.back(), 15);

  EXPECT_TRUE(cc::hexStringToUuid("00000000-0000-0000-0000-0000000000ff", out));
  EXPECT_EQ(out.front(), 0);
  EXPECT_EQ(out.back(), 255);

  EXPECT_TRUE(cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff", out));
  EXPECT_EQ(out, allOnes);
}

TEST(CacheCreatorTest, BadHexStringUuids) {
  UuidArray out = {};

  EXPECT_FALSE(cc::hexStringToUuid("", out));
  EXPECT_FALSE(cc::hexStringToUuid("----", out));

  EXPECT_FALSE(cc::hexStringToUuid("ffffffffffffffffffffffffffffffff", out));
  EXPECT_FALSE(cc::hexStringToUuid("fffffff-ffff-ffff-ffff-ffffffffffff", out));
  EXPECT_FALSE(cc::hexStringToUuid("0ffffffff-ffff-ffff-ffff-ffffffffffff", out));
  EXPECT_FALSE(cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff0", out));
  EXPECT_FALSE(cc::hexStringToUuid("ffffffff-ffff-ffff-ffff0ffffffffffff", out));
  EXPECT_FALSE(cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff-", out));

  EXPECT_FALSE(cc::hexStringToUuid("ffffffff\0-ffff-ffff-ffff-ffffffffffff", out));
  EXPECT_FALSE(cc::hexStringToUuid("gfffffff-ffff-ffff-ffff-ffffffffffff", out));

  EXPECT_FALSE(cc::hexStringToUuid("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF", out));

  EXPECT_FALSE(cc::hexStringToUuid("Hey, what's up?", out));

  UuidArray allZeros = {};
  EXPECT_EQ(out, allZeros);
}

TEST(CacheCreatorTest, FullUuidRoundtrip) {
  UuidArray uuid = {16, 2, 104, 108, 0, 3, 0, 0, 213, 232, 11, 199, 227, 23, 129, 116};
  cc::UuidString hexStr = cc::uuidToHexString(uuid);
  EXPECT_EQ(hexStr, "1002686c-0003-0000-d5e8-0bc7e3178174");

  UuidArray dumped = {};
  EXPECT_TRUE(cc::hexStringToUuid(hexStr, dumped));
  EXPECT_EQ(dumped, uuid);
}

TEST(CacheCreatorTest, GetCacheInfoFromInvalidPalMetadataBlob) {
  char badMetadata[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  llvm::StringRef noteBlob(badMetadata, sizeof(badMetadata));
  llvm::Expected<cc::ElfLlpcCacheInfo> cacheInfoOrErr = cc::getCacheInfoFromMetadataBlob(noteBlob);
  EXPECT_THAT_EXPECTED(cacheInfoOrErr, Failed());
}

TEST(CacheCreatorTest, GetCacheInfoFromValidPalMetadataBlob) {
  const char *sampleMetadata = R"(---
amdpal.pipelines:
  - .xgl_cache_info:
      .128_bit_cache_hash:
        - 17226562260713912943
        - 15513868906143827149
      .llpc_version:   !str '46.1'
...
)";

  llvm::msgpack::Document document;
  const bool parsingSucceeded = document.fromYAML(sampleMetadata);
  ASSERT_TRUE(parsingSucceeded) << "Failed to parse sample metadata";
  std::string noteBlob;
  document.writeToBlob(noteBlob);

  llvm::Expected<cc::ElfLlpcCacheInfo> cacheInfoOrErr = cc::getCacheInfoFromMetadataBlob(noteBlob);
  ASSERT_THAT_EXPECTED(cacheInfoOrErr, Succeeded());

  cc::ElfLlpcCacheInfo &elfLlpcInfo = *cacheInfoOrErr;
  EXPECT_EQ(elfLlpcInfo.cacheHash.qwords[0], 17226562260713912943u);
  EXPECT_EQ(elfLlpcInfo.cacheHash.qwords[1], 15513868906143827149u);
  EXPECT_EQ(elfLlpcInfo.llpcVersion.getMajor(), 46u);
  ASSERT_TRUE(elfLlpcInfo.llpcVersion.getMinor().has_value());
  EXPECT_EQ(*elfLlpcInfo.llpcVersion.getMinor(), 1u);
}

} // namespace
