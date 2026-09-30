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
};

// doorbell selectors on the bridge's user client
enum {
  PD_BRIDGE_SEL_WAIT_JOB = 100,
  PD_BRIDGE_SEL_DONE = 101,
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

struct PDDrawBuffer {
  uint32_t id, off, size, pad;
};

struct PDDrawTex {
  uint32_t id, off, w, h, bpr, fmt;
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
};

// a compute dispatch at PD_BRIDGE_REQ. threads = 1 when the grid counts threads, 0 for threadgroups
struct PDComputeRequest {
  uint32_t fn, mOff, mSize, threads;
  uint64_t grid[3], tpg[3];
  uint32_t tgmem[4], pad[4];
  struct PDDrawBuffer buf[PD_CS_BUFS];
  struct PDDrawTex tex[PD_DRAW_TEXS];
  uint32_t samp[16];
};

// primitive types the kext passes through from MTLPrimitiveType
enum {
  PD_PRIM_TRIANGLES = 3,
  PD_PRIM_TRIANGLE_STRIP = 4,
};

#endif
