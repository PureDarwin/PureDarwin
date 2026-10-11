// metal-cpp names IOSurfaceRef in texture signatures; PD's Metal has no IOSurface-backed textures yet
#ifndef PD_IOSURFACE_IOSURFACEREF_H
#define PD_IOSURFACE_IOSURFACEREF_H
typedef struct __IOSurface *IOSurfaceRef;
#endif
