//===-- lib/Parse/Lexer.cpp - Swift Lexer Implementation -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Parse/Lexer.h"

namespace Swift {
namespace parse {

Lexer::Lexer(llvm::StringRef source)
    : source(source), current(source.data()), line(1), column(1) {}

Lexer::~Lexer() = default;

Token Lexer::nextToken() {
  // Skip whitespace
  while (current < source.end() && (*current == ' ' || *current == '\t' ||
                                     *current == '\n' || *current == '\r')) {
    if (*current == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
    ++current;
  }

  if (current >= source.end()) {
    return Token{TokenKind::Eof, "", line, column};
  }

  // TODO: Implement full lexer
  const char *start = current;
  ++current;
  ++column;
  return Token{TokenKind::Unknown, llvm::StringRef(start, 1), line,
               column - 1};
}

Token Lexer::peekToken() {
  const char *savedCurrent = current;
  unsigned savedLine = line;
  unsigned savedColumn = column;

  Token tok = nextToken();

  current = savedCurrent;
  line = savedLine;
  column = savedColumn;

  return tok;
}

} // namespace parse
} // namespace Swift
