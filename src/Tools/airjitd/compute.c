#include "compute.h"

#include <stddef.h>
#include <string.h>

#include "airjitd.h"
#include "runtime.h"
#include "stage.h"
#include "texture.h"

// the builtin values of one thread, lanes x y z
struct pd_thread_ids {
  uint32_t grid_pos[3], tg_pos[3], tg_local[3], tptg[3], grid_size[3], tgs_per_grid[3], tg_index[3];
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
};

struct pd_dispatch {
  struct PDComputeRequest *r;
  const struct StageInfo *si;
  uint64_t groups[3];
  struct PDTex texs[PD_DRAW_TEXS];
  struct PDSampler samp;
  uint8_t args[PD_ARG_BYTES], out[PD_OUT_BYTES];
};

static uint8_t tgmem[PD_TG_MEM_SLOTS][PD_TG_MEM_MAX];

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

static const uint32_t *builtin_lanes(const struct pd_thread_ids *ids, unsigned kind) {
  unsigned i;

  for (i = 0; i < PD_ARRAY_LEN(builtins); i++) {
    if (builtins[i].kind == kind)
      return (const uint32_t *)((const uint8_t *)ids + builtins[i].off);
  }

  return 0;
}

static int bind_textures(struct pd_dispatch *c, uint8_t *base, uint32_t bsize) {
  unsigned i;

  for (i = 0; i < PD_DRAW_TEXS; i++) {
    if (!c->r->tex[i].w)
      continue;

    if (c->r->tex[i].off > bsize)
      return PD_ERR_OUT_OF_RANGE;

    pd_tex_bind(&c->texs[i], base, &c->r->tex[i]);
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
      p = &c->samp;
      break;
    case PK_TG_MEM:
      p = tgmem[loc % PD_TG_MEM_SLOTS];
      break;
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

// threadgroup memory and the imageblock tile start zeroed for every group
static void begin_group(struct pd_dispatch *c) {
  unsigned k;

  for (k = 0; k < PD_TG_MEM_SLOTS; k++) {
    if (c->r->tgmem[k])
      memset(tgmem[k], 0, c->r->tgmem[k] < PD_TG_MEM_MAX ? c->r->tgmem[k] : PD_TG_MEM_MAX);
  }

  pd_ib_begin_group((uint32_t)c->r->tpg[0], (uint32_t)c->r->tpg[1]);
}

// the builtins of thread l in group g, 0 when a dispatchThreads grid does not reach it
static int thread_ids(const struct pd_dispatch *c, const uint64_t g[3], const uint64_t l[3],
                      struct pd_thread_ids *ids) {
  const struct PDComputeRequest *r = c->r;
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
  }

  ids->tg_index[0] = (uint32_t)(l[0] + l[1] * r->tpg[0] + l[2] * r->tpg[0] * r->tpg[1]);
  return 1;
}

static void run_thread(struct pd_dispatch *c, const struct pd_thread_ids *ids) {
  const struct StageInfo *si = c->si;
  unsigned i;

  for (i = 0; i < si->nparams; i++) {
    const uint32_t *lanes = builtin_lanes(ids, si->pkind[i]);

    if (lanes)
      put_lanes(c->args + i * PD_ARG_SLOT, si->pwidth[i], si->pcount[i], lanes);
  }

  pd_stage_run(si, c->args, c->out);
}

static void run_group(struct pd_dispatch *c, const uint64_t g[3]) {
  const struct PDComputeRequest *r = c->r;
  uint64_t l[3];

  begin_group(c);

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

int pd_run_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t bsize) {
  static struct pd_dispatch c;
  uint64_t total = 1, g[3];
  unsigned k;
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

  for (g[2] = 0; g[2] < c.groups[2]; g[2]++) {
    for (g[1] = 0; g[1] < c.groups[1]; g[1]++) {
      for (g[0] = 0; g[0] < c.groups[0]; g[0]++) {
        run_group(&c, g);
      }
    }
  }

  return 0;
}
