//===-- swiftc/Support/Version.h - Version Information -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SUPPORT_VERSION_H
#define SWIFTC_SUPPORT_VERSION_H

#include "llvm/ADT/StringRef.h"
#include <string>

namespace Swift {
namespace support {

std::string getSwiftcVersion();
std::string getSwiftcRepositoryPath();
std::string getSwiftcFullRepositoryVersion();
std::string getSwiftcFullVersion();
llvm::StringRef getSwiftcToolFullVersion(llvm::StringRef toolName);

} // namespace support
} // namespace Swift

#endif // SWIFTC_SUPPORT_VERSION_H
