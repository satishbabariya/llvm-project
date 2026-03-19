//===-- lib/Frontend/CompilerInstance.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Frontend/CompilerInstance.h"
#include "clang/Basic/DiagnosticIDs.h"

namespace Swift {
namespace frontend {

CompilerInstance::CompilerInstance() = default;
CompilerInstance::~CompilerInstance() = default;

void CompilerInstance::createDiagnostics(clang::DiagnosticConsumer *client,
                                         bool shouldOwnClient) {
  clang::DiagnosticOptions *diagOpts = new clang::DiagnosticOptions();
  diagnostics = std::make_shared<clang::DiagnosticsEngine>(
      clang::DiagnosticIDs::create(), *diagOpts, client, shouldOwnClient);
}

void CompilerInstance::clearOutputFiles(bool eraseFiles) {
  // TODO: Implement output file cleanup
  (void)eraseFiles;
}

} // namespace frontend
} // namespace Swift
