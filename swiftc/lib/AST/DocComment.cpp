//===--- DocComment.cpp - Extraction of doc comments ----------------------===//
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
///
/// \file
/// This file implements extraction of documentation comments from a Swift
/// declaration.  The brief is extracted as a plain string (first paragraph of
/// the raw comment) with no Markup/cmark dependency.
///
//===----------------------------------------------------------------------===//

#include "swiftc/AST/Comment.h"
#include "swiftc/AST/Decl.h"
#include "swiftc/AST/RawComment.h"
#include "swiftc/AST/PrettyStackTrace.h"

using namespace swift;

/// Extract the text content of a raw comment, stripping comment markers
/// (///, /**, etc.) and leading decoration.
static StringRef extractCommentText(RawComment RC,
                                    SmallVectorImpl<char> &Scratch) {
  Scratch.clear();
  llvm::raw_svector_ostream OS(Scratch);

  for (const auto &C : RC.Comments) {
    StringRef Text = C.RawText;

    if (C.isLine()) {
      // Strip line comment markers (/// or //).
      unsigned Marker = 2 + (C.isOrdinary() ? 0 : 1);
      Text = Text.drop_front(Marker);
      Text = Text.rtrim("\n\r");
      // Strip one leading space if present.
      if (Text.starts_with(" "))
        Text = Text.drop_front(1);
      OS << Text << '\n';
    } else {
      // Strip block comment markers (/** ... */ or /* ... */).
      unsigned Marker = 2 + (C.isOrdinary() ? 0 : 1);
      Text = Text.drop_front(Marker);
      if (Text.ends_with("*/"))
        Text = Text.drop_back(2);
      else if (Text.ends_with("/"))
        Text = Text.drop_back(1);
      // Strip leading/trailing whitespace lines and * decorations.
      SmallVector<StringRef, 8> Lines;
      Text.split(Lines, '\n');
      for (auto Line : Lines) {
        StringRef Stripped = Line.ltrim();
        // Strip leading * decoration.
        if (Stripped.starts_with("* "))
          Stripped = Stripped.drop_front(2);
        else if (Stripped.starts_with("*") &&
                 (Stripped.size() == 1 || Stripped[1] == '\n'))
          Stripped = Stripped.drop_front(1);
        OS << Stripped << '\n';
      }
    }
  }

  return StringRef(Scratch.data(), Scratch.size());
}

/// Extract the first paragraph from comment text as the brief.
static StringRef extractBrief(StringRef Text) {
  // The brief is the first non-empty paragraph.
  // A paragraph ends at a blank line.
  StringRef Brief;
  bool InParagraph = false;
  size_t Start = 0;

  for (size_t i = 0; i <= Text.size(); ++i) {
    bool AtEnd = (i == Text.size());
    bool IsNewline = (!AtEnd && Text[i] == '\n');

    if (AtEnd || IsNewline) {
      StringRef Line = Text.slice(Start, i).ltrim().rtrim();
      if (Line.empty()) {
        if (InParagraph) {
          // End of first paragraph.
          return Brief;
        }
      } else {
        if (!InParagraph) {
          InParagraph = true;
          Brief = Line;
        } else {
          // Extend brief to include this line.
          // Brief spans from start of first line to end of this line.
          Brief = StringRef(Brief.data(),
                            Line.data() + Line.size() - Brief.data());
        }
      }
      Start = i + 1;
    }
  }

  return Brief;
}

Optional<DocComment *>
swift::getSingleDocComment(const Decl *D) {
  PrettyStackTraceDecl StackTrace("parsing comment for", D);

  auto RC = D->getRawComment();
  if (RC.isEmpty())
    return None;

  SmallVector<char, 256> Scratch;
  StringRef Text = extractCommentText(RC, Scratch);
  StringRef Brief = extractBrief(Text);

  if (Brief.empty())
    return None;

  // Copy brief into the ASTContext for lifetime management.
  auto &Ctx = D->getASTContext();
  StringRef BriefCopy = Ctx.AllocateCopy(Brief);

  // Allocate DocComment in the ASTContext.
  auto *DC = new (Ctx) DocComment(D, BriefCopy);
  return DC;
}

static Optional<DocComment *>
getAnyBaseClassDocComment(const ClassDecl *CD, const Decl *D) {
  if (const auto *VD = dyn_cast<ValueDecl>(D)) {
    const auto *BaseDecl = VD->getOverriddenDecl();
    while (BaseDecl) {
      auto Doc = getSingleDocComment(BaseDecl);
      if (Doc.hasValue())
        return Doc;
      BaseDecl = BaseDecl->getOverriddenDecl();
    }
  }
  return None;
}

static Optional<DocComment *>
getProtocolRequirementDocComment(const ProtocolDecl *ProtoExt,
                                 const Decl *D) {
  auto getSingleRequirementWithNonemptyDoc = [](const ProtocolDecl *P,
                                                const ValueDecl *VD)
    -> const ValueDecl * {
      SmallVector<ValueDecl *, 2> Members;
      P->lookupQualified(P->getDeclaredType(), VD->getFullName(),
                         NLOptions::NL_ProtocolMembers,
                         /*typeResolver=*/nullptr, Members);
    SmallVector<const ValueDecl *, 1> ProtocolRequirements;
    for (auto Member : Members)
      if (!Member->isDefinition())
        ProtocolRequirements.push_back(Member);

    if (ProtocolRequirements.size() == 1) {
      auto Requirement = ProtocolRequirements.front();
      if (!Requirement->getRawComment().isEmpty())
        return Requirement;
    }

    return nullptr;
  };

  if (const auto *VD = dyn_cast<ValueDecl>(D)) {
    SmallVector<const ValueDecl *, 4> RequirementsWithDocs;
    if (auto Requirement = getSingleRequirementWithNonemptyDoc(ProtoExt, VD))
      RequirementsWithDocs.push_back(Requirement);

    if (RequirementsWithDocs.size() == 1)
      return getSingleDocComment(RequirementsWithDocs.front());
  }
  return None;
}

Optional<DocComment *>
swift::getCascadingDocComment(const Decl *D) {
  auto Doc = getSingleDocComment(D);
  if (Doc.hasValue())
    return Doc;

  // If this refers to a class member, check base classes.
  if (const auto *CD = D->getDeclContext()->getAsClassOrClassExtensionContext())
    if (auto BaseClassDoc = getAnyBaseClassDocComment(CD, D))
      return BaseClassDoc;

  if (const auto *PE = D->getDeclContext()->getAsProtocolExtensionContext())
    if (auto ReqDoc = getProtocolRequirementDocComment(PE, D))
      return ReqDoc;

  return None;
}
