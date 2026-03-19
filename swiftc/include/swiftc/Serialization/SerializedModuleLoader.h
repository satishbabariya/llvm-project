//===-- swiftc/Serialization/SerializedModuleLoader.h -----------*- C++ -*-===//
//
// Stub header for non-migrated Serialization module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SERIALIZATION_SERIALIZEDMODULELOADER_H
#define SWIFTC_SERIALIZATION_SERIALIZEDMODULELOADER_H

#include "swiftc/AST/ModuleLoader.h"

namespace swift {

class SerializedModuleLoader : public ModuleLoader {
public:
  static std::unique_ptr<SerializedModuleLoader>
  create(ASTContext &ctx, DependencyTracker *tracker = nullptr) {
    return nullptr;
  }
};

} // namespace swift

#endif // SWIFTC_SERIALIZATION_SERIALIZEDMODULELOADER_H
