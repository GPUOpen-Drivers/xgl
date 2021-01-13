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
#pragma once

#include "util/palMetroHash.h"
#include "include/binary_cache_serialization.h"

// These Xlib defines conflict with LLVM.
#undef Bool
#undef Status

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"

namespace cc {

constexpr uint32_t AMDVendorId = 0x1002; // See https://pci-ids.ucw.cz/read/PC/1002.

// The LLPC version number in the current source tree.
constexpr uint32_t BuildLlpcMajorVersion = CC_LLPC_MAJOR_VERSION;

VkAllocationCallbacks &getDefaultAllocCallbacks();

// Deleter class that enables PAL/XGL types allocated with VkAllocationCallbacks to be used with std::unique_ptr.
// This deleter calles the free function from the given callback, instead of using a plain `delete` operator.
struct AllocCallbacksDeleter {
  AllocCallbacksDeleter(VkAllocationCallbacks &callbacks);
  void operator()(void *mem);

private:
  VkAllocationCallbacks *m_callbacks;
};

constexpr size_t UuidLength = 36;
using UuidString = llvm::SmallString<UuidLength>;

// Serializes given UUID into a printable string.
UuidString uuidToHexString(llvm::ArrayRef<uint8_t> uuid);

// Converts a UUID string into its binary representation.
bool hexStringToUuid(llvm::StringRef hexStr, llvm::MutableArrayRef<uint8_t> outUuid);

struct ElfLlpcCacheInfo {
  Util::MetroHash::Hash cacheHash;
  uint32_t llpcMajorVersion;
  uint32_t llpcMinorVersion;
};

// Tries to extract cache hash and LLPC version from the elf file.
llvm::Expected<ElfLlpcCacheInfo> getElfLlpcCacheInfo(llvm::MemoryBufferRef elfBuffer);

// Creates portable PipelineBinaryCache files from relocatable LLPC elf files.
// This class is moveable but not copyable.
class RelocatableCacheCreator {
public:
  static size_t CalculateAnticipatedCacheFileSize(llvm::ArrayRef<size_t> inputElfSizes);
  static llvm::Expected<RelocatableCacheCreator> Create(uint32_t deviceId, llvm::ArrayRef<uint8_t> uuid,
                                                        llvm::ArrayRef<uint8_t> fingerprint,
                                                        llvm::MutableArrayRef<uint8_t> outputBuffer);

  RelocatableCacheCreator() = delete;
  RelocatableCacheCreator(const RelocatableCacheCreator &) = delete;
  RelocatableCacheCreator &operator=(const RelocatableCacheCreator &) = delete;

  RelocatableCacheCreator(RelocatableCacheCreator &&) = default;
  RelocatableCacheCreator &operator=(RelocatableCacheCreator &&) = default;

  llvm::Error addElf(llvm::MemoryBufferRef elfBuffer);
  llvm::Error finalize(size_t *outTotalNumEntries, size_t *outTotalSize);

private:
  RelocatableCacheCreator(std::unique_ptr<Util::IPlatformKey, AllocCallbacksDeleter> platformKey,
                          std::unique_ptr<vk::PipelineBinaryCacheSerializer> serializer,
                          llvm::MutableArrayRef<uint8_t> outputBuffer, VkAllocationCallbacks &callbacks)
      : m_platformKey(std::move(platformKey)), m_serializer(std::move(serializer)), m_outputBuffer(outputBuffer),
        m_callbacks(&callbacks) {}

  std::unique_ptr<Util::IPlatformKey, AllocCallbacksDeleter> m_platformKey;
  std::unique_ptr<vk::PipelineBinaryCacheSerializer> m_serializer;
  llvm::MutableArrayRef<uint8_t> m_outputBuffer;
  VkAllocationCallbacks *m_callbacks;
};

} // namespace cc
