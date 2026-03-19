//===-- swiftc/SIL/SILDebugScope.h - Stub ----------------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILDEBUGSCOPE_H
#define SWIFTC_SIL_SILDEBUGSCOPE_H

#include "llvm/ADT/SmallVector.h"

namespace swift {

class SILFunction;
class SILInstruction;
class SourceManager;

/// Stub: Represents a lexical debug scope in SIL.
class SILDebugScope {
public:
  const SILDebugScope *InlinedCallSite;

  SILFunction *getInlinedFunction() const;
  const SILDebugScope *getInlinedScope() const;
  SILFunction *getParentFunction() const;

  void dump(SourceManager &SM, llvm::raw_ostream &OS = llvm::errs(),
            unsigned Indent = 0) const;
};

#ifndef NDEBUG
bool maybeScopeless(SILInstruction &I);
#endif

/// Knows how to make a deep copy of a debug scope.
class ScopeCloner {
  SILFunction &NewFn;

public:
  ScopeCloner(SILFunction &NewFn);
  const SILDebugScope *
  getOrCreateClonedScope(const SILDebugScope *OrigScope);
};

} // namespace swift

#endif // SWIFTC_SIL_SILDEBUGSCOPE_H
