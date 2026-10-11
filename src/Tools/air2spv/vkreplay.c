// vkreplay CAPTURE.bin REF.bin: a captured job (airjitdcap=) on airjitd's Vulkan executor, each output region
// compared with REF (ajreplay's CPU output) on one line; exit 0 within tolerance
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../airjitd/bridge.h"
#include "../airjitd/vulkan/vk.h"

struct cap_header {
  uint32_t magic, kind, serial, reqBytes;
};

struct cap_region {
  uint32_t off, len, after, pad;
};

static uint8_t *base, *before;
static struct cap_region outs[64], refs[64];
static uint8_t *ref_data[64];
static unsigned nout, nref;
static int verbose;

// the executor logs through the daemon's logger
int pd_log_verbose;

void logmsg(const char *fmt, ...) {
  va_list ap;

  if (!verbose)
    return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

void logv(const char *fmt, ...) {
  va_list ap;

  if (!verbose)
    return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

// pixel formats as the kext numbers them, see airjitd/texture.h
static const struct {
  unsigned bpp, ch;
  char kind;
} fmts[] = {
  { 4, 4, 'n' },      { 8, 4, 'h' },
  { 1, 1, 'n' },            { 1, 1, 'n' },
  { 4, 4, 'n' },      { 2, 2, 'n' },
  { 4, 2, 'u' },         { 4, 1, 'u' },
  { 8, 2, 'u' },         { 8, 4, 'u' },
  { 2, 1, 'u' },            { 4, 4, 'u' },
  { 2, 1, 'h' },          { 4, 2, 'h' },
  { 4, 1, 'f' },          { 8, 2, 'f' },
  { 16, 4, 'f' }, { 4, 4, 'n' },
  { 4, 4, 'n' },       { 4, 4, 'x' },
  { 4, 4, 'x' }, { 2, 1, 'n' },
  { 4, 2, 'n' },        { 8, 4, 'n' },
};

static float half_f(uint16_t h) {
  uint32_t s = (uint32_t)(h >> 15) << 31, e = (h >> 10) & 31, m = h & 0x3ff, b;
  float f;

  if (e == 0) {
    f = ldexpf((float)m, -24);
    return s ? -f : f;
  }
  b = s | (e == 31 ? 0x7f800000u | m << 13 : ((e + 112) << 23) | m << 13);
  memcpy(&f, &b, 4);
  return f;
}

// one channel of a texel as a number: unorm scaled to 0..1, float as is, integer as its value
static double channel(unsigned fmt, const uint8_t *p, unsigned c) {
  unsigned bpc;

  switch (fmts[fmt].kind) {
  case 'h':
    return half_f((uint16_t)(p[c * 2] | p[c * 2 + 1] << 8));
  case 'f': {
    float f;
    memcpy(&f, p + c * 4, 4);
    return f;
  }
  case 'x': {
    uint32_t v = p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
    return c == 3 ? (double)(v >> 30) / 3.0 : (double)((v >> (c * 10)) & 1023) / 1023.0;
  }
  default:
    bpc = fmts[fmt].bpp / fmts[fmt].ch;
    if (bpc == 1)
      return fmts[fmt].kind == 'u' ? p[c] : p[c] / 255.0;
    if (bpc == 2) {
      unsigned v = p[c * 2] | p[c * 2 + 1] << 8;
      return fmts[fmt].kind == 'u' ? v : v / 65535.0;
    }
    return (double)(p[c * 4] | p[c * 4 + 1] << 8 | p[c * 4 + 2] << 16 | (uint32_t)p[c * 4 + 3] << 24);
  }
}

// the job's textures by offset, from the request: compute and tile tex[] and rt[], draw target, ftex[] and rt[]
static struct PDDrawTex texs[64];
static unsigned ntexs;

static void add_texs(const struct PDDrawTex *t, unsigned n) {
  for (unsigned k = 0; k < n && ntexs < 64; k++) {
    if (t[k].w)
      texs[ntexs++] = t[k];
  }
}

static int region_fmt(uint32_t off, uint32_t *w, uint32_t *bpr) {
  for (unsigned i = 0; i < ntexs; i++) {
    if (texs[i].off == off && texs[i].fmt < sizeof(fmts) / sizeof(fmts[0])) {
      *w = texs[i].w, *bpr = texs[i].bpr;
      return (int)texs[i].fmt;
    }
  }
  return -1;
}

// 0 within tolerance, -1 only a few stray pixels off, -2 byte exact, otherwise the regions off
static int compare(void) {
  int bad = 0, near = 0, exact = 1;

  for (unsigned k = 0; k < nref; k++) {
    uint32_t off = refs[k].off, len = refs[k].len, w = 0, bpr = 0;
    int fmt = region_fmt(off, &w, &bpr);
    const uint8_t *got = base + off, *want = ref_data[k];
    double maxe = 0, sum = 0;
    unsigned long n = 0, over = 0, diffbytes = 0;
    double tol;

    unsigned long changed = 0;

    for (uint32_t i = 0; i < len; i++) {
      diffbytes += got[i] != want[i];
      changed += before[off + i] != want[i];
    }
    printf("  (the CPU job changed %lu of %u bytes here)\n", changed, len);

    if (fmt < 0) {
      // a buffer: compared as floats when float-like, bytes otherwise
      for (uint32_t i = 0; i + 4 <= len; i += 4) {
        float a, b;
        memcpy(&a, got + i, 4), memcpy(&b, want + i, 4);
        double e = fabs((double)a - (double)b);
        if (e == e && e > maxe)
          maxe = e;
        sum += e == e ? e : 0, n++;
      }
      tol = 1e-3;
      over = maxe > tol;
      printf("  buffer +0x%x len %u: %lu bytes differ, max %.3g mean %.3g\n", off, len, diffbytes, maxe, n ? sum / n : 0);
    } else {
      unsigned bpp = fmts[fmt].bpp, ch = fmts[fmt].ch;
      char kind = fmts[fmt].kind;

      tol = kind == 'u' ? 0 : kind == 'h' ? 4e-3 : 2.5 / 255.0;
      for (uint32_t y = 0; bpr && y < len / bpr; y++) {
        for (uint32_t x = 0; x < w; x++) {
          const uint8_t *pg = got + y * bpr + x * bpp, *pw = want + y * bpr + x * bpp;
          double pmax = 0;
          for (unsigned c = 0; c < ch; c++) {
            double a = channel((unsigned)fmt, pg, c), b = channel((unsigned)fmt, pw, c), e = fabs(a - b);
            // half results compare relative once above one
            if (kind == 'h' && fabs(b) > 1)
              e /= fabs(b);
            if (e != e)
              e = (a != a && b != b) ? 0 : 1;
            if (e > pmax)
              pmax = e;
            sum += e, n++;
          }
          if (pmax > maxe)
            maxe = pmax;
          if (verbose && pmax > tol && over < 8) {
            printf("    px %u,%u got", x, y);
            for (unsigned c = 0; c < ch; c++) {
              printf(" %g", channel((unsigned)fmt, pg, c));
            }
            printf(" want");
            for (unsigned c = 0; c < ch; c++) {
              printf(" %g", channel((unsigned)fmt, pw, c));
            }
            printf("\n");
          }
          over += pmax > tol;
        }
      }
      printf("  texture +0x%x fmt %d %ux%u: max %.4g mean %.3g, %lu px over %.4g (%lu bytes differ)\n", off, fmt, w,
             bpr ? len / bpr : 0, maxe, n ? sum / n : 0, over, tol, diffbytes);
    }

    // a handful of stray pixels (edges, NaN inputs) is "near", more is a mismatch
    if (over) {
      unsigned long px = fmt >= 0 && bpr ? (unsigned long)w * (len / bpr) : len / 4;
      if (fmt >= 0 && over <= 16 + px / 1000)
        near++;
      else
        bad++;
    }
    exact &= !diffbytes;
  }

  if (!bad && near)
    return -1;
  if (!bad && exact)
    return -2;

  return bad;
}

static uint8_t *load_cap(const char *path, struct cap_header *h, uint8_t *mem, struct cap_region *after,
                         uint8_t **after_data, unsigned *nafter) {
  FILE *f = fopen(path, "rb");
  struct cap_region rg;
  // the largest request (a draw with its extension) is several KB and grows with the bridge protocol
  static uint8_t req[65536];

  if (!f || fread(h, sizeof(*h), 1, f) != 1 || h->magic != 0x31434a41u || h->reqBytes > sizeof(req)) {
    fprintf(stderr, "vkreplay: not a capture: %s\n", path);
    exit(2);
  }
  if (fread(req, 1, h->reqBytes, f) != h->reqBytes)
    exit(2);
  while (fread(&rg, sizeof(rg), 1, f) == 1) {
    uint8_t *d;
    if ((uint64_t)rg.off + rg.len > PD_BRIDGE_BYTES)
      exit(2);
    if (rg.after) {
      d = malloc(rg.len ? rg.len : 1);
      if (fread(d, 1, rg.len, f) != rg.len)
        exit(2);
      if (*nafter < 64)
        after[*nafter] = rg, after_data[(*nafter)++] = d;
      else
        free(d);
    } else {
      if (mem && fread(mem + rg.off, 1, rg.len, f) != rg.len)
        exit(2);
      if (!mem)
        fseek(f, rg.len, SEEK_CUR);
    }
  }
  fclose(f);
  return req;
}

int main(int argc, char **argv) {
  struct cap_header h, hr;
  uint8_t *req, *out_data[64];
  int rc = 0;

  if (argc < 3) {
    fprintf(stderr, "usage: vkreplay CAPTURE.bin REF.bin\n");
    return 2;
  }
  verbose = getenv("VKV") != 0;
  setenv("AIRJITD_VULKAN", "1", 0);
  base = calloc(1, PD_BRIDGE_BYTES);
  req = load_cap(argv[1], &h, base, outs, out_data, &nout);
  memcpy(base + PD_BRIDGE_REQ, req, h.reqBytes);
  load_cap(argv[2], &hr, 0, refs, ref_data, &nref);
  before = malloc(PD_BRIDGE_BYTES);
  memcpy(before, base, PD_BRIDGE_BYTES);

  if (h.kind == PD_JOB_COMPUTE) {
    struct PDComputeRequest *q = (struct PDComputeRequest *)(base + PD_BRIDGE_REQ);
    printf("compute %u\n", h.serial);
    add_texs(q->tex, PD_DRAW_TEXS);
    rc = pd_vk_compute(q, base, PD_BRIDGE_BYTES);
  } else if (h.kind == PD_JOB_DRAW) {
    struct PDDrawRequest *q = (struct PDDrawRequest *)(base + PD_BRIDGE_REQ);

    // VKREPLAY_FBUF=DIR: each bound fragment buffer's bytes as DIR/fbufN.bin, for air2spv -P
    if (getenv("VKREPLAY_FBUF")) {
      for (unsigned i = 0; i < PD_DRAW_BUFS; i++) {
        char path[512];
        FILE *f;

        if (!q->fbuf[i].size)
          continue;
        snprintf(path, sizeof(path), "%s/fbuf%u.bin", getenv("VKREPLAY_FBUF"), i);
        if ((f = fopen(path, "wb"))) {
          fwrite(base + q->fbuf[i].off, 1, q->fbuf[i].size, f);
          fclose(f);
        }
      }
    }
    struct PDDrawTex t = { 0, q->targetOff, q->tw, q->th, q->tbpr, q->tfmt };
    printf("draw %u\n", h.serial);
    add_texs(&t, 1), add_texs(q->rt, 4), add_texs(q->ftex, PD_DRAW_TEXS);
    rc = pd_vk_draw(q, base, PD_BRIDGE_BYTES);
  } else if (h.kind == PD_JOB_TILE) {
    struct PDTileRequest *q = (struct PDTileRequest *)(base + PD_BRIDGE_REQ);
    printf("tile %u\n", h.serial);
    add_texs(q->rt, 4), add_texs(q->tex, PD_DRAW_TEXS);
    rc = pd_vk_tile(q, base, PD_BRIDGE_BYTES);
  }

  if (rc == PD_VK_FALLBACK) {
    printf("FALLBACK %s\nRESULT cpu\n", pd_vk_reason());
    return 1;
  }
  if (rc) {
    printf("RESULT fail %d\n", rc);
    return 1;
  }
  printf("vkstats translate %llu pipeline %llu upload %llu run %llu readback %llu us\n",
         (unsigned long long)pd_vk_last.translate, (unsigned long long)pd_vk_last.pipeline,
         (unsigned long long)pd_vk_last.upload, (unsigned long long)pd_vk_last.run,
         (unsigned long long)pd_vk_last.readback);

  rc = compare();
  printf("RESULT %s\n", rc == -2 ? "exact" : rc == -1 ? "near" : rc ? "mismatch" : "match");
  return rc > 0 ? 1 : 0;
}
