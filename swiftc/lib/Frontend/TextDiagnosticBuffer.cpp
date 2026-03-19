//===-- lib/Frontend/TextDiagnosticBuffer.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/Frontend/TextDiagnosticBuffer.h"
#include "llvm/ADT/SmallString.h"

namespace Swift {
namespace frontend {

void TextDiagnosticBuffer::HandleDiagnostic(
    clang::DiagnosticsEngine::Level diagLevel,
    const clang::Diagnostic &info) {
  clang::DiagnosticConsumer::HandleDiagnostic(diagLevel, info);

  llvm::SmallString<256> buf;
  info.FormatDiagnostic(buf);

  switch (diagLevel) {
  case clang::DiagnosticsEngine::Ignored:
    break;
  case clang::DiagnosticsEngine::Note:
    notes.emplace_back(info.getLocation(), std::string(buf));
    break;
  case clang::DiagnosticsEngine::Remark:
    remarks.emplace_back(info.getLocation(), std::string(buf));
    break;
  case clang::DiagnosticsEngine::Warning:
    warnings.emplace_back(info.getLocation(), std::string(buf));
    break;
  case clang::DiagnosticsEngine::Error:
  case clang::DiagnosticsEngine::Fatal:
    errors.emplace_back(info.getLocation(), std::string(buf));
    break;
  }
}

void TextDiagnosticBuffer::flushDiagnostics(
    clang::DiagnosticsEngine &diags) const {
  for (const auto &note : notes)
    diags.Report(diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                       "%0"))
        << note.second;
  for (const auto &warning : warnings)
    diags.Report(diags.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                       "%0"))
        << warning.second;
  for (const auto &error : errors)
    diags.Report(diags.getCustomDiagID(clang::DiagnosticsEngine::Error,
                                       "%0"))
        << error.second;
}

} // namespace frontend
} // namespace Swift
