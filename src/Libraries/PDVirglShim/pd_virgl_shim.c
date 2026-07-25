#include "pd_virgl_shim.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#include "IOVirtIOGPU3DShared.h"

static inline io_connect_t conn_of(pd_virgl_conn *c)
{
   return (io_connect_t)(uintptr_t)c;
}

pd_virgl_conn *pd_virgl_open(void)
{
   mach_port_t master = MACH_PORT_NULL;
   if (IOMasterPort(MACH_PORT_NULL, &master) != KERN_SUCCESS)
      return 0;
   char *matching = IOServiceMatching("IOVirtIOGPU");
   if (!matching)
      return 0;
   io_service_t svc = IO_SERVICE_NULL;
   if (IOServiceGetMatchingService(master, matching, &svc) != KERN_SUCCESS ||
       svc == IO_SERVICE_NULL)
      return 0;
   io_connect_t conn = IO_CONNECT_NULL;
   kern_return_t kr = IOServiceOpen(svc, mach_task_self(),
                                    kIOVirtIOGPU3DConnectType, &conn);
   IOObjectRelease(svc);
   if (kr != KERN_SUCCESS)
      return 0;
   return (pd_virgl_conn *)(uintptr_t)conn;
}

void pd_virgl_close(pd_virgl_conn *c)
{
   if (c)
      IOServiceClose(conn_of(c));
}

uint32_t pd_virgl_create_context(pd_virgl_conn *c)
{
   uint64_t out = 0; uint32_t cnt = 1;
   IOConnectCallScalarMethod(conn_of(c), kPDVirgl_CreateContext, 0, 0, &out, &cnt);
   return (uint32_t)out;
}

void pd_virgl_destroy_context(pd_virgl_conn *c, uint32_t ctx_id)
{
   uint64_t in = ctx_id;
   IOConnectCallScalarMethod(conn_of(c), kPDVirgl_DestroyContext, &in, 1, 0, 0);
}

int pd_virgl_create_resource(pd_virgl_conn *c,
                             const struct pd_virgl_res_create *rc,
                             uint32_t *out_res_id,
                             void **out_ptr, uint64_t *out_size)
{
   struct PDVirglResourceCreate k;
   k.target = rc->target;
   k.format = rc->format;
   k.bind = rc->bind;
   k.width = rc->width;
   k.height = rc->height;
   k.depth = rc->depth;
   k.array_size = rc->array_size;
   k.last_level = rc->last_level;
   k.nr_samples = rc->nr_samples;
   k.flags = 0;
   k.bytes_per_pixel = rc->bytes_per_pixel;
   k.size = rc->size;

   uint64_t out[2] = { 0, 0 };
   uint32_t ocnt = 2;
   kern_return_t kr = IOConnectCallMethod(conn_of(c), kPDVirgl_CreateResource,
                                          0, 0, &k, sizeof(k),
                                          out, &ocnt, 0, 0);
   if (kr != KERN_SUCCESS)
      return -1;

   uint32_t res_id = (uint32_t)out[0];
   *out_res_id = res_id;
   *out_ptr = 0;
   *out_size = 0;

   if (rc->size > 0) {
      mach_vm_address_t addr = 0;
      mach_vm_size_t msize = 0;
      if (IOConnectMapMemory64(conn_of(c), res_id, mach_task_self(),
                               &addr, &msize, kIOMapAnywhere) == KERN_SUCCESS) {
         *out_ptr = (void *)(uintptr_t)addr;
         *out_size = msize;
      }
   }
   return 0;
}

void pd_virgl_destroy_resource(pd_virgl_conn *c, uint32_t res_id, void *mapped_ptr)
{
   if (mapped_ptr)
      IOConnectUnmapMemory64(conn_of(c), res_id, mach_task_self(),
                             (mach_vm_address_t)(uintptr_t)mapped_ptr);
   uint64_t in = res_id;
   IOConnectCallScalarMethod(conn_of(c), kPDVirgl_DestroyResource, &in, 1, 0, 0);
}

void pd_virgl_attach_resource(pd_virgl_conn *c, uint32_t ctx_id, uint32_t res_id)
{
   uint64_t in[2] = { ctx_id, res_id };
   IOConnectCallScalarMethod(conn_of(c), kPDVirgl_AttachResource, in, 2, 0, 0);
}

int pd_virgl_transfer(pd_virgl_conn *c, int to_host, uint32_t ctx_id,
                      uint32_t res_id, uint32_t x, uint32_t y, uint32_t z,
                      uint32_t w, uint32_t h, uint32_t d, uint32_t level,
                      uint32_t stride, uint64_t offset, uint64_t fence_id)
{
   struct PDVirglTransfer t;
   t.ctx_id = ctx_id;
   t.resource_id = res_id;
   t.x = x; t.y = y; t.z = z;
   t.w = w; t.h = h; t.d = d;
   t.level = level;
   t.stride = stride;
   t.offset = offset;
   t.fence_id = fence_id;
   uint32_t sel = to_host ? kPDVirgl_TransferToHost : kPDVirgl_TransferFromHost;
   return IOConnectCallStructMethod(conn_of(c), sel, &t, sizeof(t), 0, 0)
              == KERN_SUCCESS ? 0 : -1;
}

int pd_virgl_submit(pd_virgl_conn *c, uint32_t ctx_id,
                    const uint32_t *cmd, uint32_t cmd_dwords, uint64_t fence_id)
{
   uint64_t in[2] = { ctx_id, fence_id };
   return IOConnectCallMethod(conn_of(c), kPDVirgl_SubmitCmd, in, 2,
                              cmd, (size_t)cmd_dwords * sizeof(uint32_t),
                              0, 0, 0, 0) == KERN_SUCCESS ? 0 : -1;
}

int pd_virgl_get_caps(pd_virgl_conn *c, void *caps, size_t *inout_len)
{
   return IOConnectCallStructMethod(conn_of(c), kPDVirgl_GetCaps, 0, 0,
                                    caps, inout_len) == KERN_SUCCESS ? 0 : -1;
}

uint64_t pd_virgl_alloc_fence(pd_virgl_conn *c)
{
   uint64_t out = 0; uint32_t cnt = 1;
   IOConnectCallScalarMethod(conn_of(c), kPDVirgl_AllocFenceId, 0, 0, &out, &cnt);
   return out;
}
