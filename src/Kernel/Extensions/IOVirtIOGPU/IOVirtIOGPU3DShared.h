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
    kPDGPU_Present,             /* structIn: PDGpuPresent damage rectangle      */
    kPDGPU_SetCursor,           /* structIn: PDGpuCursorImage + BGRA pixels;
                                 *   a zero-sized image hides the cursor        */
    kPDGPU_MoveCursor,          /* scalarIn[0]=x, scalarIn[1]=y                 */
    kPDGPU_SetScanoutResource,  /* scalarIn[0]=resId (0 restores the driver's
                                 *   framebuffer), [1]=width, [2]=height       */
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

/* kPDGPU_SetCursor input: this header immediately followed by width*height
 * BGRA pixels. virtio-gpu cursors are at most 64x64. */
struct PDGpuCursorImage {
    uint32_t width;
    uint32_t height;
    uint32_t hot_x;
    uint32_t hot_y;
};

struct PDGpuPresent {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

/* venus: a context-init client with blob resources, the host visible window and per-ring fences,
 * the pieces the DRM virtio-gpu ioctls need. One 3D context per connection, like a DRM file. */
#define kIOVirtIOGPUVenusConnectType 0x766e7573 /* 'vnus' */

enum {
    kPDVenus_GetInfo = 0,   /* structOut: PDVenusInfo                                        */
    kPDVenus_GetCaps,       /* scalarIn[0]=capset id, [1]=version; structOut: caps (truncated) */
    kPDVenus_ContextInit,   /* scalarIn[0]=capset id (0 = plain virgl); scalarOut[0]=ctx id   */
    kPDVenus_CreateBlob,    /* structIn: PDVenusBlobCreate; scalarOut[0]=res id, [1]=size     */
    kPDVenus_ResourceInfo,  /* scalarIn[0]=res id; scalarOut[0]=size, [1]=blob mem, [2]=flags */
    kPDVenus_Unref,         /* scalarIn[0]=res id                                            */
    kPDVenus_Map,           /* scalarIn[0]=res id; scalarOut[0]=map info, [1]=size. then
                             *   IOConnectMapMemory64(type = res id) maps the blob            */
    kPDVenus_Submit,        /* scalarIn[0]=ring idx, [1]=PD_VENUS_SUBMIT_* flags; structIn =
                             *   command stream (may be empty); scalarOut[0]=fence id or 0    */
    kPDVenus_FenceStatus,   /* structIn: uint64_t fence ids; structOut: uint8_t retired each  */
    kPDVenus_WaitFence,     /* scalarIn[0]=fence id, [1]=timeout us; kIOReturnTimeout         */
    kPDVenus_WaitProgress,  /* scalarIn[0]=generation seen, [1]=timeout us; scalarOut[0]=now  */
    kPDVenus_Export,        /* scalarIn[0]=res id; scalarOut[0]=token, valid while the exporting
                             *   connection is open (it holds a ref)                          */
    kPDVenus_Import,        /* scalarIn[0]=token; scalarOut[0]=res id, [1]=size, [2]=blob mem,
                             *   [3]=flags. any connection, other tasks included              */
    kPDVenus_MethodCount
};

#define PD_VENUS_SUBMIT_RING  0x1   /* ring idx is meaningful (context fence timeline) */
#define PD_VENUS_SUBMIT_FENCE 0x2   /* fence the submit; the call returns once queued   */

/* VIRTIO_GPU_MAP_CACHE_* as the host reported for a mapped blob */
#define PD_VENUS_MAP_CACHE_MASK     0x0f
#define PD_VENUS_MAP_CACHE_NONE     0x00
#define PD_VENUS_MAP_CACHE_CACHED   0x01
#define PD_VENUS_MAP_CACHE_UNCACHED 0x02
#define PD_VENUS_MAP_CACHE_WC       0x03

struct PDVenusInfo {
    uint32_t features;      /* acked VIRTIO_GPU_F_* bits                       */
    uint32_t capset_mask;   /* 1 << capset id for every capset the host offers */
    uint64_t hostmem_size;  /* host visible window, 0 = none                   */
};

struct PDVenusBlobCreate {
    uint32_t blob_mem;      /* VIRTGPU_BLOB_MEM_* (host3d only for now)        */
    uint32_t blob_flags;    /* VIRTGPU_BLOB_FLAG_*                             */
    uint64_t blob_id;
    uint64_t size;
};

#endif /* IOVIRTIOGPU_3D_SHARED_H */
