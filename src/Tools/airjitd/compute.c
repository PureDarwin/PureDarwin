#include "compute.h"

#include <dispatch/dispatch.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "airjitd.h"
#include "runtime.h"
#include "stage.h"
#include "texture.h"

// the builtin values of one thread, lanes x y z
struct pd_thread_ids {
  uint32_t grid_pos[3], tg_pos[3], tg_local[3], tptg[3], grid_size[3], tgs_per_grid[3], tg_index[3];
  uint32_t simd_lane[3], simd_width[3], simd_index[3], simds[3], quad_lane[3], quad_group[3];
};

static const struct {
  unsigned kind;
  size_t off;
} builtins[] = {
  { PK_GRID_POS, offsetof(struct pd_thread_ids, grid_pos) },
  { PK_TG_POS, offsetof(struct pd_thread_ids, tg_pos) },
  { PK_TG_LOCAL, offsetof(struct pd_thread_ids, tg_local) },
  { PK_TPTG, offsetof(struct pd_thread_ids, tptg) },
  { PK_GRID_SIZE, offsetof(struct pd_thread_ids, grid_size) },
  { PK_TGS_PER_GRID, offsetof(struct pd_thread_ids, tgs_per_grid) },
  { PK_TG_INDEX, offsetof(struct pd_thread_ids, tg_index) },
  { PK_SIMD_LANE, offsetof(struct pd_thread_ids, simd_lane) },
  { PK_SIMD_WIDTH, offsetof(struct pd_thread_ids, simd_width) },
  { PK_SIMD_INDEX, offsetof(struct pd_thread_ids, simd_index) },
  { PK_SIMDS_PER_TG, offsetof(struct pd_thread_ids, simds) },
  { PK_QUAD_INDEX, offsetof(struct pd_thread_ids, quad_lane) },
  { PK_QUAD_GROUP, offsetof(struct pd_thread_ids, quad_group) },
};

// a builtin parameter: its slot and where its lanes sit in pd_thread_ids
struct pd_builtin_slot {
  uint16_t slot, off;
  uint8_t width, count;
};

struct pd_dispatch {
  struct PDComputeRequest *r;
  const struct StageInfo *si;
  uint64_t groups[3];
  struct PDTex texs[PD_DRAW_TEXS];
  struct PDSampler samp[16];
  uint8_t args[PD_ARG_BYTES] __attribute__((aligned(16))), out[PD_OUT_BYTES] __attribute__((aligned(16)));
  struct pd_builtin_slot bslot[PD_MAX_PARAMS];
  unsigned nbslots;
};

// groups are spread over up to this many runs per core, taken in turn, once a dispatch has this many threads
#define PD_CS_CHUNKS_PER_CPU 8
#define PD_CS_MAX_CHUNKS 1024
#define PD_CS_PARALLEL_THREADS 4096

// lanes of a builtin slot: the kernel declares uint, ushort2, uint3 and so on
static void put_lanes(uint8_t *slot, unsigned width, unsigned count, const uint32_t v[3]) {
  unsigned k;

  for (k = 0; k < count && k < 3; k++) {
    if (width == 2) {
      uint16_t x = (uint16_t)v[k];
      memcpy(slot + k * 2, &x, 2);
    } else {
      memcpy(slot + k * 4, &v[k], 4);
    }
  }
}

// the builtin parameters in slot order, found once per dispatch
static void find_builtins(struct pd_dispatch *c) {
  const struct StageInfo *si = c->si;
  unsigned i, k;

  c->nbslots = 0;
  for (i = 0; i < si->nparams && i < PD_MAX_PARAMS; i++) {
    for (k = 0; k < PD_ARRAY_LEN(builtins); k++) {
      if (builtins[k].kind != si->pkind[i])
        continue;

      c->bslot[c->nbslots].slot = (uint16_t)i, c->bslot[c->nbslots].off = (uint16_t)builtins[k].off;
      c->bslot[c->nbslots].width = (uint8_t)si->pwidth[i], c->bslot[c->nbslots].count = (uint8_t)si->pcount[i];
      c->nbslots++;
      break;
    }
  }
}

static int bind_textures(struct pd_dispatch *c, uint8_t *base, uint32_t bsize) {
  unsigned i;

  // a bound sampler's flags as the draw's, 0 leaves the old defaults
  for (i = 0; i < 16; i++) {
    c->samp[i].magic = c->r->samp[i] ? PD_SAMPLER_MAGIC : 0;
    c->samp[i].flags = c->r->samp[i];
  }

  for (i = 0; i < PD_DRAW_TEXS; i++) {
    if (!c->r->tex[i].w)
      continue;

    if (c->r->tex[i].off > bsize)
      return PD_ERR_OUT_OF_RANGE;

    if (c->r->tex[i].w && pd_fmt_is_compressed(c->r->tex[i].fmt) && !pd_fmt_cpu_decodes(c->r->tex[i].fmt)) {
      logmsg("AIRJITD: texture format %u has no CPU decoder", c->r->tex[i].fmt);
      return PD_ERR_TEXTURE;
    }
    {
      const struct PDComputeExt *x = pd_compute_ext(&c->r->ext);
      pd_tex_bind_layout(&c->texs[i], base, &c->r->tex[i], x ? &x->texLayout[i] : 0);
    }
  }

  return 0;
}

// pointer slots are the same for every thread: filled once
static int bind_resources(struct pd_dispatch *c, uint8_t *base) {
  const struct StageInfo *si = c->si;
  unsigned i;

  for (i = 0; i < si->nparams; i++) {
    unsigned loc = si->ploc[i];
    const void *p;

    switch (si->pkind[i]) {
    case PK_BUFFER:
      if (loc >= PD_CS_BUFS || !c->r->buf[loc].size) {
        logmsg("AIRJITD: compute buffer %u not bound", loc);
        pd_note("buffer %u", loc);
        return PD_ERR_BUFFER;
      }
      p = base + c->r->buf[loc].off;
      break;
    case PK_TEXTURE:
      if (loc >= PD_DRAW_TEXS || !c->texs[loc].data) {
        logmsg("AIRJITD: compute texture %u not bound", loc);
        pd_note("texture %u", loc);
        return PD_ERR_TEXTURE;
      }
      p = &c->texs[loc];
      break;
    case PK_SAMPLER:
      p = &c->samp[loc < 16 ? loc : 0];
      break;
    case PK_TG_MEM:
      // every worker has its own: filled in by tg_slots
      continue;
    default:
      continue;
    }

    {
      uint64_t v = (uint64_t)(uintptr_t)p;
      memcpy(c->args + i * PD_ARG_SLOT, &v, 8);
    }
  }

  return 0;
}

// threadgroup memory parameters point at the running thread's own
static void tg_slots(struct pd_dispatch *c) {
  unsigned i;

  for (i = 0; i < c->si->nparams; i++) {
    if (c->si->pkind[i] == PK_TG_MEM) {
      uint64_t v = (uint64_t)(uintptr_t)pd_tg_mem(c->si->ploc[i]);
      memcpy(c->args + i * PD_ARG_SLOT, &v, 8);
    }
  }
}

// threadgroup memory and the imageblock tile start zeroed for every group
static void begin_group(struct pd_dispatch *c) {
  unsigned k;

  for (k = 0; k < PD_TG_MEM_SLOTS; k++) {
    uint8_t *m = pd_tg_mem(k);

    if (c->r->tgmem[k] && m)
      memset(m, 0, c->r->tgmem[k] < PD_TG_BYTES ? c->r->tgmem[k] : PD_TG_BYTES);
  }

  pd_ib_begin_group(c->r->imageblock[0] ? c->r->imageblock[0] : (uint32_t)c->r->tpg[0],
                    c->r->imageblock[1] ? c->r->imageblock[1] : (uint32_t)c->r->tpg[1]);
}

// the builtins of thread l in group g, 0 when a dispatchThreads grid does not reach it. A group at the
// edge of such a grid is clipped: its size and the thread indices in it count only the threads that run
static int thread_ids(const struct pd_dispatch *c, const uint64_t g[3], const uint64_t l[3],
                      struct pd_thread_ids *ids) {
  const struct PDComputeRequest *r = c->r;
  uint32_t n;
  unsigned k;

  memset(ids, 0, sizeof(*ids));

  for (k = 0; k < 3; k++) {
    ids->grid_pos[k] = (uint32_t)(g[k] * r->tpg[k] + l[k]);
    ids->tg_pos[k] = (uint32_t)g[k];
    ids->tg_local[k] = (uint32_t)l[k];
    ids->tptg[k] = (uint32_t)r->tpg[k];
    ids->tgs_per_grid[k] = (uint32_t)c->groups[k];
    ids->grid_size[k] = (uint32_t)(r->threads ? r->grid[k] : c->groups[k] * r->tpg[k]);

    if (r->threads && ids->grid_pos[k] >= r->grid[k])
      return 0;
    if (r->threads && (g[k] + 1) * r->tpg[k] > r->grid[k])
      ids->tptg[k] = (uint32_t)(r->grid[k] - g[k] * r->tpg[k]);
  }

  ids->tg_index[0] = (uint32_t)(l[0] + l[1] * ids->tptg[0] + l[2] * ids->tptg[0] * ids->tptg[1]);
  n = ids->tptg[0] * ids->tptg[1] * ids->tptg[2];
  ids->simd_lane[0] = ids->tg_index[0] % 32, ids->simd_width[0] = 32;
  ids->simd_index[0] = ids->tg_index[0] / 32, ids->simds[0] = (n + 31) / 32;
  ids->quad_lane[0] = ids->tg_index[0] & 3, ids->quad_group[0] = ids->tg_index[0] / 4;
  return 1;
}

static void run_thread(struct pd_dispatch *c, const struct pd_thread_ids *ids) {
  unsigned i;

  for (i = 0; i < c->nbslots; i++) {
    const struct pd_builtin_slot *b = &c->bslot[i];

    put_lanes(c->args + b->slot * PD_ARG_SLOT, b->width, b->count, (const uint32_t *)((const uint8_t *)ids + b->off));
  }

  pd_stage_run(c->si, c->args, c->out);
}

// a group with barriers: its threads as fibers, so a barrier waits for the whole group
struct pd_fiber_group {
  struct pd_dispatch *c;
  struct pd_thread_ids ids[1024];
  uint32_t lane[1024];
};

static void fiber_thread(void *arg, unsigned i) {
  struct pd_fiber_group *fg = arg;

  run_thread(fg->c, &fg->ids[i]);
}

static int run_group_fibers(struct pd_dispatch *c, const uint64_t g[3]) {
  static __thread struct pd_fiber_group *fg;
  const struct PDComputeRequest *r = c->r;
  uint64_t l[3];
  unsigned n = 0;

  if (r->tpg[0] * r->tpg[1] * r->tpg[2] > 1024 || (!fg && !(fg = malloc(sizeof(*fg)))))
    return -1;

  fg->c = c;
  for (l[2] = 0; l[2] < r->tpg[2]; l[2]++) {
    for (l[1] = 0; l[1] < r->tpg[1]; l[1]++) {
      for (l[0] = 0; l[0] < r->tpg[0]; l[0]++) {
        if (thread_ids(c, g, l, &fg->ids[n])) {
          fg->lane[n] = fg->ids[n].tg_index[0];
          n++;
        }
      }
    }
  }

  return pd_fibers_run(n, fiber_thread, fg, fg->lane);
}

static void run_group(struct pd_dispatch *c, const uint64_t g[3]) {
  const struct PDComputeRequest *r = c->r;
  uint64_t l[3];

  begin_group(c);

  if (c->si->usesBarrier && run_group_fibers(c, g) == 0)
    return;

  for (l[2] = 0; l[2] < r->tpg[2]; l[2]++) {
    for (l[1] = 0; l[1] < r->tpg[1]; l[1]++) {
      for (l[0] = 0; l[0] < r->tpg[0]; l[0]++) {
        struct pd_thread_ids ids;

        if (thread_ids(c, g, l, &ids))
          run_thread(c, &ids);
      }
    }
  }
}

// a run of groups for one worker, by linear group index
struct pd_cs_chunk {
  const struct pd_dispatch *c;
  uint64_t g0, g1;
  int sig;
};

static void run_chunk(void *arg) {
  struct pd_cs_chunk *ch = arg;
  struct pd_dispatch c = *ch->c;
  uint64_t gi, g[3];

  tg_slots(&c);

  for (gi = ch->g0; gi < ch->g1; gi++) {
    g[0] = gi % c.groups[0];
    g[1] = (gi / c.groups[0]) % c.groups[1];
    g[2] = gi / (c.groups[0] * c.groups[1]);
    run_group(&c, g);
  }
}

static void apply_chunk(void *ctx, size_t i) {
  struct pd_cs_chunk *ch = (struct pd_cs_chunk *)ctx + i;

  ch->sig = pd_guarded(run_chunk, ch);
}

int pd_run_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t bsize) {
  static struct pd_dispatch c;
  static struct pd_cs_chunk chunks[PD_CS_MAX_CHUNKS];
  uint64_t total = 1, groups, most;
  unsigned k, n;
  int rc;

  memset(&c, 0, sizeof(c));
  c.r = r;
  c.si = pd_stage(r->fn);

  if (!c.si->entry)
    return PD_ERR_NOT_COMPILED;

  if ((rc = bind_textures(&c, base, bsize)))
    return rc;

  for (k = 0; k < 3; k++) {
    if (!r->tpg[k])
      r->tpg[k] = 1;

    c.groups[k] = r->threads ? (r->grid[k] + r->tpg[k] - 1) / r->tpg[k] : r->grid[k];
    total *= c.groups[k] * r->tpg[k];
  }

  if (total == 0)
    return 0;

  if (total > PD_MAX_THREADS) {
    logmsg("AIRJITD: compute grid too large (%llu threads)", (unsigned long long)total);
    return PD_ERR_OUT_OF_RANGE;
  }

  if ((rc = bind_resources(&c, base)))
    return rc;

  find_builtins(&c);

  // groups are independent: big dispatches spread them over the cores, small ones stay here
  groups = c.groups[0] * c.groups[1] * c.groups[2];
  most = (uint64_t)pd_cpus() * PD_CS_CHUNKS_PER_CPU;
  most = most < PD_CS_MAX_CHUNKS ? most : PD_CS_MAX_CHUNKS;
  n = total >= PD_CS_PARALLEL_THREADS && groups > 1 && pd_cpus() > 1 ? (unsigned)(groups < most ? groups : most) : 1;

  for (k = 0; k < n; k++) {
    chunks[k].c = &c;
    chunks[k].g0 = groups * k / n, chunks[k].g1 = groups * (k + 1) / n;
    chunks[k].sig = 0;
  }

  if (n == 1)
    run_chunk(&chunks[0]);
  else
    dispatch_apply_f(n, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), chunks, apply_chunk);

  for (k = 0; k < n; k++) {
    if (chunks[k].sig)
      pd_job_abort(chunks[k].sig, "fault on a compute worker");
  }

  return 0;
}
