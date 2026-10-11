#ifndef AIRJITD_BRIDGE_H
#define AIRJITD_BRIDGE_H

#include <stdint.h>

// shared memory with PDIOSurface: these must match the kext
#define PD_BRIDGE_TYPE 0x50444750u
#define PD_BRIDGE_BYTES 0x8000000u
#define PD_DAEMON_READY 0x44525652u
#define PD_BRIDGE_REQ 0x100u

#define PD_DRAW_BUFS 16
#define PD_DRAW_TEXS 16
#define PD_CS_BUFS 31

enum {
  PD_JOB_LEGACY_COMPUTE = 0,
  PD_JOB_INTROSPECT = 1,
  PD_JOB_DRAW = 2,
  PD_JOB_COMPUTE = 3,
  PD_JOB_TILE = 4,
};

// doorbell selectors on the bridge's user client
enum {
  PD_BRIDGE_SEL_WAIT_JOB = 100,
  PD_BRIDGE_SEL_DONE = 101,
  // one log line, IOLogged by the kext
  PD_BRIDGE_SEL_LOG = 102,
};

enum {
  PD_STATE_IDLE = 0,
  PD_STATE_QUEUED = 1,
  PD_STATE_DONE = 2,
  PD_STATE_FAILED = 3,
};

struct PDBridgeHeader {
  volatile uint32_t state;
  uint32_t serial, status, functionID, pipelineID;
  uint32_t metallibSize, dataSize, dataOffset, bufferID;
  uint64_t bufferOffset;
  uint64_t grid[3], threadsPerGroup[3];
  uint32_t kind;
  char error[128];
};

// the protocol the daemon speaks, written at this header offset (after error[]) before status goes READY and
// cleared when it exits: 1 = it takes PDDrawExt on draws from the kext. An older daemon leaves 0 there
#define PD_BRIDGE_VERSION 1u
#define PD_BRIDGE_VERSION_OFF 0xe4u
#ifndef __cplusplus
_Static_assert(PD_BRIDGE_VERSION_OFF >= __builtin_offsetof(struct PDBridgeHeader, error) + 128 && PD_BRIDGE_VERSION_OFF + 4 <= PD_BRIDGE_REQ,
               "version word overlaps the header or the request");
#endif

struct PDDrawBuffer {
  uint32_t id, off, size, pad;
};

struct PDDrawTex {
  uint32_t id, off, w, h, bpr, fmt;
};

// level k of slice s at the PDDrawTex's off + s * sliceBytes + level[k].off, rows level[k].bpr apart (level 0:
// off 0, the PDDrawTex's bpr), a 3D level's depth slices follow each other and sliceBytes is 0
#define PD_TEX_LEVELS 16
#define PD_TEXLAYOUT_SET 1u
struct PDTexLevel {
  uint32_t off, bpr;
};

struct PDTexLayout {
  // type: MTLTextureType (2 2D, 3 2D array, 5 cube, 6 cube array, 7 3D), flags PD_TEXLAYOUT_SET
  uint8_t type, levels, samples, flags;
  // array length (times 6 for cubes), or a 3D texture's depth
  uint32_t slices, sliceBytes, pad;
  struct PDTexLevel level[PD_TEX_LEVELS];
};

// attribute i (air.vertex_input location i): MTLVertexFormat (0 unset) at offset in vertex buffer `buffer`,
// whose layout has the stride and MTLVertexStepFunction (2 per instance, every `rate`)
#define PD_VERTEX_ATTRS 31
struct PDVertexAttr {
  uint8_t format, buffer;
  uint16_t offset;
};

struct PDVertexLayout {
  uint32_t stride;
  uint8_t step, pad[3];
  uint32_t rate;
};

// what a draw adds to PDDrawRequest when ext.magic is PD_EXT_MAGIC: a request without it draws with no
// depth, culling or scissor, level 0 textures and buffer-indexed vertices, as before
#define PD_EXT_MAGIC 0x31585450u
// depthState: MTLCompareFunction in bits 0-2, depth write bit 3, bit 31 when a depth state is set
#define PD_DEPTH_WRITE 0x8u
#define PD_DEPTH_SET 0x80000000u
// stencil[0] front, [1] back: MTLCompareFunction 0-2, MTLStencilOperation on stencil fail 3-5, depth
// fail 6-8 and pass 9-11, read mask 12-19, write mask 20-27, bit 31 when the stencil test runs
#define PD_STENCIL_ON 0x80000000u
// raster: MTLCullMode in bits 0-1, MTLWinding bit 2 (0 clockwise), lines fill bit 3, depth clamp bit 4,
// scissor bit 5, bit 31 when set
#define PD_RASTER_CCW 0x4u
#define PD_RASTER_LINES 0x8u
#define PD_RASTER_CLAMP 0x10u
#define PD_RASTER_SCISSOR 0x20u
// no colour attachment: the draw writes depth and stencil only, tw/th (and the target fields) are the depth's
#define PD_RASTER_NOCOLOR 0x40u
#define PD_RASTER_SET 0x80000000u
struct PDDrawExt {
  uint32_t magic, size;
  // the depth/stencil attachment (PD_FMT_DEPTH*/STENCIL8), w 0 when the pass has none
  struct PDDrawTex depth;
  uint32_t depthState, stencil[2], stencilRef;
  uint32_t raster;
  float depthBias, depthSlope, depthBiasClamp;
  uint32_t scissor[4];
  float blendColor[4];
  // vertexStart, baseInstance and baseVertex of the draw call, instance and vertex ids include them
  uint32_t firstVertex, baseInstance;
  int32_t baseVertex;
  uint32_t pad;
  struct PDVertexAttr attr[PD_VERTEX_ATTRS];
  uint32_t pad1;
  struct PDVertexLayout layout[PD_VERTEX_ATTRS];
  // vertex stage textures and samplers, like ftex/samp for the fragment
  struct PDDrawTex vtex[PD_DRAW_TEXS];
  uint32_t vsamp[16];
  struct PDTexLayout ftexLayout[PD_DRAW_TEXS], vtexLayout[PD_DRAW_TEXS];
};

// compute and tile requests: their textures' layouts
struct PDComputeExt {
  uint32_t magic, size;
  struct PDTexLayout texLayout[PD_DRAW_TEXS];
};

// a draw at PD_BRIDGE_REQ: both stage metallibs, the bound buffers, the indices and the target
struct PDDrawRequest {
  uint32_t vfn, ffn, vmOff, vmSize, fmOff, fmSize;
  uint32_t prim, count, indexed, indexOff, indexBytes, blend;
  struct PDDrawBuffer vbuf[PD_DRAW_BUFS], fbuf[PD_DRAW_BUFS];
  uint32_t targetOff, tw, th, tbpr, tfmt, pad1;
  double viewport[6];
  struct PDDrawTex ftex[PD_DRAW_TEXS];
  // instanced draws: every instance runs the vertex stage with its instance_id, 0 means one
  uint32_t instances, pad2;
  // colour attachments 1-3 (entry 0 unused, attachment 0 is target*): w 0 when absent
  struct PDDrawTex rt[4];
  // their packed blend states, as blend is for attachment 0; 0 stores unblended
  uint32_t rtBlend[4];
  // each bound fragment sampler's flags word (host message type 3), 0 unknown
  uint32_t samp[16];
  struct PDDrawExt ext;
};

// a compute dispatch at PD_BRIDGE_REQ. threads = 1 when the grid counts threads, 0 for threadgroups
struct PDComputeRequest {
  uint32_t fn, mOff, mSize, threads;
  uint64_t grid[3], tpg[3];
  uint32_t tgmem[4];
  // an explicit imageblock's width and height (setImageblockWidth:height:), 0 = the threadgroup's
  uint32_t imageblock[2], pad[2];
  struct PDDrawBuffer buf[PD_CS_BUFS];
  struct PDDrawTex tex[PD_DRAW_TEXS];
  uint32_t samp[16];
  struct PDComputeExt ext;
};

// a request's extension when the sender filled it in
static inline const struct PDDrawExt *pd_draw_ext(const struct PDDrawRequest *r) {
  return r->ext.magic == PD_EXT_MAGIC && r->ext.size == sizeof(struct PDDrawExt) ? &r->ext : 0;
}

static inline const struct PDComputeExt *pd_compute_ext(const struct PDComputeExt *x) {
  return x->magic == PD_EXT_MAGIC && x->size == sizeof(struct PDComputeExt) ? x : 0;
}

// samp[] words (sel 257 type 3: bit 31 normalized, bits 0/2 min/mag linear, addressing at 8/12/16) carry this
// bit when the kext knows the sampler, so an all-clear one is not taken for unknown (0)
#define PD_SAMP_VALID 0x40000000u
// the MTLSamplerMipFilter value in bits 4-5 (0 not mipmapped, 1 nearest, 2 linear) and a depth compare function
// + 1 in bits 20-23. The kext's sel 257 type 3 word has not shown where Apple keeps these
#define PD_SAMP_MIP(f) (((f) >> 4) & 3)
#define PD_SAMP_COMPARE(f) (((f) >> 20) & 15)

// a tile dispatch (dispatchThreadsPerTile) at PD_BRIDGE_REQ: tpt threads run on every 32x32 tile of
// the pass, and their implicit imageblock is the pass's colour attachments rt[0..3] (w 0 when absent)
#define PD_TILE_SIZE 32
struct PDTileRequest {
  uint32_t fn, mOff, mSize, pad0;
  uint32_t tpt[3], tilesX, tilesY, pad1;
  uint32_t tgmem[4];
  struct PDDrawBuffer buf[PD_CS_BUFS];
  struct PDDrawTex tex[PD_DRAW_TEXS];
  uint32_t samp[16];
  struct PDDrawTex rt[4];
  struct PDComputeExt ext;
};

// primitive types the kext passes through from MTLPrimitiveType
enum {
  PD_PRIM_POINTS = 0,
  PD_PRIM_LINES = 1,
  PD_PRIM_LINE_STRIP = 2,
  PD_PRIM_TRIANGLES = 3,
  PD_PRIM_TRIANGLE_STRIP = 4,
};

#endif
