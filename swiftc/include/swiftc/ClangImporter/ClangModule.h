//===--- ClangModule.h - Stub for ClangModule -------------------*- C++ -*-===//
//
// Minimal stub header for non-migrated ClangImporter module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_CLANGIMPORTER_CLANGMODULE_H
#define SWIFTC_CLANGIMPORTER_CLANGMODULE_H

#include "swiftc/AST/Module.h"

namespace clang {
  class ASTContext;
  class Module;
}

namespace swift {

class ASTContext;
class ClangImporter;
class ModuleLoader;

/// Stub: Represents a Clang module that has been imported into Swift.
class ClangModuleUnit final : public LoadedFile {
  ClangImporter &owner;
  const clang::Module *clangModule;

  ~ClangModuleUnit() = default;

public:
  /// True if the given Module contains an imported Clang module unit.
  static bool hasClangModule(ModuleDecl *M);

  ClangModuleUnit(ModuleDecl &M, ClangImporter &owner,
                  const clang::Module *clangModule);

  const clang::Module *getClangModule() const { return clangModule; }

  bool isTopLevel() const;
  ModuleDecl *getAdapterModule() const;

  bool isSystemModule() const override;

  void lookupValue(ModuleDecl::AccessPathTy accessPath,
                   DeclName name, NLKind lookupKind,
                   SmallVectorImpl<ValueDecl*> &results) const override;

  void lookupVisibleDecls(ModuleDecl::AccessPathTy accessPath,
                          VisibleDeclConsumer &consumer,
                          NLKind lookupKind) const override;

  void lookupClassMembers(ModuleDecl::AccessPathTy accessPath,
                          VisibleDeclConsumer &consumer) const override;

  void lookupClassMember(ModuleDecl::AccessPathTy accessPath, DeclName name,
                         SmallVectorImpl<ValueDecl*> &decls) const override;

  void lookupObjCMethods(
      ObjCSelector selector,
      SmallVectorImpl<AbstractFunctionDecl *> &results) const override;

  void getTopLevelDecls(SmallVectorImpl<Decl*> &results) const override;
  void getDisplayDecls(SmallVectorImpl<Decl*> &results) const override;

  void getImportedModules(
      SmallVectorImpl<ModuleDecl::ImportedModule> &imports,
      ModuleDecl::ImportFilter filter) const override;

  void getImportedModulesForLookup(
      SmallVectorImpl<ModuleDecl::ImportedModule> &imports) const override;

  void collectLinkLibraries(
      ModuleDecl::LinkLibraryCallback callback) const override;

  Identifier getDiscriminatorForPrivateValue(const ValueDecl *D) const override {
    llvm_unreachable("no private decls in Clang modules");
  }

  StringRef getFilename() const override;

  const clang::Module *getUnderlyingClangModule() override {
    return getClangModule();
  }

  clang::ASTContext &getClangASTContext() const;

  static bool classof(const FileUnit *file) {
    return file->getKind() == FileUnitKind::ClangModule;
  }
  static bool classof(const DeclContext *DC) {
    return isa<FileUnit>(DC) && classof(cast<FileUnit>(DC));
  }
};

} // namespace swift

#endif
