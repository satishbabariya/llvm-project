include(GNUInstallDirs)
include(LLVMDistributionSupport)

macro(set_swiftc_windows_version_resource_properties name)
  if (DEFINED windows_resource_file)
    set_windows_version_resource_properties(${name} ${windows_resource_file}
      VERSION_MAJOR ${SWIFTC_VERSION_MAJOR}
      VERSION_MINOR ${SWIFTC_VERSION_MINOR}
      VERSION_PATCHLEVEL ${SWIFTC_VERSION_PATCHLEVEL}
      VERSION_STRING "${SWIFTC_VERSION}"
      PRODUCT_NAME "swiftc")
  endif()
endmacro()

macro(add_swiftc_subdirectory name)
  add_llvm_subdirectory(SWIFTC TOOL ${name})
endmacro()

function(add_swiftc_library name)
  set(options SHARED STATIC INSTALL_WITH_TOOLCHAIN)
  set(multiValueArgs ADDITIONAL_HEADERS)
  cmake_parse_arguments(ARG
    "${options}"
    ""
    "${multiValueArgs}"
    ${ARGN})

  set(srcs)
  if (MSVC_IDE OR XCODE)
    # Add public headers
    file(RELATIVE_PATH lib_path
      ${SWIFTC_SOURCE_DIR}/lib/
      ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT lib_path MATCHES "^[.][.]")
      file( GLOB_RECURSE headers
        ${SWIFTC_SOURCE_DIR}/include/swiftc/${lib_path}/*.h
        ${SWIFTC_SOURCE_DIR}/include/swiftc/${lib_path}/*.def)
      set_source_files_properties(${headers} PROPERTIES HEADER_FILE_ONLY ON)

      if (headers)
        set(srcs ${headers})
      endif()
    endif()
  endif(MSVC_IDE OR XCODE)

  if (srcs OR ARG_ADDITIONAL_HEADERS)
    set(srcs
      ADDITIONAL_HEADERS
      ${srcs}
      ${ARG_ADDITIONAL_HEADERS})
  endif()

  if(ARG_SHARED AND ARG_STATIC)
    set(LIBTYPE SHARED STATIC)
  elseif(ARG_SHARED)
    set(LIBTYPE SHARED)
  elseif(ARG_STATIC)
    set(LIBTYPE STATIC)
  else()
    set(LIBTYPE)
  endif()
  llvm_add_library(${name} ${LIBTYPE} ${ARG_UNPARSED_ARGUMENTS} ${srcs})

  if (TARGET ${name})

    if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY OR ${name} STREQUAL "libswiftc"
        OR ARG_INSTALL_WITH_TOOLCHAIN)
      get_target_export_arg(${name} Swiftc export_to_swiftctargets UMBRELLA swiftc-libraries)
      install(TARGETS ${name}
        COMPONENT ${name}
        ${export_to_swiftctargets}
        LIBRARY DESTINATION lib${LLVM_LIBDIR_SUFFIX}
        ARCHIVE DESTINATION lib${LLVM_LIBDIR_SUFFIX}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

      if (NOT LLVM_ENABLE_IDE)
        add_llvm_install_targets(install-${name}
                                 DEPENDS ${name}
                                 COMPONENT ${name})
      endif()

      set_property(GLOBAL APPEND PROPERTY SWIFTC_LIBS ${name})
    endif()
    set_property(GLOBAL APPEND PROPERTY SWIFTC_EXPORTS ${name})
    if (SWIFTC_PARALLEL_COMPILE_JOBS)
      set_property(TARGET ${name} PROPERTY JOB_POOL_COMPILE swiftc_compile_job_pool)
    endif()
  else()
    # Add empty "phony" target
    add_custom_target(${name})
  endif()

  set_target_properties(${name} PROPERTIES FOLDER "Swiftc/Libraries")
  set_swiftc_windows_version_resource_properties(${name})
endfunction(add_swiftc_library)

macro(add_swiftc_executable name)
  add_llvm_executable(${name} ${ARGN})
  set_swiftc_windows_version_resource_properties(${name})
endmacro(add_swiftc_executable)

macro(add_swiftc_tool name)
  if (NOT SWIFTC_BUILD_TOOLS)
    set(EXCLUDE_FROM_ALL ON)
  endif()

  add_swiftc_executable(${name} ${ARGN})

  if (SWIFTC_BUILD_TOOLS)
    get_target_export_arg(${name} Swiftc export_to_swiftctargets)
    install(TARGETS ${name}
      ${export_to_swiftctargets}
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT ${name})

    if(NOT LLVM_ENABLE_IDE)
      add_llvm_install_targets(install-${name}
                               DEPENDS ${name}
                               COMPONENT ${name})
    endif()
    set_property(GLOBAL APPEND PROPERTY SWIFTC_EXPORTS ${name})
  endif()
endmacro()

macro(add_swiftc_symlink name dest)
  llvm_add_tool_symlink(SWIFTC ${name} ${dest} ALWAYS_GENERATE)
  # Always generate install targets
  llvm_install_symlink(SWIFTC ${name} ${dest} ALWAYS_GENERATE)
endmacro()
