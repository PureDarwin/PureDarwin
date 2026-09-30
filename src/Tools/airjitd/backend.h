#ifndef AIRJITD_BACKEND_H
#define AIRJITD_BACKEND_H

#include <stdint.h>

#include "metallib.h"
#include "stage.h"

// the first-pixels compute path: fn(data, index) once per element
typedef void (*pd_legacy_entry)(uint32_t *data, uint32_t index);

// everything that turns AIR into something runnable. The rest of the daemon only calls these
struct pd_backend {
  const char *name;
  void (*init)(void);
  // fills si: parameter bindings, result layout and entry
  int (*compile_stage)(uint32_t id, const struct pd_metallib *lib, struct StageInfo *si);
  // logs the function's type, the symbols it needs and its metadata
  int (*introspect)(uint32_t id, const struct pd_metallib *lib);
  int (*compile_legacy)(uint32_t id, const struct pd_metallib *lib, pd_legacy_entry *entry);
};

extern const struct pd_backend pd_llvm_backend;

#endif
