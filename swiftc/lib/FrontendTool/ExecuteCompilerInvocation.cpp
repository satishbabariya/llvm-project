//===-- lib/FrontendTool/ExecuteCompilerInvocation.cpp ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "swiftc/FrontendTool/Utils.h"
#include "swiftc/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

namespace Swift {
namespace frontend {

bool executeCompilerInvocation(CompilerInstance *swiftc) {
  // TODO: Execute the appropriate frontend actions based on the invocation.
  llvm::outs() << "swiftc: compilation not yet implemented\n";
  (void)swiftc;
  return true;
}

} // namespace frontend
} // namespace Swift
