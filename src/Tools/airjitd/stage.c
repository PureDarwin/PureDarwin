#include "stage.h"

#include <string.h>

#include "airjitd.h"
#include "texture.h"

static struct StageInfo stages[PD_MAX_STAGES];
static uint8_t disabled[PD_MAX_STAGES];

static const struct {
  const char *attr;
  unsigned kind;
} param_attrs[] = {
  { "air.buffer", PK_BUFFER },
  { "air.texture", PK_TEXTURE },
  { "air.sampler", PK_SAMPLER },
  { "air.vertex_id", PK_VERTEX_ID },
  { "air.instance_id", PK_INSTANCE_ID },
  { "air.position", PK_POSITION },
  { "air.fragment_input", PK_STAGE_IN },
  { "air.thread_position_in_grid", PK_GRID_POS },
  { "air.threadgroup_position_in_grid", PK_TG_POS },
  { "air.thread_position_in_threadgroup", PK_TG_LOCAL },
  { "air.threads_per_threadgroup", PK_TPTG },
  { "air.threads_per_grid", PK_GRID_SIZE },
  { "air.thread_index_in_threadgroup", PK_TG_INDEX },
  { "air.threadgroups_per_grid", PK_TGS_PER_GRID },
  { "air.thread_index_in_quadgroup", PK_QUAD_INDEX },
  { "air.threads_per_tile", PK_TPTG },
  { "air.thread_index_in_simdgroup", PK_SIMD_LANE },
  { "air.threads_per_simdgroup", PK_SIMD_WIDTH },
  { "air.simdgroup_index_in_threadgroup", PK_SIMD_INDEX },
  { "air.simdgroups_per_threadgroup", PK_SIMDS_PER_TG },
  { "air.quadgroup_index_in_threadgroup", PK_QUAD_GROUP },
  { "air.front_facing", PK_FRONT_FACING },
  { "air.vertex_input", PK_VERTEX_INPUT },
  { "air.base_instance", PK_BASE_INSTANCE },
  { "air.base_vertex", PK_BASE_VERTEX },
};

struct StageInfo *pd_stage(uint32_t id) {
  return id < PD_MAX_STAGES ? &stages[id] : 0;
}

int pd_stage_disabled(uint32_t id) {
  return id < PD_MAX_STAGES && disabled[id];
}

void pd_stage_disable(uint32_t id) {
  if (id < PD_MAX_STAGES)
    disabled[id] = 1;
}

unsigned pd_param_kind(const char *attr, int stage_kind) {
  unsigned i;

  if (!strcmp(attr, "air.render_target"))
    return stage_kind == PD_STAGE_FRAGMENT ? PK_DEST : PK_NONE;

  for (i = 0; i < PD_ARRAY_LEN(param_attrs); i++) {
    if (!strcmp(attr, param_attrs[i].attr))
      return param_attrs[i].kind;
  }

  return PK_NONE;
}

static unsigned unpack_elem(const struct StageInfo *si, unsigned i, const uint8_t *out, float *vals) {
  unsigned k, n = 0;

  for (k = 0; k < si->elemCount[i]; k++) {
    if (si->elemInt[i] == 1) {
      vals[n++] = out[k];
    } else if (si->elemInt[i] == 2) {
      uint16_t v;
      memcpy(&v, out + k * 2, 2);
      vals[n++] = v;
    } else if (si->elemInt[i] == 4) {
      uint32_t v;
      memcpy(&v, out + k * 4, 4);
      vals[n++] = (float)v;
    } else if (si->elemHalf[i]) {
      uint16_t h;
      memcpy(&h, out + k * 2, 2);
      vals[n++] = half_to_float(h);
    } else {
      memcpy(&vals[n++], out + k * 4, 4);
    }
  }
  return n;
}

// the stage's kept outputs as floats: a vertex stage's position first, wherever its struct puts it
unsigned unpack_result(const struct StageInfo *si, const uint8_t *out, float *vals) {
  unsigned off[PD_MAX_RESULTS], i, n = 0, at = 0;

  for (i = 0; i < si->nelems && i < PD_MAX_RESULTS; i++) {
    off[i] = at;
    at += si->elemBytes[i];
  }
  if (si->elemPos < si->nelems && !si->elemSkip[si->elemPos])
    n += unpack_elem(si, si->elemPos, out + off[si->elemPos], vals + n);
  for (i = 0; i < si->nelems && i < PD_MAX_RESULTS; i++) {
    if (i == si->elemPos || si->elemSkip[i])
      continue;
    n += unpack_elem(si, i, out + off[i], vals + n);
  }
  return n;
}
