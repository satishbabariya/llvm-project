! Test that the driver prints help information.

! RUN: %swiftc --help 2>&1 | FileCheck %s
! CHECK: OVERVIEW: swiftc LLVM compiler
