#ifndef AIRJITD_STAGE_H
#define AIRJITD_STAGE_H

#include <stdint.h>

#define PD_MAX_STAGES 4096
#define PD_MAX_PARAMS 48
#define PD_MAX_RESULTS 8
#define PD_MAX_VARYINGS 32
#define PD_ARG_SLOT 16
#define PD_ARG_BYTES (PD_MAX_PARAMS * PD_ARG_SLOT)
#define PD_OUT_BYTES 128
// elemRT of a fragment's depth output
#define PD_RT_DEPTH 0xfe

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
  // the thread's lane in its 2x2 quad
  PK_QUAD_INDEX,
  // a fragment's air.position, wherever it sits among the stage-in parameters
  PK_POSITION,
  // simdgroups (32 lanes of consecutive thread indices) and quadgroups in compute
  PK_SIMD_LANE,
  PK_SIMD_WIDTH,
  PK_SIMD_INDEX,
  PK_SIMDS_PER_TG,
  PK_QUAD_GROUP,
  // a fragment's front_facing
  PK_FRONT_FACING,
  // a vertex's stage_in attribute (air.vertex_input, location = attribute index) and the draw's bases
  PK_VERTEX_INPUT,
  PK_BASE_INSTANCE,
  PK_BASE_VERTEX,
  PK_COUNT
};

// how a fragment input is interpolated across the triangle
enum {
  PD_INTERP_PERSPECTIVE = 0,
  PD_INTERP_LINEAR = 1,
  PD_INTERP_FLAT = 2,
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
  // integer outputs: lane width in bytes (0 for half and float), unpacked as the number they hold
  unsigned elemInt[PD_MAX_RESULTS];
  // vertex outputs that are not varyings: point size, clip distances
  unsigned elemSkip[PD_MAX_RESULTS];
  // a vertex stage's air.position output, unpacked ahead of the others (0 for any other stage)
  unsigned elemPos;
  // fragment outputs: the colour attachment each one writes (air.render_target N), or PD_RT_DEPTH
  unsigned elemRT[PD_MAX_RESULTS];
  // vertex outputs and fragment inputs pair up by a hash of their generated(..) or user(..) name, 0 none
  uint32_t elemVar[PD_MAX_RESULTS], pvar[PD_MAX_PARAMS];
  uint8_t pinterp[PD_MAX_PARAMS];
  char name[64];
  unsigned pkind[PD_MAX_PARAMS], ploc[PD_MAX_PARAMS], pwidth[PD_MAX_PARAMS], pcount[PD_MAX_PARAMS];
  // parameters holding integers (vertex inputs convert to them)
  uint8_t pint[PD_MAX_PARAMS];
  // a fragment output that is the depth (air.depth)
  int writesDepth;
  int usesDeriv, readsDest, usesQuad, usesBarrier, usesLod;
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
