//===--- LLVM.h - Import various common LLVM datatypes ----------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file forward declares and imports various common LLVM datatypes that
// swift wants to use unqualified.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_BASIC_LLVM_H
#define SWIFTC_BASIC_LLVM_H

// Do not proliferate #includes here, require clients to #include their
// dependencies.
// Casting.h has complex templates that cannot be easily forward declared.
#include "llvm/Support/Casting.h"
#include <optional>

// Forward declarations.
namespace llvm {
  // Containers.
  class StringRef;
  class Twine;
  template <typename T> class SmallPtrSetImpl;
  template <typename T, unsigned N> class SmallPtrSet;
  template <typename T> class SmallVectorImpl;
  template <typename T, unsigned N> class SmallVector;
  template <unsigned N> class SmallString;
  template<typename T> class ArrayRef;
  template<typename T> class MutableArrayRef;
  template<typename T> class TinyPtrVector;
  template <typename PT1, typename PT2> class PointerUnion;

  // Other common classes.
  class raw_ostream;
  class APInt;
  class APFloat;
} // end namespace llvm


namespace swift {
  // Casting operators.
  using llvm::isa;
  using llvm::cast;
  using llvm::dyn_cast;
  using llvm::dyn_cast_or_null;
  using llvm::cast_or_null;

  // Compatibility: wraps std::optional with the old llvm::Optional API
  // (hasValue(), getValue()) used throughout ported Swift code.
  inline constexpr std::nullopt_t None = std::nullopt;
  using NoneType = std::nullopt_t;

  template <typename T>
  class Optional : public std::optional<T> {
    using Base = std::optional<T>;
  public:
    using Base::Base;
    Optional() : Base() {}
    Optional(std::nullopt_t) : Base(std::nullopt) {}
    Optional(const T &V) : Base(V) {}
    Optional(T &&V) : Base(std::move(V)) {}
    Optional(const std::optional<T> &O) : Base(O) {}
    Optional(std::optional<T> &&O) : Base(std::move(O)) {}

    bool hasValue() const { return Base::has_value(); }
    T &getValue() { return Base::value(); }
    const T &getValue() const { return Base::value(); }
  };

  // Containers.
  using llvm::SmallPtrSetImpl;
  using llvm::SmallPtrSet;
  using llvm::SmallString;
  using llvm::StringRef;
  using llvm::Twine;
  using llvm::SmallVectorImpl;
  using llvm::SmallVector;
  using llvm::ArrayRef;
  using llvm::MutableArrayRef;
  using llvm::TinyPtrVector;
  using llvm::PointerUnion;

  // Other common classes.
  using llvm::raw_ostream;
  using llvm::APInt;
  using llvm::APFloat;
} // end namespace swift

#endif // SWIFTC_BASIC_LLVM_H
