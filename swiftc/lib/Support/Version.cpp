//===-- lib/Support/Version.cpp - Version Information ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Support/Version.h"
#include "swiftc/Version.inc"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <cstring>

#ifdef SWIFTC_VENDOR
#define SWIFTC_VENDOR_SUFFIX SWIFTC_VENDOR
#else
#define SWIFTC_VENDOR_SUFFIX ""
#endif

namespace Swift {
namespace support {

std::string getSwiftcVersion() {
  return SWIFTC_VERSION_STRING;
}

std::string getSwiftcRepositoryPath() {
#ifdef SWIFTC_REPOSITORY_STRING
  return SWIFTC_REPOSITORY_STRING;
#else
  return "";
#endif
}

std::string getSwiftcFullRepositoryVersion() {
  std::string buf;
  llvm::raw_string_ostream os(buf);
#ifdef SWIFTC_REPOSITORY_STRING
  os << "(" << SWIFTC_REPOSITORY_STRING;
#endif
  // Try to get VCS info at build time.
#ifdef SWIFTC_REVISION
  os << " " << SWIFTC_REVISION;
#endif
#ifdef SWIFTC_REPOSITORY_STRING
  os << ")";
#endif
  return buf;
}

std::string getSwiftcFullVersion() {
  std::string buf;
  llvm::raw_string_ostream os(buf);
  os << SWIFTC_VENDOR_SUFFIX << "swiftc version " << SWIFTC_VERSION_STRING;
  std::string repo = getSwiftcFullRepositoryVersion();
  if (!repo.empty()) {
    os << " " << repo;
  }
  return buf;
}

llvm::StringRef getSwiftcToolFullVersion(llvm::StringRef toolName) {
  static std::string buf;
  if (buf.empty()) {
    llvm::raw_string_ostream os(buf);
    os << toolName << " version " << SWIFTC_VERSION_STRING;
    std::string repo = getSwiftcFullRepositoryVersion();
    if (!repo.empty()) {
      os << " " << repo;
    }
  }
  return buf;
}

} // namespace support
} // namespace Swift
