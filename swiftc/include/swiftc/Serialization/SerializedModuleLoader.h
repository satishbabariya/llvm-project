//===--- SerializedModuleLoader.h - Stub -------------------------*- C++ -*-===//
//
// Minimal stub header for non-migrated Serialization module.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFTC_SERIALIZATION_MODULELOADER_H
#define SWIFTC_SERIALIZATION_MODULELOADER_H

#include "swiftc/AST/Module.h"
#include "swiftc/AST/ModuleLoader.h"
#include "llvm/Support/MemoryBuffer.h"

namespace swift {

class ModuleFile;

/// Stub: Imports serialized Swift modules into an ASTContext.
class SerializedModuleLoader : public ModuleLoader {
  ASTContext &Ctx;

  explicit SerializedModuleLoader(ASTContext &ctx, DependencyTracker *tracker);

public:
  static std::unique_ptr<SerializedModuleLoader>
  create(ASTContext &ctx, DependencyTracker *tracker = nullptr) {
    return std::unique_ptr<SerializedModuleLoader>{
        new SerializedModuleLoader(ctx, tracker)};
  }

  ~SerializedModuleLoader();

  SerializedModuleLoader(const SerializedModuleLoader &) = delete;
  SerializedModuleLoader &operator=(const SerializedModuleLoader &) = delete;

  bool canImportModule(std::pair<Identifier, SourceLoc> named) override;

  ModuleDecl *
  loadModule(SourceLoc importLoc,
             ArrayRef<std::pair<Identifier, SourceLoc>> path) override;

  FileUnit *loadAST(ModuleDecl &M, Optional<SourceLoc> diagLoc,
                    std::unique_ptr<llvm::MemoryBuffer> moduleInputBuffer,
                    std::unique_ptr<llvm::MemoryBuffer> moduleDocInputBuffer,
                    bool isFramework = false);

  void loadExtensions(NominalTypeDecl *nominal,
                      unsigned previousGeneration) override;

  void loadObjCMethods(
      ClassDecl *classDecl, ObjCSelector selector, bool isInstanceMethod,
      unsigned previousGeneration,
      llvm::TinyPtrVector<AbstractFunctionDecl *> &methods) override;

  void verifyAllModules() override;
};

/// A file-unit loaded from a serialized AST file.
class SerializedASTFile final : public LoadedFile {
  friend class SerializedModuleLoader;
  friend class ModuleFile;

  ModuleFile &File;
  bool IsSIB;

  ~SerializedASTFile() = default;

  SerializedASTFile(ModuleDecl &M, ModuleFile &file, bool isSIB = false)
      : LoadedFile(FileUnitKind::SerializedAST, M), File(file), IsSIB(isSIB) {}

public:
  bool isSIB() const { return IsSIB; }

  bool isSystemModule() const override;

  void lookupValue(ModuleDecl::AccessPathTy accessPath, DeclName name,
                   NLKind lookupKind,
                   SmallVectorImpl<ValueDecl *> &results) const override;

  TypeDecl *lookupLocalType(StringRef MangledName) const override;

  OperatorDecl *lookupOperator(Identifier name,
                               DeclKind fixity) const override;

  PrecedenceGroupDecl *
  lookupPrecedenceGroup(Identifier name) const override;

  void lookupVisibleDecls(ModuleDecl::AccessPathTy accessPath,
                          VisibleDeclConsumer &consumer,
                          NLKind lookupKind) const override;

  void lookupClassMembers(ModuleDecl::AccessPathTy accessPath,
                          VisibleDeclConsumer &consumer) const override;

  void lookupClassMember(ModuleDecl::AccessPathTy accessPath, DeclName name,
                         SmallVectorImpl<ValueDecl *> &decls) const override;

  void lookupObjCMethods(
      ObjCSelector selector,
      SmallVectorImpl<AbstractFunctionDecl *> &results) const override;

  Optional<CommentInfo> getCommentForDecl(const Decl *D) const override;
  Optional<StringRef> getGroupNameForDecl(const Decl *D) const override;
  Optional<StringRef> getSourceFileNameForDecl(const Decl *D) const override;
  Optional<unsigned> getSourceOrderForDecl(const Decl *D) const override;
  Optional<StringRef> getGroupNameByUSR(StringRef USR) const override;

  void collectAllGroups(std::vector<StringRef> &Names) const override;

  void getTopLevelDecls(SmallVectorImpl<Decl *> &results) const override;
  void getLocalTypeDecls(SmallVectorImpl<TypeDecl *> &results) const override;
  void getDisplayDecls(SmallVectorImpl<Decl *> &results) const override;

  void getImportedModules(
      SmallVectorImpl<ModuleDecl::ImportedModule> &imports,
      ModuleDecl::ImportFilter filter) const override;

  void
  collectLinkLibraries(ModuleDecl::LinkLibraryCallback callback) const override;

  Identifier
  getDiscriminatorForPrivateValue(const ValueDecl *D) const override;

  StringRef getFilename() const override;

  ClassDecl *getMainClass() const override;
  bool hasEntryPoint() const override;

  const clang::Module *getUnderlyingClangModule() override;

  static bool classof(const FileUnit *file) {
    return file->getKind() == FileUnitKind::SerializedAST;
  }
  static bool classof(const DeclContext *DC) {
    return isa<FileUnit>(DC) && classof(cast<FileUnit>(DC));
  }
};

} // end namespace swift

#endif
