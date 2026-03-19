//===-- lib/Parse/Parser.cpp - Swift Parser Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Parse/Parser.h"
#include "swiftc/Parse/Lexer.h"
#include "llvm/Support/MemoryBuffer.h"

namespace Swift {
namespace parse {

Parser::Parser() = default;
Parser::~Parser() = default;

bool Parser::parseSourceFile(llvm::StringRef filename) {
  auto bufferOrErr = llvm::MemoryBuffer::getFile(filename);
  if (!bufferOrErr) {
    return false;
  }
  return parseBuffer(*bufferOrErr.get());
}

bool Parser::parseBuffer(const llvm::MemoryBuffer &buffer) {
  Lexer lexer(buffer.getBuffer());
  // TODO: Implement parsing logic
  (void)lexer;
  return true;
}

} // namespace parse
} // namespace Swift
