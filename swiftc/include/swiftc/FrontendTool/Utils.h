//===-- swiftc/FrontendTool/Utils.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTENDTOOL_UTILS_H
#define SWIFTC_FRONTENDTOOL_UTILS_H

namespace Swift {
namespace frontend {

class CompilerInstance;

bool executeCompilerInvocation(CompilerInstance *swiftc);

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTENDTOOL_UTILS_H
