#include <CoreMedia/CMSampleBuffer.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

typedef struct OpaqueVTDecompressionSession *VTDecompressionSessionRef;
OSStatus VTDecompressionSessionCreate(CFAllocatorRef, CMVideoFormatDescriptionRef, CFDictionaryRef, CFDictionaryRef, const void *,
                                      VTDecompressionSessionRef *);
OSStatus VTCompressionSessionCreate(CFAllocatorRef, int32_t, int32_t, CMVideoCodecType, CFDictionaryRef, CFDictionaryRef, CFAllocatorRef,
                                    const void *, void *, void **);
Boolean VTIsHardwareDecodeSupported(CMVideoCodecType);

int main(void)
{
    CVPixelBufferRef pb = NULL;
    CHECK(CVPixelBufferCreate(NULL, 640, 360, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pb) == kCVReturnSuccess, "NV12 buffer");
    CHECK(CVPixelBufferGetPlaneCount(pb) == 2 && CVPixelBufferGetWidthOfPlane(pb, 1) == 320 && CVPixelBufferGetHeightOfPlane(pb, 1) == 180,
          "two planes, chroma half size");
    CHECK(CVPixelBufferLockBaseAddress(pb, 0) == kCVReturnSuccess && CVPixelBufferGetBaseAddressOfPlane(pb, 0) &&
          CVPixelBufferUnlockBaseAddress(pb, 0) == kCVReturnSuccess, "lock and plane address");
    CVBufferSetAttachment(pb, kCVImageBufferYCbCrMatrixKey, kCVImageBufferYCbCrMatrix_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
    CFDictionaryRef att = CMCopyDictionaryOfAttachments(NULL, pb, kCMAttachmentMode_ShouldPropagate);
    CHECK(att && CFEqual(CFDictionaryGetValue(att, kCVImageBufferYCbCrMatrixKey), kCVImageBufferYCbCrMatrix_ITU_R_709_2), "attachments");

    const void *keys[] = { kCVPixelBufferIOSurfacePropertiesKey };
    CFDictionaryRef empty = CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void *values[] = { empty };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CVPixelBufferRef sp = NULL;
    CHECK(CVPixelBufferCreate(NULL, 256, 128, kCVPixelFormatType_32BGRA, attrs, &sp) == kCVReturnSuccess && CVPixelBufferGetIOSurface(sp),
          "IOSurface-backed BGRA buffer");
    CHECK(CVPixelBufferGetBytesPerRow(sp) >= 1024 && CVPixelBufferGetPlaneCount(sp) == 0, "packed buffer reports no planes");
    CVPixelBufferRef wrapped = NULL;
    CHECK(CVPixelBufferCreateWithIOSurface(NULL, CVPixelBufferGetIOSurface(sp), NULL, &wrapped) == kCVReturnSuccess &&
          CVPixelBufferGetBaseAddress(wrapped) == CVPixelBufferGetBaseAddress(sp), "wrapping the same IOSurface shares memory");
    CHECK(CVPixelBufferCreate(NULL, 16, 16, 'zzzz', NULL, &sp) == kCVReturnInvalidPixelFormat, "unknown format rejected");

    // an avcC with one SPS and one PPS
    const uint8_t avcC[] = { 1, 0x64, 0, 0x1f, 0xff, 0xe1, 0, 4, 0x67, 0x64, 0, 0x1f, 1, 0, 3, 0x68, 0xee, 0x3c };
    CFDataRef data = CFDataCreate(NULL, avcC, sizeof(avcC));
    const void *ak[] = { CFSTR("avcC") }, *av[] = { data };
    CFDictionaryRef atoms = CFDictionaryCreate(NULL, ak, av, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void *ek[] = { kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms }, *ev[] = { atoms };
    CFDictionaryRef ext = CFDictionaryCreate(NULL, ek, ev, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CMVideoFormatDescriptionRef fd = NULL;
    CHECK(CMVideoFormatDescriptionCreate(NULL, kCMVideoCodecType_H264, 1920, 1080, ext, &fd) == noErr, "H.264 format description");
    CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(fd);
    CHECK(dim.width == 1920 && dim.height == 1080 && CMFormatDescriptionGetMediaSubType(fd) == 'avc1', "dimensions and codec");
    size_t count = 0, size = 0;
    int nal = 0;
    const uint8_t *ps = NULL;
    CHECK(CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fd, 1, &ps, &size, &count, &nal) == noErr && count == 2 && size == 3 &&
          ps[0] == 0x68 && nal == 4, "PPS from avcC (count %zu size %zu nal %d)", count, size, nal);

    uint8_t *mem = malloc(100);
    CMBlockBufferRef bb = NULL;
    CHECK(CMBlockBufferCreateWithMemoryBlock(NULL, mem, 100, kCFAllocatorMalloc, NULL, 10, 80, 0, &bb) == noErr &&
          CMBlockBufferGetDataLength(bb) == 80, "block buffer");
    memset(mem, 7, 100);
    uint8_t copy[4];
    CHECK(CMBlockBufferCopyDataBytes(bb, 76, 4, copy) == noErr && copy[3] == 7 &&
          CMBlockBufferCopyDataBytes(bb, 78, 4, copy) != noErr, "copy bytes and bounds");
    CMSampleTimingInfo t = { CMTimeMake(1, 30), CMTimeMake(90, 30), kCMTimeInvalid };
    size_t sizes[] = { 80 };
    CMSampleBufferRef sb = NULL;
    CHECK(CMSampleBufferCreate(NULL, bb, true, NULL, NULL, fd, 1, 1, &t, 1, sizes, &sb) == noErr && CMSampleBufferIsValid(sb), "sample buffer");
    CHECK(CMSampleBufferGetTotalSampleSize(sb) == 80 && CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb)) == 3.0, "size and time");
    CFArrayRef sa = CMSampleBufferGetSampleAttachmentsArray(sb, true);
    CHECK(sa && CFArrayGetCount(sa) == 1, "sample attachments array");

    CMTimebaseRef tb = NULL;
    CHECK(CMTimebaseCreateWithMasterClock(NULL, CMClockGetHostTimeClock(), &tb) == noErr && CMTimebaseSetRate(tb, 1.0) == noErr, "timebase runs");
    usleep(50000);
    double secs = CMTimeGetSeconds(CMTimebaseGetTime(tb));
    CHECK(secs > 0.03 && secs < 1.0, "timebase advanced %.3f s", secs);

    VTDecompressionSessionRef vt = (void *)1;
    CHECK(VTDecompressionSessionCreate(NULL, fd, NULL, NULL, NULL, &vt) == -12906 && vt == NULL, "no H.264 decoder");
    void *enc = (void *)1;
    CHECK(VTCompressionSessionCreate(NULL, 640, 480, 'avc1', NULL, NULL, NULL, NULL, NULL, &enc) == -12908 && enc == NULL, "no encoder");
    CHECK(!VTIsHardwareDecodeSupported('avc1'), "no hardware decode");
    DONE("CoreVideo/CoreMedia/VideoToolbox");
}
