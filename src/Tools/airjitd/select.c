// select.c - picks the executor for draws and dispatches the way MTLCreateSystemDefaultDevice picks a GPU
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "select.h"

#include <stdlib.h>
#include <string.h>

#ifdef AIRJITD_OPENGL
#include "opengl/gl.h"
#endif
#ifdef AIRJITD_VULKAN
#include "vulkan/vk.h"
#endif

void logmsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static int env_on(const char *name) {
  const char *e = getenv(name);

  return e && *e && strcmp(e, "0");
}

static int env_off(const char *name) {
  const char *e = getenv(name);

  return e && !strcmp(e, "0");
}

const char *pd_backend_name(enum pd_backend_kind k) {
  switch (k) {
  case PD_BACKEND_VULKAN:
    return "vulkan";
  case PD_BACKEND_OPENGL:
    return "opengl";
  case PD_BACKEND_CPU:
    return "cpu";
  default:
    return "none";
  }
}

enum pd_backend_kind pd_backend_forced(void) {
  const char *e = getenv("AIRJITD_BACKEND");

  if (e && (!strcmp(e, "vk") || !strcmp(e, "vulkan")))
    return PD_BACKEND_VULKAN;
  if (e && (!strcmp(e, "gl") || !strcmp(e, "opengl")))
    return PD_BACKEND_OPENGL;
  if (e && !strcmp(e, "cpu"))
    return PD_BACKEND_CPU;
  if (e && *e)
    logmsg("AIRJITD: AIRJITD_BACKEND=%s is not vk, gl or cpu, ignored", e);
  if (env_on("AIRJITD_OPENGL"))
    return PD_BACKEND_OPENGL;
  if (env_on("AIRJITD_VULKAN"))
    return PD_BACKEND_VULKAN;
  return PD_BACKEND_NONE;
}

static int try_vulkan(void) {
#ifdef AIRJITD_VULKAN
  if (pd_vk_available())
    return 1;
  logmsg("AIRJITD: vulkan backend unavailable: %s", pd_vk_reason());
#else
  logmsg("AIRJITD: vulkan backend not built in");
#endif
  return 0;
}

static int try_opengl(int need_hardware) {
#ifdef AIRJITD_OPENGL
  if (pd_gl_available() && (!need_hardware || pd_gl_hardware()))
    return 1;
  logmsg("AIRJITD: opengl backend unavailable: %s", pd_gl_available() ? "software renderer" : pd_gl_reason());
#else
  (void)need_hardware;
  logmsg("AIRJITD: opengl backend not built in");
#endif
  return 0;
}

enum pd_backend_kind pd_backend_choose(void) {
  static enum pd_backend_kind chosen = PD_BACKEND_NONE;
  static int choosing;
  enum pd_backend_kind f;

  // a device coming up asks again from inside the choice: no GPU executor until one is chosen
  if (chosen || choosing)
    return chosen;
  choosing = 1;
  f = pd_backend_forced();
  if (f == PD_BACKEND_VULKAN)
    chosen = try_vulkan() ? f : PD_BACKEND_CPU;
  else if (f == PD_BACKEND_OPENGL)
    chosen = try_opengl(0) ? f : PD_BACKEND_CPU;
  else if (f == PD_BACKEND_CPU)
    chosen = f;
  else if (!env_off("AIRJITD_VULKAN") && try_vulkan())
    chosen = PD_BACKEND_VULKAN;
  else if (!env_off("AIRJITD_OPENGL") && try_opengl(1))
    chosen = PD_BACKEND_OPENGL;
  else
    chosen = PD_BACKEND_CPU;
  choosing = 0;
  logmsg("AIRJITD: %s backend%s", pd_backend_name(chosen), f && f != chosen ? " (the forced one did not come up)" : "");
  return chosen;
}
