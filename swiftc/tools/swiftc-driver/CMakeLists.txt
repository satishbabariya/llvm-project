# Infrastructure to build swiftc driver entry point. Swiftc driver depends on
# LLVM libraries.

# Set your project compile flags.
link_directories(${LLVM_LIBRARY_DIR})

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  MC
  Option
  Support
  TargetParser
)

add_swiftc_tool(swiftc
  driver.cpp
  sc1_main.cpp
)

target_link_libraries(swiftc
  PRIVATE
  swiftcFrontend
  swiftcFrontendTool
)

clang_target_link_libraries(swiftc
  PRIVATE
  clangDriver
  clangOptions
  clangBasic
)

# This creates the executable with a version appended
# and creates a symlink to it without the version
if(CYGWIN OR NOT WIN32) # but it doesn't work on Windows
  set_target_properties(swiftc PROPERTIES VERSION ${SWIFTC_EXECUTABLE_VERSION})
endif()

option(SWIFTC_PLUGIN_SUPPORT "Build Swiftc with plugin support." ON)

# Enable support for plugins, which need access to symbols from swiftc
if(SWIFTC_PLUGIN_SUPPORT)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${LLVM_LIBRARY_DIR})
  export_executable_symbols_for_plugins(swiftc)
endif()

install(TARGETS swiftc DESTINATION "${CMAKE_INSTALL_BINDIR}")
