//===-- swiftc/Sema/Sema.h - Semantic Analysis ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SEMA_SEMA_H
#define SWIFTC_SEMA_SEMA_H

namespace Swift {
namespace ast {
class ASTNode;
} // namespace ast

namespace sema {

class Sema {
public:
  Sema();
  ~Sema();

  bool checkSemantics(ast::ASTNode *node);
};

} // namespace sema
} // namespace Swift

#endif // SWIFTC_SEMA_SEMA_H
