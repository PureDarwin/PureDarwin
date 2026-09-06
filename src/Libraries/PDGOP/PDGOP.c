#include <PDGOP.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>

#define kIOFBVRAMMemory 110
#define kIOFBGetPixelInformationSelector 1
#define kIOFBGetCurrentDisplayModeSelector 2
#define kIOFBSystemAperture 0
#define kPDGPU_Present 11

typedef struct PDGOPPresentRect {
    uint32_t x, y, width, height;
} PDGOPPresentRect;

static const char *gPDGOPLastErrorStage = "none";

#define PDGOP_SET_STAGE(stage) do { gPDGOPLastErrorStage = (stage); } while (0)

typedef uint32_t IODisplayModeID;
typedef uint32_t IOIndex;
typedef uint32_t IOPixelAperture;
typedef char IOPixelEncoding[64];

typedef struct IOPixelInformation {
    uint32_t bytesPerRow;
    uint32_t bytesPerPlane;
    uint32_t bitsPerPixel;
    uint32_t pixelType;
    uint32_t componentCount;
    uint32_t bitsPerComponent;
    uint32_t componentMasks[16];
    IOPixelEncoding pixelFormat;
    uint32_t flags;
    uint32_t activeWidth;
    uint32_t activeHeight;
    uint32_t reserved[2];
} IOPixelInformation;

kern_return_t
PDGOPOpen(PDGOPFramebuffer *fb)
{
    kern_return_t kr;
#ifdef _PD_IOKITLIB_H
    char *matching;
#else
    CFDictionaryRef matching;
#endif
    uint64_t scalarOut[2] = {0, 0};
    uint32_t scalarOutCnt = 2;
    uint64_t scalarIn[3];
    IOPixelInformation pixelInfo;
    size_t pixelInfoSize = sizeof(pixelInfo);

    if (fb == NULL) {
        PDGOP_SET_STAGE("argument");
        return KERN_INVALID_ARGUMENT;
    }
    memset(fb, 0, sizeof(*fb));
    PDGOP_SET_STAGE("none");

    PDGOP_SET_STAGE("IOMasterPort");
    kr = IOMasterPort(MACH_PORT_NULL, &fb->masterPort);
    if (kr != KERN_SUCCESS) {
        goto fail;
    }

    static const char *kFramebufferClasses[] = {
        "IOVirtIOGPU",
        "IOGOPFramebuffer",
        /* Some ARM64 kernels publish the concrete framebuffer under the
         * IOFramebuffer superclass in the user-visible registry. */
        "IOFramebuffer",
    };
    fb->service = IO_OBJECT_NULL;
    PDGOP_SET_STAGE("IOServiceMatching");
    for (size_t i = 0; i < sizeof(kFramebufferClasses) / sizeof(kFramebufferClasses[0]); i++) {
        matching = IOServiceMatching(kFramebufferClasses[i]);
        if (matching == NULL) {
            continue;
        }
#ifdef _PD_IOKITLIB_H
        kr = IOServiceGetMatchingService(fb->masterPort, matching,
            &fb->service);
#else
        fb->service = IOServiceGetMatchingService(fb->masterPort, matching);
        kr = fb->service != IO_OBJECT_NULL ? KERN_SUCCESS : KERN_FAILURE;
#endif
        if (fb->service != IO_OBJECT_NULL) {
            char serviceName[128] = {0};
            if (IORegistryEntryGetName(fb->service, serviceName) == KERN_SUCCESS)
                fprintf(stderr, "PDGOP: selected framebuffer service %s\n", serviceName);
            break;
        }
        kr = KERN_FAILURE;
        fb->service = IO_OBJECT_NULL;
    }
    PDGOP_SET_STAGE("IOServiceGetMatchingService");
    if (fb->service == IO_OBJECT_NULL) {
        kr = KERN_FAILURE;
        goto fail;
    }

    PDGOP_SET_STAGE("IOServiceOpen");
    kr = IOServiceOpen(fb->service, mach_task_self(), 0, &fb->connect);
    if (kr != KERN_SUCCESS) {
        goto fail;
    }

    PDGOP_SET_STAGE("GetCurrentDisplayMode");
    kr = IOConnectCallScalarMethod(fb->connect,
        kIOFBGetCurrentDisplayModeSelector, NULL, 0, scalarOut, &scalarOutCnt);
    if (kr != KERN_SUCCESS || scalarOutCnt < 2) {
        goto fail;
    }

    scalarIn[0] = (IODisplayModeID)scalarOut[0];
    scalarIn[1] = (IOIndex)scalarOut[1];
    scalarIn[2] = kIOFBSystemAperture;
    memset(&pixelInfo, 0, sizeof(pixelInfo));
    PDGOP_SET_STAGE("GetPixelInformation");
    kr = IOConnectCallMethod(fb->connect,
        kIOFBGetPixelInformationSelector, scalarIn, 3, NULL, 0,
        NULL, NULL, &pixelInfo, &pixelInfoSize);
    if (kr != KERN_SUCCESS) {
        goto fail;
    }

    PDGOP_SET_STAGE("MapVRAM");
    kr = IOConnectMapMemory64(fb->connect, kIOFBVRAMMemory, mach_task_self(),
        &fb->address, &fb->size, kIOMapAnywhere);
    if (kr != KERN_SUCCESS) {
        goto fail;
    }

    fb->width = pixelInfo.activeWidth;
    fb->height = pixelInfo.activeHeight;
    fb->stride = pixelInfo.bytesPerRow;
    fb->bpp = pixelInfo.bitsPerPixel;
    fb->pixelType = pixelInfo.pixelType;
    fb->componentMasks[0] = pixelInfo.componentMasks[0];
    fb->componentMasks[1] = pixelInfo.componentMasks[1];
    fb->componentMasks[2] = pixelInfo.componentMasks[2];

    PDGOP_SET_STAGE("none");
    return KERN_SUCCESS;

fail:
    PDGOPClose(fb);
    return kr;
}

void
PDGOPClose(PDGOPFramebuffer *fb)
{
    if (fb == NULL) {
        return;
    }
    if (fb->address != 0 && fb->connect != IO_OBJECT_NULL) {
        (void)IOConnectUnmapMemory64(fb->connect, kIOFBVRAMMemory,
            mach_task_self(), fb->address);
    }
    if (fb->connect != IO_OBJECT_NULL) {
        (void)IOServiceClose(fb->connect);
    }
    if (fb->service != IO_OBJECT_NULL) {
        (void)IOObjectRelease(fb->service);
    }
    if (fb->masterPort != MACH_PORT_NULL) {
        (void)mach_port_deallocate(mach_task_self(), fb->masterPort);
    }
    memset(fb, 0, sizeof(*fb));
}

kern_return_t
PDGOPPresent(PDGOPFramebuffer *fb, uint32_t x, uint32_t y,
             uint32_t width, uint32_t height)
{
    PDGOPPresentRect rect;

    if (fb == NULL || fb->connect == IO_OBJECT_NULL)
        return KERN_INVALID_ARGUMENT;
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return IOConnectCallStructMethod(fb->connect, kPDGPU_Present,
                                      &rect, sizeof(rect), NULL, NULL);
}

const char *
PDGOPLastErrorStage(void)
{
    return gPDGOPLastErrorStage;
}
