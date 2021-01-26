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
#include "units/doctest.h"
#include <array>

TEST_CASE("Placeholder test pass") {
  CHECK(true);
}

using UuidArray = std::array<uint8_t, 16>;

TEST_CASE("Basic UUID to string") {
  UuidArray uuid;

  uuid.fill(0);
  CHECK(cc::uuidToHexString(uuid) == "00000000-0000-0000-0000-000000000000");

  uuid[0] = 16;
  CHECK(cc::uuidToHexString(uuid) == "10000000-0000-0000-0000-000000000000");

  uuid[0] = 255;
  CHECK(cc::uuidToHexString(uuid) == "ff000000-0000-0000-0000-000000000000");

  uuid[0] = 0;
  uuid.back() = 1;
  CHECK(cc::uuidToHexString(uuid) == "00000000-0000-0000-0000-000000000001");

  uuid.back() = 15;
  CHECK(cc::uuidToHexString(uuid) == "00000000-0000-0000-0000-00000000000f");

  uuid.back() = 255;
  CHECK(cc::uuidToHexString(uuid) == "00000000-0000-0000-0000-0000000000ff");

  uuid[0] = 255;
  CHECK(cc::uuidToHexString(uuid) == "ff000000-0000-0000-0000-0000000000ff");
}

TEST_CASE("Basic hex string to UUID") {
  UuidArray allZeros = {};
  UuidArray allOnes = {};
  allOnes.fill(255);

  UuidArray out = {};
  CHECK(cc::hexStringToUuid("00000000-0000-0000-0000-000000000000", out));
  CHECK(out == allZeros);

  CHECK(cc::hexStringToUuid("10000000-0000-0000-0000-000000000000", out));
  CHECK(out.front() == 16);
  CHECK(out.back() == 0);

  CHECK(cc::hexStringToUuid("f0000000-0000-0000-0000-000000000000", out));
  CHECK(out.front() == 240);
  CHECK(out.back() == 0);

  CHECK(cc::hexStringToUuid("ff000000-0000-0000-0000-000000000000", out));
  CHECK(out.front() == 255);
  CHECK(out.back() == 0);

  CHECK(cc::hexStringToUuid("00000000-0000-0000-0000-000000000001", out));
  CHECK(out.front() == 0);
  CHECK(out.back() == 1);

  CHECK(cc::hexStringToUuid("00000000-0000-0000-0000-00000000000f", out));
  CHECK(out.front() == 0);
  CHECK(out.back() == 15);

  CHECK(cc::hexStringToUuid("00000000-0000-0000-0000-0000000000ff", out));
  CHECK(out.front() == 0);
  CHECK(out.back() == 255);

  CHECK(cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff", out));
  CHECK(out == allOnes);
}

TEST_CASE("Bad hex string UUIDs") {
  UuidArray out = {};

  CHECK(!cc::hexStringToUuid("", out));
  CHECK(!cc::hexStringToUuid("----", out));

  CHECK(!cc::hexStringToUuid("ffffffffffffffffffffffffffffffff", out));
  CHECK(!cc::hexStringToUuid("fffffff-ffff-ffff-ffff-ffffffffffff", out));
  CHECK(!cc::hexStringToUuid("0ffffffff-ffff-ffff-ffff-ffffffffffff", out));
  CHECK(!cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff0", out));
  CHECK(!cc::hexStringToUuid("ffffffff-ffff-ffff-ffff0ffffffffffff", out));
  CHECK(!cc::hexStringToUuid("ffffffff-ffff-ffff-ffff-ffffffffffff-", out));

  CHECK(!cc::hexStringToUuid("ffffffff\0-ffff-ffff-ffff-ffffffffffff", out));
  CHECK(!cc::hexStringToUuid("gfffffff-ffff-ffff-ffff-ffffffffffff", out));

  CHECK(!cc::hexStringToUuid("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF", out));

  CHECK(!cc::hexStringToUuid("Hey, what's up?", out));

  UuidArray allZeros = {};
  CHECK(out == allZeros);
}

TEST_CASE("Full UUID roundtrip") {
  UuidArray uuid = {16, 2, 104, 108, 0, 3, 0, 0, 213, 232, 11, 199, 227, 23, 129, 116};
  cc::UuidString hexStr = cc::uuidToHexString(uuid);
  CHECK(hexStr == "1002686c-0003-0000-d5e8-0bc7e3178174");

  UuidArray dumped = {};
  CHECK(cc::hexStringToUuid(hexStr, dumped));
  CHECK(dumped == uuid);
}
