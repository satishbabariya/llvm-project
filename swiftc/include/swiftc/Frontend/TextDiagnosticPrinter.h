//===-- swiftc/Frontend/TextDiagnosticPrinter.h ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H
#define SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace Swift {
namespace frontend {

class TextDiagnosticPrinter : public clang::DiagnosticConsumer {
  llvm::raw_ostream &os;
  const clang::DiagnosticOptions &diagOpts;
  std::string prefix;

public:
  TextDiagnosticPrinter(llvm::raw_ostream &os,
                        const clang::DiagnosticOptions &diagOpts);
  ~TextDiagnosticPrinter() override;

  void setPrefix(std::string value) { prefix = std::move(value); }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level diagLevel,
                        const clang::Diagnostic &info) override;
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H
