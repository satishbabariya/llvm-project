//===-- swiftc/IRGen/IRGen.h - IR Generation --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_IRGEN_IRGEN_H
#define SWIFTC_IRGEN_IRGEN_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <memory>

namespace Swift {
namespace ast {
class ASTNode;
} // namespace ast

namespace irgen {

class IRGenerator {
public:
  IRGenerator(llvm::LLVMContext &context);
  ~IRGenerator();

  std::unique_ptr<llvm::Module> generateIR(ast::ASTNode *node);

private:
  llvm::LLVMContext &context;
};

} // namespace irgen
} // namespace Swift

#endif // SWIFTC_IRGEN_IRGEN_H
