include(GNUInstallPackageDir)
include(ExtendPath)
include(FindPrefixFromConfig)

# Generate a list of CMake library targets so that other CMake projects can
# link against them.
set(SWIFTC_INSTALL_PACKAGE_DIR "${CMAKE_INSTALL_PACKAGEDIR}/swiftc" CACHE STRING
  "Path for CMake subdirectory for Swiftc (defaults to '${CMAKE_INSTALL_PACKAGEDIR}/swiftc')")
set(swiftc_cmake_builddir "${CMAKE_BINARY_DIR}/lib${LLVM_LIBDIR_SUFFIX}/cmake/swiftc")

# Keep this in sync with llvm/cmake/CMakeLists.txt!
set(LLVM_INSTALL_PACKAGE_DIR "${CMAKE_INSTALL_PACKAGEDIR}/llvm" CACHE STRING
  "Path for CMake subdirectory for LLVM (defaults to '${CMAKE_INSTALL_PACKAGEDIR}/llvm')")
string(REPLACE "${CMAKE_CFG_INTDIR}" "." llvm_cmake_builddir "${LLVM_LIBRARY_DIR}")
set(llvm_cmake_builddir "${llvm_cmake_builddir}/cmake/llvm")

get_property(SWIFTC_EXPORTS GLOBAL PROPERTY SWIFTC_EXPORTS)
export(TARGETS ${SWIFTC_EXPORTS} FILE ${swiftc_cmake_builddir}/SwiftcTargets.cmake)

# Generate SwiftcConfig.cmake for the build tree.
set(SWIFTC_CONFIG_CMAKE_DIR "${swiftc_cmake_builddir}")
set(SWIFTC_CONFIG_LLVM_CMAKE_DIR "${llvm_cmake_builddir}")
set(SWIFTC_CONFIG_INCLUDE_EXPORTS "include(\"${swiftc_cmake_builddir}/SwiftcTargets.cmake\")")
set(SWIFTC_CONFIG_INCLUDE_DIRS
  "${SWIFTC_SOURCE_DIR}/include"
  "${SWIFTC_BINARY_DIR}/include"
  )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/SwiftcConfig.cmake.in
  ${swiftc_cmake_builddir}/SwiftcConfig.cmake
  @ONLY)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/SwiftcConfigVersion.cmake.in
  ${swiftc_cmake_builddir}/SwiftcConfigVersion.cmake
  @ONLY)
set(SWIFTC_CONFIG_CMAKE_DIR)
set(SWIFTC_CONFIG_LLVM_CMAKE_DIR)

# Generate SwiftcConfig.cmake for the install tree.
find_prefix_from_config(SWIFTC_CONFIG_CODE SWIFTC_INSTALL_PREFIX "${SWIFTC_INSTALL_PACKAGE_DIR}")

extend_path(SWIFTC_CONFIG_CMAKE_DIR "\${SWIFTC_INSTALL_PREFIX}" "${SWIFTC_INSTALL_PACKAGE_DIR}")
extend_path(SWIFTC_CONFIG_LLVM_CMAKE_DIR "\${SWIFTC_INSTALL_PREFIX}" "${LLVM_INSTALL_PACKAGE_DIR}")
get_config_exports_includes(Swiftc SWIFTC_CONFIG_INCLUDE_EXPORTS)
extend_path(SWIFTC_CONFIG_INCLUDE_DIRS "\${SWIFTC_INSTALL_PREFIX}" "${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/SwiftcConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/SwiftcConfig.cmake
  @ONLY)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/SwiftcConfigVersion.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/SwiftcConfigVersion.cmake
  @ONLY)

set(SWIFTC_CONFIG_CODE)
set(SWIFTC_CONFIG_CMAKE_DIR)
set(SWIFTC_CONFIG_EXPORTS_FILE)

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  install_distribution_exports(Swiftc)

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/SwiftcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/SwiftcConfigVersion.cmake
    DESTINATION ${SWIFTC_INSTALL_PACKAGE_DIR}
    COMPONENT swiftc-cmake-exports)

  if(NOT LLVM_ENABLE_IDE)
    # Add a dummy target so this can be used with LLVM_DISTRIBUTION_COMPONENTS
    add_custom_target(swiftc-cmake-exports)
    add_llvm_install_targets(install-swiftc-cmake-exports
                             COMPONENT swiftc-cmake-exports)
  endif()
endif()
