#ifndef IOVIRTIOGPU_3D_SHARED_H
#define IOVIRTIOGPU_3D_SHARED_H

#include <stdint.h>

/* IOServiceOpen type that selects the 3D user client (vs IOFramebuffer's own
 * client types, which are small integers). */
#define kIOVirtIOGPU3DConnectType 0x76697267 /* 'virg' */

/* externalMethod selectors. */
enum {
    kPDVirgl_GetCaps = 0,      /* structOut: caps blob                          */
    kPDVirgl_CreateContext,    /* scalarOut[0]=ctxId                            */
    kPDVirgl_DestroyContext,   /* scalarIn[0]=ctxId                             */
    kPDVirgl_CreateResource,   /* structIn: ResourceCreate; scalarOut[0]=resId,
                                *           scalarOut[1]=backingSize            */
    kPDVirgl_DestroyResource,  /* scalarIn[0]=resId                             */
    kPDVirgl_AttachResource,   /* scalarIn[0]=ctxId, scalarIn[1]=resId          */
    kPDVirgl_TransferToHost,   /* structIn: Transfer                            */
    kPDVirgl_TransferFromHost, /* structIn: Transfer                           */
    kPDVirgl_SubmitCmd,        /* scalarIn[0]=ctxId, scalarIn[1]=fenceId;
                                *   structIn = virgl command stream bytes       */
    kPDVirgl_WaitFence,        /* scalarIn[0]=fenceId (v1: submits are sync)     */
    kPDVirgl_AllocFenceId,     /* scalarOut[0]=fenceId                          */
    kPDVirgl_MethodCount
};

struct PDVirglResourceCreate {
    uint32_t target;      /* pipe_texture_target (2 = TEXTURE_2D)     */
    uint32_t format;      /* virgl_formats                            */
    uint32_t bind;        /* VIRGL_BIND_*                             */
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t bytes_per_pixel; /* backing-size fallback; 4 for BGRA8    */
    uint32_t size;            /* explicit backing size; 0 = w*h*bpp    */
};

/* kPDVirgl_TransferToHost / _TransferFromHost input. */
struct PDVirglTransfer {
    uint32_t ctx_id;
    uint32_t resource_id;
    uint32_t x, y, z;
    uint32_t w, h, d;
    uint32_t level;
    uint32_t stride;
    uint64_t offset;
    uint64_t fence_id;    /* 0 = no fence                            */
};

#endif /* IOVIRTIOGPU_3D_SHARED_H */
