//===-- swiftc/SIL/AbstractionPattern.h - Stub -----------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_ABSTRACTIONPATTERN_H
#define SWIFTC_SIL_ABSTRACTIONPATTERN_H

#include "swiftc/AST/Types.h"

namespace swift {
namespace Lowering {

/// Stub: A pattern for the abstraction of a value.
class AbstractionPattern {
  CanType OrigType;

public:
  AbstractionPattern() {}
  explicit AbstractionPattern(CanType origType) : OrigType(origType) {}
  explicit AbstractionPattern(Type origType);
  explicit AbstractionPattern(CanGenericSignature signature, CanType origType);

  static AbstractionPattern getOpaque();
  static AbstractionPattern getInvalid();

  bool isValid() const;
  bool isTypeParameter() const;

  CanType getType() const { return OrigType; }

  AbstractionPattern getFunctionResultType() const;
  AbstractionPattern getFunctionInputType() const;
  AbstractionPattern getTupleElementType(unsigned index) const;

  void dump() const;
  void print(raw_ostream &OS) const;
};

} // namespace Lowering
} // namespace swift

#endif // SWIFTC_SIL_ABSTRACTIONPATTERN_H
