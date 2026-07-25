#include "pdgl_surface.h"

#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int
pdgl_surface_create(pdgl_surface_t *s)
{
    memset(s, 0, sizeof *s);

    kern_return_t kr = PDGOPOpen(&s->fb);
    if (kr != 0) {
        printf("pdgl: PDGOPOpen failed at stage '%s' (0x%x)\n",
               PDGOPLastErrorStage(), kr);
        return 1;
    }
    s->width  = (int)s->fb.width;
    s->height = (int)s->fb.height;

    s->ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!s->ctx) {
        printf("pdgl: OSMesaCreateContextExt failed\n");
        return 1;
    }

    s->buf = (unsigned char *)malloc((size_t)s->width * s->height * 4);
    if (!s->buf) {
        printf("pdgl: buffer malloc(%dx%d) failed\n", s->width, s->height);
        return 1;
    }

    if (!OSMesaMakeCurrent(s->ctx, s->buf, GL_UNSIGNED_BYTE, s->width, s->height)) {
        printf("pdgl: OSMesaMakeCurrent failed\n");
        return 1;
    }
    /* Store rows top-to-bottom so buffer row 0 == top scanline, matching the
     * framebuffer origin (OSMesa defaults to OpenGL's bottom-up Y_UP=1). */
    OSMesaPixelStore(OSMESA_Y_UP, 0);
    return 0;
}

void
pdgl_surface_swap(pdgl_surface_t *s)
{
    glFinish();

    /* RGBA -> GOP 32bpp BGRX (0x00RRGGBB in a little-endian uint32, i.e. B,G,R,X
     * byte order in memory), row by row honouring the framebuffer stride. */
    uint8_t *base = (uint8_t *)(uintptr_t)s->fb.address;
    const int W = s->width, H = s->height;
    for (int y = 0; y < H; y++) {
        uint32_t *out = (uint32_t *)(base + (uint64_t)y * s->fb.stride);
        const unsigned char *row = &s->buf[(size_t)y * W * 4];
        for (int x = 0; x < W; x++) {
            out[x] = ((uint32_t)row[x * 4 + 0] << 16) |
                     ((uint32_t)row[x * 4 + 1] << 8)  |
                      (uint32_t)row[x * 4 + 2];
        }
    }
}

void
pdgl_surface_destroy(pdgl_surface_t *s)
{
    if (s->ctx) {
        OSMesaDestroyContext(s->ctx);
        s->ctx = NULL;
    }
    free(s->buf);
    s->buf = NULL;
    PDGOPClose(&s->fb);
    memset(s, 0, sizeof *s);
}
