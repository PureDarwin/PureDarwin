#ifndef PD_VIRGL_SHIM_H
#define PD_VIRGL_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pd_virgl_conn pd_virgl_conn;

pd_virgl_conn *pd_virgl_open(void);
// the 3D connection when there is one, else the 2D framebuffer client, which has the
// VirtIO hardware cursor calls too
pd_virgl_conn *pd_virgl_open_cursor(void);
void           pd_virgl_close(pd_virgl_conn *c);

uint32_t pd_virgl_create_context(pd_virgl_conn *c);
void     pd_virgl_destroy_context(pd_virgl_conn *c, uint32_t ctx_id);

struct pd_virgl_res_create {
   uint32_t target;
   uint32_t format;
   uint32_t bind;
   uint32_t width, height, depth;
   uint32_t array_size;
   uint32_t last_level;
   uint32_t nr_samples;
   uint32_t bytes_per_pixel;
   uint32_t size;
};

int pd_virgl_create_resource(pd_virgl_conn *c,
                             const struct pd_virgl_res_create *rc,
                             uint32_t *out_res_id,
                             void **out_ptr, uint64_t *out_size);

void pd_virgl_destroy_resource(pd_virgl_conn *c, uint32_t res_id,
                               void *mapped_ptr);

void pd_virgl_attach_resource(pd_virgl_conn *c, uint32_t ctx_id, uint32_t res_id);

int pd_virgl_transfer(pd_virgl_conn *c, int to_host, uint32_t ctx_id,
                      uint32_t res_id, uint32_t x, uint32_t y, uint32_t z,
                      uint32_t w, uint32_t h, uint32_t d, uint32_t level,
                      uint32_t stride, uint64_t offset, uint64_t fence_id);

int pd_virgl_submit(pd_virgl_conn *c, uint32_t ctx_id,
                    const uint32_t *cmd, uint32_t cmd_dwords, uint64_t fence_id);

int pd_virgl_get_caps(pd_virgl_conn *c, void *caps, size_t *inout_len);

uint64_t pd_virgl_alloc_fence(pd_virgl_conn *c);

int pd_virgl_present(pd_virgl_conn *c, uint32_t x, uint32_t y,
                     uint32_t width, uint32_t height);

/* Hardware cursor. A NULL image hides it. Max 64x64 BGRA. Both return 0 on
 * success and non-zero when the device has no cursor queue. */
int pd_virgl_set_cursor(pd_virgl_conn *c, const void *bgra, uint32_t width,
                        uint32_t height, uint32_t hot_x, uint32_t hot_y);
int pd_virgl_move_cursor(pd_virgl_conn *c, uint32_t x, uint32_t y);

/* Scan out a virgl resource directly instead of the driver's framebuffer, so
 * host-rendered pixels never have to travel back through guest memory. Pass
 * res_id 0 to restore the framebuffer. */
int pd_virgl_set_scanout_resource(pd_virgl_conn *c, uint32_t res_id,
                                  uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* PD_VIRGL_SHIM_H */
