//===-- swiftc/Frontend/CompilerInstance.h - Compiler Instance ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_COMPILERINSTANCE_H
#define SWIFTC_FRONTEND_COMPILERINSTANCE_H

#include "swiftc/Frontend/CompilerInvocation.h"
#include "swiftc/Frontend/FrontendOptions.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <memory>

namespace Swift {
namespace frontend {

class CompilerInstance {
  CompilerInvocation invocation;
  FrontendOptions frontendOpts;

public:
  CompilerInstance();
  ~CompilerInstance();

  CompilerInvocation &getInvocation() { return invocation; }
  const CompilerInvocation &getInvocation() const { return invocation; }

  FrontendOptions &getFrontendOpts() { return frontendOpts; }
  const FrontendOptions &getFrontendOpts() const { return frontendOpts; }

  void clearOutputFiles(bool eraseFiles);
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_COMPILERINSTANCE_H
