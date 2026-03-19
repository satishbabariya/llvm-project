//===-- swiftc/Frontend/FrontendAction.h -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_FRONTEND_FRONTENDACTION_H
#define SWIFTC_FRONTEND_FRONTENDACTION_H

namespace Swift {
namespace frontend {

class CompilerInstance;

class FrontendAction {
public:
  FrontendAction() = default;
  virtual ~FrontendAction();

  virtual bool beginAction(CompilerInstance &ci) { return true; }
  virtual void executeAction() = 0;
  virtual void endAction() {}
};

} // namespace frontend
} // namespace Swift

#endif // SWIFTC_FRONTEND_FRONTENDACTION_H
