// IOSurfaceGPU.c - IOSurface pixel memory from the GPU's host-visible memory, for zink to render into in place
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// each process imports the memory through libdrm_virtgpu's dma-buf token, so all see the same pages
// Vulkan is loaded at run time, a process without a Venus device keeps ordinary memory
#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "IOSurfaceGPU.h"

// what libdrm_virtgpu writes at the start of a prime fd
struct prime_token {
    uint32_t magic, pad;
    uint64_t token, size;
};
#define PRIME_MAGIC 0x47565044u

static struct {
    int ready;
    VkInstance instance;
    VkDevice device;
    uint32_t memoryType;
    PFN_vkAllocateMemory allocate;
    PFN_vkFreeMemory free;
    PFN_vkMapMemory map;
    PFN_vkUnmapMemory unmap;
    PFN_vkGetMemoryFdKHR getFd;
} gpu;

static pthread_once_t gpuOnce = PTHREAD_ONCE_INIT;

static void gpuInit(void) {
    const char *iext[] = { "VK_KHR_external_memory_capabilities", "VK_KHR_get_physical_device_properties2" };
    const char *dext[] = { "VK_KHR_external_memory", "VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf" };
    const char *off = getenv("PD_IOSURFACE_GPU");
    VkPhysicalDevice devices[8], chosen = VK_NULL_HANDLE;
    uint32_t count = 8;
    void *lib;

    // opt in for now: rendering into host memory measured slower in Firefox than rendering in VRAM and copying
    if (off == NULL || strcmp(off, "1") != 0)
        return;
    if ((lib = dlopen("libvulkan.1.dylib", RTLD_LAZY | RTLD_LOCAL)) == NULL)
        return;
    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance createInstance = gipa ? (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance") : NULL;
    VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .enabledExtensionCount = 2,
                                 .ppEnabledExtensionNames = iext };

    if (createInstance == NULL || createInstance(&ici, NULL, &gpu.instance) != VK_SUCCESS)
        return;
#define I(name) PFN_##name name = (PFN_##name)gipa(gpu.instance, #name)
    I(vkEnumeratePhysicalDevices);
    I(vkGetPhysicalDeviceProperties);
    I(vkGetPhysicalDeviceMemoryProperties);
    I(vkCreateDevice);
    I(vkGetDeviceProcAddr);
#undef I
    if (vkEnumeratePhysicalDevices(gpu.instance, &count, devices) < 0)
        return;
    // the GPU device, the only one whose memory zink renders into
    for (uint32_t i = 0; i < count && chosen == VK_NULL_HANDLE; i++) {
        VkPhysicalDeviceProperties p;

        vkGetPhysicalDeviceProperties(devices[i], &p);
        if (p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU)
            chosen = devices[i];
    }
    if (chosen == VK_NULL_HANDLE)
        return;

    // host visible and coherent, cached when the device offers it since other processes read it on the CPU
    VkPhysicalDeviceMemoryProperties mp;
    VkMemoryPropertyFlags need = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetPhysicalDeviceMemoryProperties(chosen, &mp);
    gpu.memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount && gpu.memoryType == UINT32_MAX; i++)
        if ((mp.memoryTypes[i].propertyFlags & (need | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) == (need | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
            gpu.memoryType = i;
    for (uint32_t i = 0; i < mp.memoryTypeCount && gpu.memoryType == UINT32_MAX; i++)
        if ((mp.memoryTypes[i].propertyFlags & need) == need)
            gpu.memoryType = i;
    if (gpu.memoryType == UINT32_MAX)
        return;

    float priority = 1;
    VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                    .pQueuePriorities = &priority };
    VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                               .pQueueCreateInfos = &qci, .enabledExtensionCount = 3, .ppEnabledExtensionNames = dext };
    if (vkCreateDevice(chosen, &dci, NULL, &gpu.device) != VK_SUCCESS)
        return;
    gpu.allocate = (PFN_vkAllocateMemory)vkGetDeviceProcAddr(gpu.device, "vkAllocateMemory");
    gpu.free = (PFN_vkFreeMemory)vkGetDeviceProcAddr(gpu.device, "vkFreeMemory");
    gpu.map = (PFN_vkMapMemory)vkGetDeviceProcAddr(gpu.device, "vkMapMemory");
    gpu.unmap = (PFN_vkUnmapMemory)vkGetDeviceProcAddr(gpu.device, "vkUnmapMemory");
    gpu.getFd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(gpu.device, "vkGetMemoryFdKHR");
    gpu.ready = gpu.allocate && gpu.free && gpu.map && gpu.unmap && gpu.getFd;
    if (getenv("PD_IOSURFACE_TRACE"))
        fprintf(stderr, "IOSurface: GPU memory %s (type %u)\n", gpu.ready ? "available" : "unavailable", gpu.memoryType);
}

int IOSurfaceGPUAvailable(void) {
    pthread_once(&gpuOnce, gpuInit);
    return gpu.ready;
}

static int map(struct IOSurfaceGPUMemory *m, uint64_t size) {
    void *addr = NULL;

    if (gpu.map(gpu.device, (VkDeviceMemory)m->memory, 0, size, 0, &addr) != VK_SUCCESS) {
        gpu.free(gpu.device, (VkDeviceMemory)m->memory, NULL);
        return 0;
    }
    m->base = addr;
    m->size = size;
    return 1;
}

int IOSurfaceGPUAllocate(uint64_t size, struct IOSurfaceGPUMemory *m, uint64_t *token) {
    VkExportMemoryAllocateInfo export = { .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
                                          .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT };
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &export,
                                 .allocationSize = size, .memoryTypeIndex = gpu.memoryType };
    VkDeviceMemory memory;
    struct prime_token t;
    int fd = -1;

    if (!IOSurfaceGPUAvailable() || gpu.allocate(gpu.device, &mai, NULL, &memory) != VK_SUCCESS)
        return 0;
    m->memory = (uint64_t)memory;
    if (!map(m, size))
        return 0;
    VkMemoryGetFdInfoKHR gfi = { .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, .memory = memory,
                                 .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT };
    // the token outlives the fd, other processes rebuild an fd from it
    if (gpu.getFd(gpu.device, &gfi, &fd) != VK_SUCCESS || pread(fd, &t, sizeof(t), 0) != sizeof(t) || t.magic != PRIME_MAGIC) {
        if (fd >= 0)
            close(fd);
        IOSurfaceGPUFree(m);
        return 0;
    }
    close(fd);
    *token = t.token;
    return 1;
}

int IOSurfaceGPUMakeFD(uint64_t token, uint64_t size) {
    struct prime_token t = { PRIME_MAGIC, 0, token, size };
    char path[] = "/tmp/pdios.XXXXXX";
    int fd = mkstemp(path);

    if (fd < 0)
        return -1;
    unlink(path);
    if (write(fd, &t, sizeof(t)) != sizeof(t) || ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    return fd;
}

int IOSurfaceGPUImport(uint64_t token, uint64_t size, struct IOSurfaceGPUMemory *m) {
    VkDeviceMemory memory;
    int fd;

    if (!IOSurfaceGPUAvailable() || (fd = IOSurfaceGPUMakeFD(token, size)) < 0)
        return 0;
    VkImportMemoryFdInfoKHR imp = { .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, .fd = fd };
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &imp,
                                 .allocationSize = size, .memoryTypeIndex = gpu.memoryType };
    // a successful import owns the fd
    if (gpu.allocate(gpu.device, &mai, NULL, &memory) != VK_SUCCESS) {
        close(fd);
        return 0;
    }
    m->memory = (uint64_t)memory;
    return map(m, size);
}

void IOSurfaceGPUFree(struct IOSurfaceGPUMemory *m) {
    if (!gpu.ready || m->memory == 0)
        return;
    if (m->base)
        gpu.unmap(gpu.device, (VkDeviceMemory)m->memory);
    gpu.free(gpu.device, (VkDeviceMemory)m->memory, NULL);
    m->memory = 0;
    m->base = NULL;
}
