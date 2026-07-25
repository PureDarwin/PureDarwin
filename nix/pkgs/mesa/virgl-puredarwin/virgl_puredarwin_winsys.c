#include <stdio.h>
#include <string.h>
#include "util/u_surface.h"
#include "util/u_memory.h"
#include "util/format/u_format.h"
#include "util/u_inlines.h"
#include "util/u_thread.h"
#include "frontend/sw_winsys.h"

#include "virgl/virgl_winsys.h"
#include "virgl_puredarwin_public.h"

#include "pd_virgl_shim.h"

struct virgl_pd_winsys {
   struct virgl_winsys base;
   struct sw_winsys *sws;
   pd_virgl_conn *conn;
   uint32_t ctx_id;
   mtx_t mutex;
};

struct virgl_hw_res {
   struct pipe_reference reference;
   uint32_t res_handle;
   int num_cs_references;

   void *ptr;            /* IOConnectMapMemory of the kext backing */
   uint64_t map_size;
   int size;

   uint32_t format;
   uint32_t stride;
   uint32_t width;
   uint32_t height;
   uint32_t bind;

   struct sw_displaytarget *dt;
   void *mapped;
   bool attached;        /* attached to the winsys context yet? */
};

struct virgl_pd_fence {
   struct pipe_reference reference;
};

struct virgl_pd_cmd_buf {
   struct virgl_cmd_buf base;
   uint32_t *buf;
   unsigned nres;
   unsigned cres;
   struct virgl_winsys *ws;
   struct virgl_hw_res **res_bo;
   char is_handle_added[512];
   unsigned reloc_indices_hashlist[512];
};

static inline struct virgl_pd_winsys *virgl_pd_winsys(struct virgl_winsys *iws)
{
   return (struct virgl_pd_winsys *)iws;
}

static inline struct virgl_pd_cmd_buf *virgl_pd_cmd_buf(struct virgl_cmd_buf *cbuf)
{
   return (struct virgl_pd_cmd_buf *)cbuf;
}

static inline boolean can_cache_resource_with_bind(uint32_t bind)
{
   return bind == VIRGL_BIND_CONSTANT_BUFFER ||
          bind == VIRGL_BIND_INDEX_BUFFER ||
          bind == VIRGL_BIND_VERTEX_BUFFER ||
          bind == VIRGL_BIND_CUSTOM ||
          bind == VIRGL_BIND_STAGING;
}

// kext transport (via the plain-C shim)

static int pd_transfer(struct virgl_pd_winsys *vws, bool to_host,
                       struct virgl_hw_res *res, const struct pipe_box *box,
                       uint32_t stride, uint32_t level, uint32_t offset)
{
   return pd_virgl_transfer(vws->conn, to_host ? 1 : 0, vws->ctx_id,
                            res->res_handle, box->x, box->y, box->z,
                            box->width, box->height, box->depth, level,
                            stride, offset, 0);
}

// winsys ops

static int virgl_pd_transfer_put(struct virgl_winsys *vws, struct virgl_hw_res *res,
                                 const struct pipe_box *box, uint32_t stride,
                                 uint32_t layer_stride, uint32_t buf_offset, uint32_t level)
{
   return pd_transfer(virgl_pd_winsys(vws), true, res, box, stride, level, buf_offset);
}

static int virgl_pd_transfer_get(struct virgl_winsys *vws, struct virgl_hw_res *res,
                                 const struct pipe_box *box, uint32_t stride,
                                 uint32_t layer_stride, uint32_t buf_offset, uint32_t level)
{
   return pd_transfer(virgl_pd_winsys(vws), false, res, box, stride, level, buf_offset);
}

static void virgl_hw_res_destroy(struct virgl_pd_winsys *vws, struct virgl_hw_res *res)
{
   if (res->dt)
      vws->sws->displaytarget_destroy(vws->sws, res->dt);
   if (res->res_handle)
      pd_virgl_destroy_resource(vws->conn, res->res_handle, res->ptr);
   FREE(res);
}

static boolean virgl_pd_resource_is_busy(struct virgl_winsys *vws, struct virgl_hw_res *res)
{
   return FALSE; /* submits are synchronous */
}

static void virgl_pd_resource_reference(struct virgl_winsys *vws,
                                        struct virgl_hw_res **dres,
                                        struct virgl_hw_res *sres)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   struct virgl_hw_res *old = *dres;

   if (pipe_reference(&(*dres)->reference, &sres->reference))
      virgl_hw_res_destroy(pdws, old);
   *dres = sres;
}

static struct virgl_hw_res *
virgl_pd_resource_create(struct virgl_winsys *vws, enum pipe_texture_target target,
                         const void *map_front_private, uint32_t format, uint32_t bind,
                         uint32_t width, uint32_t height, uint32_t depth,
                         uint32_t array_size, uint32_t last_level, uint32_t nr_samples,
                         uint32_t flags, uint32_t size)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   struct virgl_hw_res *res = CALLOC_STRUCT(virgl_hw_res);
   if (!res)
      return NULL;

   res->bind = bind;
   res->format = format;
   res->width = width;
   res->height = height;
   res->size = size;

   if (bind & (VIRGL_BIND_DISPLAY_TARGET | VIRGL_BIND_SCANOUT)) {
      res->dt = pdws->sws->displaytarget_create(pdws->sws, bind, format, width,
                                                height, 64, map_front_private,
                                                &res->stride);
   }

   struct pd_virgl_res_create rc;
   memset(&rc, 0, sizeof(rc));
   rc.target = target;
   rc.format = pipe_to_virgl_format(format);
   rc.bind = bind;
   rc.width = width;
   rc.height = height;
   rc.depth = depth;
   rc.array_size = array_size;
   rc.last_level = last_level;
   rc.nr_samples = nr_samples;
   rc.bytes_per_pixel = util_format_get_blocksize(format);
   rc.size = size;

   uint32_t res_id = 0;
   void *ptr = NULL;
   uint64_t map_size = 0;
   if (pd_virgl_create_resource(pdws->conn, &rc, &res_id, &ptr, &map_size) != 0) {
      if (res->dt)
         pdws->sws->displaytarget_destroy(pdws->sws, res->dt);
      FREE(res);
      return NULL;
   }
   res->res_handle = res_id;
   res->ptr = ptr;
   res->map_size = map_size;

   /* Attach the resource to our single context immediately. The virgl driver
    * issues transfer_put (buffer uploads) before the first submit_cmd, and the
    * host rejects a transfer whose resource isn't attached to the context
    * ("Illegal resource N"), so attach at creation rather than only at submit. */
   pd_virgl_attach_resource(pdws->conn, pdws->ctx_id, res_id);
   res->attached = true;

   pipe_reference_init(&res->reference, 1);
   p_atomic_set(&res->num_cs_references, 0);
   return res;
}

static void *virgl_pd_resource_map(struct virgl_winsys *vws, struct virgl_hw_res *res)
{
   res->mapped = res->ptr;
   return res->ptr;
}

static void virgl_pd_resource_wait(struct virgl_winsys *vws, struct virgl_hw_res *res)
{
   /* synchronous: nothing to wait for */
}

// command buffers

static boolean virgl_pd_lookup_res(struct virgl_pd_cmd_buf *cbuf, struct virgl_hw_res *res)
{
   unsigned hash = res->res_handle & (sizeof(cbuf->is_handle_added) - 1);
   int i;
   if (cbuf->is_handle_added[hash]) {
      i = cbuf->reloc_indices_hashlist[hash];
      if (cbuf->res_bo[i] == res)
         return true;
      for (i = 0; i < (int)cbuf->cres; i++) {
         if (cbuf->res_bo[i] == res) {
            cbuf->reloc_indices_hashlist[hash] = i;
            return true;
         }
      }
   }
   return false;
}

static void virgl_pd_release_all_res(struct virgl_pd_winsys *pdws, struct virgl_pd_cmd_buf *cbuf)
{
   for (int i = 0; i < (int)cbuf->cres; i++) {
      p_atomic_dec(&cbuf->res_bo[i]->num_cs_references);
      virgl_pd_resource_reference(&pdws->base, &cbuf->res_bo[i], NULL);
   }
   cbuf->cres = 0;
}

static void virgl_pd_add_res(struct virgl_pd_winsys *pdws, struct virgl_pd_cmd_buf *cbuf,
                             struct virgl_hw_res *res)
{
   unsigned hash = res->res_handle & (sizeof(cbuf->is_handle_added) - 1);
   if (cbuf->cres >= cbuf->nres) {
      unsigned new_nres = cbuf->nres + 256;
      struct virgl_hw_res **new_bo = REALLOC(cbuf->res_bo,
                                             cbuf->nres * sizeof(void *),
                                             new_nres * sizeof(void *));
      if (!new_bo) {
         fprintf(stderr, "virgl_pd: relocation realloc failed\n");
         return;
      }
      cbuf->res_bo = new_bo;
      cbuf->nres = new_nres;
   }
   cbuf->res_bo[cbuf->cres] = NULL;
   virgl_pd_resource_reference(&pdws->base, &cbuf->res_bo[cbuf->cres], res);
   cbuf->is_handle_added[hash] = TRUE;
   cbuf->reloc_indices_hashlist[hash] = cbuf->cres;
   p_atomic_inc(&res->num_cs_references);
   cbuf->cres++;
}

static struct virgl_cmd_buf *virgl_pd_cmd_buf_create(struct virgl_winsys *vws, uint32_t size)
{
   struct virgl_pd_cmd_buf *cbuf = CALLOC_STRUCT(virgl_pd_cmd_buf);
   if (!cbuf)
      return NULL;
   cbuf->nres = 512;
   cbuf->res_bo = CALLOC(cbuf->nres, sizeof(void *));
   if (!cbuf->res_bo) { FREE(cbuf); return NULL; }
   cbuf->buf = CALLOC(size, sizeof(uint32_t));
   if (!cbuf->buf) { FREE(cbuf->res_bo); FREE(cbuf); return NULL; }
   cbuf->ws = vws;
   cbuf->base.buf = cbuf->buf;
   return &cbuf->base;
}

static void virgl_pd_cmd_buf_destroy(struct virgl_cmd_buf *_cbuf)
{
   struct virgl_pd_cmd_buf *cbuf = virgl_pd_cmd_buf(_cbuf);
   virgl_pd_release_all_res(virgl_pd_winsys(cbuf->ws), cbuf);
   FREE(cbuf->res_bo);
   FREE(cbuf->buf);
   FREE(cbuf);
}

static int virgl_pd_submit_cmd(struct virgl_winsys *vws, struct virgl_cmd_buf *_cbuf,
                               struct pipe_fence_handle **fence)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   struct virgl_pd_cmd_buf *cbuf = virgl_pd_cmd_buf(_cbuf);

   if (cbuf->base.cdw == 0)
      return 0;

   /* Attach any newly-referenced resources to the context before the host
    * executes commands that reference them. */
   for (int i = 0; i < (int)cbuf->cres; i++) {
      struct virgl_hw_res *r = cbuf->res_bo[i];
      if (!r->attached) {
         pd_virgl_attach_resource(pdws->conn, pdws->ctx_id, r->res_handle);
         r->attached = true;
      }
   }

   int ret = pd_virgl_submit(pdws->conn, pdws->ctx_id, cbuf->buf,
                             cbuf->base.cdw, 0 /* synchronous */);

   if (fence && ret == 0) {
      struct virgl_pd_fence *f = CALLOC_STRUCT(virgl_pd_fence);
      if (f) {
         pipe_reference_init(&f->reference, 1);
         *fence = (struct pipe_fence_handle *)f;
      }
   }

   virgl_pd_release_all_res(pdws, cbuf);
   memset(cbuf->is_handle_added, 0, sizeof(cbuf->is_handle_added));
   cbuf->base.cdw = 0;
   return ret;
}

static void virgl_pd_emit_res(struct virgl_winsys *vws, struct virgl_cmd_buf *_cbuf,
                              struct virgl_hw_res *res, boolean write_buf)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   struct virgl_pd_cmd_buf *cbuf = virgl_pd_cmd_buf(_cbuf);
   boolean already = virgl_pd_lookup_res(cbuf, res);

   if (write_buf)
      cbuf->base.buf[cbuf->base.cdw++] = res->res_handle;
   if (!already)
      virgl_pd_add_res(pdws, cbuf, res);
}

static boolean virgl_pd_res_is_ref(struct virgl_winsys *vws, struct virgl_cmd_buf *_cbuf,
                                   struct virgl_hw_res *res)
{
   return p_atomic_read(&res->num_cs_references) ? TRUE : FALSE;
}

static int virgl_pd_get_caps(struct virgl_winsys *vws, struct virgl_drm_caps *caps)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   virgl_ws_fill_new_caps_defaults(caps);

   size_t len = sizeof(caps->caps);
   int ret = pd_virgl_get_caps(pdws->conn, &caps->caps, &len);

   /* Clear COPY_TRANSFER_BOTH_DIRECTIONS: with it set the virgl driver marks
    * non-buffer resources as "staging" and allocates a 1-byte guest backing
    * (alloc_size=1 in virgl_resource_create_front), expecting a host-side
    * staging copy path this winsys doesn't implement. Leaving it set makes
    * every readback/transfer overrun the tiny backing (crash + host "IOV data
    * size exceeds resource capacity"). vtest clears it for the same reason. */
   caps->caps.v2.capability_bits_v2 &= ~VIRGL_CAP_V2_COPY_TRANSFER_BOTH_DIRECTIONS;
   return ret;
}

// fences (synchronous)

static struct pipe_fence_handle *virgl_pd_cs_create_fence(struct virgl_winsys *vws, int fd)
{
   struct virgl_pd_fence *f = CALLOC_STRUCT(virgl_pd_fence);
   if (f)
      pipe_reference_init(&f->reference, 1);
   return (struct pipe_fence_handle *)f;
}

static bool virgl_pd_fence_wait(struct virgl_winsys *vws, struct pipe_fence_handle *fence,
                                uint64_t timeout)
{
   return true; /* work already completed synchronously at submit time */
}

static void virgl_pd_fence_reference(struct virgl_winsys *vws,
                                     struct pipe_fence_handle **dst,
                                     struct pipe_fence_handle *src)
{
   struct virgl_pd_fence *old = (struct virgl_pd_fence *)*dst;
   struct virgl_pd_fence *nsrc = (struct virgl_pd_fence *)src;
   if (old && pipe_reference(&old->reference, nsrc ? &nsrc->reference : NULL))
      FREE(old);
   *dst = src;
}

// present

static void virgl_pd_flush_frontbuffer(struct virgl_winsys *vws, struct virgl_hw_res *res,
                                       unsigned level, unsigned layer,
                                       void *winsys_drawable_handle, struct pipe_box *sub_box)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   struct pipe_box box;
   if (!res->dt)
      return;

   memset(&box, 0, sizeof(box));
   box.z = layer;
   box.width = res->width;
   box.height = res->height;
   box.depth = 1;

   uint32_t shm_stride = util_format_get_stride(res->format, res->width);
   pd_transfer(pdws, false, res, &box, shm_stride, level, 0);

   if (res->ptr) {
      void *dt_map = pdws->sws->displaytarget_map(pdws->sws, res->dt, 0);
      util_copy_rect(dt_map, res->format, res->stride, 0, 0,
                     res->width, res->height, res->ptr, shm_stride, 0, 0);
      pdws->sws->displaytarget_unmap(pdws->sws, res->dt);
   }

   pdws->sws->displaytarget_display(pdws->sws, res->dt, winsys_drawable_handle, sub_box);
}

static void virgl_pd_winsys_destroy(struct virgl_winsys *vws)
{
   struct virgl_pd_winsys *pdws = virgl_pd_winsys(vws);
   if (pdws->ctx_id)
      pd_virgl_destroy_context(pdws->conn, pdws->ctx_id);
   if (pdws->conn)
      pd_virgl_close(pdws->conn);
   mtx_destroy(&pdws->mutex);
   FREE(pdws);
}

struct virgl_winsys *virgl_puredarwin_winsys_wrap(struct sw_winsys *sws)
{
   struct virgl_pd_winsys *pdws = CALLOC_STRUCT(virgl_pd_winsys);
   if (!pdws)
      return NULL;

   pdws->conn = pd_virgl_open();
   if (!pdws->conn) {
      fprintf(stderr, "virgl_pd: cannot open IOVirtIOGPU 3D user client\n");
      FREE(pdws);
      return NULL;
   }
   pdws->sws = sws;
   pdws->ctx_id = pd_virgl_create_context(pdws->conn);
   (void) mtx_init(&pdws->mutex, mtx_plain);

   pdws->base.destroy = virgl_pd_winsys_destroy;
   pdws->base.transfer_put = virgl_pd_transfer_put;
   pdws->base.transfer_get = virgl_pd_transfer_get;
   pdws->base.resource_create = virgl_pd_resource_create;
   pdws->base.resource_reference = virgl_pd_resource_reference;
   pdws->base.resource_map = virgl_pd_resource_map;
   pdws->base.resource_wait = virgl_pd_resource_wait;
   pdws->base.resource_is_busy = virgl_pd_resource_is_busy;
   pdws->base.cmd_buf_create = virgl_pd_cmd_buf_create;
   pdws->base.cmd_buf_destroy = virgl_pd_cmd_buf_destroy;
   pdws->base.submit_cmd = virgl_pd_submit_cmd;
   pdws->base.emit_res = virgl_pd_emit_res;
   pdws->base.res_is_referenced = virgl_pd_res_is_ref;
   pdws->base.get_caps = virgl_pd_get_caps;
   pdws->base.cs_create_fence = virgl_pd_cs_create_fence;
   pdws->base.fence_wait = virgl_pd_fence_wait;
   pdws->base.fence_reference = virgl_pd_fence_reference;
   pdws->base.supports_fences = 0;
   pdws->base.supports_encoded_transfers = 0;
   pdws->base.flush_frontbuffer = virgl_pd_flush_frontbuffer;

   return &pdws->base;
}
