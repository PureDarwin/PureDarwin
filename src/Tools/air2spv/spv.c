#include "spv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put(struct spv_buf *b, uint32_t w) {
  if (b->n == b->cap) {
    b->cap = b->cap ? b->cap * 2 : 256;
    b->w = realloc(b->w, b->cap * sizeof(uint32_t));
  }

  b->w[b->n++] = w;
}

void spv_append(struct spv_buf *d, const struct spv_buf *s) {
  for (size_t i = 0; i < s->n; i++) {
    put(d, s->w[i]);
  }
}

void spv_init(struct spv *s) {
  memset(s, 0, sizeof(*s));
  s->bound = 1;
  s->tab_cap = 1024;
  s->tab = calloc(s->tab_cap, sizeof(*s->tab));
}

void spv_free(struct spv *s) {
  struct spv_buf *bs[] = { &s->caps, &s->exts,  &s->imports, &s->model, &s->entry, &s->modes,
                           &s->debug, &s->annot, &s->globals, &s->funcs, &s->keys };

  for (unsigned i = 0; i < sizeof(bs) / sizeof(bs[0]); i++) {
    free(bs[i]->w);
  }

  free(s->tab);
}

uint32_t spv_id(struct spv *s) {
  return s->bound++;
}

void spv_emit(struct spv_buf *b, unsigned op, const uint32_t *ops, unsigned n) {
  put(b, (n + 1) << 16 | op);
  for (unsigned i = 0; i < n; i++) {
    put(b, ops[i]);
  }
}

void spv_op(struct spv_buf *b, unsigned op, unsigned n, ...) {
  uint32_t w[32];
  va_list ap;

  va_start(ap, n);
  for (unsigned i = 0; i < n && i < 32; i++) {
    w[i] = va_arg(ap, uint32_t);
  }
  va_end(ap);
  spv_emit(b, op, w, n);
}

unsigned spv_str(uint32_t *w, const char *s) {
  size_t len = strlen(s) + 1, n = (len + 3) / 4;

  memset(w, 0, n * 4);
  memcpy(w, s, len);
  return (unsigned)n;
}

void spv_cap(struct spv *s, SpvCapability c) {
  // capabilities above 127 are kept in a small list, the rest in the bitset
  if (c < 128) {
    if (s->capset[c / 64] & 1ull << (c % 64))
      return;

    s->capset[c / 64] |= 1ull << (c % 64);
  } else {
    for (size_t i = 0; i < s->caps.n; i += 2) {
      if (s->caps.w[i + 1] == (uint32_t)c)
        return;
    }
  }

  spv_op(&s->caps, SpvOpCapability, 1, c);
}

void spv_ext(struct spv *s, const char *name) {
  uint32_t w[16];
  unsigned n = spv_str(w, name);

  // linear scan of what is there: a module names a handful at most
  for (size_t i = 0; i < s->exts.n; i += s->exts.w[i] >> 16) {
    if ((s->exts.w[i] >> 16) - 1 == n && !memcmp(&s->exts.w[i + 1], w, n * 4))
      return;
  }

  spv_emit(&s->exts, SpvOpExtension, w, n);
}

void spv_name(struct spv *s, uint32_t id, const char *name) {
  uint32_t w[64];
  char tmp[200];

  snprintf(tmp, sizeof(tmp), "%s", name);
  w[0] = id;
  spv_emit(&s->debug, SpvOpName, w, 1 + spv_str(w + 1, tmp));
}

void spv_deco(struct spv *s, uint32_t id, SpvDecoration d, int nlit, ...) {
  uint32_t w[8] = { id, d };
  va_list ap;

  va_start(ap, nlit);
  for (int i = 0; i < nlit && i < 6; i++) {
    w[2 + i] = va_arg(ap, uint32_t);
  }
  va_end(ap);
  spv_emit(&s->annot, SpvOpDecorate, w, 2 + (unsigned)nlit);
}

void spv_mdeco(struct spv *s, uint32_t id, uint32_t member, SpvDecoration d, int nlit, ...) {
  uint32_t w[8] = { id, member, d };
  va_list ap;

  va_start(ap, nlit);
  for (int i = 0; i < nlit && i < 5; i++) {
    w[3 + i] = va_arg(ap, uint32_t);
  }
  va_end(ap);
  spv_emit(&s->annot, SpvOpMemberDecorate, w, 3 + (unsigned)nlit);
}

static uint32_t hash_words(unsigned op, const uint32_t *w, unsigned n) {
  uint32_t h = 2166136261u ^ op;

  for (unsigned i = 0; i < n; i++) {
    h = (h ^ w[i]) * 16777619u;
  }

  return h ? h : 1;
}

static void grow_tab(struct spv *s) {
  struct spv_key *old = s->tab;
  size_t oc = s->tab_cap;

  s->tab_cap *= 2;
  s->tab = calloc(s->tab_cap, sizeof(*s->tab));
  for (size_t i = 0; i < oc; i++) {
    if (old[i].hash) {
      size_t j = old[i].hash & (s->tab_cap - 1);
      while (s->tab[j].hash) {
        j = (j + 1) & (s->tab_cap - 1);
      }
      s->tab[j] = old[i];
    }
  }

  free(old);
}

// the key is op then the operands with the result id left out
uint32_t spv_intern(struct spv *s, unsigned op, const uint32_t *ops, unsigned n, int has_type) {
  uint32_t key[64], h, id, w[64];
  unsigned kn = 0, i;
  size_t j;

  key[kn++] = op;
  for (i = 0; i < n && kn < 64; i++) {
    key[kn++] = ops[i];
  }

  h = hash_words(op, key, kn);
  for (j = h & (s->tab_cap - 1); s->tab[j].hash; j = (j + 1) & (s->tab_cap - 1)) {
    if (s->tab[j].hash == h && s->tab[j].len == kn && !memcmp(&s->keys.w[s->tab[j].off], key, kn * 4))
      return s->tab[j].id;
  }

  id = spv_id(s);
  s->tab[j].hash = h, s->tab[j].id = id, s->tab[j].off = (uint32_t)s->keys.n, s->tab[j].len = kn;
  for (i = 0; i < kn; i++) {
    put(&s->keys, key[i]);
  }

  if (++s->tab_n * 2 > s->tab_cap)
    grow_tab(s);

  // types put the result first, constants after the result type
  if (has_type) {
    w[0] = ops[0], w[1] = id;
    memcpy(w + 2, ops + 1, (n - 1) * 4);
  } else {
    w[0] = id;
    memcpy(w + 1, ops, n * 4);
  }

  spv_emit(&s->globals, op, w, n + 1);
  return id;
}

uint32_t spv_t_void(struct spv *s) {
  return spv_intern(s, SpvOpTypeVoid, 0, 0, 0);
}

uint32_t spv_t_bool(struct spv *s) {
  return spv_intern(s, SpvOpTypeBool, 0, 0, 0);
}

uint32_t spv_t_uint(struct spv *s) {
  uint32_t w[2] = { 32, 0 };
  return spv_intern(s, SpvOpTypeInt, w, 2, 0);
}

uint32_t spv_t_int(struct spv *s) {
  uint32_t w[2] = { 32, 1 };
  return spv_intern(s, SpvOpTypeInt, w, 2, 0);
}

uint32_t spv_t_float(struct spv *s) {
  uint32_t w[1] = { 32 };
  return spv_intern(s, SpvOpTypeFloat, w, 1, 0);
}

uint32_t spv_t_vec(struct spv *s, uint32_t elem, unsigned n) {
  uint32_t w[2] = { elem, n };
  return n == 1 ? elem : spv_intern(s, SpvOpTypeVector, w, 2, 0);
}

uint32_t spv_t_ptr(struct spv *s, SpvStorageClass sc, uint32_t t) {
  uint32_t w[2] = { sc, t };
  return spv_intern(s, SpvOpTypePointer, w, 2, 0);
}

uint32_t spv_t_fn(struct spv *s, uint32_t ret) {
  uint32_t w[1] = { ret };
  return spv_intern(s, SpvOpTypeFunction, w, 1, 0);
}

uint32_t spv_t_array(struct spv *s, uint32_t elem, uint32_t len_id) {
  uint32_t w[2] = { elem, len_id };
  return spv_intern(s, SpvOpTypeArray, w, 2, 0);
}

uint32_t spv_t_struct(struct spv *s, const uint32_t *members, unsigned n) {
  return spv_intern(s, SpvOpTypeStruct, members, n, 0);
}

uint32_t spv_t_struct_unique(struct spv *s, const uint32_t *members, unsigned n) {
  uint32_t w[64], id = spv_id(s);

  w[0] = id;
  memcpy(w + 1, members, n * 4);
  spv_emit(&s->globals, SpvOpTypeStruct, w, n + 1);
  return id;
}

uint32_t spv_t_rtarray_unique(struct spv *s, uint32_t elem) {
  uint32_t id = spv_id(s);

  spv_op(&s->globals, SpvOpTypeRuntimeArray, 2, id, elem);
  return id;
}

uint32_t spv_t_image(struct spv *s, uint32_t sampled_type, SpvDim dim, unsigned sampled, SpvImageFormat f) {
  return spv_t_image_arrayed(s, sampled_type, dim, 0, sampled, f);
}

uint32_t spv_t_image_arrayed(struct spv *s, uint32_t sampled_type, SpvDim dim, unsigned arrayed, unsigned sampled, SpvImageFormat f) {
  uint32_t w[7] = { sampled_type, dim, 0, arrayed, 0, sampled, f };
  return spv_intern(s, SpvOpTypeImage, w, 7, 0);
}

uint32_t spv_t_sampler(struct spv *s) {
  return spv_intern(s, SpvOpTypeSampler, 0, 0, 0);
}

uint32_t spv_t_sampled(struct spv *s, uint32_t image) {
  return spv_intern(s, SpvOpTypeSampledImage, &image, 1, 0);
}

uint32_t spv_c_u32(struct spv *s, uint32_t v) {
  uint32_t w[2] = { spv_t_uint(s), v };
  return spv_intern(s, SpvOpConstant, w, 2, 1);
}

uint32_t spv_c_i32(struct spv *s, int32_t v) {
  uint32_t w[2] = { spv_t_int(s), (uint32_t)v };
  return spv_intern(s, SpvOpConstant, w, 2, 1);
}

uint32_t spv_c_f32(struct spv *s, float v) {
  uint32_t w[2] = { spv_t_float(s), 0 };

  memcpy(&w[1], &v, 4);
  return spv_intern(s, SpvOpConstant, w, 2, 1);
}

uint32_t spv_c_bool(struct spv *s, int v) {
  uint32_t w[1] = { spv_t_bool(s) };
  return spv_intern(s, v ? SpvOpConstantTrue : SpvOpConstantFalse, w, 1, 1);
}

uint32_t spv_c_null(struct spv *s, uint32_t type) {
  return spv_intern(s, SpvOpConstantNull, &type, 1, 1);
}

uint32_t spv_c_undef(struct spv *s, uint32_t type) {
  return spv_intern(s, SpvOpUndef, &type, 1, 1);
}

uint32_t spv_c_comp(struct spv *s, uint32_t type, const uint32_t *parts, unsigned n) {
  uint32_t w[64] = { type };

  memcpy(w + 1, parts, n * 4);
  return spv_intern(s, SpvOpConstantComposite, w, n + 1, 1);
}

uint32_t spv_var(struct spv *s, uint32_t ptr_type, SpvStorageClass sc) {
  uint32_t id = spv_id(s);

  spv_op(&s->globals, SpvOpVariable, 3, ptr_type, id, sc);
  return id;
}

uint32_t *spv_finish(struct spv *s, size_t *nwords) {
  struct spv_buf out = { 0 };
  struct spv_buf *secs[] = { &s->caps, &s->exts,  &s->imports, &s->model,   &s->entry,
                             &s->modes, &s->debug, &s->annot,   &s->globals, &s->funcs };

  put(&out, SpvMagicNumber);
  // SPIR-V 1.5, what Vulkan 1.2 consumes
  put(&out, 0x00010500);
  put(&out, 0);
  put(&out, s->bound);
  put(&out, 0);

  for (unsigned i = 0; i < sizeof(secs) / sizeof(secs[0]); i++) {
    spv_append(&out, secs[i]);
  }

  *nwords = out.n;
  return out.w;
}
