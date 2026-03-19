//===-- lib/Frontend/TextDiagnosticPrinter.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Frontend/TextDiagnosticPrinter.h"
#include "llvm/ADT/SmallString.h"

namespace Swift {
namespace frontend {

TextDiagnosticPrinter::TextDiagnosticPrinter(
    llvm::raw_ostream &os, const clang::DiagnosticOptions &diagOpts)
    : os(os), diagOpts(diagOpts) {}

TextDiagnosticPrinter::~TextDiagnosticPrinter() = default;

void TextDiagnosticPrinter::HandleDiagnostic(
    clang::DiagnosticsEngine::Level diagLevel,
    const clang::Diagnostic &info) {
  clang::DiagnosticConsumer::HandleDiagnostic(diagLevel, info);

  llvm::SmallString<256> message;
  info.FormatDiagnostic(message);

  if (!prefix.empty())
    os << prefix << ": ";

  switch (diagLevel) {
  case clang::DiagnosticsEngine::Ignored:
    break;
  case clang::DiagnosticsEngine::Note:
    os << "note: ";
    break;
  case clang::DiagnosticsEngine::Remark:
    os << "remark: ";
    break;
  case clang::DiagnosticsEngine::Warning:
    os << "warning: ";
    break;
  case clang::DiagnosticsEngine::Error:
    os << "error: ";
    break;
  case clang::DiagnosticsEngine::Fatal:
    os << "fatal error: ";
    break;
  }

  os << message << "\n";
}

} // namespace frontend
} // namespace Swift
