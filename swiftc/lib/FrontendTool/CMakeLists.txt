add_swiftc_library(swiftcFrontendTool
  ExecuteCompilerInvocation.cpp

  DEPENDS
  swiftcFrontend

  LINK_LIBS
  swiftcFrontend

  LINK_COMPONENTS
  Option
  Support
)
