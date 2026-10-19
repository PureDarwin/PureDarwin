// SPDX-License-Identifier: MIT
// CVPixelBuffer as a CF type over its own memory, caller bytes or an IOSurface
// with the packed, bi-planar and tri-planar plane layouts video decoders produce
#include <CoreVideo/CVPixelBuffer.h>
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t IOSurfaceID;
typedef struct __IOSurface *IOSurfaceRef;
extern IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties);
extern size_t IOSurfaceGetWidth(IOSurfaceRef);
extern size_t IOSurfaceGetHeight(IOSurfaceRef);
extern size_t IOSurfaceGetBytesPerRow(IOSurfaceRef);
extern size_t IOSurfaceGetAllocSize(IOSurfaceRef);
extern uint32_t IOSurfaceGetPixelFormat(IOSurfaceRef);
extern void *IOSurfaceGetBaseAddress(IOSurfaceRef);
extern size_t IOSurfaceGetPlaneCount(IOSurfaceRef);
extern size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef, size_t);
extern size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef, size_t);
extern size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef, size_t);
extern void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef, size_t);
extern int IOSurfaceLock(IOSurfaceRef, uint32_t, uint32_t *);
extern int IOSurfaceUnlock(IOSurfaceRef, uint32_t, uint32_t *);
extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfacePixelFormat, kIOSurfaceBytesPerElement,
    kIOSurfaceBytesPerRow, kIOSurfacePlaneInfo, kIOSurfacePlaneWidth, kIOSurfacePlaneHeight, kIOSurfacePlaneBytesPerRow,
    kIOSurfacePlaneBytesPerElement, kIOSurfacePlaneOffset, kIOSurfacePlaneSize, kIOSurfaceAllocSize;

#define KEY(name, value) const CFStringRef name = CFSTR(value);
KEY(kCVBufferPropagatedAttachmentsKey, "PropagatedAttachments")
KEY(kCVBufferNonPropagatedAttachmentsKey, "NonPropagatedAttachments")
KEY(kCVImageBufferCGColorSpaceKey, "CGColorSpace")
KEY(kCVImageBufferChromaLocationTopFieldKey, "CVImageBufferChromaLocationTopField")
KEY(kCVImageBufferChromaLocationBottomFieldKey, "CVImageBufferChromaLocationBottomField")
KEY(kCVImageBufferChromaLocation_Left, "Left")
KEY(kCVImageBufferChromaLocation_Center, "Center")
KEY(kCVImageBufferChromaLocation_TopLeft, "TopLeft")
KEY(kCVImageBufferColorPrimariesKey, "CVImageBufferColorPrimaries")
KEY(kCVImageBufferColorPrimaries_ITU_R_709_2, "ITU_R_709_2")
KEY(kCVImageBufferColorPrimaries_EBU_3213, "EBU_3213")
KEY(kCVImageBufferColorPrimaries_SMPTE_C, "SMPTE_C")
KEY(kCVImageBufferColorPrimaries_P22, "P22")
KEY(kCVImageBufferColorPrimaries_DCI_P3, "DCI_P3")
KEY(kCVImageBufferColorPrimaries_P3_D65, "P3_D65")
KEY(kCVImageBufferColorPrimaries_ITU_R_2020, "ITU_R_2020")
KEY(kCVImageBufferTransferFunctionKey, "CVImageBufferTransferFunction")
KEY(kCVImageBufferTransferFunction_ITU_R_709_2, "ITU_R_709_2")
KEY(kCVImageBufferTransferFunction_SMPTE_240M_1995, "SMPTE_240M_1995")
KEY(kCVImageBufferTransferFunction_UseGamma, "UseGamma")
KEY(kCVImageBufferTransferFunction_sRGB, "IEC_sRGB")
KEY(kCVImageBufferTransferFunction_Linear, "Linear")
KEY(kCVImageBufferTransferFunction_ITU_R_2020, "ITU_R_2020")
KEY(kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ, "SMPTE_ST_2084_PQ")
KEY(kCVImageBufferTransferFunction_ITU_R_2100_HLG, "ITU_R_2100_HLG")
KEY(kCVImageBufferYCbCrMatrixKey, "CVImageBufferYCbCrMatrix")
KEY(kCVImageBufferYCbCrMatrix_ITU_R_709_2, "ITU_R_709_2")
KEY(kCVImageBufferYCbCrMatrix_ITU_R_601_4, "ITU_R_601_4")
KEY(kCVImageBufferYCbCrMatrix_SMPTE_240M_1995, "SMPTE_240M_1995")
KEY(kCVImageBufferYCbCrMatrix_ITU_R_2020, "ITU_R_2020")
KEY(kCVImageBufferMasteringDisplayColorVolumeKey, "MasteringDisplayColorVolume")
KEY(kCVImageBufferContentLightLevelInfoKey, "ContentLightLevelInfo")
KEY(kCVPixelBufferPixelFormatTypeKey, "PixelFormatType")
KEY(kCVPixelBufferWidthKey, "Width")
KEY(kCVPixelBufferHeightKey, "Height")
KEY(kCVPixelBufferBytesPerRowAlignmentKey, "BytesPerRowAlignment")
KEY(kCVPixelBufferIOSurfacePropertiesKey, "IOSurfaceProperties")
KEY(kCVPixelBufferOpenGLCompatibilityKey, "OpenGLCompatibility")
KEY(kCVPixelBufferMetalCompatibilityKey, "MetalCompatibility")
KEY(kCVPixelBufferCGImageCompatibilityKey, "CGImageCompatibility")
KEY(kCVPixelBufferCGBitmapContextCompatibilityKey, "CGBitmapContextCompatibility")

#define MAX_PLANES 3

// a format's planes: bytes per pixel and the subsampling of each
struct format {
    OSType type;
    unsigned planes;
    struct { unsigned bytes, xshift, yshift; } plane[MAX_PLANES];
};

static const struct format formats[] = {
    { kCVPixelFormatType_32ARGB, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_32BGRA, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_32ABGR, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_32RGBA, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_24RGB, 1, { { 3, 0, 0 } } },
    { kCVPixelFormatType_ARGB2101010LEPacked, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_30RGB, 1, { { 4, 0, 0 } } },
    { kCVPixelFormatType_64RGBAHalf, 1, { { 8, 0, 0 } } },
    { kCVPixelFormatType_128RGBAFloat, 1, { { 16, 0, 0 } } },
    { kCVPixelFormatType_OneComponent8, 1, { { 1, 0, 0 } } },
    { kCVPixelFormatType_OneComponent16, 1, { { 2, 0, 0 } } },
    { kCVPixelFormatType_OneComponent16Half, 1, { { 2, 0, 0 } } },
    { kCVPixelFormatType_TwoComponent8, 1, { { 2, 0, 0 } } },
    { kCVPixelFormatType_422YpCbCr8, 1, { { 2, 0, 0 } } },
    { kCVPixelFormatType_422YpCbCr8_yuvs, 1, { { 2, 0, 0 } } },
    { kCVPixelFormatType_420YpCbCr8Planar, 3, { { 1, 0, 0 }, { 1, 1, 1 }, { 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8PlanarFullRange, 3, { { 1, 0, 0 }, { 1, 1, 1 }, { 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, 2, { { 1, 0, 0 }, { 2, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, 2, { { 1, 0, 0 }, { 2, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, 2, { { 2, 0, 0 }, { 4, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, 2, { { 2, 0, 0 }, { 4, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange, 2, { { 1, 0, 0 }, { 2, 1, 0 } } },
    { kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange, 2, { { 1, 0, 0 }, { 2, 0, 0 } } },
};

static const struct format *formatFor(OSType type)
{
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        if (formats[i].type == type)
            return &formats[i];
    }
    return NULL;
}

struct plane {
    uint8_t *base;
    size_t width, height, bytesPerRow;
};

struct __CVBuffer {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CFMutableDictionaryRef attachments[2];
    size_t width, height, dataSize;
    OSType format;
    unsigned planes;
    struct plane plane[MAX_PLANES];
    IOSurfaceRef surface;
    void *owned;
    // caller memory and how to give it back
    CVPixelBufferReleaseBytesCallback releaseBytes;
    CVPixelBufferReleasePlanarBytesCallback releasePlanar;
    void *releaseRefCon, *dataPtr;
};

static CFTypeID sPixelBufferType;

static void bufferFinalize(CFTypeRef cf)
{
    CVBufferRef b = (CVBufferRef)cf;

    for (int i = 0; i < 2; i++) {
        if (b->attachments[i])
            CFRelease(b->attachments[i]);
    }
    if (b->releaseBytes)
        b->releaseBytes(b->releaseRefCon, b->plane[0].base);
    if (b->releasePlanar) {
        const void *addresses[MAX_PLANES];
        for (unsigned i = 0; i < b->planes; i++)
            addresses[i] = b->plane[i].base;
        b->releasePlanar(b->releaseRefCon, b->dataPtr, b->dataSize, b->planes, addresses);
    }
    if (b->surface)
        CFRelease(b->surface);
    free(b->owned);
    pthread_mutex_destroy(&b->lock);
}

static CFStringRef bufferDescription(CFTypeRef cf)
{
    CVBufferRef b = (CVBufferRef)cf;
    OSType f = b->format;
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CVPixelBuffer %p width=%zu height=%zu format='%c%c%c%c' planes=%u%s>"),
                                    b, b->width, b->height, (char)(f >> 24), (char)(f >> 16), (char)(f >> 8), (char)f,
                                    b->planes, b->surface ? " iosurface" : "");
}

static const CFRuntimeClass sPixelBufferClass = { 0, "CVPixelBuffer", NULL, NULL, bufferFinalize, NULL, NULL, NULL, bufferDescription };

static void registerClass(void)
{
    sPixelBufferType = _CFRuntimeRegisterClass(&sPixelBufferClass);
}

CFTypeID CVPixelBufferGetTypeID(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, registerClass);
    return sPixelBufferType;
}

static Boolean isBuffer(CFTypeRef cf)
{
    return cf != NULL && CFGetTypeID(cf) == CVPixelBufferGetTypeID();
}

static CVBufferRef createBuffer(CFAllocatorRef allocator, size_t width, size_t height, OSType format)
{
    CVBufferRef b = (CVBufferRef)_CFRuntimeCreateInstance(allocator, CVPixelBufferGetTypeID(),
                                                          sizeof(*b) - sizeof(CFRuntimeBase), NULL);
    if (b == NULL)
        return NULL;
    pthread_mutex_init(&b->lock, NULL);
    for (int i = 0; i < 2; i++)
        b->attachments[i] = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    b->width = width;
    b->height = height;
    b->format = format;
    return b;
}

// attachments: one dictionary per propagation mode, a key lives in only one of them
CVBufferRef CVBufferRetain(CVBufferRef buffer)
{
    return buffer ? (CVBufferRef)CFRetain(buffer) : NULL;
}

void CVBufferRelease(CVBufferRef buffer)
{
    if (buffer)
        CFRelease(buffer);
}

void CVBufferSetAttachment(CVBufferRef buffer, CFStringRef key, CFTypeRef value, CVAttachmentMode mode)
{
    if (!isBuffer(buffer) || key == NULL || value == NULL || mode > kCVAttachmentMode_ShouldPropagate)
        return;
    pthread_mutex_lock(&buffer->lock);
    CFDictionaryRemoveValue(buffer->attachments[!mode], key);
    CFDictionarySetValue(buffer->attachments[mode], key, value);
    pthread_mutex_unlock(&buffer->lock);
}

CFTypeRef CVBufferGetAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode)
{
    CFTypeRef value = NULL;

    if (!isBuffer(buffer) || key == NULL)
        return NULL;
    pthread_mutex_lock(&buffer->lock);
    for (CVAttachmentMode m = 0; m <= kCVAttachmentMode_ShouldPropagate && value == NULL; m++) {
        value = CFDictionaryGetValue(buffer->attachments[m], key);
        if (value && mode)
            *mode = m;
    }
    pthread_mutex_unlock(&buffer->lock);
    return value;
}

CFTypeRef CVBufferCopyAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode)
{
    CFTypeRef value = CVBufferGetAttachment(buffer, key, mode);
    return value ? CFRetain(value) : NULL;
}

void CVBufferRemoveAttachment(CVBufferRef buffer, CFStringRef key)
{
    if (!isBuffer(buffer) || key == NULL)
        return;
    pthread_mutex_lock(&buffer->lock);
    CFDictionaryRemoveValue(buffer->attachments[0], key);
    CFDictionaryRemoveValue(buffer->attachments[1], key);
    pthread_mutex_unlock(&buffer->lock);
}

void CVBufferRemoveAllAttachments(CVBufferRef buffer)
{
    if (!isBuffer(buffer))
        return;
    pthread_mutex_lock(&buffer->lock);
    CFDictionaryRemoveAllValues(buffer->attachments[0]);
    CFDictionaryRemoveAllValues(buffer->attachments[1]);
    pthread_mutex_unlock(&buffer->lock);
}

CFDictionaryRef CVBufferGetAttachments(CVBufferRef buffer, CVAttachmentMode mode)
{
    if (!isBuffer(buffer) || mode > kCVAttachmentMode_ShouldPropagate)
        return NULL;
    return CFDictionaryGetCount(buffer->attachments[mode]) ? buffer->attachments[mode] : NULL;
}

CFDictionaryRef CVBufferCopyAttachments(CVBufferRef buffer, CVAttachmentMode mode)
{
    if (!isBuffer(buffer) || mode > kCVAttachmentMode_ShouldPropagate)
        return NULL;
    pthread_mutex_lock(&buffer->lock);
    CFDictionaryRef copy = CFDictionaryGetCount(buffer->attachments[mode]) ? CFDictionaryCreateCopy(NULL, buffer->attachments[mode]) : NULL;
    pthread_mutex_unlock(&buffer->lock);
    return copy;
}

static void setOne(const void *key, const void *value, void *context)
{
    void **args = context;
    CVBufferSetAttachment(args[0], key, value, (CVAttachmentMode)(uintptr_t)args[1]);
}

void CVBufferSetAttachments(CVBufferRef buffer, CFDictionaryRef attachments, CVAttachmentMode mode)
{
    void *args[2] = { buffer, (void *)(uintptr_t)mode };
    if (isBuffer(buffer) && attachments)
        CFDictionaryApplyFunction(attachments, setOne, args);
}

void CVBufferPropagateAttachments(CVBufferRef source, CVBufferRef destination)
{
    CFDictionaryRef copy = CVBufferCopyAttachments(source, kCVAttachmentMode_ShouldPropagate);
    if (copy) {
        CVBufferSetAttachments(destination, copy, kCVAttachmentMode_ShouldPropagate);
        CFRelease(copy);
    }
}

static size_t numberFrom(CFDictionaryRef d, CFStringRef key, size_t fallback)
{
    CFNumberRef n = d ? CFDictionaryGetValue(d, key) : NULL;
    long long v;
    if (n && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberLongLongType, &v) && v > 0)
        return (size_t)v;
    return fallback;
}

static size_t roundUp(size_t n, size_t a)
{
    return (n + a - 1) / a * a;
}

static CFNumberRef num(size_t v)
{
    long long l = (long long)v;
    return CFNumberCreate(NULL, kCFNumberLongLongType, &l);
}

static void put(CFMutableDictionaryRef d, CFStringRef key, size_t v)
{
    CFNumberRef n = num(v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

// the plane geometry for a format, laid out back to back with aligned rows
static size_t layout(const struct format *f, size_t width, size_t height, size_t align, struct plane *planes, size_t *offsets)
{
    size_t total = 0;
    for (unsigned i = 0; i < f->planes; i++) {
        planes[i].width = (width + (1u << f->plane[i].xshift) - 1) >> f->plane[i].xshift;
        planes[i].height = (height + (1u << f->plane[i].yshift) - 1) >> f->plane[i].yshift;
        planes[i].bytesPerRow = roundUp(planes[i].width * f->plane[i].bytes, align);
        offsets[i] = total;
        total += planes[i].bytesPerRow * planes[i].height;
    }
    return total;
}

static void adoptSurface(CVBufferRef b, IOSurfaceRef surface)
{
    b->surface = (IOSurfaceRef)CFRetain(surface);
    b->dataSize = IOSurfaceGetAllocSize(surface);
    size_t n = IOSurfaceGetPlaneCount(surface);
    if (n == 0) {
        b->planes = 1;
        b->plane[0] = (struct plane){ IOSurfaceGetBaseAddress(surface), IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface),
                                      IOSurfaceGetBytesPerRow(surface) };
        return;
    }
    b->planes = n > MAX_PLANES ? MAX_PLANES : (unsigned)n;
    for (unsigned i = 0; i < b->planes; i++) {
        b->plane[i] = (struct plane){ IOSurfaceGetBaseAddressOfPlane(surface, i), IOSurfaceGetWidthOfPlane(surface, i),
                                      IOSurfaceGetHeightOfPlane(surface, i), IOSurfaceGetBytesPerRowOfPlane(surface, i) };
    }
}

static IOSurfaceRef createSurface(const struct format *f, size_t width, size_t height, const struct plane *planes,
                                  const size_t *offsets, size_t total)
{
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    put(props, kIOSurfaceWidth, width);
    put(props, kIOSurfaceHeight, height);
    put(props, kIOSurfacePixelFormat, f->type);
    put(props, kIOSurfaceAllocSize, total);
    if (f->planes == 1) {
        put(props, kIOSurfaceBytesPerElement, f->plane[0].bytes);
        put(props, kIOSurfaceBytesPerRow, planes[0].bytesPerRow);
    } else {
        CFMutableArrayRef info = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        for (unsigned i = 0; i < f->planes; i++) {
            CFMutableDictionaryRef p = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            put(p, kIOSurfacePlaneWidth, planes[i].width);
            put(p, kIOSurfacePlaneHeight, planes[i].height);
            put(p, kIOSurfacePlaneBytesPerElement, f->plane[i].bytes);
            put(p, kIOSurfacePlaneBytesPerRow, planes[i].bytesPerRow);
            put(p, kIOSurfacePlaneOffset, offsets[i]);
            put(p, kIOSurfacePlaneSize, planes[i].bytesPerRow * planes[i].height);
            CFArrayAppendValue(info, p);
            CFRelease(p);
        }
        CFDictionarySetValue(props, kIOSurfacePlaneInfo, info);
        CFRelease(info);
    }
    IOSurfaceRef s = IOSurfaceCreate(props);
    CFRelease(props);
    return s;
}

CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef buffer)
{
    return CVBufferRetain(buffer);
}

void CVPixelBufferRelease(CVPixelBufferRef buffer)
{
    CVBufferRelease(buffer);
}

CVReturn CVPixelBufferCreate(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
                             CFDictionaryRef attributes, CVPixelBufferRef *pixelBufferOut)
{
    const struct format *f = formatFor(pixelFormatType);
    struct plane planes[MAX_PLANES];
    size_t offsets[MAX_PLANES];

    if (pixelBufferOut == NULL)
        return kCVReturnInvalidArgument;
    *pixelBufferOut = NULL;
    if (width == 0 || height == 0)
        return kCVReturnInvalidSize;
    if (f == NULL)
        return kCVReturnInvalidPixelFormat;

    size_t align = numberFrom(attributes, kCVPixelBufferBytesPerRowAlignmentKey, 64);
    if (align < 16)
        align = 16;
    size_t total = layout(f, width, height, align, planes, offsets);
    CVBufferRef b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;

    if (attributes && CFDictionaryContainsKey(attributes, kCVPixelBufferIOSurfacePropertiesKey)) {
        IOSurfaceRef s = createSurface(f, width, height, planes, offsets, total);
        if (s == NULL) {
            CFRelease(b);
            return kCVReturnAllocationFailed;
        }
        adoptSurface(b, s);
        CFRelease(s);
    } else {
        if (posix_memalign(&b->owned, 64, total) != 0) {
            b->owned = NULL;
            CFRelease(b);
            return kCVReturnAllocationFailed;
        }
        memset(b->owned, 0, total);
        b->planes = f->planes;
        b->dataSize = total;
        for (unsigned i = 0; i < f->planes; i++) {
            b->plane[i] = planes[i];
            b->plane[i].base = (uint8_t *)b->owned + offsets[i];
        }
    }
    *pixelBufferOut = b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithBytes(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
                                      void *baseAddress, size_t bytesPerRow, CVPixelBufferReleaseBytesCallback releaseCallback,
                                      void *releaseRefCon, CFDictionaryRef attributes, CVPixelBufferRef *pixelBufferOut)
{
    const struct format *f = formatFor(pixelFormatType);

    (void)attributes;
    if (pixelBufferOut == NULL || baseAddress == NULL)
        return kCVReturnInvalidArgument;
    *pixelBufferOut = NULL;
    if (f == NULL || f->planes != 1)
        return kCVReturnInvalidPixelFormat;
    if (width == 0 || height == 0 || bytesPerRow < width * f->plane[0].bytes)
        return kCVReturnInvalidSize;
    CVBufferRef b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;
    b->planes = 1;
    b->plane[0] = (struct plane){ baseAddress, width, height, bytesPerRow };
    b->dataSize = bytesPerRow * height;
    b->releaseBytes = releaseCallback;
    b->releaseRefCon = releaseRefCon;
    *pixelBufferOut = b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithPlanarBytes(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
                                            void *dataPtr, size_t dataSize, size_t numberOfPlanes, void *planeBaseAddress[],
                                            size_t planeWidth[], size_t planeHeight[], size_t planeBytesPerRow[],
                                            CVPixelBufferReleasePlanarBytesCallback releaseCallback, void *releaseRefCon,
                                            CFDictionaryRef attributes, CVPixelBufferRef *pixelBufferOut)
{
    const struct format *f = formatFor(pixelFormatType);

    (void)attributes;
    if (pixelBufferOut == NULL || planeBaseAddress == NULL || planeWidth == NULL || planeHeight == NULL || planeBytesPerRow == NULL)
        return kCVReturnInvalidArgument;
    *pixelBufferOut = NULL;
    if (f == NULL)
        return kCVReturnInvalidPixelFormat;
    if (numberOfPlanes != f->planes || numberOfPlanes < 2)
        return kCVReturnInvalidArgument;
    if (width == 0 || height == 0)
        return kCVReturnInvalidSize;
    CVBufferRef b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;
    b->planes = (unsigned)numberOfPlanes;
    for (size_t i = 0; i < numberOfPlanes; i++)
        b->plane[i] = (struct plane){ planeBaseAddress[i], planeWidth[i], planeHeight[i], planeBytesPerRow[i] };
    b->dataPtr = dataPtr;
    b->dataSize = dataSize;
    b->releasePlanar = releaseCallback;
    b->releaseRefCon = releaseRefCon;
    *pixelBufferOut = b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithIOSurface(CFAllocatorRef allocator, IOSurfaceRef surface, CFDictionaryRef attributes,
                                          CVPixelBufferRef *pixelBufferOut)
{
    (void)attributes;
    if (pixelBufferOut == NULL || surface == NULL)
        return kCVReturnInvalidArgument;
    *pixelBufferOut = NULL;
    if (IOSurfaceGetWidth(surface) == 0 || IOSurfaceGetHeight(surface) == 0)
        return kCVReturnInvalidSize;
    CVBufferRef b = createBuffer(allocator, IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface), IOSurfaceGetPixelFormat(surface));
    if (b == NULL)
        return kCVReturnAllocationFailed;
    adoptSurface(b, surface);
    *pixelBufferOut = b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferLockBaseAddress(CVPixelBufferRef buffer, CVPixelBufferLockFlags lockFlags)
{
    if (!isBuffer(buffer))
        return kCVReturnInvalidArgument;
    if (buffer->surface && IOSurfaceLock(buffer->surface, (uint32_t)(lockFlags & kCVPixelBufferLock_ReadOnly), NULL) != 0)
        return kCVReturnError;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferUnlockBaseAddress(CVPixelBufferRef buffer, CVPixelBufferLockFlags unlockFlags)
{
    if (!isBuffer(buffer))
        return kCVReturnInvalidArgument;
    if (buffer->surface && IOSurfaceUnlock(buffer->surface, (uint32_t)(unlockFlags & kCVPixelBufferLock_ReadOnly), NULL) != 0)
        return kCVReturnError;
    return kCVReturnSuccess;
}

size_t CVPixelBufferGetWidth(CVPixelBufferRef buffer) { return isBuffer(buffer) ? buffer->width : 0; }
size_t CVPixelBufferGetHeight(CVPixelBufferRef buffer) { return isBuffer(buffer) ? buffer->height : 0; }
OSType CVPixelBufferGetPixelFormatType(CVPixelBufferRef buffer) { return isBuffer(buffer) ? buffer->format : 0; }
size_t CVPixelBufferGetDataSize(CVPixelBufferRef buffer) { return isBuffer(buffer) ? buffer->dataSize : 0; }
Boolean CVPixelBufferIsPlanar(CVPixelBufferRef buffer) { return isBuffer(buffer) && buffer->planes > 1; }
IOSurfaceRef CVPixelBufferGetIOSurface(CVPixelBufferRef buffer) { return isBuffer(buffer) ? buffer->surface : NULL; }

// a packed buffer reports no planes, as on macOS
size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef buffer)
{
    return CVPixelBufferIsPlanar(buffer) ? buffer->planes : 0;
}

void *CVPixelBufferGetBaseAddress(CVPixelBufferRef buffer)
{
    return isBuffer(buffer) ? buffer->plane[0].base : NULL;
}

size_t CVPixelBufferGetBytesPerRow(CVPixelBufferRef buffer)
{
    return isBuffer(buffer) ? buffer->plane[0].bytesPerRow : 0;
}

#define PLANE(field) (CVPixelBufferIsPlanar(buffer) && plane < buffer->planes ? buffer->plane[plane].field : 0)
size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef buffer, size_t plane) { return PLANE(width); }
size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef buffer, size_t plane) { return PLANE(height); }
size_t CVPixelBufferGetBytesPerRowOfPlane(CVPixelBufferRef buffer, size_t plane) { return PLANE(bytesPerRow); }
void *CVPixelBufferGetBaseAddressOfPlane(CVPixelBufferRef buffer, size_t plane)
{
    return CVPixelBufferIsPlanar(buffer) && plane < buffer->planes ? buffer->plane[plane].base : NULL;
}

CGSize CVImageBufferGetEncodedSize(CVImageBufferRef buffer)
{
    return isBuffer(buffer) ? (CGSize){ buffer->width, buffer->height } : (CGSize){ 0, 0 };
}

CGSize CVImageBufferGetDisplaySize(CVImageBufferRef buffer)
{
    return CVImageBufferGetEncodedSize(buffer);
}
