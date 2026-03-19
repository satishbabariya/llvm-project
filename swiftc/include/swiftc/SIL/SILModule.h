//===-- swiftc/SIL/SILModule.h - SIL Module Stub ---------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILMODULE_H
#define SWIFTC_SIL_SILMODULE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include <memory>

namespace swift {

class ASTContext;
class ClassDecl;
class DeclContext;
class FileUnit;
class ModuleDecl;
class ProtocolConformance;
class ProtocolDecl;
class SILDebugScope;
class SILFunction;
class SILGlobalVariable;
class SILInstruction;
class SILOptions;
class SILType;
class SILUndef;
class SILVTable;
class SILWitnessTable;
class SILDefaultWitnessTable;
class SourceManager;
class ValueBase;
struct SILDeclRef;

namespace Lowering {
class SILGenModule;
class TypeConverter;
class TypeLowering;
} // namespace Lowering

/// A stage of SIL processing.
enum class SILStage {
  Raw,
  Canonical,
};

/// Stub: A SIL module.
class SILModule {
  llvm::BumpPtrAllocator BPA;
  ModuleDecl *TheSwiftModule;
  const DeclContext *AssociatedDeclContext;
  SILStage Stage;
  SILOptions &Options;
  bool wholeModule;

  SILModule(ModuleDecl *M, SILOptions &Options, const DeclContext *associatedDC,
            bool wholeModule);
  SILModule(const SILModule &) = delete;
  void operator=(const SILModule &) = delete;

public:
  ~SILModule();

  /// This converts Swift types to SILTypes.
  mutable Lowering::TypeConverter Types;

  ModuleDecl *getSwiftModule() const { return TheSwiftModule; }
  ASTContext &getASTContext() const;
  SourceManager &getSourceManager() const;

  const DeclContext *getAssociatedContext() const {
    return AssociatedDeclContext;
  }

  bool isWholeModule() const { return wholeModule; }
  SILOptions &getOptions() const { return Options; }
  SILStage getStage() const { return Stage; }
  void setStage(SILStage s) { Stage = s; }

  void eraseFunction(SILFunction *F);

  SILFunction *lookUpFunction(StringRef name) const;

  static std::unique_ptr<SILModule>
  createEmptyModule(ModuleDecl *M, SILOptions &Options,
                    bool WholeModule = false);

  void *allocate(unsigned Size, unsigned Align) const;
  void *allocateInst(unsigned Size, unsigned Align) const;
  void deallocateInst(SILInstruction *I);

  void verify() const;
  void dump(bool Verbose = false) const;

  const Lowering::TypeLowering &getTypeLowering(SILType t);
};

namespace Lowering {
bool usesObjCAllocator(ClassDecl *theClass);
} // namespace Lowering

} // end swift namespace

#endif // SWIFTC_SIL_SILMODULE_H
