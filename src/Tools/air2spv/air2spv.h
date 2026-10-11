#ifndef AIR2SPV_H
#define AIR2SPV_H

#include <stddef.h>
#include <stdint.h>

// AIR (the LLVM bitcode inside a metallib) to Vulkan SPIR-V. a2s_reflect is the resource model the
// executor must follow for one shader: bindings, push constant words, spec constants, varyings
enum a2s_stage { A2S_VERTEX, A2S_FRAGMENT, A2S_KERNEL, A2S_TILE };

enum a2s_res_kind {
  A2S_RES_BUFFER,
  A2S_RES_TEXTURE,
  A2S_RES_SAMPLER,
  // a constexpr sampler from the shader itself: state holds AIR's 64 bit encoding
  A2S_RES_CONST_SAMPLER,
  // framebuffer fetch: the colour attachment index is in metal
  A2S_RES_INPUT_ATTACHMENT,
  // a tile function's implicit imageblock: colour attachment metal as a storage image
  A2S_RES_ATTACHMENT_IMAGE,
};

enum a2s_access { A2S_READ = 1, A2S_WRITE = 2 };

struct a2s_resource {
  enum a2s_res_kind kind;
  uint32_t metal, set, binding, access;
  // textures: 2 or 3 dimensions, element kind 'f' 'u' 's', storage (1) or sampled (0), arrayed for array
  // and cube array textures, cube for cubes (a cube the shader reads by texel is a 2D array of 6 faces)
  uint32_t dim, storage, arrayed, cube;
  char elem;
  // a storage texture written at mip levels other than 0 is an array of this many views, one per level
  uint32_t levels;
  // bound samplers: the bool spec constant that is true when the sampler addresses texels and false for 0..1
  uint32_t spec_id;
  // written float textures: the id of the bool spec constant 48 + metal (normalized format), 0 none
  uint32_t unorm_spec;
  // storage textures: the id of the bool spec constant 64 + metal (BGRA bytes bound as RGBA), 0 none
  uint32_t bgra_spec;
  uint64_t state;
};

struct a2s_varying {
  char name[96];
  uint32_t location, components;
  // 'f' float (half widened), 'u' unsigned, 's' signed
  char elem;
  // 0 perspective, 1 no perspective, 2 flat
  uint32_t interp;
  // the word holding the Location literal, for a2s_relocate
  uint32_t word;
};

// a vertex function's stage_in attribute: Location = the attribute index, a 4 lane input of elem
struct a2s_vin {
  uint32_t location;
  char elem;
};

struct a2s_output {
  uint32_t target, components;
  char elem;
};

// push constant words the executor fills: each a buffer's byte offset or the dispatch grid
enum a2s_pc_kind { A2S_PC_BUFFER_OFFSET, A2S_PC_GRID_X, A2S_PC_GRID_Y, A2S_PC_GRID_Z };

struct a2s_pc {
  enum a2s_pc_kind kind;
  uint32_t metal;
};

#define A2S_MAX_RES 64
#define A2S_MAX_VARYINGS 32
#define A2S_MAX_PC 32

struct a2s_reflect {
  enum a2s_stage stage;
  char entry[96];
  struct a2s_resource res[A2S_MAX_RES];
  unsigned nres;
  struct a2s_varying var[A2S_MAX_VARYINGS];
  unsigned nvar;
  struct a2s_output out[8];
  unsigned nout;
  // push constant block: byte offset of word 0 in the pipeline's range, then the words
  uint32_t pc_base;
  struct a2s_pc pc[A2S_MAX_PC];
  unsigned npc;
  // kernels: spec constant ids 0 1 2 are the threadgroup size, guard says threads outside the grid
  // in the push constants return at once (only without barriers)
  int guard, barrier;
  // threadgroup memory parameters [[threadgroup(i)]]: bit i set when used, spec constant 12 + i is the
  // length in words
  uint32_t tg_params;
  // threads run in quad-major order (the kernel uses quad operations): see DESIGN
  int quad_order;
  // the threadgroup shares state (barriers, threadgroup memory, imageblocks, quads, tiles): it must run
  // as one Vulkan workgroup. Without it a group may be split into smaller workgroups (spec 8-10)
  int group_shared;
  // the alignment every pushed buffer offset must keep (16 when wide loads are used, else 4)
  uint32_t offset_align;
  // imageblock bytes per thread (explicit layout), 0 when none
  uint32_t imageblock_bytes;
  // vertex functions: their stage_in attributes
  struct a2s_vin vin[32];
  unsigned nvin;
  // the pinned constant buffer bytes the translation folded in: a specialized shader is only valid for
  // draws whose buffers hold the same values there
  struct { uint32_t metal, off, size; } pinned[128];
  unsigned npinned;
  // capabilities the module declared beyond Shader, comma separated, and the notes
  char needs[256];
  char notes[512];
};

struct a2s_options {
  // explicit half to float narrowing rounds through OpQuantizeToF16
  int quantize_half;
  // descriptor set of this stage's resources and the push constant base offset
  uint32_t set, pc_base;
  // the lowering prints what it does to stderr
  int verbose;
  // kernels exchange quad values through workgroup memory in place of subgroup quad operations
  int quad_memory;
  // no uvec2/uvec4 loads from buffers: for bindings whose pushed offset is not a multiple of 16
  int narrow_loads;
  // texture views per mip level are picked by a switch over constant indices, for devices without
  // shaderStorageImageArrayDynamicIndexing
  int no_dynamic_index;
  // discard_fragment becomes OpKill in its own block, for devices without shaderDemoteToHelperInvocation
  int kill_discard;
  // constant buffers whose contents are known for this pipeline: loads at constant offsets become those
  // bytes and the branches they decide are pruned (an uber shader shrinks to the configuration in use)
  const struct a2s_pin *pins;
  unsigned npins;
  // OpenGL backends without shader storage buffers: read-only Metal buffers as bounded std140 uint arrays (one word per 16 bytes),
  // indexed by Metal slot. A used slot needs a nonzero length, and wide storage-buffer views are disabled.
  const uint32_t *uniform_words;
};

struct a2s_pin {
  uint32_t metal;
  const uint8_t *data;
  uint32_t size;
};

// AIR's 64 bit constexpr sampler state, decoded (layout inferred from the shaders' own values)
struct a2s_sampler_desc {
  // MSL address: 0 clamp_to_zero, 1 clamp_to_edge, 2 repeat, 3 mirrored_repeat, 4 clamp_to_border
  unsigned addr[3];
  // filters: 0 nearest, 1 linear, mip 0 none, 1 nearest, 2 linear
  unsigned mag, min, mip;
  // coord::pixel
  int pixel;
};

static inline void a2s_sampler_state(uint64_t s, struct a2s_sampler_desc *d) {
  d->addr[0] = s & 7, d->addr[1] = (s >> 3) & 7, d->addr[2] = (s >> 6) & 7;
  d->mag = (s >> 9) & 3, d->min = (s >> 11) & 3, d->mip = (s >> 13) & 3;
  d->pixel = (s >> 15) & 1;
}

// the same state as the kext's sampler flags word (bridge.h), which both paths take, bit 30 keeps a
// constexpr sampler's word from reading as 0, unknown
static inline uint32_t a2s_sampler_flags(uint64_t s) {
  static const uint8_t mode[8] = { 4, 0, 2, 3, 5, 0, 0, 0 };
  struct a2s_sampler_desc d;

  a2s_sampler_state(s, &d);
  return (d.min ? 1u : 0) | (d.mag ? 4u : 0) | (uint32_t)mode[d.addr[0]] << 8 | (uint32_t)mode[d.addr[1]] << 12 |
         (uint32_t)mode[d.addr[2]] << 16 | (d.pixel ? 0 : 0x80000000u) | 0x40000000u;
}

// spec constant ids: 0-2 the threadgroup size, 8-10 the workgroup size, 3-4 a tile function's tile size, 16 + i bound sampler i
// addresses texels
#define A2S_SPEC_SAMPLER_PIXEL 16
// 8-10: the Vulkan workgroup size when a Metal threadgroup runs as several workgroups (else = 0-2)
#define A2S_SPEC_PHYS_SIZE 8
// 5: a kernel's explicit imageblock width (setImageblockWidth), 0 = the threadgroup width
#define A2S_SPEC_IMAGEBLOCK 5
// 12 + i: [[threadgroup(i)]] memory's length in 32 bit words (setThreadgroupMemoryLength / 4)
#define A2S_SPEC_TGMEM 12
// 48 + i: written texture i has a normalized format (NaN is written as 0, as Metal does)
#define A2S_SPEC_UNORM 48
// 64 + i: storage texture i holds BGRA bytes in an RGBA image (devices without BGRA storage), reads and
// writes swap red and blue
#define A2S_SPEC_BGRA 64

// translates the entry function of one metallib. words is malloc'd, errors go to err
int a2s_translate(const uint8_t *mtl, uint32_t size, const struct a2s_options *opt, uint32_t **words,
                  size_t *nwords, struct a2s_reflect *refl, char *err, size_t errlen);

// a fragment shader's input locations rewritten to the vertex outputs with the same names: an input
// with no match keeps its location. Returns how many matched
unsigned a2s_relocate(uint32_t *frag_words, const struct a2s_reflect *frag, const struct a2s_reflect *vert);

// one reflection as JSON
void a2s_reflect_json(const struct a2s_reflect *r, void *file);

#endif
