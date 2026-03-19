//===-- swiftc/SIL/SILModule.h - SIL Module Stub ---------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILMODULE_H
#define SWIFTC_SIL_SILMODULE_H

#include "llvm/ADT/StringRef.h"
#include <memory>

namespace swift {

class ASTContext;
class ModuleDecl;
class SILOptions;

class SILModule {
public:
  ASTContext &getASTContext() const;
  static std::unique_ptr<SILModule> createEmptyModule(ModuleDecl *M,
                                                       SILOptions &Options) {
    return nullptr;
  }
};

} // namespace swift

#endif // SWIFTC_SIL_SILMODULE_H
