//===-- swiftc/Frontend/FrontendOptions.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_FRONTENDOPTIONS_H
#define SWIFTC_FRONTEND_FRONTENDOPTIONS_H

#include <string>
#include <vector>

namespace Swift {
namespace frontend {

class FrontendOptions {
public:
  bool printSupportedCPUs = false;

  std::vector<std::string> inputs;
  std::string outputFile;
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_FRONTENDOPTIONS_H
