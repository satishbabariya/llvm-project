cmake_minimum_required(VERSION 3.20.0)
set(LLVM_SUBPROJECT_TITLE "Swiftc")

if(NOT DEFINED LLVM_COMMON_CMAKE_UTILS)
  set(LLVM_COMMON_CMAKE_UTILS ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
endif()
include(${LLVM_COMMON_CMAKE_UTILS}/Modules/CMakePolicy.cmake
  NO_POLICY_SCOPE)

set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)

# Swiftc requires C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SWIFTC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
  message(FATAL_ERROR "In-source builds are not allowed. \
          Please create a directory and run cmake from there, \
          passing the path to this source directory as the last argument. \
          This process created the file `CMakeCache.txt' and the directory \
          `CMakeFiles'. Please delete them.")
endif()

option(SWIFTC_ENABLE_WERROR "Fail and stop building swiftc if a warning is triggered." OFF)

# Check for a standalone build and configure as appropriate from
# there.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message("Building Swiftc as a standalone project.")
  project(Swiftc)
  set(SWIFTC_STANDALONE_BUILD ON)
else()
  set(SWIFTC_STANDALONE_BUILD OFF)
endif()

# Must go below project(..)
include(GNUInstallDirs)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "swiftc isn't supported on 32 bit CPUs")
endif()

if (SWIFTC_STANDALONE_BUILD)
  set(SWIFTC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  if (NOT MSVC_IDE)
    set(LLVM_ENABLE_ASSERTIONS ${ENABLE_ASSERTIONS}
      CACHE BOOL "Enable assertions")
    mark_as_advanced(LLVM_ENABLE_ASSERTIONS)
  endif()

  # If the user specifies a relative path to LLVM_DIR, the calls to include
  # LLVM modules fail. Append the absolute path to LLVM_DIR instead.
  if (LLVM_DIR)
    get_filename_component(LLVM_DIR_ABSOLUTE ${LLVM_DIR}
      REALPATH BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR_ABSOLUTE})
  endif()
  # We need a pre-built/installed version of LLVM.
  find_package(LLVM REQUIRED HINTS "${LLVM_DIR_ABSOLUTE}")
  if (NOT LLVM_DIR_ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
  endif()

  # Users might specify a path to CLANG_DIR that's:
  #   * a full path, or
  #   * a path relative to the path of this script.
  if (CLANG_DIR)
    get_filename_component(
      CLANG_DIR_ABSOLUTE
      ${CLANG_DIR}
      REALPATH
      BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH ${CLANG_DIR_ABSOLUTE})
    find_package(Clang REQUIRED PATHS "${CLANG_DIR_ABSOLUTE}" NO_DEFAULT_PATH)
  else()
    find_package(Clang REQUIRED)
    list(APPEND CMAKE_MODULE_PATH ${Clang_DIR})
  endif()

  # If LLVM links to zlib we need the imported targets so we can too.
  if(LLVM_ENABLE_ZLIB)
    find_package(ZLIB REQUIRED)
  endif()
  option(LLVM_ENABLE_PEDANTIC "Compile with pedantic enabled." ON)
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(USE_NO_MAYBE_UNINITIALIZED 1)
  endif()

  if (NOT DEFINED LLVM_MAIN_SRC_DIR)
    set(LLVM_MAIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../llvm")
  endif()

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  add_definitions(${LLVM_DEFINITIONS_LIST})
  list(APPEND CMAKE_REQUIRED_DEFINITIONS ${LLVM_DEFINITIONS_LIST})

  include(AddLLVM)
  include(HandleLLVMOptions)
  include(VersionFromVCS)
  include(GetErrcMessages)

  include(AddClang)

  include(TableGen)

  option(LLVM_INSTALL_TOOLCHAIN_ONLY
    "Only include toolchain files in the 'install' target." OFF)
  option(LLVM_FORCE_USE_OLD_TOOLCHAIN
    "Set to ON to force using an old, unsupported host toolchain." OFF)

  include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
  add_definitions(${LLVM_DEFINITIONS})

  link_directories("${LLVM_LIBRARY_DIR}")

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/lib${LLVM_LIBDIR_SUFFIX})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/lib${LLVM_LIBDIR_SUFFIX})

  set(LLVM_EXTERNAL_LIT "${LLVM_TOOLS_BINARY_DIR}/llvm-lit" CACHE STRING "Command used to spawn lit")
  set(LIT_ARGS_DEFAULT "-sv")
  if (MSVC OR XCODE)
    set(LIT_ARGS_DEFAULT "${LIT_ARGS_DEFAULT} --no-progress-bar")
  endif()
  set(LLVM_LIT_ARGS "${LIT_ARGS_DEFAULT}" CACHE STRING "Default options for lit")

  option(SWIFTC_INCLUDE_TESTS
         "Generate build targets for the Swiftc unit tests."
         ON)

  get_errc_messages(LLVM_LIT_ERRC_MESSAGES)

  set(SWIFTC_GTEST_AVAIL 0)
  if (SWIFTC_INCLUDE_TESTS)
    set(UNITTEST_DIR ${LLVM_THIRD_PARTY_DIR}/unittest)
    if(EXISTS ${UNITTEST_DIR}/googletest/include/gtest/gtest.h)
      if (NOT TARGET llvm_gtest)
        add_subdirectory(${UNITTEST_DIR} third-party/unittest)
      endif()
      set(SWIFTC_GTEST_AVAIL 1)
    else()
      message(WARNING
      "Unit-tests will be skipped as LLVM install does not include google-test related headers and libraries.")
      set(SWIFTC_GTEST_AVAIL 0)
    endif()
  endif()
  if (SWIFTC_GTEST_AVAIL)
    add_custom_target(check-all DEPENDS check-swiftc SwiftcUnitTests)
  else()
    add_custom_target(check-all DEPENDS check-swiftc)
  endif()
  if (LLVM_BUILD_DOCS)
    add_custom_target(doxygen ALL)
  endif()

else()
  option(SWIFTC_INCLUDE_TESTS
         "Generate build targets for the Swiftc unit tests."
         ${LLVM_INCLUDE_TESTS})
  set(SWIFTC_GTEST_AVAIL 1)

  if(SWIFTC_STANDALONE_BUILD)
    set(SWIFTC_BINARY_DIR ${CMAKE_BINARY_DIR}/tools/swiftc)
  else()
    set(SWIFTC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()

set(SWIFTC_TOOLS_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}" CACHE PATH
    "Path for binary subdirectory (defaults to '${CMAKE_INSTALL_BINDIR}')")
mark_as_advanced(SWIFTC_TOOLS_INSTALL_DIR)

set(SWIFTC_INCLUDE_DIR ${SWIFTC_BINARY_DIR}/include)

# TODO: Remove when libclangDriver is lifted out of Clang
if(SWIFTC_STANDALONE_BUILD)
  set(CLANG_INCLUDE_DIR ${CLANG_INCLUDE_DIRS} )
else()
  set(CLANG_INCLUDE_DIR ${LLVM_MAIN_SRC_DIR}/../clang/include )
  set(CLANG_TABLEGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tools/clang/include)
  include_directories(SYSTEM ${CLANG_TABLEGEN_OUTPUT_DIR})
endif()
include_directories(SYSTEM ${CLANG_INCLUDE_DIR})

# tco tool output directories
if(SWIFTC_STANDALONE_BUILD)
  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
endif()
set(LLVM_BUILD_TOOLS ON)

include_directories(BEFORE
  ${SWIFTC_BINARY_DIR}/include
  ${SWIFTC_SOURCE_DIR}/include)

# Add Swiftc-centric modules to cmake path.
list(INSERT CMAKE_MODULE_PATH 0
  "${SWIFTC_SOURCE_DIR}/cmake/modules"
  "${LLVM_COMMON_CMAKE_UTILS}/Modules"
  )
include(AddSwiftc)

if (NOT DEFAULT_SYSROOT)
  set(DEFAULT_SYSROOT "" CACHE PATH
    "The <path> to use for the system root for all compiler invocations (--sysroot=<path>).")
endif()

if (NOT ENABLE_LINKER_BUILD_ID)
  set(ENABLE_LINKER_BUILD_ID OFF CACHE BOOL "pass --build-id to ld")
endif()

set(SWIFTC_DEFAULT_LINKER "${CLANG_DEFAULT_LINKER}" CACHE STRING
  "Default linker to use (linker name or absolute path, empty for platform default)")

set(PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}")
if (NOT PACKAGE_VERSION)
  set(PACKAGE_VERSION ${LLVM_VERSION_MAJOR})
endif()

if (NOT DEFINED SWIFTC_VERSION_MAJOR)
  set(SWIFTC_VERSION_MAJOR ${LLVM_VERSION_MAJOR})
endif()

if (NOT DEFINED SWIFTC_VERSION_MINOR)
  set(SWIFTC_VERSION_MINOR ${LLVM_VERSION_MINOR})
endif()

if (NOT DEFINED SWIFTC_VERSION_PATCHLEVEL)
  set(SWIFTC_VERSION_PATCHLEVEL ${LLVM_VERSION_PATCH})
endif()

# Unlike PACKAGE_VERSION, SWIFTC_VERSION does not include LLVM_VERSION_SUFFIX.
set(SWIFTC_VERSION "${SWIFTC_VERSION_MAJOR}.${SWIFTC_VERSION_MINOR}.${SWIFTC_VERSION_PATCHLEVEL}")
message(STATUS "Swiftc version: ${SWIFTC_VERSION}")
# Swiftc executable version information
set(SWIFTC_EXECUTABLE_VERSION
    "${SWIFTC_VERSION_MAJOR}" CACHE STRING
    "Major version number to appended to the swiftc executable name.")
set(LIBSWIFTC_LIBRARY_VERSION
    "${SWIFTC_VERSION_MAJOR}" CACHE STRING
    "Major version number to appended to the libswiftc library.")

mark_as_advanced(SWIFTC_EXECUTABLE_VERSION LIBSWIFTC_LIBRARY_VERSION)

set(SWIFTC_VENDOR ${PACKAGE_VENDOR} CACHE STRING
  "Vendor-specific Swiftc version information.")
set(SWIFTC_VENDOR_UTI "org.llvm.swiftc" CACHE STRING
  "Vendor-specific uti.")

if (SWIFTC_VENDOR)
  add_definitions(-DSWIFTC_VENDOR="${SWIFTC_VENDOR} ")
endif()

set(SWIFTC_REPOSITORY_STRING "" CACHE STRING
  "Vendor-specific text for showing the repository the source is taken from.")
if (SWIFTC_REPOSITORY_STRING)
  add_definitions(-DSWIFTC_REPOSITORY_STRING="${SWIFTC_REPOSITORY_STRING}")
endif()

# Configure Swiftc's Version.inc file.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/swiftc/Version.inc.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/swiftc/Version.inc)
# Configure Swiftc's version info header file.
configure_file(
  ${SWIFTC_SOURCE_DIR}/include/swiftc/Config/config.h.cmake
  ${SWIFTC_BINARY_DIR}/include/swiftc/Config/config.h)

if (SWIFTC_ENABLE_WERROR)
  if( MSVC )
    append("/WX" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
  endif()
  if ( LLVM_COMPILER_IS_GCC_COMPATIBLE )
    append("-Werror" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    append("-Wno-error" CMAKE_REQUIRED_FLAGS)
  endif( LLVM_COMPILER_IS_GCC_COMPATIBLE )
endif()

# Add appropriate flags for GCC
if (LLVM_COMPILER_IS_GCC_COMPATIBLE)
  if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-semantic-interposition")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument -Wstring-conversion \
          -Wcovered-switch-default")
  endif()

  # Building shared libraries is bad for performance with GCC by default
  if (BUILD_SHARED_LIBS AND NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-semantic-interposition")
  endif()
endif()

list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS)

# Determine HOST_LINK_VERSION on Darwin.
set(HOST_LINK_VERSION)
if (APPLE)
  set(LD_V_OUTPUT)
  execute_process(
    COMMAND sh -c "${CMAKE_LINKER} -v 2>&1 | head -1"
    RESULT_VARIABLE HAD_ERROR
    OUTPUT_VARIABLE LD_V_OUTPUT)
  if (NOT HAD_ERROR)
    if ("${LD_V_OUTPUT}" MATCHES ".*ld64-([0-9.]+).*")
      string(REGEX REPLACE ".*ld64-([0-9.]+).*" "\\1" HOST_LINK_VERSION ${LD_V_OUTPUT})
    elseif ("${LD_V_OUTPUT}" MATCHES "[^0-9]*([0-9.]+).*")
      string(REGEX REPLACE "[^0-9]*([0-9.]+).*" "\\1" HOST_LINK_VERSION ${LD_V_OUTPUT})
    endif()
  else()
    message(FATAL_ERROR "${CMAKE_LINKER} failed with status ${HAD_ERROR}")
  endif()
endif()

# Set up job pools for swiftc.
set(SWIFTC_PARALLEL_COMPILE_JOBS CACHE STRING
  "The maximum number of concurrent compilation jobs for Swiftc (Ninja only)")
if (SWIFTC_PARALLEL_COMPILE_JOBS)
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS swiftc_compile_job_pool=${SWIFTC_PARALLEL_COMPILE_JOBS})
endif()

include(AddSwiftc)

if (SWIFTC_INCLUDE_TESTS)
  add_compile_definitions(SWIFTC_INCLUDE_TESTS=1)
endif()

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(cmake/modules)

option(SWIFTC_INCLUDE_TOOLS "Generate build targets for the Swiftc tools." ON)
option(SWIFTC_BUILD_TOOLS
  "Build the Swiftc tools. If OFF, just generate build targets." ON)
if (SWIFTC_INCLUDE_TOOLS)
  add_subdirectory(tools)
endif()

if (LLVM_INCLUDE_EXAMPLES)
  add_subdirectory(examples)
endif()

if (SWIFTC_INCLUDE_TESTS)
  add_subdirectory(test)
  if (SWIFTC_GTEST_AVAIL)
    add_subdirectory(unittests)
  endif ()
endif()

option(SWIFTC_INCLUDE_DOCS "Generate build targets for the Swiftc docs."
       ${LLVM_INCLUDE_DOCS})
if (SWIFTC_INCLUDE_DOCS)
  add_subdirectory(docs)
endif()

# Custom target to install Swiftc libraries.
add_custom_target(swiftc-libraries)
set_target_properties(swiftc-libraries PROPERTIES FOLDER "Swiftc/Meta")

if (NOT LLVM_ENABLE_IDE)
  add_llvm_install_targets(install-swiftc-libraries
   DEPENDS swiftc-libraries
   COMPONENT swiftc-libraries)
endif()

get_property(SWIFTC_LIBS GLOBAL PROPERTY SWIFTC_LIBS)
if (SWIFTC_LIBS)
  list(REMOVE_DUPLICATES SWIFTC_LIBS)
  foreach(lib ${SWIFTC_LIBS})
    add_dependencies(swiftc-libraries ${lib})
    if (NOT LLVM_ENABLE_IDE)
      add_dependencies(install-swiftc-libraries install-${lib})
    endif()
  endforeach()
endif()

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  add_llvm_install_targets(install-swiftc-headers
    COMPONENT swiftc-headers)

  install(DIRECTORY include/swiftc
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT swiftc-headers
    FILES_MATCHING
    PATTERN "*.def"
    PATTERN "*.h"
    PATTERN "*.inc"
    PATTERN "*.td"
    PATTERN "config.h" EXCLUDE
    PATTERN ".git"     EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE)

  install(DIRECTORY ${SWIFTC_INCLUDE_DIR}/swiftc
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT swiftc-headers
    FILES_MATCHING
    PATTERN "*.inc"
    )
endif()
