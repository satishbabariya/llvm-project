//===-- driver.cpp - Swiftc Driver -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the entry point to the swiftc driver; it is a thin wrapper
// for functionality in the Driver swiftc library.
//
//===----------------------------------------------------------------------===//

#include "clang/Driver/Driver.h"
#include "swiftc/Config/config.h"
#include "swiftc/Frontend/CompilerInvocation.h"
#include "swiftc/Frontend/TextDiagnosticPrinter.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include <stdlib.h>

// main frontend method. Lives inside sc1_main.cpp
extern int sc1_main(llvm::ArrayRef<const char *> argv, const char *argv0);

std::string getExecutablePath(const char *argv0) {
  // This just needs to be some symbol in the binary
  void *p = (void *)(intptr_t)getExecutablePath;
  return llvm::sys::fs::getMainExecutable(argv0, p);
}

// This lets us create the DiagnosticsEngine with a properly-filled-out
// DiagnosticOptions instance
static std::unique_ptr<clang::DiagnosticOptions>
createAndPopulateDiagOpts(llvm::ArrayRef<const char *> argv) {
  auto diagOpts = std::make_unique<clang::DiagnosticOptions>();

  // Ignore missingArgCount and the return value of ParseDiagnosticArgs.
  // Any errors that would be diagnosed here will also be diagnosed later,
  // when the DiagnosticsEngine actually exists.
  unsigned missingArgIndex, missingArgCount;
  llvm::opt::InputArgList args = clang::getDriverOptTable().ParseArgs(
      argv.slice(1), missingArgIndex, missingArgCount,
      llvm::opt::Visibility(clang::options::CLOption));

  (void)Swift::frontend::parseDiagnosticArgs(*diagOpts, args);

  return diagOpts;
}

static int executeSC1Tool(llvm::SmallVectorImpl<const char *> &argV) {
  llvm::StringRef tool = argV[1];
  if (tool == "-sc1")
    return sc1_main(llvm::ArrayRef(argV).slice(2), argV[0]);

  // Reject unknown tools.
  llvm::errs() << "error: unknown integrated tool '" << tool << "'. "
               << "Valid tools include '-sc1'.\n";
  return 1;
}

static void ExpandResponseFiles(llvm::StringSaver &saver,
                                llvm::SmallVectorImpl<const char *> &args) {
  // We're defaulting to the GNU syntax, since we don't have a CL mode.
  llvm::cl::TokenizerCallback tokenizer = &llvm::cl::TokenizeGNUCommandLine;
  llvm::cl::ExpansionContext ExpCtx(saver.getAllocator(), tokenizer);
  if (llvm::Error Err = ExpCtx.expandResponseFiles(args)) {
    llvm::errs() << toString(std::move(Err)) << '\n';
  }
}

int main(int argc, const char **argv) {

  // Initialize variables to call the driver
  llvm::InitLLVM x(argc, argv);
  llvm::SmallVector<const char *, 256> args(argv, argv + argc);

  clang::driver::ParsedClangName targetandMode =
      clang::driver::ToolChain::getTargetAndModeFromProgramName(argv[0]);
  std::string driverPath = getExecutablePath(args[0]);

  llvm::BumpPtrAllocator a;
  llvm::StringSaver saver(a);
  ExpandResponseFiles(saver, args);

  // Check if swiftc is in the frontend mode
  auto firstArg = std::find_if(args.begin() + 1, args.end(),
                               [](const char *a) { return a != nullptr; });
  if (firstArg != args.end()) {
    if (llvm::StringRef(args[1]).starts_with("-cc1")) {
      llvm::errs() << "error: unknown integrated tool '" << args[1] << "'. "
                   << "Valid tools include '-sc1'.\n";
      return 1;
    }
    // Call swiftc frontend
    if (llvm::StringRef(args[1]).starts_with("-sc1")) {
      return executeSC1Tool(args);
    }
  }

  // Not in the frontend mode - continue in the compiler driver mode.

  // Create DiagnosticsEngine for the compiler driver
  std::unique_ptr<clang::DiagnosticOptions> diagOpts =
      createAndPopulateDiagOpts(args);
  Swift::frontend::TextDiagnosticPrinter *diagClient =
      new Swift::frontend::TextDiagnosticPrinter(llvm::errs(), *diagOpts);

  diagClient->setPrefix(
      std::string(llvm::sys::path::stem(getExecutablePath(args[0]))));

  clang::DiagnosticsEngine diags(clang::DiagnosticIDs::create(), *diagOpts,
                                 diagClient);

  // Prepare the driver
  clang::driver::Driver theDriver(driverPath,
                                  llvm::sys::getDefaultTargetTriple(), diags,
                                  "swiftc LLVM compiler");
  theDriver.setTargetAndMode(targetandMode);
  theDriver.setPreferredLinker(SWIFTC_DEFAULT_LINKER);
  std::unique_ptr<clang::driver::Compilation> c(
      theDriver.BuildCompilation(args));
  llvm::SmallVector<std::pair<int, const clang::driver::Command *>, 4>
      failingCommands;

  // Run the driver
  int res = 1;
  bool isCrash = false;
  res = theDriver.ExecuteCompilation(*c, failingCommands);

  for (const auto &p : failingCommands) {
    int commandRes = p.first;
    const clang::driver::Command *failingCommand = p.second;
    if (!res)
      res = commandRes;

    // If result status is < 0, then the driver command signalled an error.
    isCrash = commandRes < 0;
#ifdef _WIN32
    isCrash |= commandRes == 3;
#endif
    if (isCrash) {
      theDriver.generateCompilationDiagnostics(*c, *failingCommand);
      break;
    }
  }

  // If we have multiple failing commands, we return the result of the first
  // failing command.
  return res;
}
