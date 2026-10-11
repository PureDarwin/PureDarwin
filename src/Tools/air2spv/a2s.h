#ifndef AIR2SPV_A2S_H
#define AIR2SPV_A2S_H

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <setjmp.h>

#include "air2spv.h"
#include "spv.h"

#define A2S_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// where a pointer can point: a word array we declared for a buffer, threadgroup or private memory
enum root_kind { ROOT_BUFFER, ROOT_WG, ROOT_FN, ROOT_PRIVATE };

struct root {
  enum root_kind kind;
  uint32_t var, sc, words;
  // buffers: the push constant word with the binding's byte offset, 0 when none
  uint32_t pc_off;
  // buffers: binding, read only, and the uvec2/uvec4 views of the same binding for wide loads
  uint32_t binding, ro, view[5];
};

// a pointer: root plus a byte offset, dyn (an id, 0 when none) + cst
struct aptr {
  int root;
  uint32_t dyn, cst;
};

// a texture or sampler handle: the variable and its pointee type
struct handle {
  uint32_t var, type;
  int res;
  // a storage texture written at other mip levels: an array of one view per level
  uint32_t levels;
  // textures: 2D array, cube (2) or cube array (3), 3D, a cube read by texel is bound as a 2D array
  int arrayed, cube, three, cube_as_array;
  // samplers addressing texels: a constexpr one says so itself, a bound one through a spec constant
  int pixel_const;
  uint32_t pixel_spec;
};

#define MAX_ROOTS 64
#define MAX_HANDLES 64

struct blk {
  LLVMBasicBlockRef bb;
  uint32_t label;
  int idom, ipdom, reach, emitted, merged, header, exit, npred;
  int *succ, nsucc;
  // natural loop body of a header, a bitset over block indices
  uint64_t *body;
};

struct a2s {
  struct spv s;
  const struct a2s_options *opt;
  struct a2s_reflect *r;
  LLVMContextRef ctx;
  LLVMModuleRef mod;
  LLVMTargetDataRef td;
  LLVMValueRef fn;
  enum a2s_stage stage;
  jmp_buf fail;
  char *err;
  size_t errlen;
  int structurize;

  // maps LLVMValueRef to id, to pointer, to handle: one open addressing table
  struct vslot {
    LLVMValueRef k;
    uint32_t id;
    int kind;
    union {
      struct aptr p;
      struct handle h;
    } u;
  } *vt;
  size_t vt_cap, vt_n;

  struct root roots[MAX_ROOTS];
  int nroots;
  // constant address space globals with their bytes in a Private root each
  LLVMValueRef cglobal[MAX_ROOTS];
  int croot[MAX_ROOTS], ncglobal;

  // function body pieces: variables, the prologue in the first block, then the blocks
  struct spv_buf vars, pro, body;
  uint32_t fn_id, first_label, glsl, pc_var, pc_struct;
  uint32_t iface[160];
  unsigned niface;

  struct blk *b;
  int nb, nwords;
  // instructions emitted a second time for regions entered from two places
  unsigned dup_used;
  // the block whose terminator is being lowered
  int cur;
  // tile functions: WorkgroupId input and the tile size spec constants, attachment images
  uint32_t tile_wgid, tile_size;
  struct handle att[8];
  int uses_barrier, uses_demote, uses_quad, shader_barrier, lane_exchange, tile_quads;
  // output variables of the entry and how its return value spreads over them
  struct {
    uint32_t var, type;
    int member, builtin, skip;
    LLVMTypeRef lty;
  } outs[16];
  int nouts;
  // pushed constants used so far: words
  uint32_t pc_ids[A2S_MAX_PC];
  struct {
    uint32_t bi, var;
  } builtins[16];
  int nbuiltins;
  // kernels using quads: the local and global position and quad lane in quad-major thread order
  uint32_t qlocal, qglobal, qlane, quad_mem;
  // kernels: the physical workgroup size and the logical (Metal) group position, group, groups, index
  uint32_t wg_phys, lpos, lgroup, lgroups, lindex, lsize;
  // explicit imageblock width (spec 5) as used, computed once
  uint32_t ib_spec, ib_w;
  // kernel: workgroup size spec composite and the grid in push constants
  uint32_t wg_size;
};

enum { VK_ID = 1, VK_PTR, VK_HANDLE, VK_AGG };

_Noreturn void a2s_fail(struct a2s *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void a2s_note(struct a2s *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

uint32_t a2s_type(struct a2s *a, LLVMTypeRef t);
uint32_t a2s_value(struct a2s *a, LLVMValueRef v);
void a2s_set(struct a2s *a, LLVMValueRef v, uint32_t id);
struct vslot *a2s_slot(struct a2s *a, LLVMValueRef k, int create);
struct aptr a2s_ptr(struct a2s *a, LLVMValueRef v);
// a pointer's byte offset in its memory, null (an optional pointer the shader tests) is A2S_NULL_BYTES
#define A2S_NULL_BYTES 0xffffffffu
uint32_t a2s_ptr_bytes(struct a2s *a, LLVMValueRef v);

// one instruction in the body: returns the result id (allocated when has_result)
uint32_t a2s_op(struct a2s *a, unsigned op, uint32_t type, const uint32_t *ops, unsigned n);
uint32_t a2s_op1(struct a2s *a, unsigned op, uint32_t type, uint32_t x);
uint32_t a2s_op2(struct a2s *a, unsigned op, uint32_t type, uint32_t x, uint32_t y);
uint32_t a2s_ext(struct a2s *a, unsigned glsl_op, uint32_t type, const uint32_t *args, unsigned n);
void a2s_stmt(struct a2s *a, unsigned op, const uint32_t *ops, unsigned n);

int a2s_bits(LLVMTypeRef t);
LLVMTypeRef a2s_scalar(LLVMTypeRef t);
unsigned a2s_lanes(LLVMTypeRef t);
uint32_t a2s_uconst(struct a2s *a, LLVMTypeRef t, uint32_t v);
uint32_t a2s_fconst(struct a2s *a, LLVMTypeRef t, float v);
uint32_t a2s_mask(struct a2s *a, LLVMTypeRef t, uint32_t v);
uint32_t a2s_sext(struct a2s *a, LLVMTypeRef t, uint32_t v);
uint32_t a2s_bool_of(struct a2s *a, LLVMTypeRef t, uint32_t v);
uint32_t a2s_quantize(struct a2s *a, LLVMTypeRef t, uint32_t v);
uint32_t a2s_load_mem(struct a2s *a, LLVMTypeRef t, struct aptr p, unsigned align);
void a2s_store_mem(struct a2s *a, LLVMTypeRef t, uint32_t v, struct aptr p, unsigned align);
uint32_t a2s_bitcast(struct a2s *a, LLVMTypeRef from, LLVMTypeRef to, uint32_t v);
uint32_t a2s_pc_word(struct a2s *a, enum a2s_pc_kind kind, uint32_t metal);

// interface: entry parameters, outputs and resources from the air.* metadata
void a2s_interface(struct a2s *a);
// loads from pinned constant buffers folded to their bytes, returns how many
unsigned a2s_pin_loads(struct a2s *a);
void a2s_return(struct a2s *a, LLVMValueRef v);
uint32_t a2s_input_var(struct a2s *a, uint32_t type, SpvBuiltIn bi, int location);
uint32_t a2s_logical_group(struct a2s *a);
uint32_t a2s_glsl(struct a2s *a);
uint32_t a2s_word_ptr(struct a2s *a, int root, uint32_t idx);
uint32_t a2s_logical_groups(struct a2s *a);
struct handle a2s_handle(struct a2s *a, LLVMValueRef v);
int a2s_add_root(struct a2s *a, enum root_kind kind, uint32_t words);

// air.* calls, 0 when the name is not one
int a2s_air_call(struct a2s *a, LLVMValueRef call, const char *name);

// control flow: blocks in structured order
void a2s_cfg(struct a2s *a);
void a2s_inst(struct a2s *a, LLVMValueRef in);

#endif
