//===-- swiftc/SIL/SILBuilder.h - SIL Builder Stub -------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILBUILDER_H
#define SWIFTC_SIL_SILBUILDER_H

#include "swiftc/SIL/SILDebugScope.h"
#include "swiftc/SIL/SILModule.h"

namespace swift {

class ASTContext;
class SILBasicBlock;
class SILDebugScope;
class SILFunction;
class SILInstruction;
class SILModule;

/// Stub: Class for creating SIL constructs.
class SILBuilder {
  friend class SILBuilderWithScope;

  SILFunction *F;
  SILBasicBlock *BB;
  const SILDebugScope *CurDebugScope = nullptr;

public:
  SILBuilder(SILFunction &F, bool isParsing = false);
  explicit SILBuilder(SILInstruction *I);
  explicit SILBuilder(SILBasicBlock *BB);

  SILFunction &getFunction() const { return *F; }
  SILModule &getModule() const;
  ASTContext &getASTContext() const;

  void setCurrentDebugScope(const SILDebugScope *DS) { CurDebugScope = DS; }
  const SILDebugScope *getCurrentDebugScope() const { return CurDebugScope; }

  bool hasValidInsertionPoint() const { return BB != nullptr; }
  SILBasicBlock *getInsertionBB() { return BB; }

  void clearInsertionPoint() { BB = nullptr; }
  void setInsertionPoint(SILInstruction *I);
  void setInsertionPoint(SILBasicBlock *BB);

  void emitBlock(SILBasicBlock *BB);

  SILBasicBlock *splitBlockForFallthrough();
};

/// Wrapper that sets debug scope from an insertion point.
class SILBuilderWithScope : public SILBuilder {
public:
  explicit SILBuilderWithScope(SILInstruction *I);
  explicit SILBuilderWithScope(SILBasicBlock *BB,
                               SILInstruction *InheritScopeFrom);
};

} // end swift namespace

#endif // SWIFTC_SIL_SILBUILDER_H
