//===-- swiftc/Frontend/CompilerInvocation.h ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_COMPILERINVOCATION_H
#define SWIFTC_FRONTEND_COMPILERINVOCATION_H

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"

namespace Swift {
namespace frontend {

struct TargetOptions {
  std::string triple;
};

class CompilerInvocation {
  TargetOptions targetOpts;

public:
  CompilerInvocation() = default;

  static bool createFromArgs(CompilerInvocation &invocation,
                             llvm::ArrayRef<const char *> commandLineArgs,
                             clang::DiagnosticsEngine &diags,
                             const char *argv0 = nullptr);

  TargetOptions &getTargetOpts() { return targetOpts; }
  const TargetOptions &getTargetOpts() const { return targetOpts; }
};

bool parseDiagnosticArgs(clang::DiagnosticOptions &diagOpts,
                         llvm::opt::ArgList &args);

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_COMPILERINVOCATION_H
