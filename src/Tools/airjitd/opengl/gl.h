#ifndef AIRJITD_GL_H
#define AIRJITD_GL_H

#include "../bridge.h"

#define PD_GL_FALLBACK 1

int pd_gl_enabled(void);
int pd_gl_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t size);
int pd_gl_clear(const struct PDDrawTex *t, const float color[4], uint8_t *base, uint32_t size);
int pd_gl_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t size);
const char *pd_gl_reason(void);
const char *pd_gl_renderer(void);
int pd_gl_hardware(void);
// Optional native Mesa framebuffer bridge; caller owns the display connection.
int pd_gl_present(uint32_t target);
void pd_gl_set_resident(int on);
int pd_gl_sync(uint32_t off, uint32_t len);
// Read a resident BGRA8 render target to a caller-owned system-memory buffer.
// Returns 1 when the target cannot use this path, 0 on success, or -1 on failure.
int pd_gl_present_copy(uint32_t off, void *destination, uint32_t pitch);
int pd_gl_present_copy_async(uint32_t off, void *destination, uint32_t pitch,
                             void **ready, uint32_t *ready_w, uint32_t *ready_h);
void pd_gl_dirty(uint32_t off, uint32_t len);

#endif
