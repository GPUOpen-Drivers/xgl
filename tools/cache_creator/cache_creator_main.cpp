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
#include "palPlatformKey.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/CommandLine.h"
#include <cassert>
#include <string>

namespace {
llvm::cl::OptionCategory CacheCreatorCat("Cache Creator Options");

llvm::cl::list<std::string> InFiles(llvm::cl::Positional, llvm::cl::OneOrMore, llvm::cl::ValueRequired,
                                    llvm::cl::cat(CacheCreatorCat), llvm::cl::desc("<Input elf file(s)>"));
llvm::cl::opt<std::string> OutFileName("o", llvm::cl::desc("Output filename"), llvm::cl::cat(CacheCreatorCat),
                                       llvm::cl::value_desc("filename"), llvm::cl::Required);
llvm::cl::opt<std::string> DeviceIdStr("device-id", llvm::cl::desc("Device ID"), llvm::cl::cat(CacheCreatorCat));
llvm::cl::opt<std::string> UUIDStr("uuid", llvm::cl::desc("UUID for the specific driver and machine"),
                                   llvm::cl::cat(CacheCreatorCat));
} // namespace

int main(int argc, char **argv) {
  for (auto &strOptionPair : llvm::cl::getRegisteredOptions()) {
    auto *opt = strOptionPair.second;

    if (opt && llvm::none_of(opt->Categories, [](const llvm::cl::OptionCategory *category) {
          return category && (category == &CacheCreatorCat || category->getName() == "Generic Options");
        })) {
      opt->setHiddenFlag(llvm::cl::Hidden);
    }
  }
  llvm::cl::ParseCommandLineOptions(argc, argv);

  Util::IPlatformKey *key = nullptr;
  (void)key;

  cc::printNotImplemented();
  assert(false && "Not implemented");
  return 1;
}
