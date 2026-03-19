# Test runner infrastructure for Swiftc. This configures the Swiftc test trees
# for use by Lit, and delegates to LLVM's lit test handlers.

llvm_canonicalize_cmake_booleans(
  SWIFTC_STANDALONE_BUILD
  LLVM_ENABLE_PLUGINS
  LLVM_INCLUDE_EXAMPLES
)

set(SWIFTC_TOOLS_DIR ${SWIFTC_BINARY_DIR}/bin)

# FIXME In out-of-tree builds, "SHLIBDIR" is undefined and passing it to
# `configure_lit_site_cfg` leads to a configuration error.
if (SWIFTC_STANDALONE_BUILD)
  set(PATHS_FOR_PLUGINS "")
else ()
  set(PATHS_FOR_PLUGINS "SHLIBDIR")
endif ()

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
  PATHS
  ${PATHS_FOR_PLUGINS}
)

set(SWIFTC_TEST_PARAMS
  swiftc_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py)

set(SWIFTC_TEST_DEPENDS
  swiftc
)

if (NOT SWIFTC_STANDALONE_BUILD)
  list(APPEND SWIFTC_TEST_DEPENDS
    llvm-config
    FileCheck
    count
    not
    llvm-dis
    llvm-objdump
    split-file
  )
endif ()

if (SWIFTC_INCLUDE_TESTS)
  if (SWIFTC_GTEST_AVAIL)
    list(APPEND SWIFTC_TEST_DEPENDS SwiftcUnitTests)
  endif()
endif()

add_custom_target(swiftc-test-depends DEPENDS ${SWIFTC_TEST_DEPENDS})
set_target_properties(swiftc-test-depends PROPERTIES FOLDER "Swiftc/Meta")

add_lit_testsuite(check-swiftc "Running the Swiftc regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  PARAMS ${SWIFTC_TEST_PARAMS}
  DEPENDS ${SWIFTC_TEST_DEPENDS}
)
set_target_properties(check-swiftc PROPERTIES FOLDER "Swiftc/Meta")

# In case of standalone builds.
if (SWIFTC_STANDALONE_BUILD)
  add_lit_testsuites(SWIFTC ${CMAKE_CURRENT_BINARY_DIR}
    PARAMS ${SWIFTC_TEST_PARAMS}
    DEPENDS ${SWIFTC_TEST_DEPENDS})
else()
  add_lit_testsuites(SWIFTC ${CMAKE_CURRENT_SOURCE_DIR}
    PARAMS ${SWIFTC_TEST_PARAMS}
    DEPENDS ${SWIFTC_TEST_DEPENDS})
endif()
