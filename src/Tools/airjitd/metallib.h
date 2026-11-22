// metallib.h - one function's bitcode and entry point inside a metallib
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef AIRJITD_METALLIB_H
#define AIRJITD_METALLIB_H

#include <stdint.h>

// one function's bitcode and entry point name inside a metallib
struct pd_metallib {
  const uint8_t *bitcode;
  uint32_t bitcode_len;
  char entry[96];
};

int pd_metallib_open(const uint8_t *mtl, uint32_t size, struct pd_metallib *lib);

#endif
