//===-- swiftc/Frontend/TextDiagnosticPrinter.h ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H
#define SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H

#include "llvm/Support/raw_ostream.h"
#include <string>

namespace Swift {
namespace frontend {

class TextDiagnosticPrinter {
  llvm::raw_ostream &os;
  std::string prefix;

public:
  TextDiagnosticPrinter(llvm::raw_ostream &os);
  ~TextDiagnosticPrinter();

  void setPrefix(std::string value) { prefix = std::move(value); }

  void handleDiagnostic(unsigned diagLevel, const std::string &message);
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_TEXTDIAGNOSTICPRINTER_H
