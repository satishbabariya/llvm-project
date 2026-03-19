//===-- lib/AST/AST.cpp - AST Node Implementation -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/AST/AST.h"

namespace Swift {
namespace ast {

ASTNode::~ASTNode() = default;

} // namespace ast
} // namespace Swift
