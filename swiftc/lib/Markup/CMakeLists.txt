add_swiftc_library(SwiftMarkup
  AST.cpp
  LineList.cpp
  Markup.cpp

  LINK_LIBS
  SwiftBasic

  LINK_COMPONENTS
  Support
)
