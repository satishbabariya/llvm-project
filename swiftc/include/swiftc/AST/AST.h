//===-- swiftc/AST/AST.h - AST Node Definitions -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_AST_AST_H
#define SWIFTC_AST_AST_H

#include "llvm/ADT/StringRef.h"
#include <memory>
#include <string>
#include <vector>

namespace Swift {
namespace ast {

class ASTNode {
public:
  enum class Kind {
    Module,
    Function,
    Variable,
    Expression,
    Statement,
  };

  ASTNode(Kind kind) : kind(kind) {}
  virtual ~ASTNode();

  Kind getKind() const { return kind; }

private:
  Kind kind;
};

class ModuleDecl : public ASTNode {
  std::string name;

public:
  ModuleDecl(llvm::StringRef name)
      : ASTNode(Kind::Module), name(name.str()) {}

  llvm::StringRef getName() const { return name; }
};

} // namespace ast
} // namespace Swift

#endif // SWIFTC_AST_AST_H
