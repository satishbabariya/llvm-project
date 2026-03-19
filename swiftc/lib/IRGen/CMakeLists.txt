add_swiftc_library(SwiftIRGen
  IRGen.cpp

  LINK_LIBS
  SwiftAST
  SwiftSupport

  LINK_COMPONENTS
  Core
  Support
)
