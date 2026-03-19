//===-- swiftc/Parse/Lexer.h - Swift Lexer ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_PARSE_LEXER_H
#define SWIFTC_PARSE_LEXER_H

#include "llvm/ADT/StringRef.h"

namespace Swift {
namespace parse {

enum class TokenKind {
  // Literals
  IntegerLiteral,
  FloatingLiteral,
  StringLiteral,

  // Identifiers
  Identifier,

  // Keywords
  KW_func,
  KW_var,
  KW_let,
  KW_if,
  KW_else,
  KW_while,
  KW_for,
  KW_in,
  KW_return,
  KW_struct,
  KW_class,
  KW_enum,
  KW_protocol,
  KW_import,
  KW_guard,
  KW_switch,
  KW_case,
  KW_default,

  // Punctuation
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  LeftBracket,
  RightBracket,
  Comma,
  Colon,
  Semicolon,
  Arrow,
  Dot,
  Equal,

  // Operators
  Plus,
  Minus,
  Star,
  Slash,

  // Special
  Eof,
  Unknown,
};

struct Token {
  TokenKind kind;
  llvm::StringRef text;
  unsigned line;
  unsigned column;
};

class Lexer {
public:
  Lexer(llvm::StringRef source);
  ~Lexer();

  Token nextToken();
  Token peekToken();

private:
  llvm::StringRef source;
  const char *current;
  unsigned line;
  unsigned column;
};

} // namespace parse
} // namespace Swift

#endif // SWIFTC_PARSE_LEXER_H
