#define _DARWIN_C_SOURCE 1

#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl.h"
#include "glsl.h"
#include "../texture.h"
#include "../blend.h"
#include "../airjitd.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLContext context = EGL_NO_CONTEXT;
static EGLSurface surface = EGL_NO_SURFACE;
static int state, hardware, compute_supported;
static GLint max_block, max_bindings, max_texture, max_units;
static char reason[512], renderer[256];

static GLuint last_fbo, last_texture;
static uint32_t last_target, last_w, last_h, last_pitch, last_format;
static uint8_t *last_base;
static int resident_mode, gpu_newer, cpu_newer;
static GLuint present_pbo[2];
static size_t present_pbo_size[2];
static int present_pending_pbo = -1, present_next_pbo;
static void *present_pending_pixels;
static uint32_t present_pending_pitch, present_pending_w, present_pending_h;
static int present_pending_565;

struct gl_format {
  GLenum internal, format, type;
};

static struct gl_format color_format(unsigned f) {
  switch (f) {
  case PD_FMT_BGRA8:
    return (struct gl_format){ GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE };
  case PD_FMT_RGBA8:
    return (struct gl_format){ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
  case PD_FMT_BGRA8_SRGB:
    return (struct gl_format){ GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE };
  case PD_FMT_RGBA8_SRGB:
    return (struct gl_format){ GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE };
  case PD_FMT_RGBA16F:
    return (struct gl_format){ GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT };
  case PD_FMT_R16F:
    return (struct gl_format){ GL_R16F, GL_RED, GL_HALF_FLOAT };
  case PD_FMT_RG16F:
    return (struct gl_format){ GL_RG16F, GL_RG, GL_HALF_FLOAT };
  case PD_FMT_R32F:
    return (struct gl_format){ GL_R32F, GL_RED, GL_FLOAT };
  case PD_FMT_RG32F:
    return (struct gl_format){ GL_RG32F, GL_RG, GL_FLOAT };
  case PD_FMT_RGBA32F:
    return (struct gl_format){ GL_RGBA32F, GL_RGBA, GL_FLOAT };
  case PD_FMT_R8:
    return (struct gl_format){ GL_R8, GL_RED, GL_UNSIGNED_BYTE };
  case PD_FMT_RG8:
    return (struct gl_format){ GL_RG8, GL_RG, GL_UNSIGNED_BYTE };
  case PD_FMT_R16:
    return (struct gl_format){ GL_R16, GL_RED, GL_UNSIGNED_SHORT };
  case PD_FMT_RG16:
    return (struct gl_format){ GL_RG16, GL_RG, GL_UNSIGNED_SHORT };
  case PD_FMT_RGBA16:
    return (struct gl_format){ GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT };
  case PD_FMT_R16UI:
    return (struct gl_format){ GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT };
  case PD_FMT_RG16UI:
    return (struct gl_format){ GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT };
  case PD_FMT_RGBA16UI:
    return (struct gl_format){ GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT };
  case PD_FMT_RGBA8UI:
    return (struct gl_format){ GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE };
  case PD_FMT_R32UI:
    return (struct gl_format){ GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT };
  case PD_FMT_RG32UI:
    return (struct gl_format){ GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT };
  case PD_FMT_RGB10A2:
    return (struct gl_format){ GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV };
  case PD_FMT_BGR10A2:
    return (struct gl_format){ GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV };
  default:
    return (struct gl_format){ 0 };
  }
}

static int sync_target(void) {
  const char *read565;
  struct gl_format f;
  size_t row;
  uint8_t *p;
  int packed565;

  if (!gpu_newer || !last_fbo)
    return 0;

  // the 16-bit desktop scans out rgb565, so reading half the bytes is cheaper
  // opt-in because it drops the target's alpha channel
  read565 = getenv("AIRJITD_GL_READBACK_565");
  packed565 = last_format == PD_FMT_BGRA8 && read565 && !strcmp(read565, "1");
  row = (size_t)last_w * (packed565 ? 2 : tex_bpp(last_format));
  f = packed565 ? (struct gl_format){ GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5 } : color_format(last_format);

  if (!f.internal || !row)
    return -1;

  p = malloc(row * last_h);
  if (!p)
    return -1;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, last_fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, last_w, last_h, f.format, f.type, p);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  if (glGetError() != GL_NO_ERROR) {
    free(p);
    return -1;
  }

  for (unsigned y = 0; y < last_h; y++) {
    uint8_t *dst = last_base + last_target + (size_t)y * last_pitch;
    const uint8_t *src = p + (size_t)(last_h - 1 - y) * row;

    if (!packed565) {
      memcpy(dst, src, row);
    } else {
      for (unsigned x = 0; x < last_w; x++) {
        uint16_t v;
        unsigned blue, green, red;
        uint32_t bgra;

        memcpy(&v, src + x * 2, 2);
        blue = v & 31, green = (v >> 5) & 63, red = (v >> 11) & 31;
        bgra = 0xff000000u | ((red << 3 | red >> 2) << 16) | ((green << 2 | green >> 4) << 8) | (blue << 3 | blue >> 2);
        memcpy(dst + x * 4, &bgra, 4);
      }
    }
  }

  free(p);
  gpu_newer = 0;
  return 0;
}

static int overlaps_target(uint32_t off, uint32_t len) {
  return last_fbo && (uint64_t)off < (uint64_t)last_target + (uint64_t)last_pitch * last_h &&
         (uint64_t)last_target < (uint64_t)off + len;
}

const char *pd_gl_reason(void) {
  return reason;
}

const char *pd_gl_renderer(void) {
  return renderer;
}

int pd_gl_hardware(void) {
  return hardware;
}

int pd_gl_enabled(void) {
  const char *e = getenv("AIRJITD_OPENGL");

  return e && *e && strcmp(e, "0");
}

static int decline(const char *why) {
  snprintf(reason, sizeof(reason), "%s", why);
  return PD_GL_FALLBACK;
}

static int start(void) {
  EGLConfig config;
  EGLint count;
  const EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                           EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
  const EGLint ctx_attrs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2,
                               EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
  const EGLint surf_attrs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
  const EGLint compute_attrs[] = { EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3,
                                   EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
  const char *name, *allow;
  GLint major = 0, minor = 0;

  if (state)
    return state > 0 ? 0 : PD_GL_FALLBACK;

  state = -1;

  // pick an offscreen display without changing the platform glfw or appkit use for windows
  display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, 0);
  if (display == EGL_NO_DISPLAY)
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (display == EGL_NO_DISPLAY || !eglInitialize(display, 0, 0) || !eglBindAPI(EGL_OPENGL_API) ||
      !eglChooseConfig(display, attrs, &config, 1, &count) || !count)
    return decline("no native EGL OpenGL configuration");

  context = eglCreateContext(display, config, EGL_NO_CONTEXT, compute_attrs);
  if (context == EGL_NO_CONTEXT)
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attrs);

  surface = eglCreatePbufferSurface(display, config, surf_attrs);
  if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE || !eglMakeCurrent(display, surface, surface, context))
    return decline("cannot create OpenGL 3.2 context");

  name = (const char *)glGetString(GL_RENDERER);
  snprintf(renderer, sizeof(renderer), "%s", name ? name : "unknown");
  hardware = name && !strstr(name, "llvmpipe") && !strstr(name, "softpipe") && !strstr(name, "Software") &&
             !strstr(name, "SwiftShader");
  fprintf(stderr, "AIRJITD: OpenGL renderer %s (%s)\n", renderer, hardware ? "hardware" : "software");

  allow = getenv("AIRJITD_GL_ALLOW_SOFTWARE");
  if (!hardware && (!allow || strcmp(allow, "1")))
    return decline("software OpenGL renderer refused (validation only: AIRJITD_GL_ALLOW_SOFTWARE=1)");

  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_block);
  glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_bindings);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_units);
  if (max_block < 16 || max_bindings < 1 || max_texture < 1)
    return decline("invalid OpenGL resource limits");

  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  compute_supported = major > 4 || (major == 4 && minor >= 3);
  state = 1;
  return 0;
}

static int blend_state(uint32_t b, const float *constant) {
  static const GLenum factors[] = { GL_ZERO,
                                    GL_ONE,
                                    GL_SRC_COLOR,
                                    GL_ONE_MINUS_SRC_COLOR,
                                    GL_SRC_ALPHA,
                                    GL_ONE_MINUS_SRC_ALPHA,
                                    GL_DST_COLOR,
                                    GL_ONE_MINUS_DST_COLOR,
                                    GL_DST_ALPHA,
                                    GL_ONE_MINUS_DST_ALPHA,
                                    GL_SRC_ALPHA_SATURATE,
                                    GL_CONSTANT_COLOR,
                                    GL_ONE_MINUS_CONSTANT_COLOR,
                                    GL_CONSTANT_ALPHA,
                                    GL_ONE_MINUS_CONSTANT_ALPHA };
  static const GLenum ops[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX };
  unsigned sr, dr, sa, da, ro, ao;

  if (!(b & PD_BLEND_KNOWN)) {
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return 0;
  }

  if (!(b & PD_BLEND_ENABLED)) {
    glDisable(GL_BLEND);
    return 0;
  }

  sr = PD_BLEND_SRC_RGB(b), dr = PD_BLEND_DST_RGB(b);
  sa = PD_BLEND_SRC_A(b), da = PD_BLEND_DST_A(b);
  ro = PD_BLEND_OP_RGB(b), ao = PD_BLEND_OP_A(b);

  if (sr >= 15 || dr >= 15 || sa >= 15 || da >= 15 || ro >= 5 || ao >= 5 || dr == PD_BF_SRC_ALPHA_SATURATED ||
      da == PD_BF_SRC_ALPHA_SATURATED)
    return decline("unsupported blend factors");

  glEnable(GL_BLEND);
  glBlendColor(constant[0], constant[1], constant[2], constant[3]);
  glBlendFuncSeparate(factors[sr], factors[dr], factors[sa], factors[da]);
  glBlendEquationSeparate(ops[ro], ops[ao]);
  return 0;
}

static int span(uint32_t off, uint64_t bytes, uint32_t size) {
  return off <= size && bytes <= (uint64_t)size - off;
}

struct stage {
  struct a2s_reflect reflect;
  uint32_t *words;
  size_t nwords;
  uint32_t lengths[32];
  char *source;
  struct pd_gl_bindings bindings;
  GLuint shader;
};

// keep linked programs across draws, matched on the actual shader bytes
// the same metal function can have different ubo sizes, so a hash is not enough
#define PROGRAM_CACHE_SIZE 8
struct cached_program {
  GLuint program;
  unsigned age;
  uint8_t *vertex, *fragment;
  uint32_t vertex_size, fragment_size;
  uint32_t lengths[2][32];
  struct a2s_reflect reflect[2];
  struct pd_gl_bindings bindings[2];
  uint32_t samplers[2][16];
};

static struct cached_program programs[PROGRAM_CACHE_SIZE];
static unsigned program_age;

static struct cached_program *find_program(const struct PDDrawRequest *r, const uint8_t *base) {
  for (unsigned i = 0; i < PROGRAM_CACHE_SIZE; i++) {
    struct cached_program *c = &programs[i];
    const struct PDDrawExt *ext;
    uint32_t zero[16] = { 0 };
    int match = 1;

    if (!c->program || c->vertex_size != r->vmSize || c->fragment_size != r->fmSize)
      continue;

    for (unsigned k = 0; k < PD_DRAW_BUFS; k++) {
      if (c->lengths[0][k] != r->vbuf[k].size / 4 + (r->vbuf[k].size % 4 != 0) ||
          c->lengths[1][k] != r->fbuf[k].size / 4 + (r->fbuf[k].size % 4 != 0))
        match = 0;
    }

    ext = pd_draw_ext(r);
    if (memcmp(c->samplers[0], ext ? ext->vsamp : zero, sizeof(zero)) || memcmp(c->samplers[1], r->samp, sizeof(zero)))
      match = 0;

    if (!match || memcmp(c->vertex, base + r->vmOff, r->vmSize) || memcmp(c->fragment, base + r->fmOff, r->fmSize))
      continue;

    c->age = ++program_age;
    return c;
  }

  return 0;
}

static int keep_program(GLuint program, const struct stage *s, const struct PDDrawRequest *r, const uint8_t *base) {
  uint8_t *v, *f;
  unsigned slot = 0;
  struct cached_program *c;
  const struct PDDrawExt *ext;

  if ((uint64_t)r->vmSize + r->fmSize > 1024 * 1024)
    return 0;

  v = malloc(r->vmSize), f = malloc(r->fmSize);
  if (!v || !f) {
    free(v);
    free(f);
    return 0;
  }

  memcpy(v, base + r->vmOff, r->vmSize);
  memcpy(f, base + r->fmOff, r->fmSize);

  for (unsigned i = 0; i < PROGRAM_CACHE_SIZE; i++) {
    if (!programs[i].program) {
      slot = i;
      break;
    }
    if (programs[i].age < programs[slot].age)
      slot = i;
  }

  c = &programs[slot];
  if (c->program)
    glDeleteProgram(c->program);

  free(c->vertex);
  free(c->fragment);
  c->program = program;
  c->age = ++program_age;
  c->vertex = v, c->fragment = f;
  c->vertex_size = r->vmSize, c->fragment_size = r->fmSize;

  for (unsigned i = 0; i < 2; i++) {
    c->reflect[i] = s[i].reflect;
    c->bindings[i] = s[i].bindings;
    memcpy(c->lengths[i], s[i].lengths, sizeof(c->lengths[i]));
  }

  ext = pd_draw_ext(r);
  if (ext)
    memcpy(c->samplers[0], ext->vsamp, sizeof(c->samplers[0]));
  else
    memset(c->samplers[0], 0, sizeof(c->samplers[0]));

  memcpy(c->samplers[1], r->samp, sizeof(c->samplers[1]));
  return 1;
}

static int translate(struct stage *s, const uint8_t *mtl, uint32_t bytes, const struct PDDrawBuffer *buffers,
                     uint32_t size, unsigned index) {
  struct a2s_options options = { .quantize_half = 1, .narrow_loads = 1, .kill_discard = 1,
                                 .set = index, .uniform_words = s->lengths };

  for (unsigned i = 0; i < PD_DRAW_BUFS; i++) {
    if (!span(buffers[i].off, buffers[i].size, size))
      return decline("Metal buffer outside the arena");

    s->lengths[i] = buffers[i].size / 4 + (buffers[i].size % 4 != 0);
  }

  if (a2s_translate(mtl, bytes, &options, &s->words, &s->nwords, &s->reflect, reason, sizeof(reason)))
    return PD_GL_FALLBACK;

  if (s->reflect.stage != (index ? A2S_FRAGMENT : A2S_VERTEX))
    return decline("unexpected Metal shader stage");

  for (unsigned i = 0; i < s->reflect.nres; i++) {
    const struct a2s_resource *r = &s->reflect.res[i];

    if (r->access & A2S_WRITE || r->kind == A2S_RES_INPUT_ATTACHMENT || r->kind == A2S_RES_ATTACHMENT_IMAGE)
      return decline("writable shader resources require a newer OpenGL executor");

    if (r->kind == A2S_RES_TEXTURE && (r->metal >= PD_DRAW_TEXS || r->storage))
      return decline("unsupported shader texture binding");

    if (r->kind != A2S_RES_BUFFER)
      continue;

    if (r->metal >= PD_DRAW_BUFS)
      return decline("unsupported buffer binding");

    if ((uint64_t)s->lengths[r->metal] * 16 > (uint32_t)max_block)
      return decline("Metal buffer exceeds the device's uniform block limit");
  }

  return 0;
}

static int compile(struct stage *s, const struct a2s_reflect *fragment, unsigned index, const uint32_t *samplers) {
  const char *dump;
  GLint ok;

  if (pd_glsl_source(s->words, s->nwords, &s->reflect, fragment, index, samplers, &s->bindings, 0, &s->source, reason,
                     sizeof(reason)))
    return PD_GL_FALLBACK;

  dump = getenv("AIRJITD_GL_DUMP");
  if (dump && *dump) {
    char path[1024];
    FILE *file;

    snprintf(path, sizeof(path), "%s/stage%u.glsl", dump, index);
    file = fopen(path, "w");
    if (file) {
      fputs(s->source, file);
      fclose(file);
    }
  }

  s->shader = glCreateShader(index ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
  if (!s->shader)
    return decline("cannot allocate OpenGL shader");

  glShaderSource(s->shader, 1, (const GLchar *const *)&s->source, 0);
  glCompileShader(s->shader);
  glGetShaderiv(s->shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(s->shader, sizeof(reason), 0, reason);
    return PD_GL_FALLBACK;
  }

  return 0;
}

static int bind_buffers(GLuint program, struct stage *s, unsigned stage, const struct PDDrawBuffer *buffers,
                        const uint8_t *base, GLuint *objects, unsigned *nobjects) {
  char name[96];

  for (unsigned i = 0; i < s->reflect.nres; i++) {
    const struct a2s_resource *r = &s->reflect.res[i];
    const struct PDDrawBuffer *b;
    GLuint block, binding;
    size_t bytes;
    uint8_t *padded;

    if (r->kind != A2S_RES_BUFFER)
      continue;

    b = &buffers[r->metal];
    snprintf(name, sizeof(name), "PDStage%uBuffer%u", stage, r->binding);
    block = glGetUniformBlockIndex(program, name);
    if (block == GL_INVALID_INDEX)
      continue;

    if (*nobjects >= (unsigned)max_bindings || *nobjects >= 2 * PD_DRAW_BUFS)
      return decline("uniform buffer binding limit exceeded");

    bytes = (size_t)s->lengths[r->metal] * 16;
    padded = calloc(1, bytes);
    if (!padded)
      return decline("out of memory packing a uniform buffer");

    for (unsigned word = 0; word < s->lengths[r->metal]; word++) {
      uint32_t at = word * 4, n = b->size - at < 4 ? b->size - at : 4;

      memcpy(padded + (size_t)word * 16, base + b->off + at, n);
    }

    binding = *nobjects;
    glGenBuffers(1, &objects[binding]);
    ++*nobjects;
    glBindBuffer(GL_UNIFORM_BUFFER, objects[binding]);
    glBufferData(GL_UNIFORM_BUFFER, bytes, padded, GL_STREAM_DRAW);
    free(padded);
    glUniformBlockBinding(program, block, binding);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, objects[binding]);
  }

  for (unsigned i = 0; i < s->reflect.npc; i++) {
    if (s->reflect.pc[i].kind != A2S_PC_BUFFER_OFFSET)
      return decline("unexpected draw push constant");

    snprintf(name, sizeof(name), "pd_stage%u_push.word%u", stage, i);

    // each upload starts at its metal buffer's own base
    glUniform1ui(glGetUniformLocation(program, name), 0);
  }

  return 0;
}

static GLenum address_mode(unsigned mode) {
  static const GLenum modes[] = { GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT,
                                  GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER };

  return modes[mode < 6 ? mode : 0];
}

// metal texture rows are addressed from row zero, so sampled textures upload in that order
// only framebuffer attachments take the render-coordinate flip
static int bind_textures(GLuint program, const struct stage *stage, const struct PDDrawTex *textures,
                         const struct PDTexLayout *layouts, uint8_t *base, uint32_t size, GLuint *objects,
                         GLuint *samplers, unsigned *count) {
  static const GLenum filters[2][3] = { { GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR },
                                        { GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR } };

  for (unsigned i = 0; i < stage->bindings.count; i++) {
    const struct pd_gl_pair *pair = &stage->bindings.pair[i];
    const struct PDDrawTex *t;
    const struct PDTexLayout *l;
    int layout;
    unsigned levels, type, slices;
    GLenum target, internal, format, scalar, error;
    unsigned out_bpp, components, unit, mip;
    uint32_t flags;

    if (pair->texture >= PD_DRAW_TEXS || !textures)
      return decline("missing Metal texture binding");

    t = &textures[pair->texture];
    l = layouts ? &layouts[pair->texture] : 0;
    layout = l && (l->flags & PD_TEXLAYOUT_SET);
    levels = layout && l->levels ? l->levels : 1;
    type = layout ? l->type : PD_TEXTYPE_2D;
    slices = layout && l->slices ? l->slices : 1;

    if (!t->w || !t->h || t->fmt >= PD_FMT_COUNT || levels > PD_TEX_LEVELS || t->w > (unsigned)max_texture ||
        t->h > (unsigned)max_texture || slices > (unsigned)max_texture || (layout && l->samples > 1) ||
        *count + 1 >= (unsigned)max_units || *count >= 64)
      return decline("invalid sampled texture dimensions or binding limits");

    target = type == PD_TEXTYPE_2D ? GL_TEXTURE_2D :
             type == PD_TEXTYPE_2D_ARRAY ? GL_TEXTURE_2D_ARRAY :
             type == PD_TEXTYPE_CUBE ? GL_TEXTURE_CUBE_MAP :
             type == PD_TEXTYPE_3D ? GL_TEXTURE_3D : 0;
    if (!target || (type == PD_TEXTYPE_CUBE && (slices != 6 || t->w != t->h)))
      return decline("texture type is not supported by this device");

    internal = GL_RGBA32F, format = GL_RGBA, scalar = GL_FLOAT;
    out_bpp = 16, components = 4;

    if (pd_fmt_is_int(t->fmt)) {
      switch (t->fmt) {
      case PD_FMT_R32UI:
        internal = GL_R32UI, format = GL_RED_INTEGER;
        scalar = GL_UNSIGNED_INT, out_bpp = 4;
        break;
      case PD_FMT_RG32UI:
        internal = GL_RG32UI, format = GL_RG_INTEGER;
        scalar = GL_UNSIGNED_INT, out_bpp = 8;
        break;
      case PD_FMT_R16UI:
        internal = GL_R16UI, format = GL_RED_INTEGER;
        scalar = GL_UNSIGNED_SHORT, out_bpp = 2;
        break;
      case PD_FMT_RG16UI:
        internal = GL_RG16UI, format = GL_RG_INTEGER;
        scalar = GL_UNSIGNED_SHORT, out_bpp = 4;
        break;
      case PD_FMT_RGBA16UI:
        internal = GL_RGBA16UI, format = GL_RGBA_INTEGER;
        scalar = GL_UNSIGNED_SHORT, out_bpp = 8;
        break;
      case PD_FMT_RGBA8UI:
        internal = GL_RGBA8UI, format = GL_RGBA_INTEGER;
        scalar = GL_UNSIGNED_BYTE, out_bpp = 4;
        break;
      default:
        return decline("unsupported integer texture format");
      }
    }

    if (pd_fmt_is_compressed(t->fmt) && !pd_fmt_cpu_decodes(t->fmt))
      return decline("unsupported compressed texture format");

    unit = (*count)++;
    glGenTextures(1, &objects[unit]);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target, objects[unit]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, levels - 1);

    for (unsigned level = 0; level < levels; level++) {
      unsigned w, h, depth, bpp, rows;
      uint32_t pitch, loff;
      uint64_t min_pitch, slice_stride, length;
      size_t bytes;
      uint8_t *pixels;

      w = t->w >> level, h = t->h >> level;
      if (!w)
        w = 1;
      if (!h)
        h = 1;

      depth = type == PD_TEXTYPE_3D ? slices >> level : type == PD_TEXTYPE_2D ? 1 : slices;
      if (!depth)
        depth = 1;

      pitch = layout ? l->level[level].bpr : t->bpr;
      loff = layout ? l->level[level].off : 0;
      bpp = tex_bpp(t->fmt);
      min_pitch = pd_fmt_is_compressed(t->fmt) ? (uint64_t)((w + 3) / 4) * bpp : (uint64_t)w * bpp;
      rows = pd_fmt_is_compressed(t->fmt) ? (h + 3) / 4 : h;
      if (!pitch)
        pitch = (uint32_t)min_pitch;
      if (pitch < min_pitch)
        return decline("sampled texture pitch is too small");

      slice_stride = type == PD_TEXTYPE_3D ? (uint64_t)pitch * rows : layout ? l->sliceBytes : 0;
      if (depth > 1 && !slice_stride)
        return decline("sampled texture has no slice stride");

      length = (uint64_t)pitch * rows;
      for (unsigned z = 0; z < depth; z++) {
        uint64_t off = (uint64_t)t->off + loff + z * slice_stride;

        if (off > UINT32_MAX || !span((uint32_t)off, length, size))
          return decline("sampled texture outside arena");

        if (gpu_newer && overlaps_target((uint32_t)off, (uint32_t)length) && sync_target())
          return PD_ERR_BUFFER;
      }

      bytes = (size_t)w * h * depth * out_bpp;
      pixels = malloc(bytes);
      if (!pixels)
        return decline("out of memory uploading sampled texture");

      for (unsigned z = 0; z < depth; z++) {
        for (unsigned y = 0; y < h; y++) {
          for (unsigned x = 0; x < w; x++) {
            const uint8_t *src = base + t->off + loff + z * slice_stride;
            uint8_t *dst = pixels + (((size_t)z * h + y) * w + x) * out_bpp;

            if (pd_fmt_is_int(t->fmt)) {
              memcpy(dst, src + (size_t)y * pitch + x * bpp, out_bpp);
            } else if (pd_fmt_is_compressed(t->fmt)) {
              pd_bc_texel(t->fmt, src, pitch, x, y, (float *)dst);
            } else {
              pd_texel_load(t->fmt, src + (size_t)y * pitch + x * bpp, (float *)dst);
            }
          }
        }
      }

      if (target == GL_TEXTURE_2D) {
        glTexImage2D(target, level, internal, w, h, 0, format, scalar, pixels);
      } else if (target == GL_TEXTURE_CUBE_MAP) {
        for (unsigned face = 0; face < 6; face++) {
          glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, internal, w, h, 0, format, scalar,
                       pixels + (size_t)face * w * h * out_bpp);
        }
      } else {
        glTexImage3D(target, level, internal, w, h, depth, 0, format, scalar, pixels);
      }

      free(pixels);
    }

    flags = pair->flags ? pair->flags : 0xc0000000u;
    mip = PD_SAMP_MIP(flags);
    glGenSamplers(1, &samplers[unit]);
    glSamplerParameteri(samplers[unit], GL_TEXTURE_MIN_FILTER, filters[!!(flags & 1)][mip < 3 ? mip : 0]);
    glSamplerParameteri(samplers[unit], GL_TEXTURE_MAG_FILTER, flags & 4 ? GL_LINEAR : GL_NEAREST);
    glSamplerParameteri(samplers[unit], GL_TEXTURE_WRAP_S, address_mode((flags >> 8) & 15));
    glSamplerParameteri(samplers[unit], GL_TEXTURE_WRAP_T, address_mode((flags >> 12) & 15));
    glSamplerParameteri(samplers[unit], GL_TEXTURE_WRAP_R, address_mode((flags >> 16) & 15));
    glBindSampler(unit, samplers[unit]);
    glUniform1i(glGetUniformLocation(program, pair->name), unit);

    error = glGetError();
    if (error != GL_NO_ERROR) {
      snprintf(reason, sizeof(reason), "texture upload failed: 0x%04x", error);
      return PD_GL_FALLBACK;
    }
  }

  return 0;
}

static const GLenum compare_functions[8] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
                                             GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };
static const GLenum stencil_operations[8] = { GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR,
                                              GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP };

struct depth_pixel {
  float depth;
  uint32_t stencil;
};

static int depth_upload(const struct PDDrawTex *t, uint8_t *base, uint32_t size, GLuint *object,
                        struct depth_pixel **pixels) {
  unsigned bpp = tex_bpp(t->fmt);

  if ((!pd_fmt_is_depth(t->fmt) && !pd_fmt_has_stencil(t->fmt)) || !bpp || t->bpr < (uint64_t)t->w * bpp ||
      !span(t->off, (uint64_t)t->bpr * t->h, size))
    return decline("invalid depth/stencil attachment");

  if (gpu_newer && overlaps_target(t->off, t->bpr * t->h) && sync_target())
    return PD_ERR_BUFFER;

  *pixels = malloc((size_t)t->w * t->h * sizeof(**pixels));
  if (!*pixels)
    return decline("out of memory staging depth/stencil");

  for (unsigned y = 0; y < t->h; y++) {
    for (unsigned x = 0; x < t->w; x++) {
      const uint8_t *p = base + t->off + (size_t)(t->h - 1 - y) * t->bpr + x * bpp;
      struct depth_pixel *d = *pixels + (size_t)y * t->w + x;

      d->depth = pd_fmt_is_depth(t->fmt) ? pd_depth_load(t->fmt, p) : 1;
      d->stencil = pd_fmt_has_stencil(t->fmt) ? pd_stencil_load(t->fmt, p) : 0;
    }
  }

  glGenTextures(1, object);
  glBindTexture(GL_TEXTURE_2D, *object);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, t->w, t->h, 0, GL_DEPTH_STENCIL,
               GL_FLOAT_32_UNSIGNED_INT_24_8_REV, *pixels);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, *object, 0);
  return 0;
}

static int raster_state(const struct PDDrawRequest *r, const struct PDDrawExt *ext) {
  unsigned cull;

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_CLAMP);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glFrontFace(GL_CW);
  if (!ext)
    return 0;

  glFrontFace(ext->raster & PD_RASTER_CCW ? GL_CCW : GL_CW);
  cull = ext->raster & 3;
  if (cull > 2)
    return decline("invalid cull mode");

  if (cull) {
    glEnable(GL_CULL_FACE);
    glCullFace(cull == 1 ? GL_FRONT : GL_BACK);
  }

  if (ext->raster & PD_RASTER_LINES)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  if (ext->raster & PD_RASTER_CLAMP)
    glEnable(GL_DEPTH_CLAMP);

  if (ext->raster & PD_RASTER_SCISSOR) {
    if (ext->scissor[2] > INT32_MAX || ext->scissor[3] > INT32_MAX)
      return decline("invalid scissor size");

    glEnable(GL_SCISSOR_TEST);
    glScissor(ext->scissor[0], (GLint)((int64_t)r->th - ext->scissor[1] - ext->scissor[3]), ext->scissor[2],
              ext->scissor[3]);
  }

  if (ext->depthBiasClamp)
    return decline("depth bias clamping needs a newer OpenGL extension");

  if (ext->depthBias || ext->depthSlope) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(ext->depthSlope, ext->depthBias);
  }

  if (ext->depth.w && (ext->depthState & PD_DEPTH_SET)) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(compare_functions[ext->depthState & 7]);
    glDepthMask(!!(ext->depthState & PD_DEPTH_WRITE));
  }

  if (ext->depth.w && ((ext->stencil[0] | ext->stencil[1]) & PD_STENCIL_ON)) {
    glEnable(GL_STENCIL_TEST);
    for (unsigned side = 0; side < 2; side++) {
      uint32_t st = ext->stencil[side];
      GLenum face = side ? GL_BACK : GL_FRONT;

      glStencilFuncSeparate(face, st & PD_STENCIL_ON ? compare_functions[st & 7] : GL_ALWAYS,
                            (ext->stencilRef >> (side * 8)) & 255, (st >> 12) & 255);
      glStencilMaskSeparate(face, (st >> 20) & 255);
      glStencilOpSeparate(face, stencil_operations[(st >> 3) & 7], stencil_operations[(st >> 6) & 7],
                          stencil_operations[(st >> 9) & 7]);
    }
  }

  return 0;
}

static int bind_attributes(const struct PDDrawRequest *r, const struct PDDrawExt *ext,
                           const struct a2s_reflect *reflect, const uint8_t *base, GLuint *objects,
                           unsigned *count) {
  static const struct {
    unsigned n, bytes;
    char kind;
  } formats[56] = {
    [1] = { 2, 1, 'u' },  [2] = { 3, 1, 'u' },  [3] = { 4, 1, 'u' },  [4] = { 2, 1, 's' },
    [5] = { 3, 1, 's' },  [6] = { 4, 1, 's' },  [7] = { 2, 1, 'U' },  [8] = { 3, 1, 'U' },
    [9] = { 4, 1, 'U' },  [10] = { 2, 1, 'S' }, [11] = { 3, 1, 'S' }, [12] = { 4, 1, 'S' },
    [13] = { 2, 2, 'u' }, [14] = { 3, 2, 'u' }, [15] = { 4, 2, 'u' }, [16] = { 2, 2, 's' },
    [17] = { 3, 2, 's' }, [18] = { 4, 2, 's' }, [19] = { 2, 2, 'U' }, [20] = { 3, 2, 'U' },
    [21] = { 4, 2, 'U' }, [22] = { 2, 2, 'S' }, [23] = { 3, 2, 'S' }, [24] = { 4, 2, 'S' },
    [25] = { 2, 2, 'h' }, [26] = { 3, 2, 'h' }, [27] = { 4, 2, 'h' }, [28] = { 1, 4, 'f' },
    [29] = { 2, 4, 'f' }, [30] = { 3, 4, 'f' }, [31] = { 4, 4, 'f' }, [32] = { 1, 4, 's' },
    [33] = { 2, 4, 's' }, [34] = { 3, 4, 's' }, [35] = { 4, 4, 's' }, [36] = { 1, 4, 'u' },
    [37] = { 2, 4, 'u' }, [38] = { 3, 4, 'u' }, [39] = { 4, 4, 'u' }, [40] = { 4, 4, 'A' },
    [41] = { 4, 4, 'B' }, [42] = { 4, 1, 'C' }, [45] = { 1, 1, 'u' }, [46] = { 1, 1, 's' },
    [47] = { 1, 1, 'U' }, [48] = { 1, 1, 'S' }, [49] = { 1, 2, 'u' }, [50] = { 1, 2, 's' },
    [51] = { 1, 2, 'U' }, [52] = { 1, 2, 'S' }, [53] = { 1, 2, 'h' }
  };
  GLint max;
  uint64_t vertex_max;

  if (!reflect->nvin)
    return 0;

  if (!ext)
    return decline("stage-in shader has no vertex descriptor");

  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max);
  vertex_max = r->count ? (uint64_t)ext->firstVertex + r->count - 1 : 0;
  if (r->indexed) {
    vertex_max = 0;
    for (unsigned i = 0; i < r->count; i++) {
      uint32_t value = 0;
      int64_t adjusted;

      memcpy(&value, base + r->indexOff + (size_t)i * r->indexBytes, r->indexBytes);
      adjusted = (int64_t)value + ext->baseVertex;
      if (adjusted < 0)
        return decline("negative vertex buffer index");

      if ((uint64_t)adjusted > vertex_max)
        vertex_max = adjusted;
    }
  }

  for (unsigned i = 0; i < reflect->nvin; i++) {
    unsigned loc = reflect->vin[i].location;
    const struct PDVertexAttr *at;
    const struct PDVertexLayout *layout;
    const struct PDDrawBuffer *b;
    unsigned n, bytes, width;
    char kind;
    uint64_t index, offset;
    GLuint object;
    GLenum type;

    if (loc >= PD_VERTEX_ATTRS || loc >= (unsigned)max)
      return decline("vertex attribute limit exceeded");

    at = &ext->attr[loc];
    if (at->buffer >= PD_VERTEX_ATTRS || at->format >= 56 || !formats[at->format].n)
      return decline("invalid vertex descriptor");

    layout = &ext->layout[at->buffer];
    b = &r->vbuf[at->buffer];
    n = formats[at->format].n, bytes = formats[at->format].bytes;
    kind = formats[at->format].kind;
    width = kind == 'A' || kind == 'B' ? 4 : n * bytes;
    index = layout->step == 0 ? 0 :
            layout->step == 2 ? ext->baseInstance + ((r->instances ? r->instances : 1) - 1) / (layout->rate ? layout->rate : 1) :
            vertex_max;
    if ((uint64_t)at->offset + index * layout->stride + width > b->size || layout->stride > INT32_MAX)
      return decline("vertex attribute reads outside its buffer");

    glGenBuffers(1, &object);
    objects[(*count)++] = object;
    glBindBuffer(GL_ARRAY_BUFFER, object);
    glBufferData(GL_ARRAY_BUFFER, b->size, base + b->off, GL_STREAM_DRAW);

    type = kind == 'h' ? GL_HALF_FLOAT :
           kind == 'f' ? GL_FLOAT :
           kind == 'A' ? GL_INT_2_10_10_10_REV :
           kind == 'B' ? GL_UNSIGNED_INT_2_10_10_10_REV :
           kind == 's' || kind == 'S' ? (bytes == 1 ? GL_BYTE : bytes == 2 ? GL_SHORT : GL_INT) :
           bytes == 1 ? GL_UNSIGNED_BYTE : bytes == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    offset = at->offset + (layout->step == 2 ? (uint64_t)ext->baseInstance * layout->stride : 0);

    if (reflect->vin[i].elem != 'f') {
      glVertexAttribIPointer(loc, n, type, layout->stride, (void *)(uintptr_t)offset);
    } else {
      glVertexAttribPointer(loc, kind == 'C' ? GL_BGRA : n, type,
                            kind == 'U' || kind == 'S' || kind == 'A' || kind == 'B' || kind == 'C', layout->stride,
                            (void *)(uintptr_t)offset);
    }

    glVertexAttribDivisor(loc, layout->step == 0 ? UINT32_MAX : layout->step == 2 ? (layout->rate ? layout->rate : 1) : 0);
    glEnableVertexAttribArray(loc);
  }

  return 0;
}

int pd_gl_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t size) {
  static const GLenum primitive[] = { GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP };
  struct stage stages[2] = { 0 };
  GLuint program = 0, fbo = 0, texture = 0, vao = 0, indices = 0, buffers[2 * PD_DRAW_BUFS] = { 0 };
  unsigned nbuf = 0, ntex = 0, nattrs = 0;
  GLuint attribute_objects[32] = { 0 };
  GLuint tex_objects[64] = { 0 }, sampler_objects[64] = { 0 };
  uint8_t *pixels = 0;
  GLuint depth_object = 0;
  struct depth_pixel *depth_pixels = 0;
  int rc = PD_GL_FALLBACK, cached = 0, borrowed = 0;
  const struct PDDrawExt *ext = pd_draw_ext(r);
  struct cached_program *hit;
  struct gl_format target_format;
  size_t row, bytes;
  GLenum format, pixel_type, error;
  GLint linked;
  double vx, vy, vw, vh;
  unsigned mask;
  GLsizei instances;

  pthread_mutex_lock(&mutex);
  if (start())
    goto done;

  reason[0] = 0;
  if (!eglMakeCurrent(display, surface, surface, context)) {
    decline("cannot bind OpenGL executor context");
    goto done;
  }

  while (glGetError() != GL_NO_ERROR) {
  }

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  if (!base || !r->tw || !r->th || r->tw > (unsigned)max_texture || r->th > (unsigned)max_texture ||
      r->tbpr < (uint64_t)r->tw * tex_bpp(r->tfmt) || !span(r->targetOff, (uint64_t)r->tbpr * r->th, size) ||
      !span(r->vmOff, r->vmSize, size) || !span(r->fmOff, r->fmSize, size)) {
    decline("invalid draw ranges or dimensions");
    goto done;
  }

  if ((!color_format(r->tfmt).internal) || r->count > 0x7fffffffu || r->instances > 0x7fffffffu ||
      r->prim > PD_PRIM_TRIANGLE_STRIP) {
    decline("draw format or primitive is not supported yet");
    goto done;
  }

  if (r->rt[1].w || r->rt[2].w || r->rt[3].w || (ext && (ext->raster & PD_RASTER_NOCOLOR))) {
    decline("extended raster state is not supported yet");
    goto done;
  }

  if (r->indexed && ((r->indexBytes != 2 && r->indexBytes != 4) ||
                     !span(r->indexOff, (uint64_t)r->count * r->indexBytes, size))) {
    decline("invalid index buffer");
    goto done;
  }

  for (unsigned i = 0; i < PD_DRAW_BUFS; i++) {
    if (!span(r->vbuf[i].off, r->vbuf[i].size, size) || !span(r->fbuf[i].off, r->fbuf[i].size, size)) {
      decline("Metal buffer outside the arena");
      goto done;
    }
  }

  for (unsigned i = 0; i < PD_DRAW_BUFS; i++) {
    if (gpu_newer && (overlaps_target(r->vbuf[i].off, r->vbuf[i].size) ||
                      overlaps_target(r->fbuf[i].off, r->fbuf[i].size))) {
      if (sync_target()) {
        rc = PD_ERR_BUFFER;
        goto done;
      }
    }
  }

  if (gpu_newer && r->indexed && overlaps_target(r->indexOff, r->count * r->indexBytes) && sync_target()) {
    rc = PD_ERR_BUFFER;
    goto done;
  }

  hit = find_program(r, base);
  if (hit) {
    program = hit->program;
    cached = 1;
    for (unsigned i = 0; i < 2; i++) {
      stages[i].reflect = hit->reflect[i];
      stages[i].bindings = hit->bindings[i];
      memcpy(stages[i].lengths, hit->lengths[i], sizeof(stages[i].lengths));
    }
  } else {
    if (translate(&stages[0], base + r->vmOff, r->vmSize, r->vbuf, size, 0) ||
        translate(&stages[1], base + r->fmOff, r->fmSize, r->fbuf, size, 1))
      goto done;

    if (a2s_relocate(stages[1].words, &stages[1].reflect, &stages[0].reflect) != stages[1].reflect.nvar) {
      decline("fragment varyings do not match the vertex stage");
      goto done;
    }

    if (compile(&stages[0], &stages[1].reflect, 0, ext ? ext->vsamp : 0) || compile(&stages[1], 0, 1, r->samp))
      goto done;

    program = glCreateProgram();
    glAttachShader(program, stages[0].shader);
    glAttachShader(program, stages[1].shader);
    for (unsigned i = 0; i < 4; i++) {
      char output[32];

      snprintf(output, sizeof(output), "pd_color%u", i);
      glBindFragDataLocation(program, i, output);
    }

    for (unsigned i = 0; i < stages[0].reflect.nvin; i++) {
      char name[64];
      unsigned loc = stages[0].reflect.vin[i].location;

      snprintf(name, sizeof(name), "pd_attribute%u", loc);
      glBindAttribLocation(program, loc, name);
    }

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
      glGetProgramInfoLog(program, sizeof(reason), 0, reason);
      goto done;
    }

    glDetachShader(program, stages[0].shader);
    glDetachShader(program, stages[1].shader);
    cached = keep_program(program, stages, r, base);
  }

  glUseProgram(program);
  glUniform1i(glGetUniformLocation(program, "SPIRV_Cross_BaseInstance"), ext ? ext->baseInstance : 0);
  glUniform1i(glGetUniformLocation(program, "SPIRV_Cross_BaseVertex"), ext ? ext->baseVertex : 0);
  if (bind_buffers(program, &stages[0], 0, r->vbuf, base, buffers, &nbuf) ||
      bind_buffers(program, &stages[1], 1, r->fbuf, base, buffers, &nbuf))
    goto done;

  rc = bind_textures(program, &stages[0], ext ? ext->vtex : 0, ext ? ext->vtexLayout : 0, base, size, tex_objects,
                     sampler_objects, &ntex);
  if (!rc)
    rc = bind_textures(program, &stages[1], r->ftex, ext ? ext->ftexLayout : 0, base, size, tex_objects,
                       sampler_objects, &ntex);
  if (rc)
    goto done;

  rc = PD_GL_FALLBACK;
  glActiveTexture(GL_TEXTURE0 + ntex);
  target_format = color_format(r->tfmt);
  row = (size_t)r->tw * tex_bpp(r->tfmt), bytes = row * r->th;
  if (r->tbpr < row) {
    decline("render target pitch is too small");
    goto done;
  }

  format = target_format.format;
  pixel_type = target_format.type;
  borrowed = resident_mode && last_fbo && !cpu_newer && last_base == base && last_target == r->targetOff &&
             last_w == r->tw && last_h == r->th && last_pitch == r->tbpr && last_format == r->tfmt;
  if (borrowed) {
    fbo = last_fbo, texture = last_texture;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  } else {
    if (sync_target()) {
      rc = PD_ERR_BUFFER;
      goto done;
    }

    pixels = malloc(bytes);
    if (!pixels) {
      decline("out of memory staging render target");
      goto done;
    }

    for (unsigned y = 0; y < r->th; y++) {
      memcpy(pixels + (size_t)y * row, base + r->targetOff + (size_t)(r->th - 1 - y) * r->tbpr, row);
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, target_format.internal, r->tw, r->th, 0, format, pixel_type, pixels);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  }

  if (ext && ext->depth.w) {
    if (ext->depth.w != r->tw || ext->depth.h != r->th) {
      decline("attachment dimensions do not match");
      goto done;
    }

    rc = depth_upload(&ext->depth, base, size, &depth_object, &depth_pixels);
    if (rc)
      goto done;

    rc = PD_GL_FALLBACK;
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    decline("incomplete OpenGL render target");
    goto done;
  }

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  if (bind_attributes(r, ext, &stages[0].reflect, base, attribute_objects, &nattrs))
    goto done;

  if (r->indexed) {
    glGenBuffers(1, &indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (size_t)r->count * r->indexBytes, base + r->indexOff, GL_STREAM_DRAW);
  }

  vx = r->viewport[0], vy = r->viewport[1];
  vw = r->viewport[2], vh = r->viewport[3];
  if (vw <= 0 || vh <= 0) {
    vx = vy = 0;
    vw = r->tw, vh = r->th;
  }

  glViewport((GLint)vx, (GLint)(r->th - vy - vh), (GLsizei)vw, (GLsizei)vh);
  glDepthRange(r->viewport[4], r->viewport[5]);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DITHER);
  if (r->tfmt == PD_FMT_BGRA8_SRGB || r->tfmt == PD_FMT_RGBA8_SRGB)
    glEnable(GL_FRAMEBUFFER_SRGB);
  else
    glDisable(GL_FRAMEBUFFER_SRGB);

  if (raster_state(r, ext))
    goto done;

  glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
  if (blend_state(r->blend, ext ? ext->blendColor : pd_blend_constant))
    goto done;

  mask = r->blend & PD_BLEND_KNOWN ? PD_BLEND_MASK(r->blend) : PD_MASK_ALL;
  glColorMask(!!(mask & PD_MASK_RED), !!(mask & PD_MASK_GREEN), !!(mask & PD_MASK_BLUE), !!(mask & PD_MASK_ALPHA));
  instances = r->instances ? r->instances : 1;
  if (r->indexed)
    glDrawElementsInstancedBaseVertex(primitive[r->prim], r->count, r->indexBytes == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
                                      0, instances, ext ? ext->baseVertex : 0);
  else
    glDrawArraysInstanced(primitive[r->prim], ext ? ext->firstVertex : 0, r->count, instances);

  error = glGetError();
  if (error != GL_NO_ERROR) {
    snprintf(reason, sizeof(reason), "OpenGL draw failed: 0x%04x", error);
    if (resident_mode)
      rc = PD_ERR_BUFFER;
    goto done;
  }

  if (depth_object)
    glReadPixels(0, 0, r->tw, r->th, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, depth_pixels);

  if (!resident_mode)
    glReadPixels(0, 0, r->tw, r->th, format, pixel_type, pixels);

  glFinish();
  error = glGetError();
  if (error != GL_NO_ERROR) {
    snprintf(reason, sizeof(reason), "OpenGL readback failed: 0x%04x", error);
    if (resident_mode)
      rc = PD_ERR_BUFFER;
    goto done;
  }

  if (depth_object) {
    const struct PDDrawTex *t = &ext->depth;
    unsigned bpp = tex_bpp(t->fmt);

    for (unsigned y = 0; y < t->h; y++) {
      for (unsigned x = 0; x < t->w; x++) {
        struct depth_pixel *d = depth_pixels + (size_t)(t->h - 1 - y) * t->w + x;
        uint8_t *p = base + t->off + (size_t)y * t->bpr + x * bpp;

        if (pd_fmt_is_depth(t->fmt))
          pd_depth_store(t->fmt, p, d->depth);
        if (pd_fmt_has_stencil(t->fmt))
          pd_stencil_store(t->fmt, p, d->stencil);
      }
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
  }

  for (unsigned y = 0; !resident_mode && y < r->th; y++) {
    memcpy(base + r->targetOff + (size_t)y * r->tbpr, pixels + (size_t)(r->th - 1 - y) * row, row);
  }

  if (!borrowed && last_fbo)
    glDeleteFramebuffers(1, &last_fbo);
  if (!borrowed && last_texture)
    glDeleteTextures(1, &last_texture);

  last_fbo = fbo, last_texture = texture;
  last_target = r->targetOff, last_pitch = r->tbpr;
  last_w = r->tw, last_h = r->th;
  last_format = r->tfmt, last_base = base;
  gpu_newer = resident_mode, cpu_newer = 0;
  fbo = texture = 0;
  rc = 0;

done:
  if (state > 0) {
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    if (depth_object)
      glDeleteTextures(1, &depth_object);
    if (nattrs)
      glDeleteBuffers(nattrs, attribute_objects);
    if (indices)
      glDeleteBuffers(1, &indices);

    for (unsigned i = 0; i < ntex; i++)
      glBindSampler(i, 0);

    if (ntex) {
      glDeleteTextures(ntex, tex_objects);
      glDeleteSamplers(ntex, sampler_objects);
    }

    if (nbuf)
      glDeleteBuffers(nbuf, buffers);
    if (vao)
      glDeleteVertexArrays(1, &vao);
    if (fbo && !borrowed)
      glDeleteFramebuffers(1, &fbo);
    if (texture && !borrowed)
      glDeleteTextures(1, &texture);
    if (program && !cached)
      glDeleteProgram(program);

    for (unsigned i = 0; i < 2; i++) {
      if (stages[i].shader)
        glDeleteShader(stages[i].shader);
    }
  }

  if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT)
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  free(depth_pixels);
  free(pixels);
  for (unsigned i = 0; i < 2; i++) {
    free(stages[i].words);
    free(stages[i].source);
  }

  pthread_mutex_unlock(&mutex);
  return rc;
}

int pd_gl_present(uint32_t target) {
  int rc = -1;
  int (*present)(void);

  pthread_mutex_lock(&mutex);
  if (state != 1 || !hardware || !last_fbo || target != last_target)
    goto done;

  present = (int (*)(void))dlsym(RTLD_DEFAULT, "pd_radeon_present");
  if (!present || !eglMakeCurrent(display, surface, surface, context))
    goto done;

  glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
  rc = present();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

done:
  pthread_mutex_unlock(&mutex);
  return rc;
}

static int read_present_target(void *destination, uint32_t pitch) {
  static int gpu_readback_checked;
  static int (*gpu_readback)(void *, unsigned);
  const char *read565;
  int packed565;
  size_t row;
  uint8_t *scratch;

  if (!destination || pitch < (uint64_t)last_w * 4 || (pitch & 3))
    return -1;

  read565 = getenv("AIRJITD_GL_READBACK_565");
  packed565 = read565 && !strcmp(read565, "1");
  glBindFramebuffer(GL_READ_FRAMEBUFFER, last_fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);

  // a gpu copy straight into the destination avoids cpu reads over the vram aperture
  if (!gpu_readback_checked) {
    const char *off;

    gpu_readback_checked = 1;
    off = getenv("AIRJITD_GL_CPU_READBACK");
    if (!off || strcmp(off, "1"))
      gpu_readback = (int (*)(void *, unsigned))dlsym(RTLD_DEFAULT, "pd_radeon_readback");
  }

  if (gpu_readback && !packed565 && gpu_readback(destination, pitch) == 0)
    return 0;

  if (packed565) {
    uint16_t *packed;
    int ok;

    row = (size_t)last_w * 2;
    packed = malloc(row * last_h);
    if (!packed)
      return -1;

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glReadPixels(0, 0, last_w, last_h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, packed);
    ok = glGetError() == GL_NO_ERROR;
    if (ok) {
      for (unsigned y = 0; y < last_h; y++) {
        uint8_t *dst = (uint8_t *)destination + (size_t)y * pitch;
        const uint16_t *src = (const uint16_t *)((const uint8_t *)packed + (size_t)(last_h - 1 - y) * row);

        for (unsigned x = 0; x < last_w; x++) {
          uint16_t v = src[x];
          unsigned blue = v & 31, green = (v >> 5) & 63, red = (v >> 11) & 31;
          uint32_t bgra = 0xff000000u | ((red << 3 | red >> 2) << 16) | ((green << 2 | green >> 4) << 8) |
                          (blue << 3 | blue >> 2);

          memcpy(dst + x * 4, &bgra, 4);
        }
      }
    }

    free(packed);
    return ok ? 0 : -1;
  }

  glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(pitch / 4));
  glReadPixels(0, 0, last_w, last_h, GL_BGRA, GL_UNSIGNED_BYTE, destination);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  if (glGetError() != GL_NO_ERROR)
    return -1;

  // gl rows start at the bottom, so flip in place with one row of scratch
  row = (size_t)last_w * 4;
  scratch = malloc(row);
  if (!scratch)
    return -1;

  for (unsigned y = 0; y < last_h / 2; y++) {
    uint8_t *top = (uint8_t *)destination + (size_t)y * pitch;
    uint8_t *bottom = (uint8_t *)destination + (size_t)(last_h - 1 - y) * pitch;

    memcpy(scratch, top, row);
    memcpy(top, bottom, row);
    memcpy(bottom, scratch, row);
  }

  free(scratch);
  return 0;
}

int pd_gl_present_copy(uint32_t off, void *destination, uint32_t pitch) {
  static int reported_present_state;
  int rc = 1, supported;

  pthread_mutex_lock(&mutex);
  supported = state == 1 && hardware && gpu_newer && !cpu_newer && last_fbo && off == last_target &&
              last_format == PD_FMT_BGRA8 && pitch >= (uint64_t)last_w * 4;
  if (!supported && !reported_present_state && getenv("INDIUM_PD_TRACE_PRESENT")) {
    fprintf(stderr,
            "pd-gl: direct present unavailable: state=%d hw=%d gpu_newer=%d cpu_newer=%d fbo=%u off=%08x target=%08x format=%u pitch=%u width=%u\n",
            state, hardware, gpu_newer, cpu_newer, last_fbo, off, last_target, last_format, pitch, last_w);
    reported_present_state = 1;
  }

  if (!supported)
    goto done;

  if (!eglMakeCurrent(display, surface, surface, context)) {
    rc = -1;
    goto done;
  }

  rc = read_present_target(destination, pitch) ? -1 : 0;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

done:
  pthread_mutex_unlock(&mutex);
  return rc;
}

static int decode_present_pixels(const void *source, void *destination, uint32_t pitch, uint32_t width,
                                 uint32_t height, int packed565) {
  size_t row;
  uint8_t *scratch;

  if (!source || !destination || pitch < (uint64_t)width * 4)
    return 0;

  if (packed565) {
    row = (size_t)width * 2;
    for (unsigned y = 0; y < height; y++) {
      uint8_t *dst = (uint8_t *)destination + (size_t)y * pitch;
      const uint16_t *src = (const uint16_t *)((const uint8_t *)source + (size_t)(height - 1 - y) * row);

      for (unsigned x = 0; x < width; x++) {
        uint16_t v = src[x];
        unsigned blue = v & 31, green = (v >> 5) & 63, red = (v >> 11) & 31;
        uint32_t bgra = 0xff000000u | ((red << 3 | red >> 2) << 16) | ((green << 2 | green >> 4) << 8) |
                        (blue << 3 | blue >> 2);

        memcpy(dst + x * 4, &bgra, 4);
      }
    }

    return 1;
  }

  row = (size_t)width * 4;
  scratch = malloc(row);
  if (!scratch)
    return 0;

  for (unsigned y = 0; y < height / 2; y++) {
    uint8_t *top = (uint8_t *)destination + (size_t)y * pitch;
    uint8_t *bottom = (uint8_t *)destination + (size_t)(height - 1 - y) * pitch;

    memcpy(scratch, top, row);
    memcpy(top, bottom, row);
    memcpy(bottom, scratch, row);
  }

  free(scratch);
  return 1;
}

int pd_gl_present_copy_async(uint32_t off, void *destination, uint32_t pitch, void **ready, uint32_t *ready_w,
                             uint32_t *ready_h) {
  int rc = 1, supported, packed565, index;
  const char *read565;
  size_t bytes;

  if (ready)
    *ready = NULL;
  if (ready_w)
    *ready_w = 0;
  if (ready_h)
    *ready_h = 0;

  pthread_mutex_lock(&mutex);
  supported = destination && state == 1 && hardware && gpu_newer && !cpu_newer && last_fbo && off == last_target &&
              last_format == PD_FMT_BGRA8 && pitch >= (uint64_t)last_w * 4;
  if (!supported && present_pending_pbo < 0)
    goto done;

  if (!eglMakeCurrent(display, surface, surface, context)) {
    rc = -1;
    goto done;
  }

  if (present_pending_pbo >= 0) {
    size_t pending_bytes;
    void *mapped;
    int ok;

    index = present_pending_pbo;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, present_pbo[index]);
    pending_bytes = (size_t)present_pending_w * present_pending_h * (present_pending_565 ? 2 : 4);
    mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)pending_bytes, GL_MAP_READ_BIT);
    ok = mapped && decode_present_pixels(mapped, present_pending_pixels, present_pending_pitch, present_pending_w,
                                         present_pending_h, present_pending_565);
    if (mapped && glUnmapBuffer(GL_PIXEL_PACK_BUFFER) != GL_TRUE)
      ok = 0;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    if (ready)
      *ready = present_pending_pixels;
    if (ready_w)
      *ready_w = present_pending_w;
    if (ready_h)
      *ready_h = present_pending_h;

    present_pending_pbo = -1;
    present_pending_pixels = NULL;
    if (!ok) {
      rc = -1;
      goto detach;
    }
  }

  if (!supported) {
    rc = 1;
    goto detach;
  }

  read565 = getenv("AIRJITD_GL_READBACK_565");
  packed565 = read565 && !strcmp(read565, "1");
  bytes = (size_t)last_w * last_h * (packed565 ? 2 : 4);
  index = present_next_pbo;
  if (!present_pbo[index])
    glGenBuffers(1, &present_pbo[index]);

  glBindBuffer(GL_PIXEL_PACK_BUFFER, present_pbo[index]);
  if (present_pbo_size[index] < bytes) {
    glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)bytes, NULL, GL_STREAM_READ);
    present_pbo_size[index] = bytes;
  }

  while (glGetError() != GL_NO_ERROR) {
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, last_fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  if (packed565)
    glReadPixels(0, 0, last_w, last_h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, (void *)0);
  else
    glReadPixels(0, 0, last_w, last_h, GL_BGRA, GL_UNSIGNED_BYTE, (void *)0);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  if (glGetError() != GL_NO_ERROR) {
    rc = -1;
    goto detach;
  }

  present_pending_pbo = index;
  present_pending_pixels = destination;
  present_pending_pitch = pitch;
  present_pending_w = last_w;
  present_pending_h = last_h;
  present_pending_565 = packed565;
  present_next_pbo = 1 - index;
  rc = 2;

detach:
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

done:
  pthread_mutex_unlock(&mutex);
  return rc;
}

int pd_gl_sync(uint32_t off, uint32_t len) {
  int rc = 0;

  pthread_mutex_lock(&mutex);
  if (gpu_newer && overlaps_target(off, len)) {
    if (!eglMakeCurrent(display, surface, surface, context)) {
      rc = -1;
    } else {
      rc = sync_target();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
  }

  pthread_mutex_unlock(&mutex);
  return rc;
}

void pd_gl_dirty(uint32_t off, uint32_t len) {
  pthread_mutex_lock(&mutex);
  if (overlaps_target(off, len)) {
    cpu_newer = 1;
    gpu_newer = 0;
  }

  pthread_mutex_unlock(&mutex);
}

void pd_gl_set_resident(int on) {
  if (!on && pd_gl_sync(0, UINT32_MAX))
    return;

  pthread_mutex_lock(&mutex);
  resident_mode = !!on;
  pthread_mutex_unlock(&mutex);
}

int pd_gl_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t size) {
  struct a2s_reflect reflect = { 0 };
  uint32_t *words = 0;
  size_t nwords = 0;
  struct pd_gl_bindings bindings = { 0 };
  char *source = 0;
  GLuint shader = 0, program = 0, buffer = 0;
  uint8_t *packed = 0;
  uint32_t lo = UINT32_MAX, hi = 0, length;
  int rc = PD_GL_FALLBACK;
  uint64_t threads = 1;
  GLuint groups[3];
  GLint limit, ok, max_ssbo;
  GLint64 max_storage;
  struct a2s_options options = { .quantize_half = 1, .narrow_loads = 1, .quad_memory = 1, .kill_discard = 1 };
  const char *dump;

  pthread_mutex_lock(&mutex);
  if (start())
    goto done;

  if (!compute_supported) {
    decline("OpenGL compute requires version 4.3; using CPU on this device");
    goto done;
  }

  if (!eglMakeCurrent(display, surface, surface, context))
    goto done;

  while (glGetError() != GL_NO_ERROR) {
  }

  if (!span(r->mOff, r->mSize, size)) {
    decline("compute shader outside arena");
    goto done;
  }

  glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &limit);
  for (unsigned i = 0; i < 3; i++) {
    GLint axis, maxgroups;
    uint64_t g;

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &axis);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &maxgroups);
    if (!r->tpg[i] || r->tpg[i] > (unsigned)axis || !r->grid[i] || r->grid[i] > UINT32_MAX) {
      decline("invalid compute dimensions");
      goto done;
    }

    threads *= r->tpg[i];
    g = r->threads ? (r->grid[i] + r->tpg[i] - 1) / r->tpg[i] : r->grid[i];
    if (g > (unsigned)maxgroups) {
      decline("compute grid exceeds device limit");
      goto done;
    }

    groups[i] = g;
  }

  if (threads > (unsigned)limit) {
    decline("compute group exceeds device limit");
    goto done;
  }

  if (a2s_translate(base + r->mOff, r->mSize, &options, &words, &nwords, &reflect, reason, sizeof(reason)))
    goto done;

  if (reflect.stage != A2S_KERNEL || reflect.quad_order || reflect.imageblock_bytes) {
    decline("unsupported compute workgroup layout");
    goto done;
  }

  for (unsigned i = 0; i < reflect.nres; i++) {
    const struct a2s_resource *res = &reflect.res[i];
    const struct PDDrawBuffer *b;

    if (res->kind != A2S_RES_BUFFER || res->metal >= PD_CS_BUFS) {
      decline("compute image resources require image binding support");
      goto done;
    }

    b = &r->buf[res->metal];
    if (!b->size || !span(b->off, b->size, size)) {
      decline("invalid compute buffer");
      goto done;
    }

    if (b->off < lo)
      lo = b->off;
    if (b->off + b->size > hi)
      hi = b->off + b->size;
  }

  if (lo == UINT32_MAX)
    lo = hi = 0;

  // one shared upload keeps aliasing between metal buffer ranges
  length = hi - lo;
  glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_storage);
  if (length > 64 * 1024 * 1024 || length > (uint64_t)max_storage) {
    decline("compute buffer span exceeds device limit");
    goto done;
  }

  if (sync_target()) {
    rc = PD_ERR_BUFFER;
    goto done;
  }

  if (pd_glsl_source(words, nwords, &reflect, 0, 2, r->samp, &bindings, r, &source, reason, sizeof(reason)))
    goto done;

  dump = getenv("AIRJITD_GL_DUMP");
  if (dump && *dump) {
    char path[1024];
    FILE *f;

    snprintf(path, sizeof(path), "%s/compute.glsl", dump);
    f = fopen(path, "w");
    if (f) {
      fputs(source, f);
      fclose(f);
    }
  }

  shader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(shader, 1, (const GLchar *const *)&source, 0);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(shader, sizeof(reason), 0, reason);
    goto done;
  }

  program = glCreateProgram();
  glAttachShader(program, shader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    glGetProgramInfoLog(program, sizeof(reason), 0, reason);
    goto done;
  }

  glUseProgram(program);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, length ? length : 4, base + lo, GL_STREAM_DRAW);
  glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_ssbo);

  for (unsigned i = 0; i < reflect.nres; i++) {
    char name[96];
    GLuint index;

    snprintf(name, sizeof(name), "PDStage2Buffer%u", reflect.res[i].binding);
    index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, name);
    if (index == GL_INVALID_INDEX)
      continue;

    if (i >= (unsigned)max_ssbo) {
      decline("compute buffer binding limit");
      goto done;
    }

    glShaderStorageBlockBinding(program, index, i);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, buffer);
  }

  for (unsigned i = 0; i < reflect.npc; i++) {
    uint32_t value = 0;
    const struct a2s_pc *p = &reflect.pc[i];
    char name[96];

    if (p->kind == A2S_PC_BUFFER_OFFSET) {
      value = r->buf[p->metal].off - lo;
    } else {
      unsigned axis = p->kind - A2S_PC_GRID_X;
      uint64_t v = r->threads ? r->grid[axis] : r->grid[axis] * r->tpg[axis];

      if (v > UINT32_MAX) {
        decline("compute grid overflow");
        goto done;
      }

      value = v;
    }

    snprintf(name, sizeof(name), "pd_stage2_push.word%u", i);
    glUniform1ui(glGetUniformLocation(program, name), value);
  }

  if (glGetError() != GL_NO_ERROR) {
    decline("compute binding failed");
    goto done;
  }

  glDispatchCompute(groups[0], groups[1], groups[2]);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  glFinish();

  packed = malloc(length ? length : 4);
  if (!packed) {
    rc = PD_ERR_BUFFER;
    goto done;
  }

  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, length, packed);
  if (glGetError() != GL_NO_ERROR) {
    decline("compute execution/readback failed");
    goto done;
  }

  for (unsigned i = 0; i < reflect.nres; i++) {
    if (reflect.res[i].access & A2S_WRITE) {
      const struct PDDrawBuffer *b = &r->buf[reflect.res[i].metal];

      memcpy(base + b->off, packed + b->off - lo, b->size);
      if (overlaps_target(b->off, b->size)) {
        cpu_newer = 1;
        gpu_newer = 0;
      }
    }
  }

  rc = 0;

done:
  if (state > 0) {
    glUseProgram(0);
    if (buffer)
      glDeleteBuffers(1, &buffer);
    if (program)
      glDeleteProgram(program);
    if (shader)
      glDeleteShader(shader);
  }

  if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT)
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  free(source);
  free(words);
  free(packed);
  pthread_mutex_unlock(&mutex);
  return rc;
}

int pd_gl_clear(const struct PDDrawTex *t, const float color[4], uint8_t *base, uint32_t size) {
  int rc = 0;
  GLuint fbo = 0, texture = 0;
  int borrowed = 0;
  struct gl_format format;

  pthread_mutex_lock(&mutex);
  if (!resident_mode || !pd_gl_enabled() || start() || !base || !t->w || !t->h || t->w > (unsigned)max_texture ||
      t->h > (unsigned)max_texture || pd_fmt_is_int(t->fmt) || t->bpr < (uint64_t)t->w * tex_bpp(t->fmt) ||
      !span(t->off, (uint64_t)t->bpr * t->h, size))
    goto done;

  format = color_format(t->fmt);
  if (!format.internal || !eglMakeCurrent(display, surface, surface, context))
    goto done;

  while (glGetError() != GL_NO_ERROR) {
  }

  borrowed = last_fbo && last_target == t->off && last_w == t->w && last_h == t->h && last_pitch == t->bpr &&
             last_format == t->fmt && last_base == base;
  if (borrowed) {
    fbo = last_fbo, texture = last_texture;
  } else {
    if (sync_target())
      goto done;

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal, t->w, t->h, 0, format.format, format.type, 0);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    goto done;

  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (t->fmt == PD_FMT_BGRA8_SRGB || t->fmt == PD_FMT_RGBA8_SRGB)
    glEnable(GL_FRAMEBUFFER_SRGB);
  else
    glDisable(GL_FRAMEBUFFER_SRGB);

  glClearBufferfv(GL_COLOR, 0, color);
  glFinish();
  if (glGetError() != GL_NO_ERROR)
    goto done;

  if (!borrowed && last_fbo)
    glDeleteFramebuffers(1, &last_fbo);
  if (!borrowed && last_texture)
    glDeleteTextures(1, &last_texture);

  last_fbo = fbo, last_texture = texture;
  last_target = t->off, last_pitch = t->bpr;
  last_w = t->w, last_h = t->h;
  last_format = t->fmt, last_base = base;
  gpu_newer = 1, cpu_newer = 0;
  fbo = texture = 0;
  rc = 1;

done:
  if (state > 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (fbo && !borrowed)
      glDeleteFramebuffers(1, &fbo);
    if (texture && !borrowed)
      glDeleteTextures(1, &texture);
  }

  if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT)
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  pthread_mutex_unlock(&mutex);
  return rc;
}
