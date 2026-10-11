// pdmtl: a minimal Metal-shaped C API over airjitd's executor, for running metallibs on Linux
#ifndef PDMTL_H
#define PDMTL_H

#include <stddef.h>
#include <stdint.h>

struct pdmtl_lib;
struct pdmtl_fn;
struct PDDrawTex;

// a resource is a range of the shared arena: contents is CPU-visible like MTLStorageModeShared
struct pdmtl_buf {
  void *contents;
  uint32_t off, size;
};

struct pdmtl_tex {
  void *contents;
  uint32_t off, w, h, bpr, fmt;
};

int pdmtl_init(size_t arena_bytes);
struct pdmtl_buf pdmtl_new_buffer(const void *bytes, size_t len);
struct pdmtl_tex pdmtl_new_texture(uint32_t w, uint32_t h, uint32_t fmt);

// an MTLB metallib from disk, functions by name like newFunctionWithName:
struct pdmtl_lib *pdmtl_lib_load(const char *path);
struct pdmtl_lib *pdmtl_lib_load_data(const void *data, size_t size);
unsigned pdmtl_lib_count(const struct pdmtl_lib *lib);
const char *pdmtl_lib_name(const struct pdmtl_lib *lib, unsigned i);
struct pdmtl_fn *pdmtl_fn_get(struct pdmtl_lib *lib, const char *name);

// a function constant's value, by index or (when name is set) by name, as its bytes
struct pdmtl_constant {
  uint32_t index;
  char name[64];
  uint8_t bytes[64];
};

// a copy of fn with its function constants set (absent ones undefined) and static initialisers folded,
// or fn itself when it has neither
struct pdmtl_fn *pdmtl_fn_specialize(struct pdmtl_fn *fn, const struct pdmtl_constant *c, unsigned nc);

// the function constants fn declares (!air.function_constants), with their MSL type names
struct pdmtl_constant_info {
  uint32_t index;
  int required;
  char name[64], type[16];
};

unsigned pdmtl_fn_constants(const struct pdmtl_fn *fn, struct pdmtl_constant_info *out, unsigned max);

// dispatchThreads:threadsPerThreadgroup: with buffers at [[buffer(i)]], 0 or a daemon error
int pdmtl_dispatch_threads(struct pdmtl_fn *fn, const struct pdmtl_buf *bufs, unsigned nbufs, const uint64_t grid[3],
                           const uint64_t tpg[3]);

// drawPrimitives:vertexStart:0 vertexCount: into one colour target, vertex buffers at [[buffer(i)]]
int pdmtl_draw(struct pdmtl_fn *vfn, struct pdmtl_fn *ffn, uint32_t prim, uint32_t count, const struct pdmtl_buf *vbufs,
               unsigned nvbufs, struct pdmtl_tex *target);

// a draw with fragment buffers, instances and source-alpha blending
struct pdmtl_draw_desc {
  struct pdmtl_fn *vfn, *ffn;
  uint32_t prim, count, instances;
  const struct pdmtl_buf *vbufs, *fbufs;
  unsigned nvbufs, nfbufs;
  int blend;
};

int pdmtl_draw_ex(const struct pdmtl_draw_desc *d, struct pdmtl_tex *target);

// compiles a function for the CPU path without running it, 0 when it compiles
int pdmtl_fn_compile(struct pdmtl_fn *fn);

// present the most recent OpenGL draw through Mesa's native Radeon bridge.
// The caller owns the display, unsupported presenters return -1.
int pdmtl_present_native(const struct pdmtl_tex *target);

// query the executor used for the last job.
int pdmtl_last_on_vulkan(void);
int pdmtl_last_on_opengl(void);

// raw access for test runners: the arena, allocations in it, a function's blob, and whole
// PD requests (struct PDComputeRequest, PDDrawRequest, PDTileRequest from airjitd's bridge.h)
uint8_t *pdmtl_arena(void);
uint32_t pdmtl_arena_size(void);
uint32_t pdmtl_alloc(size_t len);
void pdmtl_fn_ref(const struct pdmtl_fn *fn, uint32_t *id, uint32_t *off, uint32_t *size);
int pdmtl_exec_compute(void *req);
int pdmtl_exec_draw(void *req);
int pdmtl_exec_tile(void *req);

// every pixel of a texture in the arena set to one colour, in the texture's format
void pdmtl_fill(const struct PDDrawTex *t, const float rgba[4]);

// a depth/stencil texture's depth and stencil set (which: PDMTL_CLEAR_*), as a pass's clear does
#define PDMTL_CLEAR_DEPTH 1
#define PDMTL_CLEAR_STENCIL 2
void pdmtl_fill_depth(const struct PDDrawTex *t, float depth, unsigned stencil, int which);

// resident resources on the Vulkan executor (vk.h): callers then mark texture bytes they write (pdmtl_dirty, while
// buffers are compared by content) and bring GPU-written bytes back before reading them (pdmtl_sync)
void pdmtl_resident(int on);
void pdmtl_dirty(uint32_t off, uint32_t len);
void pdmtl_sync(uint32_t off, uint32_t len);
void pdmtl_sync_all(void);
// the end of a command buffer: its batch is submitted, and buffers the GPU wrote come back
void pdmtl_finish(void);

// levels 1 and up from level 0 by a 2x2 box filter, every slice (generateMipmapsForTexture)
struct PDTexLayout;
void pdmtl_generate_mipmaps(const struct PDDrawTex *t, const struct PDTexLayout *l);

#endif
