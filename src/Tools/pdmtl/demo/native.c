/* SPDX-License-Identifier: MIT */
/* Metal shaders on Mesa, presented through the native Radeon framebuffer. */
#include "pdmtl.h"
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;
static void stop(int signo) { (void)signo; stopped = 1; }
static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  const char *path = "/usr/local/share/metal-pd/demo-xcode26.metallib";
  unsigned width = 960, height = 600;
  double duration = 30;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-l") && i + 1 < argc) path = argv[++i];
    else if (!strcmp(argv[i], "-s") && i + 1 < argc) duration = atof(argv[++i]);
    else if (!strcmp(argv[i], "-w") && i + 1 < argc) width = (unsigned)atoi(argv[++i]);
    else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = (unsigned)atoi(argv[++i]);
    else return 2;
  }
  if (!width || !height || width > 4096 || height > 4096 || duration <= 0) return 2;
  setenv("AIRJITD_OPENGL", "1", 1);
  unsetenv("AIRJITD_VULKAN");
  signal(SIGINT, stop); signal(SIGTERM, stop);
  io_iterator_t it = 0;
  io_service_t service;
  io_connect_t display = 0;
  if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("PDRadeonFramebuffer"), &it)) return 3;
  while ((service = IOIteratorNext(it))) {
    kern_return_t rc = IOServiceOpen(service, mach_task_self(), 0, &display);
    IOObjectRelease(service);
    if (!rc && display) break;
  }
  IOObjectRelease(it);
  if (!display) { fprintf(stderr, "metal-native: cannot own the Radeon display\n"); return 3; }
  int result = 4;
  if (pdmtl_init(0)) goto done;
  pdmtl_resident(1);
  struct pdmtl_lib *lib = pdmtl_lib_load(path);
  if (!lib) goto done;
  struct pdmtl_fn *vb = pdmtl_fn_get(lib, "v_bg"), *fb = pdmtl_fn_get(lib, "f_plasma");
  struct pdmtl_fn *vs = pdmtl_fn_get(lib, "v_star"), *fs = pdmtl_fn_get(lib, "f_star");
  if (!vb || !fb || !vs || !fs) goto done;
  struct { float time, aspect, mouse[2]; } u = {0, (float)width / height, {0, 0}};
  struct pdmtl_buf uniforms = pdmtl_new_buffer(&u, sizeof(u));
  struct pdmtl_tex target = pdmtl_new_texture(width, height, 0); /* BGRA8 */
  if (!uniforms.contents || !target.contents) goto done;
  struct pdmtl_draw_desc bg = { vb, fb, 4, 4, 0, 0, &uniforms, 0, 1, 0 };
  struct pdmtl_draw_desc stars = { vs, fs, 3, 30, 3, &uniforms, 0, 1, 0, 1 };
  double start = now(), report = start;
  unsigned frames = 0;
  while (!stopped && now() - start < duration) {
    u.time = (float)(now() - start);
    memcpy(uniforms.contents, &u, sizeof(u));
    if (pdmtl_draw_ex(&bg, &target) || !pdmtl_last_on_opengl() ||
        pdmtl_draw_ex(&stars, &target) || !pdmtl_last_on_opengl() || pdmtl_present_native(&target)) {
      fprintf(stderr, "metal-native: hardware draw or native presentation failed\n"); goto done;
    }
    frames++;
    double t = now();
    if (t - report >= 5) {
      printf("metal-native: %u GPU frames presented, %.1f FPS\n", frames, frames / (t - start));
      report = t;
    }
    double left = start + frames / 60.0 - t;
    if (left > 0) usleep((useconds_t)(left * 1e6));
  }
  printf("metal-native: PASS %u hardware frames, %.1f FPS\n", frames, frames / (now() - start));
  result = 0;
done:
  IOServiceClose(display);
  return result;
}
