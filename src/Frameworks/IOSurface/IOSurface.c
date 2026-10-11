// IOSurface for PureDarwin applications: a CoreFoundation type over memory of this process, with Apple's
// properties, planes, locking and lookup by ID. Surfaces are not shared between processes yet (the kernel's
// IOSurfaceRoot serves Apple's own IOSurface framework, not this one)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define KEY(n) const CFStringRef kIOSurface##n = CFSTR("IOSurface" #n);
KEY(AllocSize) KEY(Width) KEY(Height) KEY(BytesPerRow) KEY(BytesPerElement) KEY(ElementWidth) KEY(ElementHeight)
KEY(Offset) KEY(PlaneInfo) KEY(PlaneWidth) KEY(PlaneHeight) KEY(PlaneBytesPerRow) KEY(PlaneOffset) KEY(PlaneSize)
KEY(PlaneBase) KEY(PlaneBitsPerElement) KEY(PlaneBytesPerElement) KEY(PlaneElementWidth) KEY(PlaneElementHeight)
KEY(CacheMode) KEY(IsGlobal) KEY(PixelFormat) KEY(PixelSizeCastingAllowed) KEY(Name)

typedef uint32_t IOSurfaceID;
typedef struct __IOSurface *IOSurfaceRef;
enum { kIOSurfaceLockReadOnly = 1, kIOSurfaceLockAvoidSync = 2 };
#define MAX_PLANES 4

struct plane {
    size_t width, height, bytesPerRow, bytesPerElement, elementWidth, elementHeight, offset, size;
};

struct __IOSurface {
    CFRuntimeBase base;
    IOSurfaceID id;
    size_t width, height, bytesPerRow, bytesPerElement, elementWidth, elementHeight, allocSize;
    uint32_t pixelFormat, seed;
    int32_t useCount, lockCount;
    unsigned nplanes;
    struct plane planes[MAX_PLANES];
    uint8_t *memory;
    CFMutableDictionaryRef values;
};

static CFTypeID surfaceType;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableDictionaryRef byID;
static IOSurfaceID nextID = 1;

static void surfaceFinalize(CFTypeRef cf) {
    IOSurfaceRef s = (IOSurfaceRef)cf;

    pthread_mutex_lock(&lock);
    if (byID)
        CFDictionaryRemoveValue(byID, (const void *)(uintptr_t)s->id);
    pthread_mutex_unlock(&lock);
    free(s->memory);
    if (s->values)
        CFRelease(s->values);
}

static const CFRuntimeClass surfaceClass = { 0, "IOSurface", NULL, NULL, surfaceFinalize, NULL, NULL, NULL, NULL };

static void registerType(void) {
    surfaceType = _CFRuntimeRegisterClass(&surfaceClass);
}

CFTypeID IOSurfaceGetTypeID(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once(&once, registerType);
    return surfaceType;
}

static size_t number(CFDictionaryRef d, CFStringRef key, size_t fallback) {
    CFNumberRef n = d ? CFDictionaryGetValue(d, key) : NULL;
    long long v;

    if (n && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberLongLongType, &v) && v >= 0)
        return (size_t)v;
    return fallback;
}

static size_t align16(size_t n) {
    return (n + 15) & ~(size_t)15;
}

IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties) {
    IOSurfaceRef s;
    CFArrayRef planes = properties ? CFDictionaryGetValue(properties, kIOSurfacePlaneInfo) : NULL;
    size_t total = 0;

    s = (IOSurfaceRef)_CFRuntimeCreateInstance(NULL, IOSurfaceGetTypeID(), sizeof(*s) - sizeof(CFRuntimeBase), NULL);
    if (!s)
        return NULL;
    s->width = number(properties, kIOSurfaceWidth, 0), s->height = number(properties, kIOSurfaceHeight, 0);
    s->bytesPerElement = number(properties, kIOSurfaceBytesPerElement, 1);
    s->elementWidth = number(properties, kIOSurfaceElementWidth, 1), s->elementHeight = number(properties, kIOSurfaceElementHeight, 1);
    if (!s->elementWidth)
        s->elementWidth = 1;
    if (!s->elementHeight)
        s->elementHeight = 1;
    s->bytesPerRow = number(properties, kIOSurfaceBytesPerRow,
                            align16((s->width + s->elementWidth - 1) / s->elementWidth * s->bytesPerElement));
    s->pixelFormat = (uint32_t)number(properties, kIOSurfacePixelFormat, 0);
    if (planes && CFGetTypeID(planes) == CFArrayGetTypeID()) {
        CFIndex n = CFArrayGetCount(planes);
        for (CFIndex i = 0; i < n && i < MAX_PLANES; i++) {
            CFDictionaryRef p = CFArrayGetValueAtIndex(planes, i);
            struct plane *pl = &s->planes[s->nplanes++];
            pl->width = number(p, kIOSurfacePlaneWidth, s->width), pl->height = number(p, kIOSurfacePlaneHeight, s->height);
            pl->bytesPerElement = number(p, kIOSurfacePlaneBytesPerElement, s->bytesPerElement);
            pl->elementWidth = number(p, kIOSurfacePlaneElementWidth, 1), pl->elementHeight = number(p, kIOSurfacePlaneElementHeight, 1);
            if (!pl->elementWidth)
                pl->elementWidth = 1;
            if (!pl->elementHeight)
                pl->elementHeight = 1;
            pl->bytesPerRow = number(p, kIOSurfacePlaneBytesPerRow,
                                     align16((pl->width + pl->elementWidth - 1) / pl->elementWidth * pl->bytesPerElement));
            pl->offset = number(p, kIOSurfacePlaneOffset, total);
            pl->size = number(p, kIOSurfacePlaneSize, pl->bytesPerRow * ((pl->height + pl->elementHeight - 1) / pl->elementHeight));
            if (pl->offset + pl->size > total)
                total = pl->offset + pl->size;
        }
    } else {
        total = s->bytesPerRow * ((s->height + s->elementHeight - 1) / s->elementHeight);
    }
    s->allocSize = number(properties, kIOSurfaceAllocSize, total);
    if (s->allocSize < total)
        s->allocSize = total;
    s->memory = calloc(1, s->allocSize ? s->allocSize : 1);
    s->values = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!s->memory || !s->values) {
        CFRelease(s);
        return NULL;
    }
    // lookup by ID does not keep the surface alive, as on macOS
    pthread_mutex_lock(&lock);
    if (!byID)
        byID = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    s->id = nextID++;
    CFDictionarySetValue(byID, (const void *)(uintptr_t)s->id, s);
    pthread_mutex_unlock(&lock);
    return s;
}

IOSurfaceRef IOSurfaceLookup(IOSurfaceID csid) {
    IOSurfaceRef s = NULL;

    pthread_mutex_lock(&lock);
    if (byID && (s = (IOSurfaceRef)CFDictionaryGetValue(byID, (const void *)(uintptr_t)csid)))
        CFRetain(s);
    pthread_mutex_unlock(&lock);
    return s;
}

IOSurfaceID IOSurfaceGetID(IOSurfaceRef s) { return s ? s->id : 0; }
size_t IOSurfaceGetWidth(IOSurfaceRef s) { return s ? s->width : 0; }
size_t IOSurfaceGetHeight(IOSurfaceRef s) { return s ? s->height : 0; }
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef s) { return s ? s->bytesPerRow : 0; }
size_t IOSurfaceGetBytesPerElement(IOSurfaceRef s) { return s ? s->bytesPerElement : 0; }
size_t IOSurfaceGetElementWidth(IOSurfaceRef s) { return s ? s->elementWidth : 0; }
size_t IOSurfaceGetElementHeight(IOSurfaceRef s) { return s ? s->elementHeight : 0; }
size_t IOSurfaceGetAllocSize(IOSurfaceRef s) { return s ? s->allocSize : 0; }
uint32_t IOSurfaceGetPixelFormat(IOSurfaceRef s) { return s ? s->pixelFormat : 0; }
uint32_t IOSurfaceGetSeed(IOSurfaceRef s) { return s ? s->seed : 0; }
void *IOSurfaceGetBaseAddress(IOSurfaceRef s) { return s ? s->memory : NULL; }
size_t IOSurfaceGetPlaneCount(IOSurfaceRef s) { return s ? s->nplanes : 0; }

#define PLANE_GETTER(name, field)                                         \
    size_t IOSurfaceGet##name##OfPlane(IOSurfaceRef s, size_t i) {        \
        return s && i < s->nplanes ? s->planes[i].field : 0;              \
    }
PLANE_GETTER(Width, width)
PLANE_GETTER(Height, height)
PLANE_GETTER(BytesPerRow, bytesPerRow)
PLANE_GETTER(BytesPerElement, bytesPerElement)
PLANE_GETTER(ElementWidth, elementWidth)
PLANE_GETTER(ElementHeight, elementHeight)

void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef s, size_t i) {
    return s && i < s->nplanes ? s->memory + s->planes[i].offset : NULL;
}

// one process holds the memory: locking only counts, and a write lock moves the seed
int IOSurfaceLock(IOSurfaceRef s, uint32_t options, uint32_t *seed) {
    if (!s)
        return -1;
    __atomic_add_fetch(&s->lockCount, 1, __ATOMIC_SEQ_CST);
    if (seed)
        *seed = s->seed;
    return 0;
}

int IOSurfaceUnlock(IOSurfaceRef s, uint32_t options, uint32_t *seed) {
    if (!s || __atomic_sub_fetch(&s->lockCount, 1, __ATOMIC_SEQ_CST) < 0)
        return -1;
    if (!(options & kIOSurfaceLockReadOnly))
        __atomic_add_fetch(&s->seed, 1, __ATOMIC_SEQ_CST);
    if (seed)
        *seed = s->seed;
    return 0;
}

void IOSurfaceIncrementUseCount(IOSurfaceRef s) { if (s) __atomic_add_fetch(&s->useCount, 1, __ATOMIC_SEQ_CST); }
void IOSurfaceDecrementUseCount(IOSurfaceRef s) { if (s) __atomic_sub_fetch(&s->useCount, 1, __ATOMIC_SEQ_CST); }
int32_t IOSurfaceGetUseCount(IOSurfaceRef s) { return s ? s->useCount : 0; }
int IOSurfaceIsInUse(IOSurfaceRef s) { return s && s->useCount > 0; }

// the app's own values attached to the surface
void IOSurfaceSetValue(IOSurfaceRef s, CFStringRef key, CFTypeRef value) {
    if (s && key && value)
        CFDictionarySetValue(s->values, key, value);
}

CFTypeRef IOSurfaceCopyValue(IOSurfaceRef s, CFStringRef key) {
    CFTypeRef v = s && key ? CFDictionaryGetValue(s->values, key) : NULL;
    return v ? CFRetain(v) : NULL;
}

void IOSurfaceRemoveValue(IOSurfaceRef s, CFStringRef key) {
    if (s && key)
        CFDictionaryRemoveValue(s->values, key);
}

size_t IOSurfaceGetPropertyMaximum(CFStringRef property) {
    return 16384;
}

size_t IOSurfaceGetPropertyAlignment(CFStringRef property) {
    return CFEqual(property, kIOSurfaceBytesPerRow) ? 16 : 1;
}

size_t IOSurfaceAlignProperty(CFStringRef property, size_t value) {
    size_t a = IOSurfaceGetPropertyAlignment(property);
    return (value + a - 1) / a * a;
}
