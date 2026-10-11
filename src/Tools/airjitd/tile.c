#include "tile.h"

#include <dispatch/dispatch.h>
#include <string.h>

#include "airjitd.h"
#include "compute.h"
#include "runtime.h"
#include "stage.h"
#include "texture.h"

struct pd_tile_run {
  struct PDTileRequest *r;
  const struct StageInfo *si;
  struct PDTex texs[PD_DRAW_TEXS], rts[4];
  struct PDSampler samp[16];
  uint8_t args[PD_ARG_BYTES] __attribute__((aligned(16))), out[PD_OUT_BYTES] __attribute__((aligned(16)));
};

// tiles are spread over up to this many runs per core, taken in turn
#define PD_TILE_CHUNKS_PER_CPU 8
#define PD_TILE_MAX_CHUNKS 1024

// the builtins of one thread: x y z lanes, as compute's
struct pd_tile_ids {
  uint32_t local[3], tile[3], grid[3], tpt[3], tiles[3], grid_size[3], index[3], quad[3];
};

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

static const uint32_t *ids_lanes(const struct pd_tile_ids *ids, unsigned kind) {
  switch (kind) {
  case PK_TG_LOCAL:
    return ids->local;
  case PK_TG_POS:
    return ids->tile;
  case PK_GRID_POS:
    return ids->grid;
  case PK_TPTG:
    return ids->tpt;
  case PK_TGS_PER_GRID:
    return ids->tiles;
  case PK_GRID_SIZE:
    return ids->grid_size;
  case PK_TG_INDEX:
    return ids->index;
  case PK_QUAD_INDEX:
    return ids->quad;
  default:
    return 0;
  }
}

static int bind_resources(struct pd_tile_run *t, uint8_t *base, uint32_t bsize) {
  const struct StageInfo *si = t->si;
  struct PDTileRequest *r = t->r;
  unsigned i;

  for (i = 0; i < PD_DRAW_TEXS; i++) {
    if (r->tex[i].w && r->tex[i].off <= bsize)
      pd_tex_bind_layout(&t->texs[i], base, &r->tex[i], pd_compute_ext(&r->ext) ? &r->ext.texLayout[i] : 0);
  }

  for (i = 0; i < 4; i++) {
    if (r->rt[i].w && r->rt[i].off <= bsize)
      pd_tex_bind(&t->rts[i], base, &r->rt[i]);
  }

  for (i = 0; i < 16; i++) {
    t->samp[i].magic = r->samp[i] ? PD_SAMPLER_MAGIC : 0;
    t->samp[i].flags = r->samp[i];
  }

  for (i = 0; i < si->nparams; i++) {
    unsigned loc = si->ploc[i];
    const void *p;

    switch (si->pkind[i]) {
    case PK_BUFFER:
      if (loc >= PD_CS_BUFS || !r->buf[loc].size) {
        logmsg("AIRJITD: tile buffer %u not bound", loc);
        pd_note("tile buffer %u", loc);
        return PD_ERR_BUFFER;
      }
      p = base + r->buf[loc].off;
      break;
    case PK_TEXTURE:
      if (loc >= PD_DRAW_TEXS || !t->texs[loc].data) {
        logmsg("AIRJITD: tile texture %u not bound", loc);
        pd_note("tile texture %u", loc);
        return PD_ERR_TEXTURE;
      }
      p = &t->texs[loc];
      break;
    case PK_SAMPLER:
      p = &t->samp[loc < 16 ? loc : 0];
      break;
    case PK_TG_MEM:
      // every worker has its own: filled in by run_tiles
      continue;
    default:
      continue;
    }

    {
      uint64_t v = (uint64_t)(uintptr_t)p;
      memcpy(t->args + i * PD_ARG_SLOT, &v, 8);
    }
  }

  return 0;
}

// thread (x, y) of tile (tx, ty). Thread indices run quad by quad, as a real GPU numbers them
static void tile_ids(const struct PDTileRequest *r, uint32_t tx, uint32_t ty, uint32_t x, uint32_t y,
                     struct pd_tile_ids *ids) {
  uint32_t qw = (r->tpt[0] + 1) / 2;

  memset(ids, 0, sizeof(*ids));
  ids->local[0] = x, ids->local[1] = y;
  ids->tile[0] = tx, ids->tile[1] = ty;
  ids->grid[0] = tx * r->tpt[0] + x, ids->grid[1] = ty * r->tpt[1] + y;
  ids->tpt[0] = r->tpt[0], ids->tpt[1] = r->tpt[1], ids->tpt[2] = 1;
  ids->tiles[0] = r->tilesX, ids->tiles[1] = r->tilesY, ids->tiles[2] = 1;
  ids->grid_size[0] = r->tilesX * r->tpt[0], ids->grid_size[1] = r->tilesY * r->tpt[1], ids->grid_size[2] = 1;
  ids->index[0] = ((y / 2) * qw + x / 2) * 4 + (x & 1) + 2 * (y & 1);
  ids->quad[0] = (x & 1) | (y & 1) << 1;
}

static void run_thread(struct pd_tile_run *t, const struct pd_tile_ids *ids) {
  const struct StageInfo *si = t->si;
  unsigned i;

  for (i = 0; i < si->nparams; i++) {
    const uint32_t *lanes = ids_lanes(ids, si->pkind[i]);

    if (lanes)
      put_lanes(t->args + i * PD_ARG_SLOT, si->pwidth[i], si->pcount[i], lanes);
  }

  pd_stage_run(si, t->args, t->out);
}

// one quad's lanes, pass by pass when they shuffle: only the last pass writes anything
static void run_quad(struct pd_tile_run *t, uint32_t tx, uint32_t ty, uint32_t qx, uint32_t qy) {
  const struct PDTileRequest *r = t->r;
  uint32_t n = 0, p, lane;

  pd_quad_reset();

  for (p = 0;; p++) {
    int last = !t->si->usesQuad || (p > 0 && p >= n);

    pd_set_dry(!last);

    for (lane = 0; lane < 4; lane++) {
      uint32_t x = qx * 2 + (lane & 1), y = qy * 2 + (lane >> 1);
      struct pd_tile_ids ids;

      if (x >= r->tpt[0] || y >= r->tpt[1])
        continue;

      tile_ids(r, tx, ty, x, y, &ids);
      pd_quad_lane(lane, p);
      run_thread(t, &ids);
    }

    pd_quad_end_pass();

    if (last)
      break;

    {
      uint32_t c = pd_quad_calls();

      if (c > n)
        n = c > 16 ? 16 : c;
    }
  }

  pd_set_dry(0);
}

// a run of tiles for one worker, by linear tile index
struct pd_tile_chunk {
  const struct pd_tile_run *t;
  uint32_t t0, t1;
  int sig;
};

static void run_tiles(void *arg) {
  struct pd_tile_chunk *ch = arg;
  struct pd_tile_run t = *ch->t;
  const struct PDTileRequest *r = t.r;
  uint32_t ti, qx, qy, k;

  for (k = 0; k < t.si->nparams; k++) {
    if (t.si->pkind[k] == PK_TG_MEM) {
      uint64_t v = (uint64_t)(uintptr_t)pd_tg_mem(t.si->ploc[k]);
      memcpy(t.args + k * PD_ARG_SLOT, &v, 8);
    }
  }

  pd_quad_enable(1);

  for (ti = ch->t0; ti < ch->t1; ti++) {
    uint32_t tx = ti % r->tilesX, ty = ti / r->tilesX;

    // threadgroup memory starts zeroed for every tile
    for (k = 0; k < PD_TG_SLOTS; k++) {
      uint8_t *m = pd_tg_mem(k);

      if (r->tgmem[k] && m)
        memset(m, 0, r->tgmem[k] < PD_TG_BYTES ? r->tgmem[k] : PD_TG_BYTES);
    }

    pd_tile_begin(t.rts, tx * PD_TILE_SIZE, ty * PD_TILE_SIZE);

    for (qy = 0; qy < (r->tpt[1] + 1) / 2; qy++) {
      for (qx = 0; qx < (r->tpt[0] + 1) / 2; qx++) {
        run_quad(&t, tx, ty, qx, qy);
      }
    }
  }

  pd_quad_enable(0);
}

static void apply_tiles(void *ctx, size_t i) {
  struct pd_tile_chunk *ch = (struct pd_tile_chunk *)ctx + i;

  ch->sig = pd_guarded(run_tiles, ch);
}

int pd_run_tile(struct PDTileRequest *r, uint8_t *base, uint32_t bsize) {
  static struct pd_tile_run t;
  static struct pd_tile_chunk chunks[PD_TILE_MAX_CHUNKS];
  uint32_t tiles, n, k, most;
  int rc;

  memset(&t, 0, sizeof(t));
  t.r = r;
  t.si = pd_stage(r->fn);

  if (!t.si || !t.si->entry)
    return PD_ERR_NOT_COMPILED;

  if (!r->tpt[0] || !r->tpt[1] || r->tpt[0] * r->tpt[1] > 1024 || r->tilesX > 4096 || r->tilesY > 4096)
    return PD_ERR_OUT_OF_RANGE;

  if ((rc = bind_resources(&t, base, bsize)))
    return rc;

  // tiles touch disjoint pixels of the attachments: one run of them per worker
  tiles = r->tilesX * r->tilesY;
  most = pd_cpus() * PD_TILE_CHUNKS_PER_CPU;
  most = most < PD_TILE_MAX_CHUNKS ? most : PD_TILE_MAX_CHUNKS;
  n = tiles < most ? tiles : most;

  for (k = 0; k < n; k++) {
    chunks[k].t = &t;
    chunks[k].t0 = tiles * k / n, chunks[k].t1 = tiles * (k + 1) / n;
    chunks[k].sig = 0;
  }

  if (n == 1)
    run_tiles(&chunks[0]);
  else if (n > 1)
    dispatch_apply_f(n, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), chunks, apply_tiles);

  for (k = 0; k < n; k++) {
    if (chunks[k].sig)
      pd_job_abort(chunks[k].sig, "fault on a tile worker");
  }

  return 0;
}
