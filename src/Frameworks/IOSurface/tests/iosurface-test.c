// SPDX-License-Identifier: MIT
// iosurface-test: surfaces shared between two processes through a Mach port and a global ID
// with locking, seeds, use counts, shared values and a two-plane NV12 surface
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct __IOSurface *IOSurfaceRef;
typedef uint32_t IOSurfaceID;
extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat,
    kIOSurfaceIsGlobal, kIOSurfacePlaneInfo, kIOSurfacePlaneWidth, kIOSurfacePlaneHeight,
    kIOSurfacePlaneBytesPerElement, kIOSurfaceBytesPerRow;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef);
IOSurfaceRef IOSurfaceLookup(IOSurfaceID);
IOSurfaceID IOSurfaceGetID(IOSurfaceRef);
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef);
IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t);
void *IOSurfaceGetBaseAddress(IOSurfaceRef);
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef, size_t);
size_t IOSurfaceGetWidth(IOSurfaceRef), IOSurfaceGetHeight(IOSurfaceRef), IOSurfaceGetBytesPerRow(IOSurfaceRef);
size_t IOSurfaceGetPlaneCount(IOSurfaceRef), IOSurfaceGetWidthOfPlane(IOSurfaceRef, size_t);
size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef, size_t), IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef, size_t);
size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef, size_t);
int IOSurfaceLock(IOSurfaceRef, uint32_t, uint32_t *), IOSurfaceUnlock(IOSurfaceRef, uint32_t, uint32_t *);
uint32_t IOSurfaceGetSeed(IOSurfaceRef);
void IOSurfaceIncrementUseCount(IOSurfaceRef);
int32_t IOSurfaceGetUseCount(IOSurfaceRef);
void IOSurfaceSetValue(IOSurfaceRef, CFStringRef, CFTypeRef);
CFTypeRef IOSurfaceCopyValue(IOSurfaceRef, CFStringRef);

static int failures;
static const char *who = "parent";

static void check(int ok, const char *what) {
    printf("%s %s: %s\n", ok ? "ok  " : "FAIL", who, what);
    if (!ok)
        failures++;
}

static void put(CFMutableDictionaryRef d, CFStringRef k, long long v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, k, n);
    CFRelease(n);
}

static IOSurfaceRef bgra(int w, int h, int global) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOSurfaceRef s;

    put(d, kIOSurfaceWidth, w), put(d, kIOSurfaceHeight, h), put(d, kIOSurfaceBytesPerElement, 4);
    put(d, kIOSurfacePixelFormat, 'BGRA');
    if (global)
        CFDictionarySetValue(d, kIOSurfaceIsGlobal, kCFBooleanTrue);
    s = IOSurfaceCreate(d);
    CFRelease(d);
    return s;
}

static int child(const char *mode, const char *idText) {
    IOSurfaceRef s = NULL;
    uint32_t seed = 0;

    who = "child";
    if (strcmp(mode, "port") == 0) {
        mach_port_t *ports = NULL;
        mach_msg_type_number_t count = 0;

        // a spawned task inherits the ports its parent registered
        check(mach_ports_lookup(mach_task_self(), &ports, &count) == KERN_SUCCESS && count > 0 &&
              ports[0] != MACH_PORT_NULL, "inherited the surface port");
        if (count > 0)
            s = IOSurfaceLookupFromMachPort(ports[0]);
    } else {
        s = IOSurfaceLookup((IOSurfaceID)strtoul(idText, NULL, 0));
        if (strcmp(mode, "gone") == 0) {
            check(s == NULL, "a retired global ID no longer resolves");
            return failures;
        }
    }
    check(s != NULL, "looked the surface up");
    if (!s)
        return 1;
    check(IOSurfaceGetWidth(s) == 64 && IOSurfaceGetHeight(s) == 32, "sees the creator's geometry");
    uint32_t *px = IOSurfaceGetBaseAddress(s);
    check(px[0] == 0xff112233 && px[64 * 32 - 1] == 0xff445566, "reads the creator's pixels");
    CFStringRef v = IOSurfaceCopyValue(s, CFSTR("Greeting"));
    check(v && CFEqual(v, CFSTR("hello")), "reads the creator's value");
    if (v)
        CFRelease(v);
    IOSurfaceLock(s, 0, &seed);
    px[0] = 0xffaabbcc;
    IOSurfaceUnlock(s, 0, NULL);
    check(IOSurfaceGetSeed(s) == seed + 1, "a write unlock moves the seed");
    IOSurfaceIncrementUseCount(s);
    IOSurfaceSetValue(s, CFSTR("Answer"), CFSTR("from the child"));
    return failures;
}

static int spawnChild(const char *mode, IOSurfaceID id) {
    char idText[64], self[1024];
    char *argv[] = { self, (char *)mode, idText, NULL };
    pid_t pid;
    int status = 0;
    uint32_t size = sizeof(self);
    extern int _NSGetExecutablePath(char *, uint32_t *);

    snprintf(idText, sizeof(idText), "0x%x", id);
    _NSGetExecutablePath(self, &size);
    if (posix_spawn(&pid, self, NULL, NULL, argv, NULL) != 0)
        return -1;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void shareOver(const char *mode, int global) {
    IOSurfaceRef s = bgra(64, 32, global);
    uint32_t *px;
    uint32_t seedBefore;
    CFStringRef v;

    check(s != NULL, global ? "created a global surface" : "created a surface");
    if (!s)
        return;
    px = IOSurfaceGetBaseAddress(s);
    for (int i = 0; i < 64 * 32; i++)
        px[i] = 0xff112233;
    px[64 * 32 - 1] = 0xff445566;
    IOSurfaceSetValue(s, CFSTR("Greeting"), CFSTR("hello"));
    seedBefore = IOSurfaceGetSeed(s);
    if (!global) {
        mach_port_t port = IOSurfaceCreateMachPort(s);
        check(port != MACH_PORT_NULL, "made a Mach port for it");
        check(mach_ports_register(mach_task_self(), &port, 1) == KERN_SUCCESS, "registered the port for the child");
    } else {
        check(IOSurfaceGetID(s) != 0 && IOSurfaceGetID(s) < 0x80000000u, "got a global ID from iosurfaced");
    }
    check(spawnChild(mode, IOSurfaceGetID(s)) == 0, "the child passed");
    check(px[0] == 0xffaabbcc, "sees the child's write");
    check(IOSurfaceGetSeed(s) == seedBefore + 1, "sees the child's seed");
    check(IOSurfaceGetUseCount(s) == 1, "sees the child's use count");
    v = IOSurfaceCopyValue(s, CFSTR("Answer"));
    check(v && CFEqual(v, CFSTR("from the child")), "sees the child's value");
    if (v)
        CFRelease(v);
    if (global) {
        IOSurfaceID id = IOSurfaceGetID(s);
        CFRelease(s);
        // the broker hears of the release through the dead token, give it a moment
        usleep(200000);
        check(spawnChild("gone", id) == 0, "the ID is retired once the creator lets go");
    } else {
        CFRelease(s);
    }
}

static void nv12(void) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef planes = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    CFMutableDictionaryRef y = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef uv = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOSurfaceRef s, t;
    mach_port_t port;

    put(y, kIOSurfacePlaneWidth, 640), put(y, kIOSurfacePlaneHeight, 480), put(y, kIOSurfacePlaneBytesPerElement, 1);
    put(uv, kIOSurfacePlaneWidth, 320), put(uv, kIOSurfacePlaneHeight, 240), put(uv, kIOSurfacePlaneBytesPerElement, 2);
    CFArrayAppendValue(planes, y), CFArrayAppendValue(planes, uv);
    put(d, kIOSurfaceWidth, 640), put(d, kIOSurfaceHeight, 480), put(d, kIOSurfacePixelFormat, '420v');
    CFDictionarySetValue(d, kIOSurfacePlaneInfo, planes);
    s = IOSurfaceCreate(d);
    check(s && IOSurfaceGetPlaneCount(s) == 2, "NV12 has two planes");
    if (!s)
        return;
    check(IOSurfaceGetWidthOfPlane(s, 0) == 640 && IOSurfaceGetHeightOfPlane(s, 1) == 240 &&
          IOSurfaceGetBytesPerElementOfPlane(s, 1) == 2 && IOSurfaceGetBytesPerRowOfPlane(s, 1) >= 640,
          "NV12 plane geometry");
    check((uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 1) >=
          (uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 0) + IOSurfaceGetBytesPerRowOfPlane(s, 0) * 480,
          "the chroma plane follows the luma plane");
    // the same process looking a port up gets the same surface back
    port = IOSurfaceCreateMachPort(s);
    t = IOSurfaceLookupFromMachPort(port);
    check(t == s, "a lookup in the creating process returns the same surface");
    mach_port_deallocate(mach_task_self(), port);
    CFRelease(t), CFRelease(s), CFRelease(d), CFRelease(planes), CFRelease(y), CFRelease(uv);
}

int main(int argc, char **argv) {
    if (argc > 1)
        return child(argv[1], argc > 2 ? argv[2] : "0");
    shareOver("port", 0);
    shareOver("global", 1);
    nv12();
    printf("iosurface-test: %d failure%s\n", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
