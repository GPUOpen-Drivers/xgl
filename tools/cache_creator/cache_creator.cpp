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
#include "palPlatformKey.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cinttypes>
#include <numeric>

#if _POSIX_C_SOURCE >= 200112L
#include <stdlib.h>
#endif

namespace cc {

namespace {
void *VKAPI_PTR defaultAllocFunc(void *userData, size_t size, size_t alignment, VkSystemAllocationScope allocType) {
  // On both POSIX and Windows, alignment is required to be a power of 2.
  // See https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc,
  // and https://linux.die.net/man/3/aligned_alloc.
  const size_t requiredAlignment = llvm::PowerOf2Ceil(std::max(alignment, sizeof(void *)));
#if defined(_WIN32)
  return _aligned_malloc(size, requiredAlignment);
#elif _POSIX_C_SOURCE >= 200112L
  void *mem = nullptr;
  if (posix_memalign(&mem, requiredAlignment, size) != 0)
    return nullptr;
  return mem;
#else
#error Platform not handled
#endif
}

void *VKAPI_PTR defaultReallocFunc(void *userData, void *original, size_t size, size_t alignment,
                                   VkSystemAllocationScope allocType) {
  llvm_unreachable("Reallocation not supported"); // See https://github.com/GPUOpen-Drivers/xgl/issues/70.
}

void VKAPI_PTR defaultFreeFunc(void *userData, void *mem) {
#if defined(_WIN32)
  return _aligned_free(mem);
#elif _POSIX_C_SOURCE >= 200112L
  free(mem);
#else
#error Platform not handled
#endif
}

void VKAPI_PTR defaultAllocNotification(void *userData, size_t size, VkInternalAllocationType allocationType,
                                        VkSystemAllocationScope allocationScope) {
  // No notification required.
}

void VKAPI_PTR defaultFreeNotification(void *userData, size_t size, VkInternalAllocationType allocationType,
                                       VkSystemAllocationScope allocationScope) {
  // No notification required.
}
} // namespace

// =====================================================================================================================
// Provides the default allocation callbacks, used by XGL code.
//
// @returns : Vulkan allocation callbacks
VkAllocationCallbacks &getDefaultAllocCallbacks() {
  static VkAllocationCallbacks defaultCallbacks = {
      nullptr, defaultAllocFunc, defaultReallocFunc, defaultFreeFunc, defaultAllocNotification, defaultFreeNotification,
  };
  return defaultCallbacks;
}

AllocCallbacksDeleter::AllocCallbacksDeleter(VkAllocationCallbacks &callbacks) : m_callbacks(&callbacks) {
}

void AllocCallbacksDeleter::operator()(void *mem) {
  m_callbacks->pfnFree(m_callbacks->pUserData, mem);
}

static bool isValidHexUUIDStr(llvm::StringRef hexStr) {
  // Sample valid UUID string: 12345678-abcd-ef00-ffff-0123456789ab,
  // see: https://en.wikipedia.org/wiki/Universally_unique_identifier.
  static const llvm::Regex uuidRegex("^[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12}$");
  return hexStr.size() == UuidLength && uuidRegex.match(hexStr);
}

// =====================================================================================================================
// Serializes given UUID into a printable string.
//
// @param uuid : The input UUID in the binary format
// @returns : UUID string in the printable format
UuidString uuidToHexString(llvm::ArrayRef<uint8_t> uuid) {
  assert(uuid.size() == VK_UUID_SIZE);

  UuidString res;
  llvm::raw_svector_ostream os(res);
  for (size_t groupSize : {4, 2, 2, 2, 6}) {
    os << llvm::format_bytes(uuid.take_front(groupSize), llvm::None,
                             /* NumPerLine = */ 16, /* BytesPerLine = */ 6);
    uuid = uuid.drop_front(groupSize);
    if (!uuid.empty())
      os << '-';
  }
  assert(isValidHexUUIDStr(res));
  return res;
}

static uint8_t hexDigitToNum(char c) {
  assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  return (c >= '0' && c <= '9') ? c - '0' : 10 + (c - 'a');
}

// =====================================================================================================================
// Converts a UUID string into its binary representation.
//
// @param hexStr : The input hex string in the UUID format
// @param [out] outUuid : The output buffer where de-serialized UUID gets written to
// @returns : `true` on success, `false` when the passed `hexStr` is not valid
bool hexStringToUuid(llvm::StringRef hexStr, llvm::MutableArrayRef<uint8_t> outUuid) {
  assert(outUuid.size() == VK_UUID_SIZE);
  if (!isValidHexUUIDStr(hexStr))
    return false;

  // Go over all hex digits and write out a nibble at a time.
  // Updates `outUuid` so that the current byte is always at the front.
  for (auto &&posAndHexDigit : llvm::enumerate(llvm::make_filter_range(hexStr, [](char c) { return c != '-'; }))) {
    const uint8_t num = hexDigitToNum(posAndHexDigit.value());
    if (posAndHexDigit.index() % 2 == 0) {
      outUuid.front() = num << 4;
    } else {
      outUuid.front() |= num;
      outUuid = outUuid.drop_front();
    }
  }

  return true;
}

// =====================================================================================================================
// Tries to extract cache hash and LLPC version from the elf file.
//
// @param elfBuffer : The input elf
// @returns : The hash found on success, or error on failure
llvm::Expected<ElfLlpcCacheInfo> getElfLlpcCacheInfo(llvm::MemoryBufferRef elfBuffer) {
  auto elfObjectOrErr = llvm::object::ELF64LEObjectFile::create(elfBuffer);
  if (auto err = elfObjectOrErr.takeError())
    return std::move(err);

  using ElfFileTy = llvm::object::ELF64LEFile;
  const ElfFileTy &elfFile = elfObjectOrErr->getELFFile();

  auto sectionsOrErr = elfFile.sections();
  if (auto err = sectionsOrErr.takeError())
    return std::move(err);

  for (const auto &sectionHeader : *sectionsOrErr) {
    if (sectionHeader.sh_type != llvm::ELF::SHT_NOTE)
      continue;

    auto sectionNameOrErr = elfFile.getSectionName(sectionHeader);
    if (auto err = sectionNameOrErr.takeError()) {
      llvm::consumeError(std::move(err));
      continue;
    }
    if (!sectionNameOrErr->startswith(".note"))
      continue;

    llvm::Error notesError(llvm::Error::success());
    auto noteRange = elfFile.notes(sectionHeader, notesError);
    if (notesError)
      return std::move(notesError);

    Util::MetroHash::Hash hash = {};
    bool foundHash = false;
    uint32_t llpcVersion[2] = {};
    bool foundLlpcVersion = false;

    for (const ElfFileTy::Elf_Note &note : noteRange) {
      llvm::StringRef noteName = note.getName();
      llvm::ArrayRef<uint8_t> noteBlob = note.getDesc();

      if (noteName.startswith("llpc_cache_hash")) {
        assert(noteBlob.size() == sizeof(Util::MetroHash::Hash) && "Invalid llpc_cache_hash note");
        memcpy(hash.bytes, noteBlob.data(), sizeof(hash));
        foundHash = true;
      } else if (noteName.startswith("llpc_version")) {
        assert(noteBlob.size() == sizeof(llpcVersion) && "Invalid llpc_version note");
        memcpy(llpcVersion, noteBlob.data(), sizeof(llpcVersion));
        foundLlpcVersion = true;
      }

      if (foundHash && foundLlpcVersion)
        return ElfLlpcCacheInfo{hash, llpcVersion[0], llpcVersion[1]};
    }
  }

  return llvm::createStringError(llvm::object::object_error::invalid_file_type, "Could not find shader/elf elf hash");
}

// =====================================================================================================================
// Computed the total size necessary to serialize a portable PipelineBinaryCache file.
//
// @param inputelfSizes : Sizes of input elf files (in bytes)
// @returns : Total size required to create cache file (in bytes)
size_t RelocatableCacheCreator::CalculateAnticipatedCacheFileSize(llvm::ArrayRef<size_t> inputElfSizes) {
  const size_t totalFileContentsSize = std::accumulate(inputElfSizes.begin(), inputElfSizes.end(), 0);
  const size_t numFiles = inputElfSizes.size();
  const size_t anticipatedBlobSize =
      vk::PipelineBinaryCacheSerializer::CalculateAnticipatedCacheBlobSize(numFiles, totalFileContentsSize);
  return vk::VkPipelineCacheHeaderDataSize + anticipatedBlobSize;
}

// =====================================================================================================================
// Initializes a RelocatableCacheCreator object.
//
// @param deviceId : The device identifier of the target GPU
// @param uuid : Pipeline cache UUID in the binary format
// @param fingerprint : Initial data used to initialize the platform key. This should include information about the
//                      target GPU and the driver/compiler stack used to construct the cache and later consume it.
// @param [in/out] outputBuffer : Memory buffer where the pipeline cache data will be written
// @returns : A RelocatableCacheCreator object on success, error when initialization failures
llvm::Expected<RelocatableCacheCreator> RelocatableCacheCreator::Create(uint32_t deviceId, llvm::ArrayRef<uint8_t> uuid,
                                                                        llvm::ArrayRef<uint8_t> fingerprint,
                                                                        llvm::MutableArrayRef<uint8_t> outputBuffer) {
  VkAllocationCallbacks &callbacks = cc::getDefaultAllocCallbacks();

  const Util::HashAlgorithm hashAlgo = Util::HashAlgorithm::Sha1;
  const size_t keyMemSize = Util::GetPlatformKeySize(hashAlgo);
  void *keyMem = callbacks.pfnAllocation(callbacks.pUserData, keyMemSize, 16, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
  assert(keyMem);

  uint8_t *initialData = fingerprint.empty() ? nullptr : const_cast<uint8_t *>(fingerprint.data());
  Util::IPlatformKey *key = nullptr;
  if (Util::CreatePlatformKey(hashAlgo, initialData, fingerprint.size(), keyMem, &key) != Util::Result::Success)
    return llvm::createStringError(std::errc::state_not_recoverable, "Failed to create platform key");

  assert(key);
  std::unique_ptr<Util::IPlatformKey, AllocCallbacksDeleter> managedKey(key, {callbacks});

  size_t vkHeaderBytes = 0;
  if (vk::WriteVkPipelineCacheHeaderData(outputBuffer.data(), outputBuffer.size(), cc::AMDVendorId, deviceId,
                                         const_cast<uint8_t *>(uuid.data()), uuid.size(),
                                         &vkHeaderBytes) != Util::Result::Success)
    return llvm::createStringError(std::errc::state_not_recoverable, "Failed to write Vulkan Pipeline Cache header");
  assert(vkHeaderBytes == vk::VkPipelineCacheHeaderDataSize);

  llvm::MutableArrayRef<uint8_t> privateCacheData = outputBuffer.drop_front(vkHeaderBytes);
  auto serializer = std::make_unique<vk::PipelineBinaryCacheSerializer>();
  assert(serializer);

  if (serializer->Initialize(privateCacheData.size(), privateCacheData.data()) != Util::Result::Success)
    return llvm::createStringError(std::errc::state_not_recoverable,
                                   "Failed to initialize PipelineBinaryCacheSerializer");

  return RelocatableCacheCreator(std::move(managedKey), std::move(serializer), outputBuffer, callbacks);
}

// =====================================================================================================================
// Adds a new cache entry with the provided elf file.
//
// @param elfBuffer : Buffer with a relocatable shader elf compiled with LLPC
// @returns : Error if it's not possible to process the elf or append it to the output buffer, or success
llvm::Error RelocatableCacheCreator::addElf(llvm::MemoryBufferRef elfBuffer) {
  auto elfLlpcInfoOrErr = cc::getElfLlpcCacheInfo(elfBuffer);
  if (auto err = elfLlpcInfoOrErr.takeError())
    return llvm::createFileError(elfBuffer.getBufferIdentifier(), std::move(err));

  vk::BinaryCacheEntry entry = {};
  entry.dataSize = elfBuffer.getBufferSize();
  entry.hashId = elfLlpcInfoOrErr->cacheHash;

  // TODO(kuhar): Also check if the LLPC minor version from the elf matches the current LLPC version from the build.
  // LLPC minor version is not currently available to this targer, so we need to update CMake to access it here.
  if (elfLlpcInfoOrErr->llpcMajorVersion != BuildLlpcMajorVersion)
    return llvm::createFileError(elfBuffer.getBufferIdentifier(),
                                 llvm::createStringError(std::errc::state_not_recoverable,
                                                         "Elf LLPC version (% " PRIu32
                                                         ")  not compatible with the tool LLPC version (%" PRIu32 ")",
                                                         elfLlpcInfoOrErr->llpcMajorVersion, BuildLlpcMajorVersion));

  if (m_serializer->AddPipelineBinary(&entry, elfBuffer.getBufferStart()) != Util::Result::Success)
    return llvm::createFileError(
        elfBuffer.getBufferIdentifier(),
        llvm::createStringError(std::errc::state_not_recoverable, "Failed to add cache entry"));

  return llvm::Error::success();
}

// =====================================================================================================================
// Finalizes the cache file and writes remaining validation data.
//
// @param [out] outTotalNumEntries : (Optional) Variable to store the total number of cache entries added
// @param [out] outTotalSize: (Optional) Variable to store the final cache blob size
// @returns : Error if it's not possible to finish cache serialization, or success.
llvm::Error RelocatableCacheCreator::finalize(size_t *outTotalNumEntries, size_t *outTotalSize) {
  size_t actualNumEntries = 0;
  size_t actualCacheSize = 0;
  if (m_serializer->Finalize(m_callbacks, m_platformKey.get(), &actualNumEntries, &actualCacheSize) !=
      Util::Result::Success)
    return llvm::createStringError(std::errc::state_not_recoverable, "Failed to serialize cache");

  if (outTotalNumEntries)
    *outTotalNumEntries = actualNumEntries;
  if (outTotalSize)
    *outTotalSize = actualCacheSize + vk::VkPipelineCacheHeaderDataSize;

  return llvm::Error::success();
}

} // namespace cc
