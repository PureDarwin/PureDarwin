#ifndef AIR2SPV_SPV_H
#define AIR2SPV_SPV_H

#include <stddef.h>
#include <stdint.h>
#include <spirv/unified1/spirv.h>

// a growable run of SPIR-V words
struct spv_buf {
  uint32_t *w;
  size_t n, cap;
};

// the module sections in the order the binary needs them, and the type/constant dedupe table
struct spv {
  uint32_t bound;
  struct spv_buf caps, exts, imports, model, entry, modes, debug, annot, globals, funcs;
  // open addressing over (op, operands) of types and constants: index into keys
  struct spv_key {
    uint32_t hash, id, off, len;
  } *tab;
  size_t tab_cap, tab_n;
  struct spv_buf keys;
  uint32_t glsl;
  uint64_t capset[2];
};

void spv_init(struct spv *s);
void spv_free(struct spv *s);
uint32_t spv_id(struct spv *s);
void spv_append(struct spv_buf *d, const struct spv_buf *s);
void spv_emit(struct spv_buf *b, unsigned op, const uint32_t *ops, unsigned n);
// op with operands given inline, terminated by count
void spv_op(struct spv_buf *b, unsigned op, unsigned n, ...);
// a string as literal words appended to w (returns the word count)
unsigned spv_str(uint32_t *w, const char *s);

void spv_cap(struct spv *s, SpvCapability c);
void spv_ext(struct spv *s, const char *name);
void spv_name(struct spv *s, uint32_t id, const char *name);
void spv_deco(struct spv *s, uint32_t id, SpvDecoration d, int nlit, ...);
void spv_mdeco(struct spv *s, uint32_t id, uint32_t member, SpvDecoration d, int nlit, ...);

// interned global instructions: types and constants, result id at the position given
uint32_t spv_intern(struct spv *s, unsigned op, const uint32_t *ops, unsigned n, int has_type);

uint32_t spv_t_void(struct spv *s);
uint32_t spv_t_bool(struct spv *s);
uint32_t spv_t_uint(struct spv *s);
uint32_t spv_t_int(struct spv *s);
uint32_t spv_t_float(struct spv *s);
uint32_t spv_t_vec(struct spv *s, uint32_t elem, unsigned n);
uint32_t spv_t_ptr(struct spv *s, SpvStorageClass sc, uint32_t t);
uint32_t spv_t_fn(struct spv *s, uint32_t ret);
uint32_t spv_t_array(struct spv *s, uint32_t elem, uint32_t len_id);
uint32_t spv_t_struct(struct spv *s, const uint32_t *members, unsigned n);
// a struct that is never interned: blocks need their own decorations
uint32_t spv_t_struct_unique(struct spv *s, const uint32_t *members, unsigned n);
uint32_t spv_t_rtarray_unique(struct spv *s, uint32_t elem);
uint32_t spv_t_image(struct spv *s, uint32_t sampled_type, SpvDim dim, unsigned sampled, SpvImageFormat f);
uint32_t spv_t_image_arrayed(struct spv *s, uint32_t sampled_type, SpvDim dim, unsigned arrayed, unsigned sampled, SpvImageFormat f);
uint32_t spv_t_sampler(struct spv *s);
uint32_t spv_t_sampled(struct spv *s, uint32_t image);

uint32_t spv_c_u32(struct spv *s, uint32_t v);
uint32_t spv_c_i32(struct spv *s, int32_t v);
uint32_t spv_c_f32(struct spv *s, float v);
uint32_t spv_c_bool(struct spv *s, int v);
uint32_t spv_c_null(struct spv *s, uint32_t type);
uint32_t spv_c_undef(struct spv *s, uint32_t type);
uint32_t spv_c_comp(struct spv *s, uint32_t type, const uint32_t *parts, unsigned n);

// a global OpVariable (not interned)
uint32_t spv_var(struct spv *s, uint32_t ptr_type, SpvStorageClass sc);

// the finished binary, malloc'd
uint32_t *spv_finish(struct spv *s, size_t *nwords);

#endif
