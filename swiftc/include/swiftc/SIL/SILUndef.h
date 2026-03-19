//===-- swiftc/SIL/SILUndef.h - Stub ---------------------------*- C++ -*-===//
//
// Stub header for non-migrated SIL module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SIL_SILUNDEF_H
#define SWIFTC_SIL_SILUNDEF_H

namespace swift {

class SILModule;
class SILType;

/// Stub: Represents an undefined SIL value.
class SILUndef {
public:
  static SILUndef *get(SILType Ty, SILModule *M);
  static SILUndef *get(SILType Ty, SILModule &M) { return get(Ty, &M); }
};

} // end swift namespace

#endif // SWIFTC_SIL_SILUNDEF_H
