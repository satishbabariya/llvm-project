//===-- lib/Frontend/CompilerInvocation.cpp --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Frontend/CompilerInvocation.h"
#include "llvm/TargetParser/Host.h"

namespace Swift {
namespace frontend {

bool CompilerInvocation::createFromArgs(
    CompilerInvocation &invocation,
    llvm::ArrayRef<const char *> commandLineArgs,
    clang::DiagnosticsEngine &diags, const char *argv0) {
  // Set default target triple
  invocation.getTargetOpts().triple = llvm::sys::getDefaultTargetTriple();

  // TODO: Parse command line arguments
  (void)commandLineArgs;
  (void)diags;
  (void)argv0;

  return true;
}

bool parseDiagnosticArgs(clang::DiagnosticOptions &diagOpts,
                         llvm::opt::ArgList &args) {
  // TODO: Parse diagnostic-specific arguments
  (void)diagOpts;
  (void)args;
  return true;
}

} // namespace frontend
} // namespace Swift
