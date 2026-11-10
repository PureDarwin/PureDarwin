// pdmtl-shot.c - renders and compares Metal executor output
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// pdmtl-shot LIB OUT.ppm [W H TIME] renders one offscreen frame
// -c compares PPMs, -k runs a named compute kernel
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdmtl.h"
#include "bridge.h"

struct uniforms {
  float time, aspect, mouse[2];
};

static unsigned char *read_ppm(const char *path, int *w, int *h) {
  FILE *f = fopen(path, "rb");
  unsigned char *p;

  if (!f || fscanf(f, "P6 %d %d 255", w, h) != 2 || fgetc(f) < 0) {
    if (f)
      fclose(f);
    return 0;
  }
  p = malloc((size_t)*w * *h * 3);
  if (p && fread(p, 3, (size_t)*w * *h, f) != (size_t)*w * *h)
    free(p), p = 0;
  fclose(f);
  return p;
}

// max and mean channel difference, and the pixels off by more than 2
static int compare(const char *a, const char *b) {
  int wa, ha, wb, hb;
  unsigned char *pa = read_ppm(a, &wa, &ha), *pb = read_ppm(b, &wb, &hb);
  size_t n, off = 0, i;
  unsigned max = 0;
  double sum = 0;

  if (!pa || !pb || wa != wb || ha != hb) {
    printf("COMPARE %s %s: unreadable or different sizes\n", a, b);
    return 2;
  }
  n = (size_t)wa * ha;
  for (i = 0; i < n; i++) {
    unsigned px = 0;

    for (int c = 0; c < 3; c++) {
      unsigned d = (unsigned)abs(pa[i * 3 + c] - pb[i * 3 + c]);

      px = d > px ? d : px;
      sum += d;
    }
    max = px > max ? px : max;
    off += px > 2;
  }
  printf("COMPARE %dx%d max %u mean %.4f pixels-over-2 %zu (%.3f%%)\n", wa, ha, max, sum / (n * 3.0), off,
         100.0 * off / n);
  return off * 1000 > n;
}

// prints out[i] for each thread, one per line, for diffing between backends
static int kernel(const char *path, const char *name, unsigned n) {
  struct pdmtl_lib *lib;
  struct pdmtl_fn *fn;
  struct pdmtl_buf out;
  uint64_t grid[3] = { n, 1, 1 }, tpg[3] = { 64, 1, 1 };
  int rc;

  if (pdmtl_init(0) || !(lib = pdmtl_lib_load(path)) || !(fn = pdmtl_fn_get(lib, name))) {
    fprintf(stderr, "cannot load %s from %s\n", name, path);
    return 4;
  }
  out = pdmtl_new_buffer(0, n * sizeof(float));
  memset(out.contents, 0xff, n * sizeof(float));
  if ((rc = pdmtl_dispatch_threads(fn, &out, 1, grid, tpg))) {
    fprintf(stderr, "dispatch failed: %d\n", rc);
    return 5;
  }
  pdmtl_finish();
  pdmtl_sync(out.off, out.size);
  printf("KERNEL executor %s\n", pdmtl_last_on_vulkan() ? "vulkan" : pdmtl_last_on_opengl() ? "opengl" : "cpu");
  for (unsigned i = 0; i < n; i++) {
    printf("%u %.6g\n", i, ((float *)out.contents)[i]);
  }
  return 0;
}

// managed storage: add_arrays must see only the range pdmtl_modified named
static int managed_check(const char *path, const char *name) {
  struct pdmtl_lib *lib;
  struct pdmtl_fn *fn;
  struct pdmtl_buf b[3];
  const unsigned n = 128, len = n * 65536 * 4;
  uint64_t grid[3] = { n, 1, 1 }, tpg[3] = { 64, 1, 1 };
  float *a, *in, *out;
  int bad = 0;

  if (pdmtl_init((size_t)256 << 20) || !(lib = pdmtl_lib_load(path)) || !(fn = pdmtl_fn_get(lib, name)))
    return 4;
  pdmtl_resident(1);
  b[0] = pdmtl_new_buffer(0, len), b[1] = pdmtl_new_buffer(0, len), b[2] = pdmtl_new_buffer(0, n * 4);
  a = b[0].contents, in = b[1].contents, out = b[2].contents;
  memset(a, 0, len), memset(in, 0, len), memset(out, 0, n * sizeof(*out));
  pdmtl_managed(b[0].off, len, 1);
  for (unsigned i = 0; i < n; i++) {
    a[i] = (float)i, in[i] = 10;
  }
  pdmtl_modified(b[0].off, len), pdmtl_modified(b[1].off, len);
  for (int step = 0; step < 3; step++) {
    if (step == 1)
      a[7] = 1000;
    if (step == 2)
      pdmtl_modified(b[0].off + 7 * 4, 4);
    if (pdmtl_dispatch_threads(fn, b, 3, grid, tpg))
      return 5;
    pdmtl_finish();
    pdmtl_sync(b[2].off, b[2].size);
    for (unsigned i = 0; i < n; i++) {
      float want = (i == 7 && step == 2 ? 1000 : (float)i) + 10;

      if (out[i] != want && !(i == 7 && step == 1 && out[i] == 1010 && !pdmtl_last_on_vulkan()))
        bad++, printf("step %d out[%u] %g want %g\n", step, i, out[i], want);
    }
  }
  printf("MANAGED %s on %s\n", bad ? "FAIL" : "ok", pdmtl_last_on_vulkan() ? "vulkan" : "cpu");
  return bad != 0;
}

int main(int argc, char **argv) {
  int w = 320, h = 200;
  float t = 1.25f;
  struct pdmtl_lib *lib;
  struct pdmtl_fn *vbg, *fbg, *vst, *fst;
  struct uniforms u;
  struct pdmtl_buf ub;
  struct pdmtl_tex tex;
  struct PDDrawTex dt;
  float black[4] = { 0, 0, 0, 1 };
  FILE *f;

  if (argc == 4 && !strcmp(argv[1], "-c"))
    return compare(argv[2], argv[3]);
  if (argc == 4 && !strcmp(argv[1], "-m"))
    return managed_check(argv[2], argv[3]);
  if (argc == 5 && !strcmp(argv[1], "-k"))
    return kernel(argv[2], argv[3], (unsigned)atoi(argv[4]));
  if (argc < 3) {
    fprintf(stderr, "usage: pdmtl-shot LIB OUT.ppm [W H TIME] | -c A.ppm B.ppm\n");
    return 2;
  }
  if (argc >= 6)
    w = atoi(argv[3]), h = atoi(argv[4]), t = (float)atof(argv[5]);
  if (pdmtl_init(0))
    return 2;
  if (!(lib = pdmtl_lib_load(argv[1])) || !(vbg = pdmtl_fn_get(lib, "v_bg")) || !(fbg = pdmtl_fn_get(lib, "f_plasma")) ||
      !(vst = pdmtl_fn_get(lib, "v_star")) || !(fst = pdmtl_fn_get(lib, "f_star"))) {
    fprintf(stderr, "cannot load the demo functions from %s\n", argv[1]);
    return 4;
  }
  u = (struct uniforms){ t, (float)w / (float)h, { 0.25f, -0.1f } };
  ub = pdmtl_new_buffer(&u, sizeof(u));
  tex = pdmtl_new_texture((uint32_t)w, (uint32_t)h, 0);
  dt = (struct PDDrawTex){ 0, tex.off, tex.w, tex.h, tex.bpr, tex.fmt };
  pdmtl_fill(&dt, black);
  {
    struct pdmtl_draw_desc bg = { vbg, fbg, 4, 4, 0, 0, &ub, 0, 1, 0 };
    struct pdmtl_draw_desc st = { vst, fst, 3, 30, 3, &ub, 0, 1, 0, 1 };
    int rc;

    if ((rc = pdmtl_draw_ex(&bg, &tex)) || (rc = pdmtl_draw_ex(&st, &tex))) {
      fprintf(stderr, "draw failed: %d\n", rc);
      return 5;
    }
    printf("SHOT executor %s\n", pdmtl_last_on_vulkan() ? "vulkan" : pdmtl_last_on_opengl() ? "opengl" : "cpu");
  }
  pdmtl_finish();
  pdmtl_sync(tex.off, tex.bpr * tex.h);
  if (!(f = fopen(argv[2], "wb")))
    return 6;
  fprintf(f, "P6 %d %d 255\n", w, h);
  for (int y = 0; y < h; y++) {
    const unsigned char *row = (const unsigned char *)tex.contents + (size_t)y * tex.bpr;

    // BGRA8 to RGB
    for (int x = 0; x < w; x++) {
      unsigned char px[3] = { row[x * 4 + 2], row[x * 4 + 1], row[x * 4] };
      fwrite(px, 1, 3, f);
    }
  }
  fclose(f);
  return 0;
}
