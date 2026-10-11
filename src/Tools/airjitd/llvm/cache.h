#ifndef AIRJITD_LLVM_CACHE_H
#define AIRJITD_LLVM_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "../metallib.h"
#include "../stage.h"

// compiled stages on disk: the object file LLVM made and the stage's StageInfo, keyed by this
// build of the daemon and the function's bitcode, so a later boot loads them and skips compiling
#define PD_CACHE_KEY 65

// the cpu and feature string the JIT compiles for (backend.c), part of the key
void pd_jit_host(const char **cpu, const char **features);

// the key for lib's function, 0 when there is a cache to use
int pd_cache_key(const struct pd_metallib *lib, char key[PD_CACHE_KEY]);

// 0 with the stage's info (entry unset) and its object in a malloc'd buffer
int pd_cache_load(const char *key, struct StageInfo *si, uint8_t **obj, size_t *len);

void pd_cache_store(const char *key, const struct StageInfo *si, const uint8_t *obj, size_t len);

#endif
