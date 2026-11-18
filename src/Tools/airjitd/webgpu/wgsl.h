// wgsl.h - SPIR-V to WGSL with Tint, the shader step between air2spv and WebGPU
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef AIRJITD_WGSL_H
#define AIRJITD_WGSL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// WGSL for the SPIR-V words, malloc'd and NUL-terminated, or 0 with the reason in err
char *pd_spv_to_wgsl(const uint32_t *words, size_t nwords, char *err, size_t errlen);
#ifdef __cplusplus
}
#endif
#endif
