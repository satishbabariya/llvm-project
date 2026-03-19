//===-- swiftc/Parse/Parser.h - Swift Parser --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_PARSE_PARSER_H
#define SWIFTC_PARSE_PARSER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>

namespace Swift {
namespace parse {

class Parser {
public:
  Parser();
  ~Parser();

  bool parseSourceFile(llvm::StringRef filename);
  bool parseBuffer(const llvm::MemoryBuffer &buffer);
};

} // namespace parse
} // namespace Swift

#endif // SWIFTC_PARSE_PARSER_H
