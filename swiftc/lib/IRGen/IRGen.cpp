//===-- lib/IRGen/IRGen.cpp - IR Generation Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/IRGen/IRGen.h"
#include "swiftc/AST/AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace Swift {
namespace irgen {

IRGenerator::IRGenerator(llvm::LLVMContext &context) : context(context) {}

IRGenerator::~IRGenerator() = default;

std::unique_ptr<llvm::Module> IRGenerator::generateIR(ast::ASTNode *node) {
  auto module = std::make_unique<llvm::Module>("swiftc_module", context);
  // TODO: Implement IR generation from AST
  (void)node;
  return module;
}

} // namespace irgen
} // namespace Swift
