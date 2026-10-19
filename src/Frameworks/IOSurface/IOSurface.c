// IOSurface as a CF type over a named Mach memory entry, passed to other processes as a Mach port

// the first page describes the surface, so the receiver needs only the port
// global surfaces are also registered with iosurfaced, which answers IOSurfaceLookup across processes
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "iosurfaced.h"

#define KEY(n) const CFStringRef kIOSurface##n = CFSTR("IOSurface" #n);
KEY(AllocSize) KEY(Width) KEY(Height) KEY(BytesPerRow) KEY(BytesPerElement) KEY(ElementWidth) KEY(ElementHeight)
KEY(Offset) KEY(PlaneInfo) KEY(PlaneWidth) KEY(PlaneHeight) KEY(PlaneBytesPerRow) KEY(PlaneOffset) KEY(PlaneSize)
KEY(PlaneBase) KEY(PlaneBitsPerElement) KEY(PlaneBytesPerElement) KEY(PlaneElementWidth) KEY(PlaneElementHeight)
KEY(CacheMode) KEY(IsGlobal) KEY(PixelFormat) KEY(PixelSizeCastingAllowed) KEY(Name)

typedef uint32_t IOSurfaceID;
typedef struct __IOSurface *IOSurfaceRef;
enum { kIOSurfaceLockReadOnly = 1, kIOSurfaceLockAvoidSync = 2 };
#define kIOReturnSuccess 0
#define kIOReturnBadArgument ((int)0xe00002c2)
#define MAX_PLANES 4

// the header every process maps, its layout is fixed and only grows at the end
#define HEADER_MAGIC 0x50444953u
#define HEADER_VERSION 1
#define HEADER_SIZE 4096

struct shared_plane {
    uint64_t width, height, bytesPerRow, bytesPerElement, elementWidth, elementHeight, offset, size;
};

struct shared_header {
    uint32_t magic, version;
    uint32_t id, pixelFormat;
    uint64_t width, height, bytesPerRow, bytesPerElement, elementWidth, elementHeight, allocSize, dataOffset;
    uint32_t nplanes, global;
    struct shared_plane planes[MAX_PLANES];
    _Atomic uint32_t seed;
    _Atomic int32_t lockCount;
    _Atomic int32_t useCount;
    _Atomic uint32_t valuesLock;
    uint32_t valuesLength, reserved;
    uint8_t values[];
};

#define VALUES_MAX (HEADER_SIZE - sizeof(struct shared_header))

struct __IOSurface {
    CFRuntimeBase base;
    struct shared_header *header;
    uint8_t *memory;
    mach_vm_address_t mapping;
    mach_vm_size_t mappedSize;
    mach_port_t entry;
    mach_port_t token;
    // values too large for the shared header stay with this process
    CFMutableDictionaryRef localValues;
};

static CFTypeID surfaceType;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableDictionaryRef byID;
static _Atomic uint32_t nextLocalID;
static mach_port_t daemonService = MACH_PORT_NULL;
static pthread_once_t daemonOnce = PTHREAD_ONCE_INIT;

static void forget(IOSurfaceRef s) {
    pthread_mutex_lock(&lock);
    if (byID && s->header && CFDictionaryGetValue(byID, (const void *)(uintptr_t)s->header->id) == s)
        CFDictionaryRemoveValue(byID, (const void *)(uintptr_t)s->header->id);
    pthread_mutex_unlock(&lock);
}

static void surfaceFinalize(CFTypeRef cf) {
    IOSurfaceRef s = (IOSurfaceRef)cf;

    forget(s);
    // dropping the token's receive right is what tells iosurfaced the global ID is gone
    if (s->token != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), s->token, MACH_PORT_RIGHT_RECEIVE, -1);
    if (s->mapping)
        mach_vm_deallocate(mach_task_self(), s->mapping, s->mappedSize);
    if (s->entry != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), s->entry);
    if (s->localValues)
        CFRelease(s->localValues);
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

static IOSurfaceRef newSurface(void) {
    IOSurfaceRef s = (IOSurfaceRef)_CFRuntimeCreateInstance(NULL, IOSurfaceGetTypeID(),
                                                           sizeof(struct __IOSurface) - sizeof(CFRuntimeBase), NULL);

    if (s)
        s->localValues = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return s;
}

static void remember(IOSurfaceRef s) {
    pthread_mutex_lock(&lock);
    if (!byID)
        byID = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(byID, (const void *)(uintptr_t)s->header->id, s);
    pthread_mutex_unlock(&lock);
}

// a surface this process already maps comes back as the same object
static IOSurfaceRef known(IOSurfaceID id) {
    IOSurfaceRef s = NULL;

    pthread_mutex_lock(&lock);
    if (byID && (s = (IOSurfaceRef)CFDictionaryGetValue(byID, (const void *)(uintptr_t)id)))
        CFRetain(s);
    pthread_mutex_unlock(&lock);
    return s;
}

static size_t number(CFDictionaryRef d, CFStringRef key, size_t fallback) {
    CFNumberRef n = d ? CFDictionaryGetValue(d, key) : NULL;
    long long v;

    if (n && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberLongLongType, &v) && v >= 0)
        return (size_t)v;
    return fallback;
}

static int flag(CFDictionaryRef d, CFStringRef key) {
    CFTypeRef v = d ? CFDictionaryGetValue(d, key) : NULL;

    if (v && CFGetTypeID(v) == CFBooleanGetTypeID())
        return CFBooleanGetValue(v);
    return number(d, key, 0) != 0;
}

static size_t align16(size_t n) {
    return (n + 15) & ~(size_t)15;
}

static size_t roundPage(size_t n) {
    return (n + vm_page_size - 1) & ~(size_t)(vm_page_size - 1);
}

static void findDaemon(void) {
    if (bootstrap_look_up(bootstrap_port, IOSURFACED_SERVICE, &daemonService) != KERN_SUCCESS)
        daemonService = MACH_PORT_NULL;
}

static mach_port_t daemonPort(void) {
    pthread_once(&daemonOnce, findDaemon);
    return daemonService;
}

// IDs made without iosurfaced carry the pid in the top bits, so they never meet the daemon's low IDs
static IOSurfaceID localID(void) {
    return 0x80000000u | ((uint32_t)getpid() & 0x7fff) << 16 | ((atomic_fetch_add(&nextLocalID, 1) + 1) & 0xffff);
}

// hands iosurfaced the memory and a token whose death retires the ID, returns 0 without a daemon
static IOSurfaceID registerGlobal(IOSurfaceRef s) {
    mach_port_t service = daemonPort(), reply = MACH_PORT_NULL;
    // mach_msg sends and receives through one buffer, so the request and its reply share it
    union {
        struct iosurfaced_register_request req;
        struct iosurfaced_reply rep;
    } m;
#define req m.req
#define rep m.rep

    if (service == MACH_PORT_NULL)
        return 0;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->token) != KERN_SUCCESS)
        return 0;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply) != KERN_SUCCESS)
        goto fail;
    memset(&req, 0, sizeof(req));
    req.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) | MACH_MSGH_BITS_COMPLEX;
    req.head.msgh_size = sizeof(req);
    req.head.msgh_remote_port = service;
    req.head.msgh_local_port = reply;
    req.head.msgh_id = IOSURFACED_REGISTER;
    req.body.msgh_descriptor_count = 2;
    req.token.name = s->token;
    req.token.disposition = MACH_MSG_TYPE_MAKE_SEND;
    req.token.type = MACH_MSG_PORT_DESCRIPTOR;
    req.entry.name = s->entry;
    req.entry.disposition = MACH_MSG_TYPE_COPY_SEND;
    req.entry.type = MACH_MSG_PORT_DESCRIPTOR;
    if (mach_msg(&req.head, MACH_SEND_MSG | MACH_RCV_MSG | MACH_RCV_TIMEOUT, sizeof(req), sizeof(m), reply,
                 2000, MACH_PORT_NULL) != MACH_MSG_SUCCESS || rep.head.msgh_id != IOSURFACED_REGISTER + 100 ||
        rep.result != KERN_SUCCESS)
        goto fail;
    mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    return rep.id;
#undef req
#undef rep
fail:
    if (reply != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    mach_port_mod_refs(mach_task_self(), s->token, MACH_PORT_RIGHT_RECEIVE, -1);
    s->token = MACH_PORT_NULL;
    return 0;
}

// maps a memory entry whose first page is a surface header, NULL if the port is not one
static IOSurfaceRef mapEntry(mach_port_t entry) {
    mach_vm_address_t peek = 0, addr = 0;
    struct shared_header *h;
    mach_vm_size_t size;
    IOSurfaceRef s;

    if (mach_vm_map(mach_task_self(), &peek, HEADER_SIZE, 0, VM_FLAGS_ANYWHERE, entry, 0, FALSE,
                    VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE) != KERN_SUCCESS)
        return NULL;
    h = (struct shared_header *)peek;
    if (h->magic != HEADER_MAGIC || h->version != HEADER_VERSION || h->dataOffset != HEADER_SIZE) {
        mach_vm_deallocate(mach_task_self(), peek, HEADER_SIZE);
        return NULL;
    }
    if ((s = known(h->id))) {
        mach_vm_deallocate(mach_task_self(), peek, HEADER_SIZE);
        return s;
    }
    size = roundPage(HEADER_SIZE + h->allocSize);
    mach_vm_deallocate(mach_task_self(), peek, HEADER_SIZE);
    if (mach_vm_map(mach_task_self(), &addr, size, 0, VM_FLAGS_ANYWHERE, entry, 0, FALSE,
                    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS)
        return NULL;
    if (!(s = newSurface())) {
        mach_vm_deallocate(mach_task_self(), addr, size);
        return NULL;
    }
    s->mapping = addr, s->mappedSize = size;
    s->header = (struct shared_header *)addr;
    s->memory = (uint8_t *)addr + HEADER_SIZE;
    // this object keeps its own send right, the caller still owns the one it passed in
    mach_port_mod_refs(mach_task_self(), entry, MACH_PORT_RIGHT_SEND, 1);
    s->entry = entry;
    remember(s);
    return s;
}

IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties) {
    CFArrayRef planes = properties ? CFDictionaryGetValue(properties, kIOSurfacePlaneInfo) : NULL;
    struct shared_header h;
    memory_object_size_t size;
    mach_vm_address_t addr = 0;
    IOSurfaceRef s;
    size_t total = 0;

    memset(&h, 0, sizeof(h));
    h.width = number(properties, kIOSurfaceWidth, 0), h.height = number(properties, kIOSurfaceHeight, 0);
    h.bytesPerElement = number(properties, kIOSurfaceBytesPerElement, 1);
    h.elementWidth = number(properties, kIOSurfaceElementWidth, 1), h.elementHeight = number(properties, kIOSurfaceElementHeight, 1);
    if (!h.elementWidth)
        h.elementWidth = 1;
    if (!h.elementHeight)
        h.elementHeight = 1;
    h.bytesPerRow = number(properties, kIOSurfaceBytesPerRow,
                           align16((h.width + h.elementWidth - 1) / h.elementWidth * h.bytesPerElement));
    h.pixelFormat = (uint32_t)number(properties, kIOSurfacePixelFormat, 0);
    if (planes && CFGetTypeID(planes) == CFArrayGetTypeID()) {
        CFIndex n = CFArrayGetCount(planes);
        for (CFIndex i = 0; i < n && i < MAX_PLANES; i++) {
            CFDictionaryRef p = CFArrayGetValueAtIndex(planes, i);
            struct shared_plane *pl = &h.planes[h.nplanes++];
            pl->width = number(p, kIOSurfacePlaneWidth, h.width), pl->height = number(p, kIOSurfacePlaneHeight, h.height);
            pl->bytesPerElement = number(p, kIOSurfacePlaneBytesPerElement, h.bytesPerElement);
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
        total = h.bytesPerRow * ((h.height + h.elementHeight - 1) / h.elementHeight);
    }
    h.allocSize = number(properties, kIOSurfaceAllocSize, total);
    if (h.allocSize < total)
        h.allocSize = total;
    if (h.width == 0 || h.height == 0 || h.allocSize == 0)
        return NULL;
    h.magic = HEADER_MAGIC, h.version = HEADER_VERSION, h.dataOffset = HEADER_SIZE;
    h.global = flag(properties, kIOSurfaceIsGlobal);

    if (!(s = newSurface()))
        return NULL;
    size = roundPage(HEADER_SIZE + h.allocSize);
    if (mach_make_memory_entry_64(mach_task_self(), &size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE,
                                  &s->entry, MACH_PORT_NULL) != KERN_SUCCESS) {
        s->entry = MACH_PORT_NULL;
        CFRelease(s);
        return NULL;
    }
    if (mach_vm_map(mach_task_self(), &addr, size, 0, VM_FLAGS_ANYWHERE, s->entry, 0, FALSE,
                    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS) {
        CFRelease(s);
        return NULL;
    }
    s->mapping = addr, s->mappedSize = size;
    s->header = (struct shared_header *)addr;
    s->memory = (uint8_t *)addr + HEADER_SIZE;
    memcpy(s->header, &h, sizeof(h));
    if (!h.global || !(s->header->id = registerGlobal(s)))
        s->header->id = localID();
    remember(s);
    return s;
}

IOSurfaceRef IOSurfaceLookup(IOSurfaceID csid) {
    IOSurfaceRef s = known(csid);
    mach_port_t service = daemonPort(), reply = MACH_PORT_NULL;
    union {
        struct iosurfaced_lookup_request req;
        struct iosurfaced_lookup_reply rep;
    } m;
#define req m.req
#define rep m.rep

    if (s || service == MACH_PORT_NULL || csid == 0)
        return s;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply) != KERN_SUCCESS)
        return NULL;
    memset(&req, 0, sizeof(req));
    req.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    req.head.msgh_size = sizeof(req);
    req.head.msgh_remote_port = service;
    req.head.msgh_local_port = reply;
    req.head.msgh_id = IOSURFACED_LOOKUP;
    req.id = csid;
    if (mach_msg(&req.head, MACH_SEND_MSG | MACH_RCV_MSG | MACH_RCV_TIMEOUT, sizeof(req), sizeof(m), reply,
                 2000, MACH_PORT_NULL) == MACH_MSG_SUCCESS && (rep.head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        rep.body.msgh_descriptor_count == 1 && rep.entry.name != MACH_PORT_NULL) {
        s = mapEntry(rep.entry.name);
        mach_port_deallocate(mach_task_self(), rep.entry.name);
    }
    mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    return s;
#undef req
#undef rep
}

// a new send right the caller owns and may pass to any process
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef s) {
    if (!s || s->entry == MACH_PORT_NULL)
        return MACH_PORT_NULL;
    if (mach_port_mod_refs(mach_task_self(), s->entry, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS)
        return MACH_PORT_NULL;
    return s->entry;
}

IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t port) {
    return port == MACH_PORT_NULL ? NULL : mapEntry(port);
}

IOSurfaceID IOSurfaceGetID(IOSurfaceRef s) { return s ? s->header->id : 0; }
size_t IOSurfaceGetWidth(IOSurfaceRef s) { return s ? (size_t)s->header->width : 0; }
size_t IOSurfaceGetHeight(IOSurfaceRef s) { return s ? (size_t)s->header->height : 0; }
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef s) { return s ? (size_t)s->header->bytesPerRow : 0; }
size_t IOSurfaceGetBytesPerElement(IOSurfaceRef s) { return s ? (size_t)s->header->bytesPerElement : 0; }
size_t IOSurfaceGetElementWidth(IOSurfaceRef s) { return s ? (size_t)s->header->elementWidth : 0; }
size_t IOSurfaceGetElementHeight(IOSurfaceRef s) { return s ? (size_t)s->header->elementHeight : 0; }
size_t IOSurfaceGetAllocSize(IOSurfaceRef s) { return s ? (size_t)s->header->allocSize : 0; }
uint32_t IOSurfaceGetPixelFormat(IOSurfaceRef s) { return s ? s->header->pixelFormat : 0; }
uint32_t IOSurfaceGetSeed(IOSurfaceRef s) { return s ? atomic_load(&s->header->seed) : 0; }
void *IOSurfaceGetBaseAddress(IOSurfaceRef s) { return s ? s->memory : NULL; }
size_t IOSurfaceGetPlaneCount(IOSurfaceRef s) { return s ? s->header->nplanes : 0; }

// a surface without planes answers for plane 0 as itself, as macOS does
#define PLANE_GETTER(name, field)                                                       \
    size_t IOSurfaceGet##name##OfPlane(IOSurfaceRef s, size_t i) {                      \
        if (!s)                                                                         \
            return 0;                                                                   \
        if (s->header->nplanes == 0)                                                    \
            return i == 0 ? (size_t)s->header->field : 0;                               \
        return i < s->header->nplanes ? (size_t)s->header->planes[i].field : 0;         \
    }
PLANE_GETTER(Width, width)
PLANE_GETTER(Height, height)
PLANE_GETTER(BytesPerRow, bytesPerRow)
PLANE_GETTER(BytesPerElement, bytesPerElement)
PLANE_GETTER(ElementWidth, elementWidth)
PLANE_GETTER(ElementHeight, elementHeight)

void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef s, size_t i) {
    if (!s)
        return NULL;
    if (s->header->nplanes == 0)
        return i == 0 ? s->memory : NULL;
    return i < s->header->nplanes ? s->memory + s->header->planes[i].offset : NULL;
}

// memory is coherent between processes here, so a lock only counts and a write unlock moves the seed
int IOSurfaceLock(IOSurfaceRef s, uint32_t options, uint32_t *seed) {
    if (!s)
        return kIOReturnBadArgument;
    atomic_fetch_add(&s->header->lockCount, 1);
    if (seed)
        *seed = atomic_load(&s->header->seed);
    return kIOReturnSuccess;
}

int IOSurfaceUnlock(IOSurfaceRef s, uint32_t options, uint32_t *seed) {
    if (!s)
        return kIOReturnBadArgument;
    if (atomic_fetch_sub(&s->header->lockCount, 1) <= 0) {
        atomic_fetch_add(&s->header->lockCount, 1);
        return kIOReturnBadArgument;
    }
    if (!(options & kIOSurfaceLockReadOnly))
        atomic_fetch_add(&s->header->seed, 1);
    if (seed)
        *seed = atomic_load(&s->header->seed);
    return kIOReturnSuccess;
}

// the use count lives in the shared header, so every process sees the others' uses
void IOSurfaceIncrementUseCount(IOSurfaceRef s) { if (s) atomic_fetch_add(&s->header->useCount, 1); }
void IOSurfaceDecrementUseCount(IOSurfaceRef s) { if (s) atomic_fetch_sub(&s->header->useCount, 1); }
int32_t IOSurfaceGetUseCount(IOSurfaceRef s) { return s ? atomic_load(&s->header->useCount) : 0; }
int IOSurfaceIsInUse(IOSurfaceRef s) { return s && atomic_load(&s->header->useCount) > 0; }

static void valuesLock(IOSurfaceRef s) {
    uint32_t expected = 0;

    while (!atomic_compare_exchange_weak(&s->header->valuesLock, &expected, 1)) {
        expected = 0;
        sched_yield();
    }
}

static void valuesUnlock(IOSurfaceRef s) {
    atomic_store(&s->header->valuesLock, 0);
}

// the values every process sees, kept as a binary property list in the header
static CFMutableDictionaryRef readShared(IOSurfaceRef s) {
    CFMutableDictionaryRef d = NULL;

    if (s->header->valuesLength > 0 && s->header->valuesLength <= VALUES_MAX) {
        CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, s->header->values, s->header->valuesLength, kCFAllocatorNull);
        CFPropertyListRef p = data ? CFPropertyListCreateWithData(NULL, data, kCFPropertyListMutableContainers, NULL, NULL) : NULL;

        if (data)
            CFRelease(data);
        if (p && CFGetTypeID(p) == CFDictionaryGetTypeID())
            d = (CFMutableDictionaryRef)p;
        else if (p)
            CFRelease(p);
    }
    if (!d)
        d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return d;
}

static int writeShared(IOSurfaceRef s, CFDictionaryRef d) {
    CFDataRef data = CFPropertyListCreateData(NULL, d, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    int ok = data && (size_t)CFDataGetLength(data) <= VALUES_MAX;

    if (ok) {
        memcpy(s->header->values, CFDataGetBytePtr(data), CFDataGetLength(data));
        s->header->valuesLength = (uint32_t)CFDataGetLength(data);
    }
    if (data)
        CFRelease(data);
    return ok;
}

// a value that is not a property list or does not fit stays in this process only
void IOSurfaceSetValue(IOSurfaceRef s, CFStringRef key, CFTypeRef value) {
    CFMutableDictionaryRef d;

    if (!s || !key || !value)
        return;
    valuesLock(s);
    d = readShared(s);
    CFDictionarySetValue(d, key, value);
    if (CFPropertyListIsValid(d, kCFPropertyListBinaryFormat_v1_0) && writeShared(s, d))
        CFDictionaryRemoveValue(s->localValues, key);
    else
        CFDictionarySetValue(s->localValues, key, value);
    valuesUnlock(s);
    CFRelease(d);
}

CFTypeRef IOSurfaceCopyValue(IOSurfaceRef s, CFStringRef key) {
    CFMutableDictionaryRef d;
    CFTypeRef v;

    if (!s || !key)
        return NULL;
    if ((v = CFDictionaryGetValue(s->localValues, key)))
        return CFRetain(v);
    valuesLock(s);
    d = readShared(s);
    valuesUnlock(s);
    if ((v = CFDictionaryGetValue(d, key)))
        CFRetain(v);
    CFRelease(d);
    return v;
}

void IOSurfaceRemoveValue(IOSurfaceRef s, CFStringRef key) {
    CFMutableDictionaryRef d;

    if (!s || !key)
        return;
    CFDictionaryRemoveValue(s->localValues, key);
    valuesLock(s);
    d = readShared(s);
    if (CFDictionaryContainsKey(d, key)) {
        CFDictionaryRemoveValue(d, key);
        writeShared(s, d);
    }
    valuesUnlock(s);
    CFRelease(d);
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
