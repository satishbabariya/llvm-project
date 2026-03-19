add_swiftc_library(SwiftSema
  Sema.cpp

  LINK_LIBS
  SwiftAST
  SwiftSupport

  LINK_COMPONENTS
  Support
)
