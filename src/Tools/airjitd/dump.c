// airjitddump=ID[,ID...]: a job running one of the IDs dumps its target, textures and attachments to
// /private/var/tmp as PPMs. airjitddumpafter=SECONDS waits that long after boot
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "airjitd.h"
#include "bridge.h"
#include "dump.h"
#include "metallib.h"
#include "stage.h"
#include "texture.h"

#define PD_DUMP_IDS 16
#define PD_DUMP_EACH 4

#define PD_DUMP_KIND_EACH 2
#define PD_DUMP_FILES 200
#define PD_DUMP_FNS 4096

static uint32_t dump_ids[PD_DUMP_IDS], dump_done[PD_DUMP_IDS];
static int dump_n = -1, dump_compute, dump_tile, dump_files;
static long dump_after;
// per-kernel counts for the compute and tile keywords
static uint8_t kind_done[PD_DUMP_FNS];

static void dump_parse(void) {
  char args[1024], *p;
  size_t len = sizeof(args) - 1;

  dump_n = 0;
  if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
    return;
  args[len] = 0;
  if ((p = strstr(args, "airjitddumpafter=")) != NULL)
    dump_after = strtol(p + strlen("airjitddumpafter="), NULL, 10);
  if ((p = strstr(args, "airjitddump=")) == NULL)
    return;
  p += strlen("airjitddump=");
  // numbers name kernel or stage ids, "compute" and "tile" every job of that kind
  while (*p && *p != ' ') {
    if (strncmp(p, "compute", 7) == 0) {
      dump_compute = 1;
      p += 7;
    } else if (strncmp(p, "tile", 4) == 0) {
      dump_tile = 1;
      p += 4;
    } else if (*p >= '0' && *p <= '9' && dump_n < PD_DUMP_IDS) {
      dump_ids[dump_n++] = (uint32_t)strtoul(p, &p, 10);
    } else {
      break;
    }
    if (*p == ',')
      p++;
  }
}

// seconds since boot have reached airjitddumpafter
static int dump_due(void) {
  struct timeval bt;
  size_t len = sizeof(bt);

  if (dump_after <= 0)
    return 1;
  if (sysctlbyname("kern.boottime", &bt, &len, NULL, 0) != 0)
    return 1;
  return time(NULL) - bt.tv_sec >= dump_after;
}

// a compute or tile job of a kind the keywords name, while that kernel has dumps left
static int dump_kind(int on, uint32_t fn) {
  if (dump_n < 0)
    dump_parse();
  if (!on || fn >= PD_DUMP_FNS || kind_done[fn] >= PD_DUMP_KIND_EACH || !dump_due())
    return 0;
  kind_done[fn]++;
  return 1;
}

// the slot for this ID while it still has dumps left, -1 otherwise
static int dump_slot(uint32_t id) {
  if (dump_n < 0)
    dump_parse();
  for (int i = 0; i < dump_n; i++) {
    if (dump_ids[i] == id)
      return dump_done[i] < PD_DUMP_EACH && dump_due() ? i : -1;
  }
  return -1;
}

static void dump_one(const char *what, uint32_t serial, unsigned k, uint8_t *base, uint32_t bsize,
                     uint32_t off, uint32_t w, uint32_t h, uint32_t bpr, unsigned fmt) {
  char path[128];
  FILE *f;

  if (w == 0 || h == 0 || bpr == 0 || off >= bsize || (uint64_t)bpr * h > bsize - off ||
      dump_files >= PD_DUMP_FILES)
    return;
  dump_files++;
  snprintf(path, sizeof(path), "/private/var/tmp/aj-%u-%s%u-fmt%u.ppm", serial, what, k, fmt);
  if ((f = fopen(path, "wb")) == NULL)
    return;
  fprintf(f, "P6\n%u %u\n255\n", w, h);
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      float px[4];
      uint8_t rgb[3];

      pd_texel_load(fmt, pd_texel_addr(base + off, bpr, fmt, x, y), px);
      for (int c = 0; c < 3; c++)
        rgb[c] = (uint8_t)(pd_clamp(px[c], 0.0f, 1.0f) * 255.0f + 0.5f);
      fwrite(rgb, 1, 3, f);
    }
  }
  // the guest's apfs keeps written pages cached until something forces them out
  fflush(f);
  fsync(fileno(f));
  fclose(f);
  logmsg("AIRJITD: dumped %s", path);
}

static void dump_texs(const char *what, uint32_t serial, uint8_t *base, uint32_t bsize,
                      const struct PDDrawTex *t, unsigned n) {
  for (unsigned k = 0; k < n; k++)
    dump_one(what, serial, k, base, bsize, t[k].off, t[k].w, t[k].h, t[k].bpr, t[k].fmt);
}

void pd_dump_draw(const struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial) {
  int s = dump_slot(r->ffn);

  if (s < 0)
    s = dump_slot(r->vfn);
  if (s < 0)
    return;
  dump_done[s]++;
  dump_one("target", serial, 0, base, bsize, r->targetOff, r->tw, r->th, r->tbpr, r->tfmt);
  dump_texs("ftex", serial, base, bsize, r->ftex, PD_DRAW_TEXS);
}

void pd_dump_compute(const struct PDComputeRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial) {
  int s = dump_slot(r->fn);

  if (s < 0 && !dump_kind(dump_compute, r->fn))
    return;
  if (s >= 0)
    dump_done[s]++;
  dump_texs("tex", serial, base, bsize, r->tex, PD_DRAW_TEXS);
}

void pd_dump_tile(const struct PDTileRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial) {
  int s = dump_slot(r->fn);

  if (s < 0 && !dump_kind(dump_tile, r->fn))
    return;
  if (s >= 0)
    dump_done[s]++;
  dump_texs("rt", serial, base, bsize, r->rt, 4);
  dump_texs("tex", serial, base, bsize, r->tex, PD_DRAW_TEXS);
}

// airjitdcap=NAME[,NAME...] ("all" for every job): a matching job goes to /private/var/tmp/ajc-SERIAL.bin with
// its regions before and after, airjitdcapmax=N bounds the files, airjitdcapeach=N the jobs per name
#define PD_CAP_NAMES 16
#define PD_CAP_EACH 3
#define PD_CAP_FILES 400
#define PD_CAP_MAGIC 0x31434a41u

struct pd_cap_header {
  uint32_t magic, kind, serial, reqBytes;
};

struct pd_cap_region {
  uint32_t off, len, after, pad;
};

static char cap_names[PD_CAP_NAMES][48];
static unsigned cap_done[PD_CAP_NAMES];
static int cap_n = -1, cap_files, cap_max = PD_CAP_FILES, cap_each = PD_CAP_EACH;
static FILE *cap_f;

static void cap_parse(void) {
  char args[1024], *p;
  size_t len = sizeof(args) - 1;

  cap_n = 0;
  if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
    return;
  args[len] = 0;
  if ((p = strstr(args, "airjitdcapmax=")) != NULL)
    cap_max = (int)strtol(p + strlen("airjitdcapmax="), NULL, 10);
  if ((p = strstr(args, "airjitdcapeach=")) != NULL)
    cap_each = (int)strtol(p + strlen("airjitdcapeach="), NULL, 10);
  if ((p = strstr(args, "airjitdcap=")) == NULL)
    return;
  p += strlen("airjitdcap=");
  while (*p && *p != ' ' && cap_n < PD_CAP_NAMES) {
    size_t n = strcspn(p, ", ");

    snprintf(cap_names[cap_n++], sizeof(cap_names[0]), "%.*s", (int)n, p);
    p += n;
    if (*p == ',')
      p++;
  }
}

// the name slot a job matches while it has captures left, -1 otherwise: its stages' names, or "big"
// for a draw into a target at least 800 wide
static int cap_slot(const char *a, const char *b, const char *c) {
  if (cap_n < 0)
    cap_parse();
  if (dump_n < 0)
    dump_parse();
  for (int i = 0; i < cap_n; i++) {
    // "all" and "big" are bounded by airjitdcapmax only
    int all = !strcmp(cap_names[i], "all");
    int many = all || !strcmp(cap_names[i], "big");

    if (!all && !(a && strstr(a, cap_names[i])) && !(b && strstr(b, cap_names[i])) &&
        !(c && !strcmp(c, cap_names[i])))
      continue;
    return (many || (int)cap_done[i] < cap_each) && cap_files < cap_max && dump_due() ? i : -1;
  }
  return -1;
}

static void cap_region(uint8_t *base, uint32_t bsize, uint32_t off, uint64_t len, uint32_t after) {
  struct pd_cap_region rg = { off, (uint32_t)len, after, 0 };

  if (!cap_f || !len || off >= bsize || len > bsize - off)
    return;
  fwrite(&rg, sizeof(rg), 1, cap_f);
  fwrite(base + off, 1, len, cap_f);
}

static void cap_texs(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, unsigned n, uint32_t after) {
  for (unsigned k = 0; k < n; k++) {
    if (t[k].w)
      cap_region(base, bsize, t[k].off, (uint64_t)t[k].bpr * t[k].h, after);
  }
}

static void cap_bufs(uint8_t *base, uint32_t bsize, const struct PDDrawBuffer *b, unsigned n, uint32_t after) {
  for (unsigned k = 0; k < n; k++) {
    cap_region(base, bsize, b[k].off, b[k].size, after);
  }
}

static void cap_close(uint32_t serial);

static void cap_open(uint32_t kind, uint32_t serial, const void *req, uint32_t req_bytes, int slot) {
  struct pd_cap_header h = { PD_CAP_MAGIC, kind, serial, req_bytes };
  char path[96];

  // a job that faulted never got its outputs
  if (cap_f)
    cap_close(0);
  if (slot < 0)
    return;
  cap_done[slot]++;
  cap_files++;
  snprintf(path, sizeof(path), "/private/var/tmp/ajc-%u.bin", serial);
  if ((cap_f = fopen(path, "wb")) == NULL)
    return;
  fwrite(&h, sizeof(h), 1, cap_f);
  fwrite(req, 1, req_bytes, cap_f);
}

static void cap_close(uint32_t serial) {
  if (!cap_f)
    return;
  fflush(cap_f);
  fsync(fileno(cap_f));
  fclose(cap_f);
  cap_f = NULL;
  logmsg("AIRJITD: captured job %u", serial);
}

// vulkan can run a stage without initializing the CPU backend's StageInfo.
static const char *cap_stage_name(const struct StageInfo *stage, uint8_t *base, uint32_t bsize,
                                  uint32_t off, uint32_t size, struct pd_metallib *lib) {
  if (stage && stage->name[0])
    return stage->name;
  if (off <= bsize && size <= bsize - off && !pd_metallib_open(base + off, size, lib))
    return lib->entry;
  return 0;
}

void pd_cap_draw(const struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after) {
  const struct StageInfo *vs = pd_stage(r->vfn), *fs = pd_stage(r->ffn);

  if (!after) {
    struct pd_metallib vl, fl;
    const char *vn = cap_stage_name(vs, base, bsize, r->vmOff, r->vmSize, &vl);
    const char *fn = cap_stage_name(fs, base, bsize, r->fmOff, r->fmSize, &fl);
    cap_open(PD_JOB_DRAW, serial, r, sizeof(*r), cap_slot(vn, fn, r->tw >= 800 ? "big" : 0));
    cap_region(base, bsize, r->vmOff, r->vmSize, 0);
    cap_region(base, bsize, r->fmOff, r->fmSize, 0);
    cap_bufs(base, bsize, r->vbuf, PD_DRAW_BUFS, 0);
    cap_bufs(base, bsize, r->fbuf, PD_DRAW_BUFS, 0);
    if (r->indexed)
      cap_region(base, bsize, r->indexOff, r->indexBytes ? r->indexBytes : (uint64_t)r->count * 2, 0);
    cap_texs(base, bsize, r->ftex, PD_DRAW_TEXS, 0);
  }
  cap_region(base, bsize, r->targetOff, (uint64_t)r->tbpr * r->th, (uint32_t)after);
  cap_texs(base, bsize, r->rt, 4, (uint32_t)after);
  if (after)
    cap_close(serial);
  else if (cap_f) {
    fflush(cap_f);
    logmsg("AIRJITD: captured inputs job %u", serial);
  }
}

void pd_cap_compute(const struct PDComputeRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after) {
  const struct StageInfo *si = pd_stage(r->fn);

  if (!after) {
    struct pd_metallib lib;
    cap_open(PD_JOB_COMPUTE, serial, r, sizeof(*r),
             cap_slot(cap_stage_name(si, base, bsize, r->mOff, r->mSize, &lib), 0, 0));
    cap_region(base, bsize, r->mOff, r->mSize, 0);
  }
  cap_bufs(base, bsize, r->buf, PD_CS_BUFS, (uint32_t)after);
  cap_texs(base, bsize, r->tex, PD_DRAW_TEXS, (uint32_t)after);
  if (after)
    cap_close(serial);
  else if (cap_f) {
    fflush(cap_f);
    logmsg("AIRJITD: captured inputs job %u", serial);
  }
}

void pd_cap_tile(const struct PDTileRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after) {
  const struct StageInfo *si = pd_stage(r->fn);

  if (!after) {
    struct pd_metallib lib;
    cap_open(PD_JOB_TILE, serial, r, sizeof(*r),
             cap_slot(cap_stage_name(si, base, bsize, r->mOff, r->mSize, &lib), 0, 0));
    cap_region(base, bsize, r->mOff, r->mSize, 0);
  }
  cap_bufs(base, bsize, r->buf, PD_CS_BUFS, (uint32_t)after);
  cap_texs(base, bsize, r->tex, PD_DRAW_TEXS, (uint32_t)after);
  cap_texs(base, bsize, r->rt, 4, (uint32_t)after);
  if (after)
    cap_close(serial);
  else if (cap_f) {
    fflush(cap_f);
    logmsg("AIRJITD: captured inputs job %u", serial);
  }
}
