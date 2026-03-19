//===-- lib/Sema/Sema.cpp - Semantic Analysis Implementation ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Sema/Sema.h"
#include "swiftc/AST/AST.h"

namespace Swift {
namespace sema {

Sema::Sema() = default;
Sema::~Sema() = default;

bool Sema::checkSemantics(ast::ASTNode *node) {
  if (!node)
    return false;
  // TODO: Implement semantic analysis
  return true;
}

} // namespace sema
} // namespace Swift
