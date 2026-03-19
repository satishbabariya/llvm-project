#===-- lib/Support/CMakeLists.txt ------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===------------------------------------------------------------------------===#

find_first_existing_vc_file("${LLVM_MAIN_SRC_DIR}" llvm_vc)
find_first_existing_vc_file("${SWIFTC_SOURCE_DIR}" swiftc_vc)

# The VC revision include that we want to generate.
set(version_inc "${CMAKE_CURRENT_BINARY_DIR}/VCSVersion.inc")

set(generate_vcs_version_script "${LLVM_CMAKE_DIR}/GenerateVersionFromVCS.cmake")

if(llvm_vc AND LLVM_APPEND_VC_REV)
  set(llvm_source_dir ${LLVM_MAIN_SRC_DIR})
endif()
if(swiftc_vc AND LLVM_APPEND_VC_REV)
  set(swiftc_source_dir ${SWIFTC_SOURCE_DIR})
endif()

# Create custom target to generate the VC revision include.
add_custom_command(OUTPUT "${version_inc}"
  DEPENDS "${llvm_vc}" "${swiftc_vc}" "${generate_vcs_version_script}"
  COMMAND ${CMAKE_COMMAND} "-DNAMES=\"LLVM;SWIFTC\""
                           "-DLLVM_SOURCE_DIR=${llvm_source_dir}"
                           "-DSWIFTC_SOURCE_DIR=${swiftc_source_dir}"
                           "-DHEADER_FILE=${version_inc}"
                           "-DLLVM_FORCE_VC_REVISION=${LLVM_FORCE_VC_REVISION}"
                           "-DLLVM_FORCE_VC_REPOSITORY=${LLVM_FORCE_VC_REPOSITORY}"
                           -P "${generate_vcs_version_script}")

# Mark the generated header as being generated.
set_source_files_properties("${version_inc}"
  PROPERTIES GENERATED TRUE
             HEADER_FILE_ONLY TRUE)

if(SWIFTC_VENDOR)
  set_source_files_properties(Version.cpp
    PROPERTIES COMPILE_DEFINITIONS "SWIFTC_VENDOR=\"${SWIFTC_VENDOR} \"")
endif()

add_swiftc_library(SwiftSupport
  Version.cpp
  ${version_inc}

  LINK_COMPONENTS
  Support
)
