#include <PDSurface.h>

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>
struct PDSurfaceDevice {
    mach_port_t  masterPort;
    io_connect_t connect;
    const char  *name;
};

struct PDSurface {
    PDSurfaceDeviceRef device;
    struct PDSurfaceInfo info;
    void    *base;
    uint64_t mappedLength;
    bool     owned;   /* created here rather than looked up */
};

/* Tried in order, the same way PDGOP finds a framebuffer. A new driver joins
 * by implementing the protocol and being added here. */
static const char *kSurfaceProviderClasses[] = {
    "IOVirtIOGPU",
};

kern_return_t
PDSurfaceDeviceOpen(PDSurfaceDeviceRef *outDevice)
{
    if (outDevice == NULL) {
        return KERN_INVALID_ARGUMENT;
    }
    *outDevice = NULL;

    PDSurfaceDeviceRef device = calloc(1, sizeof(*device));
    if (device == NULL) {
        return KERN_RESOURCE_SHORTAGE;
    }

    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &device->masterPort);
    if (kr != KERN_SUCCESS) {
        free(device);
        return kr;
    }

    for (size_t i = 0;
         i < sizeof(kSurfaceProviderClasses) / sizeof(kSurfaceProviderClasses[0]);
         i++) {
        /* PureDarwin's IOKitLib and Apple's disagree on both the matching
         * dictionary's type and this call's signature, and which one is in
         * scope depends on the include path. PDGOP splits the same way. */
#ifdef _PD_IOKITLIB_H
        char *matching = IOServiceMatching(kSurfaceProviderClasses[i]);
#else
        CFDictionaryRef matching = IOServiceMatching(kSurfaceProviderClasses[i]);
#endif
        if (matching == NULL) {
            continue;
        }

        io_service_t service = IO_OBJECT_NULL;
#ifdef _PD_IOKITLIB_H
        if (IOServiceGetMatchingService(device->masterPort, matching,
                                        &service) != KERN_SUCCESS) {
            continue;
        }
#else
        service = IOServiceGetMatchingService(device->masterPort, matching);
#endif
        if (service == IO_OBJECT_NULL) {
            continue;
        }

        kr = IOServiceOpen(service, mach_task_self(), kPDSurfaceConnectType,
                           &device->connect);
        IOObjectRelease(service);
        if (kr != KERN_SUCCESS) {
            device->connect = IO_OBJECT_NULL;
            continue;  /* driver exists but does not serve surfaces */
        }

        uint64_t version = 0;
        uint32_t versionCount = 1;
        if (IOConnectCallScalarMethod(device->connect, kPDSurface_GetVersion,
                                      NULL, 0, &version,
                                      &versionCount) != KERN_SUCCESS ||
            version != kPDSurfaceProtocolVersion) {
            IOServiceClose(device->connect);
            device->connect = IO_OBJECT_NULL;
            continue;
        }

        device->name = kSurfaceProviderClasses[i];
        *outDevice = device;
        return KERN_SUCCESS;
    }

    free(device);
    return KERN_NO_SPACE;
}

void
PDSurfaceDeviceClose(PDSurfaceDeviceRef device)
{
    if (device == NULL) {
        return;
    }
    if (device->connect != IO_OBJECT_NULL) {
        IOServiceClose(device->connect);
    }
    free(device);
}

const char *
PDSurfaceDeviceGetName(PDSurfaceDeviceRef device)
{
    return device != NULL ? device->name : NULL;
}

/* The driver maps a surface's storage under its own id, so the id doubles as
 * the memory type for IOConnectMapMemory64. */
static kern_return_t
map_surface(PDSurfaceRef surface)
{
    mach_vm_address_t address = 0;
    mach_vm_size_t size = 0;

    kern_return_t kr = IOConnectMapMemory64(surface->device->connect,
        (uint32_t)surface->info.surface_id, mach_task_self(), &address, &size,
        kIOMapAnywhere);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    surface->base = (void *)(uintptr_t)address;
    surface->mappedLength = size;
    return KERN_SUCCESS;
}

kern_return_t
PDSurfaceCreate(PDSurfaceDeviceRef device,
                const PDSurfaceDescriptor *descriptor,
                PDSurfaceRef *outSurface)
{
    if (device == NULL || descriptor == NULL || outSurface == NULL) {
        return KERN_INVALID_ARGUMENT;
    }
    *outSurface = NULL;

    struct PDSurfaceCreateRequest request = {
        .width = descriptor->width,
        .height = descriptor->height,
        .format = descriptor->format,
        .usage = descriptor->usage,
    };

    uint64_t output[3] = { 0, 0, 0 };
    uint32_t outputCount = 3;
    kern_return_t kr = IOConnectCallMethod(device->connect, kPDSurface_Create,
        NULL, 0, &request, sizeof(request), output, &outputCount, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    PDSurfaceRef surface = calloc(1, sizeof(*surface));
    if (surface == NULL) {
        uint64_t id = output[0];
        IOConnectCallScalarMethod(device->connect, kPDSurface_Destroy, &id, 1,
                                  NULL, NULL);
        return KERN_RESOURCE_SHORTAGE;
    }

    surface->device = device;
    surface->owned = true;
    surface->info.surface_id = output[0];
    surface->info.width = descriptor->width;
    surface->info.height = descriptor->height;
    surface->info.format = descriptor->format;
    surface->info.usage = descriptor->usage;
    surface->info.stride = (uint32_t)output[1];
    surface->info.length = output[2];

    if (descriptor->usage & kPDSurfaceUsageLinear) {
        map_surface(surface);  /* best effort; callers check the base address */
    }

    *outSurface = surface;
    return KERN_SUCCESS;
}

kern_return_t
PDSurfaceLookup(PDSurfaceDeviceRef device, uint64_t surfaceID,
                PDSurfaceRef *outSurface)
{
    if (device == NULL || outSurface == NULL) {
        return KERN_INVALID_ARGUMENT;
    }
    *outSurface = NULL;

    PDSurfaceRef surface = calloc(1, sizeof(*surface));
    if (surface == NULL) {
        return KERN_RESOURCE_SHORTAGE;
    }

    uint64_t input = surfaceID;
    size_t infoSize = sizeof(surface->info);
    kern_return_t kr = IOConnectCallMethod(device->connect, kPDSurface_Lookup,
        &input, 1, NULL, 0, NULL, NULL, &surface->info, &infoSize);
    if (kr != KERN_SUCCESS) {
        free(surface);
        return kr;
    }

    surface->device = device;
    surface->owned = false;
    if (surface->info.usage & kPDSurfaceUsageLinear) {
        map_surface(surface);
    }

    *outSurface = surface;
    return KERN_SUCCESS;
}

void
PDSurfaceRelease(PDSurfaceRef surface)
{
    if (surface == NULL) {
        return;
    }
    if (surface->base != NULL) {
        IOConnectUnmapMemory64(surface->device->connect,
            (uint32_t)surface->info.surface_id, mach_task_self(),
            (mach_vm_address_t)(uintptr_t)surface->base);
    }
    if (surface->owned) {
        uint64_t id = surface->info.surface_id;
        IOConnectCallScalarMethod(surface->device->connect, kPDSurface_Destroy,
                                  &id, 1, NULL, NULL);
    }
    free(surface);
}

uint32_t PDSurfaceGetWidth(PDSurfaceRef s)  { return s ? s->info.width  : 0; }
uint32_t PDSurfaceGetHeight(PDSurfaceRef s) { return s ? s->info.height : 0; }
uint32_t PDSurfaceGetFormat(PDSurfaceRef s) { return s ? s->info.format : 0; }
uint32_t PDSurfaceGetStride(PDSurfaceRef s) { return s ? s->info.stride : 0; }
void    *PDSurfaceGetBaseAddress(PDSurfaceRef s) { return s ? s->base : NULL; }
uint64_t PDSurfaceGetID(PDSurfaceRef s) { return s ? s->info.surface_id : 0; }

kern_return_t
PDSurfaceFlush(PDSurfaceRef surface, uint32_t x, uint32_t y,
               uint32_t width, uint32_t height)
{
    if (surface == NULL) {
        return KERN_INVALID_ARGUMENT;
    }
    if (width == 0 || height == 0) {
        x = 0;
        y = 0;
        width = surface->info.width;
        height = surface->info.height;
    }

    struct PDSurfaceFlushRequest request = {
        .surface_id = surface->info.surface_id,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };
    return IOConnectCallStructMethod(surface->device->connect, kPDSurface_Flush,
                                     &request, sizeof(request), NULL, NULL);
}

kern_return_t
PDSurfaceSetScanout(PDSurfaceDeviceRef device, PDSurfaceRef surface)
{
    if (device == NULL) {
        return KERN_INVALID_ARGUMENT;
    }
    if (surface != NULL && !(surface->info.usage & kPDSurfaceUsageScanout)) {
        return KERN_INVALID_ARGUMENT;
    }

    uint64_t id = surface != NULL ? surface->info.surface_id : 0;
    return IOConnectCallScalarMethod(device->connect, kPDSurface_SetScanout,
                                     &id, 1, NULL, NULL);
}
