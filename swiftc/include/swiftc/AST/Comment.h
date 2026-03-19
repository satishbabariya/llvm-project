//===--- Comment.h - Swift-specific comment parsing -------------*- C++ -*-===//
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

#ifndef SWIFTC_AST_COMMENT_H
#define SWIFTC_AST_COMMENT_H

#include "swiftc/Basic/LLVM.h"
#include "llvm/ADT/StringRef.h"

namespace swift {
class Decl;

/// A parsed documentation comment attached to a declaration.
/// Stores only the brief summary as a plain string — no Markup AST needed.
class DocComment {
  const Decl *D;
  StringRef Brief;

public:
  DocComment(const Decl *D, StringRef Brief) : D(D), Brief(Brief) {}

  const Decl *getDecl() const { return D; }

  Optional<StringRef> getBrief() const {
    if (Brief.empty())
      return None;
    return Brief;
  }

  bool isEmpty() const { return Brief.empty(); }
};

/// Get a parsed documentation comment for the declaration, if there is one.
Optional<DocComment *> getSingleDocComment(const Decl *D);

/// Attempt to get a doc comment from the declaration, or other inherited
/// sources, like from base classes or protocols.
Optional<DocComment *> getCascadingDocComment(const Decl *D);

} // namespace swift

#endif // SWIFTC_AST_COMMENT_H
