add_swiftc_library(SwiftParse
  Confusables.cpp
  Lexer.cpp
  ParseDecl.cpp
  ParseExpr.cpp
  ParseGeneric.cpp
  ParseIfConfig.cpp
  ParsePattern.cpp
  ParseRegex.cpp
  ParseRequests.cpp
  ParseStmt.cpp
  ParseType.cpp
  ParseVersion.cpp
  Parser.cpp
  PersistentParserState.cpp

  LINK_LIBS
  SwiftAST
  SwiftBasic
  SwiftSupport

  LINK_COMPONENTS
  Support
)
