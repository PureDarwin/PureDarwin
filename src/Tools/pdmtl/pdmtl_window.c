#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "pdmtl_window.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include "opengl/gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vulkan/vk.h"

#define VK_FUNCS(X)                                                                                                   \
  X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)                              \
  X(vkGetPhysicalDeviceSurfaceFormatsKHR) X(vkGetPhysicalDeviceMemoryProperties) X(vkGetDeviceProcAddr)              \
  X(vkDestroySurfaceKHR)

#define VK_DEV_FUNCS(X)                                                                                               \
  X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR)               \
  X(vkQueuePresentKHR) X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateFence) X(vkWaitForFences)                \
  X(vkResetFences) X(vkCreateCommandPool) X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer)  \
  X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkResetCommandBuffer) X(vkCmdPipelineBarrier) X(vkCmdCopyBufferToImage)       \
  X(vkCmdClearColorImage) X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory)  \
  X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkDeviceWaitIdle) X(vkCreateSemaphore)

#define DECL(f) static PFN_##f f;
VK_FUNCS(DECL)
VK_DEV_FUNCS(DECL)

#define MAX_IMAGES 8
#define MAX_EVENTS 64

struct pdmtl_window {
  // null for a surface someone else made (pdmtl_surface_attach): its size is req_w x req_h
  GLFWwindow *glfw;
  int gl, own_glfw;
  void *frame_context;
  pdmtl_frame_present frame_present;
  pdmtl_frame_acquire frame_acquire;
  pdmtl_frame_commit frame_commit;
  pdmtl_frame_cancel frame_cancel;
  int zero, own_surface, req_w, req_h, srgb;
  VkInstance inst;
  VkPhysicalDevice phys;
  VkDevice dev;
  VkQueue queue;
  uint32_t qfam;
  VkSurfaceKHR surface;
  VkSwapchainKHR swap;
  VkImage images[MAX_IMAGES];
  VkImageView views[MAX_IMAGES];
  uint32_t nimages, w, h;
  int stale;
  VkCommandPool pool;
  VkCommandBuffer cb;
  VkFence fence;
  // the command buffer's own fence (reused once it signalled), and the semaphore a present waits on
  VkFence cb_fence;
  int cb_busy;
  VkSemaphore rendered;
  VkBuffer staging;
  VkDeviceMemory staging_mem;
  void *staging_map;
  struct pdmtl_drawable drawable;
  struct pdmtl_event ev[MAX_EVENTS];
  unsigned ev_head, ev_tail;
};

static void push(struct pdmtl_window *w, struct pdmtl_event e) {
  unsigned next = (w->ev_tail + 1) % MAX_EVENTS;

  // a full queue drops the newest event
  if (next == w->ev_head)
    return;
  w->ev[w->ev_tail] = e;
  w->ev_tail = next;
}

static void on_key(GLFWwindow *g, int key, int scancode, int action, int mods) {
  (void)scancode, (void)mods;
  push(glfwGetWindowUserPointer(g), (struct pdmtl_event){ .type = PDMTL_EV_KEY, .key = key, .action = action });
}

static void on_move(GLFWwindow *g, double x, double y) {
  push(glfwGetWindowUserPointer(g), (struct pdmtl_event){ .type = PDMTL_EV_MOUSE_MOVE, .x = x, .y = y });
}

static void on_button(GLFWwindow *g, int button, int action, int mods) {
  double x, y;

  (void)mods;
  glfwGetCursorPos(g, &x, &y);
  push(glfwGetWindowUserPointer(g),
       (struct pdmtl_event){ .type = PDMTL_EV_MOUSE_BUTTON, .button = button, .action = action, .x = x, .y = y });
}

static void on_resize(GLFWwindow *g, int w, int h) {
  struct pdmtl_window *win = glfwGetWindowUserPointer(g);

  win->stale = 1;
  push(win, (struct pdmtl_event){ .type = PDMTL_EV_RESIZE, .w = w, .h = h });
}

static void on_close(GLFWwindow *g) {
  push(glfwGetWindowUserPointer(g), (struct pdmtl_event){ .type = PDMTL_EV_CLOSE });
}

static uint32_t mem_type(struct pdmtl_window *w, uint32_t bits, VkMemoryPropertyFlags want) {
  VkPhysicalDeviceMemoryProperties mp;

  vkGetPhysicalDeviceMemoryProperties(w->phys, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    if ((bits & 1u << i) && (mp.memoryTypes[i].propertyFlags & want) == want)
      return i;
  }
  return 0;
}

static void barrier(VkCommandBuffer cb, VkImage im, VkImageLayout from, VkImageLayout to) {
  VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

  b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  b.oldLayout = from, b.newLayout = to, b.image = im;
  b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 0, 0, 1, &b);
}

static void begin(struct pdmtl_window *w) {
  VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

  if (w->cb_busy) {
    vkWaitForFences(w->dev, 1, &w->cb_fence, 1, UINT64_MAX);
    vkResetFences(w->dev, 1, &w->cb_fence);
    w->cb_busy = 0;
  }
  vkResetCommandBuffer(w->cb, 0);
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(w->cb, &bi);
}

// on the executor's queue: its barriers order these after everything the executor submitted before. Nothing
// waits here, signal sets the semaphore a present waits on
static void submit_signal(struct pdmtl_window *w, int signal) {
  VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };

  vkEndCommandBuffer(w->cb);
  si.commandBufferCount = 1, si.pCommandBuffers = &w->cb;
  if (signal)
    si.signalSemaphoreCount = 1, si.pSignalSemaphores = &w->rendered;
  vkQueueSubmit(w->queue, 1, &si, w->cb_fence);
  w->cb_busy = 1;
}

static void submit(struct pdmtl_window *w) {
  submit_signal(w, 0);
}

static void destroy_swapchain(struct pdmtl_window *w) {
  if (!w->swap)
    return;
  vkDeviceWaitIdle(w->dev);
  for (uint32_t i = 0; i < w->nimages; i++) {
    vkDestroyImageView(w->dev, w->views[i], 0);
  }
  vkDestroySwapchainKHR(w->dev, w->swap, 0);
  if (w->staging) {
    vkDestroyBuffer(w->dev, w->staging, 0);
    vkFreeMemory(w->dev, w->staging_mem, 0);
  }
  w->swap = 0, w->staging = 0, w->nimages = 0;
}

static int make_swapchain(struct pdmtl_window *w) {
  VkSurfaceCapabilitiesKHR caps;
  VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  VkSurfaceFormatKHR fmts[64];
  VkFormat vkfmt;
  uint32_t nf = 64, i;
  int fw, fh;

  destroy_swapchain(w);
  fw = w->req_w, fh = w->req_h;
  if (w->glfw)
    glfwGetFramebufferSize(w->glfw, &fw, &fh);
  if (fw <= 0 || fh <= 0)
    return -1;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(w->phys, w->surface, &caps);
  vkGetPhysicalDeviceSurfaceFormatsKHR(w->phys, w->surface, &nf, fmts);
  vkfmt = w->srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
  for (i = 0; i < nf && fmts[i].format != vkfmt; i++) {
  }
  if (i == nf && w->srgb) {
    fprintf(stderr, "pdmtl: the surface has no B8G8R8A8_SRGB format, presenting BGRA8Unorm\n");
    w->srgb = 0, vkfmt = VK_FORMAT_B8G8R8A8_UNORM;
    for (i = 0; i < nf && fmts[i].format != vkfmt; i++) {
    }
  }
  if (i == nf) {
    fprintf(stderr, "pdmtl: the surface has no B8G8R8A8_UNORM format\n");
    return -1;
  }

  w->w = caps.currentExtent.width != 0xffffffffu ? caps.currentExtent.width : (uint32_t)fw;
  w->h = caps.currentExtent.height != 0xffffffffu ? caps.currentExtent.height : (uint32_t)fh;
  ci.surface = w->surface;
  ci.minImageCount = caps.minImageCount + 1 > MAX_IMAGES ? MAX_IMAGES : caps.minImageCount + 1;
  if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
    ci.minImageCount = caps.maxImageCount;
  ci.imageFormat = vkfmt, ci.imageColorSpace = fmts[i].colorSpace;
  ci.imageExtent.width = w->w, ci.imageExtent.height = w->h, ci.imageArrayLayers = 1;
  ci.imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) & caps.supportedUsageFlags;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, ci.presentMode = VK_PRESENT_MODE_FIFO_KHR, ci.clipped = 1;
  if (vkCreateSwapchainKHR(w->dev, &ci, 0, &w->swap) != VK_SUCCESS)
    return -1;
  w->nimages = MAX_IMAGES;
  vkGetSwapchainImagesKHR(w->dev, w->swap, &w->nimages, w->images);
  for (i = 0; i < w->nimages; i++) {
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

    vi.image = w->images[i], vi.viewType = VK_IMAGE_VIEW_TYPE_2D, vi.format = vkfmt;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    vkCreateImageView(w->dev, &vi, 0, &w->views[i]);
  }

  // the drawable's place in the arena: the CPU path draws there, zero-copy only uses its offset
  w->drawable.texture.w = w->w, w->drawable.texture.h = w->h, w->drawable.texture.fmt = w->srgb ? 18 : 0;
  w->drawable.texture.bpr = w->w * 4;
  w->drawable.texture.off = pdmtl_alloc((size_t)w->w * w->h * 4);
  w->drawable.texture.contents = pdmtl_arena() + w->drawable.texture.off;
  w->drawable.win = w;

  // copy mode presents through it every frame, zero-copy only a frame the CPU finished
  {
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements rq;

    bi.size = (VkDeviceSize)w->w * w->h * 4, bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vkCreateBuffer(w->dev, &bi, 0, &w->staging);
    vkGetBufferMemoryRequirements(w->dev, w->staging, &rq);
    ai.allocationSize = rq.size;
    ai.memoryTypeIndex =
        mem_type(w, rq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(w->dev, &ai, 0, &w->staging_mem);
    vkBindBufferMemory(w->dev, w->staging, w->staging_mem, 0);
    vkMapMemory(w->dev, w->staging_mem, 0, VK_WHOLE_SIZE, 0, &w->staging_map);
  }
  w->stale = 0;
  return 0;
}

static int attach(struct pdmtl_window *w);

struct pdmtl_window *pdmtl_frame_attach(void *context, pdmtl_frame_present present, int width, int height) {
  if (!present || width <= 0 || height <= 0) return 0;
  struct pdmtl_window *w = calloc(1,sizeof(*w));
  if (!w) return 0;
  w->gl=1; w->frame_context=context; w->frame_present=present; w->req_w=width; w->req_h=height;
  w->drawable.win=w;
  return w;
}

struct pdmtl_window *pdmtl_frame_attach_buffers(void *context, pdmtl_frame_present present,
    pdmtl_frame_acquire acquire, pdmtl_frame_commit commit, pdmtl_frame_cancel cancel, int width, int height) {
  if (!acquire || !commit || !cancel) return 0;
  struct pdmtl_window *w=pdmtl_frame_attach(context,present,width,height);
  if (w) w->frame_acquire=acquire, w->frame_commit=commit, w->frame_cancel=cancel;
  return w;
}

static int glfw_present_frame(void *context,const void *pixels,int width,int height,int pitch) {
  GLFWwindow *window=context;
  glfwMakeContextCurrent(window);
  GLuint texture=0,fbo=0;
  glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glPixelStorei(GL_UNPACK_ROW_LENGTH,pitch/4);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
  int w,h; glfwGetFramebufferSize(window,&w,&h);
  glDisable(GL_SCISSOR_TEST);
  glBlitFramebuffer(0,0,width,height,0,h,w,0,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  int rc=glGetError()==GL_NO_ERROR?0:-1;
  glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(1,&fbo); glDeleteTextures(1,&texture);
  if (!rc) glfwSwapBuffers(window);
  glfwMakeContextCurrent(0);
  return rc;
}

struct pdmtl_window *pdmtl_glfw_attach(void *window) {
  int width,height;
  if (!window) return 0;
  glfwGetFramebufferSize(window,&width,&height);
  struct pdmtl_window *w=pdmtl_frame_attach(window,glfw_present_frame,width,height);
  if (w) w->glfw=window;
  return w;
}

struct pdmtl_window *pdmtl_window_create(int width, int height, const char *title) {
  static const char *const dev_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
  struct pdmtl_window *w = calloc(1, sizeof(*w));
  struct pd_vk_handles h;
  const char **iext;
  uint32_t niext;

  if (pd_gl_enabled()) {
    free(w);
    if (!glfwInit()) return 0;
    glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window=glfwCreateWindow(width,height,title,0,0);
    if (!window) return 0;
    w=pdmtl_glfw_attach(window);
    if (!w) { glfwDestroyWindow(window); return 0; }
    w->own_glfw=1;
    glfwSetWindowUserPointer(window,w);
    glfwSetKeyCallback(window,on_key); glfwSetCursorPosCallback(window,on_move);
    glfwSetMouseButtonCallback(window,on_button); glfwSetFramebufferSizeCallback(window,on_resize); glfwSetWindowCloseCallback(window,on_close);
    return w;
  }
  if (!glfwInit() || !glfwVulkanSupported()) {
    fprintf(stderr, "pdmtl: GLFW or its Vulkan loader is unavailable\n");
    return 0;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  if (!(w->glfw = glfwCreateWindow(width, height, title, 0, 0)))
    return 0;
  glfwSetWindowUserPointer(w->glfw, w);
  glfwSetKeyCallback(w->glfw, on_key);
  glfwSetCursorPosCallback(w->glfw, on_move);
  glfwSetMouseButtonCallback(w->glfw, on_button);
  glfwSetFramebufferSizeCallback(w->glfw, on_resize);
  glfwSetWindowCloseCallback(w->glfw, on_close);

  // the executor's device presents too: it has to be created with the surface extensions
  iext = glfwGetRequiredInstanceExtensions(&niext);
  pd_vk_want_extensions(iext, niext, dev_ext, 1);
  if (pd_vk_start(&h)) {
    fprintf(stderr, "pdmtl: no Vulkan device to present with: %s\n", pd_vk_reason());
    return 0;
  }
  if (glfwCreateWindowSurface(h.instance, w->glfw, 0, &w->surface) != VK_SUCCESS)
    return 0;
  w->own_surface = 1; w->own_glfw = 1;
  return attach(w) ? 0 : w;
}

struct pdmtl_window *pdmtl_surface_attach(uint64_t surface, int width, int height) {
  struct pdmtl_window *w = calloc(1, sizeof(*w));

  w->surface = (VkSurfaceKHR)surface, w->req_w = width, w->req_h = height;
  if (attach(w)) {
    free(w);
    return 0;
  }
  return w;
}

// the swapchain and everything presenting needs, on the executor's device
static int attach(struct pdmtl_window *w) {
  const char *mode = getenv("PDMTL_PRESENT");
  struct pd_vk_handles h;
  VkBool32 can = 0;
  PFN_vkGetInstanceProcAddr gipa;

  if (pd_vk_start(&h)) {
    fprintf(stderr, "pdmtl: no Vulkan device to present with: %s\n", pd_vk_reason());
    return -1;
  }
  gipa = (PFN_vkGetInstanceProcAddr)h.gipa;
  w->inst = h.instance, w->phys = h.physical, w->dev = h.device, w->queue = h.queue, w->qfam = h.queue_family;
#define LOAD(f) f = (PFN_##f)gipa(w->inst, #f);
  VK_FUNCS(LOAD)
#define LOAD_DEV(f) f = (PFN_##f)vkGetDeviceProcAddr(w->dev, #f);
  VK_DEV_FUNCS(LOAD_DEV)

  vkGetPhysicalDeviceSurfaceSupportKHR(w->phys, w->qfam, w->surface, &can);
  if (!can) {
    fprintf(stderr, "pdmtl: the executor's queue cannot present to this window\n");
    return -1;
  }

  // zero-copy needs the executor to draw: AIRJITD_VULKAN on, and not forced to copy
  w->zero = pd_vk_enabled() && !(mode && !strcmp(mode, "copy"));
  {
    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    pci.queueFamilyIndex = w->qfam, pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(w->dev, &pci, 0, &w->pool);
    ai.commandPool = w->pool, ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(w->dev, &ai, &w->cb);
    vkCreateFence(w->dev, &fi, 0, &w->fence);
    vkCreateFence(w->dev, &fi, 0, &w->cb_fence);
    {
      VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      vkCreateSemaphore(w->dev, &sci, 0, &w->rendered);
    }
  }
  return make_swapchain(w);
}

int pdmtl_window_zero_copy(const struct pdmtl_window *w) {
  return w->zero;
}

void pdmtl_window_set_srgb(struct pdmtl_window *w, int srgb) {
  if (!!srgb == w->srgb)
    return;
  w->srgb = !!srgb, w->stale = 1;
}

struct pdmtl_drawable *pdmtl_next_drawable(struct pdmtl_window *w) {
  VkResult r;
  if (w->gl) {
    if (w->glfw) {
      if (glfwWindowShouldClose(w->glfw)) return 0;
      glfwGetFramebufferSize(w->glfw,&w->req_w,&w->req_h);
    }
    if (w->req_w <= 0 || w->req_h <= 0) return 0;
    if (!w->drawable.texture.contents || w->w != (unsigned)w->req_w || w->h != (unsigned)w->req_h) {
      w->drawable.texture=pdmtl_new_texture(w->req_w,w->req_h,w->srgb?18:0);
      if (!w->drawable.texture.contents) return 0;
      w->w=w->req_w; w->h=w->req_h;
    }
    w->drawable.texture.fmt=w->srgb?18:0;
    return &w->drawable;
  }

  for (int tries = 0; tries < 4; tries++) {
    if (w->glfw && glfwWindowShouldClose(w->glfw))
      return 0;
    if (w->stale || !w->swap) {
      if (make_swapchain(w)) {
        if (w->glfw)
          glfwWaitEvents();
        continue;
      }
    }
    r = vkAcquireNextImageKHR(w->dev, w->swap, UINT64_MAX, VK_NULL_HANDLE, w->fence, &w->drawable.index);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
      w->stale = 1;
      continue;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
      fprintf(stderr, "pdmtl: no drawable, vkAcquireNextImageKHR %d\n", (int)r);
      return 0;
    }
    vkWaitForFences(w->dev, 1, &w->fence, 1, UINT64_MAX);
    vkResetFences(w->dev, 1, &w->fence);
    if (w->zero) {
      // GENERAL is the layout the executor's render passes expect, contents start undefined like Metal's
      begin(w);
      barrier(w->cb, w->images[w->drawable.index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
      submit(w);
      pd_vk_external_attachment(w->drawable.texture.off, (uint64_t)w->images[w->drawable.index],
                                (uint64_t)w->views[w->drawable.index]);
    }
    return &w->drawable;
  }
  fprintf(stderr, "pdmtl: no drawable, no swapchain for %dx%d\n", w->req_w, w->req_h);
  return 0;
}

void pdmtl_clear(struct pdmtl_drawable *d, float r, float g, float b, float a) {
  struct pdmtl_window *w = d->win;

  if (w->gl) {
    struct PDDrawTex t={.off=d->texture.off,.w=d->texture.w,.h=d->texture.h,.bpr=d->texture.bpr,.fmt=d->texture.fmt};
    float color[4]={r,g,b,a}; pdmtl_fill(&t,color); return;
  }
  if (w->zero) {
    VkClearColorValue c = { { r, g, b, a } };
    VkImageSubresourceRange sr = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    begin(w);
    vkCmdClearColorImage(w->cb, w->images[d->index], VK_IMAGE_LAYOUT_GENERAL, &c, 1, &sr);
    submit(w);
  } else {
    uint8_t px[4] = { (uint8_t)(b * 255 + 0.5f), (uint8_t)(g * 255 + 0.5f), (uint8_t)(r * 255 + 0.5f),
                      (uint8_t)(a * 255 + 0.5f) };
    uint32_t v, *p = d->texture.contents;

    memcpy(&v, px, 4);
    for (size_t i = 0; i < (size_t)d->texture.w * d->texture.h; i++) {
      p[i] = v;
    }
  }
}

static uint64_t present_clock_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int pdmtl_present(struct pdmtl_drawable *d) {
  struct pdmtl_window *w = d->win;
  VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  VkImage im = w->images[d->index];
  VkResult r;

  if (w->gl) {
    if (w->frame_acquire && w->frame_commit && w->frame_cancel) {
      const char *async=getenv("AIRJITD_GL_ASYNC_PRESENT");
      if (async && !strcmp(async,"1")) {
        int pitch=0, rc;
        void *pixels=w->frame_acquire(w->frame_context,d->texture.w,d->texture.h,&pitch);
        void *ready=NULL; uint32_t ready_w=0,ready_h=0;
        if (pixels && pitch >= (uint64_t)d->texture.w*4)
          rc=pd_gl_present_copy_async(d->texture.off,pixels,pitch,&ready,&ready_w,&ready_h);
        else
          rc=pd_gl_present_copy_async(d->texture.off,NULL,0,&ready,&ready_w,&ready_h);
        if (ready) {
          int cr=rc<0?-1:w->frame_commit(w->frame_context,ready,ready_w,ready_h);
          if (cr) w->frame_cancel(w->frame_context,ready);
          if (rc>=0 && cr) rc=-1;
        }
        if (rc==2) return 0;
        if (pixels) w->frame_cancel(w->frame_context,pixels);
        if (rc<0) {
          fprintf(stderr,"pdmtl: asynchronous OpenGL readback failed\n");
          return -1;
        }
      }
      static unsigned trace_frames;
      static uint64_t trace_acquire, trace_read, trace_commit;
      const char *trace=getenv("INDIUM_PD_TRACE_PRESENT");
      uint64_t t0=trace?present_clock_us():0;
      int pitch=0;
      void *pixels=w->frame_acquire(w->frame_context,d->texture.w,d->texture.h,&pitch);
      uint64_t t1=trace?present_clock_us():0;
      if (pixels && pitch >= (uint64_t)d->texture.w*4) {
        int direct=pd_gl_present_copy(d->texture.off,pixels,pitch);
        uint64_t t2=trace?present_clock_us():0;
        if (direct==0) {
          int rc=w->frame_commit(w->frame_context,pixels,d->texture.w,d->texture.h);
          uint64_t t3=trace?present_clock_us():0;
          if (rc) w->frame_cancel(w->frame_context,pixels);
          if (trace) {
            trace_acquire+=t1-t0; trace_read+=t2-t1; trace_commit+=t3-t2;
            if (++trace_frames==30) {
              static uint64_t trace_last;
              uint64_t now=present_clock_us();
              fprintf(stderr,"pdmtl: direct present avg acquire %.2f ms, readback %.2f ms, commit %.2f ms, %.1f fps\n",
                      trace_acquire/30000.0,trace_read/30000.0,trace_commit/30000.0,
                      trace_last ? 30e6/(now-trace_last) : 0.0);
              trace_last=now;
              trace_frames=0; trace_acquire=trace_read=trace_commit=0;
            }
          }
          return rc;
        }
        w->frame_cancel(w->frame_context,pixels);
        if (direct<0) {
          fprintf(stderr,"pdmtl: refusing to present a failed OpenGL readback\n");
          return -1;
        }
      } else if (pixels) {
        w->frame_cancel(w->frame_context,pixels);
      }
      if (trace && !trace_frames++) fprintf(stderr,"pdmtl: direct frame-buffer path unavailable; using legacy present\n");
    }
    if (pd_gl_sync(d->texture.off,d->texture.bpr*d->texture.h)) {
      fprintf(stderr,"pdmtl: refusing to present a failed OpenGL readback\n");
      return -1;
    }
    pdmtl_sync(d->texture.off,d->texture.bpr*d->texture.h);
    return w->frame_present(w->frame_context,d->texture.contents,d->texture.w,d->texture.h,d->texture.bpr);
  }
  // the executor's batch goes first, in copy mode the frame's pixels come back from the GPU first
  pd_vk_flush(0);
  if (!w->zero)
    pdmtl_sync(d->texture.off, d->texture.bpr * d->texture.h);
  begin(w);
  if (w->zero && !pd_vk_external_written()) {
    barrier(w->cb, im, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  } else {
    // copy mode, or a frame the CPU finished (a multisample resolve): the arena's pixels go up
    if (w->zero)
      pdmtl_sync(d->texture.off, d->texture.bpr * d->texture.h);
    VkBufferImageCopy cp = { 0 };

    memcpy(w->staging_map, d->texture.contents, (size_t)w->w * w->h * 4);
    barrier(w->cb, im, w->zero ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, cp.imageSubresource.layerCount = 1;
    cp.imageExtent.width = w->w, cp.imageExtent.height = w->h, cp.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(w->cb, w->staging, im, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    barrier(w->cb, im, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }
  submit_signal(w, 1);
  pd_vk_external_attachment(0, 0, 0);

  pi.waitSemaphoreCount = 1, pi.pWaitSemaphores = &w->rendered;
  pi.swapchainCount = 1, pi.pSwapchains = &w->swap, pi.pImageIndices = &d->index;
  r = vkQueuePresentKHR(w->queue, &pi);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
    w->stale = 1;
  return r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR ? 0 : -1;
}

int pdmtl_poll_event(struct pdmtl_window *w, struct pdmtl_event *ev) {
  if (w->ev_head == w->ev_tail && w->glfw)
    glfwPollEvents();
  if (w->ev_head == w->ev_tail)
    return 0;
  *ev = w->ev[w->ev_head];
  w->ev_head = (w->ev_head + 1) % MAX_EVENTS;
  return 1;
}

void pdmtl_window_destroy(struct pdmtl_window *w) {
  if (!w)
    return;
  if (w->gl && w->frame_acquire && w->frame_commit && w->frame_cancel) {
    void *ready=NULL; uint32_t ready_w=0,ready_h=0;
    int rc=pd_gl_present_copy_async(w->drawable.texture.off,NULL,0,&ready,&ready_w,&ready_h);
    if (ready) {
      if (rc<0 || w->frame_commit(w->frame_context,ready,ready_w,ready_h))
        w->frame_cancel(w->frame_context,ready);
    }
  }
  if (!w->gl) destroy_swapchain(w);
  if (w->surface && w->own_surface)
    vkDestroySurfaceKHR(w->inst, w->surface, 0);
  if (w->glfw && w->own_glfw) {
    glfwDestroyWindow(w->glfw);
    glfwTerminate();
  }
  free(w);
}
