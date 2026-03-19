add_swiftc_library(SwiftParse
  Lexer.cpp
  Parser.cpp

  LINK_LIBS
  SwiftAST
  SwiftSupport

  LINK_COMPONENTS
  Support
)
