//===-- swiftc/Markup/Markup.h - Markup AST --------------------*- C++ -*-===//
//
// Stub header for non-migrated Markup module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_MARKUP_MARKUP_H
#define SWIFTC_MARKUP_MARKUP_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"

namespace swift {

struct RawComment;

namespace markup {

class MarkupASTNode;
class Document;
class Paragraph;
class ParamField;
class ReturnsField;
class ThrowsField;
class LineList;

/// Parsed comment parts extracted from markup.
struct CommentParts {
  Optional<const Paragraph *> Brief;
  Optional<const ReturnsField *> ReturnsField;
  Optional<const ThrowsField *> ThrowsField;
  ArrayRef<const ParamField *> ParamFields;
  ArrayRef<const MarkupASTNode *> BodyNodes;

  bool isEmpty() const {
    return !Brief.hasValue() && !ReturnsField.hasValue() &&
           !ThrowsField.hasValue() && ParamFields.empty() &&
           BodyNodes.empty();
  }
};

class MarkupContext {
  llvm::BumpPtrAllocator Allocator;

public:
  void *allocate(unsigned long Bytes, unsigned Alignment) {
    return Allocator.Allocate(Bytes, Alignment);
  }

  template <typename T, typename It>
  T *allocateCopy(It Begin, It End) {
    T *Res =
        static_cast<T *>(allocate(sizeof(T) * (End - Begin), alignof(T)));
    for (unsigned i = 0; Begin != End; ++Begin, ++i)
      new (Res + i) T(*Begin);
    return Res;
  }

  template <typename T>
  MutableArrayRef<T> allocateCopy(ArrayRef<T> Array) {
    return MutableArrayRef<T>(allocateCopy<T>(Array.begin(), Array.end()),
                              Array.size());
  }

  StringRef allocateCopy(StringRef Str) {
    ArrayRef<char> Result =
        allocateCopy<char, const char *>(Str.data(), Str.data() + Str.size());
    return StringRef(Result.data(), Result.size());
  }

  LineList getLineList(swift::RawComment RC);
};

Document *parseDocument(MarkupContext &MC, LineList &LL);

} // namespace markup
} // namespace swift

#endif // SWIFTC_MARKUP_MARKUP_H
