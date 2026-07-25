#ifndef PD_GL_SURFACE_H
#define PD_GL_SURFACE_H

#include <PDGOP.h>
#include <GL/osmesa.h>

typedef struct {
    PDGOPFramebuffer fb;    /* opened GOP framebuffer (geometry + mmap'd VRAM) */
    OSMesaContext    ctx;   /* softpipe OSMesa context, made current on create  */
    unsigned char   *buf;   /* RGBA8 render target, width*height*4              */
    int              width;
    int              height;
} pdgl_surface_t;

int pdgl_surface_create(pdgl_surface_t *s);
void pdgl_surface_swap(pdgl_surface_t *s);
void pdgl_surface_destroy(pdgl_surface_t *s);

#endif /* PD_GL_SURFACE_H */
