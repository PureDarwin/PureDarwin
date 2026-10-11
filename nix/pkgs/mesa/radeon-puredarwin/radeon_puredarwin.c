/* SPDX-License-Identifier: MIT */
/* PM4 relocation semantics adapted from Linux r600_cs.c. */
/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */

#include "radeon_puredarwin.h"
#include "r600/r600_public.h"
#include "r600/r600_pipe_common.h"
#include "frontend/sw_winsys.h"
#include "frontend/winsys_handle.h"
#include "util/format/u_format.h"
#include "util/box.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/os_time.h"
#include "drm-uapi/radeon_drm.h"
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#if defined(__x86_64__)
#include <smmintrin.h>
#endif

struct pd_bo {
    struct pb_buffer_lean base;
    struct pd_winsys *ws;
    uint64_t handle, address;
    mach_vm_address_t map;
    mach_vm_size_t map_size;
    void *read_shadow;
    enum radeon_bo_flag flags;
    struct radeon_bo_metadata metadata;
    bool scanout;
};
struct radeon_winsys_ctx {
    struct pd_winsys *ws;
    enum pipe_reset_status reset;
};
struct pipe_fence_handle {
    struct pipe_reference reference;
    atomic_bool complete;
};
struct pd_cs {
    struct radeon_winsys_ctx *ctx;
    void (*flush)(void *, unsigned, struct pipe_fence_handle **);
    void *flush_ctx;
    struct pb_buffer_lean *bos[512];
    unsigned usage[512], count;
    struct pipe_fence_handle *next;
};
static bool unref(struct radeon_winsys *ws) { return true; }
static void destroy(struct radeon_winsys *rws) {
    struct pd_winsys *ws = (void *)rws;
    pipe_resource_reference(&ws->present_buffers[0],NULL);
    pipe_resource_reference(&ws->present_buffers[1],NULL);
    pipe_resource_reference(&ws->readback_buffer,NULL);
    pipe_resource_reference(&ws->scanout_resource,NULL);
    radeon_surface_manager_free(ws->surf_man);
    IOServiceClose(ws->conn);
    free(ws);
}
static int get_fd(struct radeon_winsys *ws) { return -1; }
static void query_info(struct radeon_winsys *ws, struct radeon_info *info) { *info = ((struct pd_winsys *)ws)->info; }
static void fence_reference(struct radeon_winsys *ws, struct pipe_fence_handle **dst, struct pipe_fence_handle *src) {
    struct pipe_fence_handle *old = *dst;
    if (pipe_reference(old ? &old->reference : NULL, src ? &src->reference : NULL)) free(old);
    *dst = src;
}
static bool fence_wait(struct radeon_winsys *ws, struct pipe_fence_handle *f, uint64_t timeout) {
    return !f || atomic_load(&f->complete);
}
static struct pipe_fence_handle *new_fence(void) {
    struct pipe_fence_handle *f = calloc(1, sizeof(*f));
    if (f) { pipe_reference_init(&f->reference, 1); atomic_init(&f->complete, false); }
    return f;
}
static struct pb_buffer_lean *buffer_create(struct radeon_winsys *rws, uint64_t size, unsigned alignment, enum radeon_bo_domain domain, enum radeon_bo_flag flags) {
    struct pd_winsys *ws = (void *)rws;
    if (ws->failed || flags & (RADEON_FLAG_SPARSE | RADEON_FLAG_ENCRYPTED | RADEON_FLAG_NO_VMA)) return NULL;
    struct pd_bo *bo = calloc(1, sizeof(*bo));
    if (!bo) return NULL;
    alignment = MAX2(alignment, 4096);
    uint64_t in[2] = { size, alignment }, out[3]; uint32_t n = 3;
    if (IOConnectCallScalarMethod(ws->conn, PD_RADEON_ALLOC, in, 2, out, &n)) { free(bo); return NULL; }
    bo->ws = ws; bo->handle = out[0]; bo->address = out[1]; bo->base.size = out[2]; bo->flags = flags;
    bo->base.alignment_log2 = util_logbase2(alignment);
    pipe_reference_init(&bo->base.reference, 1); ws->allocated += out[2];
    return &bo->base;
}
static void buffer_destroy(struct radeon_winsys *ws, struct pb_buffer_lean *buf) {
    struct pd_bo *bo = (void *)buf;
    if (bo->map) IOConnectUnmapMemory64(bo->ws->conn, bo->handle, mach_task_self(), bo->map);
    IOConnectCallScalarMethod(bo->ws->conn, PD_RADEON_FREE, &bo->handle, 1, NULL, NULL);
    if (!bo->scanout) bo->ws->allocated -= bo->base.size;
    free(bo->read_shadow);
    free(bo);
}
static bool cs_referenced(struct radeon_cmdbuf *rcs, struct pb_buffer_lean *bo, unsigned usage) {
    if (!rcs || !rcs->priv) return false;
    struct pd_cs *cs = rcs->priv;
    for (unsigned i=0; i<cs->count; i++) if (cs->bos[i] == bo && (cs->usage[i]&usage)) return true;
    return false;
}
#if defined(__x86_64__)
static bool streaming_reads_supported(void) {
    unsigned eax=1, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return !!(ecx & (1u << 19));
}
/* Render BOs are mapped WC by the kernel. Streaming loads fetch complete
 * cache lines instead of issuing a PCIe read for each ordinary scalar load.
 * Only read-only maps use this shadow; uploads retain their persistent BAR map. */
__attribute__((target("sse4.1")))
static void copy_wc_read(void *destination, const void *source, size_t size) {
    unsigned char *out=destination;
    const unsigned char *in=source;
    __asm__ volatile("mfence" ::: "memory");
    size_t offset=0;
    for (; offset+64<=size; offset+=64) {
        __m128i a=_mm_stream_load_si128((__m128i *)(in+offset));
        __m128i b=_mm_stream_load_si128((__m128i *)(in+offset+16));
        __m128i c=_mm_stream_load_si128((__m128i *)(in+offset+32));
        __m128i d=_mm_stream_load_si128((__m128i *)(in+offset+48));
        _mm_storeu_si128((__m128i *)(out+offset),a);
        _mm_storeu_si128((__m128i *)(out+offset+16),b);
        _mm_storeu_si128((__m128i *)(out+offset+32),c);
        _mm_storeu_si128((__m128i *)(out+offset+48),d);
    }
    for (;offset<size;offset++) out[offset]=in[offset];
    __asm__ volatile("mfence" ::: "memory");
}
#endif
static void *buffer_map(struct radeon_winsys *ws, struct pb_buffer_lean *buf, struct radeon_cmdbuf *rcs, enum pipe_map_flags usage) {
    struct pd_bo *bo = (void *)buf;
    if (!(usage & PIPE_MAP_UNSYNCHRONIZED) && cs_referenced(rcs, buf, RADEON_USAGE_READWRITE)) {
        if (usage & PIPE_MAP_DONTBLOCK) return NULL;
        struct pd_cs *cs = rcs->priv; cs->flush(cs->flush_ctx, 0, NULL);
    }
    if (bo->ws->failed) return NULL;
    if (!bo->map && IOConnectMapMemory64(bo->ws->conn, bo->handle, mach_task_self(), &bo->map, &bo->map_size, kIOMapAnywhere)) return NULL;
#if defined(__x86_64__)
    if ((usage & (PIPE_MAP_READ | PIPE_MAP_WRITE)) == PIPE_MAP_READ &&
        !(bo->map & 63) && bo->base.size <= bo->map_size && streaming_reads_supported() &&
        !getenv("PD_RADEON_DIRECT_READ")) {
        if (!bo->read_shadow) bo->read_shadow=malloc(bo->base.size);
        if (bo->read_shadow) {
            copy_wc_read(bo->read_shadow,(const void *)(uintptr_t)bo->map,bo->base.size);
            return bo->read_shadow;
        }
    }
#endif
    return (void *)(uintptr_t)bo->map;
}
static void buffer_unmap(struct radeon_winsys *ws, struct pb_buffer_lean *buf) { /* persistent BAR mapping */ }
static bool buffer_wait(struct radeon_winsys *ws, struct pb_buffer_lean *buf, uint64_t timeout, unsigned usage) { return !((struct pd_winsys *)ws)->failed; }
static uint64_t buffer_address(struct pb_buffer_lean *buf) { return ((struct pd_bo *)buf)->address; }
static unsigned buffer_reloc(struct pb_buffer_lean *buf) { return 0; }
static enum radeon_bo_domain buffer_domain(struct pb_buffer_lean *buf) { return RADEON_DOMAIN_VRAM; }
static enum radeon_bo_flag buffer_flags(struct pb_buffer_lean *buf) { return ((struct pd_bo *)buf)->flags; }
static bool bo_false(struct pb_buffer_lean *buf) { return false; }
static void get_metadata(struct radeon_winsys *ws, struct pb_buffer_lean *buf, struct radeon_bo_metadata *md, struct radeon_surf *surf) { *md = ((struct pd_bo *)buf)->metadata; }
static void set_metadata(struct radeon_winsys *ws, struct pb_buffer_lean *buf, struct radeon_bo_metadata *md, const struct radeon_surf *surf) { ((struct pd_bo *)buf)->metadata = *md; }
static struct pb_buffer_lean *from_handle(struct radeon_winsys *rws, struct winsys_handle *handle, unsigned alignment, bool linear) {
    struct pd_winsys *ws=(void *)rws;
    if(handle->type!=WINSYS_HANDLE_TYPE_KMS || !ws->scanout.handle || handle->handle!=ws->scanout.handle) return NULL;
    struct pd_bo *bo=calloc(1,sizeof(*bo)); if(!bo) return NULL;
    pipe_reference_init(&bo->base.reference,1);
    bo->base.size=ws->scanout.size; bo->base.alignment_log2=12;
    bo->ws=ws; bo->handle=ws->scanout.handle; bo->address=ws->scanout.offset; bo->scanout=true;
    bo->metadata.u.legacy.stride=ws->scanout.stride; bo->metadata.u.legacy.scanout=true;
    return &bo->base;
}
static bool get_handle(struct radeon_winsys *ws, struct pb_buffer_lean *buf, struct winsys_handle *handle) { return false; }
static struct radeon_winsys_ctx *ctx_create(struct radeon_winsys *ws, unsigned flags) {
    struct radeon_winsys_ctx *ctx = calloc(1,sizeof(*ctx)); if(ctx) ctx->ws=(void *)ws; return ctx;
}
static void ctx_destroy(struct radeon_winsys_ctx *ctx) { free(ctx); }
static void reset_status(struct radeon_winsys_ctx *ctx, enum pipe_reset_status status, const char *fmt, ...) { ctx->reset=status; }
static enum pipe_reset_status query_reset(struct radeon_winsys_ctx *ctx, bool full, bool *needed, bool *completed) {
    if (needed) *needed=ctx->ws->failed; if(completed) *completed=false;
    return ctx->ws->failed ? PIPE_UNKNOWN_CONTEXT_RESET : ctx->reset;
}
static bool cs_create(struct radeon_cmdbuf *rcs, struct radeon_winsys_ctx *ctx, enum amd_ip_type ip, void (*flush)(void *,unsigned,struct pipe_fence_handle **), void *user) {
    if (ip != AMD_IP_GFX) return false;
    struct pd_cs *cs = calloc(1,sizeof(*cs)); if(!cs) return false;
    rcs->current.buf=calloc(PD_RADEON_MAX_SUBMIT_DW,4);
    if(!rcs->current.buf) { free(cs); return false; }
    rcs->current.max_dw=PD_RADEON_MAX_SUBMIT_DW; rcs->priv=cs; cs->ctx=ctx; cs->flush=flush; cs->flush_ctx=user; return true;
}
static void cs_clear(struct radeon_cmdbuf *rcs) {
    struct pd_cs *cs=rcs->priv;
    for(unsigned i=0;i<cs->count;i++) radeon_bo_reference(&cs->ctx->ws->base,&cs->bos[i],NULL);
    cs->count=0; rcs->used_vram_kb=rcs->used_gart_kb=0; rcs->current.cdw=0;
}
static void cs_destroy(struct radeon_cmdbuf *rcs) {
    if(!rcs->priv) return;
    struct pd_cs *cs=rcs->priv; cs_clear(rcs);
    fence_reference(&cs->ctx->ws->base,&cs->next,NULL);
    free(rcs->current.buf); free(cs); memset(rcs,0,sizeof(*rcs));
}
static int cs_lookup(struct radeon_cmdbuf *rcs, struct pb_buffer_lean *buf) {
    struct pd_cs *cs=rcs->priv;
    for(unsigned i=0;i<cs->count;i++) if(cs->bos[i]==buf) return i;
    return -1;
}
static unsigned cs_add(struct radeon_cmdbuf *rcs, struct pb_buffer_lean *buf, unsigned usage, enum radeon_bo_domain domain) {
    struct pd_cs *cs=rcs->priv; int i=cs_lookup(rcs,buf);
    if(i<0) { if(cs->count==512) { cs->ctx->ws->failed=true; return 0; }
        i=cs->count++; cs->bos[i]=NULL; cs->usage[i]=0;
        radeon_bo_reference(&cs->ctx->ws->base,&cs->bos[i],buf); rcs->used_vram_kb += DIV_ROUND_UP(buf->size,1024);
    }
    cs->usage[i]|=usage; return i;
}
static bool cs_validate(struct radeon_cmdbuf *rcs) { return !((struct pd_cs *)rcs->priv)->ctx->ws->failed; }
static bool cs_space(struct radeon_cmdbuf *rcs, unsigned dw) { return dw <= rcs->current.max_dw-rcs->current.cdw; }
static unsigned cs_list(struct radeon_cmdbuf *rcs,struct radeon_bo_list_item *list) {
    struct pd_cs *cs=rcs->priv;
    if(list) for(unsigned i=0;i<cs->count;i++) list[i]=(struct radeon_bo_list_item){cs->bos[i]->size,buffer_address(cs->bos[i]),cs->usage[i]};
    return cs->count;
}
/* R600/R700 emit BO-relative PM4 even on the VM-capable winsys path.
 * Resolve their relocation NOPs against the BOs retained by this submission.
 * Unknown relocation forms fail before reaching the GPU. */
static bool cs_relocate(struct radeon_cmdbuf *rcs) {
    struct pd_cs *cs=rcs->priv;
    uint32_t *dw=rcs->current.buf;
    unsigned pending[2], shifts[2], count=0, next=0;
    for(unsigned i=0;i<rcs->current.cdw;) {
        unsigned type=dw[i]>>30;
        if(type==2) { i++; continue; }
        if(type!=3) return false;
        unsigned n=((dw[i]>>16)&0x3fff)+1, op=(dw[i]>>8)&0xff;
        if(n>rcs->current.cdw-i-1) return false;
        if(op==0x10 && n==1) {
            unsigned reloc=dw[i+1];
            if(next>=count || (reloc&3) || reloc/4>=cs->count) {
                fprintf(stderr,"r600: unsupported relocation at %u (pending %u/%u, BO %u/%u)\n",i,next,count,reloc/4,cs->count);
                for (unsigned j=i>24?i-24:0;j<i+2;j++) fprintf(stderr,"r600: PM4[%u] = %08x\n",j,dw[j]);
                return false;
            }
            uint64_t address=buffer_address(cs->bos[reloc/4]);
            dw[pending[next]]+=(uint32_t)(address>>shifts[next]);
            next++;
        } else if(op==0x69 || op==0x68) {
            unsigned reg=(op==0x69?0x28000:0x8000)+dw[i+1]*4;
            count=next=0;
            for(unsigned j=1;j<n;j++,reg+=4) {
                bool address = reg==0x2800c || reg==0x28014 ||
                    (reg>=0x28040 && reg<=0x2805c) ||
                    (reg>=0x280c0 && reg<=0x280fc) ||
                    reg==0x28840 || reg==0x28858 || reg==0x2886c ||
                    reg==0x28880 || reg==0x28894 ||
                    (reg>=0x28940 && reg<=0x289fc) ||
                    (reg>=0x28ad8 && reg<=0x28b08 && ((reg-0x28ad8)%16)==0) ||
                    (reg>=0x8c40 && reg<=0x8c68 && !(reg&7));
                if(address) {
                    if(count==2) return false;
                    pending[count]=i+1+j; shifts[count++]=8;
                }
            }
        } else if(op==0x6d) {
            count=next=0;
            if(n!=8) return false;
            unsigned kind=dw[i+8]>>30;
            if(kind==3) { pending[count]=i+2; shifts[count++]=0; }
            else if(kind==2) {
                pending[count]=i+4; shifts[count++]=8;
                pending[count]=i+5; shifts[count++]=8;
            } else return false;
        } else if(op==0x43 && n==4 && (dw[i+2]!=0xffffffff || dw[i+3]!=0)) {
            count=1; next=0; pending[0]=i+3; shifts[0]=8;
        } else if(op==0x72 && n==2 && dw[i+1]<4) {
            /* R7xx stream-output base update, address in 256-byte units. */
            count=1; next=0; pending[0]=i+2; shifts[0]=8;
        } else if(op==0x34 && n==5) {
            /* Stream-output filled-size store or memory-sourced offset. */
            count=next=0;
            if(dw[i+1]&1) { pending[count]=i+2; shifts[count++]=0; }
            if(((dw[i+1]>>1)&3)==2) { pending[count]=i+4; shifts[count++]=0; }
        } else if(op==0x2b && n==4) {
            /* R600/R700 DRAW_INDEX: index-buffer address is the first
             * payload word, unlike the newer DRAW_INDEX_2 packet below. */
            count=1; next=0; pending[0]=i+1; shifts[0]=0;
        } else if((op==0x46 && n==3) || (op==0x47 && n==5) ||
                  (op==0x3c && n==6 && (dw[i+1]&0x10))) {
            count=1; next=0; pending[0]=i+2; shifts[0]=0;
        } else { count=next=0; }
        i+=n+1;
    }
    return true;
}
static int cs_flush(struct radeon_cmdbuf *rcs,unsigned flags,struct pipe_fence_handle **fence) {
    struct pd_cs *cs=rcs->priv; struct pd_winsys *ws=cs->ctx->ws;
    struct pipe_fence_handle *f=cs->next ? cs->next : new_fence(); cs->next=NULL;
    int result=0;
    if(ws->failed) result=-EIO;
    else if(rcs->current.cdw && !(flags&RADEON_FLUSH_NOOP) && !cs_relocate(rcs)) { ws->failed=true; result=-EINVAL; }
    else if(rcs->current.cdw && !(flags&RADEON_FLUSH_NOOP)) {
        while(rcs->current.cdw&7) rcs->current.buf[rcs->current.cdw++]=0x80000000;
        if (getenv("PD_RADEON_DUMP_CS")) {
            fprintf(stderr,"PDIB %llu %u\n",(unsigned long long)ws->submissions,rcs->current.cdw);
            for (unsigned i=0;i<rcs->current.cdw;i+=8) {
                fprintf(stderr,"PDIB %04x",i);
                for (unsigned j=i;j<MIN2(i+8,rcs->current.cdw);j++) fprintf(stderr," %08x",rcs->current.buf[j]);
                fprintf(stderr,"\n");
            }
        }
        /* Drain write-combined shader, vertex and constant uploads on the
         * writing CPU before the kernel starts the command processor. */
#if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile ("sfence" ::: "memory");
#endif
        kern_return_t kr=IOConnectCallStructMethod(ws->conn,PD_RADEON_SUBMIT,rcs->current.buf,rcs->current.cdw*4,NULL,NULL);
        if(kr) { fprintf(stderr,"r600: submission failed 0x%x (%u dwords)\n",kr,rcs->current.cdw); ws->failed=true; result=-EIO; }
        else ws->submissions++;
    }
    if(f) atomic_store(&f->complete,result==0);
    if(fence) fence_reference(&ws->base,fence,f);
    fence_reference(&ws->base,&f,NULL); cs_clear(rcs); return result;
}
static struct pipe_fence_handle *cs_next(struct radeon_cmdbuf *rcs) {
    struct pd_cs *cs=rcs->priv;
    if(!cs->next) cs->next=new_fence();
    struct pipe_fence_handle *f=NULL; fence_reference(&cs->ctx->ws->base,&f,cs->next); return f;
}
static void cs_sync(struct radeon_cmdbuf *rcs) { }
static bool cs_feature(struct radeon_cmdbuf *rcs,enum radeon_feature_id feature,bool enable) { return false; }
static bool cs_secure(struct radeon_cmdbuf *rcs) { return false; }
static enum amd_ip_type cs_ip(struct radeon_cmdbuf *rcs) { return AMD_IP_GFX; }
static uint64_t query_value(struct radeon_winsys *rws,enum radeon_value_id value) {
    struct pd_winsys *ws=(void *)rws;
    switch(value) {
    case RADEON_REQUESTED_VRAM_MEMORY: case RADEON_VRAM_USAGE: case RADEON_MAPPED_VRAM: return ws->allocated;
    case RADEON_NUM_GFX_IBS: return ws->submissions;
    default: return 0;
    }
}
static bool read_registers(struct radeon_winsys *ws,unsigned offset,unsigned count,uint32_t *out) {
    memset(out,0,count*sizeof(*out)); return false;
}
int pd_surface_query(int conn,unsigned request,uint32_t *value) {
    struct pd_radeon_info info; size_t n=sizeof(info);
    if(IOConnectCallStructMethod(conn,PD_RADEON_INFO,NULL,0,&info,&n)) return -EIO;
    switch(request) {
    case RADEON_INFO_DEVICE_ID: *value=info.pci_id; return 0;
    case RADEON_INFO_TILING_CONFIG: *value=info.tiling_config; return 0;
    default: return -EINVAL;
    }
}
static void flush_frontbuffer(struct pipe_screen *screen, struct pipe_context *ctx,
    struct pipe_resource *resource, unsigned level, unsigned layer,
    void *drawable, unsigned nboxes, struct pipe_box *boxes) {
    struct pd_winsys *ws=(void *)((struct r600_common_screen *)screen)->ws;
    if (!ws->sw || ws->failed) return;
    unsigned width=u_minify(resource->width0,level), height=u_minify(resource->height0,level);
    unsigned stride=0;
    struct sw_displaytarget *dt=ws->sw->displaytarget_create(ws->sw,
        PIPE_BIND_DISPLAY_TARGET,resource->format,width,height,64,NULL,&stride);
    if (!dt) return;
    struct pipe_box box; u_box_2d(0,0,width,height,&box); box.z=layer;
    struct pipe_transfer *transfer=NULL;
    void *src=ctx->texture_map(ctx,resource,level,PIPE_MAP_READ,&box,&transfer);
    void *dst=ws->sw->displaytarget_map(ws->sw,dt,PIPE_MAP_WRITE);
    if (src && dst) {
        unsigned row=util_format_get_stride(resource->format,width);
        for (unsigned y=0;y<height;y++) memcpy((char *)dst+y*stride,(char *)src+y*transfer->stride,row);
    }
    if (src) ctx->texture_unmap(ctx,transfer);
    if (dst) ws->sw->displaytarget_unmap(ws->sw,dt);
    if (src && dst) ws->sw->displaytarget_display(ws->sw,dt,drawable,nboxes,boxes);
    ws->sw->displaytarget_destroy(ws->sw,dt);
}
struct pipe_screen *radeon_puredarwin_screen_create(struct sw_winsys *sw, const struct pipe_screen_config *config) {
    io_iterator_t iter; io_connect_t conn=0; io_service_t svc;
    if(IOServiceGetMatchingServices(kIOMasterPortDefault,IOServiceMatching("PDRadeonFramebuffer"),&iter)) return NULL;
    while((svc=IOIteratorNext(iter))) {
        kern_return_t kr=IOServiceOpen(svc,mach_task_self(),PD_RADEON_RENDER_CLIENT,&conn);
        IOObjectRelease(svc); if(!kr) break; conn=0;
    }
    IOObjectRelease(iter); if(!conn) return NULL;
    struct pd_winsys *ws=calloc(1,sizeof(*ws));
    if(!ws) { IOServiceClose(conn); return NULL; }
    ws->conn=conn; ws->sw=sw; size_t n=sizeof(ws->hw);
    if(IOConnectCallStructMethod(conn,PD_RADEON_INFO,NULL,0,&ws->hw,&n) || ws->hw.version!=1 || ws->hw.pci_id!=0x9490) { destroy(&ws->base); return NULL; }
    struct radeon_info *info=&ws->info;
    info->pci_id=ws->hw.pci_id; info->family=CHIP_RV730; info->gfx_level=R700;
    snprintf(info->marketing_name,sizeof(info->marketing_name),"AMD Radeon HD 4670");
    info->has_dedicated_vram=true; info->has_graphics=true;
    info->drm_major=2; info->drm_minor=50;
    info->vram_size_kb=info->vram_vis_size_kb=info->max_heap_size_kb=ws->hw.heap_size/1024;
    /* All domains reside in the CPU-visible local VRAM heap. */
    info->gart_size_kb=info->vram_size_kb; info->gart_page_size=4096; info->min_alloc_size=4096;
    info->max_alignment=8*1024*1024; info->max_render_backends=2; info->enabled_rb_mask=3; info->num_rb=2;
    info->num_tile_pipes=1<<((ws->hw.tiling_config&0xe)>>1);
    info->r600_num_banks=4<<((ws->hw.tiling_config&0x30)>>4);
    info->r600_pipe_interleave_bytes=256<<((ws->hw.tiling_config&0xc0)>>6);
    info->r600_gb_backend_map=ws->hw.backend_map; info->r600_gb_backend_map_valid=true;
    /* Local VRAM only; no GART-backed allocations. */
    /* R700 PM4 uses legacy relocations, resolved in cs_relocate. */
    info->r600_has_virtual_memory=false; info->r600_max_quad_pipes=2;
    info->num_cu=ws->hw.active_simds; info->max_se=info->num_se=info->max_sa_per_se=1;
    info->max_gpu_freq_mhz=ws->hw.max_clock_mhz; info->clock_crystal_freq=ws->hw.clock_crystal_khz;
    info->ip[AMD_IP_GFX].num_queues=1; info->ip[AMD_IP_GFX].ib_alignment=4096;
    info->gfx_ib_pad_with_type2=true; info->tcc_cache_line_size=64;
    struct radeon_winsys *r=&ws->base;
    r->unref=unref; r->destroy=destroy; r->get_fd=get_fd; r->query_info=query_info;
    r->buffer_create=buffer_create; r->buffer_destroy=buffer_destroy; r->buffer_map=buffer_map; r->buffer_unmap=buffer_unmap;
    r->buffer_wait=buffer_wait; r->buffer_get_virtual_address=buffer_address; r->buffer_get_reloc_offset=buffer_reloc;
    r->buffer_get_initial_domain=buffer_domain; r->buffer_get_flags=buffer_flags;
    r->buffer_is_user_ptr=bo_false; r->buffer_is_suballocated=bo_false; r->buffer_has_vm_always_valid=bo_false;
    r->buffer_get_metadata=get_metadata; r->buffer_set_metadata=set_metadata; r->buffer_from_handle=from_handle; r->buffer_get_handle=get_handle;
    r->ctx_create=ctx_create; r->ctx_destroy=ctx_destroy; r->ctx_query_reset_status=query_reset; r->ctx_set_sw_reset_status=reset_status;
    r->cs_create=cs_create; r->cs_destroy=cs_destroy; r->cs_add_buffer=cs_add; r->cs_lookup_buffer=cs_lookup;
    r->cs_validate=cs_validate; r->cs_check_space=cs_space; r->cs_get_buffer_list=cs_list; r->cs_flush=cs_flush;
    r->cs_get_next_fence=cs_next; r->cs_is_buffer_referenced=cs_referenced; r->cs_request_feature=cs_feature; r->cs_sync_flush=cs_sync;
    r->cs_get_ip_type=cs_ip; r->cs_is_secure=cs_secure; r->fence_reference=fence_reference; r->fence_wait=fence_wait;
    r->surface_init=pd_surface_init; r->query_value=query_value; r->read_registers=read_registers;
    ws->surf_man=radeon_surface_manager_new(conn);
    if(!ws->surf_man) { destroy(r); return NULL; }
    fprintf(stderr,"r600: PureDarwin RV730, %u SIMD, %llu MB local heap\n",ws->hw.active_simds,(unsigned long long)(ws->hw.heap_size>>20));
    struct pipe_screen *screen=r600_screen_create(r,config);
    if(!screen) { destroy(r); return NULL; }
    r->screen=screen; screen->flush_frontbuffer=flush_frontbuffer; return screen;
}
/* The Darwin software DRI frontend selects the native device above. */
struct radeon_winsys *radeon_drm_winsys_create(int fd, const struct pipe_screen_config *config, struct pipe_screen *(*create)(struct radeon_winsys *,const struct pipe_screen_config *)) { return NULL; }

/* The demo owns the IOFramebuffer server connection while using this bridge. */
bool pd_radeon_present_resource(struct pipe_context *ctx, struct pipe_resource *src, unsigned level, unsigned layer, bool flip) {
    if (!ctx || !src || ctx->screen->flush_frontbuffer!=flush_frontbuffer) return false;
    struct pd_winsys *ws=(void *)((struct r600_common_screen *)ctx->screen)->ws;
    if(ws->failed) return false;
    if(!ws->present_buffers[0]) {
        size_t size=sizeof(ws->scanout);
        if(IOConnectCallStructMethod(ws->conn,PD_RADEON_SCANOUT,NULL,0,&ws->scanout,&size)) return false;
        IOConnectCallScalarMethod(ws->conn,PD_RADEON_FREE,&ws->scanout.handle,1,NULL,NULL);
        ws->scanout.handle=0;
        struct pipe_resource templ={0};
        templ.target=PIPE_TEXTURE_2D;
        if (ws->scanout.reserved == 16)
            templ.format=PIPE_FORMAT_B5G6R5_UNORM;
        else if (ws->scanout.reserved == 32)
            templ.format=PIPE_FORMAT_B8G8R8X8_UNORM;
        else return false;
        templ.width0=ws->scanout.width; templ.height0=ws->scanout.height;
        templ.depth0=templ.array_size=1; templ.bind=PIPE_BIND_RENDER_TARGET|PIPE_BIND_SCANOUT|PIPE_BIND_LINEAR;
        for(unsigned i=0;i<2;i++) {
            ws->present_buffers[i]=ctx->screen->resource_create(ctx->screen,&templ);
            if(!ws->present_buffers[i]) {
                pipe_resource_reference(&ws->present_buffers[0],NULL);
                pipe_resource_reference(&ws->present_buffers[1],NULL);
                return false;
            }
            struct r600_texture *texture=(void *)ws->present_buffers[i];
            fprintf(stderr,"r600: present buffer %u mode=%u pitch=%u address=%08llx\n",i,
                texture->surface.u.legacy.level[0].mode,
                texture->surface.u.legacy.level[0].nblk_x*(ws->scanout.reserved/8),
                (unsigned long long)buffer_address(texture->resource.buf));
        }
    }
    struct pipe_resource *dst=ws->present_buffers[ws->present_index];
    struct pipe_blit_info blit={0};
    blit.src.resource=src; blit.src.level=level; blit.src.format=src->format;
    blit.src.box.width=u_minify(src->width0,level); blit.src.box.height=u_minify(src->height0,level);
    blit.src.box.z=layer; blit.src.box.depth=1;
    if(flip) { blit.src.box.y=blit.src.box.height; blit.src.box.height=-blit.src.box.height; }
    blit.dst.resource=dst; blit.dst.format=dst->format;
    blit.dst.box.width=ws->scanout.width; blit.dst.box.height=ws->scanout.height; blit.dst.box.depth=1;
    blit.mask=PIPE_MASK_RGBA; blit.filter=PIPE_TEX_FILTER_NEAREST;
    ctx->blit(ctx,&blit); ctx->flush(ctx,NULL,0);
    if(ws->failed) return false;
    struct r600_texture *texture=(void *)dst;
    struct pd_bo *bo=(void *)texture->resource.buf;
    uint64_t args[4]={bo->handle,ws->scanout.width,ws->scanout.height,
        texture->surface.u.legacy.level[0].nblk_x*(ws->scanout.reserved/8)};
    kern_return_t kr=IOConnectCallScalarMethod(ws->conn,PD_RADEON_FLIP,args,4,NULL,NULL);
    if(kr) { fprintf(stderr,"r600: page flip failed 0x%x\n",kr); return false; }
    ws->present_index^=1;
    return true;
}

/* GPU blit into a linear BGRA staging buffer, then a GPU copy into the caller's memory. */
bool pd_radeon_readback_resource(struct pipe_context *ctx, struct pipe_resource *src, unsigned level, unsigned layer, bool flip, void *dst, unsigned pitch) {
    if (!ctx || !src || !dst || ctx->screen->flush_frontbuffer!=flush_frontbuffer) return false;
    struct pd_winsys *ws=(void *)((struct r600_common_screen *)ctx->screen)->ws;
    if(ws->failed) return false;
    unsigned width=u_minify(src->width0,level), height=u_minify(src->height0,level);
    if(ws->readback_buffer && (ws->readback_buffer->width0!=width || ws->readback_buffer->height0!=height))
        pipe_resource_reference(&ws->readback_buffer,NULL);
    if(!ws->readback_buffer) {
        struct pipe_resource templ={0};
        templ.target=PIPE_TEXTURE_2D; templ.format=PIPE_FORMAT_B8G8R8A8_UNORM;
        templ.width0=width; templ.height0=height; templ.depth0=templ.array_size=1;
        templ.bind=PIPE_BIND_RENDER_TARGET|PIPE_BIND_LINEAR;
        ws->readback_buffer=ctx->screen->resource_create(ctx->screen,&templ);
        if(!ws->readback_buffer) return false;
    }
    struct pipe_blit_info blit={0};
    blit.src.resource=src; blit.src.level=level; blit.src.format=src->format;
    blit.src.box.width=width; blit.src.box.height=height; blit.src.box.z=layer; blit.src.box.depth=1;
    if(flip) { blit.src.box.y=height; blit.src.box.height=-(int)height; }
    blit.dst.resource=ws->readback_buffer; blit.dst.format=ws->readback_buffer->format;
    blit.dst.box.width=width; blit.dst.box.height=height; blit.dst.box.depth=1;
    blit.mask=PIPE_MASK_RGBA; blit.filter=PIPE_TEX_FILTER_NEAREST;
    ctx->blit(ctx,&blit); ctx->flush(ctx,NULL,0);
    if(ws->failed) return false;
    struct r600_texture *texture=(void *)ws->readback_buffer;
    struct pd_bo *bo=(void *)texture->resource.buf;
    uint64_t args[7]={bo->handle,0,texture->surface.u.legacy.level[0].nblk_x*4,width*4,height,pitch,(uint64_t)(uintptr_t)dst};
    return IOConnectCallScalarMethod(ws->conn,PD_RADEON_COPY_OUT,args,7,NULL,NULL)==0;
}
