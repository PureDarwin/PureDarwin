// demo [-l LIB] [-s SECONDS] [-w W -h H]: an Apple-compiled metallib drawn into a pdmtl window, Esc or q
// quits. AIRJITD_VULKAN=1 runs the Vulkan executor, PDMTL_PRESENT=copy copies instead of drawing in place
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pdmtl_window.h"

struct uniforms {
  float time, aspect, mouse[2];
};

static double now_s(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  const char *libpath = "demo.metallib";
  double seconds = 0, t0, tlast, render = 0, present = 0;
  int w = 960, h = 600, frames = 0, total = 0, quit = 0;
  struct pdmtl_window *win;
  struct pdmtl_lib *lib;
  struct pdmtl_fn *vbg, *fbg, *vst, *fst;
  struct uniforms u = { 0, 1, { 0, 0 } };
  struct pdmtl_buf ub;
  const char *mode;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-l") && i + 1 < argc)
      libpath = argv[++i];
    else if (!strcmp(argv[i], "-s") && i + 1 < argc)
      seconds = atof(argv[++i]);
    else if (!strcmp(argv[i], "-w") && i + 1 < argc)
      w = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-h") && i + 1 < argc)
      h = atoi(argv[++i]);
  }
  if (pdmtl_init(0))
    return 2;
  // the window first: the executor's Vulkan device is created with the window's extensions
#ifdef __APPLE__
  if (!(win = pdmtl_window_create(w, h, "pdmtl: Metal on PureDarwin")))
#else
  if (!(win = pdmtl_window_create(w, h, "pdmtl: Metal on Linux")))
#endif
    return 3;
  // resources stay on the executor's device between frames (PDMTL_RESIDENT=0: the per-job path)
  if (!getenv("PDMTL_RESIDENT") || strcmp(getenv("PDMTL_RESIDENT"), "0"))
    pdmtl_resident(1);
  if (!(lib = pdmtl_lib_load(libpath)) || !(vbg = pdmtl_fn_get(lib, "v_bg")) || !(fbg = pdmtl_fn_get(lib, "f_plasma")) ||
      !(vst = pdmtl_fn_get(lib, "v_star")) || !(fst = pdmtl_fn_get(lib, "f_star"))) {
    fprintf(stderr, "cannot load the demo functions from %s\n", libpath);
    return 4;
  }
  ub = pdmtl_new_buffer(&u, sizeof(u));
  mode = !getenv("AIRJITD_VULKAN") ? "cpu" : pdmtl_window_zero_copy(win) ? "vulkan-zero-copy" : "vulkan-copy";
  printf("demo: %s, %s\n", mode, libpath);

  t0 = tlast = now_s();
  while (!quit) {
    struct pdmtl_event ev;
    struct pdmtl_drawable *d;
    double a, b, c;

    while (pdmtl_poll_event(win, &ev)) {
      if (ev.type == PDMTL_EV_CLOSE || (ev.type == PDMTL_EV_KEY && ev.action == 1 && (ev.key == 256 || ev.key == 'Q')))
        quit = 1;
      if (ev.type == PDMTL_EV_MOUSE_MOVE && w > 0 && h > 0) {
        u.mouse[0] = (float)(ev.x / w * 2 - 1) * ((float)w / h);
        u.mouse[1] = (float)(1 - ev.y / h * 2);
      }
      if (ev.type == PDMTL_EV_RESIZE)
        w = ev.w, h = ev.h;
    }
    if (quit || !(d = pdmtl_next_drawable(win)))
      break;

    a = now_s();
    u.time = (float)(a - t0);
    u.aspect = (float)d->texture.w / (float)d->texture.h;
    memcpy(ub.contents, &u, sizeof(u));
    {
      struct pdmtl_draw_desc bg = { vbg, fbg, 4, 4, 0, 0, &ub, 0, 1, 0 };
      struct pdmtl_draw_desc st = { vst, fst, 3, 30, 3, &ub, 0, 1, 0, 1 };
      int rc;

      pdmtl_clear(d, 0, 0, 0, 1);
      if ((rc = pdmtl_draw_ex(&bg, &d->texture)) || (rc = pdmtl_draw_ex(&st, &d->texture))) {
        fprintf(stderr, "draw failed: %d\n", rc);
        return 5;
      }
    }
    b = now_s();
    pdmtl_present(d);
    c = now_s();
    render += b - a, present += c - b;
    frames++, total++;

    if (c - tlast >= 2.0) {
      printf("demo: %s %ux%u: %.1f fps, render %.2f ms, present %.2f ms per frame\n", mode, d->texture.w, d->texture.h,
             frames / (c - tlast), render / frames * 1e3, present / frames * 1e3);
      fflush(stdout);
      frames = 0, render = present = 0, tlast = c;
    }
    if (seconds && c - t0 >= seconds)
      break;
  }
  printf("demo: %d frames in %.1f s\n", total, now_s() - t0);
  pdmtl_window_destroy(win);
  return 0;
}
