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
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <memory>

namespace Swift {
namespace frontend {

class CompilerInstance {
  CompilerInvocation invocation;
  std::shared_ptr<clang::DiagnosticsEngine> diagnostics;
  FrontendOptions frontendOpts;

public:
  CompilerInstance();
  ~CompilerInstance();

  CompilerInvocation &getInvocation() { return invocation; }
  const CompilerInvocation &getInvocation() const { return invocation; }

  bool hasDiagnostics() const { return diagnostics != nullptr; }
  clang::DiagnosticsEngine &getDiagnostics() const { return *diagnostics; }

  void createDiagnostics(
      clang::DiagnosticConsumer *client = nullptr,
      bool shouldOwnClient = true);

  FrontendOptions &getFrontendOpts() { return frontendOpts; }
  const FrontendOptions &getFrontendOpts() const { return frontendOpts; }

  void clearOutputFiles(bool eraseFiles);
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_COMPILERINSTANCE_H
