#ifndef AIRJITD_STAGE_H
#define AIRJITD_STAGE_H

#include <stdint.h>

#define PD_MAX_STAGES 256
#define PD_MAX_PARAMS 48
#define PD_MAX_RESULTS 8
#define PD_MAX_VARYINGS 32
#define PD_ARG_SLOT 16
#define PD_ARG_BYTES (PD_MAX_PARAMS * PD_ARG_SLOT)
#define PD_OUT_BYTES 128

// what a shader parameter is bound to, from its air.* argument descriptor
enum {
  PK_NONE,
  PK_BUFFER,
  PK_TEXTURE,
  PK_SAMPLER,
  PK_VERTEX_ID,
  PK_STAGE_IN,
  // kernel builtins, one slot of pcount lanes of pwidth bytes each
  PK_GRID_POS,
  PK_TG_POS,
  PK_TG_LOCAL,
  PK_TPTG,
  PK_GRID_SIZE,
  PK_TG_INDEX,
  PK_TGS_PER_GRID,
  PK_TG_MEM,
  // framebuffer fetch: the target's current colour at this pixel
  PK_DEST,
  PK_INSTANCE_ID,
  PK_COUNT
};

// which entry point list a function came from
enum {
  PD_STAGE_VERTEX,
  PD_STAGE_FRAGMENT,
  PD_STAGE_KERNEL,
};

// a compiled stage: entry(args, out) with PD_ARG_SLOT bytes per parameter,
// out gets the returned struct packed (2 bytes per half, 4 per float)
typedef void (*pd_stage_entry)(void *args, void *out);

struct StageInfo {
  pd_stage_entry entry;
  unsigned nparams, nelems, outBytes;
  unsigned elemBytes[PD_MAX_RESULTS], elemCount[PD_MAX_RESULTS], elemHalf[PD_MAX_RESULTS];
  // vertex outputs that are not varyings: point size, clip distances
  unsigned elemSkip[PD_MAX_RESULTS];
  // fragment outputs: the colour attachment each one writes (air.render_target N)
  unsigned elemRT[PD_MAX_RESULTS];
  unsigned pkind[PD_MAX_PARAMS], ploc[PD_MAX_PARAMS], pwidth[PD_MAX_PARAMS], pcount[PD_MAX_PARAMS];
  int usesDeriv, readsDest;
};

struct StageInfo *pd_stage(uint32_t id);
int pd_stage_disabled(uint32_t id);
void pd_stage_disable(uint32_t id);

// the parameter kind an air.* attribute string names, PK_NONE when it names none
unsigned pd_param_kind(const char *attr, int stage_kind);

// a stage result as floats, element by element, returning how many
unsigned unpack_result(const struct StageInfo *si, const uint8_t *out, float *vals);

static inline void pd_stage_run(const struct StageInfo *si, void *args, void *out) {
  si->entry(args, out);
}

#endif
