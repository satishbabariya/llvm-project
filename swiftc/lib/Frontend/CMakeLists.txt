add_swiftc_library(swiftcFrontend
  CompilerInstance.cpp
  CompilerInvocation.cpp
  FrontendAction.cpp
  TextDiagnosticPrinter.cpp
  TextDiagnosticBuffer.cpp

  LINK_LIBS
  SwiftParse
  SwiftSema
  SwiftAST
  SwiftIRGen
  SwiftSupport

  LINK_COMPONENTS
  Option
  Support
  Target
  TargetParser
)
