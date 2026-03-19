//===-- swiftc/SIL/SILArgument.h - Stub ------------------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILARGUMENT_H
#define SWIFTC_SIL_SILARGUMENT_H

namespace swift {

class SILBasicBlock;
class SILFunction;
class SILModule;
class SILType;
class ValueDecl;

/// Stub: Represents an argument to a SIL basic block.
class SILArgument {
  SILBasicBlock *ParentBB;
  const ValueDecl *Decl;

public:
  SILBasicBlock *getParent() { return ParentBB; }
  const SILBasicBlock *getParent() const { return ParentBB; }

  SILFunction *getFunction();
  const SILFunction *getFunction() const;

  SILModule &getModule() const;

  const ValueDecl *getDecl() const { return Decl; }
  unsigned getIndex() const;
};

class SILPHIArgument : public SILArgument {};
class SILFunctionArgument : public SILArgument {};

} // end swift namespace

#endif // SWIFTC_SIL_SILARGUMENT_H
