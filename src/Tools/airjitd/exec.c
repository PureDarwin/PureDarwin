#include "exec.h"

#include "airjitd.h"
#include "compute.h"
#include "raster.h"
#include "tile.h"
#ifdef AIRJITD_OPENGL
#include "opengl/gl.h"
#endif
#ifdef AIRJITD_VULKAN
#include "vulkan/vk.h"
#endif

int pd_exec_on_vulkan;
int pd_exec_on_opengl;

#ifdef AIRJITD_VULKAN
// the bytes a texture spans with its levels and slices
static uint32_t tex_span(const struct PDDrawTex *t, const struct PDTexLayout *l) {
  uint32_t end = t->bpr * t->h, k;

  if (!l || !(l->flags & PD_TEXLAYOUT_SET))
    return end;
  if (l->type != 7)
    return l->sliceBytes * (l->slices ? l->slices : 1) > end ? l->sliceBytes * (l->slices ? l->slices : 1) : end;
  for (k = 1; k < l->levels && k < PD_TEX_LEVELS; k++) {
    uint32_t h = t->h >> k ? t->h >> k : 1, d = l->slices >> k ? l->slices >> k : 1, e = l->level[k].off + l->level[k].bpr * h * d;
    end = e > end ? e : end;
  }
  return end * (l->slices ? l->slices : 1);
}

static void dirty_tex(const struct PDDrawTex *t, const struct PDTexLayout *l) {
  if (t->w)
    pd_vk_dirty(t->off, tex_span(t, l));
}

// a job the Vulkan executor declined runs on the CPU against the bridge: resident copies the GPU wrote come
// back first, and what the CPU may have written is uploaded again before the GPU next reads it
static void before_cpu(void) {
  if (pd_vk_resident())
    pd_vk_sync_all();
}

static void after_cpu_draw(const struct PDDrawRequest *r) {
  const struct PDDrawExt *x = pd_draw_ext(r);
  struct PDDrawTex t = { 0, r->targetOff, r->tw, r->th, r->tbpr, r->tfmt };

  if (!pd_vk_resident())
    return;
  dirty_tex(&t, 0);
  for (unsigned k = 1; k < 4; k++) {
    dirty_tex(&r->rt[k], 0);
  }
  if (x && x->depth.w)
    dirty_tex(&x->depth, 0);
}

static void after_cpu_compute(const struct PDDrawTex *tex, const struct PDComputeExt *x, const struct PDDrawTex *rt) {
  if (!pd_vk_resident())
    return;
  for (unsigned k = 0; k < PD_DRAW_TEXS; k++) {
    dirty_tex(&tex[k], x ? &x->texLayout[k] : 0);
  }
  for (unsigned k = 0; rt && k < 4; k++) {
    dirty_tex(&rt[k], 0);
  }
}
#endif

int pd_exec_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t size, uint32_t serial, pd_stage_ready ready) {
  int rc;

  pd_exec_on_vulkan = 0;
  pd_exec_on_opengl = 0;
#ifdef AIRJITD_OPENGL
  if (pd_gl_enabled()) {
#ifdef AIRJITD_VULKAN
    before_cpu();
#endif
    rc = pd_gl_draw(r, base, size);
    if (rc != PD_GL_FALLBACK) {
      pd_exec_on_opengl = rc == 0;
#ifdef AIRJITD_VULKAN
      if (!rc) after_cpu_draw(r);
#endif
      return rc;
    }
  }
#endif
#ifdef AIRJITD_OPENGL
  if (pd_gl_sync(0, UINT32_MAX)) return PD_ERR_BUFFER;
#endif
#ifdef AIRJITD_VULKAN
  if (pd_vk_enabled() && (rc = pd_vk_draw(r, base, size)) != PD_VK_FALLBACK) {
    pd_exec_on_vulkan = 1;
#ifdef AIRJITD_OPENGL
    if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
    return rc;
  }
#endif

  if ((rc = ready(r->vfn, base + r->vmOff, r->vmSize, "vertex stage")) ||
      (rc = ready(r->ffn, base + r->fmOff, r->fmSize, "fragment stage")))
    return rc;
#ifdef AIRJITD_VULKAN
  before_cpu();
  rc = pd_run_draw(r, base, size, serial);
  after_cpu_draw(r);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#else
  rc = pd_run_draw(r, base, size, serial);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#endif
}

int pd_exec_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t size, pd_stage_ready ready) {
  int rc;

  pd_exec_on_vulkan = 0;
  pd_exec_on_opengl = 0;
#ifdef AIRJITD_OPENGL
  if (pd_gl_enabled()) {
#ifdef AIRJITD_VULKAN
    before_cpu();
#endif
    rc=pd_gl_compute(r,base,size);
    if (rc!=PD_GL_FALLBACK) {
      pd_exec_on_opengl=rc==0;
#ifdef AIRJITD_VULKAN
      if (!rc) after_cpu_compute(r->tex,pd_compute_ext(&r->ext),0);
#endif
      return rc;
    }
  }
#endif

#ifdef AIRJITD_OPENGL
  if (pd_gl_sync(0, UINT32_MAX)) return PD_ERR_BUFFER;
#endif
#ifdef AIRJITD_VULKAN
  if (pd_vk_enabled() && (rc = pd_vk_compute(r, base, size)) != PD_VK_FALLBACK) {
    pd_exec_on_vulkan = 1;
#ifdef AIRJITD_OPENGL
    if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
    return rc;
  }
#endif

  if ((rc = ready(r->fn, base + r->mOff, r->mSize, "kernel")))
    return rc;
#ifdef AIRJITD_VULKAN
  before_cpu();
  rc = pd_run_compute(r, base, size);
  after_cpu_compute(r->tex, pd_compute_ext(&r->ext), 0);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#else
  rc = pd_run_compute(r, base, size);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#endif
}

int pd_exec_tile(struct PDTileRequest *r, uint8_t *base, uint32_t size, pd_stage_ready ready) {
  int rc;

  pd_exec_on_vulkan = 0;
  pd_exec_on_opengl = 0;
#ifdef AIRJITD_OPENGL
  if (pd_gl_sync(0, UINT32_MAX)) return PD_ERR_BUFFER;
#endif
#ifdef AIRJITD_VULKAN
  if (pd_vk_enabled() && (rc = pd_vk_tile(r, base, size)) != PD_VK_FALLBACK) {
    pd_exec_on_vulkan = 1;
#ifdef AIRJITD_OPENGL
    if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
    return rc;
  }
#endif

  if ((rc = ready(r->fn, base + r->mOff, r->mSize, "tile kernel")))
    return rc;
#ifdef AIRJITD_VULKAN
  before_cpu();
  rc = pd_run_tile(r, base, size);
  after_cpu_compute(r->tex, pd_compute_ext(&r->ext), r->rt);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#else
  rc = pd_run_tile(r, base, size);
#ifdef AIRJITD_OPENGL
  if (!rc) pd_gl_dirty(0, UINT32_MAX);
#endif
  return rc;
#endif
}
