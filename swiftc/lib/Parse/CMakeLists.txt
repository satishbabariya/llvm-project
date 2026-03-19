add_swiftc_library(SwiftParse
  Lexer.cpp
  ParseDecl.cpp
  ParseExpr.cpp
  ParseGeneric.cpp
  ParsePattern.cpp
  Parser.cpp
  ParseSIL.cpp
  ParseStmt.cpp
  ParseType.cpp
  PersistentParserState.cpp
  Scope.cpp

  LINK_LIBS
  SwiftAST
  SwiftBasic
  SwiftSupport

  LINK_COMPONENTS
  Support
)
