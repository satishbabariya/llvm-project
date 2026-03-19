//===-- include/swiftc/Config/config.h.cmake ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/* This generated file is for internal use. Do not include it from headers. */

#ifdef SWIFTC_CONFIG_H
#error config.h can only be included once
#else
#define SWIFTC_CONFIG_H

#define SWIFTC_VERSION            "${SWIFTC_VERSION}"

#define SWIFTC_DEFAULT_LINKER     "${SWIFTC_DEFAULT_LINKER}"

#endif
