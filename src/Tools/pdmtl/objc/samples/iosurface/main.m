// SPDX-License-Identifier: MIT
// Metal over IOSurface across processes, a child renders into a surface its parent shares and checks
// then CPU writes reach Metal through the seed, and an offscreen CAMetalLayer presents into a surface
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>

extern char **environ;

// darling-metal's headers predate the IOSurface members
@protocol PDSurfaceTexture
- (id)iosurface;
- (NSUInteger)iosurfacePlane;
@end
@protocol PDSurfaceDevice
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d iosurface:(id)surface plane:(NSUInteger)plane;
@end

static id (*SurfaceCreate)(NSDictionary *);
static mach_port_t (*SurfaceCreateMachPort)(id);
static id (*SurfaceLookupFromMachPort)(mach_port_t);
static void *(*SurfaceBase)(id);
static size_t (*SurfaceBytesPerRow)(id);
static int (*SurfaceLock)(id, uint32_t, uint32_t *);
static int (*SurfaceUnlock)(id, uint32_t, uint32_t *);
static uint32_t (*SurfaceSeed)(id);
static CFTypeID (*SurfaceTypeID)(void);

static int failures;

#define CHECK(c, ...) do { \
    if (!(c)) { failures++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    else { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

#define W 64
#define H 32

static BOOL loadSurfaces(void) {
    void *h = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_NOW | RTLD_GLOBAL);
    if (!h)
        return NO;
    SurfaceCreate = dlsym(h, "IOSurfaceCreate");
    SurfaceCreateMachPort = dlsym(h, "IOSurfaceCreateMachPort");
    SurfaceLookupFromMachPort = dlsym(h, "IOSurfaceLookupFromMachPort");
    SurfaceBase = dlsym(h, "IOSurfaceGetBaseAddress");
    SurfaceBytesPerRow = dlsym(h, "IOSurfaceGetBytesPerRow");
    SurfaceLock = dlsym(h, "IOSurfaceLock");
    SurfaceUnlock = dlsym(h, "IOSurfaceUnlock");
    SurfaceSeed = dlsym(h, "IOSurfaceGetSeed");
    SurfaceTypeID = dlsym(h, "IOSurfaceGetTypeID");
    return SurfaceCreate && SurfaceLookupFromMachPort && SurfaceTypeID;
}

static id<MTLTexture> surfaceTexture(id<MTLDevice> device, id surface) {
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                 width:W height:H mipmapped:NO];
    d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    return [(id<PDSurfaceDevice>)device newTextureWithDescriptor:d iosurface:surface plane:0];
}

static void clearTo(id<MTLCommandQueue> queue, id<MTLTexture> t, MTLClearColor color, id<MTLDrawable> present) {
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = t;
    rp.colorAttachments[0].loadAction = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.colorAttachments[0].clearColor = color;
    id<MTLCommandBuffer> cb = [queue commandBuffer];
    [[cb renderCommandEncoderWithDescriptor:rp] endEncoding];
    if (present)
        [cb presentDrawable:present];
    [cb commit];
    [cb waitUntilCompleted];
}

static uint32_t pixelAt(id surface, int x, int y) {
    return ((uint32_t *)((uint8_t *)SurfaceBase(surface) + y * SurfaceBytesPerRow(surface)))[x];
}

// the child: the surface comes from the parent's registered ports, Metal clears it to 0x80/0x40/0x20
static int child(void) {
    mach_port_array_t ports;
    mach_msg_type_number_t n;
    if (mach_ports_lookup(mach_task_self(), &ports, &n) != KERN_SUCCESS || n < 1 || !MACH_PORT_VALID(ports[0]))
        return 2;
    id surface = SurfaceLookupFromMachPort(ports[0]);
    if (!surface)
        return 3;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLTexture> t = surfaceTexture(device, surface);
    if (!t)
        return 4;
    clearTo([device newCommandQueue], t, MTLClearColorMake(0x80 / 255.0, 0x40 / 255.0, 0x20 / 255.0, 1), nil);
    return 0;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (!loadSurfaces()) {
            printf("FAIL no IOSurface.framework\n");
            return 1;
        }
        if (argc > 1 && !strcmp(argv[1], "--child"))
            return child();

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        CHECK(device != nil, "a Metal device");
        if (!device)
            return 1;
        id<MTLCommandQueue> queue = [device newCommandQueue];
        NSDictionary *props = @{ @"IOSurfaceWidth": @W, @"IOSurfaceHeight": @H, @"IOSurfaceBytesPerElement": @4,
                                 @"IOSurfacePixelFormat": @((unsigned)'BGRA') };
        id surface = SurfaceCreate(props);
        CHECK(surface != nil, "a %dx%d BGRA surface", W, H);

        // the child renders through Metal into the surface
        mach_port_t port = SurfaceCreateMachPort(surface);
        mach_port_t reg[1] = { port };
        CHECK(mach_ports_register(mach_task_self(), reg, 1) == KERN_SUCCESS, "register the surface port");
        pid_t pid;
        char *args[] = { argv[0], "--child", NULL };
        int status = -1;
        CHECK(posix_spawn(&pid, argv[0], NULL, NULL, args, environ) == 0, "spawn the child");
        waitpid(pid, &status, 0);
        mach_ports_register(mach_task_self(), NULL, 0);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "child rendered (status %d)", status);
        uint32_t p = pixelAt(surface, 3, 5), q = pixelAt(surface, W - 1, H - 1);
        CHECK(p == 0xff804020 && q == 0xff804020, "parent sees the child's pixels (0x%08x 0x%08x)", p, q);

        // the surface's texture, its properties, and a CPU write reaching Metal through the seed
        id<MTLTexture> t = surfaceTexture(device, surface);
        CHECK(t != nil, "newTextureWithDescriptor:iosurface:plane:");
        CHECK([(id<PDSurfaceTexture>)t iosurface] == surface && [(id<PDSurfaceTexture>)t iosurfacePlane] == 0, "texture.iosurface is the surface");
        uint32_t px[W * H];
        [t getBytes:px bytesPerRow:W * 4 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
        CHECK(px[0] == 0xff804020, "texture reads the child's pixels (0x%08x)", px[0]);
        SurfaceLock(surface, 0, NULL);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                ((uint32_t *)((uint8_t *)SurfaceBase(surface) + y * SurfaceBytesPerRow(surface)))[x] = 0xff000000 | x << 16 | y;
        SurfaceUnlock(surface, 0, NULL);
        MTLTextureDescriptor *pd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                      width:W height:H mipmapped:NO];
        id<MTLTexture> plain = [device newTextureWithDescriptor:pd];
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
        [blit copyFromTexture:t sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(W, H, 1)
                    toTexture:plain destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        [plain getBytes:px bytesPerRow:W * 4 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
        CHECK(px[7 * W + 9] == (0xff000000 | 9 << 16 | 7), "a blit sees the CPU write (0x%08x)", px[7 * W + 9]);

        // a render into the surface bumps its seed
        uint32_t seed = SurfaceSeed(surface);
        clearTo(queue, t, MTLClearColorMake(0, 1, 0, 1), nil);
        CHECK(SurfaceSeed(surface) != seed && pixelAt(surface, 0, 0) == 0xff00ff00, "render moves the seed (0x%08x)",
              pixelAt(surface, 0, 0));

        // an offscreen CAMetalLayer: presenting makes its surface the layer's contents
        CAMetalLayer *layer = [CAMetalLayer layer];
        layer.device = device;
        layer.drawableSize = CGSizeMake(W, H);
        id<CAMetalDrawable> d = [layer nextDrawable];
        CHECK(d != nil && [(id<PDSurfaceTexture>)d.texture iosurface] != nil, "offscreen drawable over a surface");
        clearTo(queue, d.texture, MTLClearColorMake(0, 0, 1, 1), d);
        id contents = layer.contents;
        CHECK(contents && CFGetTypeID((__bridge CFTypeRef)contents) == SurfaceTypeID(), "presented layer holds a surface");
        if (contents)
            CHECK(pixelAt(contents, W / 2, H / 2) == 0xff0000ff, "layer surface is blue (0x%08x)", pixelAt(contents, W / 2, H / 2));

        printf("%d failures\n", failures);
    }
    return failures != 0;
}
