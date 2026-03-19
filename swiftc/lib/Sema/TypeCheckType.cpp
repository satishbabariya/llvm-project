//===--- TypeCheckType.cpp - Type Validation ------------------------------===//
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
// This file implements validation for Swift types, emitting semantic errors as
// appropriate and checking default initializer values.
//
//===----------------------------------------------------------------------===//

#include "TypeChecker.h"
#include "GenericTypeResolver.h"
#include "TypeCheckAvailability.h"

#include "swiftc/Strings.h"
#include "swiftc/AST/ASTVisitor.h"
#include "swiftc/AST/ASTWalker.h"
#include "swiftc/AST/ForeignErrorConvention.h"
#include "swiftc/AST/GenericEnvironment.h"
#include "swiftc/AST/NameLookup.h"
#include "swiftc/AST/PrettyStackTrace.h"
#include "swiftc/AST/TypeLoc.h"
#include "swiftc/Basic/SourceManager.h"
#include "swiftc/Basic/Statistic.h"
#include "swiftc/Basic/StringExtras.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/SaveAndRestore.h"

using namespace swift;

#define DEBUG_TYPE "TypeCheckType"

GenericTypeResolver::~GenericTypeResolver() { }

Type TypeChecker::getArraySliceType(SourceLoc loc, Type elementType) {
  if (!Context.getArrayDecl()) {
    diagnose(loc, diag::sugar_type_not_found, 0);
    return Type();
  }

  return ArraySliceType::get(elementType);
}

Type TypeChecker::getDictionaryType(SourceLoc loc, Type keyType, 
                                    Type valueType) {
  if (!Context.getDictionaryDecl()) {
    diagnose(loc, diag::sugar_type_not_found, 3);
    return Type();
  }

    return DictionaryType::get(keyType, valueType);
}

Type TypeChecker::getOptionalType(SourceLoc loc, Type elementType) {
  if (!Context.getOptionalDecl()) {
    diagnose(loc, diag::sugar_type_not_found, 1);
    return Type();
  }

  return OptionalType::get(elementType);
}

Type TypeChecker::getImplicitlyUnwrappedOptionalType(SourceLoc loc, Type elementType) {
  if (!Context.getImplicitlyUnwrappedOptionalDecl()) {
    diagnose(loc, diag::sugar_type_not_found, 2);
    return Type();
  }

  return ImplicitlyUnwrappedOptionalType::get(elementType);
}

static Type getStdlibType(TypeChecker &TC, Type &cached, DeclContext *dc,
                          StringRef name) {
  if (cached.isNull()) {
    ModuleDecl *stdlib = TC.Context.getStdlibModule();
    LookupTypeResult lookup = TC.lookupMemberType(dc, ModuleType::get(stdlib),
                                                  TC.Context.getIdentifier(
                                                    name));
    if (lookup)
      cached = lookup.back().second;
  }
  return cached;
}

Type TypeChecker::getStringType(DeclContext *dc) {
  return ::getStdlibType(*this, StringType, dc, "String");
}
Type TypeChecker::getInt8Type(DeclContext *dc) {
  return ::getStdlibType(*this, Int8Type, dc, "Int8");
}
Type TypeChecker::getUInt8Type(DeclContext *dc) {
  return ::getStdlibType(*this, UInt8Type, dc, "UInt8");
}

/// Find the standard type of exceptions.
///
/// We call this the "exception type" to try to avoid confusion with
/// the AST's ErrorType node.
Type TypeChecker::getExceptionType(DeclContext *dc, SourceLoc loc) {
  if (NominalTypeDecl *decl = Context.getErrorDecl())
    return decl->getDeclaredType();

  // Not really sugar, but the actual diagnostic text is fine.
  diagnose(loc, diag::sugar_type_not_found, 4);
  return Type();
}

static Type getObjectiveCNominalType(TypeChecker &TC,
                                     Type &cache,
                                     Identifier ModuleName,
                                     Identifier TypeName,
                                     DeclContext *dc) {
  if (cache)
    return cache;

  auto &Context = TC.Context;

  // FIXME: Does not respect visibility of the module.
  ModuleDecl *module = Context.getLoadedModule(ModuleName);
  if (!module)
    return nullptr;

  NameLookupOptions lookupOptions
    = defaultMemberLookupOptions |
      NameLookupFlags::KnownPrivate;
  if (auto result = TC.lookupMemberType(dc, ModuleType::get(module), TypeName,
                                        lookupOptions)) {
    for (auto pair : result) {
      if (auto nominal = dyn_cast<NominalTypeDecl>(pair.first)) {
        cache = nominal->getDeclaredType();
        return cache;
      }
    }
  }

  return nullptr;
}

void TypeChecker::forceExternalDeclMembers(NominalTypeDecl *nominalDecl) {
  // Force any delayed members added to the nominal type declaration.
  if (nominalDecl->hasDelayedMembers()) {
    this->handleExternalDecl(nominalDecl);
    nominalDecl->setHasDelayedMembers(false);
  }
}

// Walk up through the type scopes to find the context containing the type
// being resolved.
//
// FIXME: UnqualifiedLookup already has this information; it needs to be
// plumbed through.
static std::tuple<DeclContext *, NominalTypeDecl *, bool>
findDeclContextForType(TypeChecker &TC,
                       TypeDecl *typeDecl,
                       DeclContext *fromDC,
                       TypeResolutionOptions options) {

  auto ownerDC = typeDecl->getDeclContext();

  // If the type is declared at the top level, there's nothing we can learn from
  // walking our parent contexts.
  if (ownerDC->isModuleScopeContext())
    return std::make_tuple(ownerDC, nullptr, true);

  // Workaround for issue where generic typealias generic parameters are
  // looked up with the wrong 'fromDC'.
  if (isa<TypeAliasDecl>(ownerDC)) {
    assert(isa<GenericTypeParamDecl>(typeDecl));
    return std::make_tuple(ownerDC, nullptr, true);
  }

  bool needsBaseType = (ownerDC->isTypeContext() &&
                        !isa<GenericTypeParamDecl>(typeDecl));
  NominalTypeDecl *ownerNominal =
      ownerDC->getAsNominalTypeOrNominalTypeExtensionContext();

  // We might have an invalid extension that didn't resolve.
  if (needsBaseType && ownerNominal == nullptr)
    return std::make_tuple(nullptr, nullptr, false);

  // First, check for containment in one of our parent contexts.
  for (auto parentDC = fromDC; !parentDC->isModuleContext();
       parentDC = parentDC->getParent()) {
    auto parentNominal =
        parentDC->getAsNominalTypeOrNominalTypeExtensionContext();

    if (ownerDC == parentDC)
      return std::make_tuple(parentDC, parentNominal, true);

    if (isa<ExtensionDecl>(parentDC) && typeDecl == parentNominal) {
      assert(parentDC->getParent()->isModuleScopeContext());
      return std::make_tuple(parentDC, parentNominal, true);
    }

    // FIXME: Horrible hack. Don't allow us to reference a generic parameter
    // from a context outside a ProtocolDecl.
    if (isa<ProtocolDecl>(parentDC) && isa<GenericTypeParamDecl>(typeDecl))
      return std::make_tuple(nullptr, nullptr, false);
  }

  if (!needsBaseType) {
    assert(false && "Should have found non-type context by now");
    return std::make_tuple(nullptr, nullptr, false);
  }

  // Now, search the supertypes or refined protocols of each parent
  // context.
  for (auto parentDC = fromDC; !parentDC->isModuleContext();
       parentDC = parentDC->getParent()) {
    // For the next steps we need our parentDC to be a type context
    if (!parentDC->isTypeContext())
      continue;

    llvm::SmallPtrSet<NominalTypeDecl *, 8> visited;
    llvm::SmallVector<NominalTypeDecl *, 8> stack;

    // Start with the type of the current context.
    auto fromNominal = parentDC->getAsNominalTypeOrNominalTypeExtensionContext();
    if (!fromNominal)
      return std::make_tuple(nullptr, nullptr, false);

    // Break circularity.
    auto pushDecl = [&](NominalTypeDecl *nominal) -> void {
      if (visited.insert(nominal).second)
        stack.push_back(nominal);
    };
    pushDecl(fromNominal);

    // If we are in a protocol extension there might be other type aliases and
    // nominal types brought into the context through requirements on Self,
    // for example:
    //
    // extension MyProtocol where Self : YourProtocol { ... }
    if (parentDC->getAsProtocolExtensionContext()) {
      auto ED = cast<ExtensionDecl>(parentDC);
      if (auto genericSig = ED->getGenericSignature()) {
        for (auto req : genericSig->getRequirements()) {
          if (req.getKind() == RequirementKind::Conformance ||
              req.getKind() == RequirementKind::Superclass) {
            if (req.getFirstType()->isEqual(ED->getSelfInterfaceType()))
              if (auto *nominal = req.getSecondType()->getAnyNominal())
                pushDecl(nominal);
          }
        }
      }
    }

    while (!stack.empty()) {
      auto parentNominal = stack.back();

      stack.pop_back();

      // Check if we found the right context.
      if (parentNominal == ownerNominal)
        return std::make_tuple(parentDC, parentNominal, true);

      // If not, walk into the superclass and inherited protocols, if any.
      if (auto *protoDecl = dyn_cast<ProtocolDecl>(parentNominal)) {
        for (auto *refined : protoDecl->getInheritedProtocols(&TC))
          pushDecl(refined);
      } else {
        if (auto *classDecl = dyn_cast<ClassDecl>(parentNominal))
          if (auto superclassTy = classDecl->getSuperclass())
            if (auto superclassDecl = superclassTy->getClassOrBoundGenericClass())
              pushDecl(superclassDecl);
        if (!options.contains(TR_InheritanceClause)) {
          // FIXME: wrong nominal decl
          for (auto conforms : fromNominal->getAllProtocols()) {
            pushDecl(conforms);
          }
        }
      }
    }

    // FIXME: Horrible hack. Don't allow us to reference a generic parameter
    // or associated type from a context outside a ProtocolDecl.
    if (isa<ProtocolDecl>(parentDC) && isa<AbstractTypeParamDecl>(typeDecl))
      return std::make_tuple(nullptr, nullptr, false);
  }

  assert(false && "Should have found context by now");
  return std::make_tuple(nullptr, nullptr, false);
}

Type TypeChecker::resolveTypeInContext(
       TypeDecl *typeDecl,
       DeclContext *fromDC,
       TypeResolutionOptions options,
       bool isSpecialized,
       GenericTypeResolver *resolver) {
  GenericTypeToArchetypeResolver defaultResolver(fromDC);
  if (!resolver)
    resolver = &defaultResolver;

  // FIXME: foundDC and foundNominal should come from UnqualifiedLookup
  DeclContext *foundDC;
  NominalTypeDecl *foundNominal;
  bool valid;
  std::tie(foundDC, foundNominal, valid) =
      findDeclContextForType(*this, typeDecl, fromDC, options);

  if (!valid)
    return ErrorType::get(Context);

  assert(foundDC && "Should have found DeclContext by now");

  // If we are referring to a type within its own context, and we have either
  // a generic type with no generic arguments or a non-generic type, use the
  // type within the context.
  if (auto nominalType = dyn_cast<NominalTypeDecl>(typeDecl)) {
    if (!isa<ProtocolDecl>(nominalType) &&
        (!nominalType->getGenericParams() || !isSpecialized)) {
      forceExternalDeclMembers(nominalType);
      for (auto parentDC = fromDC;
           !parentDC->isModuleScopeContext();
           parentDC = parentDC->getParent()) {
        if (parentDC->getAsNominalTypeOrNominalTypeExtensionContext() == nominalType)
          return resolver->resolveTypeOfContext(parentDC);
      }
    }
  }

  bool hasDependentType = typeDecl->getDeclaredInterfaceType()
      ->hasTypeParameter();

  if (!foundNominal || !hasDependentType) {
    // If we found a generic parameter, map to the archetype if there is one.
    if (auto genericParam = dyn_cast<GenericTypeParamDecl>(typeDecl)) {
      return resolver->resolveGenericTypeParamType(
          genericParam->getDeclaredInterfaceType()
              ->castTo<GenericTypeParamType>());
    }

    // If this is a typealias not in type context, we still need the
    // interface type; the typealias might be in a function context, and
    // its underlying type might reference outer generic parameters.
    if (auto *aliasDecl = dyn_cast<TypeAliasDecl>(typeDecl)) {
      // For a generic typealias, return the unbound generic form of the type.
      if (aliasDecl->getGenericParams())
        return aliasDecl->getUnboundGenericType();

      return resolver->resolveTypeOfDecl(aliasDecl);
    }

    // When a nominal type used outside its context, return the unbound
    // generic form of the type.
    if (auto *nominalDecl = dyn_cast<NominalTypeDecl>(typeDecl))
      return nominalDecl->getDeclaredType();

    assert(!hasDependentType);
    return typeDecl->getDeclaredInterfaceType();
  }

  // Now let's get the base type.
  Type selfType;

  // If we started from a protocol but found a member of a concrete type,
  // we have a protocol extension with a superclass constraint on 'Self'.
  // Use the concrete type and not the protocol 'Self' type as the base
  // of the substitution.
  if (foundDC->getAsProtocolExtensionContext() &&
      !isa<ProtocolDecl>(foundNominal))
    selfType = foundNominal->getDeclaredType();
  // Otherwise, just use the type of the context we're looking at.
  else if (isa<NominalTypeDecl>(typeDecl))
    selfType = resolver->resolveTypeOfDecl(foundNominal);
  else
    selfType = resolver->resolveTypeOfContext(foundDC);

  if (!selfType || selfType->hasError())
    return ErrorType::get(Context);

  // If we started from a protocol and found an associated type member
  // of a (possibly inherited) protocol, resolve it via the resolver.
  if (auto *assocType = dyn_cast<AssociatedTypeDecl>(typeDecl)) {
    // Odd special case, ask Doug to explain it over pizza one day
    if (selfType->isTypeParameter())
      return resolver->resolveSelfAssociatedType(
          selfType, assocType);
  }

  // Finally, substitute the base type into the member type.
  return substMemberTypeWithBase(fromDC->getParentModule(), typeDecl,
                                 selfType);
}

/// This function checks if a bound generic type is UnsafePointer<Void> or
/// UnsafeMutablePointer<Void>. For these two type representations, we should
/// warn users that they are deprecated and replace them with more handy
/// UnsafeRawPointer and UnsafeMutableRawPointer, respectively.
static bool isPointerToVoid(ASTContext &Ctx, Type Ty, bool &IsMutable) {
  if (Ty.isNull())
    return false;
  auto *BGT = Ty->getAs<BoundGenericType>();
  if (!BGT)
    return false;
  if (BGT->getDecl() != Ctx.getUnsafePointerDecl() &&
      BGT->getDecl() != Ctx.getUnsafeMutablePointerDecl())
    return false;
  IsMutable = BGT->getDecl() == Ctx.getUnsafeMutablePointerDecl();
  assert(BGT->getGenericArgs().size() == 1);
  return BGT->getGenericArgs().front()->isVoid();
}

Type TypeChecker::applyGenericArguments(Type type, TypeDecl *decl,
                                        SourceLoc loc, DeclContext *dc,
                                        GenericIdentTypeRepr *generic,
                                        TypeResolutionOptions options,
                                        GenericTypeResolver *resolver,
                                 UnsatisfiedDependency *unsatisfiedDependency) {

  if (type->hasError()) {
    generic->setInvalid();
    return type;
  }

  // We must either have an unbound generic type, or a generic type alias.
  if (!type->is<UnboundGenericType>() &&
      !(isa<TypeAliasDecl>(decl) &&
        cast<TypeAliasDecl>(decl)->getGenericParams())) {

    auto diag = diagnose(loc, diag::not_a_generic_type, type);

    // Don't add fixit on module type; that isn't the right type regardless
    // of whether it had generic arguments.
    if (!type->is<ModuleType>())
      diag.fixItRemove(generic->getAngleBrackets());

    generic->setInvalid();
    return type;
  }

  // If we have a non-generic type alias, we have an unbound generic type.
  // Grab the decl from the unbound generic type.
  //
  // The idea is if you write:
  //
  // typealias Foo = Bar.Baz
  //
  // Then 'Foo<Int>' applies arguments to Bar.Baz, whereas if you write:
  //
  // typealias Foo<T> = Bar.Baz<T>
  //
  // Then 'Foo<Int>' applies arguments to Foo itself.
  //
  if (isa<TypeAliasDecl>(decl) &&
      !cast<TypeAliasDecl>(decl)->getGenericParams()) {
    decl = type->castTo<UnboundGenericType>()->getDecl();
  }

  // Make sure we have the right number of generic arguments.
  // FIXME: If we have fewer arguments than we need, that might be okay, if
  // we're allowed to deduce the remaining arguments from context.
  auto genericDecl = cast<GenericTypeDecl>(decl);
  auto genericArgs = generic->getGenericArgs();
  auto genericParams = genericDecl->getGenericParams();
  if (genericParams->size() != genericArgs.size()) {
    diagnose(loc, diag::type_parameter_count_mismatch, decl->getName(),
             genericParams->size(), genericArgs.size(),
             genericArgs.size() < genericParams->size())
        .highlight(generic->getAngleBrackets());
    diagnose(decl, diag::generic_type_declared_here,
             decl->getName());
    return ErrorType::get(Context);
  }

  // In SIL mode, Optional<T> interprets T as a SIL type.
  if (options.contains(TR_SILType)) {
    if (auto nominal = dyn_cast<NominalTypeDecl>(decl)) {
      if (nominal->classifyAsOptionalType()) {
        // Validate the generic argument.
        TypeLoc arg = genericArgs[0];
        if (validateType(arg, dc, withoutContext(options, true), resolver))
          return nullptr;

        Type objectType = arg.getType();
        if (!objectType)
          return nullptr;

        return BoundGenericType::get(nominal, /*parent*/ Type(), objectType);
      }
    }  
  }

  // FIXME: More principled handling of circularity.
  if (!genericDecl->hasValidSignature()) {
    diagnose(loc, diag::recursive_type_reference,
             genericDecl->getDescriptiveKind(), genericDecl->getName());
    diagnose(genericDecl, diag::type_declared_here);
    return ErrorType::get(Context);
  }

  SmallVector<TypeLoc, 8> args;
  for (auto tyR : genericArgs)
    args.push_back(tyR);

  auto result = applyUnboundGenericArguments(type, genericDecl, loc, dc, args,
                                             options, resolver,
                                             unsatisfiedDependency);
  if (!result)
    return result;
  bool isMutablePointer;
  if (isPointerToVoid(dc->getASTContext(), result, isMutablePointer)) {
    if (isMutablePointer)
      diagnose(loc, diag::use_of_void_pointer, "Mutable").
        fixItReplace(generic->getSourceRange(), "UnsafeMutableRawPointer");
    else
      diagnose(loc, diag::use_of_void_pointer, "").
        fixItReplace(generic->getSourceRange(), "UnsafeRawPointer");
  }
  return result;
}

/// Apply generic arguments to the given type.
Type TypeChecker::applyUnboundGenericArguments(
    Type type, GenericTypeDecl *decl, SourceLoc loc, DeclContext *dc,
    MutableArrayRef<TypeLoc> genericArgs,
    TypeResolutionOptions options,
    GenericTypeResolver *resolver,
    UnsatisfiedDependency *unsatisfiedDependency) {

  options -= TR_SILType;
  options -= TR_ImmediateFunctionInput;
  options -= TR_FunctionInput;
  options -= TR_AllowUnavailableProtocol;

  assert(genericArgs.size() == decl->getGenericParams()->size() &&
         "invalid arguments, use applyGenericArguments for diagnostic emitting");

  // Make sure we always have a resolver to use.
  GenericTypeToArchetypeResolver defaultResolver(dc);
  if (!resolver)
    resolver = &defaultResolver;

  // Validate the generic arguments and capture just the types.
  SmallVector<Type, 4> genericArgTypes;
  for (auto &genericArg : genericArgs) {
    // Validate the generic argument.
    if (validateType(genericArg, dc, options, resolver, unsatisfiedDependency))
      return ErrorType::get(Context);

    if (!genericArg.getType())
      return nullptr;

    genericArgTypes.push_back(genericArg.getType());
  }

  // If we're completing a generic TypeAlias, then we map the types provided
  // onto the underlying type.
  if (auto *TAD = dyn_cast<TypeAliasDecl>(decl)) {
    TypeSubstitutionMap subs;

    // The type should look like SomeNominal<T, U>.Alias<V, W>.

    // Get the substitutions for outer generic parameters from the parent
    // type.
    auto *unboundType = type->castTo<UnboundGenericType>();
    if (auto parentType = unboundType->getParent())
      subs = parentType->getContextSubstitutions(TAD->getDeclContext());

    // Get the substitutions for the inner parameters.
    auto signature = TAD->getGenericSignature();
    for (unsigned i = 0, e = genericArgs.size(); i < e; i++) {
      auto t = signature->getInnermostGenericParams()[i];
      subs[t->getCanonicalType()->castTo<GenericTypeParamType>()] =
        genericArgs[i].getType();
    }

    // Apply substitutions to the interface type of the typealias.
    type = TAD->getDeclaredInterfaceType();
    return type.subst(dc->getParentModule(), subs, SubstFlags::UseErrorType);
  }
  
  // Form the bound generic type.
  auto *UGT = type->castTo<UnboundGenericType>();
  auto *BGT = BoundGenericType::get(cast<NominalTypeDecl>(decl),
                                    UGT->getParent(), genericArgTypes);

  // Check protocol conformance.
  if (!BGT->hasTypeParameter() && !BGT->hasTypeVariable()) {
    SourceLoc noteLoc = decl->getLoc();
    if (noteLoc.isInvalid())
      noteLoc = loc;

    // FIXME: Record that we're checking substitutions, so we can't end up
    // with infinite recursion.

    // Check the generic arguments against the generic signature.
    auto genericSig = decl->getGenericSignature();

    // Collect the complete set of generic arguments.
    assert(genericSig != nullptr);
    auto substitutions = BGT->getContextSubstitutions(BGT->getDecl());

    auto result = checkGenericArguments(
        dc, loc, noteLoc, UGT, genericSig,
        substitutions, unsatisfiedDependency);

    // Unsatisfied dependency case.
    if (result.first)
      return Type();

    // Failure case.
    if (!result.second)
      return ErrorType::get(Context);

  }

  return BGT;
}

/// \brief Diagnose a use of an unbound generic type.
static void diagnoseUnboundGenericType(TypeChecker &tc, Type ty,SourceLoc loc) {
  auto unbound = ty->castTo<UnboundGenericType>();
  {
    InFlightDiagnostic diag = tc.diagnose(loc,
        diag::generic_type_requires_arguments, ty);
    if (auto *genericD = unbound->getDecl()) {
      SmallString<64> genericArgsToAdd;
      if (tc.getDefaultGenericArgumentsString(genericArgsToAdd, genericD))
        diag.fixItInsertAfter(loc, genericArgsToAdd);
    }
  }
  tc.diagnose(unbound->getDecl()->getLoc(), diag::generic_type_declared_here,
              unbound->getDecl()->getName());
}

/// \brief Returns a valid type or ErrorType in case of an error.
static Type resolveTypeDecl(TypeChecker &TC, TypeDecl *typeDecl, SourceLoc loc,
                            DeclContext *dc,
                            GenericIdentTypeRepr *generic,
                            TypeResolutionOptions options,
                            GenericTypeResolver *resolver,
                            UnsatisfiedDependency *unsatisfiedDependency) {
  assert(dc && "No declaration context for type resolution?");

  // If we have a callback to report dependencies, do so.
  if (unsatisfiedDependency) {
    if ((*unsatisfiedDependency)(requestResolveTypeDecl(typeDecl)))
      return nullptr;
  } else {
    // Validate the declaration.
    TC.validateDecl(typeDecl);
  }

  // If we didn't bail out with an unsatisfiedDependency,
  // and were not able to validate recursively, bail out.
  if (!typeDecl->hasInterfaceType()) {
    TC.diagnose(loc, diag::recursive_type_reference,
                typeDecl->getDescriptiveKind(), typeDecl->getName());
    TC.diagnose(typeDecl->getLoc(), diag::type_declared_here);
    return ErrorType::get(TC.Context);
  }

  // Resolve the type declaration to a specific type. How this occurs
  // depends on the current context and where the type was found.
  Type type =
      TC.resolveTypeInContext(typeDecl, dc, options, generic, resolver);

  if (type->is<UnboundGenericType>() && !generic &&
      !options.contains(TR_AllowUnboundGenerics) &&
      !options.contains(TR_ResolveStructure)) {
    diagnoseUnboundGenericType(TC, type, loc);
    return ErrorType::get(TC.Context);
  }

  if (generic && !options.contains(TR_ResolveStructure)) {
    // Apply the generic arguments to the type.
    type = TC.applyGenericArguments(type, typeDecl, loc, dc, generic,
                                    options, resolver, unsatisfiedDependency);
    if (!type)
      return nullptr;
  }

  assert(type);
  return type;
}

/// Diagnose a reference to an unknown type.
///
/// This routine diagnoses a reference to an unknown type, and
/// attempts to fix the reference via various means.
///
/// \param tc The type checker through which we should emit the diagnostic.
/// \param dc The context in which name lookup occurred.
///
/// \returns either the corrected type, if possible, or an error type to
/// that correction failed.
static Type diagnoseUnknownType(TypeChecker &tc, DeclContext *dc,
                                Type parentType,
                                SourceRange parentRange,
                                ComponentIdentTypeRepr *comp,
                                TypeResolutionOptions options,
                                NameLookupOptions lookupOptions,
                                GenericTypeResolver *resolver,
                                UnsatisfiedDependency *unsatisfiedDependency) {
  // Unqualified lookup case.
  if (parentType.isNull()) {
    // Attempt to refer to 'Self' within a non-protocol nominal
    // type. Fix this by replacing 'Self' with the nominal type name.
    DeclContext *nominalDC = nullptr;
    NominalTypeDecl *nominal = nullptr;
    if (comp->getIdentifier() == tc.Context.Id_Self &&
        !isa<GenericIdentTypeRepr>(comp) &&
        (nominalDC = dc->getInnermostTypeContext()) &&
        (nominal = nominalDC->getAsNominalTypeOrNominalTypeExtensionContext())) {
      // Retrieve the nominal type and resolve it within this context.
      assert(!isa<ProtocolDecl>(nominal) && "Cannot be a protocol");
      auto type = resolver->resolveTypeOfContext(dc->getInnermostTypeContext());
      if (type->hasError())
        return type;

      // Produce a Fix-It replacing 'Self' with the nominal type name.
      tc.diagnose(comp->getIdLoc(), diag::self_in_nominal, nominal->getName())
        .fixItReplace(comp->getIdLoc(), nominal->getName().str());
      comp->overwriteIdentifier(nominal->getName());
      comp->setValue(nominal);
      return type;
    }


    // Try ignoring access control.
    DeclContext *lookupDC = dc;
    if (options.contains(TR_GenericSignature))
      lookupDC = dc->getParent();

    NameLookupOptions relookupOptions = lookupOptions;
    relookupOptions |= NameLookupFlags::KnownPrivate;
    relookupOptions |= NameLookupFlags::IgnoreAccessibility;
    auto inaccessibleResults =
        tc.lookupUnqualifiedType(lookupDC, comp->getIdentifier(), comp->getIdLoc(),
                                 relookupOptions);
    if (!inaccessibleResults.empty()) {
      // FIXME: What if the unviable candidates have different levels of access?
      auto first = cast<TypeDecl>(inaccessibleResults.front());
      tc.diagnose(comp->getIdLoc(), diag::candidate_inaccessible,
                  comp->getIdentifier(), first->getFormalAccess());

      // FIXME: If any of the candidates (usually just one) are in the same
      // module we could offer a fix-it.
      for (auto lookupResult : inaccessibleResults)
        tc.diagnose(lookupResult, diag::type_declared_here);

      // Don't try to recover here; we'll get more access-related diagnostics
      // downstream if we do.
      return ErrorType::get(tc.Context);
    }

    // Fallback.
    SourceLoc L = comp->getIdLoc();
    SourceRange R = SourceRange(comp->getIdLoc());

    // Check if the unknown type is in the type remappings.
    auto &Remapped = tc.Context.RemappedTypes;
    auto TypeName = comp->getIdentifier().str();
    auto I = Remapped.find(TypeName);
    if (I != Remapped.end()) {
      auto RemappedTy = I->second->getString();
      tc.diagnose(L, diag::use_undeclared_type_did_you_mean,
                  comp->getIdentifier(), RemappedTy)
        .highlight(R)
        .fixItReplace(R, RemappedTy);

      // Replace the computed type with the suggested type.
      comp->overwriteIdentifier(tc.Context.getIdentifier(RemappedTy));

      return I->second;
    }

    tc.diagnose(L, diag::use_undeclared_type,
                comp->getIdentifier())
      .highlight(R);

    return ErrorType::get(tc.Context);
  }

  // Qualified lookup case.
  if (!parentType->mayHaveMembers()) {
    tc.diagnose(comp->getIdLoc(), diag::invalid_member_type,
                comp->getIdentifier(), parentType)
        .highlight(parentRange);
    return ErrorType::get(tc.Context);
  }

  // Try ignoring access control.
  NameLookupOptions relookupOptions = lookupOptions;
  relookupOptions |= NameLookupFlags::KnownPrivate;
  relookupOptions |= NameLookupFlags::IgnoreAccessibility;
  auto inaccessibleMembers = tc.lookupMemberType(dc, parentType,
                                                 comp->getIdentifier(),
                                                 relookupOptions);
  if (inaccessibleMembers) {
    // FIXME: What if the unviable candidates have different levels of access?
    const TypeDecl *first = inaccessibleMembers.front().first;
    tc.diagnose(comp->getIdLoc(), diag::candidate_inaccessible,
                comp->getIdentifier(), first->getFormalAccess());

    // FIXME: If any of the candidates (usually just one) are in the same module
    // we could offer a fix-it.
    for (auto lookupResult : inaccessibleMembers)
      tc.diagnose(lookupResult.first, diag::type_declared_here);

    // Don't try to recover here; we'll get more access-related diagnostics
    // downstream if we do.
    return ErrorType::get(tc.Context);
  }

  // FIXME: Typo correction!

  // Lookup into a type.
  if (auto moduleType = parentType->getAs<ModuleType>()) {
    tc.diagnose(comp->getIdLoc(), diag::no_module_type,
                comp->getIdentifier(), moduleType->getModule()->getName());
  } else {
    // Situation where class tries to inherit from itself, such
    // would produce an assertion when trying to lookup members of the class.
    auto lazyResolver = tc.Context.getLazyResolver();
    if (auto superClass = parentType->getSuperclass(lazyResolver)) {
      if (superClass->isEqual(parentType)) {
        auto decl = parentType->getAnyNominal();
        if (decl) {
          tc.diagnose(decl->getLoc(), diag::circular_class_inheritance,
                      decl->getNameStr());
          return ErrorType::get(tc.Context);
        }
      }
    }

    LookupResult memberLookup;
    // Let's try to lookup given identifier as a member of the parent type,
    // this allows for more precise diagnostic, which distinguishes between
    // identifier not found as a member type vs. not found at all.
    NameLookupOptions memberLookupOptions = lookupOptions;
    memberLookupOptions |= NameLookupFlags::IgnoreAccessibility;
    memberLookupOptions |= NameLookupFlags::KnownPrivate;

    memberLookup = tc.lookupMember(dc, parentType, comp->getIdentifier(),
                                   memberLookupOptions);

    // Looks like this is not a member type, but simply a member of parent type.
    if (!memberLookup.empty()) {
      auto &member = memberLookup[0];
      tc.diagnose(comp->getIdLoc(), diag::invalid_member_reference,
                  member->getDescriptiveKind(), comp->getIdentifier(),
                  parentType)
          .highlight(parentRange);
    } else {
      tc.diagnose(comp->getIdLoc(), diag::invalid_member_type,
                  comp->getIdentifier(), parentType)
          .highlight(parentRange);
    }
  }
  return ErrorType::get(tc.Context);
}

/// Resolve the given identifier type representation as an unqualified type,
/// returning the type it references.
///
/// \returns Either the resolved type or a null type, the latter of
/// which indicates that some dependencies were unsatisfied.
static Type
resolveTopLevelIdentTypeComponent(TypeChecker &TC, DeclContext *DC,
                                  ComponentIdentTypeRepr *comp,
                                  TypeResolutionOptions options,
                                  bool diagnoseErrors,
                                  GenericTypeResolver *resolver,
                                  UnsatisfiedDependency *unsatisfiedDependency){
  // Short-circuiting.
  if (comp->isInvalid()) return ErrorType::get(TC.Context);

  // If the component has already been bound to a declaration, handle
  // that now.
  if (ValueDecl *VD = comp->getBoundDecl()) {
    auto *typeDecl = cast<TypeDecl>(VD);

    // Resolve the type declaration within this context.
    return resolveTypeDecl(TC, typeDecl, comp->getIdLoc(), DC,
                           dyn_cast<GenericIdentTypeRepr>(comp), options,
                           resolver, unsatisfiedDependency);
  }

  // Resolve the first component, which is the only one that requires
  // unqualified name lookup.
  DeclContext *lookupDC = DC;

  // Dynamic 'Self' in the result type of a function body.
  if (options.contains(TR_DynamicSelfResult) &&
      comp->getIdentifier() == TC.Context.Id_Self) {
    auto func = cast<FuncDecl>(DC);
    assert(func->hasDynamicSelf() && "Not marked as having dynamic Self?");

    // FIXME: The passed-in TypeRepr should get 'typechecked' as well.
    // The issue is though that ComponentIdentTypeRepr only accepts a ValueDecl
    // while the 'Self' type is more than just a reference to a TypeDecl.

    auto selfType = resolver->resolveTypeOfContext(func->getDeclContext());
    return DynamicSelfType::get(selfType, TC.Context);
  }

  // For lookups within the generic signature, look at the generic
  // parameters (only), then move up to the enclosing context.
  if (options.contains(TR_GenericSignature)) {
    GenericParamList *genericParams;
    if (auto *generic = dyn_cast<GenericTypeDecl>(DC)) {
      genericParams = generic->getGenericParams();
    } else if (auto *ext = dyn_cast<ExtensionDecl>(DC)) {
      genericParams = ext->getGenericParams();
    } else {
      genericParams = cast<AbstractFunctionDecl>(DC)->getGenericParams();
    }

    if (genericParams) {
      auto matchingParam =
          std::find_if(genericParams->begin(), genericParams->end(),
                       [comp](const GenericTypeParamDecl *param) {
        return param->getFullName().matchesRef(comp->getIdentifier());
      });

      if (matchingParam != genericParams->end()) {
        comp->setValue(*matchingParam);
        return resolveTopLevelIdentTypeComponent(TC, DC, comp, options,
                                                 diagnoseErrors, resolver,
                                                 unsatisfiedDependency);
      }
    }

    // If the lookup occurs from within a trailing 'where' clause of
    // a constrained extension, also look for associated types.
    if (genericParams && genericParams->hasTrailingWhereClause() &&
        isa<ExtensionDecl>(DC) && comp->getIdLoc().isValid() &&
        TC.Context.SourceMgr.rangeContainsTokenLoc(
          genericParams->getTrailingWhereClauseSourceRange(),
          comp->getIdLoc())) {
      // We need to be able to perform qualified lookup into the given
      // declaration context.
      if (unsatisfiedDependency &&
          (*unsatisfiedDependency)(
            requestQualifiedLookupInDeclContext({ DC, comp->getIdentifier(),
                                                  comp->getIdLoc() })))
        return nullptr;

      auto nominal = DC->getAsNominalTypeOrNominalTypeExtensionContext();
      SmallVector<ValueDecl *, 4> decls;
      if (DC->lookupQualified(nominal->getDeclaredInterfaceType(),
                              comp->getIdentifier(),
                              NL_QualifiedDefault|NL_ProtocolMembers,
                              &TC,
                              decls)) {
        for (const auto decl : decls) {
          // FIXME: Better ambiguity handling.
          if (auto assocType = dyn_cast<AssociatedTypeDecl>(decl)) {
            comp->setValue(assocType);
            return resolveTopLevelIdentTypeComponent(TC, DC, comp, options,
                                                     diagnoseErrors, resolver,
                                                     unsatisfiedDependency);
          }
        }
      }
    }

    if (!DC->isCascadingContextForLookup(/*excludeFunctions*/false))
      options |= TR_KnownNonCascadingDependency;

    // The remaining lookups will be in the parent context.
    lookupDC = DC->getParent();
  }

  // We need to be able to perform unqualified lookup into the given
  // declaration context.
  if (unsatisfiedDependency &&
      (*unsatisfiedDependency)(
        requestUnqualifiedLookupInDeclContext({ lookupDC, 
                                                comp->getIdentifier(),
                                                comp->getIdLoc() })))
    return nullptr;

  NameLookupOptions lookupOptions = defaultUnqualifiedLookupOptions;
  if (options.contains(TR_KnownNonCascadingDependency))
    lookupOptions |= NameLookupFlags::KnownPrivate;
  auto globals = TC.lookupUnqualifiedType(lookupDC,
                                          comp->getIdentifier(),
                                          comp->getIdLoc(),
                                          lookupOptions);

  // Process the names we found.
  Type current;
  TypeDecl *currentDecl = nullptr;
  bool isAmbiguous = false;
  for (const auto &typeDecl : globals) {

    // If necessary, add delayed members to the declaration.
    if (auto nomDecl = dyn_cast<NominalTypeDecl>(typeDecl)) {
      TC.forceExternalDeclMembers(nomDecl);
    }

    Type type = resolveTypeDecl(TC, typeDecl, comp->getIdLoc(), DC,
                                dyn_cast<GenericIdentTypeRepr>(comp), options,
                                resolver, unsatisfiedDependency);

    if (!type || type->hasError())
      return type;

    // If this is the first result we found, record it.
    if (current.isNull()) {
      current = type;
      currentDecl = typeDecl;
      continue;
    }

    // Otherwise, check for an ambiguity.
    if (!current->isEqual(type)) {
      isAmbiguous = true;
      break;
    }

    // We have a found multiple type aliases that refer to the same thing.
    // Ignore the duplicate.
  }

  // Complain about any ambiguities we detected.
  // FIXME: We could recover by looking at later components.
  if (isAmbiguous) {
    if (diagnoseErrors) {
      TC.diagnose(comp->getIdLoc(), diag::ambiguous_type_base,
                  comp->getIdentifier())
        .highlight(comp->getIdLoc());
      for (auto typeDecl : globals) {
        TC.diagnose(typeDecl, diag::found_candidate);
      }
    }

    comp->setInvalid();
    return ErrorType::get(TC.Context);
  }

  // If we found nothing, complain and give ourselves a chance to recover.
  if (current.isNull()) {
    // If we're not allowed to complain or we couldn't fix the
    // source, bail out.
    if (!diagnoseErrors)
      return ErrorType::get(TC.Context);

    return diagnoseUnknownType(TC, DC, nullptr, SourceRange(), comp, options,
                               lookupOptions, resolver, unsatisfiedDependency);
  }

  comp->setValue(currentDecl);
  return current;
}

/// Resolve the given identifier type representation as a qualified
/// lookup within the given parent type, returning the type it
/// references.
static Type resolveNestedIdentTypeComponent(
              TypeChecker &TC, DeclContext *DC,
              Type parentTy,
              SourceRange parentRange,
              ComponentIdentTypeRepr *comp,
              TypeResolutionOptions options,
              bool diagnoseErrors,
              GenericTypeResolver *resolver,
              UnsatisfiedDependency *unsatisfiedDependency) {
  // Local function to produce a diagnostic if the type we referenced was an
  // associated type but the type itself was erroneous. We'll produce a
  // diagnostic here if the diagnostic for the bad type witness would show up in
  // a different context.
  auto maybeDiagnoseBadConformanceRef = [&](AssociatedTypeDecl *assocType,
                                            ProtocolConformance *conformance) {
    // If we aren't emitting any diagnostics, we're done.
    if (!diagnoseErrors)
      return;

    // If we weren't given a conformance, go look it up.
    if (!conformance) {
      if (auto conformanceRef =
            TC.conformsToProtocol(
              parentTy, assocType->getProtocol(), DC,
              (ConformanceCheckFlags::InExpression|
               ConformanceCheckFlags::SuppressDependencyTracking))) {
        if (conformanceRef->isConcrete())
          conformance = conformanceRef->getConcrete();
      }
    }

    // If there is a conformance and it comes from the same source file as type
    // resolution, don't diagnose.
    if (conformance &&
        conformance->getDeclContext()->getParentSourceFile() ==
          DC->getParentSourceFile())
      return;

    // If any errors have occurred, don't bother diagnosing this cross-file
    // issue.
    if (TC.Context.Diags.hadAnyError())
      return;

    TC.diagnose(comp->getLoc(), diag::broken_associated_type_witness,
                assocType->getFullName(), parentTy);
  };

  // Short-circuiting.
  if (comp->isInvalid()) return ErrorType::get(TC.Context);

  // If the parent is a type parameter, the member is a dependent member,
  // and we skip much of the work below.
  if (parentTy->isTypeParameter()) {
    auto memberType = resolver->resolveDependentMemberType(parentTy, DC,
                                                      parentRange, comp);
    assert(memberType && "Received null dependent member type");
    return memberType;
  }

  // Phase 2: If a declaration has already been bound, use it.
  if (ValueDecl *decl = comp->getBoundDecl()) {
    auto *typeDecl = cast<TypeDecl>(decl);

    // Otherwise, simply substitute the parent type into the member.
    auto memberType = TC.substMemberTypeWithBase(DC->getParentModule(),
                                                 typeDecl, parentTy);

    // Diagnose the bad reference if we need to.
    if (typeDecl && isa<AssociatedTypeDecl>(typeDecl) && memberType->hasError())
      maybeDiagnoseBadConformanceRef(cast<AssociatedTypeDecl>(typeDecl), nullptr);

    // Propagate failure.
    if (!memberType || memberType->hasError()) return memberType;

    // If there are generic arguments, apply them now.
    if (auto genComp = dyn_cast<GenericIdentTypeRepr>(comp)) {
      memberType = TC.applyGenericArguments(
          memberType, typeDecl, comp->getIdLoc(), DC, genComp,
          options, resolver, unsatisfiedDependency);

      // Propagate failure.
      if (!memberType || memberType->hasError()) return memberType;
    }

    // We're done.
    return memberType;
  }

  // Phase 1: Find and bind the component decl.

  // Look for member types with the given name.
  bool isKnownNonCascading = options.contains(TR_KnownNonCascadingDependency);
  if (!isKnownNonCascading && options.contains(TR_InExpression)) {
    // Expressions cannot affect a function's signature.
    isKnownNonCascading = isa<AbstractFunctionDecl>(DC);
  }

  // We need to be able to perform qualified lookup into the given type.
  if (unsatisfiedDependency) {
    DeclContext *dc;
    if (auto parentNominal = parentTy->getAnyNominal())
      dc = parentNominal;
    else if (auto parentModule = parentTy->getAs<ModuleType>())
      dc = parentModule->getModule();
    else
      dc = nullptr;

    if (dc &&
        (*unsatisfiedDependency)(
          requestQualifiedLookupInDeclContext({ dc, comp->getIdentifier(),
                                                comp->getIdLoc() })))
      return nullptr;
  }

  NameLookupOptions lookupOptions = defaultMemberLookupOptions;
  if (isKnownNonCascading)
    lookupOptions |= NameLookupFlags::KnownPrivate;
  // FIXME: Lift the restriction for TR_InheritanceClause
  if (options.contains(TR_ExtensionBinding) ||
      options.contains(TR_InheritanceClause))
    lookupOptions -= NameLookupFlags::ProtocolMembers;
  LookupTypeResult memberTypes;
  if (parentTy->mayHaveMembers())
    memberTypes = TC.lookupMemberType(DC, parentTy, comp->getIdentifier(),
                                      lookupOptions);

  // Name lookup was ambiguous. Complain.
  // FIXME: Could try to apply generic arguments first, and see whether
  // that resolves things. But do we really want that to succeed?
  if (memberTypes.size() > 1) {
    if (diagnoseErrors)
      TC.diagnoseAmbiguousMemberType(parentTy, parentRange,
                                     comp->getIdentifier(), comp->getIdLoc(),
                                     memberTypes);
    return ErrorType::get(TC.Context);
  }

  // If we didn't find anything, complain.
  Type memberType;
  TypeDecl *member = nullptr;
  if (!memberTypes) {
    // If we're not allowed to complain or we couldn't fix the
    // source, bail out.
    if (!diagnoseErrors) {
      return ErrorType::get(TC.Context);
    }

    Type ty = diagnoseUnknownType(TC, DC, parentTy, parentRange, comp, options,
                                  lookupOptions, resolver,
                                  unsatisfiedDependency);
    if (!ty || ty->hasError()) {
      return ErrorType::get(TC.Context);
    }

    memberType = ty;
    member = cast_or_null<TypeDecl>(comp->getBoundDecl());
  } else {
    memberType = memberTypes.back().second;
    member = memberTypes.back().first;
  }

  if (parentTy->isExistentialType() && isa<AssociatedTypeDecl>(member)) {
    if (diagnoseErrors)
      TC.diagnose(comp->getIdLoc(), diag::assoc_type_outside_of_protocol,
                  comp->getIdentifier());

    return ErrorType::get(TC.Context);
  }

  if (parentTy->isExistentialType() && isa<TypeAliasDecl>(member) &&
      memberType->hasTypeParameter()) {
    if (diagnoseErrors)
      TC.diagnose(comp->getIdLoc(), diag::typealias_outside_of_protocol,
                  comp->getIdentifier());

    return ErrorType::get(TC.Context);
  }

  // If there are generic arguments, apply them now.
  if (auto genComp = dyn_cast<GenericIdentTypeRepr>(comp))
    memberType = TC.applyGenericArguments(
        memberType, member, comp->getIdLoc(), DC, genComp,
        options, resolver, unsatisfiedDependency);

  // If we found a reference to an associated type or other member type that
  // was marked invalid, just return ErrorType to silence downstream errors.
  if (member && member->isInvalid())
    memberType = ErrorType::get(TC.Context);

  // Diagnose the bad reference if we need to.
  if (member && isa<AssociatedTypeDecl>(member) && memberType->hasError())
    maybeDiagnoseBadConformanceRef(cast<AssociatedTypeDecl>(member), nullptr);

  if (member)
    comp->setValue(member);
  return memberType;
}

static Type resolveIdentTypeComponent(
              TypeChecker &TC, DeclContext *DC,
              ArrayRef<ComponentIdentTypeRepr *> components,
              TypeResolutionOptions options,
              bool diagnoseErrors,
              GenericTypeResolver *resolver,
              UnsatisfiedDependency *unsatisfiedDependency) {
  auto comp = components.back();

  // The first component uses unqualified lookup.
  auto parentComps = components.slice(0, components.size()-1);
  if (parentComps.empty()) {
    return resolveTopLevelIdentTypeComponent(TC, DC, comp, options,
                                             diagnoseErrors, resolver,
                                             unsatisfiedDependency);
  }

  // All remaining components use qualified lookup.

  // Resolve the parent type.
  Type parentTy = resolveIdentTypeComponent(TC, DC, parentComps, options,
                                            diagnoseErrors, resolver,
                                            unsatisfiedDependency);
  if (!parentTy || parentTy->hasError()) return parentTy;
  
  SourceRange parentRange(parentComps.front()->getIdLoc(),
                          parentComps.back()->getSourceRange().End);

  // Resolve the nested type.
  return resolveNestedIdentTypeComponent(TC, DC, parentTy,
                                         parentRange, comp,
                                         options, diagnoseErrors,
                                         resolver,
                                         unsatisfiedDependency);
}

static bool diagnoseAvailability(IdentTypeRepr *IdType,
                                 DeclContext *DC, TypeChecker &TC,
                                 bool AllowPotentiallyUnavailableProtocol) {
  auto componentRange = IdType->getComponentRange();
  for (auto comp : componentRange) {
    if (auto typeDecl = dyn_cast_or_null<TypeDecl>(comp->getBoundDecl())) {
      // In Swift 3, components other than the last one were not properly
      // checked for availability.
      // FIXME: We should try to downgrade these errors to warnings, not just
      // skip diagnosing them.
      if (TC.getLangOpts().isSwiftVersion3() && comp != componentRange.back())
        continue;

      if (diagnoseDeclAvailability(typeDecl, TC, DC, comp->getIdLoc(),
                                   AllowPotentiallyUnavailableProtocol,
                                   /*SignalOnPotentialUnavailability*/false)) {
        return true;
      }
    }
  }

  return false;
}

// Hack to apply context-specific @escaping to an AST function type.
static Type applyNonEscapingFromContext(DeclContext *DC,
                                        Type ty,
                                        TypeResolutionOptions options) {
  // Remember whether this is a function parameter.
  bool isFunctionParam =
    options.contains(TR_FunctionInput) ||
    options.contains(TR_ImmediateFunctionInput);

  bool defaultNoEscape = isFunctionParam;

  // Desugar here
  auto *funcTy = ty->castTo<FunctionType>();
  auto extInfo = funcTy->getExtInfo();
  if (defaultNoEscape && !extInfo.isNoEscape()) {
    extInfo = extInfo.withNoEscape();

    // We lost the sugar to flip the isNoEscape bit.
    //
    // FIXME: It would be better to add a new AttributedType sugared type,
    // which would wrap the NameAliasType or ParenType, and apply the
    // isNoEscape bit when de-sugaring.
    // <https://bugs.swift.org/browse/SR-2520>
    return FunctionType::get(funcTy->getInput(), funcTy->getResult(), extInfo);
  }

  // Note: original sugared type
  return ty;
}

/// \brief Returns a valid type or ErrorType in case of an error.
Type TypeChecker::resolveIdentifierType(
       DeclContext *DC,
       IdentTypeRepr *IdType,
       TypeResolutionOptions options,
       bool diagnoseErrors,
       GenericTypeResolver *resolver,
       UnsatisfiedDependency *unsatisfiedDependency) {
  assert(resolver && "Missing generic type resolver");

  auto ComponentRange = IdType->getComponentRange();
  auto Components = llvm::makeArrayRef(ComponentRange.begin(),
                                       ComponentRange.end());
  Type result = resolveIdentTypeComponent(*this, DC, Components, options, 
                                          diagnoseErrors, resolver,
                                          unsatisfiedDependency);
  if (!result) return nullptr;

  if (auto moduleTy = result->getAs<ModuleType>()) {
    if (diagnoseErrors) {
      auto moduleName = moduleTy->getModule()->getName();
      diagnose(Components.back()->getIdLoc(),
               diag::use_undeclared_type, moduleName);
      diagnose(Components.back()->getIdLoc(),
               diag::note_module_as_type, moduleName);
    }
    Components.back()->setInvalid();
    return ErrorType::get(Context);
  }

  // Hack to apply context-specific @escaping to a typealias with an underlying
  // function type.
  if (result->is<FunctionType>())
    result = applyNonEscapingFromContext(DC, result, options);

  // Check the availability of the type.

  // We allow a type to conform to a protocol that is less available than
  // the type itself. This enables a type to retroactively model or directly
  // conform to a protocol only available on newer OSes and yet still be used on
  // older OSes.
  // To support this, inside inheritance clauses we allow references to
  // protocols that are unavailable in the current type refinement context.

  if (!(options & TR_AllowUnavailable) &&
      diagnoseAvailability(IdType, DC, *this,
                           options.contains(TR_AllowUnavailableProtocol))) {
    Components.back()->setInvalid();
    return ErrorType::get(Context);
  }
  
  return result;
}

/// Returns true if any illegal IUOs were found. If inference of IUO type is
/// disabled, IUOs may only be specified in the following positions:
///  * outermost type
///  * function param
///  * function return type
static bool checkForIllegalIUOs(TypeChecker &TC, TypeRepr *Repr,
                                TypeResolutionOptions Options) {
  class IllegalIUOWalker : public ASTWalker {
    TypeChecker &TC;
    SmallVector<bool, 4> IUOsAllowed;
    bool FoundIllegalIUO = false;

  public:
    IllegalIUOWalker(TypeChecker &TC, bool IsGenericParameter)
      : TC(TC)
      , IUOsAllowed{!IsGenericParameter} {}

    bool walkToTypeReprPre(TypeRepr *T) override {
      bool iuoAllowedHere = IUOsAllowed.back();

      // Raise a diagnostic if we run into a prohibited IUO.
      if (!iuoAllowedHere) {
        if (auto *iuoTypeRepr =
            dyn_cast<ImplicitlyUnwrappedOptionalTypeRepr>(T)) {
          TC.diagnose(iuoTypeRepr->getStartLoc(), diag::iuo_in_illegal_position)
            .fixItReplace(iuoTypeRepr->getExclamationLoc(), "?");
          FoundIllegalIUO = true;
        }
      }

      bool childIUOsAllowed = false;
      if (iuoAllowedHere) {
        if (auto *tupleTypeRepr = dyn_cast<TupleTypeRepr>(T)) {
          if (tupleTypeRepr->isParenType()) {
            childIUOsAllowed = true;
          }
        } else if (isa<FunctionTypeRepr>(T)) {
          childIUOsAllowed = true;
        } else if (isa<AttributedTypeRepr>(T) || isa<InOutTypeRepr>(T)) {
          childIUOsAllowed = true;
        }
      }
      IUOsAllowed.push_back(childIUOsAllowed);
      return true;
    }

    bool walkToTypeReprPost(TypeRepr *T) override {
      IUOsAllowed.pop_back();
      return true;
    }

    bool getFoundIllegalIUO() const { return FoundIllegalIUO; }
  };

  IllegalIUOWalker Walker(TC, Options.contains(TR_GenericSignature));
  Repr->walk(Walker);
  return Walker.getFoundIllegalIUO();
}

bool TypeChecker::validateType(TypeLoc &Loc, DeclContext *DC,
                               TypeResolutionOptions options,
                               GenericTypeResolver *resolver,
                               UnsatisfiedDependency *unsatisfiedDependency) {
  // FIXME: Verify that these aren't circular and infinite size.
  
  // If we've already validated this type, don't do so again.
  if (Loc.wasValidated())
    return Loc.isError();

  SWIFT_FUNC_STAT;

  if (Loc.getType().isNull()) {
    // Raise error if we parse an IUO type in an illegal position.
    checkForIllegalIUOs(*this, Loc.getTypeRepr(), options);

    // Special case: in computed property setter, newValue closure is escaping
    if (isa<FuncDecl>(DC) && cast<FuncDecl>(DC)->isSetter())
      options |= TR_ImmediateSetterNewValue;

    auto type = resolveType(Loc.getTypeRepr(), DC, options, resolver,
                            unsatisfiedDependency);
    if (!type) {
      // If a dependency went unsatisfied, just return false.
      if (unsatisfiedDependency) return false;

      type = ErrorType::get(Context);

      // Diagnose types that are illegal in SIL.
    } else if (options.contains(TR_SILType) && !type->isLegalSILType()) {
      diagnose(Loc.getLoc(), diag::illegal_sil_type, type);
      Loc.setType(ErrorType::get(Context), true);
      return true;
    }

    // Special case: in computed property setter, newValue closure is escaping
    if (auto funcDecl = dyn_cast<FuncDecl>(DC))
      if (funcDecl->isSetter())
        if (auto funTy = type->getAs<AnyFunctionType>())
          type = funTy->withExtInfo(funTy->getExtInfo().withNoEscape(false));

    Loc.setType(type, true);
    return Loc.isError();
  }

  Loc.setType(Loc.getType(), true);
  return Loc.isError();
}

namespace {
  const auto DefaultParameterConvention = ParameterConvention::Direct_Unowned;
  const auto DefaultResultConvention = ResultConvention::Unowned;

  class TypeResolver {
    TypeChecker &TC;
    ASTContext &Context;
    DeclContext *DC;
    GenericTypeResolver *Resolver;
    swift::UnsatisfiedDependency *UnsatisfiedDependency;
  public:
    TypeResolver(TypeChecker &tc, DeclContext *DC,
                 GenericTypeResolver *resolver,
                 swift::UnsatisfiedDependency *unsatisfiedDependency)
      : TC(tc), Context(tc.Context), DC(DC), Resolver(resolver),
        UnsatisfiedDependency(unsatisfiedDependency)
    {
      assert(resolver);
    }

    Type resolveType(TypeRepr *repr, TypeResolutionOptions options);

  private:

    Type resolveAttributedType(AttributedTypeRepr *repr,
                               TypeResolutionOptions options);
    Type resolveAttributedType(TypeAttributes &attrs, TypeRepr *repr,
                               TypeResolutionOptions options);
    Type resolveASTFunctionType(FunctionTypeRepr *repr,
                                TypeResolutionOptions options,
                                FunctionType::ExtInfo extInfo
                                  = FunctionType::ExtInfo());
    Type resolveSILFunctionType(FunctionTypeRepr *repr,
                                TypeResolutionOptions options,
                                SILFunctionType::ExtInfo extInfo
                                  = SILFunctionType::ExtInfo(),
                                ParameterConvention calleeConvention
                                  = DefaultParameterConvention);
    SILParameterInfo resolveSILParameter(TypeRepr *repr,
                                         TypeResolutionOptions options);
    bool resolveSILResults(TypeRepr *repr, TypeResolutionOptions options,
                           SmallVectorImpl<SILResultInfo> &results,
                           Optional<SILResultInfo> &errorResult);
    bool resolveSingleSILResult(TypeRepr *repr, TypeResolutionOptions options,
                                SmallVectorImpl<SILResultInfo> &results,
                                Optional<SILResultInfo> &errorResult);
    Type resolveInOutType(InOutTypeRepr *repr,
                          TypeResolutionOptions options);
    Type resolveArrayType(ArrayTypeRepr *repr,
                          TypeResolutionOptions options);
    Type resolveDictionaryType(DictionaryTypeRepr *repr,
                               TypeResolutionOptions options);
    Type resolveOptionalType(OptionalTypeRepr *repr,
                             TypeResolutionOptions options);
    Type resolveImplicitlyUnwrappedOptionalType(ImplicitlyUnwrappedOptionalTypeRepr *repr,
                                      TypeResolutionOptions options);
    Type resolveTupleType(TupleTypeRepr *repr,
                          TypeResolutionOptions options);
    Type resolveCompositionType(CompositionTypeRepr *repr,
                                TypeResolutionOptions options);
    Type resolveMetatypeType(MetatypeTypeRepr *repr,
                             TypeResolutionOptions options);
    Type resolveProtocolType(ProtocolTypeRepr *repr,
                             TypeResolutionOptions options);
    Type resolveSILBoxType(SILBoxTypeRepr *repr,
                           TypeResolutionOptions options);

    Type buildMetatypeType(MetatypeTypeRepr *repr,
                           Type instanceType,
                           Optional<MetatypeRepresentation> storedRepr);
    Type buildProtocolType(ProtocolTypeRepr *repr,
                           Type instanceType,
                           Optional<MetatypeRepresentation> storedRepr);
  };
} // end anonymous namespace

Type TypeChecker::resolveType(TypeRepr *TyR, DeclContext *DC,
                              TypeResolutionOptions options,
                              GenericTypeResolver *resolver,
                              UnsatisfiedDependency *unsatisfiedDependency) {
  PrettyStackTraceTypeRepr stackTrace(Context, "resolving", TyR);

  // Make sure we always have a resolver to use.
  GenericTypeToArchetypeResolver defaultResolver(DC);
  if (!resolver)
    resolver = &defaultResolver;

  TypeResolver typeResolver(*this, DC, resolver, unsatisfiedDependency);
  auto result = typeResolver.resolveType(TyR, options);
  
  // If we resolved down to an error, make sure to mark the typeRepr as invalid
  // so we don't produce a redundant diagnostic.
  if (result && result->hasError())
    TyR->setInvalid();
  return result;
}

Type TypeResolver::resolveType(TypeRepr *repr, TypeResolutionOptions options) {
  assert(repr && "Cannot validate null TypeReprs!");

  // If we know the type representation is invalid, just return an
  // error type.
  if (repr->isInvalid()) return ErrorType::get(TC.Context);

  // Strip the "is function input" bits unless this is a type that knows about
  // them.
  if (!isa<InOutTypeRepr>(repr) &&
      !isa<TupleTypeRepr>(repr) &&
      !isa<AttributedTypeRepr>(repr) &&
      !isa<FunctionTypeRepr>(repr) &&
      !isa<IdentTypeRepr>(repr)) {
    options -= TR_ImmediateFunctionInput;
    options -= TR_FunctionInput;
  }

  bool isImmediateSetterNewValue = options.contains(TR_ImmediateSetterNewValue);
  options -= TR_ImmediateSetterNewValue;

  if (Context.LangOpts.DisableAvailabilityChecking)
    options |= TR_AllowUnavailable;

  switch (repr->getKind()) {
  case TypeReprKind::Error:
    return ErrorType::get(Context);

  case TypeReprKind::Attributed:
    return resolveAttributedType(cast<AttributedTypeRepr>(repr), options);
  case TypeReprKind::InOut:
    return resolveInOutType(cast<InOutTypeRepr>(repr), options);

  case TypeReprKind::SimpleIdent:
  case TypeReprKind::GenericIdent:
  case TypeReprKind::CompoundIdent:
    return TC.resolveIdentifierType(DC, cast<IdentTypeRepr>(repr), options,
                                    /*diagnoseErrors*/ true, Resolver,
                                    UnsatisfiedDependency);

  case TypeReprKind::Function:
    if (!(options & TR_SILType)) {
      // Default non-escaping for closure parameters
      auto result =
          resolveASTFunctionType(cast<FunctionTypeRepr>(repr), options);
      if (result && result->is<FunctionType>() && !isImmediateSetterNewValue)
        return applyNonEscapingFromContext(DC, result, options);
      return result;
    }
    return resolveSILFunctionType(cast<FunctionTypeRepr>(repr), options);

  case TypeReprKind::SILBox:
    assert((options & TR_SILType) && "SILBox repr in non-SIL type context?!");
    return resolveSILBoxType(cast<SILBoxTypeRepr>(repr), options);

  case TypeReprKind::Array:
    return resolveArrayType(cast<ArrayTypeRepr>(repr), options);

  case TypeReprKind::Dictionary:
    return resolveDictionaryType(cast<DictionaryTypeRepr>(repr), options);

  case TypeReprKind::Optional:
    return resolveOptionalType(cast<OptionalTypeRepr>(repr), options);

  case TypeReprKind::ImplicitlyUnwrappedOptional:
    return resolveImplicitlyUnwrappedOptionalType(
             cast<ImplicitlyUnwrappedOptionalTypeRepr>(repr),
             options);

  case TypeReprKind::Tuple:
    return resolveTupleType(cast<TupleTypeRepr>(repr), options);

  case TypeReprKind::Composition:
    return resolveCompositionType(cast<CompositionTypeRepr>(repr), options);

  case TypeReprKind::Metatype:
    return resolveMetatypeType(cast<MetatypeTypeRepr>(repr), options);

  case TypeReprKind::Protocol:
    return resolveProtocolType(cast<ProtocolTypeRepr>(repr), options);

  case TypeReprKind::Fixed:
    return cast<FixedTypeRepr>(repr)->getType();
  }
  llvm_unreachable("all cases should be handled");
}

static Type rebuildWithDynamicSelf(ASTContext &Context, Type ty) {
  OptionalTypeKind OTK;
  if (auto metatypeTy = ty->getAs<MetatypeType>()) {
    return MetatypeType::get(
        rebuildWithDynamicSelf(Context, metatypeTy->getInstanceType()),
        metatypeTy->getRepresentation());
  } else if (auto optionalTy = ty->getAnyOptionalObjectType(OTK)) {
    return OptionalType::get(
        OTK, rebuildWithDynamicSelf(Context, optionalTy));
  } else {
    return DynamicSelfType::get(ty, Context);
  }
}

Type TypeResolver::resolveAttributedType(AttributedTypeRepr *repr,
                                         TypeResolutionOptions options) {
  // Copy the attributes, since we're about to start hacking on them.
  TypeAttributes attrs = repr->getAttrs();
  assert(!attrs.empty());

  return resolveAttributedType(attrs, repr->getTypeRepr(), options);
}

Type TypeResolver::resolveAttributedType(TypeAttributes &attrs,
                                         TypeRepr *repr,
                                         TypeResolutionOptions options) {
  // Remember whether this is a function parameter.
  bool isFunctionParam =
    options.contains(TR_FunctionInput) ||
    options.contains(TR_ImmediateFunctionInput);
  bool isVariadicFunctionParam =
    options.contains(TR_VariadicFunctionInput);

  // The type we're working with, in case we want to build it differently
  // based on the attributes we see.
  Type ty;
  
  // In SIL *only*, allow @thin, @thick, or @objc_metatype to apply to
  // a metatype.
  if (attrs.has(TAK_thin) || attrs.has(TAK_thick) || 
      attrs.has(TAK_objc_metatype)) {
    if (auto SF = DC->getParentSourceFile()) {
      if (SF->Kind == SourceFileKind::SIL) {
        TypeRepr *base;
        if (auto metatypeRepr = dyn_cast<MetatypeTypeRepr>(repr)) {
          base = metatypeRepr->getBase();
        } else if (auto protocolRepr = dyn_cast<ProtocolTypeRepr>(repr)) {
          base = protocolRepr->getBase();
        } else {
          base = nullptr;
        }

        if (base) {
          Optional<MetatypeRepresentation> storedRepr;
          // The instance type is not a SIL type.
          auto instanceOptions = options;
          instanceOptions -= TR_SILType;
          instanceOptions -= TR_ImmediateFunctionInput;
          instanceOptions -= TR_FunctionInput;

          auto instanceTy = resolveType(base, instanceOptions);
          if (!instanceTy || instanceTy->hasError())
            return instanceTy;

          // Check for @thin.
          if (attrs.has(TAK_thin)) {
            storedRepr = MetatypeRepresentation::Thin;
            attrs.clearAttribute(TAK_thin);
          }

          // Check for @thick.
          if (attrs.has(TAK_thick)) {
            if (storedRepr)
              TC.diagnose(repr->getStartLoc(), 
                          diag::sil_metatype_multiple_reprs);
              
            storedRepr = MetatypeRepresentation::Thick;
            attrs.clearAttribute(TAK_thick);
          }

          // Check for @objc_metatype.
          if (attrs.has(TAK_objc_metatype)) {
            if (storedRepr)
              TC.diagnose(repr->getStartLoc(), 
                          diag::sil_metatype_multiple_reprs);
              
            storedRepr = MetatypeRepresentation::ObjC;
            attrs.clearAttribute(TAK_objc_metatype);
          }

          if (instanceTy->hasError()) {
            ty = instanceTy;
          } else if (auto metatype = dyn_cast<MetatypeTypeRepr>(repr)) {
            ty = buildMetatypeType(metatype, instanceTy, storedRepr);
          } else {
            ty = buildProtocolType(cast<ProtocolTypeRepr>(repr),
                                   instanceTy, storedRepr);
          }
        }
      }
    }
  }

  // Pass down the variable function type attributes to the
  // function-type creator.
  static const TypeAttrKind FunctionAttrs[] = {
    TAK_convention, TAK_noreturn, TAK_pseudogeneric,
    TAK_callee_owned, TAK_callee_guaranteed, TAK_noescape, TAK_autoclosure,
    TAK_escaping
  };

  auto checkUnsupportedAttr = [&](TypeAttrKind attr) {
    if (attrs.has(attr)) {
      TC.diagnose(attrs.getLoc(attr), diag::attribute_not_supported);
      attrs.clearAttribute(attr);
    }
  };
  
  // Some function representation attributes are not supported at source level;
  // only SIL knows how to handle them.  Reject them unless this is a SIL input.
  if (!(options & TR_SILType)) {
    for (auto silOnlyAttr : {TAK_callee_owned, TAK_callee_guaranteed}) {
      checkUnsupportedAttr(silOnlyAttr);
    }
  }  

  // Other function representation attributes are not normally supported at
  // source level, but we want to support them there in SIL files.
  auto SF = DC->getParentSourceFile();
  if (!SF || SF->Kind != SourceFileKind::SIL) {
    for (auto silOnlyAttr : {TAK_thin, TAK_thick}) {
      checkUnsupportedAttr(silOnlyAttr);
    }
  }
  
  bool hasFunctionAttr = false;
  for (auto i : FunctionAttrs)
    if (attrs.has(i)) {
      hasFunctionAttr = true;
      break;
    }

  // Function attributes require a syntactic function type.
  FunctionTypeRepr *fnRepr = dyn_cast<FunctionTypeRepr>(repr);

  if (hasFunctionAttr && fnRepr && (options & TR_SILType)) {
    SILFunctionType::Representation rep;

    auto calleeConvention = ParameterConvention::Direct_Unowned;
    if (attrs.has(TAK_callee_owned)) {
      if (attrs.has(TAK_callee_guaranteed)) {
        TC.diagnose(attrs.getLoc(TAK_callee_owned),
                    diag::sil_function_repeat_convention, /*callee*/ 2);
      }
      calleeConvention = ParameterConvention::Direct_Owned;
    } else if (attrs.has(TAK_callee_guaranteed)) {
      calleeConvention = ParameterConvention::Direct_Guaranteed;
    }

    if (!attrs.hasConvention()) {
      rep = SILFunctionType::Representation::Thick;
    } else {
      // SIL exposes a greater number of conventions than Swift source.
      auto parsedRep =
      llvm::StringSwitch<Optional<SILFunctionType::Representation>>
      (attrs.getConvention())
      .Case("thick", SILFunctionType::Representation::Thick)
      .Case("block", SILFunctionType::Representation::Block)
      .Case("thin", SILFunctionType::Representation::Thin)
      .Case("c", SILFunctionType::Representation::CFunctionPointer)
      .Case("method", SILFunctionType::Representation::Method)
      .Case("objc_method", SILFunctionType::Representation::ObjCMethod)
      .Case("witness_method", SILFunctionType::Representation::WitnessMethod)
      .Default(None);
      if (!parsedRep) {
        TC.diagnose(attrs.getLoc(TAK_convention),
                    diag::unsupported_sil_convention, attrs.getConvention());
        rep = SILFunctionType::Representation::Thin;
      } else {
        rep = *parsedRep;
      }
    }

    // Resolve the function type directly with these attributes.
    SILFunctionType::ExtInfo extInfo(rep, attrs.has(TAK_pseudogeneric));

    ty = resolveSILFunctionType(fnRepr, options, extInfo, calleeConvention);
    if (!ty || ty->hasError()) return ty;
  } else if (hasFunctionAttr && fnRepr) {

    FunctionType::Representation rep = FunctionType::Representation::Swift;
    if (attrs.hasConvention()) {
      auto parsedRep =
        llvm::StringSwitch<Optional<FunctionType::Representation>>
          (attrs.getConvention())
          .Case("swift", FunctionType::Representation::Swift)
          .Case("block", FunctionType::Representation::Block)
          .Case("thin", FunctionType::Representation::Thin)
          .Case("c", FunctionType::Representation::CFunctionPointer)
          .Default(None);
      if (!parsedRep) {
        TC.diagnose(attrs.getLoc(TAK_convention),
                    diag::unsupported_convention, attrs.getConvention());
        rep = FunctionType::Representation::Swift;
      } else {
        rep = *parsedRep;
      }
    }

    // @autoclosure is only valid on parameters.
    if (!isFunctionParam && attrs.has(TAK_autoclosure)) {
      TC.diagnose(attrs.getLoc(TAK_autoclosure),
                  isVariadicFunctionParam
                      ? diag::attr_not_on_variadic_parameters
                      : diag::attr_only_on_parameters, "@autoclosure");
      attrs.clearAttribute(TAK_autoclosure);
    }

    // @noreturn has been replaced with a 'Never' return type.
    if (fnRepr && attrs.has(TAK_noreturn)) {
      auto &SM = TC.Context.SourceMgr;
      auto loc = attrs.getLoc(TAK_noreturn);
      auto attrRange = SourceRange(
        loc.getAdvancedLoc(-1),
        Lexer::getLocForEndOfToken(SM, loc));

      auto resultRange = fnRepr->getResultTypeRepr()->getSourceRange();

      TC.diagnose(loc, diag::noreturn_not_supported)
          .fixItRemove(attrRange)
          .fixItReplace(resultRange, "Never");
    }

    // Resolve the function type directly with these attributes.
    FunctionType::ExtInfo extInfo(rep,
                                  attrs.has(TAK_autoclosure),
                                  attrs.has(TAK_noescape),
                                  fnRepr->throws());

    ty = resolveASTFunctionType(fnRepr, options, extInfo);
    if (!ty || ty->hasError()) return ty;
  }

  auto instanceOptions = options;
  instanceOptions -= TR_ImmediateFunctionInput;
  instanceOptions -= TR_FunctionInput;

  // If we didn't build the type differently above, we might have
  // a typealias pointing at a function type with the @escaping
  // attribute. Resolve the type as if it were in non-parameter
  // context, and then set isNoEscape if @escaping is not present.
  if (!ty) ty = resolveType(repr, instanceOptions);
  if (!ty || ty->hasError()) return ty;

  // Handle @escaping
  if (hasFunctionAttr && ty->is<FunctionType>()) {
    if (attrs.has(TAK_escaping)) {
      // For compatibility with 3.0, we don't emit an error if it appears on a
      // variadic argument list.
      bool skipDiagnostic =
          isVariadicFunctionParam && Context.isSwiftVersion3();

      // The attribute is meaningless except on parameter types.
      bool shouldDiagnose = !isFunctionParam && !skipDiagnostic;
      if (shouldDiagnose) {
        auto &SM = TC.Context.SourceMgr;
        auto loc = attrs.getLoc(TAK_escaping);
        auto attrRange = SourceRange(
          loc.getAdvancedLoc(-1),
          Lexer::getLocForEndOfToken(SM, loc));

        TC.diagnose(loc, diag::escaping_non_function_parameter)
            .fixItRemove(attrRange);

        // Try to find a helpful note based on how the type is being used
        if (options.contains(TR_ImmediateOptionalTypeArgument)) {
          TC.diagnose(repr->getLoc(), diag::escaping_optional_type_argument);
        }
      }

      attrs.clearAttribute(TAK_escaping);
    } else {
      // No attribute; set the isNoEscape bit if we're in parameter context.
      ty = applyNonEscapingFromContext(DC, ty, options);
    }
  }

  if (hasFunctionAttr && !fnRepr) {
    // @autoclosure usually auto-implies @noescape, don't complain about both
    // of them.
    if (attrs.has(TAK_autoclosure))
      attrs.clearAttribute(TAK_noescape);

    for (auto i : FunctionAttrs) {
      if (attrs.has(i)) {
        TC.diagnose(attrs.getLoc(i), diag::attribute_requires_function_type,
                    TypeAttributes::getAttrName(i));
        attrs.clearAttribute(i);
      }
    }
  } else if (hasFunctionAttr && fnRepr) {
    // Remove the function attributes from the set so that we don't diagnose.
    for (auto i : FunctionAttrs)
      attrs.clearAttribute(i);
    attrs.convention = None;
  }

  // In SIL, handle @opened (n), which creates an existential archetype.
  if (attrs.has(TAK_opened)) {
    if (!ty->isExistentialType()) {
      TC.diagnose(attrs.getLoc(TAK_opened), diag::opened_non_protocol, ty);
    } else {
      ty = ArchetypeType::getOpened(ty, attrs.OpenedID);
    }
    attrs.clearAttribute(TAK_opened);
  }

  // In SIL files *only*, permit @weak and @unowned to apply directly to types.
  if (attrs.hasOwnership()) {
    if (auto SF = DC->getParentSourceFile()) {
      if (SF->Kind == SourceFileKind::SIL) {
        if (((attrs.has(TAK_sil_weak) || attrs.has(TAK_sil_unmanaged)) &&
             ty->getAnyOptionalObjectType()) ||
            (!attrs.has(TAK_sil_weak) && ty->hasReferenceSemantics())) {
          ty = ReferenceStorageType::get(ty, attrs.getOwnership(), Context);
          attrs.clearOwnership();
        }
      }
    }
  }
  
  // In SIL *only*, allow @block_storage to specify a block storage type.
  if ((options & TR_SILType) && attrs.has(TAK_block_storage)) {
    ty = SILBlockStorageType::get(ty->getCanonicalType());
    attrs.clearAttribute(TAK_block_storage);
  }
  
  // In SIL *only*, allow @box to specify a box type.
  if ((options & TR_SILType) && attrs.has(TAK_box)) {
    ty = SILBoxType::get(ty->getCanonicalType());
    attrs.clearAttribute(TAK_box);
  }

  // In SIL *only*, allow @dynamic_self to specify a dynamic Self type.
  if ((options & TR_SILMode) && attrs.has(TAK_dynamic_self)) {
    ty = rebuildWithDynamicSelf(TC.Context, ty);
    attrs.clearAttribute(TAK_dynamic_self);
  }

  for (unsigned i = 0; i != TypeAttrKind::TAK_Count; ++i)
    if (attrs.has((TypeAttrKind)i))
      TC.diagnose(attrs.getLoc((TypeAttrKind)i),
                  diag::attribute_does_not_apply_to_type);

  return ty;
}

Type TypeResolver::resolveASTFunctionType(FunctionTypeRepr *repr,
                                          TypeResolutionOptions options,
                                          FunctionType::ExtInfo extInfo) {
  options -= TR_ImmediateFunctionInput;
  options -= TR_FunctionInput;

  Type inputTy = resolveType(repr->getArgsTypeRepr(),
                             options | TR_ImmediateFunctionInput);
  if (!inputTy || inputTy->hasError()) return inputTy;

  Type outputTy = resolveType(repr->getResultTypeRepr(), options);
  if (!outputTy || outputTy->hasError()) return outputTy;

  extInfo = extInfo.withThrows(repr->throws());

  // If this is a function type without parens around the parameter list,
  // diagnose this and produce a fixit to add them.
  if (!isa<TupleTypeRepr>(repr->getArgsTypeRepr()) &&
      !repr->isWarnedAbout()) {
    auto args = repr->getArgsTypeRepr();
    TC.diagnose(args->getStartLoc(), diag::function_type_no_parens)
      .highlight(args->getSourceRange())
      .fixItInsert(args->getStartLoc(), "(")
      .fixItInsertAfter(args->getEndLoc(), ")");
    
    // Don't emit this warning three times when in generics.
    repr->setWarned();
  }

  // SIL uses polymorphic function types to resolve overloaded member functions.
  if (auto genericEnv = repr->getGenericEnvironment()) {
    inputTy = genericEnv->mapTypeOutOfContext(inputTy);
    outputTy = genericEnv->mapTypeOutOfContext(outputTy);
    return GenericFunctionType::get(genericEnv->getGenericSignature(),
                                    inputTy, outputTy, extInfo);
  }

  auto fnTy = FunctionType::get(inputTy, outputTy, extInfo);
  // If the type is a block or C function pointer, it must be representable in
  // ObjC.
  switch (auto rep = extInfo.getRepresentation()) {
  case AnyFunctionType::Representation::Block:
  case AnyFunctionType::Representation::CFunctionPointer:
    if (!fnTy->isRepresentableIn(ForeignLanguage::ObjectiveC, DC)) {
      StringRef strName =
        rep == AnyFunctionType::Representation::Block ? "block" : "c";
      auto extInfo2 =
        extInfo.withRepresentation(AnyFunctionType::Representation::Swift);
      auto simpleFnTy = FunctionType::get(inputTy, outputTy, extInfo2);
      TC.diagnose(repr->getStartLoc(), diag::objc_convention_invalid,
                  simpleFnTy, strName);
    }
    break;

  case AnyFunctionType::Representation::Thin:
  case AnyFunctionType::Representation::Swift:
    break;
  }
  
  return fnTy;
}

Type TypeResolver::resolveSILBoxType(SILBoxTypeRepr *repr,
                                     TypeResolutionOptions options) {
  // Resolve the field types.
  SmallVector<SILField, 4> fields;
  {
    // Resolve field types using the box type's generic environment, if it
    // has one. (TODO: Field types should never refer to generic parameters
    // outside the box's own environment; we should really validate that...)
    Optional<GenericTypeToArchetypeResolver>
      resolveSILBoxGenericParams;
    Optional<llvm::SaveAndRestore<GenericTypeResolver*>>
      useSILBoxGenericEnv;
    if (auto env = repr->getGenericEnvironment()) {
      resolveSILBoxGenericParams = GenericTypeToArchetypeResolver(env);
      useSILBoxGenericEnv.emplace(Resolver, &*resolveSILBoxGenericParams);
    }
    
    for (auto &fieldRepr : repr->getFields()) {
      auto fieldTy = resolveType(fieldRepr.FieldType, options);
      fields.push_back({fieldTy->getCanonicalType(), fieldRepr.Mutable});
    }
  }

  // Substitute out parsed context types into interface types.
  CanGenericSignature genericSig;
  if (auto *genericEnv = repr->getGenericEnvironment()) {
    genericSig = genericEnv->getGenericSignature()->getCanonicalSignature();
    
    for (auto &field : fields) {
      auto transTy = genericEnv->mapTypeOutOfContext(field.getLoweredType());
      field = {transTy->getCanonicalType(), field.isMutable()};
    }
  }
  
  // Resolve the generic arguments.
  // Start by building a TypeSubstitutionMap.
  SmallVector<Substitution, 4> genericArgs;
  if (genericSig) {
    TypeSubstitutionMap genericArgMap;
    ArrayRef<GenericTypeParamType *> params;

    params = genericSig->getGenericParams();
    if (repr->getGenericArguments().size()
          != genericSig->getAllDependentTypes().size()) {
      TC.diagnose(repr->getLoc(), diag::sil_box_arg_mismatch);
      return ErrorType::get(Context);
    }
  
    for (unsigned i : indices(params)) {
      auto argTy = resolveType(repr->getGenericArguments()[i], options);
      genericArgMap.insert({params[i], argTy->getCanonicalType()});
    }
    
    bool ok = true;
    genericSig->getSubstitutions(genericArgMap,
      [&](CanType depTy, Type replacement, ProtocolType *proto)
      -> ProtocolConformanceRef {
        auto result = TC.conformsToProtocol(replacement, proto->getDecl(), DC,
                                            ConformanceCheckOptions());
        // TODO: getSubstitutions callback ought to return Optional.
        if (!result) {
          ok = false;
          return ProtocolConformanceRef(proto->getDecl());
        }
        
        return *result;
      },
      genericArgs);

    if (!ok)
      return ErrorType::get(Context);
    
    // Canonicalize the replacement types.
    for (auto &arg : genericArgs) {
      arg = Substitution(arg.getReplacement()->getCanonicalType(),
                         arg.getConformances());
    }
  }
  
  auto layout = SILLayout::get(Context, genericSig, fields);
  return SILBoxType::get(Context, layout, genericArgs);
}

Type TypeResolver::resolveSILFunctionType(FunctionTypeRepr *repr,
                                          TypeResolutionOptions options,
                                          SILFunctionType::ExtInfo extInfo,
                                          ParameterConvention callee) {
  options -= TR_ImmediateFunctionInput;
  options -= TR_FunctionInput;

  bool hasError = false;

  // Resolve parameter and result types using the function's generic
  // environment.
  SmallVector<SILParameterInfo, 4> params;
  SmallVector<SILResultInfo, 4> results;
  Optional<SILResultInfo> errorResult;
  {
    Optional<GenericTypeToArchetypeResolver>
      resolveSILFunctionGenericParams;
    Optional<llvm::SaveAndRestore<GenericTypeResolver*>>
      useSILFunctionGenericEnv;
    
    // Resolve generic params using the function's generic environment, if it
    // has one.
    if (auto env = repr->getGenericEnvironment()) {
      resolveSILFunctionGenericParams = GenericTypeToArchetypeResolver(env);
      useSILFunctionGenericEnv.emplace(Resolver,
                                       &*resolveSILFunctionGenericParams);
    }
    
    if (auto tuple = dyn_cast<TupleTypeRepr>(repr->getArgsTypeRepr())) {
      // SIL functions cannot be variadic.
      if (tuple->hasEllipsis()) {
        TC.diagnose(tuple->getEllipsisLoc(), diag::sil_function_ellipsis);
      }
      // SIL functions cannot have parameter names.
      for (auto nameLoc : tuple->getUnderscoreLocs()) {
        if (nameLoc.isValid())
          TC.diagnose(nameLoc, diag::sil_function_input_label);
      }

      for (auto elt : tuple->getElements()) {
        auto param = resolveSILParameter(elt,
                                         options | TR_ImmediateFunctionInput);
        params.push_back(param);
        if (!param.getType()) return nullptr;

        if (param.getType()->hasError())
          hasError = true;
      }
    } else {
      SILParameterInfo param = resolveSILParameter(repr->getArgsTypeRepr(),
                                           options | TR_ImmediateFunctionInput);
      params.push_back(param);
      if (!param.getType()) return nullptr;

      if (param.getType()->hasError())
        hasError = true;
    }

    {
      // FIXME: Deal with unsatisfied dependencies.
      if (resolveSILResults(repr->getResultTypeRepr(), options,
                            results, errorResult)) {
        hasError = true;
      }
    }
  } // restore generic type resolver

  if (hasError) {
    return ErrorType::get(Context);
  }

  // FIXME: Remap the parsed context types to interface types.
  CanGenericSignature genericSig;
  SmallVector<SILParameterInfo, 4> interfaceParams;
  SmallVector<SILResultInfo, 4> interfaceResults;
  Optional<SILResultInfo> interfaceErrorResult;
  if (auto *genericEnv = repr->getGenericEnvironment()) {
    genericSig = genericEnv->getGenericSignature()->getCanonicalSignature();
 
    for (auto &param : params) {
      auto transParamType = genericEnv->mapTypeOutOfContext(
          param.getType())->getCanonicalType();
      interfaceParams.push_back(param.getWithType(transParamType));
    }
    for (auto &result : results) {
      auto transResultType = genericEnv->mapTypeOutOfContext(
          result.getType())->getCanonicalType();
      interfaceResults.push_back(result.getWithType(transResultType));
    }

    if (errorResult) {
      auto transErrorResultType = genericEnv->mapTypeOutOfContext(
          errorResult->getType())->getCanonicalType();
      interfaceErrorResult =
        errorResult->getWithType(transErrorResultType);
    }
  } else {
    interfaceParams = params;
    interfaceResults = results;
    interfaceErrorResult = errorResult;
  }
  return SILFunctionType::get(genericSig, extInfo,
                              callee,
                              interfaceParams, interfaceResults,
                              interfaceErrorResult,
                              Context);
}

SILParameterInfo TypeResolver::resolveSILParameter(
                                 TypeRepr *repr,
                                 TypeResolutionOptions options) {
  assert((options & TR_FunctionInput) | (options & TR_ImmediateFunctionInput) &&
         "Parameters should be marked as inputs");
  auto convention = DefaultParameterConvention;
  Type type;
  bool hadError = false;

  if (auto attrRepr = dyn_cast<AttributedTypeRepr>(repr)) {
    auto attrs = attrRepr->getAttrs();
    auto checkFor = [&](TypeAttrKind tak, ParameterConvention attrConv) {
      if (!attrs.has(tak)) return;
      if (convention != DefaultParameterConvention) {
        TC.diagnose(attrs.getLoc(tak), diag::sil_function_repeat_convention,
                    /*input*/ 0);
        hadError = true;
      }
      attrs.clearAttribute(tak);
      convention = attrConv;
    };
    checkFor(TypeAttrKind::TAK_in_guaranteed,
             ParameterConvention::Indirect_In_Guaranteed);
    checkFor(TypeAttrKind::TAK_in, ParameterConvention::Indirect_In);
    checkFor(TypeAttrKind::TAK_inout, ParameterConvention::Indirect_Inout);
    checkFor(TypeAttrKind::TAK_inout_aliasable,
             ParameterConvention::Indirect_InoutAliasable);
    checkFor(TypeAttrKind::TAK_owned, ParameterConvention::Direct_Owned);
    checkFor(TypeAttrKind::TAK_guaranteed,
             ParameterConvention::Direct_Guaranteed);

    type = resolveAttributedType(attrs, attrRepr->getTypeRepr(), options);
  } else {
    type = resolveType(repr, options);
  }

  if (!type || type->hasError()) {
    hadError = true;

  // Diagnose types that are illegal in SIL.
  } else if (!type->isLegalSILType()) {
    TC.diagnose(repr->getLoc(), diag::illegal_sil_type, type);
    hadError = true;
  }

  if (hadError) type = ErrorType::get(Context);
  return SILParameterInfo(type->getCanonicalType(), convention);
}

bool TypeResolver::resolveSingleSILResult(TypeRepr *repr,
                                          TypeResolutionOptions options,
                              SmallVectorImpl<SILResultInfo> &ordinaryResults,
                                       Optional<SILResultInfo> &errorResult) {
  Type type;
  auto convention = DefaultResultConvention;
  bool isErrorResult = false;

  if (auto attrRepr = dyn_cast<AttributedTypeRepr>(repr)) {
    // Copy the attributes out; we're going to destructively modify them.
    auto attrs = attrRepr->getAttrs();

    // Recognize @error.
    if (attrs.has(TypeAttrKind::TAK_error)) {
      attrs.clearAttribute(TypeAttrKind::TAK_error);
      isErrorResult = true;

      // Error results are always implicitly @owned.
      convention = ResultConvention::Owned;
    }

    // Recognize result conventions.
    bool hadError = false;
    auto checkFor = [&](TypeAttrKind tak, ResultConvention attrConv) {
      if (!attrs.has(tak)) return;
      if (convention != DefaultResultConvention) {
        TC.diagnose(attrs.getLoc(tak), diag::sil_function_repeat_convention,
                    /*result*/ 1);
        hadError = true;
      }
      attrs.clearAttribute(tak);
      convention = attrConv;
    };
    checkFor(TypeAttrKind::TAK_out, ResultConvention::Indirect);
    checkFor(TypeAttrKind::TAK_owned, ResultConvention::Owned);
    checkFor(TypeAttrKind::TAK_unowned_inner_pointer,
             ResultConvention::UnownedInnerPointer);
    checkFor(TypeAttrKind::TAK_autoreleased, ResultConvention::Autoreleased);
    if (hadError) return true;

    type = resolveAttributedType(attrs, attrRepr->getTypeRepr(), options);
  } else {
    type = resolveType(repr, options);
  }

  // Propagate type-resolution errors out.
  if (!type || type->hasError()) return true;

  // Diagnose types that are illegal in SIL.
  if (!type->isLegalSILType()) {
    TC.diagnose(repr->getStartLoc(), diag::illegal_sil_type, type);
    return false;
  }

  assert(!isErrorResult || convention == ResultConvention::Owned);
  SILResultInfo resolvedResult(type->getCanonicalType(), convention);

  if (!isErrorResult) {
    ordinaryResults.push_back(resolvedResult);
    return false;
  }

  // Error result types must have pointer-like representation.
  // FIXME: check that here?

  // We don't expect to have a reason to support multiple independent
  // error results.  (Would this be disjunctive or conjunctive?)
  if (errorResult.hasValue()) {
    TC.diagnose(repr->getStartLoc(),
                diag::sil_function_multiple_error_results);
    return true;
  }

  errorResult = resolvedResult;
  return false;
}

bool TypeResolver::resolveSILResults(TypeRepr *repr,
                                     TypeResolutionOptions options,
                                SmallVectorImpl<SILResultInfo> &ordinaryResults,
                                Optional<SILResultInfo> &errorResult) {
  if (auto tuple = dyn_cast<TupleTypeRepr>(repr)) {
    bool hadError = false;
    for (auto nameLoc : tuple->getUnderscoreLocs()) {
      if (nameLoc.isValid())
        TC.diagnose(nameLoc, diag::sil_function_output_label);
    }
    for (auto elt : tuple->getElements()) {
      if (resolveSingleSILResult(elt, options, ordinaryResults, errorResult))
        hadError = true;
    }
    return hadError;
  }

  return resolveSingleSILResult(repr, options, ordinaryResults, errorResult);
}

Type TypeResolver::resolveInOutType(InOutTypeRepr *repr,
                                    TypeResolutionOptions options) {
  // inout is only valid for function parameters.
  if (!(options & TR_FunctionInput) &&
      !(options & TR_ImmediateFunctionInput)) {
    TC.diagnose(repr->getInOutLoc(),
                (options & TR_VariadicFunctionInput)
                    ? diag::attr_not_on_variadic_parameters
                    : diag::attr_only_on_parameters,
                "'inout'");
    repr->setInvalid();
    return ErrorType::get(Context);
  }

  // Anything within the inout isn't a parameter anymore.
  options -= TR_ImmediateFunctionInput;
  options -= TR_FunctionInput;

  Type ty = resolveType(cast<InOutTypeRepr>(repr)->getBase(), options);
  if (!ty || ty->hasError()) return ty;
  return InOutType::get(ty);
}


Type TypeResolver::resolveArrayType(ArrayTypeRepr *repr,
                                    TypeResolutionOptions options) {
  // FIXME: diagnose non-materializability of element type!
  Type baseTy = resolveType(repr->getBase(), withoutContext(options));
  if (!baseTy || baseTy->hasError()) return baseTy;

  auto sliceTy = TC.getArraySliceType(repr->getBrackets().Start, baseTy);
  if (!sliceTy)
    return ErrorType::get(Context);

  return sliceTy;
}

Type TypeResolver::resolveDictionaryType(DictionaryTypeRepr *repr,
                                         TypeResolutionOptions options) {
  // FIXME: diagnose non-materializability of key/value type?
  Type keyTy = resolveType(repr->getKey(), withoutContext(options));
  if (!keyTy || keyTy->hasError()) return keyTy;

  Type valueTy = resolveType(repr->getValue(), withoutContext(options));
  if (!valueTy || valueTy->hasError()) return valueTy;

  auto dictDecl = TC.Context.getDictionaryDecl();

  if (auto dictTy = TC.getDictionaryType(repr->getBrackets().Start, keyTy, 
                                         valueTy)) {
    // Check the requirements on the generic arguments.
    auto unboundTy = dictDecl->getDeclaredType();
    TypeLoc args[2] = { TypeLoc(repr->getKey()), TypeLoc(repr->getValue()) };
    args[0].setType(keyTy, true);
    args[1].setType(valueTy, true);

    if (!TC.applyUnboundGenericArguments(
            unboundTy, dictDecl, repr->getStartLoc(), DC, args,
            options, Resolver, UnsatisfiedDependency)) {
      return nullptr;
    }

    return dictTy;
  }

  return ErrorType::get(Context);
}

Type TypeResolver::resolveOptionalType(OptionalTypeRepr *repr,
                                       TypeResolutionOptions options) {
  auto elementOptions = withoutContext(options, true);
  elementOptions |= TR_ImmediateOptionalTypeArgument;

  // The T in T? is a generic type argument and therefore always an AST type.
  // FIXME: diagnose non-materializability of element type!
  Type baseTy = resolveType(repr->getBase(), elementOptions);
  if (!baseTy || baseTy->hasError()) return baseTy;

  auto optionalTy = TC.getOptionalType(repr->getQuestionLoc(), baseTy);
  if (!optionalTy) return ErrorType::get(Context);

  return optionalTy;
}

Type TypeResolver::resolveImplicitlyUnwrappedOptionalType(
       ImplicitlyUnwrappedOptionalTypeRepr *repr,
       TypeResolutionOptions options) {
  auto elementOptions = withoutContext(options, true);
  elementOptions |= TR_ImmediateOptionalTypeArgument;

  // The T in T! is a generic type argument and therefore always an AST type.
  // FIXME: diagnose non-materializability of element type!
  Type baseTy = resolveType(repr->getBase(), elementOptions);
  if (!baseTy || baseTy->hasError()) return baseTy;

  auto uncheckedOptionalTy =
    TC.getImplicitlyUnwrappedOptionalType(repr->getExclamationLoc(), baseTy);
  if (!uncheckedOptionalTy)
    return ErrorType::get(Context);

  return uncheckedOptionalTy;
}

Type TypeResolver::resolveTupleType(TupleTypeRepr *repr,
                                    TypeResolutionOptions options) {
  bool isImmediateFunctionInput = options.contains(TR_ImmediateFunctionInput);
  SmallVector<TupleTypeElt, 8> elements;
  elements.reserve(repr->getNumElements());
  
  // If this is the top level of a function input list, peel off the
  // ImmediateFunctionInput marker and install a FunctionInput one instead.
  auto elementOptions = options;
  if (repr->isParenType()) {
    // If we have a single ParenType, don't clear the context bits; we
    // still want to parse the type contained therein as if it were in
    // parameter position, meaning function types are not @escaping by
    // default. We still want to reduce `ImmediateFunctionInput` to
    // `FunctionInput` so that e.g. ((foo: Int)) -> Int is considered a
    // tuple argument rather than a labeled Int argument.
    if (isImmediateFunctionInput) {
      elementOptions -= TR_ImmediateFunctionInput;
      elementOptions |= TR_FunctionInput;
    }
  } else {
    elementOptions = withoutContext(elementOptions, true);
    if (isImmediateFunctionInput)
      elementOptions |= TR_FunctionInput;
  }

  bool complained = false;

  // Variadic tuples are not permitted.
  if (repr->hasEllipsis() &&
      !isImmediateFunctionInput) {
    TC.diagnose(repr->getEllipsisLoc(), diag::tuple_ellipsis);
    repr->removeEllipsis();
    complained = true;
  }

  for (unsigned i = 0, end = repr->getNumElements(); i != end; ++i) {
    auto *tyR = repr->getElement(i);
    Type ty;
    Identifier name;
    bool variadic = false;

    // If the element has a label, stash the label.
    // FIXME: Preserve and serialize parameter names in function types, maybe
    // with a new sugar type.
    if (!isImmediateFunctionInput)
      name = repr->getElementName(i);

    // If the element is a variadic parameter, resolve the parameter type as if
    // it were in non-parameter position, since we want functions to be
    // @escaping in this case.
    auto thisElementOptions = elementOptions;
    if (repr->hasEllipsis() &&
        elements.size() == repr->getEllipsisIndex()) {
      thisElementOptions = withoutContext(elementOptions);
      thisElementOptions |= TR_VariadicFunctionInput;
      variadic = true;
    }

    ty = resolveType(tyR, thisElementOptions);
    if (!ty || ty->hasError()) return ty;

    // If the element is a variadic parameter, the underlying type is actually
    // an ArraySlice of the element type.
    if (variadic)
      ty = TC.getArraySliceType(repr->getEllipsisLoc(), ty);

    auto paramFlags = isImmediateFunctionInput
                          ? ParameterTypeFlags::fromParameterType(ty, variadic)
                          : ParameterTypeFlags();
    elements.emplace_back(ty, name, paramFlags);
  }

  // Single-element labeled tuples are not permitted outside of declarations
  // or SIL, either.
  if (!isImmediateFunctionInput) {
    if (elements.size() == 1 && elements[0].hasName()
        && !(options & TR_SILType)
        && !(options & TR_EnumCase)) {
      if (!complained) {
        TC.diagnose(repr->getElementNameLoc(0),
                    diag::tuple_single_element)
          .fixItRemoveChars(repr->getElementNameLoc(0),
                            repr->getElement(0)->getStartLoc());
      }

      elements[0] = TupleTypeElt(elements[0].getType());
    }
  }

  return TupleType::get(elements, Context);
}

/// Restore Swift3 behavior of ambiguous composition for source compatibility.
///
/// Currently, 'P1 & P2.Type' is parsed as (composition P1, (metatype P2))
/// In Swift3, that was (metatype (composition P1, P2)).
/// For source compatibility, before resolving Type of that, reconstruct
/// TypeRepr as so, and emit a warning with fix-it to enclose it with
/// parenthesis; '(P1 & P2).Type'
//
/// \param Comp The type composition to be checked and fixed.
///
/// \returns Fixed TypeRepr, or nullptr that indicates no need to fix.
static TypeRepr *fixCompositionWithPostfix(TypeChecker &TC,
                                           CompositionTypeRepr *Comp) {
  // Only for Swift3
  if (!TC.Context.isSwiftVersion3())
    return nullptr;

  auto Types = Comp->getTypes();
  TypeRepr *LastType = nullptr;
  for (auto i = Types.begin(), e = Types.end(); i != e; ++i) {
    if (!isa<IdentTypeRepr>(*i)) {
      // Found non-IdentType not at the last, can't help.
      if (i + 1 != e)
        return nullptr;
      LastType = *i;
    }
  }
  // Only IdentType(s) it's OK.
  if (!LastType)
    return nullptr;

  // Strip off the postfix type repr.
  SmallVector<TypeRepr *, 2> Postfixes;
  while (true) {
    if (auto T = dyn_cast<ProtocolTypeRepr>(LastType)) {
      Postfixes.push_back(LastType);
      LastType = T->getBase();
    } else if (auto T = dyn_cast<MetatypeTypeRepr>(LastType)) {
      Postfixes.push_back(LastType);
      LastType = T->getBase();
    } else if (auto T = dyn_cast<OptionalTypeRepr>(LastType)) {
      Postfixes.push_back(LastType);
      LastType = T->getBase();
    } else if (auto T =
        dyn_cast<ImplicitlyUnwrappedOptionalTypeRepr>(LastType)) {
      Postfixes.push_back(LastType);
      LastType = T->getBase();
    } else if (!isa<IdentTypeRepr>(LastType)) {
      // Found non-IdentTypeRepr, can't help;
      return nullptr;
    } else {
      break;
    }
  }
  assert(!Postfixes.empty() && isa<IdentTypeRepr>(LastType));

  // Now, we know we can fix-it. do it.
  SmallVector<TypeRepr *, 4> Protocols(Types.begin(), Types.end() - 1);
  Protocols.push_back(LastType);

  // Emit fix-it to enclose composition part into parentheses.
  TypeRepr *InnerMost = Postfixes.back();
  TC.diagnose(InnerMost->getLoc(), diag::protocol_composition_with_postfix,
      isa<ProtocolTypeRepr>(InnerMost) ? ".Protocol" :
      isa<MetatypeTypeRepr>(InnerMost) ? ".Type" :
      isa<OptionalTypeRepr>(InnerMost) ? "?" :
      isa<ImplicitlyUnwrappedOptionalTypeRepr>(InnerMost) ? "!" :
      /* unreachable */"")
    .highlight({Comp->getStartLoc(), LastType->getEndLoc()})
    .fixItInsert(Comp->getStartLoc(), "(")
    .fixItInsertAfter(LastType->getEndLoc(), ")");

  // Reconstruct postfix type repr with collected protocols.
  TypeRepr *Fixed = CompositionTypeRepr::create(
    TC.Context, Protocols, Comp->getStartLoc(),
    {Comp->getCompositionRange().Start, LastType->getEndLoc()});

  // Add back postix TypeRepr(s) to the composition.
  while (Postfixes.size()) {
    auto Postfix = Postfixes.pop_back_val();
    if (auto T = dyn_cast<ProtocolTypeRepr>(Postfix))
      Fixed = new (TC.Context) ProtocolTypeRepr(Fixed, T->getProtocolLoc());
    else if (auto T = dyn_cast<MetatypeTypeRepr>(Postfix))
      Fixed = new (TC.Context) MetatypeTypeRepr(Fixed, T->getMetaLoc());
    else if (auto T = dyn_cast<OptionalTypeRepr>(Postfix))
      Fixed = new (TC.Context) OptionalTypeRepr(Fixed, T->getQuestionLoc());
    else if (auto T = dyn_cast<ImplicitlyUnwrappedOptionalTypeRepr>(Postfix))
      Fixed = new (TC.Context)
        ImplicitlyUnwrappedOptionalTypeRepr(Fixed, T->getExclamationLoc());
    else
      llvm_unreachable("unexpected type repr");
  }

  return Fixed;
}

Type TypeResolver::resolveCompositionType(CompositionTypeRepr *repr,
                                          TypeResolutionOptions options) {

  // Fix 'P1 & P2.Type' to '(P1 & P2).Type' for Swift3
  if (auto fixed = fixCompositionWithPostfix(TC, repr))
    return resolveType(fixed, options);

  SmallVector<Type, 4> ProtocolTypes;
  for (auto tyR : repr->getTypes()) {
    Type ty = TC.resolveType(tyR, DC, withoutContext(options), Resolver);
    if (!ty || ty->hasError()) return ty;

    if (!ty->isExistentialType()) {
      TC.diagnose(tyR->getStartLoc(), diag::protocol_composition_not_protocol,
                  ty);
      continue;
    }

    ProtocolTypes.push_back(ty);
  }
  return ProtocolCompositionType::get(Context, ProtocolTypes);
}

Type TypeResolver::resolveMetatypeType(MetatypeTypeRepr *repr,
                                       TypeResolutionOptions options) {
  // The instance type of a metatype is always abstract, not SIL-lowered.
  Type ty = resolveType(repr->getBase(), withoutContext(options));
  if (!ty || ty->hasError()) return ty;

  Optional<MetatypeRepresentation> storedRepr;
  
  // In SIL mode, a metatype must have a @thin, @thick, or
  // @objc_metatype attribute, so metatypes should have been lowered
  // in resolveAttributedType.
  if (options & TR_SILType) {
    TC.diagnose(repr->getStartLoc(), diag::sil_metatype_without_repr);
    storedRepr = MetatypeRepresentation::Thick;
  }

  return buildMetatypeType(repr, ty, storedRepr);
}

Type TypeResolver::buildMetatypeType(
       MetatypeTypeRepr *repr,
       Type instanceType,
       Optional<MetatypeRepresentation> storedRepr) {
  if (instanceType->isAnyExistentialType()) {
    // TODO: diagnose invalid representations?
    return ExistentialMetatypeType::get(instanceType, storedRepr);
  } else {
    return MetatypeType::get(instanceType, storedRepr);
  }
}

Type TypeResolver::resolveProtocolType(ProtocolTypeRepr *repr,
                                       TypeResolutionOptions options) {
  // The instance type of a metatype is always abstract, not SIL-lowered.
  Type ty = resolveType(repr->getBase(), withoutContext(options));
  if (!ty || ty->hasError()) return ty;

  Optional<MetatypeRepresentation> storedRepr;
  
  // In SIL mode, a metatype must have a @thin, @thick, or
  // @objc_metatype attribute, so metatypes should have been lowered
  // in resolveAttributedType.
  if (options & TR_SILType) {
    TC.diagnose(repr->getStartLoc(), diag::sil_metatype_without_repr);
    storedRepr = MetatypeRepresentation::Thick;
  }

  return buildProtocolType(repr, ty, storedRepr);
}

Type TypeResolver::buildProtocolType(
       ProtocolTypeRepr *repr,
       Type instanceType,
       Optional<MetatypeRepresentation> storedRepr) {
  if (!instanceType->isAnyExistentialType()) {
    TC.diagnose(repr->getProtocolLoc(), diag::dot_protocol_on_non_existential,
                instanceType);
    return ErrorType::get(TC.Context);
  }

  return MetatypeType::get(instanceType, storedRepr);
}

Type TypeChecker::substMemberTypeWithBase(ModuleDecl *module,
                                          TypeDecl *member,
                                          Type baseTy) {
  // For type members of a base class, make sure we use the right
  // derived class as the parent type.
  if (auto *ownerClass = member->getDeclContext()
          ->getAsClassOrClassExtensionContext()) {
    if (auto *archetypeTy = baseTy->getAs<ArchetypeType>())
      baseTy = archetypeTy->getSuperclass();
    baseTy = baseTy->getSuperclassForDecl(ownerClass, this);
  }

  auto parentTy = baseTy;
  if (baseTy->is<ModuleType>())
    parentTy = Type();

  // The declared interface type for a generic type will have the type
  // arguments; strip them off.
  if (auto *nominalDecl = dyn_cast<NominalTypeDecl>(member)) {
    if (!isa<ProtocolDecl>(nominalDecl) &&
        nominalDecl->getGenericParams()) {
      return UnboundGenericType::get(
          nominalDecl, parentTy,
          nominalDecl->getASTContext());
    } else {
      return NominalType::get(
          nominalDecl, parentTy,
          nominalDecl->getASTContext());
    }
  }

  if (auto *aliasDecl = dyn_cast<TypeAliasDecl>(member)) {
    if (aliasDecl->getGenericParams()) {
      return UnboundGenericType::get(
          aliasDecl, parentTy,
          aliasDecl->getASTContext());
    }
  }

  auto memberType = member->getDeclaredInterfaceType();
  if (!parentTy)
    return memberType;

  auto subs = parentTy->getContextSubstitutions(member->getDeclContext());
  return memberType.subst(module, subs, SubstFlags::UseErrorType);
}

Type TypeChecker::getSuperClassOf(Type type) {
  return type->getSuperclass(this);
}

static void lookupAndAddLibraryTypes(TypeChecker &TC,
                                     ModuleDecl *Stdlib,
                                     ArrayRef<Identifier> TypeNames,
                                     llvm::DenseSet<CanType> &Types) {
  SmallVector<ValueDecl *, 4> Results;
  for (Identifier Id : TypeNames) {
    Stdlib->lookupValue({}, Id, NLKind::UnqualifiedLookup, Results);
    for (auto *VD : Results) {
      if (auto *TD = dyn_cast<TypeDecl>(VD)) {
        TC.validateDecl(TD);
        Types.insert(TD->getDeclaredInterfaceType()->getCanonicalType());
      }
    }
    Results.clear();
  }
}


namespace {

class UnsupportedProtocolVisitor
  : public TypeReprVisitor<UnsupportedProtocolVisitor>, public ASTWalker
{
  TypeChecker &TC;
  bool checkStatements;
  bool hitTopStmt;
    
public:
  UnsupportedProtocolVisitor(TypeChecker &tc, bool checkStatements)
    : TC(tc), checkStatements(checkStatements), hitTopStmt(false) { }

  bool walkToTypeReprPre(TypeRepr *T) override {
    if (T->isInvalid())
      return false;
    if (auto compound = dyn_cast<CompoundIdentTypeRepr>(T)) {
      // Only visit the last component to check, because nested typealiases in
      // existentials are okay.
      visit(compound->getComponentRange().back());
      return false;
    }
    visit(T);
    return true;
  }

  std::pair<bool, Stmt*> walkToStmtPre(Stmt *S) override {
    if (checkStatements && !hitTopStmt) {
      hitTopStmt = true;
      return { true, S };
    }

    return { false, S };
  }

  bool walkToDeclPre(Decl *D) override {
    return !checkStatements;
  }

  void visitIdentTypeRepr(IdentTypeRepr *T) {
    if (T->isInvalid())
      return;
    
    auto comp = T->getComponentRange().back();
    if (auto proto = dyn_cast_or_null<ProtocolDecl>(comp->getBoundDecl())) {
      if (!proto->existentialTypeSupported(&TC)) {
        TC.diagnose(comp->getIdLoc(), diag::unsupported_existential_type,
                    proto->getName());
        T->setInvalid();
      }
    } else if (auto alias = dyn_cast_or_null<TypeAliasDecl>(comp->getBoundDecl())) {
      if (!alias->hasInterfaceType())
        return;
      auto type = Type(alias->getDeclaredInterfaceType()->getDesugaredType());
      type.findIf([&](Type type) -> bool {
        if (T->isInvalid())
          return false;
        SmallVector<ProtocolDecl*, 2> protocols;
        if (type->isExistentialType(protocols)) {
          for (auto *proto : protocols) {
            if (proto->existentialTypeSupported(&TC))
              continue;
            
            TC.diagnose(comp->getIdLoc(), diag::unsupported_existential_type,
                        proto->getName());
            T->setInvalid();
          }
        }
        return false;
      });
    }
  }
};

} // end anonymous namespace

void TypeChecker::checkUnsupportedProtocolType(Decl *decl) {
  if (!decl || decl->isInvalid())
    return;

  // Global type aliases are okay.
  if (isa<TypeAliasDecl>(decl) &&
      decl->getDeclContext()->isModuleScopeContext())
    return;

  // Non-typealias type declarations are okay.
  if (isa<TypeDecl>(decl) && !isa<TypeAliasDecl>(decl))
    return;

  // Extensions are okay.
  if (isa<ExtensionDecl>(decl))
    return;

  UnsupportedProtocolVisitor visitor(*this, /*checkStatements=*/false);
  decl->walk(visitor);
}

void TypeChecker::checkUnsupportedProtocolType(Stmt *stmt) {
  if (!stmt)
    return;

  UnsupportedProtocolVisitor visitor(*this, /*checkStatements=*/true);
  stmt->walk(visitor);
}
