//===--- ClangImporter.h - Stub for ClangImporter ---------------*- C++ -*-===//
//
// Minimal stub header for non-migrated ClangImporter module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_CLANG_IMPORTER_H
#define SWIFTC_CLANG_IMPORTER_H

#include "swiftc/AST/ClangModuleLoader.h"

/// The maximum number of SIMD vector elements we currently try to import.
#define SWIFT_MAX_IMPORTED_SIMD_ELEMENTS 4

namespace swift {

class ASTContext;
class ClangImporterOptions;
class ClangModuleUnit;
class Decl;
class DeclContext;
class ImportDecl;
class IRGenOptions;
class LazyResolver;
class ModuleDecl;
class NominalTypeDecl;
class VisibleDeclConsumer;

/// Stub: Class that imports Clang modules into Swift.
class ClangImporter final : public ClangModuleLoader {
public:
  class Implementation;

  static std::unique_ptr<ClangImporter>
  create(ASTContext &ctx, const ClangImporterOptions &clangImporterOpts,
         DependencyTracker *tracker = nullptr);

  ~ClangImporter();

  ClangImporter(const ClangImporter &) = delete;
  ClangImporter &operator=(const ClangImporter &) = delete;

  bool canImportModule(std::pair<Identifier, SourceLoc> named) override;

  ModuleDecl *loadModule(
      SourceLoc importLoc,
      ArrayRef<std::pair<Identifier, SourceLoc>> path) override;

  void loadExtensions(NominalTypeDecl *nominal,
                      unsigned previousGeneration) override;

  void loadObjCMethods(
      ClassDecl *classDecl,
      ObjCSelector selector,
      bool isInstanceMethod,
      unsigned previousGeneration,
      llvm::TinyPtrVector<AbstractFunctionDecl *> &methods) override;

  bool addSearchPath(StringRef newSearchPath, bool isFramework) override;

  ModuleDecl *getImportedHeaderModule() const override;

  void verifyAllModules() override;

  void setTypeResolver(LazyResolver &resolver);
  void clearTypeResolver();

  clang::ASTContext &getClangASTContext() const override;
  clang::Preprocessor &getClangPreprocessor() const override;
  clang::Sema &getClangSema() const override;

  void printStatistics() const override;

  static bool isModuleImported(const clang::Module *M);
};

} // namespace swift

#endif
