// the Vulkan executor. Jobs keep the CPU path's contract: everything a job reads is in the bridge at the request's offsets, and what it writes goes back there.
// The loader is opened at run time, so a daemon built with this file still starts where no Vulkan exists
#define VK_NO_PROTOTYPES
#include "vk.h"

#include <math.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif

#include "../../air2spv/air2spv.h"

// set when the device is a forwarded one (Venus): its host compiles our shaders, with its own memory limits
static int gpu_remote;

// where upload and readback time goes, logged with the draw rect summary: content hashing, image
// staging copies, buffer copies, readback copies (us and bytes) and new staging buffers
static struct {
  uint64_t hash_us, hash_b, img_us, img_b, buf_us, buf_b, back_us, back_b, newbuf_us, newbuf_n;
  // a draw's attachment setup and resource binding, and new images made (create, memory, view)
  uint64_t att_us, bind_us, newimg_us, newimg_n;
  // the attachment pool: reuses, new images, evictions and what freeing an evicted image costs
  uint64_t pool_hit, pool_new, pool_evict, evict_us, evict_fb_us, fb_new, fb_new_us;
} prof;

// the daemon's loggers (airjitd.h pulls in mach headers this file does not need)
void logmsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void logv(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define VK_GLOBAL(X) X(vkCreateInstance) X(vkEnumerateInstanceVersion) X(vkEnumerateInstanceExtensionProperties)

#define VK_INSTANCE(X)                                                                                                 \
  X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceQueueFamilyProperties)    \
  X(vkGetPhysicalDeviceFeatures2) X(vkGetPhysicalDeviceProperties2) X(vkGetPhysicalDeviceFormatProperties)            \
  X(vkCreateDevice) X(vkGetDeviceProcAddr) X(vkEnumerateDeviceExtensionProperties)

#define VK_DEVICE(X)                                                                                                   \
  X(vkGetDeviceQueue) X(vkCreateCommandPool) X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer)                       \
  X(vkEndCommandBuffer) X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkFreeCommandBuffers) X(vkCmdPipelineBarrier)        \
  X(vkCreateFence) X(vkWaitForFences) X(vkResetFences) X(vkCreateSemaphore) X(vkCreatePipelineCache)                   \
  X(vkCmdCopyBuffer) X(vkCmdClearDepthStencilImage) X(vkCmdClearColorImage)           \
  X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory)            \
  X(vkCreateBufferView) X(vkDestroyBufferView)                                                                         \
  X(vkBindBufferMemory) X(vkMapMemory) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements)              \
  X(vkBindImageMemory) X(vkCreateImageView) X(vkDestroyImageView) X(vkCmdCopyBufferToImage)                            \
  X(vkCmdCopyImageToBuffer) X(vkCreateSampler) X(vkDestroySampler) X(vkCreateShaderModule)                             \
  X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool)         \
  X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreatePipelineLayout) X(vkCreateComputePipelines)          \
  X(vkCreateGraphicsPipelines) X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdPushConstants)                   \
  X(vkCmdDispatch) X(vkCreateRenderPass) X(vkCreateFramebuffer) X(vkDestroyFramebuffer) X(vkCmdBeginRenderPass)        \
  X(vkCmdEndRenderPass) X(vkCmdBindIndexBuffer) X(vkCmdDraw) X(vkCmdDrawIndexed) X(vkCmdBindVertexBuffers)

#define VK_DECL(f) static PFN_##f f;
VK_GLOBAL(VK_DECL)
VK_INSTANCE(VK_DECL)
VK_DEVICE(VK_DECL)
static PFN_vkGetInstanceProcAddr gipa;

struct pd_vk_stats pd_vk_last;

static int state;  // 0 not tried, 1 ready, -1 unavailable
static char reason[256];
static jmp_buf job_fail;

static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static VkCommandPool pool;
static VkPhysicalDeviceMemoryProperties memprops;

// resident mode: jobs record into one command buffer per batch (pd_vk_flush ends it), resources stay on the
// device between jobs (see the resident table below)
static int resident_mode;
static VkCommandBuffer batch;
static VkFence batch_fence;
// separate from the resident batch: synchronous jobs/readbacks wait for their
// own submission without QueueWaitIdle creating an additional queue operation.
static VkFence submit_fence;
static int batch_inflight;
static VkPipelineCache pcache;
// a render pass is open in the batch: a job that declines must close it
static int in_pass;
// a readback runs on its own command buffer and waits, outside the batch
static int direct;
// per-job mode records a job's uploads, work and readback copies into one command buffer, submitted once
// when the CPU needs results (AIRJITD_VK_JOBBATCH=0: a submission and wait per step)
static int job_batch = -1;
static VkCommandBuffer jcb;
// work recorded into the job's buffer that later steps have not waited for yet. On PowerVR each barrier between different kinds of work is a firmware round trip,
// so a job takes one before its pass or dispatch and one before its readback, a single one for all uploads
static int jcb_dirty;

// what the device allows, or the profile pretends it allows
static struct {
  uint32_t max_invocations, max_push, quad_compute, dyn_image_index;
  // lines by the diamond exit rule (VK_KHR_line_rasterization bresenhamLines), as Metal draws them
  uint32_t bresenham;
  // cube arrays, depth clamp and its bias clamp, base vertex and instance in shaders
  uint32_t cube_array, depth_clamp, bias_clamp, draw_params;
  // discard as a demote (the 1.3 feature), without it shaders discard with OpKill
  uint32_t demote;
  // pipeline creation flags of the 1.3 feature: Venus creates a pipeline synchronously when one is set, so a
  // pipeline the host rejects comes back as an error and no null handle is bound later
  uint32_t cache_control;
  // samplerAnisotropy: maxSamplerAnisotropy, else 0
  float anisotropy;
} lim;

// samplers the kext did not know: compute jobs (CoreImage) address texels, draws normalized coordinates
static int unknown_pixel;

// a presenter's extensions, and the image it lends for an attachment (pd_vk_external_attachment)
static const char *const *want_inst, *const *want_dev;
static unsigned nwant_inst, nwant_dev;
static uint32_t qfamily;
static struct {
  uint32_t off;
  VkImage image;
  VkImageView view;
  // the CPU wrote its bytes after the last draw into it (a copy, a resolve): the presenter uploads them
  int written;
} ext_att;

const char *pd_vk_reason(void) {
  return reason;
}

static uint64_t now_us(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

__attribute__((format(printf, 1, 2))) _Noreturn static void decline(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(reason, sizeof(reason), fmt, ap);
  va_end(ap);
  longjmp(job_fail, 1);
}

#define CK(x)                                                                                                          \
  do {                                                                                                                 \
    VkResult r_ = (x);                                                                                                 \
    if (r_ != VK_SUCCESS)                                                                                              \
      decline("%s: VkResult %d", #x, (int)r_);                                                                         \
  } while (0)

int pd_vk_enabled(void) {
  static int on = -1;

  if (on < 0) {
    const char *e = getenv("AIRJITD_VULKAN");
    on = e && *e && *e != '0';
  }
  return on;
}

// the libdrm shim of PDPowerVR maps the buffers made while this is on cacheable, cleaned by the kext around submits
// and waits, since the driver's only memory type is write-combined, which the CPU reads back at tens of MB/s
static void (*drm_cpu_cached)(int);

// the job's cacheable host buffers: cleaned before a submission, cleaned and invalidated after its wait, since
// the GPU does not snoop the CPU caches (pdpvrflush=0 leaves this to the task)
#define HOSTMAPS 256
static struct {
  uint8_t *p;
  size_t n;
} hostmaps[HOSTMAPS];
static unsigned nhostmaps;

// venus on an arm64 kvm guest: the host gpu writes host-visible memory around the guest's caches (no forced
// write-back stage 2 on the pi 5), so the job's buffers are cleaned and invalidated like PowerVR's cacheable ones
static int remote_incoherent(void) {
#if defined(__aarch64__)
  return gpu_remote;
#else
  return 0;
#endif
}

static void host_track(void *p, size_t n) {
  if ((drm_cpu_cached || remote_incoherent()) && nhostmaps < HOSTMAPS)
    hostmaps[nhostmaps].p = p, hostmaps[nhostmaps++].n = n;
}

static void host_maintain(int invalidate) {
#if defined(__aarch64__)
  for (unsigned i = 0; i < nhostmaps; i++) {
    uintptr_t a = (uintptr_t)hostmaps[i].p & ~(uintptr_t)63, e = (uintptr_t)hostmaps[i].p + hostmaps[i].n;

    for (; a < e; a += 64) {
      if (invalidate)
        __asm__ volatile("dc civac, %0" ::"r"(a) : "memory");
      else
        __asm__ volatile("dc cvac, %0" ::"r"(a) : "memory");
    }
  }
  __asm__ volatile("dsb sy" ::: "memory");
#else
  (void)invalidate;
#endif
}

// venus host memory may be mapped as arm64 device memory, where memcpy's unaligned accesses fault (SIGBUS).
// copies to and from it go in aligned words on the mapped side, everywhere else they stay memcpy
static int map_device(void) {
#if defined(__aarch64__)
  return gpu_remote;
#else
  return 0;
#endif
}

static void to_map(void *dst, const void *src, size_t n) {
  volatile uint8_t *d = dst;
  const uint8_t *s = src;

  if (!map_device()) {
    memcpy(dst, src, n);
    return;
  }
  for (; n && ((uintptr_t)d & 7); n--) {
    *d++ = *s++;
  }
  for (; n >= 8; n -= 8, d += 8, s += 8) {
    uint64_t v;

    memcpy(&v, s, 8);
    *(volatile uint64_t *)d = v;
  }
  for (; n; n--) {
    *d++ = *s++;
  }
}

static void from_map(void *dst, const void *src, size_t n) {
  const volatile uint8_t *s = src;
  uint8_t *d = dst;

  if (!map_device()) {
    memcpy(dst, src, n);
    return;
  }
  for (; n && ((uintptr_t)s & 7); n--) {
    *d++ = *s++;
  }
  for (; n >= 8; n -= 8, d += 8, s += 8) {
    uint64_t v = *(const volatile uint64_t *)s;

    memcpy(d, &v, 8);
  }
  for (; n; n--) {
    *d++ = *s++;
  }
}

static void zero_map(void *dst, size_t n) {
  static const uint8_t zeros[4096];

  for (size_t k = 0; k < n; k += sizeof(zeros)) {
    to_map((uint8_t *)dst + k, zeros, n - k < sizeof(zeros) ? n - k : sizeof(zeros));
  }
}

static void find_drm_hint(void) {
#ifdef __APPLE__
  const char *e = getenv("AIRJITD_VK_CACHED");

  if (e && *e == '0')
    return;
  for (uint32_t i = 0; i < _dyld_image_count(); i++) {
    const char *n = _dyld_get_image_name(i);
    void *h;

    if (n && strstr(n, "libdrm_pdpvr") && (h = dlopen(n, RTLD_NOLOAD | RTLD_LOCAL))) {
      drm_cpu_cached = (void (*)(int))dlsym(h, "pd_drm_set_cpu_cached");
      break;
    }
  }
  if (drm_cpu_cached)
    logmsg("AIRJITD: vulkan host buffers mapped cacheable");
#endif
}

static int vk_init(void) {
  // the system loader by path first: a bare name finds a libvulkan an app brought along (MoltenVK apps)
  static const char *const libs[] = { "libvulkan.so.1", "libvulkan.so", "/usr/lib/libvulkan.1.dylib", "libvulkan.1.dylib", "libMoltenVK.dylib" };
  VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
  VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
  VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
  VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
  VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
  VkPhysicalDeviceVulkan11Features f11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
  VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
  VkPhysicalDeviceSubgroupProperties sg = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
  VkPhysicalDeviceLineRasterizationFeaturesKHR lr = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_KHR };
  const char *dev_ext[17], *inst_ext[16];
  unsigned ndev_ext = 0;
  VkPhysicalDeviceProperties2 p2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
  VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  VkQueueFamilyProperties qf[16];
  uint32_t n = 1, nq = 16, qfam;
  const char *prof = getenv("AIRJITD_VK_PROFILE");
  void *lib = 0;
  float prio = 1;

  // AIRJITD_VK_ICD names a driver to use without a loader (the stock macOS userland has none)
  if (getenv("AIRJITD_VK_ICD")) {
    typedef VkResult (*negotiate_fn)(uint32_t *);
    negotiate_fn neg;
    uint32_t ver = 5;

    if ((lib = dlopen(getenv("AIRJITD_VK_ICD"), RTLD_NOW | RTLD_LOCAL))) {
      if ((neg = (negotiate_fn)dlsym(lib, "vk_icdNegotiateLoaderICDInterfaceVersion")))
        neg(&ver);
      gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    }
  } else {
    for (unsigned i = 0; i < sizeof(libs) / sizeof(libs[0]) && !lib; i++) {
      lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
    }
    if (lib)
      gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
  }
  if (!gipa) {
    snprintf(reason, sizeof(reason), "no Vulkan loader or driver: %s", dlerror());
    return -1;
  }

#define VK_LOAD_GLOBAL(f) f = (PFN_##f)gipa(0, #f);
  VK_GLOBAL(VK_LOAD_GLOBAL)
  app.apiVersion = VK_API_VERSION_1_2;
  ici.pApplicationInfo = &app;
  // a window system's extension the loader lacks is dropped and that window system just cannot present
  {
    VkExtensionProperties have[128];
    uint32_t nhave = 128, n = 0;

    if (!vkEnumerateInstanceExtensionProperties || vkEnumerateInstanceExtensionProperties(0, &nhave, have) < 0)
      nhave = 0;
    for (unsigned i = 0; i < nwant_inst && n < 16; i++) {
      uint32_t k;

      for (k = 0; k < nhave && strcmp(have[k].extensionName, want_inst[i]); k++) {
      }
      if (nhave && k == nhave)
        logmsg("AIRJITD: instance extension %s unavailable, skipped", want_inst[i]);
      else
        inst_ext[n++] = want_inst[i];
    }
    ici.enabledExtensionCount = n, ici.ppEnabledExtensionNames = inst_ext;
  }
  if (!vkCreateInstance || vkCreateInstance(&ici, 0, &inst) != VK_SUCCESS) {
    snprintf(reason, sizeof(reason), "no Vulkan instance");
    return -1;
  }
#define VK_LOAD_INSTANCE(f) f = (PFN_##f)gipa(inst, #f);
  VK_INSTANCE(VK_LOAD_INSTANCE)
  if (vkEnumeratePhysicalDevices(inst, &n, &phys) < 0 || !n) {
    snprintf(reason, sizeof(reason), "no Vulkan device");
    return -1;
  }

  vkGetPhysicalDeviceMemoryProperties(phys, &memprops);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
  for (qfam = 0; qfam < nq && !(qf[qfam].queueFlags & VK_QUEUE_GRAPHICS_BIT); qfam++) {
  }
  if (qfam == nq) {
    snprintf(reason, sizeof(reason), "no graphics queue");
    return -1;
  }

  {
    VkExtensionProperties ext[512];
    uint32_t ne = 512;
    if (vkEnumerateDeviceExtensionProperties(phys, 0, &ne, ext) >= 0) {
      for (uint32_t i = 0; i < ne; i++) {
        if (!strcmp(ext[i].extensionName, VK_KHR_LINE_RASTERIZATION_EXTENSION_NAME))
          lim.bresenham = 1;
      }
    }
  }

  p2.pNext = &sg;
  vkGetPhysicalDeviceProperties2(phys, &p2);
  gpu_remote = strstr(p2.properties.deviceName, "Venus") != 0 || strstr(p2.properties.deviceName, "Virtio") != 0;
  f2.pNext = &f11, f11.pNext = &f12, f12.pNext = &f13;
  f13.pNext = lim.bresenham ? &lr : 0;
  vkGetPhysicalDeviceFeatures2(phys, &f2);
  lim.bresenham = lim.bresenham && lr.bresenhamLines;
  lim.cube_array = f2.features.imageCubeArray, lim.depth_clamp = f2.features.depthClamp;
  lim.anisotropy = f2.features.samplerAnisotropy ? p2.properties.limits.maxSamplerAnisotropy : 0;
  lim.bias_clamp = f2.features.depthBiasClamp, lim.draw_params = f11.shaderDrawParameters;
  lim.max_invocations = p2.properties.limits.maxComputeWorkGroupInvocations;
  lim.max_push = p2.properties.limits.maxPushConstantsSize;
  lim.quad_compute = (sg.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT) &&
                     (sg.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) && sg.subgroupSize >= 4;
  lim.dyn_image_index = f2.features.shaderStorageImageArrayDynamicIndexing;
  lim.demote = p2.properties.apiVersion >= VK_API_VERSION_1_3 && f13.shaderDemoteToHelperInvocation;
  lim.cache_control = p2.properties.apiVersion >= VK_API_VERSION_1_3 && f13.pipelineCreationCacheControl;
  if (prof && !strcmp(prof, "pvr")) {
    // mesa 26 pvr: 128 invocations, subgroup size 1, no dynamic indexing of image arrays
    lim.max_invocations = lim.max_invocations < 128 ? lim.max_invocations : 128;
    lim.max_push = 128, lim.quad_compute = 0, lim.dyn_image_index = 0;
  }
  if (prof && !strcmp(prof, "v3dv")) {
    // the Pi 5's v3dv through Venus: 256 invocations, no quads, no dynamic image array indexing, Vulkan 1.2 (no demote)
    lim.max_invocations = lim.max_invocations < 256 ? lim.max_invocations : 256;
    lim.max_push = 128, lim.quad_compute = 0, lim.dyn_image_index = 0, lim.demote = 0;
  }

  // only what the translated shaders may declare
  {
    VkPhysicalDeviceFeatures want = { 0 };
    int demote = f13.shaderDemoteToHelperInvocation;

    want.shaderStorageImageWriteWithoutFormat = f2.features.shaderStorageImageWriteWithoutFormat;
    want.shaderStorageImageReadWithoutFormat = f2.features.shaderStorageImageReadWithoutFormat;
    want.independentBlend = f2.features.independentBlend;
    want.sampleRateShading = f2.features.sampleRateShading;
    want.shaderStorageImageArrayDynamicIndexing = lim.dyn_image_index;
    want.imageCubeArray = lim.cube_array, want.depthClamp = lim.depth_clamp, want.depthBiasClamp = lim.bias_clamp;
    want.textureCompressionBC = f2.features.textureCompressionBC;
    want.samplerAnisotropy = f2.features.samplerAnisotropy;
    f2.features = want;
    f12 = (VkPhysicalDeviceVulkan12Features){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    f13 = (VkPhysicalDeviceVulkan13Features){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    f13.shaderDemoteToHelperInvocation = (VkBool32)demote;
    f13.pipelineCreationCacheControl = lim.cache_control;
    lr = (VkPhysicalDeviceLineRasterizationFeaturesKHR){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_KHR };
    lr.bresenhamLines = lim.bresenham;
    f13.pNext = lim.bresenham ? &lr : 0;
    f11 = (VkPhysicalDeviceVulkan11Features){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    f11.shaderDrawParameters = lim.draw_params;
    f11.pNext = p2.properties.apiVersion >= VK_API_VERSION_1_3 ? (void *)&f13 : lim.bresenham ? (void *)&lr : 0;
    f2.pNext = &f11;
  }

  qci.queueFamilyIndex = qfam, qci.queueCount = 1, qci.pQueuePriorities = &prio;
  dci.pNext = &f2, dci.queueCreateInfoCount = 1, dci.pQueueCreateInfos = &qci;
  if (lim.bresenham)
    dev_ext[ndev_ext++] = VK_KHR_LINE_RASTERIZATION_EXTENSION_NAME;
  for (unsigned i = 0; i < nwant_dev && ndev_ext < 17; i++) {
    dev_ext[ndev_ext++] = want_dev[i];
  }
  dci.enabledExtensionCount = ndev_ext, dci.ppEnabledExtensionNames = dev_ext;
  if (vkCreateDevice(phys, &dci, 0, &dev) != VK_SUCCESS) {
    snprintf(reason, sizeof(reason), "device creation failed");
    return -1;
  }
#define VK_LOAD_DEVICE(f) f = (PFN_##f)vkGetDeviceProcAddr(dev, #f);
  VK_DEVICE(VK_LOAD_DEVICE)
  vkGetDeviceQueue(dev, qfam, 0, &queue);
  qfamily = qfam;
  pci.queueFamilyIndex = qfam, pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(dev, &pci, 0, &pool) != VK_SUCCESS)
    return -1;
  {
    VkPipelineCacheCreateInfo pc = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    if (vkCreatePipelineCache(dev, &pc, 0, &pcache) != VK_SUCCESS)
      pcache = VK_NULL_HANDLE;
    if (vkCreateFence(dev, &fi, 0, &batch_fence) != VK_SUCCESS ||
        vkCreateFence(dev, &fi, 0, &submit_fence) != VK_SUCCESS)
      return -1;
  }

  logmsg("AIRJITD: vulkan executor on %s (api %u.%u), %u invocations, quads %s, image array index %s%s",
         p2.properties.deviceName, VK_API_VERSION_MAJOR(p2.properties.apiVersion),
         VK_API_VERSION_MINOR(p2.properties.apiVersion), lim.max_invocations, lim.quad_compute ? "yes" : "no",
         lim.dyn_image_index ? "yes" : "no", prof ? " (profile)" : "");
  for (uint32_t i = 0; i < memprops.memoryTypeCount; i++) {
    logmsg("AIRJITD: vulkan memory type %u heap %u flags 0x%x", i, memprops.memoryTypes[i].heapIndex,
           memprops.memoryTypes[i].propertyFlags);
  }
  find_drm_hint();
  return 0;
}

static int ready(void) {
  if (!state)
    state = vk_init() ? -1 : 1;
  if (state < 0 && !reason[0])
    snprintf(reason, sizeof(reason), "Vulkan unavailable");
  return state > 0;
}

void pd_vk_want_extensions(const char *const *instance, unsigned ninstance, const char *const *device, unsigned ndevice) {
  want_inst = instance, nwant_inst = ninstance;
  want_dev = device, nwant_dev = ndevice;
}

int pd_vk_start(struct pd_vk_handles *h) {
  if (!ready())
    return -1;
  h->gipa = (void *)gipa, h->instance = inst, h->physical = phys, h->device = dev, h->queue = queue;
  h->queue_family = qfamily;
  return 0;
}

void pd_vk_external_attachment(uint32_t off, uint64_t image, uint64_t view) {
  ext_att.off = off, ext_att.image = (VkImage)image, ext_att.view = (VkImageView)view, ext_att.written = 0;
}

int pd_vk_external_written(void) {
  int w = ext_att.written;

  ext_att.written = 0;
  return w;
}

// what a job made, freed when it ends, in resident mode a batch's, freed once its fence signalled (two: the batch
// being recorded and the one in flight)
#define ARENA_BUFS 512
#define ARENA_VIEWS 256
#define ARENA_POOLS 128
struct job_arena {
  VkBuffer buf[ARENA_BUFS];
  VkImage img[64];
  VkImageView view[ARENA_VIEWS];
  VkBufferView bview[ARENA_VIEWS];
  VkDeviceMemory mem[ARENA_BUFS + 64];
  VkFramebuffer fbs[ARENA_POOLS];
  VkDescriptorPool dpools[ARENA_POOLS];
  VkFramebuffer fb;
  VkDescriptorPool dpool;
  // a batch's command buffer
  VkCommandBuffer cb;
  unsigned nbuf, nimg, nview, nmem, nfb, npool, nbview;
};
static struct job_arena arenas[2];
static int cur_arena;
#define arena arenas[cur_arena]

static void arena_free(void) {
  unsigned i;

  if (arena.cb)
    vkFreeCommandBuffers(dev, pool, 1, &arena.cb);
  if (arena.fb)
    vkDestroyFramebuffer(dev, arena.fb, 0);
  if (arena.dpool)
    vkDestroyDescriptorPool(dev, arena.dpool, 0);
  for (i = 0; i < arena.nfb; i++) {
    vkDestroyFramebuffer(dev, arena.fbs[i], 0);
  }
  for (i = 0; i < arena.npool; i++) {
    vkDestroyDescriptorPool(dev, arena.dpools[i], 0);
  }
  for (i = 0; i < arena.nview; i++) {
    vkDestroyImageView(dev, arena.view[i], 0);
  }
  for (i = 0; i < arena.nbview; i++) {
    vkDestroyBufferView(dev, arena.bview[i], 0);
  }
  for (i = 0; i < arena.nimg; i++) {
    vkDestroyImage(dev, arena.img[i], 0);
  }
  for (i = 0; i < arena.nbuf; i++) {
    vkDestroyBuffer(dev, arena.buf[i], 0);
  }
  for (i = 0; i < arena.nmem; i++) {
    vkFreeMemory(dev, arena.mem[i], 0);
  }
  memset(&arena, 0, sizeof(arena));
}

// pixel formats as the kext numbers them (airjitd/texture.h)
static const struct {
  VkFormat vk;
  unsigned bpp;
  char kind;
} fmts[] = {
  { VK_FORMAT_B8G8R8A8_UNORM, 4, 'n' },       { VK_FORMAT_R16G16B16A16_SFLOAT, 8, 'h' },
  { VK_FORMAT_R8_UNORM, 1, 'n' },             { VK_FORMAT_R8_UNORM, 1, 'n' },
  { VK_FORMAT_R8G8B8A8_UNORM, 4, 'n' },       { VK_FORMAT_R8G8_UNORM, 2, 'n' },
  { VK_FORMAT_R16G16_UINT, 4, 'u' },          { VK_FORMAT_R32_UINT, 4, 'u' },
  { VK_FORMAT_R32G32_UINT, 8, 'u' },          { VK_FORMAT_R16G16B16A16_UINT, 8, 'u' },
  { VK_FORMAT_R16_UINT, 2, 'u' },             { VK_FORMAT_R8G8B8A8_UINT, 4, 'u' },
  { VK_FORMAT_R16_SFLOAT, 2, 'h' },           { VK_FORMAT_R16G16_SFLOAT, 4, 'h' },
  { VK_FORMAT_R32_SFLOAT, 4, 'f' },           { VK_FORMAT_R32G32_SFLOAT, 8, 'f' },
  { VK_FORMAT_R32G32B32A32_SFLOAT, 16, 'f' }, { VK_FORMAT_R8G8B8A8_SRGB, 4, 'n' },
  { VK_FORMAT_B8G8R8A8_SRGB, 4, 'n' },        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4, 'n' },
  { VK_FORMAT_A2R10G10B10_UNORM_PACK32, 4, 'n' }, { VK_FORMAT_R16_UNORM, 2, 'n' },
  { VK_FORMAT_R16G16_UNORM, 4, 'n' },         { VK_FORMAT_R16G16B16A16_UNORM, 8, 'n' },
  // depth and stencil (airjitd/texture.h): one interleaved plane in the bridge, two aspects here
  { VK_FORMAT_D32_SFLOAT, 4, 'd' },           { VK_FORMAT_D16_UNORM, 2, 'd' },
  { VK_FORMAT_D32_SFLOAT_S8_UINT, 8, 'd' },   { VK_FORMAT_D24_UNORM_S8_UINT, 4, 'd' },
  { VK_FORMAT_S8_UINT, 1, 'd' },
  // block compressed (kind 'c'): bpp is bytes per 4x4 block, rows are block rows
  { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, 'c' },  { VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8, 'c' },
  { VK_FORMAT_BC2_UNORM_BLOCK, 16, 'c' },      { VK_FORMAT_BC2_SRGB_BLOCK, 16, 'c' },
  { VK_FORMAT_BC3_UNORM_BLOCK, 16, 'c' },      { VK_FORMAT_BC3_SRGB_BLOCK, 16, 'c' },
  { VK_FORMAT_BC4_UNORM_BLOCK, 8, 'c' },       { VK_FORMAT_BC4_SNORM_BLOCK, 8, 'c' },
  { VK_FORMAT_BC5_UNORM_BLOCK, 16, 'c' },      { VK_FORMAT_BC5_SNORM_BLOCK, 16, 'c' },
  { VK_FORMAT_BC6H_SFLOAT_BLOCK, 16, 'c' },    { VK_FORMAT_BC6H_UFLOAT_BLOCK, 16, 'c' },
  { VK_FORMAT_BC7_UNORM_BLOCK, 16, 'c' },      { VK_FORMAT_BC7_SRGB_BLOCK, 16, 'c' },
  // integer formats texel buffers and textures take (texture.h 43-54)
  { VK_FORMAT_R8_UINT, 1, 'u' },              { VK_FORMAT_R8G8_UINT, 2, 'u' },
  { VK_FORMAT_R32G32B32A32_UINT, 16, 'u' },   { VK_FORMAT_R8_SINT, 1, 'i' },
  { VK_FORMAT_R8G8_SINT, 2, 'i' },            { VK_FORMAT_R8G8B8A8_SINT, 4, 'i' },
  { VK_FORMAT_R16_SINT, 2, 'i' },             { VK_FORMAT_R16G16_SINT, 4, 'i' },
  { VK_FORMAT_R16G16B16A16_SINT, 8, 'i' },    { VK_FORMAT_R32_SINT, 4, 'i' },
  { VK_FORMAT_R32G32_SINT, 8, 'i' },          { VK_FORMAT_R32G32B32A32_SINT, 16, 'i' },
};
// fmts[] entry of R8G8B8A8_UNORM
#define FMT_RGBA8 4


#define NFMTS (sizeof(fmts) / sizeof(fmts[0]))

// bytes of a row of w texels and the rows of h texels: block rows for a compressed format
static uint64_t fmt_row_bytes(unsigned f, uint32_t w) {
  return fmts[f].kind == 'c' ? (uint64_t)((w + 3) / 4) * fmts[f].bpp : (uint64_t)w * fmts[f].bpp;
}

static uint32_t fmt_rows(unsigned f, uint32_t h) {
  return fmts[f].kind == 'c' ? (h + 3) / 4 : h;
}

static uint32_t mem_type(uint32_t bits, VkMemoryPropertyFlags want) {
  for (uint32_t i = 0; i < memprops.memoryTypeCount; i++) {
    if ((bits & (1u << i)) && (memprops.memoryTypes[i].propertyFlags & want) == want)
      return i;
  }
  decline("no memory type for 0x%x", want);
}

// a type with want and prefer if there is one, else one with want
static uint32_t mem_type_pref(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags prefer) {
  for (uint32_t i = 0; i < memprops.memoryTypeCount; i++) {
    if ((bits & (1u << i)) && (memprops.memoryTypes[i].propertyFlags & (want | prefer)) == (want | prefer))
      return i;
  }
  return mem_type(bits, want);
}

struct buf {
  VkBuffer b;
  void *map;
};

static uint64_t fnv(const uint8_t *p, size_t n, uint64_t h);

// jobs run so far: what a pool handed out during the current one stays its
static uint64_t job_count;
// this job is a tile dispatch, and the device was lost (every later submit fails until a new device)
static int job_is_tile, device_lost;

int pd_vk_device_lost(void) {
  return device_lost;
}

// per-job mode: host buffers kept from job to job by power-of-two size. On PowerVR every new allocation is a
// buffer object plus a GPU VM map (and an unmap when freed), each a kernel call that waits for the firmware
#define BUFPOOL 96
#define BUFPOOL_BYTES (96u << 20)
static struct {
  VkBuffer b;
  VkDeviceMemory m;
  void *map;
  VkDeviceSize size;
  uint64_t job, used;
} bufpool[BUFPOOL];
static uint64_t bufpool_clock, bufpool_total;
static int bufpool_on = -1;

static struct buf alloc_host_buf(VkDeviceSize size, VkBufferUsageFlags usage, int readback, VkDeviceMemory *mem);

static int pooled_buf(VkDeviceSize size, struct buf *out) {
  VkDeviceSize want = 65536;
  unsigned i, slot = BUFPOOL;

  if (bufpool_on < 0) {
    const char *e = getenv("AIRJITD_VK_BUFPOOL");
    bufpool_on = !e || *e != '0';
  }
  if (!bufpool_on || resident_mode || size > BUFPOOL_BYTES / 4)
    return 0;
  while (want < size) {
    want <<= 1;
  }
  for (i = 0; i < BUFPOOL; i++) {
    if (bufpool[i].b && bufpool[i].size == want && bufpool[i].job != job_count) {
      bufpool[i].job = job_count, bufpool[i].used = ++bufpool_clock;
      out->b = bufpool[i].b, out->map = bufpool[i].map;
      return 1;
    }
  }
  // room: a free entry, else the least recently used ones not in this job go
  for (;;) {
    unsigned lru = BUFPOOL;

    slot = BUFPOOL;
    for (i = 0; i < BUFPOOL; i++) {
      if (!bufpool[i].b) {
        slot = slot == BUFPOOL ? i : slot;
        continue;
      }
      if (bufpool[i].job != job_count && (lru == BUFPOOL || bufpool[i].used < bufpool[lru].used))
        lru = i;
    }
    if (slot != BUFPOOL && bufpool_total + want <= BUFPOOL_BYTES)
      break;
    if (lru == BUFPOOL)
      return 0;
    vkDestroyBuffer(dev, bufpool[lru].b, 0);
    vkFreeMemory(dev, bufpool[lru].m, 0);
    bufpool_total -= bufpool[lru].size;
    memset(&bufpool[lru], 0, sizeof(bufpool[lru]));
  }
  // every role a job's host buffer has, so any request of the size can take it
  *out = alloc_host_buf(want,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        1, &bufpool[slot].m);
  bufpool[slot].b = out->b, bufpool[slot].map = out->map, bufpool[slot].size = want;
  bufpool[slot].job = job_count, bufpool[slot].used = ++bufpool_clock;
  bufpool_total += want;
  return 1;
}

// a host-visible buffer of the job (or batch). One the CPU reads back from is host cached: uncached device
// memory reads back at a few MB/s
static struct buf new_buf_mem(VkDeviceSize size, VkBufferUsageFlags usage, int readback) {
  struct buf r;

  if (pooled_buf(size, &r)) {
    host_track(r.map, size);
    return r;
  }
  r = alloc_host_buf(size, usage, readback, 0);
  host_track(r.map, size);
  return r;
}

// mem 0: the buffer and its memory belong to the job's arena, else to the caller
static struct buf alloc_host_buf(VkDeviceSize size, VkBufferUsageFlags usage, int readback, VkDeviceMemory *mem) {
  VkBufferCreateInfo ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  VkMemoryRequirements rq;
  VkDeviceMemory m;
  struct buf r;

  if (!mem && (arena.nbuf == ARENA_BUFS || arena.nmem == ARENA_BUFS + 64))
    decline("too many buffers");
  ci.size = size ? (size + 15) & ~(VkDeviceSize)15 : 16, ci.usage = usage;
  CK(vkCreateBuffer(dev, &ci, 0, &r.b));
  pd_vk_last.allocs++;
  if (!mem)
    arena.buf[arena.nbuf++] = r.b;
  vkGetBufferMemoryRequirements(dev, r.b, &rq);
  ai.allocationSize = rq.size;
  ai.memoryTypeIndex = readback ? mem_type_pref(rq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                                : mem_type(rq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  logv("AIRJITD:   host buffer %llu bytes, usage 0x%x: memory types 0x%x, type %u", (unsigned long long)size, usage,
       rq.memoryTypeBits, ai.memoryTypeIndex);
  if (drm_cpu_cached)
    drm_cpu_cached(1);
  {
    VkResult ar = vkAllocateMemory(dev, &ai, 0, &m);

    if (drm_cpu_cached)
      drm_cpu_cached(0);
    if (ar != VK_SUCCESS && mem)
      vkDestroyBuffer(dev, r.b, 0);
    CK(ar);
  }
  if (mem)
    *mem = m;
  else
    arena.mem[arena.nmem++] = m;
  CK(vkBindBufferMemory(dev, r.b, m, 0));
  CK(vkMapMemory(dev, m, 0, VK_WHOLE_SIZE, 0, &r.map));
  return r;
}

static struct buf new_buf(VkDeviceSize size, VkBufferUsageFlags usage) {
  return new_buf_mem(size, usage, (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0);
}

struct img {
  VkImage i;
  VkImageView v;
  VkFormat f;
  struct PDDrawTex t;
  // a texture's levels and slices (l.flags 0: level 0 only), per-level views of a storage texture
  struct PDTexLayout l;
  VkImageAspectFlags aspect;
  VkImageView lv[16];
  // resident images: their memory and views by VkImageViewType, kept with them
  VkDeviceMemory mem;
  VkImageView tv[8];
  struct resident *res;
};


static VkCommandBuffer begin(void) {
  VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  VkCommandBuffer cb;

  if (resident_mode && batch && !direct)
    return batch;
  if (!resident_mode && jcb)
    return jcb;
  ai.commandPool = pool, ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, ai.commandBufferCount = 1;
  CK(vkAllocateCommandBuffers(dev, &ai, &cb));
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CK(vkBeginCommandBuffer(cb, &bi));
  if (resident_mode && !direct)
    batch = cb;
  if (!resident_mode && job_batch)
    jcb = cb;
  return cb;
}

// everything recorded so far happens before what comes next
static void full_barrier(VkCommandBuffer cb) {
  VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };

  mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, 0, 0, 0);
}

static void submit(VkCommandBuffer cb) {
  VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  VkResult r;

  uint64_t t0 = now_us();

  // a batch keeps recording: the next commands wait for these
  if (resident_mode && cb == batch) {
    full_barrier(cb);
    return;
  }
  if (cb && cb == jcb) {
    jcb_dirty = 1;
    return;
  }
  CK(vkEndCommandBuffer(cb));
  si.commandBufferCount = 1, si.pCommandBuffers = &cb;
  host_maintain(0);
  logv("AIRJITD: vulkan submit: queue submit");
  r = vkResetFences(dev, 1, &submit_fence);
  if (r == VK_SUCCESS)
    r = vkQueueSubmit(queue, 1, &si, submit_fence);
  if (r == VK_SUCCESS) {
    logv("AIRJITD: vulkan submit: wait fence");
    // a fence that never signals freezes every client: say so every 5 s while waiting on
    for (unsigned s = 5; (r = vkWaitForFences(dev, 1, &submit_fence, VK_TRUE, 5000000000ull)) == VK_TIMEOUT; s += 5)
      logmsg("AIRJITD: vulkan submit: fence still pending after %u s (job %llu)", s, (unsigned long long)job_count);
  }
  logv("AIRJITD: vulkan submit: finished (%d)", (int)r);
  host_maintain(1);
  pd_vk_last.wait += now_us() - t0, pd_vk_last.submits++;
  vkFreeCommandBuffers(dev, pool, 1, &cb);
  if (r == VK_ERROR_DEVICE_LOST)
    device_lost = 1;
  if (r != VK_SUCCESS)
    decline("submit: VkResult %d", (int)r);
}

// runs what the job recorded so far and waits for it
static void job_sync(void) {
  VkCommandBuffer cb = jcb;

  if (!cb)
    return;
  jcb = 0, jcb_dirty = 0;
  submit(cb);
}

// what comes next in the job's buffer waits for everything recorded before
static void job_fence(VkCommandBuffer cb) {
  if (cb && cb == jcb && jcb_dirty)
    full_barrier(cb), jcb_dirty = 0;
}

static void barrier_a(VkCommandBuffer cb, VkImage im, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to) {
  VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

  b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  b.oldLayout = from, b.newLayout = to, b.image = im;
  b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.subresourceRange.aspectMask = aspect, b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  // an upload in the job's buffer: transfer to transfer, which needs no firmware wait, job_fence covers the rest
  if (cb && cb == jcb) {
    VkPipelineStageFlags src = from == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(cb, src, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b);
    jcb_dirty = 1;
    return;
  }
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 0, 0, 1, &b);
}

static void barrier(VkCommandBuffer cb, VkImage im, VkImageLayout from, VkImageLayout to) {
  barrier_a(cb, im, VK_IMAGE_ASPECT_COLOR_BIT, from, to);
}

static uint32_t mip_dim(uint32_t n, unsigned k) {
  n >>= k;
  return n ? n : 1;
}

// the copy regions of a texture in the bridge: level k of slice s at its offset from t->off, rows of its
// own pitch. Answers how many, and the byte span they cover
static unsigned tex_regions(const struct PDDrawTex *t, const struct PDTexLayout *l, unsigned f, VkBufferImageCopy *cp,
                            unsigned max, uint64_t *span) {
  unsigned n = 0, k, s, levels = 1, slices = 1, three = 0, bpp = fmts[f].bpp, block = fmts[f].kind == 'c' ? 4 : 1;

  *span = 0;
  if (l && (l->flags & PD_TEXLAYOUT_SET)) {
    levels = l->levels ? l->levels : 1, three = l->type == 7;
    slices = three ? 1 : l->slices ? l->slices : 1;
  }
  for (s = 0; s < slices; s++) {
    for (k = 0; k < levels && k < 16; k++) {
      uint32_t bpr = k ? l->level[k].bpr : t->bpr, w = mip_dim(t->w, k), h = mip_dim(t->h, k);
      uint32_t d = three ? mip_dim(l->slices ? l->slices : 1, k) : 1;
      uint64_t off = (uint64_t)s * (l ? l->sliceBytes : 0) + (k ? l->level[k].off : 0);
      uint64_t end = off + (uint64_t)bpr * fmt_rows(f, h) * d;

      if (n == max)
        decline("texture with too many levels and slices");
      if (bpr < fmt_row_bytes(f, w) || bpr % bpp || off % 4)
        decline("texture level %u pitch %u", k, bpr);
      memset(&cp[n], 0, sizeof(cp[n]));
      // a compressed level's row length and height are in texels, whole blocks of them
      cp[n].bufferOffset = off, cp[n].bufferRowLength = bpr / bpp * block, cp[n].bufferImageHeight = fmt_rows(f, h) * block;
      cp[n].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, cp[n].imageSubresource.mipLevel = k;
      cp[n].imageSubresource.baseArrayLayer = s, cp[n].imageSubresource.layerCount = 1;
      cp[n].imageExtent.width = w, cp[n].imageExtent.height = h, cp[n].imageExtent.depth = d;
      *span = end > *span ? end : *span;
      n++;
    }
  }
  return n;
}

// a texture from the bridge, uploaded and left in GENERAL for sampling, storage or attachment use. l gives its levels and slices (NULL: level 0),
// view is the view type the shader declares (-1 for 2D), and dim3 makes a layoutless texture a 3D image of depth 1 (the kext only knows 2D)

// set while a resident resource is made: its objects stay out of the job's arena and live in device memory
static int keep_objects;

// a draw's attachments only move the pixels it can touch: viewport and scissor of the target. Outside
// it the image may hold anything, nothing reads it back
static struct {
  int on;
  uint32_t x, y, w, h, tw, th;
} draw_rect;
static int fullrt = -1;

// narrows a single-level 2D copy of a target-sized image to draw_rect and gives the byte range it
// covers, 0 when the image is not one to narrow
static int rect_region(const struct img *r, VkBufferImageCopy *cp, unsigned ncp, uint64_t *lo, uint64_t *hi) {
  unsigned bpp = fmts[r->t.fmt].bpp;

  if (!draw_rect.on || ncp != 1 || fmts[r->t.fmt].kind == 'c' || r->t.w != draw_rect.tw || r->t.h != draw_rect.th ||
      cp->imageExtent.depth != 1 ||
      cp->imageSubresource.mipLevel || cp->imageSubresource.baseArrayLayer)
    return 0;
  cp->bufferOffset += (uint64_t)draw_rect.y * r->t.bpr + (uint64_t)draw_rect.x * bpp;
  cp->bufferImageHeight = draw_rect.h;
  cp->imageOffset.x = (int32_t)draw_rect.x, cp->imageOffset.y = (int32_t)draw_rect.y;
  cp->imageExtent.width = draw_rect.w, cp->imageExtent.height = draw_rect.h;
  *lo = cp->bufferOffset;
  *hi = cp->bufferOffset + (uint64_t)(draw_rect.h - 1) * r->t.bpr + (uint64_t)draw_rect.w * bpp;
  return 1;
}

// a texture's bytes from the bridge into r, recorded on the job's (or batch's) command buffer
static void img_upload(struct img *r, uint8_t *base) {
  VkBufferImageCopy cp[96];
  VkCommandBuffer cb;
  struct buf st;
  uint64_t span, lo, hi;
  unsigned ncp = tex_regions(&r->t, r->l.flags ? &r->l : 0, r->t.fmt, cp, 96, &span);

  uint64_t t0 = now_us();

  st = new_buf(span, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  prof.newbuf_us += now_us() - t0, prof.newbuf_n++;
  t0 = now_us();
  if (rect_region(r, cp, ncp, &lo, &hi)) {
    to_map((uint8_t *)st.map + lo, base + r->t.off + lo, hi - lo);
    pd_vk_last.up_bytes += hi - lo, prof.img_b += hi - lo;
  } else {
    to_map(st.map, base + r->t.off, span);
    pd_vk_last.up_bytes += span, prof.img_b += span;
  }
  prof.img_us += now_us() - t0;
  cb = begin();
  barrier(cb, r->i, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdCopyBufferToImage(cb, st.b, r->i, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ncp, cp);
  barrier(cb, r->i, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
  submit(cb);
}

static struct img new_img_l(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, const struct PDTexLayout *l,
                            int attachment, int dim3, int view) {
  VkImageCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  VkMemoryRequirements rq;
  VkFormatProperties fp;
  VkBufferImageCopy cp[96];
  VkDeviceMemory m;
  struct img r = { 0 };
  unsigned bpp, ncp, levels = 1, layers = 1, depth = 1;
  uint64_t span;

  uint64_t tn = now_us();

  if (t->fmt >= NFMTS || fmts[t->fmt].kind == 'd')
    decline("texture format %u", t->fmt);
  bpp = fmts[t->fmt].bpp;
  if (l && !(l->flags & PD_TEXLAYOUT_SET))
    l = 0;
  ncp = tex_regions(t, l, t->fmt, cp, 96, &span);
  if (t->off > bsize || span > bsize - t->off || t->bpr < fmt_row_bytes(t->fmt, t->w))
    decline("texture outside the bridge");
  if (arena.nimg == 64)
    decline("too many images");
  if (l) {
    levels = l->levels ? l->levels : 1;
    if (l->type == 7)
      depth = l->slices ? l->slices : 1;
    else
      layers = l->slices ? l->slices : 1;
  }
  if ((view == VK_IMAGE_VIEW_TYPE_CUBE || view == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) && (layers % 6 || t->w != t->h))
    decline("cube texture with %u slices", layers);
  if (view == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY && !lim.cube_array)
    decline("cube arrays");

  r.t = *t, r.f = fmts[t->fmt].vk, r.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (l)
    r.l = *l;
  vkGetPhysicalDeviceFormatProperties(phys, r.f, &fp);
  // block compressed formats need the device's textureCompressionBC
  if (fmts[t->fmt].kind == 'c' && !(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
    decline("format %u not sampled by this device", t->fmt);
  ci.imageType = dim3 || depth > 1 || view == VK_IMAGE_VIEW_TYPE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D, ci.format = r.f;
  ci.extent.width = t->w, ci.extent.height = t->h, ci.extent.depth = depth;
  ci.mipLevels = levels, ci.arrayLayers = layers, ci.samples = VK_SAMPLE_COUNT_1_BIT, ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
    ci.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  // a resident image serves every role its format allows: sampled, storage, attachment
  if (attachment || (keep_objects && (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)))
    ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  if (t->fmt == 17 || t->fmt == 18)
    ci.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  if (layers % 6 == 0 && t->w == t->h && ci.imageType == VK_IMAGE_TYPE_2D)
    ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  CK(vkCreateImage(dev, &ci, 0, &r.i));
  if (!keep_objects)
    arena.img[arena.nimg++] = r.i;
  vkGetImageMemoryRequirements(dev, r.i, &rq);
  ai.allocationSize = rq.size, ai.memoryTypeIndex = mem_type_pref(rq.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CK(vkAllocateMemory(dev, &ai, 0, &m));
  if (keep_objects)
    r.mem = m;
  else
    arena.mem[arena.nmem++] = m;
  CK(vkBindImageMemory(dev, r.i, m, 0));
  pd_vk_last.allocs++;

  vi.image = r.i, vi.format = r.f;
  vi.viewType = view >= 0 ? (VkImageViewType)view : ci.imageType == VK_IMAGE_TYPE_3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vi.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  vi.subresourceRange.layerCount = vi.viewType == VK_IMAGE_VIEW_TYPE_2D || vi.viewType == VK_IMAGE_VIEW_TYPE_3D ? 1 : VK_REMAINING_ARRAY_LAYERS;
  // A8 samples as (0, 0, 0, a)
  if (t->fmt == 2 && !attachment) {
    vi.components.r = vi.components.g = vi.components.b = VK_COMPONENT_SWIZZLE_ZERO;
    vi.components.a = VK_COMPONENT_SWIZZLE_R;
  }
  if (attachment)
    vi.subresourceRange.levelCount = 1;
  CK(vkCreateImageView(dev, &vi, 0, &r.v));
  if (!keep_objects)
    arena.view[arena.nview++] = r.v;

  (void)ncp, (void)cp;
  prof.newimg_us += now_us() - tn, prof.newimg_n++;
  img_upload(&r, base);
  return r;
}

static struct img new_img(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, int attachment, int dim3) {
  return new_img_l(base, bsize, t, 0, attachment, dim3, -1);
}

// the views of a storage texture's levels, one each
static void level_views(struct img *r, const struct a2s_resource *res, int view) {
  unsigned k, n = r->l.levels ? r->l.levels : 1;

  for (k = 0; k < n && k < 16; k++) {
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

    if (arena.nview == 64)
      decline("too many image views");
    vi.image = r->i, vi.format = r->f, vi.viewType = (VkImageViewType)view;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vi.subresourceRange.baseMipLevel = k;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = view == VK_IMAGE_VIEW_TYPE_2D || view == VK_IMAGE_VIEW_TYPE_3D ? 1 : VK_REMAINING_ARRAY_LAYERS;
    CK(vkCreateImageView(dev, &vi, 0, &r->lv[k]));
    if (!r->res)
      arena.view[arena.nview++] = r->lv[k];
  }
  (void)res;
}

// a depth/stencil attachment: the bridge's interleaved plane split into the depth and stencil aspects
static void depth_planes(const struct img *r, uint8_t *px, uint8_t *dp, uint8_t *sp, int to_bridge) {
  uint32_t x, y, w = r->t.w, h = r->t.h, f = r->t.fmt;
  unsigned bpp = fmts[f].bpp, db = f == 25 ? 2 : f == 28 ? 0 : 4;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      uint8_t *p = px + (uint64_t)y * r->t.bpr + (uint64_t)x * bpp;
      uint64_t i = (uint64_t)y * w + x;

      if (db && to_bridge && f == 27) {
        // D24S8: depth in the word's low 24 bits, the stencil byte on top
        uint32_t v;
        memcpy(&v, dp + i * 4, 4);
        v = (v & 0xffffff) | (uint32_t)p[3] << 24;
        memcpy(p, &v, 4);
      } else if (db) {
        if (to_bridge)
          memcpy(p, dp + i * db, db);
        else
          memcpy(dp + i * db, p, db);
      }
      if (f == 26 || f == 27 || f == 28) {
        unsigned so = f == 26 ? 4 : f == 27 ? 3 : 0;
        if (to_bridge)
          p[so] = sp[i];
        else
          sp[i] = p[so];
      }
    }
  }
}

static void depth_upload(struct img *r, uint8_t *base) {
  VkBufferImageCopy cp[2] = { { 0 }, { 0 } };
  VkCommandBuffer cb;
  struct buf st;
  uint64_t npx = (uint64_t)r->t.w * r->t.h, dsize = r->t.fmt == 28 ? 0 : npx * (r->t.fmt == 25 ? 2 : 4);
  unsigned nc = 0;

  // the depth plane tightly packed (words, or halves for D16), then the stencil bytes
  st = new_buf(dsize + npx + 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  depth_planes(r, base + r->t.off, st.map, (uint8_t *)st.map + dsize, 0);
  pd_vk_last.up_bytes += (uint64_t)r->t.bpr * r->t.h;
  if (r->aspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    cp[nc].imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, nc++;
  if (r->aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    cp[nc].bufferOffset = dsize, cp[nc].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT, nc++;
  for (unsigned k = 0; k < nc; k++) {
    cp[k].imageSubresource.layerCount = 1, cp[k].imageExtent.width = r->t.w, cp[k].imageExtent.height = r->t.h;
    cp[k].imageExtent.depth = 1;
  }
  cb = begin();
  barrier_a(cb, r->i, r->aspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdCopyBufferToImage(cb, st.b, r->i, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, nc, cp);
  barrier_a(cb, r->i, r->aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
  submit(cb);
}

static struct img new_depth(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t) {
  VkImageCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  VkMemoryRequirements rq;
  VkFormatProperties fp;
  VkDeviceMemory m;
  struct img r = { 0 };
  unsigned f = t->fmt;

  if (f >= NFMTS || fmts[f].kind != 'd')
    decline("depth format %u", f);
  if (t->off > bsize || (uint64_t)t->bpr * t->h > bsize - t->off || t->bpr < t->w * fmts[f].bpp)
    decline("depth attachment outside the bridge");
  r.t = *t, r.f = fmts[f].vk;
  vkGetPhysicalDeviceFormatProperties(phys, r.f, &fp);
  if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    decline("depth format %u is no attachment here", f);
  r.aspect = (f != 28 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (f >= 26 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
  ci.imageType = VK_IMAGE_TYPE_2D, ci.format = r.f, ci.extent.width = t->w, ci.extent.height = t->h, ci.extent.depth = 1;
  ci.mipLevels = 1, ci.arrayLayers = 1, ci.samples = VK_SAMPLE_COUNT_1_BIT, ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (arena.nimg == 64)
    decline("too many images");
  CK(vkCreateImage(dev, &ci, 0, &r.i));
  if (!keep_objects)
    arena.img[arena.nimg++] = r.i;
  vkGetImageMemoryRequirements(dev, r.i, &rq);
  ai.allocationSize = rq.size, ai.memoryTypeIndex = mem_type_pref(rq.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CK(vkAllocateMemory(dev, &ai, 0, &m));
  if (keep_objects)
    r.mem = m;
  else
    arena.mem[arena.nmem++] = m;
  CK(vkBindImageMemory(dev, r.i, m, 0));
  vi.image = r.i, vi.viewType = VK_IMAGE_VIEW_TYPE_2D, vi.format = r.f;
  vi.subresourceRange.aspectMask = r.aspect, vi.subresourceRange.levelCount = 1, vi.subresourceRange.layerCount = 1;
  CK(vkCreateImageView(dev, &vi, 0, &r.v));
  if (!keep_objects)
    arena.view[arena.nview++] = r.v;
  depth_upload(&r, base);
  return r;
}

static void read_depth(uint8_t *base, struct img *r) {
  VkBufferImageCopy cp[2] = { { 0 }, { 0 } };
  uint64_t npx = (uint64_t)r->t.w * r->t.h, dsize = r->t.fmt == 28 ? 0 : npx * (r->t.fmt == 25 ? 2 : 4);
  struct buf st = new_buf(dsize + npx + 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkCommandBuffer cb;
  unsigned nc = 0;

  if (r->aspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    cp[nc].imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, nc++;
  if (r->aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    cp[nc].bufferOffset = dsize, cp[nc].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT, nc++;
  for (unsigned k = 0; k < nc; k++) {
    cp[k].imageSubresource.layerCount = 1, cp[k].imageExtent.width = r->t.w, cp[k].imageExtent.height = r->t.h;
    cp[k].imageExtent.depth = 1;
  }
  cb = begin();
  job_fence(cb);
  vkCmdCopyImageToBuffer(cb, r->i, VK_IMAGE_LAYOUT_GENERAL, st.b, nc, cp);
  submit(cb);
  job_sync();
  depth_planes(r, base + r->t.off, st.map, (uint8_t *)st.map + dsize, 1);
  pd_vk_last.down_bytes += (uint64_t)r->t.bpr * r->t.h;
}

// several images back into the bridge with one submission, every level and slice
// the bridge's size in resident mode (pd_vk_set_resident), 0 otherwise
static uint32_t res_bsize;

static void read_imgs(uint8_t *base, struct img **ims, unsigned n) {
  struct buf st[8];
  VkBufferImageCopy cp[8][96];
  unsigned ncp[8];
  uint64_t span[8];
  VkCommandBuffer cb;
  unsigned k, i;

  if (!n)
    return;
  cb = begin();
  job_fence(cb);
  for (k = 0; k < n && k < 8; k++) {
    struct img *r = ims[k];

    uint64_t lo, hi;

    ncp[k] = tex_regions(&r->t, r->l.flags ? &r->l : 0, r->t.fmt, cp[k], 96, &span[k]);
    rect_region(r, cp[k], ncp[k], &lo, &hi);
    st[k] = new_buf(span[k], VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vkCmdCopyImageToBuffer(cb, r->i, VK_IMAGE_LAYOUT_GENERAL, st[k].b, ncp[k], cp[k]);
  }
  submit(cb);
  job_sync();
  uint64_t tb = now_us(), d0 = pd_vk_last.down_bytes;

  for (k = 0; k < n && k < 8; k++) {
    unsigned f = ims[k]->t.fmt;

    if (res_bsize && (uint64_t)ims[k]->t.off + span[k] > res_bsize) {
      logmsg("AIRJITD: image at 0x%x, %ux%u pitch %u format %u, span %llu past the bridge (%u bytes), not read back",
             ims[k]->t.off, ims[k]->t.w, ims[k]->t.h, ims[k]->t.bpr, f, (unsigned long long)span[k], res_bsize);
      continue;
    }
    // row by row (block rows for a compressed format): the bytes between rows and levels stay as they were
    for (i = 0; i < ncp[k]; i++) {
      const VkBufferImageCopy *c = &cp[k][i];
      uint64_t pitch = fmt_row_bytes(f, c->bufferRowLength), width = fmt_row_bytes(f, c->imageExtent.width);
      uint64_t rows = (uint64_t)fmt_rows(f, c->imageExtent.height) * c->imageExtent.depth, y;

      for (y = 0; y < rows; y++) {
        uint64_t o = c->bufferOffset + y * pitch;
        from_map(base + ims[k]->t.off + o, (uint8_t *)st[k].map + o, (size_t)width);
        pd_vk_last.down_bytes += width;
      }
    }
  }
  prof.back_us += now_us() - tb, prof.back_b += pd_vk_last.down_bytes - d0;
}


// resident resources: device copies of bridge ranges kept across jobs. Textures re-upload when marked dirty (Metal
// has no CPU pointer into them), buffers when their hash changes, GPU writes stay there until pd_vk_sync
enum { RES_BUF, RES_IMG, RES_DEPTH };
#define MAX_RESIDENT 512

struct resident {
  uint32_t off, size;
  uint64_t key, hash, used;
  int kind, live, stale, gpu_newer;
  VkBuffer b;
  VkDeviceMemory mem;
  struct img im;
};

static struct resident residents[MAX_RESIDENT];
static uint64_t use_clock;
static uint8_t *res_base;
static uint32_t res_bsize;

/*
 * xxHash - Extremely Fast Hash algorithm
 * Header File
 * Copyright (C) 2012-2023 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

// XXH64 (adapted from xxHash's BSD-2-Clause implementation above). memcpy loads
// accept unaligned bridge offsets, every byte contributes to the cache key.
static uint64_t hash_rot(uint64_t x, unsigned n) { return (x << n) | (x >> (64 - n)); }
static uint64_t hash_load64(const uint8_t *p) { uint64_t w; memcpy(&w, p, 8); return w; }
static uint32_t hash_load32(const uint8_t *p) { uint32_t w; memcpy(&w, p, 4); return w; }
static uint64_t hash_round(uint64_t acc, uint64_t word) {
  return hash_rot(acc + word * 14029467366897019727ull, 31) * 11400714785074694791ull;
}
static uint64_t content_hash_raw(const uint8_t *p, size_t n);
static uint64_t now_us(void);

static uint64_t content_hash(const uint8_t *p, size_t n) {
  uint64_t t0 = now_us(), h = content_hash_raw(p, n);

  prof.hash_us += now_us() - t0, prof.hash_b += n;
  return h;
}

static uint64_t content_hash_raw(const uint8_t *p, size_t n) {
  const uint64_t p1 = 11400714785074694791ull, p2 = 14029467366897019727ull;
  const uint64_t p3 = 1609587929392839161ull, p4 = 9650029242287828579ull, p5 = 2870177450012600261ull;
  size_t left = n;
  uint64_t h;
  if (left >= 32) {
    uint64_t a = p1 + p2, b = p2, c = 0, d = 0 - p1;
    do {
      a = hash_round(a, hash_load64(p)); b = hash_round(b, hash_load64(p + 8));
      c = hash_round(c, hash_load64(p + 16)); d = hash_round(d, hash_load64(p + 24));
      p += 32, left -= 32;
    } while (left >= 32);
    h = hash_rot(a, 1) + hash_rot(b, 7) + hash_rot(c, 12) + hash_rot(d, 18);
    h = (h ^ hash_round(0, a)) * p1 + p4;
    h = (h ^ hash_round(0, b)) * p1 + p4;
    h = (h ^ hash_round(0, c)) * p1 + p4;
    h = (h ^ hash_round(0, d)) * p1 + p4;
  } else h = p5;
  h += n;
  while (left >= 8) {
    h ^= hash_round(0, hash_load64(p));
    h = hash_rot(h, 27) * p1 + p4;
    p += 8, left -= 8;
  }
  if (left >= 4) {
    h ^= (uint64_t)hash_load32(p) * p1;
    h = hash_rot(h, 23) * p2 + p3;
    p += 4, left -= 4;
  }
  while (left) {
    h ^= *p++ * p5; h = hash_rot(h, 11) * p1; --left;
  }
  h ^= h >> 33; h *= p2; h ^= h >> 29; h *= p3; h ^= h >> 32;
  return h;
}


static int overlaps(const struct resident *r, uint32_t off, uint32_t len) {
  return r->live && off < r->off + r->size && r->off < off + len;
}

static void res_destroy(struct resident *r) {
  unsigned k;

  if (r->kind == RES_BUF) {
    vkDestroyBuffer(dev, r->b, 0);
  } else {
    for (k = 0; k < 8; k++) {
      if (r->im.tv[k])
        vkDestroyImageView(dev, r->im.tv[k], 0);
    }
    for (k = 0; k < 16; k++) {
      if (r->im.lv[k])
        vkDestroyImageView(dev, r->im.lv[k], 0);
    }
    vkDestroyImageView(dev, r->im.v, 0);
    vkDestroyImage(dev, r->im.i, 0);
  }
  vkFreeMemory(dev, r->kind == RES_BUF ? r->mem : r->im.mem, 0);
  memset(r, 0, sizeof(*r));
}

// ends the batch and submits it, wait: until it has run, and its arena is freed
void pd_vk_flush(int wait) {
  VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  uint64_t t0 = now_us();

  if (state != 1 || !resident_mode)
    return;
  if (batch) {
    // one batch in flight: the one before must be done before its arena is reused
    if (batch_inflight) {
      vkWaitForFences(dev, 1, &batch_fence, 1, UINT64_MAX);
      vkResetFences(dev, 1, &batch_fence);
      cur_arena ^= 1, arena_free(), cur_arena ^= 1;
      batch_inflight = 0;
    }
    arena.cb = batch;
    vkEndCommandBuffer(batch);
    si.commandBufferCount = 1, si.pCommandBuffers = &batch;
    if (vkQueueSubmit(queue, 1, &si, batch_fence) != VK_SUCCESS)
      logmsg("AIRJITD: vulkan batch submit failed");
    batch = 0, batch_inflight = 1, in_pass = 0;
    cur_arena ^= 1;
    pd_vk_last.submits++;
  }
  if (wait && batch_inflight) {
    vkWaitForFences(dev, 1, &batch_fence, 1, UINT64_MAX);
    vkResetFences(dev, 1, &batch_fence);
    cur_arena ^= 1, arena_free(), cur_arena ^= 1;
    batch_inflight = 0;
  }
  pd_vk_last.wait += now_us() - t0;
}

// what the GPU wrote into r, back into the bridge (the batch must have run)
static void res_readback(struct resident *r) {
  // the bytes go back into the bridge: never past its end
  if ((uint64_t)r->off + r->size > res_bsize) {
    logmsg("AIRJITD: resident %s at 0x%x + %u past the bridge (%u bytes), not read back",
           r->kind == RES_BUF ? "buffer" : r->kind == RES_DEPTH ? "depth" : "image", r->off, r->size, res_bsize);
    r->gpu_newer = 0;
    return;
  }
  logv("AIRJITD:   read back %s at 0x%x + %u into %p (bridge %p + %u)", r->kind == RES_BUF ? "buffer" : r->kind == RES_DEPTH ? "depth" : "image",
       r->off, r->size, (void *)(res_base + r->off), (void *)res_base, res_bsize);
  direct = 1;
  if (r->kind == RES_BUF) {
    VkBufferCopy c = { 0, 0, r->size };
    struct buf st = new_buf(r->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkCommandBuffer cb = begin();

    vkCmdCopyBuffer(cb, r->b, st.b, 1, &c);
    submit(cb);
    logv("AIRJITD:   read back from staging %p", st.map);
    from_map(res_base + r->off, st.map, r->size);
    pd_vk_last.down_bytes += r->size;
  } else if (r->kind == RES_DEPTH) {
    read_depth(res_base, &r->im);
  } else {
    struct img *im = &r->im;
    read_imgs(res_base, &im, 1);
  }
  direct = 0;
  r->gpu_newer = 0;
}

static int sync_range(uint32_t off, uint32_t len) {
  int n = 0;

  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    struct resident *r = &residents[i];

    if (!overlaps(r, off, len) || !r->gpu_newer)
      continue;
    if (!n++)
      pd_vk_flush(1);
    res_readback(r);
  }
  return n;
}

void pd_vk_sync(uint32_t off, uint32_t len) {
  if (state != 1 || !resident_mode || setjmp(job_fail))
    return;
  sync_range(off, len);
}

void pd_vk_sync_all(void) {
  pd_vk_sync(0, 0xffffffffu);
}

void pd_vk_finish(void) {
  int bufs = 0;

  if (state != 1 || !resident_mode)
    return;
  // apps read buffer memory directly once the command buffer completes: what the GPU wrote goes back now
  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    bufs |= residents[i].live && residents[i].kind == RES_BUF && residents[i].gpu_newer;
  }
  if (!bufs) {
    pd_vk_flush(0);
    return;
  }
  if (setjmp(job_fail))
    return;
  pd_vk_flush(1);
  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    if (residents[i].live && residents[i].kind == RES_BUF && residents[i].gpu_newer)
      res_readback(&residents[i]);
  }
}

void pd_vk_dirty(uint32_t off, uint32_t len) {
  if (ext_att.image && ext_att.off >= off && ext_att.off < off + len)
    ext_att.written = 1;
  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    if (overlaps(&residents[i], off, len))
      residents[i].stale = 1, residents[i].gpu_newer = 0;
  }
}

void pd_vk_set_resident(int on, uint8_t *base, uint32_t bsize) {
  resident_mode = on, res_base = base, res_bsize = bsize;
}

int pd_vk_resident(void) {
  return resident_mode && state == 1;
}

// the GPU wrote r: other resources over the same bytes are out of date
static void res_written(struct resident *r) {
  if (!r)
    return;
  r->gpu_newer = 1, r->stale = 0;
  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    if (&residents[i] != r && overlaps(&residents[i], r->off, r->size))
      residents[i].stale = 1;
  }
}

// a slot for a resource over [off, off + size) made as key: the one that matches, else a fresh one. A
// different resource over overlapping bytes with newer GPU contents goes back to the bridge first
static struct resident *res_slot(uint32_t off, uint32_t size, uint64_t key, int kind, int *fresh) {
  struct resident *r = 0, *lru = 0;
  unsigned i;

  for (i = 0; i < MAX_RESIDENT; i++) {
    struct resident *s = &residents[i];

    if (s->live && s->off == off && s->size == size && s->key == key && s->kind == kind)
      r = s;
  }
  for (i = 0; i < MAX_RESIDENT; i++) {
    struct resident *s = &residents[i];

    if (s != r && s->gpu_newer && overlaps(s, off, size)) {
      sync_range(s->off, s->size);
      if (r)
        r->stale = 1;
    }
  }
  if (r) {
    r->used = ++use_clock;
    *fresh = 0;
    return r;
  }
  for (i = 0; i < MAX_RESIDENT; i++) {
    if (!residents[i].live) {
      r = &residents[i];
      break;
    }
    if (!residents[i].gpu_newer && (!lru || residents[i].used < lru->used))
      lru = &residents[i];
  }
  // full: the least recently used goes, once nothing in flight uses it
  if (!r) {
    if (!lru)
      decline("resident table full");
    pd_vk_flush(1);
    res_destroy(lru);
    r = lru;
  }
  r->off = off, r->size = size, r->key = key, r->kind = kind, r->live = 1, r->used = ++use_clock;
  *fresh = 1;
  return r;
}

// a device-local buffer with the bridge's bytes, uploaded when they changed. Small ones the shader only reads (setBytes data, at a new place every frame)
// are copied into the batch to keep the table from churning
#define RES_SMALL_BUFFER 65536
static VkBuffer res_buffer(uint8_t *base, uint32_t off, uint32_t size, int writes, struct resident **out) {
  int fresh;
  struct resident *r;
  uint64_t h;

  logv("AIRJITD:   resident buffer 0x%x + %u%s", off, size, writes ? " written" : "");
  if (!writes && size <= RES_SMALL_BUFFER) {
    struct buf b;

    // a sync flushes the batch and frees its arena: the buffer comes from the next one
    for (unsigned i = 0; i < MAX_RESIDENT; i++) {
      if (residents[i].gpu_newer && overlaps(&residents[i], off, size))
        sync_range(off, size);
    }
    b = new_buf_mem(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, 0);
    logv("AIRJITD:   small buffer through %p + %u", b.map, size);
    to_map(b.map, base + off, size);
    pd_vk_last.up_bytes += size;
    if (out)
      *out = 0;
    return b.b;
  }
  r = res_slot(off, size, 0x627566ull, RES_BUF, &fresh);
  h = content_hash(base + off, size);

  if (fresh) {
    VkBufferCreateInfo ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements rq;

    ci.size = size ? (size + 15) & ~(VkDeviceSize)15 : 16;
    // texel buffer usage too: a texture_buffer binds these bytes through a view
    ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
               VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    if (vkCreateBuffer(dev, &ci, 0, &r->b) != VK_SUCCESS) {
      r->live = 0;
      decline("resident buffer");
    }
    vkGetBufferMemoryRequirements(dev, r->b, &rq);
    ai.allocationSize = rq.size, ai.memoryTypeIndex = mem_type_pref(rq.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(dev, &ai, 0, &r->mem) != VK_SUCCESS) {
      vkDestroyBuffer(dev, r->b, 0);
      r->live = 0;
      decline("no device memory for a %u byte buffer", size);
    }
    vkBindBufferMemory(dev, r->b, r->mem, 0);
    pd_vk_last.allocs++;
  }
  if (fresh || r->stale || (h != r->hash && !r->gpu_newer)) {
    VkBufferCopy c = { 0, 0, size };
    struct buf st = new_buf(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkCommandBuffer cb = begin();

    logv("AIRJITD:   upload through staging %p + %u", st.map, size);
    to_map(st.map, base + off, size);
    vkCmdCopyBuffer(cb, st.b, r->b, 1, &c);
    submit(cb);
    pd_vk_last.up_bytes += size;
    r->hash = h, r->stale = 0;
  }
  if (out)
    *out = writes ? r : 0;
  return r->b;
}

// a buffer the gpu wrote is read back before the job allocates, as a sync while binding frees the job's own buffers.
// only a resident buffer matching a large binding exactly stays on the gpu, res_buffer then uses it as is
static void presync_buffers(const struct PDDrawBuffer *bufs, unsigned n) {
  for (unsigned i = 0; i < n; i++) {
    const struct PDDrawBuffer *b = &bufs[i];

    if (!b->size)
      continue;
    for (unsigned k = 0; k < MAX_RESIDENT; k++) {
      struct resident *s = &residents[k];

      if (!s->gpu_newer || !overlaps(s, b->off, b->size))
        continue;
      if (s->kind == RES_BUF && s->off == b->off && s->size == b->size && b->size > RES_SMALL_BUFFER)
        continue;
      // back in the bridge, and every resident copy of these bytes uploads them again when next used
      sync_range(b->off, b->size);
      pd_vk_dirty(b->off, b->size);
      break;
    }
  }
}

static uint64_t layout_key(const struct PDDrawTex *t, const struct PDTexLayout *l, int extra) {
  uint64_t k = fnv((const uint8_t *)&t->w, 4 * sizeof(uint32_t), 14695981039346656037ull ^ (uint64_t)extra);
  return l ? fnv((const uint8_t *)l, sizeof(*l), k) : k;
}

// a texture or colour attachment kept on the device, uploaded when its bytes were marked written
static struct img *res_image(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, const struct PDTexLayout *l, int dim3,
                             struct resident **out) {
  int fresh;
  uint64_t span;
  VkBufferImageCopy cp[96];
  struct resident *r;

  if (l && !(l->flags & PD_TEXLAYOUT_SET))
    l = 0;
  if (t->fmt >= NFMTS)
    decline("texture format %u", t->fmt);
  tex_regions(t, l, t->fmt, cp, 96, &span);
  r = res_slot(t->off, (uint32_t)span, layout_key(t, l, dim3), RES_IMG, &fresh);
  if (fresh) {
    keep_objects = 1;
    r->im = new_img_l(base, bsize, t, l, 0, dim3, -1);
    keep_objects = 0;
    r->im.res = r;
  } else if (r->stale) {
    img_upload(&r->im, base);
  }
  r->stale = 0, r->im.t = *t;
  if (out)
    *out = r;
  return &r->im;
}

// the view of a resident image a shader declares, all its levels
static VkImageView res_view(struct img *im, int view) {
  VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

  if (view < 0 || view > 7)
    return im->v;
  if (im->tv[view])
    return im->tv[view];
  vi.image = im->i, vi.format = im->f, vi.viewType = (VkImageViewType)view;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vi.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  vi.subresourceRange.layerCount = view == VK_IMAGE_VIEW_TYPE_2D || view == VK_IMAGE_VIEW_TYPE_3D ? 1 : VK_REMAINING_ARRAY_LAYERS;
  if (im->t.fmt == 2) {
    vi.components.r = vi.components.g = vi.components.b = VK_COMPONENT_SWIZZLE_ZERO;
    vi.components.a = VK_COMPONENT_SWIZZLE_R;
  }
  CK(vkCreateImageView(dev, &vi, 0, &im->tv[view]));
  return im->tv[view];
}

static struct img *res_depth(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, struct resident **out) {
  int fresh;
  struct resident *r = res_slot(t->off, t->bpr * t->h, layout_key(t, 0, 0x7a), RES_DEPTH, &fresh);

  if (fresh) {
    keep_objects = 1;
    r->im = new_depth(base, bsize, t);
    keep_objects = 0;
    r->im.res = r;
  } else if (r->stale) {
    depth_upload(&r->im, base);
  }
  r->stale = 0;
  if (out)
    *out = r;
  return &r->im;
}

// a pass's clear on the GPU: nonzero when the attachment is resident and the clear was recorded
int pd_vk_clear(const struct PDDrawTex *t, const float *value, uint32_t stencil, int which) {
  struct resident *r = 0;
  VkCommandBuffer cb;

  if (state != 1 || !resident_mode || setjmp(job_fail))
    return 0;
  for (unsigned i = 0; i < MAX_RESIDENT; i++) {
    struct resident *s = &residents[i];
    if (s->live && s->kind != RES_BUF && s->off == t->off && s->im.t.w == t->w && s->im.t.h == t->h && s->im.t.fmt == t->fmt)
      r = s;
  }
  if (!r)
    return 0;
  cb = begin();
  if (r->kind == RES_DEPTH) {
    VkClearDepthStencilValue v = { value[0], stencil };
    VkImageSubresourceRange sr = { 0, 0, 1, 0, 1 };
    // a clear of only one plane keeps the other
    sr.aspectMask = r->im.aspect & ((which & 1 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (which & 2 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
    if (!sr.aspectMask)
      return 1;
    vkCmdClearDepthStencilImage(cb, r->im.i, VK_IMAGE_LAYOUT_GENERAL, &v, 1, &sr);
  } else {
    VkClearColorValue v;
    VkImageSubresourceRange sr = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    memcpy(v.float32, value, 16);
    vkCmdClearColorImage(cb, r->im.i, VK_IMAGE_LAYOUT_GENERAL, &v, 1, &sr);
  }
  submit(cb);
  res_written(r);
  return 1;
}

// the sampler message's flags word: bit 31 normalized, bits 0/2 min/mag linear, address modes at 8 12 16
static VkSamplerAddressMode addr_mode(unsigned m) {
  switch (m) {
  case 1: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  case 2: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case 3: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case 4:
  case 5: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

// samplers by their state, kept for the daemon's life
static struct {
  uint64_t key;
  VkSampler s;
} samplers[256];
static unsigned nsamplers;

static VkSampler keep_sampler(VkSamplerCreateInfo *ci) {
  VkSampler s;
  uint64_t key;

  // texel addressing is the shader's job (spec constant 16 + i): the sampler stays normalized
  ci->unnormalizedCoordinates = 0;
  key = fnv((const uint8_t *)&ci->magFilter, (size_t)((const uint8_t *)&ci->unnormalizedCoordinates - (const uint8_t *)&ci->magFilter), 1);
  for (unsigned i = 0; i < nsamplers; i++) {
    if (samplers[i].key == key)
      return samplers[i].s;
  }
  if (nsamplers == 256)
    decline("too many samplers");
  CK(vkCreateSampler(dev, ci, 0, &s));
  samplers[nsamplers].key = key, samplers[nsamplers++].s = s;
  return s;
}

static VkSampler bound_sampler(uint32_t flags) {
  VkSamplerCreateInfo ci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

  // unknown state (0): linear and clamped like the CPU path
  if (!flags)
    flags = 0x80000005u;
  ci.magFilter = flags & 4 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  ci.minFilter = flags & 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  ci.addressModeU = addr_mode((flags >> 8) & 15), ci.addressModeV = addr_mode((flags >> 12) & 15);
  ci.addressModeW = addr_mode((flags >> 16) & 15);
  // a mip filter samples the levels. Without one level 0 only, but minifying still takes the min filter:
  // a max lod of 0.25 with nearest levels keeps level 0 and lets the lod pick the filter
  if (PD_SAMP_MIP(flags)) {
    ci.mipmapMode = PD_SAMP_MIP(flags) == 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.maxLod = VK_LOD_CLAMP_NONE;
  } else if (flags & 0x40000000u) {
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, ci.maxLod = 0.25f;
  }
  return keep_sampler(&ci);
}

static int sampler_pixel(uint32_t flags) {
  return flags ? !(flags & 0x80000000u) : unknown_pixel;
}

static float half_to_float(uint16_t h) {
  int e = (h >> 10) & 31, m = h & 1023;
  float v = e ? ldexpf((float)(m | 1024), e - 25) : ldexpf((float)m, -24);

  return h & 0x8000 ? -v : v;
}

// a constexpr sampler from its whole AIR state: the flags word's filters and addressing, plus the border colour,
// anisotropy and lod clamp only the state carries. Comparison samplers wait for sample_compare in air2spv
static VkSampler const_sampler(uint64_t state) {
  uint32_t flags = a2s_sampler_flags(state);
  struct a2s_sampler_desc d;
  VkSamplerCreateInfo ci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
  static const VkBorderColor border[4] = { VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                                           VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK };

  a2s_sampler_state(state, &d);
  ci.magFilter = d.mag ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  ci.minFilter = d.min ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  ci.addressModeU = addr_mode((flags >> 8) & 15), ci.addressModeV = addr_mode((flags >> 12) & 15);
  ci.addressModeW = addr_mode((flags >> 16) & 15);
  ci.borderColor = border[d.border];
  ci.minLod = half_to_float(d.lod_min);
  ci.maxLod = d.lod_max == 0x7bff ? VK_LOD_CLAMP_NONE : half_to_float(d.lod_max);
  if (d.mip) {
    ci.mipmapMode = d.mip == 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  } else {
    // level 0 only, as bound_sampler does
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, ci.maxLod = 0.25f, ci.minLod = 0;
  }
  if (d.anisotropy > 1 && lim.anisotropy > 1) {
    ci.anisotropyEnable = VK_TRUE;
    ci.maxAnisotropy = d.anisotropy < lim.anisotropy ? (float)d.anisotropy : lim.anisotropy;
  }
  return keep_sampler(&ci);
}

// translations by metallib content and options, kept for the daemon's life
struct shader {
  uint64_t key;
  uint32_t *words;
  size_t nwords;
  struct a2s_reflect *refl;
  VkShaderModule mod;
  uint64_t job, used;
};

static struct shader shaders[512];
static unsigned nshaders;
static uint64_t shader_clock;

// a free entry, or the least recently used one no part of this job holds: pipelines keep no
// pointers to their modules, so a module can go once its pipelines exist
static struct shader *shader_slot(void) {
  unsigned lru = sizeof(shaders) / sizeof(shaders[0]);
  struct shader *s;

  if (nshaders < lru)
    return &shaders[nshaders];
  for (unsigned i = 0; i < nshaders; i++) {
    if (shaders[i].job != job_count && (lru == nshaders || shaders[i].used < shaders[lru].used))
      lru = i;
  }
  if (lru == nshaders)
    decline("translation cache full");
  s = &shaders[lru];
  vkDestroyShaderModule(dev, s->mod, 0);
  free(s->words), free(s->refl);
  memset(s, 0, sizeof(*s));
  return s;
}

static struct shader *shader_use(struct shader *s) {
  s->job = job_count, s->used = ++shader_clock;
  return s;
}

static uint64_t fnv(const uint8_t *p, size_t n, uint64_t h) {
  for (size_t i = 0; i < n; i++) {
    h = (h ^ p[i]) * 1099511628211ull;
  }
  return h;
}

// SPIR-V above this many KB stays on the CPU path, as one huge pipeline compile can take more memory than a small host has and kill the driver.
// AIRJITD_VK_MAX_SPIRV_KB sets it (0: no limit), else per GB of memory here 256 KB for a forwarded GPU, 1024 KB for a local one
static uint64_t spirv_limit_kb(void) {
  static int64_t limit = -1;

  if (limit < 0) {
    const char *e = getenv("AIRJITD_VK_MAX_SPIRV_KB");
    uint64_t mem = 0;
    size_t len = sizeof(mem);

    if (e)
      limit = strtoll(e, 0, 10);
#ifdef __APPLE__
    else if (sysctlbyname("hw.memsize", &mem, &len, 0, 0) == 0 && mem)
      limit = (int64_t)(mem >> 30 ? mem >> 30 : 1) * (gpu_remote ? 256 : 1024);
#endif
    else
      limit = 0;
    if (limit < 0)
      limit = 0;
    logmsg("AIRJITD: shaders above %lld KB of SPIR-V stay on the CPU%s", (long long)limit, limit ? "" : " (no limit)");
  }
  return (uint64_t)limit;
}

// translations found too large, declined again without translating
static uint64_t big_keys[64];
static unsigned nbig;

// specialization of large shaders (uber shaders) on their constant buffers: air2spv folds the integer loads it can reach at constant offsets and reports which bytes it used.
// a plan remembers those ranges per shader, so later draws find their variant by hashing the same bytes without translating
#define PIN_MAX_BYTES 4096
struct pin_plan {
  uint64_t key;
  unsigned n;
  struct { uint32_t metal, off, size; } r[128];
};
static struct pin_plan plans[16];
static unsigned nplans, plan_next;

static uint64_t pin_min_bytes(void) {
  const char *e = getenv("AIRJITD_VK_PIN_KB");

  return (e ? strtoull(e, 0, 10) : 64) << 10;
}

static uint64_t pin_key(const struct pin_plan *p, const struct a2s_pin *pins, unsigned npins) {
  uint64_t h = 1469598103934665603ull;

  for (unsigned i = 0; i < p->n; i++) {
    for (unsigned j = 0; j < npins; j++) {
      if (pins[j].metal != p->r[i].metal)
        continue;
      for (uint32_t k = 0; k < p->r[i].size; k++) {
        uint32_t o = p->r[i].off + k;
        h = (h ^ (o < pins[j].size ? pins[j].data[o] : 0xff)) * 1099511628211ull;
      }
    }
  }
  return h;
}

static struct shader *translate_pinned(const uint8_t *mtl, uint32_t size, uint32_t set, uint32_t pcb,
                                       const struct a2s_pin *pins, unsigned npins) {
  struct a2s_options o = { 1, set, pcb, 0, !lim.quad_compute, 0, !lim.dyn_image_index, !lim.demote, pins, npins };
  uint64_t t0 = now_us();
  uint64_t key = content_hash(mtl, size) ^ ((uint64_t)set << 56 | (uint64_t)pcb << 40);
  struct pin_plan *plan = 0;

  if (npins && size < pin_min_bytes())
    npins = 0, o.npins = 0;
  if (npins) {
    for (unsigned i = 0; i < nplans; i++) {
      if (plans[i].key == key)
        plan = &plans[i];
    }
    if (plan)
      key ^= pin_key(plan, pins, npins);
  }
  VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  struct shader *s;
  char err[512];

  for (unsigned i = 0; i < nshaders; i++) {
    if (shaders[i].key == key) {
      pd_vk_last.translated_cached++;
      pd_vk_last.translate += now_us() - t0;
      return shader_use(&shaders[i]);
    }
  }

  for (unsigned i = 0; i < nbig; i++) {
    if (big_keys[i] == key)
      decline("shader over the SPIR-V size limit");
  }
  s = shader_slot();
  s->refl = calloc(1, sizeof(*s->refl));
  if (a2s_translate(mtl, size, &o, &s->words, &s->nwords, s->refl, err, sizeof(err))) {
    free(s->refl), s->refl = 0;
    decline("air2spv: %s", err);
  }
  {
    static uint64_t largest;
    uint64_t kb = ((uint64_t)s->nwords * 4 + 1023) >> 10, limit = spirv_limit_kb();

    if (kb > largest) {
      largest = kb;
      logmsg("AIRJITD: largest shader so far %llu KB of SPIR-V", (unsigned long long)kb);
    }
    if (limit && kb > limit) {
      free(s->words), free(s->refl), s->words = 0, s->refl = 0;
      if (nbig < sizeof(big_keys) / sizeof(big_keys[0]))
        big_keys[nbig++] = key;
      logmsg("AIRJITD: shader %016llx is %llu KB of SPIR-V, over the %llu KB limit: CPU path", (unsigned long long)key,
             (unsigned long long)kb, (unsigned long long)limit);
      decline("shader over the SPIR-V size limit");
    }
  }
  ci.codeSize = s->nwords * 4, ci.pCode = s->words;
  if (vkCreateShaderModule(dev, &ci, 0, &s->mod) != VK_SUCCESS) {
    free(s->words), free(s->refl), s->words = 0, s->refl = 0;
    decline("shader module rejected");
  }
  if (npins && !plan) {
    uint64_t base_key = key;

    // the first translation: remember which bytes it depended on, key it by their values. With every
    // plan taken the oldest goes, its variants stay cached until they age out
    if (nplans < sizeof(plans) / sizeof(plans[0]))
      plan = &plans[nplans++];
    else
      plan = &plans[plan_next++ % (sizeof(plans) / sizeof(plans[0]))];
    plan->key = base_key, plan->n = s->refl->npinned;
    memcpy(plan->r, s->refl->pinned, sizeof(plan->r[0]) * plan->n);
    key ^= pin_key(plan, pins, npins);
    logmsg("AIRJITD: shader %016llx specialized on %u constant ranges: %zu KB of SPIR-V", (unsigned long long)base_key,
           s->refl->npinned, s->nwords * 4 >> 10);
  }
  s->key = key;
  if (s == &shaders[nshaders])
    nshaders++;
  pd_vk_last.translate += now_us() - t0;
  return shader_use(s);
}

static struct shader *translate(const uint8_t *mtl, uint32_t size, uint32_t set, uint32_t pcb) {
  return translate_pinned(mtl, size, set, pcb, 0, 0);
}

static VkDescriptorType desc_type(const struct a2s_resource *r) {
  switch (r->kind) {
  case A2S_RES_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case A2S_RES_TEXTURE:
    if (r->texel)
      return r->storage ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    return r->storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case A2S_RES_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
  case A2S_RES_ATTACHMENT_IMAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  default: return VK_DESCRIPTOR_TYPE_SAMPLER;
  }
}

static VkDescriptorSetLayout set_layout(const struct a2s_reflect *r, VkShaderStageFlags stages) {
  VkDescriptorSetLayoutBinding b[A2S_MAX_RES];
  VkDescriptorSetLayoutCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  VkDescriptorSetLayout l;

  for (unsigned i = 0; i < r->nres; i++) {
    memset(&b[i], 0, sizeof(b[i]));
    b[i].binding = r->res[i].binding, b[i].descriptorType = desc_type(&r->res[i]);
    b[i].descriptorCount = r->res[i].levels ? r->res[i].levels : 1, b[i].stageFlags = stages;
  }
  ci.bindingCount = r->nres, ci.pBindings = b;
  CK(vkCreateDescriptorSetLayout(dev, &ci, 0, &l));
  return l;
}

// per-job mode: textures a shader only samples, kept by content, so an atlas or image that did not change since
// an earlier job is not made and uploaded again (AIRJITD_VK_TEXCACHE=0 turns it off)
#define TEXCACHE 48
#define TEXCACHE_BYTES (48u << 20)
static struct {
  uint64_t key, shape, used, job;
  uint32_t bytes;
  int live;
  struct img im;
} texcache[TEXCACHE];
static uint64_t texcache_clock, texcache_total;
static int texcache_on = -1;

static void texcache_drop(unsigned i) {
  vkDestroyImageView(dev, texcache[i].im.v, 0);
  vkDestroyImage(dev, texcache[i].im.i, 0);
  vkFreeMemory(dev, texcache[i].im.mem, 0);
  texcache_total -= texcache[i].bytes;
  memset(&texcache[i], 0, sizeof(texcache[i]));
}

static struct img sampled_img(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t, int dim3) {
  VkBufferImageCopy cp[96];
  uint64_t span, key;
  unsigned i, slot = TEXCACHE;
  struct img im;

  if (texcache_on < 0) {
    const char *e = getenv("AIRJITD_VK_TEXCACHE");
    texcache_on = !e || *e != '0';
  }
  if (!texcache_on || t->fmt >= NFMTS)
    return new_img_l(base, bsize, t, 0, 0, dim3, -1);
  tex_regions(t, 0, t->fmt, cp, 96, &span);
  if (t->off > bsize || span > bsize - t->off || span > TEXCACHE_BYTES / 4)
    return new_img_l(base, bsize, t, 0, 0, dim3, -1);
  key = content_hash(base + t->off, span) ^ layout_key(t, 0, dim3);
  for (i = 0; i < TEXCACHE; i++) {
    if (texcache[i].live && texcache[i].key == key) {
      texcache[i].used = ++texcache_clock, texcache[i].job = job_count;
      pd_vk_last.tex_hits++;
      return texcache[i].im;
    }
  }
  // an image of the same shape whose contents went unused longest takes the new bytes: no new allocation
  {
    uint64_t shape = layout_key(t, 0, dim3);
    unsigned reuse = TEXCACHE;

    for (i = 0; i < TEXCACHE; i++) {
      if (texcache[i].live && texcache[i].shape == shape && texcache[i].job != job_count &&
          (reuse == TEXCACHE || texcache[i].used < texcache[reuse].used))
        reuse = i;
    }
    if (reuse < TEXCACHE && texcache[reuse].used + TEXCACHE / 2 < texcache_clock) {
      texcache[reuse].im.t = *t;
      img_upload(&texcache[reuse].im, base);
      texcache[reuse].key = key, texcache[reuse].used = ++texcache_clock, texcache[reuse].job = job_count;
      return texcache[reuse].im;
    }
  }
  // room: the least recently used go, never one this job already bound
  for (;;) {
    unsigned lru = TEXCACHE, n = 0;

    for (i = 0; i < TEXCACHE; i++) {
      if (!texcache[i].live) {
        slot = slot == TEXCACHE ? i : slot;
        continue;
      }
      n++;
      if (texcache[i].job != job_count && (lru == TEXCACHE || texcache[i].used < texcache[lru].used))
        lru = i;
    }
    if (slot != TEXCACHE && texcache_total + span <= TEXCACHE_BYTES)
      break;
    if (lru == TEXCACHE || !n)
      return new_img_l(base, bsize, t, 0, 0, dim3, -1);
    texcache_drop(lru);
    slot = TEXCACHE;
  }
  keep_objects = 1;
  im = new_img_l(base, bsize, t, 0, 0, dim3, -1);
  keep_objects = 0;
  texcache[slot].key = key, texcache[slot].used = ++texcache_clock, texcache[slot].job = job_count;
  texcache[slot].shape = layout_key(t, 0, dim3);
  texcache[slot].bytes = (uint32_t)span, texcache[slot].live = 1, texcache[slot].im = im;
  texcache_total += span;
  return im;
}

// per-job mode: colour attachments and their framebuffers kept by size and format from job to job. A new framebuffer costs the PowerVR driver its render target dataset and SPM state,
// and dropping one a firmware cleanup (8 slots evicted on 16% of the A733's draws, 116 s per 4096). contents still upload every job
#define ATTPOOL 32
#define ATTPOOL_BYTES (256u << 20)
#define FBCACHE 64
static uint64_t attpool_total;
static struct {
  uint32_t w, h, fmt;
  uint64_t used, job, bytes;
  int live;
  struct img im;
  // the image holds exactly the bridge bytes at clean_off that hashed to clean_hash: the last readback
  int clean;
  uint32_t clean_off;
  uint64_t clean_hash;
} attpool[ATTPOOL];
// the pool slot attachment_img last handed out, ATTPOOL for none
static unsigned attpool_last = ATTPOOL;
static uint64_t upload_skips;
// AIRJITD_VK_UPLOADSKIP=1: hash targets to skip unchanged uploads. Off by default, on the A733 two full
// hashes per draw cost more than the quarter of uploads they save
static int uploadskip_on = -1;
static struct {
  VkRenderPass rp;
  VkImageView v[4];
  uint32_t n, w, h;
  uint64_t used;
  VkFramebuffer fb;
} fbcache[FBCACHE];
static uint64_t pool_clock;
static int attpool_on = -1;

static void fb_drop_view(VkImageView v) {
  for (unsigned i = 0; i < FBCACHE; i++) {
    for (unsigned k = 0; fbcache[i].fb && k < fbcache[i].n; k++) {
      if (fbcache[i].v[k] == v) {
        vkDestroyFramebuffer(dev, fbcache[i].fb, 0);
        memset(&fbcache[i], 0, sizeof(fbcache[i]));
        break;
      }
    }
  }
}

static struct img attachment_img(uint8_t *base, uint32_t bsize, const struct PDDrawTex *t) {
  unsigned i, hit = ATTPOOL, slot = ATTPOOL;
  struct img im;

  if (attpool_on < 0) {
    const char *e = getenv("AIRJITD_VK_ATTPOOL");
    attpool_on = !e || *e != '0';
  }
  attpool_last = ATTPOOL;
  if (!attpool_on)
    return new_img(base, bsize, t, 1, 0);
  for (i = 0; i < ATTPOOL; i++) {
    if (attpool[i].live && attpool[i].job != job_count && attpool[i].w == t->w && attpool[i].h == t->h &&
        attpool[i].fmt == t->fmt && (hit == ATTPOOL || (attpool[i].clean && attpool[i].clean_off == t->off)))
      hit = i;
  }
  if (hit < ATTPOOL) {
    uint64_t span;
    VkBufferImageCopy cp[96];

    // a reused image takes this job's bytes, which may sit at another offset
    tex_regions(t, 0, t->fmt, cp, 96, &span);
    if (t->off > bsize || span > bsize - t->off || t->bpr < fmt_row_bytes(t->fmt, t->w))
      decline("texture outside the bridge");
    attpool[hit].im.t = *t;
    // what this image got from its last readback is still what the bridge holds: nothing to upload
    if (uploadskip_on < 0) {
      const char *e = getenv("AIRJITD_VK_UPLOADSKIP");
      uploadskip_on = e && *e == '1';
    }
    if (uploadskip_on && attpool[hit].clean && attpool[hit].clean_off == t->off &&
        content_hash(base + t->off, span) == attpool[hit].clean_hash) {
      upload_skips++;
    } else {
      img_upload(&attpool[hit].im, base);
    }
    // until this job reads it back, the image may differ from the bridge
    attpool[hit].clean = 0;
    attpool[hit].used = ++pool_clock, attpool[hit].job = job_count;
    attpool_last = hit;
    prof.pool_hit++;
    return attpool[hit].im;
  }
  uint64_t need = t->fmt < NFMTS ? fmt_row_bytes(t->fmt, t->w) * fmt_rows(t->fmt, t->h) : 0;

  // one target past a quarter of the budget would empty the pool for itself
  if (need == 0 || need > ATTPOOL_BYTES / 4)
    return new_img(base, bsize, t, 1, 0);
  // a free slot within the budget, giving up the least recently used targets (not this job's) until there is
  for (;;) {
    unsigned lru = ATTPOOL;

    slot = ATTPOOL;
    for (i = 0; i < ATTPOOL; i++) {
      if (!attpool[i].live) {
        slot = slot == ATTPOOL ? i : slot;
        continue;
      }
      if (attpool[i].job != job_count && (lru == ATTPOOL || attpool[i].used < attpool[lru].used))
        lru = i;
    }
    if (slot != ATTPOOL && attpool_total + need <= ATTPOOL_BYTES)
      break;
    if (lru == ATTPOOL)
      return new_img(base, bsize, t, 1, 0);
    uint64_t te = now_us();

    fb_drop_view(attpool[lru].im.v);
    prof.evict_fb_us += now_us() - te;
    vkDestroyImageView(dev, attpool[lru].im.v, 0);
    vkDestroyImage(dev, attpool[lru].im.i, 0);
    vkFreeMemory(dev, attpool[lru].im.mem, 0);
    attpool[lru].live = 0;
    attpool_total -= attpool[lru].bytes;
    prof.pool_evict++, prof.evict_us += now_us() - te;
  }
  prof.pool_new++;
  keep_objects = 1;
  im = new_img_l(base, bsize, t, 0, 1, 0, -1);
  keep_objects = 0;
  attpool[slot].w = t->w, attpool[slot].h = t->h, attpool[slot].fmt = t->fmt, attpool[slot].im = im;
  attpool[slot].used = ++pool_clock, attpool[slot].job = job_count, attpool[slot].live = 1;
  attpool[slot].bytes = need, attpool_total += need;
  attpool[slot].clean = 0, attpool_last = slot;
  return im;
}

// a framebuffer over pooled attachments, kept for the next job with the same pass and images
static VkFramebuffer cached_fb(VkRenderPass rp, const VkImageView *v, uint32_t n, uint32_t w, uint32_t h) {
  VkFramebufferCreateInfo fbi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
  unsigned i, slot = 0;

  for (i = 0; i < FBCACHE; i++) {
    if (fbcache[i].fb && fbcache[i].rp == rp && fbcache[i].n == n && fbcache[i].w == w && fbcache[i].h == h &&
        !memcmp(fbcache[i].v, v, n * sizeof(*v))) {
      fbcache[i].used = ++pool_clock;
      return fbcache[i].fb;
    }
  }
  // a free entry, else the least recently used
  for (i = 0; i < FBCACHE; i++) {
    if (!fbcache[i].fb) {
      slot = i;
      break;
    }
    if (fbcache[i].used < fbcache[slot].used)
      slot = i;
  }
  uint64_t tf = now_us();

  if (fbcache[slot].fb)
    vkDestroyFramebuffer(dev, fbcache[slot].fb, 0);
  memset(&fbcache[slot], 0, sizeof(fbcache[slot]));
  fbi.renderPass = rp, fbi.attachmentCount = n, fbi.pAttachments = v;
  fbi.width = w, fbi.height = h, fbi.layers = 1;
  CK(vkCreateFramebuffer(dev, &fbi, 0, &fbcache[slot].fb));
  prof.fb_new++, prof.fb_new_us += now_us() - tf;
  fbcache[slot].rp = rp, fbcache[slot].n = n, fbcache[slot].w = w, fbcache[slot].h = h, fbcache[slot].used = ++pool_clock;
  memcpy(fbcache[slot].v, v, n * sizeof(*v));
  return fbcache[slot].fb;
}

// a stage's bound resources for this job, by Metal index
struct stage_res {
  const struct a2s_reflect *r;
  VkBuffer bufs[32];
  struct img *imgs[16], *att[8], *scratch;
  // texture_buffer bindings: views over a buffer holding the texture's bytes
  VkBufferView bviews[16];
  VkSampler samp[48];
  // resident mode: what the stage writes, gpu_newer once the job is recorded
  struct resident *wres[48];
  unsigned nwres;
};

static void stage_written(struct stage_res *s, struct resident *r) {
  if (r && s->nwres < 48)
    s->wres[s->nwres++] = r;
}

static struct img jobimgs[64];
static unsigned njobimgs;

// the view a shader's texture declaration takes
static int view_type(const struct a2s_resource *r) {
  if (r->dim == 3)
    return VK_IMAGE_VIEW_TYPE_3D;
  if (r->cube)
    return r->arrayed ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
  return r->arrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
}

// metal leaves a buffer the shader reads but the encoder never bound undefined, here it reads 64 KB of zeros
// (the CPU path's raster.c does the same), kept for the daemon's life
static VkBuffer unbound_buffer(void) {
  static VkBuffer zeros;
  static VkDeviceMemory mem;

  if (!zeros) {
    struct buf b = alloc_host_buf(65536, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, &mem);

    zero_map(b.map, 65536);
    zeros = b.b;
  }
  return zeros;
}

// a storage binding of BGRA8 bytes on a device without BGRA8 storage images: an RGBA8 image over the same
// bytes, the shader swapping red and blue (spec constant A2S_SPEC_BGRA)
static int bgra_as_rgba(const struct a2s_resource *r, const struct PDDrawTex *t) {
  static int known = -1, tiles = -1;

  if (!r->storage || !r->bgra_spec || t->fmt >= NFMTS || fmts[t->fmt].vk != VK_FORMAT_B8G8R8A8_UNORM)
    return 0;
  // the first tile kernel let through this way locked up the A733's compute data master: tiles stay on
  // the CPU unless AIRJITD_VK_TILE_BGRA=1
  if (tiles < 0) {
    const char *e = getenv("AIRJITD_VK_TILE_BGRA");
    tiles = e && *e == '1';
  }
  if (job_is_tile && !tiles)
    return 0;
  if (known < 0) {
    VkFormatProperties fp;

    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_B8G8R8A8_UNORM, &fp);
    known = !(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
  }
  return known;
}

static void bind_stage(struct stage_res *s, uint8_t *base, uint32_t bsize, const struct PDDrawBuffer *bufs, unsigned nb,
                       const struct PDDrawTex *texs, const uint32_t *samp, VkBuffer *written, unsigned *nwritten,
                       struct buf *wmaps, const struct PDDrawBuffer **wsrc, const struct PDTexLayout *layouts) {
  for (unsigned i = 0; i < s->r->nres; i++) {
    const struct a2s_resource *r = &s->r->res[i];

    if (r->kind == A2S_RES_BUFFER) {
      struct buf b;
      if (r->metal >= nb)
        decline("buffer %u not bound", r->metal);
      if (!bufs[r->metal].size) {
        s->bufs[r->metal] = unbound_buffer();
        continue;
      }
      if (bufs[r->metal].off > bsize || bufs[r->metal].size > bsize - bufs[r->metal].off)
        decline("buffer %u outside the bridge", r->metal);
      if (resident_mode) {
        struct resident *wr;
        s->bufs[r->metal] = res_buffer(base, bufs[r->metal].off, bufs[r->metal].size, (r->access & A2S_WRITE) != 0, &wr);
        stage_written(s, wr);
        continue;
      }
      {
        uint64_t t0 = now_us();

        b = new_buf(bufs[r->metal].size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        to_map(b.map, base + bufs[r->metal].off, bufs[r->metal].size);
        prof.buf_us += now_us() - t0, prof.buf_b += bufs[r->metal].size;
      }
      pd_vk_last.up_bytes += bufs[r->metal].size;
      s->bufs[r->metal] = b.b;
      if (r->access & A2S_WRITE && *nwritten < 32)
        wmaps[*nwritten] = b, wsrc[(*nwritten)++] = &bufs[r->metal], (void)written;
    } else if (r->kind == A2S_RES_TEXTURE && r->texel) {
      // the texture's bytes are its MTLBuffer's (newTextureWithDescriptor:offset:bytesPerRow:): copied into a
      // texel buffer, and back after the job when the shader writes them
      static struct PDDrawBuffer texel_src[64];
      static unsigned ntexel_src;
      const struct PDDrawTex *tt;
      VkBufferViewCreateInfo vi = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
      VkFormatProperties fp;
      VkFormatFeatureFlags need;
      uint32_t size;
      struct buf b;

      if (r->metal >= 16 || !texs[r->metal].w)
        decline("texture buffer %u not bound", r->metal);
      tt = &texs[r->metal];
      if (tt->fmt >= NFMTS || fmts[tt->fmt].kind == 'c' || fmts[tt->fmt].kind == 'd')
        decline("texture buffer %u has format %u", r->metal, tt->fmt);
      vkGetPhysicalDeviceFormatProperties(phys, fmts[tt->fmt].vk, &fp);
      need = r->storage ? VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT : VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
      if (!(fp.bufferFeatures & need))
        decline("format %u has no texel buffer support", tt->fmt);
      size = (uint32_t)fmt_row_bytes(tt->fmt, tt->w);
      if (tt->off > bsize || size > bsize - tt->off)
        decline("texture buffer %u outside the bridge", r->metal);
      if (arena.nbview == ARENA_VIEWS)
        decline("too many buffer views");
      if (resident_mode) {
        // the resident buffer over the same bridge bytes, so the texture and its MTLBuffer stay one memory
        struct resident *wr;

        vi.buffer = res_buffer(base, tt->off, size, r->storage, &wr);
        stage_written(s, wr);
      } else {
        b = new_buf(size, r->storage ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                     : VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
        to_map(b.map, base + tt->off, size);
        pd_vk_last.up_bytes += size;
        vi.buffer = b.b;
      }
      vi.format = fmts[tt->fmt].vk, vi.offset = 0, vi.range = VK_WHOLE_SIZE;
      CK(vkCreateBufferView(dev, &vi, 0, &s->bviews[r->metal]));
      arena.bview[arena.nbview++] = s->bviews[r->metal];
      if (!resident_mode && r->storage && *nwritten < 32) {
        struct PDDrawBuffer *src = &texel_src[ntexel_src++ % 64];

        src->off = tt->off, src->size = size;
        wmaps[*nwritten] = b, wsrc[(*nwritten)++] = src;
      }
    } else if (r->kind == A2S_RES_TEXTURE) {
      struct PDDrawTex tt;

      if (r->metal >= 16 || !texs[r->metal].w)
        decline("texture %u not bound", r->metal);
      tt = texs[r->metal];
      if (bgra_as_rgba(r, &tt))
        tt.fmt = FMT_RGBA8;
      if (r->storage && tt.fmt < NFMTS) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(phys, fmts[tt.fmt].vk, &fp);
        if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
          decline("format %u has no storage image support", tt.fmt);
      }
      {
        const struct PDTexLayout *l = layouts && (layouts[r->metal].flags & PD_TEXLAYOUT_SET) ? &layouts[r->metal] : 0;
        int view = view_type(r);

        if (!l && view != VK_IMAGE_VIEW_TYPE_2D && view != VK_IMAGE_VIEW_TYPE_3D)
          decline("texture %u is an array or cube without a layout", r->metal);
        if (resident_mode) {
          // the job's copy of the resident image, with the view this shader declares
          struct resident *rr;
          struct img *im = res_image(base, bsize, &tt, l, r->dim == 3 && !l, &rr);

          if (l && r->levels && !im->lv[0])
            level_views(im, r, view);
          jobimgs[njobimgs] = *im;
          if (l)
            jobimgs[njobimgs].v = res_view(im, view);
          if (r->storage)
            stage_written(s, rr);
        } else {
          if (!l && !r->storage && !(r->access & A2S_WRITE))
            jobimgs[njobimgs] = sampled_img(base, bsize, &tt, r->dim == 3);
          else
            jobimgs[njobimgs] = new_img_l(base, bsize, &tt, l, 0, r->dim == 3 && !l, l ? view : -1);
          if (l && r->levels)
            level_views(&jobimgs[njobimgs], r, view);
        }
      }
      s->imgs[r->metal] = &jobimgs[njobimgs++];
      if (r->levels && !s->scratch) {
        // the bridge holds level 0 only: other levels go to scratch nothing reads back
        jobimgs[njobimgs] = new_img(base, bsize, &tt, 0, 0);
        s->scratch = &jobimgs[njobimgs++];
      }
    } else if (r->kind == A2S_RES_SAMPLER) {
      s->samp[r->metal] = bound_sampler(r->metal < 16 ? samp[r->metal] : 0);
    } else if (r->kind == A2S_RES_CONST_SAMPLER) {
      s->samp[16 + r->metal] = const_sampler(r->state);
    }
  }
}

static VkDescriptorSet fill_set(struct stage_res *s, VkDescriptorSetLayout l) {
  VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  VkWriteDescriptorSet w[A2S_MAX_RES];
  VkDescriptorBufferInfo bi[A2S_MAX_RES];
  VkDescriptorImageInfo ii[A2S_MAX_RES], lv[A2S_MAX_RES][16];
  VkDescriptorSet set;
  unsigned n = 0;

  ai.descriptorPool = arena.dpool, ai.descriptorSetCount = 1, ai.pSetLayouts = &l;
  CK(vkAllocateDescriptorSets(dev, &ai, &set));

  for (unsigned i = 0; i < s->r->nres; i++) {
    const struct a2s_resource *r = &s->r->res[i];

    memset(&w[n], 0, sizeof(w[n]));
    memset(&ii[n], 0, sizeof(ii[n]));
    w[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, w[n].dstSet = set, w[n].dstBinding = r->binding;
    w[n].descriptorCount = 1, w[n].descriptorType = desc_type(r);
    if (r->kind == A2S_RES_BUFFER) {
      bi[n].buffer = s->bufs[r->metal], bi[n].offset = 0, bi[n].range = VK_WHOLE_SIZE;
      w[n].pBufferInfo = &bi[n];
    } else if (r->kind == A2S_RES_TEXTURE && r->texel) {
      w[n].pTexelBufferView = &s->bviews[r->metal];
    } else if (r->kind == A2S_RES_TEXTURE || r->kind == A2S_RES_INPUT_ATTACHMENT || r->kind == A2S_RES_ATTACHMENT_IMAGE) {
      struct img *im = r->kind == A2S_RES_TEXTURE ? s->imgs[r->metal] : s->att[r->metal];
      if (!im)
        decline("attachment %u not bound", r->metal);
      ii[n].imageView = im->v, ii[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      w[n].pImageInfo = &ii[n];
      if (r->levels) {
        for (unsigned k = 0; k < r->levels && k < 16; k++) {
          lv[n][k] = ii[n];
          if (im->lv[k])
            lv[n][k].imageView = im->lv[k];
          else if (k)
            lv[n][k].imageView = s->scratch->v;
        }
        w[n].descriptorCount = r->levels, w[n].pImageInfo = lv[n];
      }
    } else {
      ii[n].sampler = r->kind == A2S_RES_SAMPLER ? s->samp[r->metal] : s->samp[16 + r->metal];
      w[n].pImageInfo = &ii[n];
    }
    n++;
  }

  vkUpdateDescriptorSets(dev, n, w, 0, 0);
  return set;
}

static void keep_dpool(void) {
  if (arena.dpool && arena.npool < ARENA_POOLS)
    arena.dpools[arena.npool++] = arena.dpool, arena.dpool = 0;
  if (arena.fb && arena.nfb < ARENA_POOLS)
    arena.fbs[arena.nfb++] = arena.fb, arena.fb = 0;
}

static void new_dpool(void) {
  keep_dpool();
  VkDescriptorPoolSize ps[] = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64 },
                                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },  { VK_DESCRIPTOR_TYPE_SAMPLER, 32 },
                                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 8 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 16 },
                                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 16 } };
  VkDescriptorPoolCreateInfo pci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };

  pci.maxSets = 2, pci.poolSizeCount = 7, pci.pPoolSizes = ps;
  CK(vkCreateDescriptorPool(dev, &pci, 0, &arena.dpool));
}

// spec constants: 0-2 the threadgroup, 8-10 the workgroup, 3-4 tile size, 16 + i bound sampler i
// addresses texels
struct spec {
  VkSpecializationMapEntry e[32];
  uint32_t v[32];
  VkSpecializationInfo info;
};

static void spec_add(struct spec *sp, uint32_t id, uint32_t v) {
  unsigned n = sp->info.mapEntryCount;

  if (n == 32)
    return;
  sp->e[n].constantID = id, sp->e[n].offset = n * 4, sp->e[n].size = 4, sp->v[n] = v;
  sp->info.mapEntryCount = n + 1, sp->info.dataSize = (n + 1) * 4;
}

static VkSpecializationInfo *spec_init(struct spec *sp, const struct a2s_reflect *r, const uint32_t *samp,
                                       const struct PDDrawTex *texs) {
  memset(sp, 0, sizeof(*sp));
  sp->info.pMapEntries = sp->e, sp->info.pData = sp->v;
  for (unsigned i = 0; i < r->nres; i++) {
    const struct a2s_resource *x = &r->res[i];
    if (x->kind == A2S_RES_SAMPLER)
      spec_add(sp, x->spec_id, (uint32_t)sampler_pixel(x->metal < 16 ? samp[x->metal] : 0));
    if (x->kind == A2S_RES_TEXTURE && x->unorm_spec && texs && x->metal < 16 && texs[x->metal].fmt < NFMTS)
      spec_add(sp, A2S_SPEC_UNORM + x->metal, fmts[texs[x->metal].fmt].kind == 'n');
    if (x->kind == A2S_RES_TEXTURE && x->bgra_spec && texs && x->metal < 16)
      spec_add(sp, A2S_SPEC_BGRA + x->metal, (uint32_t)bgra_as_rgba(x, &texs[x->metal]));
  }
  return &sp->info;
}

static void push_words(const struct a2s_reflect *r, uint32_t *pc, const uint64_t grid[3]) {
  for (unsigned k = 0; k < r->npc; k++) {
    pc[k] = r->pc[k].kind == A2S_PC_BUFFER_OFFSET ? 0 : (uint32_t)grid[r->pc[k].kind - A2S_PC_GRID_X];
  }
}

// pipelines keyed by everything that goes into them
struct pipe {
  uint64_t key;
  VkPipeline p;
  VkPipelineLayout pl;
  VkDescriptorSetLayout dl[2];
  VkRenderPass rp;
};

static struct pipe pipes[512];
static unsigned npipes;

static struct pipe *find_pipe(uint64_t key) {
  for (unsigned i = 0; i < npipes; i++) {
    if (pipes[i].key == key) {
      // an entry whose build declined part way keeps its key but no pipeline: binding it would bind a null one
      if (!pipes[i].p)
        decline("pipeline declined on an earlier draw");
      pd_vk_last.pipeline_cached++;
      return &pipes[i];
    }
  }
  return 0;
}

static struct pipe *add_pipe(uint64_t key) {
  if (npipes == sizeof(pipes) / sizeof(pipes[0]))
    decline("pipeline cache full");
  memset(&pipes[npipes], 0, sizeof(pipes[0]));
  pipes[npipes].key = key;
  return &pipes[npipes++];
}

// the workgroup a threadgroup runs as: itself, or for a group without shared state over the device
// limit, a smaller box that divides it
static int split_group(const struct a2s_reflect *r, const uint32_t tpg[3], uint32_t phys[3]) {
  phys[0] = tpg[0], phys[1] = tpg[1], phys[2] = tpg[2];
  while ((uint64_t)phys[0] * phys[1] * phys[2] > lim.max_invocations) {
    unsigned k = phys[0] >= phys[1] && phys[0] >= phys[2] ? 0 : phys[1] >= phys[2] ? 1 : 2;
    // a barrier, shared memory or a tile's imageblock spans the whole threadgroup: split, a workgroup would
    // sync only its slice (on the A733 a split tile kernel locked up the GPU)
    if (r->group_shared || r->barrier || job_is_tile)
      return -1;
    // quads are 2x2 blocks: a split keeps every dimension even
    if (phys[k] & 1 || (r->quad_order && k < 2 && phys[k] & 2)) {
      unsigned j;
      for (j = 0; j < 3 && (phys[j] & 1 || (r->quad_order && j < 2 && phys[j] & 2) || phys[j] == 1); j++) {
      }
      if (j == 3)
        return -1;
      k = j;
    }
    phys[k] /= 2;
  }
  return 0;
}

static int compute_like(uint8_t *base, uint32_t bsize, const uint8_t *mtl, uint32_t msize, const uint32_t tpg[3],
                        const uint32_t groups[3], const uint64_t grid[3], const struct PDDrawBuffer *bufs,
                        const struct PDDrawTex *texs, const uint32_t *samp, const struct PDDrawTex *rt, int tile,
                        uint32_t ib_width, const uint32_t *tgmem, const struct PDTexLayout *layouts) {
  struct shader *sh;
  struct stage_res s;
  struct spec sp;
  struct pipe *pp;
  struct buf wmaps[32];
  const struct PDDrawBuffer *wsrc[32];
  struct img *outs[24];
  uint32_t pc[A2S_MAX_PC], phys[3], disp[3], nout = 0, nw = 0, k;
  uint64_t key, t0;
  VkDescriptorSet set;
  VkCommandBuffer cb;

  if (resident_mode)
    presync_buffers(bufs, PD_CS_BUFS);
  sh = translate(mtl, msize, 0, 0);
  // a grid that leaves an edge group partial: its missing threads would still run here (Vulkan has no
  // partial workgroups) and, past a barrier, cannot simply return
  for (k = 0; k < 3 && !tile; k++) {
    if (sh->refl->barrier && grid[k] % tpg[k])
      decline("partial threadgroup in a kernel with barriers");
  }
  if (split_group(sh->refl, tpg, phys))
    decline("threadgroup %ux%ux%u over the device's %u invocations%s", tpg[0], tpg[1], tpg[2], lim.max_invocations,
            sh->refl->group_shared ? " and shares state" : sh->refl->barrier ? " and has barriers" : job_is_tile ? " in a tile" : "");
  if (sh->refl->npc * 4 > lim.max_push)
    decline("push constants");

  t0 = now_us();
  memset(&s, 0, sizeof(s));
  s.r = sh->refl;
  for (k = 0; tile && k < 4; k++) {
    if (!rt[k].w)
      continue;
    if (resident_mode) {
      struct resident *rr;
      jobimgs[njobimgs] = *res_image(base, bsize, &rt[k], 0, 0, &rr);
      stage_written(&s, rr);
    } else {
      jobimgs[njobimgs] = new_img(base, bsize, &rt[k], 0, 0);
    }
    s.att[k] = &jobimgs[njobimgs++];
  }
  bind_stage(&s, base, bsize, bufs, PD_CS_BUFS, texs, samp, 0, &nw, wmaps, wsrc, layouts);
  pd_vk_last.upload += now_us() - t0;

  t0 = now_us();
  spec_init(&sp, sh->refl, samp, texs);
  for (k = 0; k < 3; k++) {
    spec_add(&sp, k, tpg[k]);
    spec_add(&sp, A2S_SPEC_PHYS_SIZE + k, phys[k]);
    disp[k] = groups[k] * (tpg[k] / phys[k]);
  }
  if (ib_width)
    spec_add(&sp, A2S_SPEC_IMAGEBLOCK, ib_width);
  for (k = 0; k < 4; k++) {
    if (sh->refl->tg_params & (1u << k))
      spec_add(&sp, A2S_SPEC_TGMEM + k, tgmem[k] >= 4 ? (tgmem[k] + 3) / 4 : 1);
  }
  if (tile) {
    spec_add(&sp, 3, PD_TILE_SIZE);
    spec_add(&sp, 4, PD_TILE_SIZE);
  }
  key = fnv((const uint8_t *)sp.v, sp.info.dataSize, sh->key ^ 0x636f6d70ull);
  key = fnv((const uint8_t *)sp.e, sp.info.mapEntryCount * sizeof(sp.e[0]), key);
  if (!(pp = find_pipe(key))) {
    VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkComputePipelineCreateInfo cpi = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    VkPushConstantRange pcr = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sh->refl->npc * 4 };

    pp = add_pipe(key);
    pp->dl[0] = set_layout(sh->refl, VK_SHADER_STAGE_COMPUTE_BIT);
    pli.setLayoutCount = 1, pli.pSetLayouts = pp->dl;
    pli.pushConstantRangeCount = sh->refl->npc ? 1 : 0, pli.pPushConstantRanges = &pcr;
    CK(vkCreatePipelineLayout(dev, &pli, 0, &pp->pl));
    cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpi.stage.module = sh->mod, cpi.stage.pName = "main", cpi.stage.pSpecializationInfo = &sp.info;
    cpi.layout = pp->pl, cpi.flags = lim.cache_control ? VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT : 0;
    VkResult pr = vkCreateComputePipelines(dev, pcache, 1, &cpi, 0, &pp->p);
    if (pr != VK_SUCCESS || !pp->p) {
      pp->key = 0, pp->p = VK_NULL_HANDLE;
      decline("compute pipeline rejected (%d)", (int)pr);
    }
  }
  new_dpool();
  set = fill_set(&s, pp->dl[0]);
  pd_vk_last.pipeline += now_us() - t0;

  t0 = now_us();
  cb = begin();
  job_fence(cb);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pp->p);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pp->pl, 0, 1, &set, 0, 0);
  push_words(sh->refl, pc, grid);
  if (sh->refl->npc)
    vkCmdPushConstants(cb, pp->pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sh->refl->npc * 4, pc);
  vkCmdDispatch(cb, disp[0], disp[1], disp[2]);
  submit(cb);
  job_sync();
  pd_vk_last.run += now_us() - t0;

  // resident: what the job wrote stays on the GPU until someone reads it
  if (resident_mode) {
    for (k = 0; k < s.nwres; k++) {
      res_written(s.wres[k]);
    }
    return 0;
  }

  // what the job wrote, back where the CPU path writes it
  t0 = now_us();
  for (k = 0; k < nw; k++) {
    from_map(base + wsrc[k]->off, wmaps[k].map, wsrc[k]->size);
    pd_vk_last.down_bytes += wsrc[k]->size;
  }
  for (k = 0; k < sh->refl->nres && nout < 24; k++) {
    const struct a2s_resource *r = &sh->refl->res[k];
    if (r->kind == A2S_RES_TEXTURE && r->storage && !r->texel)
      outs[nout++] = s.imgs[r->metal];
    if (r->kind == A2S_RES_ATTACHMENT_IMAGE && s.att[r->metal])
      outs[nout++] = s.att[r->metal];
  }
  for (k = 0; k < nout; k += 8) {
    read_imgs(base, outs + k, nout - k < 8 ? nout - k : 8);
  }
  pd_vk_last.readback += now_us() - t0;
  return 0;
}

static int run(int (*fn)(void *, uint8_t *, uint32_t), void *r, uint8_t *base, uint32_t bsize) {
  int rc;

  memset(&pd_vk_last, 0, sizeof(pd_vk_last));
  reason[0] = 0;
  if (!ready())
    return PD_VK_FALLBACK;
  njobimgs = 0, job_count++, nhostmaps = 0, job_is_tile = 0;
  draw_rect.on = 0;
  if (setjmp(job_fail)) {
    draw_rect.on = 0;
    keep_objects = 0, direct = 0;
    // a batch keeps what it recorded so far, an open render pass is closed
    if (resident_mode) {
      if (in_pass && batch)
        vkCmdEndRenderPass(batch);
      in_pass = 0;
    } else {
      if (jcb)
        vkFreeCommandBuffers(dev, pool, 1, &jcb);
      jcb = 0, jcb_dirty = 0;
      arena_free();
    }
    logv("AIRJITD: vulkan declined: %s", reason);
    return PD_VK_FALLBACK;
  }
  if (resident_mode) {
    // a batch whose arena is filling up runs now
    if (arena.nbuf > ARENA_BUFS - 64 || arena.nview > ARENA_VIEWS - 32 || arena.nbview > ARENA_VIEWS - 32 || arena.npool > ARENA_POOLS - 4 || arena.nimg > 48)
      pd_vk_flush(1);
    return fn(r, base, bsize);
  }
  if (job_batch < 0) {
    const char *e = getenv("AIRJITD_VK_JOBBATCH");
    job_batch = !e || *e != '0';
  }
  rc = fn(r, base, bsize);
  job_sync();
  {
    uint64_t t0 = now_us();
    arena_free();
    pd_vk_last.teardown += now_us() - t0;
  }
  return rc;
}

static int do_compute(void *vr, uint8_t *base, uint32_t bsize) {
  struct PDComputeRequest *q = vr;
  uint32_t tpg[3], groups[3];
  uint64_t grid[3];

  if (q->mOff > bsize || q->mSize > bsize - q->mOff)
    decline("metallib outside the bridge");
  for (int k = 0; k < 3; k++) {
    uint64_t t = q->tpg[k] ? q->tpg[k] : 1, g = q->threads ? (q->grid[k] + t - 1) / t : q->grid[k];
    if (t > 1024 || g > 65535)
      decline("grid %llu x %llu", (unsigned long long)g, (unsigned long long)t);
    tpg[k] = (uint32_t)t, groups[k] = (uint32_t)g;
    grid[k] = q->threads ? q->grid[k] : g * t;
  }
  if (!groups[0] || !groups[1] || !groups[2])
    return 0;
  unknown_pixel = 1;
  return compute_like(base, bsize, base + q->mOff, q->mSize, tpg, groups, grid, q->buf, q->tex, q->samp, 0, 0, q->imageblock[0], q->tgmem,
                      pd_compute_ext(&q->ext) ? q->ext.texLayout : 0);
}

static int do_tile(void *vr, uint8_t *base, uint32_t bsize) {
  struct PDTileRequest *q = vr;

  job_is_tile = 1;
  uint32_t tpg[3], groups[3] = { q->tilesX, q->tilesY, 1 };
  uint64_t grid[3];

  if (q->mOff > bsize || q->mSize > bsize - q->mOff)
    decline("metallib outside the bridge");
  for (int k = 0; k < 3; k++) {
    tpg[k] = q->tpt[k] ? q->tpt[k] : 1;
    grid[k] = (uint64_t)groups[k] * tpg[k];
  }
  unknown_pixel = 1;
  return compute_like(base, bsize, base + q->mOff, q->mSize, tpg, groups, grid, q->buf, q->tex, q->samp, q->rt, 1, 0, q->tgmem,
                      pd_compute_ext(&q->ext) ? q->ext.texLayout : 0);
}

int pd_vk_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t bsize) {
  return run(do_compute, r, base, bsize);
}

int pd_vk_tile(struct PDTileRequest *r, uint8_t *base, uint32_t bsize) {
  return run(do_tile, r, base, bsize);
}

// the MTLBlendFactor and MTLBlendOperation values as the kext packs them (airjitd/blend.h)
static VkBlendFactor blend_factor(unsigned f) {
  static const VkBlendFactor m[] = {
    VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    VK_BLEND_FACTOR_CONSTANT_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
  };
  return f < 15 ? m[f] : VK_BLEND_FACTOR_ONE;
}

static VkBlendOp blend_op(unsigned o) {
  static const VkBlendOp m[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN,
                                 VK_BLEND_OP_MAX };
  return o < 5 ? m[o] : VK_BLEND_OP_ADD;
}

static VkPipelineColorBlendAttachmentState blend_state(uint32_t st, int integer) {
  VkPipelineColorBlendAttachmentState b = { 0 };
  unsigned m = st >> 28;

  b.colorWriteMask = 15;
  if (integer)
    return b;

  // unknown state: premultiplied source over, what the CPU path does
  if (!(st & 1)) {
    b.blendEnable = 1;
    b.srcColorBlendFactor = b.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    b.dstColorBlendFactor = b.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    return b;
  }

  b.colorWriteMask = (m & 8 ? 1 : 0) | (m & 4 ? 2 : 0) | (m & 2 ? 4 : 0) | (m & 1 ? 8 : 0);
  if (st & 2) {
    b.blendEnable = 1;
    b.srcColorBlendFactor = blend_factor((st >> 2) & 31), b.dstColorBlendFactor = blend_factor((st >> 7) & 31);
    b.srcAlphaBlendFactor = blend_factor((st >> 12) & 31), b.dstAlphaBlendFactor = blend_factor((st >> 17) & 31);
    b.colorBlendOp = blend_op((st >> 22) & 7), b.alphaBlendOp = blend_op((st >> 25) & 7);
  }
  return b;
}

static int has_res(const struct a2s_reflect *r, enum a2s_res_kind kind, unsigned k) {
  for (unsigned i = 0; i < r->nres; i++) {
    if (r->res[i].kind == kind && r->res[i].metal == k)
      return 1;
  }
  return 0;
}

// the MTLVertexFormat as Vulkan reads it: integer attributes as unsigned (the shader declares uvec4)
static VkFormat vertex_format(unsigned f) {
  static const VkFormat m[56] = {
    [1] = VK_FORMAT_R8G8_UINT, [2] = VK_FORMAT_R8G8B8_UINT, [3] = VK_FORMAT_R8G8B8A8_UINT, [4] = VK_FORMAT_R8G8_UINT,
    [5] = VK_FORMAT_R8G8B8_UINT, [6] = VK_FORMAT_R8G8B8A8_UINT, [7] = VK_FORMAT_R8G8_UNORM, [8] = VK_FORMAT_R8G8B8_UNORM,
    [9] = VK_FORMAT_R8G8B8A8_UNORM, [10] = VK_FORMAT_R8G8_SNORM, [11] = VK_FORMAT_R8G8B8_SNORM,
    [12] = VK_FORMAT_R8G8B8A8_SNORM, [13] = VK_FORMAT_R16G16_UINT, [14] = VK_FORMAT_R16G16B16_UINT,
    [15] = VK_FORMAT_R16G16B16A16_UINT, [16] = VK_FORMAT_R16G16_UINT, [17] = VK_FORMAT_R16G16B16_UINT,
    [18] = VK_FORMAT_R16G16B16A16_UINT, [19] = VK_FORMAT_R16G16_UNORM, [20] = VK_FORMAT_R16G16B16_UNORM,
    [21] = VK_FORMAT_R16G16B16A16_UNORM, [22] = VK_FORMAT_R16G16_SNORM, [23] = VK_FORMAT_R16G16B16_SNORM,
    [24] = VK_FORMAT_R16G16B16A16_SNORM, [25] = VK_FORMAT_R16G16_SFLOAT, [26] = VK_FORMAT_R16G16B16_SFLOAT,
    [27] = VK_FORMAT_R16G16B16A16_SFLOAT, [28] = VK_FORMAT_R32_SFLOAT, [29] = VK_FORMAT_R32G32_SFLOAT,
    [30] = VK_FORMAT_R32G32B32_SFLOAT, [31] = VK_FORMAT_R32G32B32A32_SFLOAT, [32] = VK_FORMAT_R32_UINT,
    [33] = VK_FORMAT_R32G32_UINT, [34] = VK_FORMAT_R32G32B32_UINT, [35] = VK_FORMAT_R32G32B32A32_UINT,
    [36] = VK_FORMAT_R32_UINT, [37] = VK_FORMAT_R32G32_UINT, [38] = VK_FORMAT_R32G32B32_UINT,
    [39] = VK_FORMAT_R32G32B32A32_UINT, [40] = VK_FORMAT_A2B10G10R10_SNORM_PACK32,
    [41] = VK_FORMAT_A2B10G10R10_UNORM_PACK32, [42] = VK_FORMAT_B8G8R8A8_UNORM, [45] = VK_FORMAT_R8_UINT,
    [46] = VK_FORMAT_R8_UINT, [47] = VK_FORMAT_R8_UNORM, [48] = VK_FORMAT_R8_SNORM, [49] = VK_FORMAT_R16_UINT,
    [50] = VK_FORMAT_R16_UINT, [51] = VK_FORMAT_R16_UNORM, [52] = VK_FORMAT_R16_SNORM, [53] = VK_FORMAT_R16_SFLOAT,
  };
  VkFormatProperties fp;

  if (f >= 56 || !m[f])
    decline("vertex format %u", f);
  vkGetPhysicalDeviceFormatProperties(phys, m[f], &fp);
  if (!(fp.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))
    decline("vertex format %u has no vertex buffer support", f);
  return m[f];
}

// a render pass and pipeline for this draw's attachments, stages and state
static struct pipe *draw_pipe(struct PDDrawRequest *q, const struct PDDrawExt *x, struct shader *vs, struct shader *fs,
                              struct img **atts, unsigned natt, struct img *zimg, struct spec *vsp, struct spec *fsp) {
  static const VkPrimitiveTopology topo[] = { VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                              VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP };
  const struct a2s_reflect *fr = fs->refl, *vr = vs->refl;
  VkPipelineColorBlendAttachmentState cba[4];
  uint32_t keyw[64], nk = 0, k, map[4], n = 0;
  struct pipe *pp;

  // what goes into the key: both shaders, formats, blend, primitive, sampler addressing
  keyw[nk++] = q->prim | (uint32_t)q->indexed << 8;
  for (k = 0; k < natt; k++) {
    uint32_t st = k == 0 ? q->blend : q->rtBlend[k];
    int written = 0;

    // attachments 1-3 with state 0 store unblended (bridge.h), attachment 0 falls back to source over
    cba[k] = blend_state(k && !st ? 0xf0000001u : st, atts[k] && fmts[atts[k]->t.fmt].kind == 'u');
    for (unsigned o = 0; o < fr->nout; o++) {
      written |= fr->out[o].target == k;
    }
    // an attachment the fragment function does not write keeps its contents
    if (!written)
      cba[k].colorWriteMask = 0;
    keyw[nk++] = atts[k] ? atts[k]->t.fmt + 1 : 0;
    keyw[nk++] = (uint32_t)fnv((const uint8_t *)&cba[k], sizeof(cba[k]), 0);
  }
  // the extension's fixed-function state and the attributes the vertex function reads
  if (x) {
    keyw[nk++] = zimg ? zimg->t.fmt + 1 : 0;
    keyw[nk++] = x->depthState, keyw[nk++] = x->stencil[0], keyw[nk++] = x->stencil[1], keyw[nk++] = x->stencilRef;
    keyw[nk++] = x->raster;
    keyw[nk++] = (uint32_t)fnv((const uint8_t *)&x->depthBias, 3 * sizeof(float), 0);
    keyw[nk++] = (uint32_t)fnv((const uint8_t *)x->blendColor, sizeof(x->blendColor), 0);
    for (k = 0; k < vs->refl->nvin && nk < 62; k++) {
      unsigned loc = vs->refl->vin[k].location;
      const struct PDVertexAttr *at = loc < PD_VERTEX_ATTRS ? &x->attr[loc] : &x->attr[0];
      keyw[nk++] = (uint32_t)fnv((const uint8_t *)at, sizeof(*at), loc);
      keyw[nk++] = (uint32_t)fnv((const uint8_t *)&x->layout[at->buffer < PD_VERTEX_ATTRS ? at->buffer : 0], sizeof(x->layout[0]), 0);
    }
  }
  {
    uint64_t key = fnv((const uint8_t *)keyw, nk * 4, vs->key * 31 + fs->key);
    key = fnv((const uint8_t *)vsp->v, vsp->info.dataSize, key);
    key = fnv((const uint8_t *)fsp->v, fsp->info.dataSize, key);
    if ((pp = find_pipe(key)))
      return pp;
    pp = add_pipe(key);
  }

  {
    VkAttachmentDescription ad[5];
    VkAttachmentReference cref[4], iref[4], zref;
    VkSubpassDescription sd = { 0 };
    VkRenderPassCreateInfo rpi = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

    for (k = 0; k < natt; k++) {
      map[k] = atts[k] ? n : VK_ATTACHMENT_UNUSED;
      if (!atts[k])
        continue;
      memset(&ad[n], 0, sizeof(ad[n]));
      ad[n].format = atts[k]->f, ad[n].samples = VK_SAMPLE_COUNT_1_BIT, ad[n].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      ad[n].storeOp = VK_ATTACHMENT_STORE_OP_STORE, ad[n].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      ad[n].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      ad[n].initialLayout = ad[n].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
      n++;
    }
    for (k = 0; k < natt; k++) {
      cref[k].attachment = map[k], cref[k].layout = VK_IMAGE_LAYOUT_GENERAL;
      iref[k].attachment = has_res(fr, A2S_RES_INPUT_ATTACHMENT, k) ? map[k] : VK_ATTACHMENT_UNUSED;
      iref[k].layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    sd.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, sd.colorAttachmentCount = natt, sd.pColorAttachments = cref;
    sd.inputAttachmentCount = natt, sd.pInputAttachments = iref;
    if (zimg) {
      memset(&ad[n], 0, sizeof(ad[n]));
      ad[n].format = zimg->f, ad[n].samples = VK_SAMPLE_COUNT_1_BIT;
      ad[n].loadOp = ad[n].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      ad[n].storeOp = ad[n].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      ad[n].initialLayout = ad[n].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
      zref.attachment = n++, zref.layout = VK_IMAGE_LAYOUT_GENERAL;
      sd.pDepthStencilAttachment = &zref;
    }
    rpi.attachmentCount = n, rpi.pAttachments = ad, rpi.subpassCount = 1, rpi.pSubpasses = &sd;
    CK(vkCreateRenderPass(dev, &rpi, 0, &pp->rp));
  }

  {
    VkPushConstantRange pcr[2];
    VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    unsigned np = 0;

    pp->dl[0] = set_layout(vr, VK_SHADER_STAGE_VERTEX_BIT);
    pp->dl[1] = set_layout(fr, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (vr->npc)
      pcr[np++] = (VkPushConstantRange){ VK_SHADER_STAGE_VERTEX_BIT, 0, vr->npc * 4 };
    if (fr->npc)
      pcr[np++] = (VkPushConstantRange){ VK_SHADER_STAGE_FRAGMENT_BIT, 64, fr->npc * 4 };
    pli.setLayoutCount = 2, pli.pSetLayouts = pp->dl, pli.pushConstantRangeCount = np, pli.pPushConstantRanges = pcr;
    CK(vkCreatePipelineLayout(dev, &pli, 0, &pp->pl));
  }

  {
    VkPipelineShaderStageCreateInfo stg[2];
    VkPipelineVertexInputStateCreateInfo vin = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineRasterizationLineStateCreateInfoKHR ls = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_KHR };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineColorBlendStateCreateInfo cbs = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkPipelineDynamicStateCreateInfo dyn = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo dss = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkVertexInputAttributeDescription va[32];
    VkVertexInputBindingDescription vb[32];
    VkDynamicState ds[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkGraphicsPipelineCreateInfo gpi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    memset(stg, 0, sizeof(stg));
    stg[0].sType = stg[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stg[0].stage = VK_SHADER_STAGE_VERTEX_BIT, stg[0].module = vs->mod, stg[0].pName = "main";
    stg[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT, stg[1].module = fs->mod, stg[1].pName = "main";
    stg[0].pSpecializationInfo = &vsp->info, stg[1].pSpecializationInfo = &fsp->info;
    ia.topology = q->prim < 5 ? topo[q->prim] : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = q->indexed && q->prim == PD_PRIM_TRIANGLE_STRIP;
    vps.viewportCount = 1, vps.scissorCount = 1;
    rs.polygonMode = VK_POLYGON_MODE_FILL, rs.cullMode = VK_CULL_MODE_NONE, rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1;
    if (x && (x->raster & PD_RASTER_SET)) {
      static const VkCullModeFlags cull[4] = { VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE };
      // the viewport's negative height keeps Metal's clockwise front faces clockwise here
      rs.cullMode = cull[x->raster & 3];
      rs.frontFace = x->raster & PD_RASTER_CCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
      if (x->raster & PD_RASTER_LINES)
        decline("line fill mode");
      if (x->raster & PD_RASTER_CLAMP) {
        if (!lim.depth_clamp)
          decline("depth clamp");
        rs.depthClampEnable = 1;
      }
    }
    if (x && zimg && (x->depthBias != 0 || x->depthSlope != 0)) {
      if (x->depthBiasClamp != 0 && !lim.bias_clamp)
        decline("depth bias clamp");
      rs.depthBiasEnable = 1, rs.depthBiasConstantFactor = x->depthBias, rs.depthBiasSlopeFactor = x->depthSlope;
      rs.depthBiasClamp = x->depthBiasClamp;
    }
    // the MTLCompareFunction and MTLStencilOperation numbers match VkCompareOp and VkStencilOp
    if (x && zimg) {
      VkStencilOpState *so[2] = { &dss.front, &dss.back };
      dss.depthTestEnable = (zimg->aspect & VK_IMAGE_ASPECT_DEPTH_BIT) && (x->depthState & PD_DEPTH_SET);
      dss.depthWriteEnable = dss.depthTestEnable && (x->depthState & PD_DEPTH_WRITE);
      dss.depthCompareOp = (VkCompareOp)(x->depthState & 7);
      dss.stencilTestEnable = (zimg->aspect & VK_IMAGE_ASPECT_STENCIL_BIT) && ((x->stencil[0] | x->stencil[1]) & PD_STENCIL_ON);
      for (k = 0; k < 2; k++) {
        uint32_t s = x->stencil[k];
        so[k]->compareOp = (VkCompareOp)(s & 7), so[k]->failOp = (VkStencilOp)(s >> 3 & 7);
        so[k]->depthFailOp = (VkStencilOp)(s >> 6 & 7), so[k]->passOp = (VkStencilOp)(s >> 9 & 7);
        so[k]->compareMask = s >> 12 & 0xff, so[k]->writeMask = s >> 20 & 0xff;
        so[k]->reference = x->stencilRef >> (8 * k) & 0xff;
      }
      dss.maxDepthBounds = 1;
      gpi.pDepthStencilState = &dss;
    }
    // stage_in attributes: one binding per vertex buffer they read, a step of 2 is per instance
    if (vs->refl->nvin) {
      unsigned nb = 0, b;

      if (!x)
        decline("stage_in vertex attributes without a vertex descriptor");
      for (k = 0; k < vs->refl->nvin && k < 32; k++) {
        unsigned loc = vs->refl->vin[k].location;
        const struct PDVertexAttr *at;
        const struct PDVertexLayout *ly;

        if (loc >= PD_VERTEX_ATTRS || !(at = &x->attr[loc])->format)
          decline("vertex attribute %u not described", loc);
        if (at->buffer >= PD_DRAW_BUFS || !q->vbuf[at->buffer].size)
          decline("vertex attribute %u reads unbound buffer %u", loc, at->buffer);
        ly = &x->layout[at->buffer];
        if (ly->step == 2 && ly->rate > 1)
          decline("per instance step rate %u", ly->rate);
        va[k].location = loc, va[k].binding = at->buffer, va[k].format = vertex_format(at->format), va[k].offset = at->offset;
        for (b = 0; b < nb && vb[b].binding != at->buffer; b++) {
        }
        if (b == nb) {
          vb[nb].binding = at->buffer, vb[nb].stride = ly->step == 0 ? 0 : ly->stride;
          vb[nb++].inputRate = ly->step == 2 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        }
      }
      vin.vertexAttributeDescriptionCount = vs->refl->nvin, vin.pVertexAttributeDescriptions = va;
      vin.vertexBindingDescriptionCount = nb, vin.pVertexBindingDescriptions = vb;
    }
    if (x)
      memcpy(cbs.blendConstants, x->blendColor, sizeof(cbs.blendConstants));
    if (q->prim == PD_PRIM_LINES || q->prim == PD_PRIM_LINE_STRIP) {
      ls.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR;
      rs.pNext = &ls;
    }
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    cbs.attachmentCount = natt, cbs.pAttachments = cba;
    dyn.dynamicStateCount = 2, dyn.pDynamicStates = ds;
    gpi.stageCount = 2, gpi.pStages = stg, gpi.pVertexInputState = &vin, gpi.pInputAssemblyState = &ia;
    gpi.pViewportState = &vps, gpi.pRasterizationState = &rs, gpi.pMultisampleState = &ms;
    gpi.pColorBlendState = &cbs, gpi.pDynamicState = &dyn, gpi.layout = pp->pl, gpi.renderPass = pp->rp;
    gpi.flags = lim.cache_control ? VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT : 0;
    VkResult pr = vkCreateGraphicsPipelines(dev, pcache, 1, &gpi, 0, &pp->p);
    if (pr != VK_SUCCESS || !pp->p) {
      pp->key = 0, pp->p = VK_NULL_HANDLE;
      decline("graphics pipeline rejected (%d)", (int)pr);
    }
  }

  return pp;
}

// a texture the GPU wrote under another shape (a colour attachment, sampled with all its levels) comes back to the bridge before the job allocates anything,
// since syncing from inside the binding would flush the batch and free the job's own buffers and views
static void presync_textures(const struct PDDrawTex *texs, const struct PDTexLayout *layouts, unsigned n) {
  for (unsigned i = 0; i < n; i++) {
    const struct PDDrawTex *t = &texs[i];
    const struct PDTexLayout *l = layouts && (layouts[i].flags & PD_TEXLAYOUT_SET) ? &layouts[i] : 0;
    VkBufferImageCopy cp[96];
    uint64_t span;

    if (!t->w || t->fmt >= NFMTS)
      continue;
    tex_regions(t, l, t->fmt, cp, 96, &span);
    for (unsigned k = 0; k < MAX_RESIDENT; k++) {
      struct resident *s = &residents[k];

      if (!s->gpu_newer || !overlaps(s, t->off, (uint32_t)span))
        continue;
      if (s->kind == RES_IMG && s->off == t->off && (s->key == layout_key(t, l, 0) || s->key == layout_key(t, l, 1)))
        continue;
      // back in the bridge, and every resident copy of these bytes uploads them again when next used
      sync_range(t->off, (uint32_t)span);
      pd_vk_dirty(t->off, (uint32_t)span);
      break;
    }
  }
}

static int do_draw(void *vr, uint8_t *base, uint32_t bsize) {
  struct PDDrawRequest *q = vr;
  struct shader *vs, *fs;
  struct stage_res vsr, fsr;
  struct spec vsp, fsp;
  struct buf wmaps[32];
  const struct PDDrawBuffer *wsrc[32];
  struct PDDrawTex att[4];
  struct img *atts[4] = { 0 }, *outs[4], *zimg = 0;
  struct resident *attres[4] = { 0 }, *zres = 0;
  unsigned att_slot[4] = { ATTPOOL, ATTPOOL, ATTPOOL, ATTPOOL };
  const struct PDDrawExt *x = pd_draw_ext(q);
  uint32_t vpc[A2S_MAX_PC], fpc[A2S_MAX_PC], *fw, nw = 0, natt = 0, nout = 0, k;
  uint64_t nogrid[3] = { 0, 0, 0 }, t0;
  VkImageView views[5];
  VkFramebuffer fb;
  unsigned pooled = 0;
  VkDescriptorSet sets[2];
  VkCommandBuffer cb;
  struct pipe *pp;
  struct a2s_reflect *fr;

  if (q->vmOff > bsize || q->vmSize > bsize - q->vmOff || q->fmOff > bsize || q->fmSize > bsize - q->fmOff)
    decline("metallib outside the bridge");
  if (q->prim > PD_PRIM_TRIANGLE_STRIP || !q->count)
    decline("primitive %u count %u", q->prim, q->count);
  if ((q->prim == PD_PRIM_LINES || q->prim == PD_PRIM_LINE_STRIP) && !lim.bresenham)
    decline("lines without diamond exit rasterization");
  unknown_pixel = 0;
  if (resident_mode) {
    presync_buffers(q->vbuf, PD_DRAW_BUFS);
    presync_buffers(q->fbuf, PD_DRAW_BUFS);
    presync_textures(q->ftex, x ? x->ftexLayout : 0, PD_DRAW_TEXS);
    if (x)
      presync_textures(x->vtex, x->vtexLayout, PD_DRAW_TEXS);
  }

  logv("AIRJITD: vulkan draw %u/%u: translate", q->vfn, q->ffn);
  vs = translate(base + q->vmOff, q->vmSize, 0, 0);
  {
    struct a2s_pin pins[PD_DRAW_BUFS];
    unsigned np = 0;

    for (unsigned i = 0; i < PD_DRAW_BUFS; i++) {
      if (!q->fbuf[i].size || q->fbuf[i].off > bsize || q->fbuf[i].size > bsize - q->fbuf[i].off)
        continue;
      pins[np].metal = i, pins[np].data = base + q->fbuf[i].off;
      pins[np].size = q->fbuf[i].size < PIN_MAX_BYTES ? q->fbuf[i].size : PIN_MAX_BYTES;
      np++;
    }
    fs = translate_pinned(base + q->fmOff, q->fmSize, 1, 64, pins, np);
  }
  logv("AIRJITD: vulkan draw %u/%u: translated", q->vfn, q->ffn);
  if (vs->refl->stage != A2S_VERTEX || fs->refl->stage != A2S_FRAGMENT)
    decline("stages are not a vertex and a fragment function");

  // fragment inputs take the locations of the vertex outputs with the same names: a copy of the
  // fragment module per vertex function
  {
    uint64_t key = fs->key * 1099511628211ull ^ vs->key;
    struct shader *lf = 0;

    for (k = 0; k < nshaders; k++) {
      if (shaders[k].key == key)
        lf = shader_use(&shaders[k]);
    }
    if (!lf) {
      VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

      lf = shader_slot();
      fw = malloc(fs->nwords * 4);
      memcpy(fw, fs->words, fs->nwords * 4);
      fr = malloc(sizeof(*fr));
      *fr = *fs->refl;
      if (a2s_relocate(fw, fr, vs->refl) != fr->nvar) {
        free(fw), free(fr);
        decline("a fragment input has no vertex output");
      }
      ci.codeSize = fs->nwords * 4, ci.pCode = fw;
      lf->words = fw, lf->nwords = fs->nwords, lf->refl = fr, lf->key = key;
      if (vkCreateShaderModule(dev, &ci, 0, &lf->mod) != VK_SUCCESS) {
        lf->words = 0, lf->refl = 0, lf->key = 0;
        free(fw), free(fr);
        decline("shader module rejected");
      }
      if (lf == &shaders[nshaders])
        nshaders++;
      shader_use(lf);
    }
    fs = lf;
  }

  for (k = 0; k < fs->refl->nout; k++) {
    if (fs->refl->out[k].target >= 4)
      decline("render target %u", fs->refl->out[k].target);
  }

  t0 = now_us();
  att[0] = (struct PDDrawTex){ 0, q->targetOff, q->tw, q->th, q->tbpr, q->tfmt };
  if (x && (x->raster & PD_RASTER_SET) && (x->raster & PD_RASTER_NOCOLOR))
    att[0].w = 0;
  for (k = 1; k < 4; k++) {
    att[k] = q->rt[k];
  }
  // the pixels this draw can touch: its viewport, cut by the scissor and the target
  {
    double x0 = q->viewport[0], y0 = q->viewport[1], x1 = x0 + q->viewport[2], y1 = y0 + q->viewport[3];
    uint32_t rx0, ry0, rx1, ry1;

    if (x && (x->raster & (PD_RASTER_SET | PD_RASTER_SCISSOR)) == (PD_RASTER_SET | PD_RASTER_SCISSOR)) {
      x0 = x0 > x->scissor[0] ? x0 : x->scissor[0], y0 = y0 > x->scissor[1] ? y0 : x->scissor[1];
      x1 = x1 < (double)x->scissor[0] + x->scissor[2] ? x1 : (double)x->scissor[0] + x->scissor[2];
      y1 = y1 < (double)x->scissor[1] + x->scissor[3] ? y1 : (double)x->scissor[1] + x->scissor[3];
    }
    // truncation floors here, the edges are not negative
    rx0 = x0 <= 0 ? 0 : x0 >= q->tw ? q->tw : (uint32_t)x0;
    ry0 = y0 <= 0 ? 0 : y0 >= q->th ? q->th : (uint32_t)y0;
    rx1 = x1 <= rx0 ? rx0 : x1 >= q->tw ? q->tw : (uint32_t)x1 + (x1 > (uint32_t)x1);
    ry1 = y1 <= ry0 ? ry0 : y1 >= q->th ? q->th : (uint32_t)y1 + (y1 > (uint32_t)y1);
    // nothing to touch still copies one pixel, a zero extent is not a valid copy
    if (rx1 == rx0)
      rx0 = rx0 ? rx0 - 1 : 0, rx1 = rx0 + 1;
    if (ry1 == ry0)
      ry0 = ry0 ? ry0 - 1 : 0, ry1 = ry0 + 1;
    if (fullrt < 0) {
      const char *e = getenv("AIRJITD_VK_FULLRT");
      fullrt = e && *e != '0';
    }
    draw_rect.x = rx0, draw_rect.y = ry0, draw_rect.w = rx1 - rx0, draw_rect.h = ry1 - ry0;
    draw_rect.tw = q->tw, draw_rect.th = q->th;
    // AIRJITD_VK_FULLRT=1 moves whole targets as before
    if (fullrt || !q->tw || !q->th)
      draw_rect.w = draw_rect.h = 0;
    draw_rect.on = draw_rect.w != 0;
    // every 1024 draws: how many touch less than their whole target, and the share of pixels
    {
      static uint64_t ndraws, npartial, px_full, px_rect;

      ndraws++, px_full += (uint64_t)q->tw * q->th;
      px_rect += draw_rect.w ? (uint64_t)draw_rect.w * draw_rect.h : (uint64_t)q->tw * q->th;
      npartial += draw_rect.w && (uint64_t)draw_rect.w * draw_rect.h < (uint64_t)q->tw * q->th;
      if (ndraws % 1024 == 0)
        logmsg("AIRJITD: draw rects: %llu of %llu draws partial, %llu%% of target pixels moved, %llu uploads skipped",
               (unsigned long long)npartial, (unsigned long long)ndraws,
               (unsigned long long)(px_full ? px_rect * 100 / px_full : 100), (unsigned long long)upload_skips);
      if (ndraws % 1024 == 0)
        logmsg("AIRJITD: upload profile ms/MB: hash %llu/%llu, image copies %llu/%llu, buffer copies %llu/%llu, "
               "readback copies %llu/%llu, %llu new staging buffers %llu ms; attachments %llu ms, binding %llu ms, "
               "%llu new images %llu ms",
               (unsigned long long)prof.hash_us / 1000, (unsigned long long)prof.hash_b >> 20,
               (unsigned long long)prof.img_us / 1000, (unsigned long long)prof.img_b >> 20,
               (unsigned long long)prof.buf_us / 1000, (unsigned long long)prof.buf_b >> 20,
               (unsigned long long)prof.back_us / 1000, (unsigned long long)prof.back_b >> 20,
               (unsigned long long)prof.newbuf_n, (unsigned long long)prof.newbuf_us / 1000,
               (unsigned long long)prof.att_us / 1000, (unsigned long long)prof.bind_us / 1000,
               (unsigned long long)prof.newimg_n, (unsigned long long)prof.newimg_us / 1000);
      // its own line: the bridge log truncates at 512 bytes
      if (ndraws % 1024 == 0)
        logmsg("AIRJITD: attachment pool: %llu hits %llu new %llu evicted, evictions %llu ms (framebuffers %llu ms); "
               "%llu new framebuffers %llu ms",
               (unsigned long long)prof.pool_hit, (unsigned long long)prof.pool_new, (unsigned long long)prof.pool_evict,
               (unsigned long long)prof.evict_us / 1000, (unsigned long long)prof.evict_fb_us / 1000,
               (unsigned long long)prof.fb_new, (unsigned long long)prof.fb_new_us / 1000);
    }
  }
  uint64_t ta = now_us();
  for (k = 0; k < 4; k++) {
    if (!att[k].w)
      continue;
    if (att[k].fmt < NFMTS && fmts[att[k].fmt].kind == 'c')
      decline("compressed format %u as an attachment", att[k].fmt);
    if (ext_att.image && att[k].off == ext_att.off) {
      // the presenter's image: drawn in place, its contents are already there
      if (att[k].fmt >= NFMTS)
        decline("texture format %u", att[k].fmt);
      ext_att.written = 0;
      jobimgs[njobimgs] = (struct img){ ext_att.image, ext_att.view, fmts[att[k].fmt].vk, att[k] };
    } else if (resident_mode) {
      jobimgs[njobimgs] = *res_image(base, bsize, &att[k], 0, 0, &attres[k]);
    } else {
      jobimgs[njobimgs] = attachment_img(base, bsize, &att[k]);
      att_slot[k] = attpool_last;
      pooled++;
    }
    atts[k] = &jobimgs[njobimgs++];
    natt = k + 1;
  }
  draw_rect.on = 0;
  prof.att_us += now_us() - ta;
  for (k = 0; k < fs->refl->nout; k++) {
    struct img *a = atts[fs->refl->out[k].target];
    // an integer output into a float attachment (or back) is undefined in Vulkan: the CPU decides
    if (a && (fmts[a->t.fmt].kind == 'u') != (fs->refl->out[k].elem == 'u'))
      decline("attachment %u format %u against a %s output", fs->refl->out[k].target, a->t.fmt,
              fs->refl->out[k].elem == 'u' ? "integer" : "float");
  }

  if (x && x->depth.w) {
    jobimgs[njobimgs] = resident_mode ? *res_depth(base, bsize, &x->depth, &zres) : new_depth(base, bsize, &x->depth);
    zimg = &jobimgs[njobimgs++];
  }

  memset(&vsr, 0, sizeof(vsr)), memset(&fsr, 0, sizeof(fsr));
  vsr.r = vs->refl, fsr.r = fs->refl;
  // the vertex stage has its own textures and samplers in the extension
  ta = now_us();
  bind_stage(&vsr, base, bsize, q->vbuf, PD_DRAW_BUFS, x ? x->vtex : q->ftex, x ? x->vsamp : q->samp, 0, &nw, wmaps, wsrc,
             x ? x->vtexLayout : 0);
  bind_stage(&fsr, base, bsize, q->fbuf, PD_DRAW_BUFS, q->ftex, q->samp, 0, &nw, wmaps, wsrc, x ? x->ftexLayout : 0);
  prof.bind_us += now_us() - ta;
  for (k = 0; k < 4; k++) {
    fsr.att[k] = atts[k];
  }
  pd_vk_last.upload += now_us() - t0;

  t0 = now_us();
  spec_init(&vsp, vs->refl, x ? x->vsamp : q->samp, x ? x->vtex : q->ftex);
  spec_init(&fsp, fs->refl, q->samp, q->ftex);
  logv("AIRJITD: vulkan draw %u/%u: create pipeline", q->vfn, q->ffn);
  pp = draw_pipe(q, x, vs, fs, atts, natt, zimg, &vsp, &fsp);
  logv("AIRJITD: vulkan draw %u/%u: pipeline created", q->vfn, q->ffn);
  new_dpool();
  sets[0] = fill_set(&vsr, pp->dl[0]);
  sets[1] = fill_set(&fsr, pp->dl[1]);
  {
    VkFramebufferCreateInfo fbi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    unsigned n = 0;
    for (k = 0; k < natt; k++) {
      if (atts[k])
        views[n++] = atts[k]->v;
    }
    if (zimg)
      views[n++] = zimg->v;
    // only when every view is a pooled one
    if (attpool_on > 0 && !zimg && pooled == n && n <= 4) {
      fb = cached_fb(pp->rp, views, n, q->tw, q->th);
    } else {
      fbi.renderPass = pp->rp, fbi.attachmentCount = n, fbi.pAttachments = views;
      fbi.width = q->tw, fbi.height = q->th, fbi.layers = 1;
      CK(vkCreateFramebuffer(dev, &fbi, 0, &arena.fb));
      fb = arena.fb;
    }
  }
  pd_vk_last.pipeline += now_us() - t0;
  logv("AIRJITD: vulkan draw %u/%u: pipeline ready", q->vfn, q->ffn);

  t0 = now_us();
  {
    VkRenderPassBeginInfo rbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    VkViewport vp;
    VkRect2D sc = { { 0, 0 }, { q->tw, q->th } }, ra = sc;
    PFN_vkCmdSetViewport setvp = (PFN_vkCmdSetViewport)vkGetDeviceProcAddr(dev, "vkCmdSetViewport");
    PFN_vkCmdSetScissor setsc = (PFN_vkCmdSetScissor)vkGetDeviceProcAddr(dev, "vkCmdSetScissor");

    // metal's y points down: a viewport of negative height lands the same pixels
    vp.x = (float)q->viewport[0], vp.width = (float)q->viewport[2];
    vp.y = (float)(q->viewport[1] + q->viewport[3]), vp.height = -(float)q->viewport[3];
    vp.minDepth = (float)q->viewport[4], vp.maxDepth = (float)q->viewport[5];
    if (x && (x->raster & (PD_RASTER_SET | PD_RASTER_SCISSOR)) == (PD_RASTER_SET | PD_RASTER_SCISSOR)) {
      uint32_t x0 = x->scissor[0] < q->tw ? x->scissor[0] : q->tw, y0 = x->scissor[1] < q->th ? x->scissor[1] : q->th;
      sc.offset.x = (int32_t)x0, sc.offset.y = (int32_t)y0;
      sc.extent.width = x->scissor[2] < q->tw - x0 ? x->scissor[2] : q->tw - x0;
      sc.extent.height = x->scissor[3] < q->th - y0 ? x->scissor[3] : q->th - y0;
    }
    // only the moved pixels are valid in the attachments: keep every write inside them
    if (draw_rect.w) {
      int32_t ex = sc.offset.x + (int32_t)sc.extent.width, ey = sc.offset.y + (int32_t)sc.extent.height;
      int32_t rx = (int32_t)(draw_rect.x + draw_rect.w), ry = (int32_t)(draw_rect.y + draw_rect.h);

      sc.offset.x = sc.offset.x > (int32_t)draw_rect.x ? sc.offset.x : (int32_t)draw_rect.x;
      sc.offset.y = sc.offset.y > (int32_t)draw_rect.y ? sc.offset.y : (int32_t)draw_rect.y;
      ex = ex < rx ? ex : rx, ey = ey < ry ? ey : ry;
      sc.extent.width = ex > sc.offset.x ? (uint32_t)(ex - sc.offset.x) : 0;
      sc.extent.height = ey > sc.offset.y ? (uint32_t)(ey - sc.offset.y) : 0;
    }
    cb = begin();
    job_fence(cb);
    rbi.renderPass = pp->rp, rbi.framebuffer = fb, rbi.renderArea = ra;
    vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    in_pass = resident_mode;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pp->p);
    setvp(cb, 0, 1, &vp);
    setsc(cb, 0, 1, &sc);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pp->pl, 0, 2, sets, 0, 0);
    push_words(vs->refl, vpc, nogrid);
    push_words(fs->refl, fpc, nogrid);
    if (vs->refl->npc)
      vkCmdPushConstants(cb, pp->pl, VK_SHADER_STAGE_VERTEX_BIT, 0, vs->refl->npc * 4, vpc);
    if (fs->refl->npc)
      vkCmdPushConstants(cb, pp->pl, VK_SHADER_STAGE_FRAGMENT_BIT, 64, fs->refl->npc * 4, fpc);
    // the vertex buffers stage_in attributes read
    for (k = 0; x && k < vs->refl->nvin; k++) {
      unsigned b = x->attr[vs->refl->vin[k].location].buffer;
      const struct PDDrawBuffer *db = &q->vbuf[b];
      VkDeviceSize zero = 0;
      struct buf vbuf;

      if (db->off > bsize || db->size > bsize - db->off)
        decline("vertex buffer %u outside the bridge", b);
      if (resident_mode) {
        vbuf.b = res_buffer(base, db->off, db->size, 0, 0);
      } else {
        vbuf = new_buf(db->size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        to_map(vbuf.map, base + db->off, db->size);
        pd_vk_last.up_bytes += db->size;
      }
      vkCmdBindVertexBuffers(cb, b, 1, &vbuf.b, &zero);
    }
    if ((x && (x->baseInstance || x->baseVertex || x->firstVertex)) && !lim.draw_params &&
        (vs->refl->needs[0] && strstr(vs->refl->needs, "DrawParameters")))
      decline("base vertex or instance in the shader");
    if (q->indexed) {
      unsigned ib_bytes = x && q->indexBytes == 4 ? 4 : 2;
      struct buf ib;
      if (q->indexOff > bsize || (uint64_t)q->count * ib_bytes > bsize - q->indexOff)
        decline("indices outside the bridge");
      if (resident_mode) {
        ib.b = res_buffer(base, q->indexOff, q->count * ib_bytes, 0, 0);
      } else {
        ib = new_buf((VkDeviceSize)q->count * ib_bytes + 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        to_map(ib.map, base + q->indexOff, (size_t)q->count * ib_bytes);
      }
      vkCmdBindIndexBuffer(cb, ib.b, 0, ib_bytes == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
      logv("AIRJITD:   indexed draw: %u indices of %u bytes at 0x%x, %u instances, base vertex %d, base instance %u", q->count,
           ib_bytes, q->indexOff, q->instances, x ? x->baseVertex : 0, x ? x->baseInstance : 0);
      vkCmdDrawIndexed(cb, q->count, q->instances ? q->instances : 1, 0, x ? x->baseVertex : 0, x ? x->baseInstance : 0);
    } else {
      vkCmdDraw(cb, q->count, q->instances ? q->instances : 1, x ? x->firstVertex : 0, x ? x->baseInstance : 0);
    }
    vkCmdEndRenderPass(cb);
    in_pass = 0;
    submit(cb);
  }
  pd_vk_last.run += now_us() - t0;

  // resident: the attachments, depth and written resources stay on the GPU until someone reads them
  if (resident_mode) {
    for (k = 0; k < 4; k++) {
      int written = 0;
      for (unsigned o = 0; o < fs->refl->nout; o++) {
        written |= fs->refl->out[o].target == k;
      }
      if (attres[k] && written)
        res_written(attres[k]);
    }
    if (zres && (x->depthState & PD_DEPTH_WRITE || (x->stencil[0] | x->stencil[1]) & PD_STENCIL_ON))
      res_written(zres);
    for (k = 0; k < vsr.nwres; k++) {
      res_written(vsr.wres[k]);
    }
    for (k = 0; k < fsr.nwres; k++) {
      res_written(fsr.wres[k]);
    }
    return 0;
  }

  t0 = now_us();
  for (k = 0; k < 4; k++) {
    if (atts[k] && !(ext_att.image && atts[k]->i == ext_att.image))
      outs[nout++] = atts[k];
  }
  draw_rect.on = draw_rect.w != 0;
  read_imgs(base, outs, nout);
  draw_rect.on = 0;
  // a whole-target readback leaves the pooled image equal to the bridge: remember what it holds
  for (k = 0; k < 4; k++) {
    int partial = draw_rect.w && att[k].w == draw_rect.tw && att[k].h == draw_rect.th &&
                  (uint64_t)draw_rect.w * draw_rect.h < (uint64_t)att[k].w * att[k].h;

    if (uploadskip_on > 0 && att_slot[k] < ATTPOOL && !partial) {
      uint64_t span = (uint64_t)att[k].bpr * att[k].h;

      attpool[att_slot[k]].clean = 1, attpool[att_slot[k]].clean_off = att[k].off;
      attpool[att_slot[k]].clean_hash = content_hash(base + att[k].off, span);
    }
  }
  if (zimg && x && (x->depthState & PD_DEPTH_WRITE || (x->stencil[0] | x->stencil[1]) & PD_STENCIL_ON))
    read_depth(base, zimg);
  pd_vk_last.readback += now_us() - t0;
  return 0;
}

int pd_vk_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize) {
  return run(do_draw, r, base, bsize);
}
