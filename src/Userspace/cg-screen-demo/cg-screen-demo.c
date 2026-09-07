#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGContext.h>
#include <PDSurface.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
fill_demo(CGContextRef context, size_t width, size_t height)
{
    CGContextSetRGBFillColor(context, 0.04, 0.05, 0.09, 1.0);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));
    CGContextSetRGBFillColor(context, 0.12, 0.45, 0.85, 1.0);
    CGContextFillRect(context, CGRectMake(width / 10, height / 10,
                                           width * 8 / 10, height / 5));
    CGContextSetRGBFillColor(context, 0.85, 0.20, 0.12, 1.0);
    CGContextFillRect(context, CGRectMake(width / 10, height * 4 / 10,
                                           width * 3 / 10, height * 4 / 10));
    CGContextSetRGBFillColor(context, 0.15, 0.75, 0.35, 1.0);
    CGContextFillRect(context, CGRectMake(width * 6 / 10, height * 4 / 10,
                                           width * 3 / 10, height * 4 / 10));
}

int
main(int argc, char **argv)
{
    uint32_t width = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 1024;
    uint32_t height = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 768;
    PDSurfaceDeviceRef device = NULL;
    PDSurfaceRef surface = NULL;
    CGColorSpaceRef colorSpace = NULL;
    CGContextRef context = NULL;
    void *bitmap = NULL;
    int status = 1;

    kern_return_t kr = PDSurfaceDeviceOpen(&device);
    if (width == 0 || height == 0 || kr != KERN_SUCCESS) {
        fprintf(stderr, "cg-screen-demo: no PDSurface device\n");
        goto done;
    }
    PDSurfaceDescriptor descriptor = {
        .width = width, .height = height,
        .format = kPDSurfaceFormatXRGB8888,
        .usage = kPDSurfaceUsageLinear | kPDSurfaceUsageScanout,
    };
    kr = PDSurfaceCreate(device, &descriptor, &surface);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "cg-screen-demo: surface creation failed\n");
        goto done;
    }
    void *pixels = PDSurfaceGetBaseAddress(surface);
    uint32_t stride = PDSurfaceGetStride(surface);
    if (pixels == NULL || stride < width * 4) {
        fprintf(stderr, "cg-screen-demo: surface is not CPU-mappable\n");
        goto done;
    }
    bitmap = calloc(1, (size_t)stride * height);
    if (bitmap == NULL) {
        fprintf(stderr, "cg-screen-demo: bitmap allocation failed\n");
        goto done;
    }
    colorSpace = CGColorSpaceCreateDeviceRGB();
    context = CGBitmapContextCreate(bitmap, width, height, 8, stride,
        colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    if (context == NULL) {
        fprintf(stderr, "cg-screen-demo: bitmap context creation failed\n");
        goto done;
    }
    fill_demo(context, width, height);
    memcpy(pixels, bitmap, (size_t)stride * height);
    if (PDSurfaceFlush(surface, 0, 0, 0, 0) != KERN_SUCCESS ||
        PDSurfaceSetScanout(device, surface) != KERN_SUCCESS) {
        fprintf(stderr, "cg-screen-demo: scanout failed\n");
        goto done;
    }
    fprintf(stderr, "cg-screen-demo: %ux%u displayed on %s\n",
            width, height, PDSurfaceDeviceGetName(device));
    fprintf(stderr, "cg-screen-demo: press return to exit\n");
    char input;
    while (read(STDIN_FILENO, &input, sizeof(input)) < 0 && errno == EINTR) {
    }
    status = 0;
done:
    if (context != NULL) CGContextRelease(context);
    if (colorSpace != NULL) CGColorSpaceRelease(colorSpace);
    if (surface != NULL) PDSurfaceRelease(surface);
    if (device != NULL) PDSurfaceDeviceClose(device);
    free(bitmap);
    return status;
}
