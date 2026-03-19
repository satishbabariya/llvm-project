//===-- swiftc/Frontend/TextDiagnosticBuffer.h -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_TEXTDIAGNOSTICBUFFER_H
#define SWIFTC_FRONTEND_TEXTDIAGNOSTICBUFFER_H

#include "clang/Basic/Diagnostic.h"
#include <string>
#include <vector>

namespace Swift {
namespace frontend {

class TextDiagnosticBuffer : public clang::DiagnosticConsumer {
public:
  using DiagList = std::vector<std::pair<clang::SourceLocation, std::string>>;

private:
  DiagList errors;
  DiagList warnings;
  DiagList remarks;
  DiagList notes;

public:
  const DiagList &getErrors() const { return errors; }
  const DiagList &getWarnings() const { return warnings; }
  const DiagList &getRemarks() const { return remarks; }
  const DiagList &getNotes() const { return notes; }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level diagLevel,
                        const clang::Diagnostic &info) override;

  void flushDiagnostics(clang::DiagnosticsEngine &diags) const;
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_TEXTDIAGNOSTICBUFFER_H
