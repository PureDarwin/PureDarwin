// SPDX-License-Identifier: MIT
// CVBuffer attachments, CVImageBuffer color keys and CVPixelBuffer over memory or an IOSurface
#ifndef __COREVIDEO_CVPIXELBUFFER__
#define __COREVIDEO_CVPIXELBUFFER__

#include <CoreVideo/CVBase.h>
#include <CoreVideo/CVReturn.h>
#include <CoreFoundation/CFCGTypes.h>

CF_EXTERN_C_BEGIN

typedef struct __CVBuffer *CVBufferRef;
typedef CVBufferRef CVImageBufferRef;
typedef CVImageBufferRef CVPixelBufferRef;
struct __IOSurface;

typedef uint32_t CVAttachmentMode;
enum {
    kCVAttachmentMode_ShouldNotPropagate = 0,
    kCVAttachmentMode_ShouldPropagate = 1,
};

typedef uint64_t CVPixelBufferLockFlags;
enum { kCVPixelBufferLock_ReadOnly = 1 };

enum {
    kCVPixelFormatType_32ARGB = 0x00000020,
    kCVPixelFormatType_24RGB = 0x00000018,
    kCVPixelFormatType_32BGRA = 'BGRA',
    kCVPixelFormatType_32ABGR = 'ABGR',
    kCVPixelFormatType_32RGBA = 'RGBA',
    kCVPixelFormatType_OneComponent8 = 'L008',
    kCVPixelFormatType_OneComponent16 = 'L016',
    kCVPixelFormatType_OneComponent16Half = 'L00h',
    kCVPixelFormatType_TwoComponent8 = '2C08',
    kCVPixelFormatType_422YpCbCr8 = '2vuy',
    kCVPixelFormatType_422YpCbCr8_yuvs = 'yuvs',
    kCVPixelFormatType_420YpCbCr8Planar = 'y420',
    kCVPixelFormatType_420YpCbCr8PlanarFullRange = 'f420',
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange = '420v',
    kCVPixelFormatType_420YpCbCr8BiPlanarFullRange = '420f',
    kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420',
    kCVPixelFormatType_420YpCbCr10BiPlanarFullRange = 'xf20',
    kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange = '422v',
    kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange = '444v',
    kCVPixelFormatType_64RGBAHalf = 'RGhA',
    kCVPixelFormatType_128RGBAFloat = 'RGfA',
    kCVPixelFormatType_ARGB2101010LEPacked = 'l10r',
    kCVPixelFormatType_30RGB = 'R10k',
};

typedef void (*CVPixelBufferReleaseBytesCallback)(void *releaseRefCon, const void *baseAddress);
typedef void (*CVPixelBufferReleasePlanarBytesCallback)(void *releaseRefCon, const void *dataPtr, size_t dataSize,
                                                        size_t numberOfPlanes, const void *planeAddresses[]);

COREVIDEO_EXPORT const CFStringRef kCVBufferPropagatedAttachmentsKey;
COREVIDEO_EXPORT const CFStringRef kCVBufferNonPropagatedAttachmentsKey;

COREVIDEO_EXPORT const CFStringRef kCVImageBufferCGColorSpaceKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferChromaLocationTopFieldKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferChromaLocationBottomFieldKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferChromaLocation_Left;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferChromaLocation_Center;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferChromaLocation_TopLeft;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimariesKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_ITU_R_709_2;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_EBU_3213;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_SMPTE_C;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_P22;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_DCI_P3;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_P3_D65;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferColorPrimaries_ITU_R_2020;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunctionKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_ITU_R_709_2;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_SMPTE_240M_1995;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_UseGamma;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_sRGB;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_Linear;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_ITU_R_2020;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferTransferFunction_ITU_R_2100_HLG;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferYCbCrMatrixKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_709_2;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_601_4;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_2020;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferMasteringDisplayColorVolumeKey;
COREVIDEO_EXPORT const CFStringRef kCVImageBufferContentLightLevelInfoKey;

COREVIDEO_EXPORT const CFStringRef kCVPixelBufferPixelFormatTypeKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferWidthKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferHeightKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferBytesPerRowAlignmentKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferIOSurfacePropertiesKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferOpenGLCompatibilityKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferMetalCompatibilityKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferCGImageCompatibilityKey;
COREVIDEO_EXPORT const CFStringRef kCVPixelBufferCGBitmapContextCompatibilityKey;

COREVIDEO_EXPORT CVBufferRef CVBufferRetain(CVBufferRef buffer);
COREVIDEO_EXPORT void CVBufferRelease(CVBufferRef buffer);
COREVIDEO_EXPORT void CVBufferSetAttachment(CVBufferRef buffer, CFStringRef key, CFTypeRef value, CVAttachmentMode mode);
COREVIDEO_EXPORT CFTypeRef CVBufferGetAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode);
COREVIDEO_EXPORT CFTypeRef CVBufferCopyAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode);
COREVIDEO_EXPORT void CVBufferRemoveAttachment(CVBufferRef buffer, CFStringRef key);
COREVIDEO_EXPORT void CVBufferRemoveAllAttachments(CVBufferRef buffer);
COREVIDEO_EXPORT CFDictionaryRef CVBufferGetAttachments(CVBufferRef buffer, CVAttachmentMode mode);
COREVIDEO_EXPORT CFDictionaryRef CVBufferCopyAttachments(CVBufferRef buffer, CVAttachmentMode mode);
COREVIDEO_EXPORT void CVBufferSetAttachments(CVBufferRef buffer, CFDictionaryRef attachments, CVAttachmentMode mode);
COREVIDEO_EXPORT void CVBufferPropagateAttachments(CVBufferRef source, CVBufferRef destination);

COREVIDEO_EXPORT CFTypeID CVPixelBufferGetTypeID(void);
COREVIDEO_EXPORT CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef buffer);
COREVIDEO_EXPORT void CVPixelBufferRelease(CVPixelBufferRef buffer);
COREVIDEO_EXPORT CVReturn CVPixelBufferCreate(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
                                              CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
COREVIDEO_EXPORT CVReturn CVPixelBufferCreateWithBytes(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
                                                       void *baseAddress, size_t bytesPerRow,
                                                       CVPixelBufferReleaseBytesCallback releaseCallback, void *releaseRefCon,
                                                       CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
COREVIDEO_EXPORT CVReturn CVPixelBufferCreateWithPlanarBytes(CFAllocatorRef allocator, size_t width, size_t height,
                                                             OSType pixelFormatType, void *dataPtr, size_t dataSize,
                                                             size_t numberOfPlanes, void *planeBaseAddress[],
                                                             size_t planeWidth[], size_t planeHeight[], size_t planeBytesPerRow[],
                                                             CVPixelBufferReleasePlanarBytesCallback releaseCallback,
                                                             void *releaseRefCon, CFDictionaryRef pixelBufferAttributes,
                                                             CVPixelBufferRef *pixelBufferOut);
COREVIDEO_EXPORT CVReturn CVPixelBufferCreateWithIOSurface(CFAllocatorRef allocator, struct __IOSurface *surface,
                                                           CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
COREVIDEO_EXPORT CVReturn CVPixelBufferLockBaseAddress(CVPixelBufferRef buffer, CVPixelBufferLockFlags lockFlags);
COREVIDEO_EXPORT CVReturn CVPixelBufferUnlockBaseAddress(CVPixelBufferRef buffer, CVPixelBufferLockFlags unlockFlags);
COREVIDEO_EXPORT size_t CVPixelBufferGetWidth(CVPixelBufferRef buffer);
COREVIDEO_EXPORT size_t CVPixelBufferGetHeight(CVPixelBufferRef buffer);
COREVIDEO_EXPORT OSType CVPixelBufferGetPixelFormatType(CVPixelBufferRef buffer);
COREVIDEO_EXPORT void *CVPixelBufferGetBaseAddress(CVPixelBufferRef buffer);
COREVIDEO_EXPORT size_t CVPixelBufferGetBytesPerRow(CVPixelBufferRef buffer);
COREVIDEO_EXPORT size_t CVPixelBufferGetDataSize(CVPixelBufferRef buffer);
COREVIDEO_EXPORT Boolean CVPixelBufferIsPlanar(CVPixelBufferRef buffer);
COREVIDEO_EXPORT size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef buffer);
COREVIDEO_EXPORT size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef buffer, size_t plane);
COREVIDEO_EXPORT size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef buffer, size_t plane);
COREVIDEO_EXPORT void *CVPixelBufferGetBaseAddressOfPlane(CVPixelBufferRef buffer, size_t plane);
COREVIDEO_EXPORT size_t CVPixelBufferGetBytesPerRowOfPlane(CVPixelBufferRef buffer, size_t plane);
// IOSurfaceRef, spelled out so this header does not depend on how IOSurface is declared
COREVIDEO_EXPORT struct __IOSurface *CVPixelBufferGetIOSurface(CVPixelBufferRef buffer);
COREVIDEO_EXPORT CGSize CVImageBufferGetEncodedSize(CVImageBufferRef buffer);
COREVIDEO_EXPORT CGSize CVImageBufferGetDisplaySize(CVImageBufferRef buffer);

CF_EXTERN_C_END

#endif
