//===-- swiftc/SIL/InstructionUtils.h - Stub -------------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_INSTRUCTIONUTILS_H
#define SWIFTC_SIL_INSTRUCTIONUTILS_H

#include "llvm/ADT/NullablePtr.h"

namespace swift {

class SILFunction;
class SILInstruction;
class SILValue;

SILValue getUnderlyingObject(SILValue V);
SILValue getUnderlyingAddressRoot(SILValue V);
SILValue getUnderlyingObjectStopAtMarkDependence(SILValue V);
SILValue stripSinglePredecessorArgs(SILValue V);
SILValue stripCasts(SILValue V);
SILValue stripCastsWithoutMarkDependence(SILValue V);
SILValue stripUpCasts(SILValue V);
SILValue stripClassCasts(SILValue V);
SILValue stripAddressProjections(SILValue V);
SILValue stripUnaryAddressProjections(SILValue V);
SILValue stripValueProjections(SILValue V);
SILValue stripIndexingInsts(SILValue V);
SILValue stripExpectIntrinsic(SILValue V);

/// Evaluates whether a function has qualified or unqualified ownership.
class FunctionOwnershipEvaluator {
  NullablePtr<SILFunction> F;
  bool HasOwnershipQualifiedInstruction = false;

public:
  FunctionOwnershipEvaluator() {}
  FunctionOwnershipEvaluator(SILFunction *F) : F(F) {}
  void reset(SILFunction *NewF) {
    F = NewF;
    HasOwnershipQualifiedInstruction = false;
  }
  bool evaluate(SILInstruction *I);
};

} // end namespace swift

#endif // SWIFTC_SIL_INSTRUCTIONUTILS_H
