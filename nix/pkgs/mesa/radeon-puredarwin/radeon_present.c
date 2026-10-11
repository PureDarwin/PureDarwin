/* SPDX-License-Identifier: MIT */
#include "main/context.h"
#include "main/framebuffer.h"
#include "main/readpix.h"
#include "main/renderbuffer.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_cb_flush.h"
#include "radeon/puredarwin/radeon_puredarwin.h"

int pd_radeon_present(void);

// native bring-up bridge: present the current GL color buffer to the primary
// IOFramebuffer scanout. The caller holds its exclusive server connection.
__attribute__((visibility("default"))) int pd_radeon_present(void)
{
    struct gl_context *ctx=_mesa_get_current_context();
    if(!ctx || !ctx->ReadBuffer) return -1;
    struct st_context *st=st_context(ctx);
    st_finish(st);
    struct gl_renderbuffer *rb=_mesa_get_read_renderbuffer_for_format(ctx,GL_RGBA);
    if(!rb || !rb->texture) return -1;
    return pd_radeon_present_resource(st->pipe,rb->texture,rb->surface.level,
        rb->surface.first_layer,_mesa_fb_orientation(ctx->ReadBuffer)!=Y_0_TOP) ? 0 : -1;
}

int pd_radeon_readback(void *dst, unsigned pitch);

// copy the current GL color buffer into caller memory using the GPU.
__attribute__((visibility("default"))) int pd_radeon_readback(void *dst, unsigned pitch)
{
    struct gl_context *ctx=_mesa_get_current_context();
    if(!ctx || !ctx->ReadBuffer) return -1;
    struct st_context *st=st_context(ctx);
    st_finish(st);
    struct gl_renderbuffer *rb=_mesa_get_read_renderbuffer_for_format(ctx,GL_RGBA);
    if(!rb || !rb->texture) return -1;
    return pd_radeon_readback_resource(st->pipe,rb->texture,rb->surface.level,
        rb->surface.first_layer,_mesa_fb_orientation(ctx->ReadBuffer)!=Y_0_TOP,dst,pitch) ? 0 : -1;
}
