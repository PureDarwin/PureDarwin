// CMSampleBuffer.h - block buffers, format descriptions, sample buffers, attachments and clocks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef CMSAMPLEBUFFER_H
#define CMSAMPLEBUFFER_H

#include "CMTime.h"
#include <CoreVideo/CVPixelBuffer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t CMBlockBufferFlags;
typedef CFIndex CMItemCount;
typedef CFIndex CMItemIndex;
typedef FourCharCode CMMediaType;
typedef FourCharCode CMVideoCodecType;
typedef uint32_t CMAttachmentMode;
typedef CFTypeRef CMAttachmentBearerRef;

typedef struct OpaqueCMBlockBuffer *CMBlockBufferRef;
typedef struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef CMFormatDescriptionRef CMVideoFormatDescriptionRef;
typedef CMFormatDescriptionRef CMAudioFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueCMClock *CMClockRef;
typedef struct OpaqueCMTimebase *CMTimebaseRef;

typedef struct {
    int32_t width;
    int32_t height;
} CMVideoDimensions;

typedef struct {
    CMTime duration;
    CMTime presentationTimeStamp;
    CMTime decodeTimeStamp;
} CMSampleTimingInfo;

typedef struct {
    uint32_t version;
    void *(*AllocateBlock)(void *refCon, size_t sizeInBytes);
    void (*FreeBlock)(void *refCon, void *doomedMemoryBlock, size_t sizeInBytes);
    void *refCon;
} CMBlockBufferCustomBlockSource;

typedef OSStatus (*CMSampleBufferMakeDataReadyCallback)(CMSampleBufferRef sbuf, void *makeDataReadyRefcon);

enum {
    kCMMediaType_Video = 'vide',
    kCMMediaType_Audio = 'soun',
    kCMVideoCodecType_H264 = 'avc1',
    kCMVideoCodecType_HEVC = 'hvc1',
    kCMVideoCodecType_VP9 = 'vp09',
    kCMVideoCodecType_AV1 = 'av01',
};

enum {
    kCMAttachmentMode_ShouldNotPropagate = 0,
    kCMAttachmentMode_ShouldPropagate = 1,
};

enum {
    kCMBlockBufferNoErr = 0,
    kCMBlockBufferStructureAllocationFailedErr = -12700,
    kCMBlockBufferBlockAllocationFailedErr = -12701,
    kCMBlockBufferBadCustomBlockSourceErr = -12702,
    kCMBlockBufferBadOffsetParameterErr = -12703,
    kCMBlockBufferBadLengthParameterErr = -12704,
    kCMBlockBufferBadPointerParameterErr = -12705,
    kCMBlockBufferEmptyBBufErr = -12706,
    kCMFormatDescriptionError_InvalidParameter = -12710,
    kCMFormatDescriptionError_AllocationFailed = -12711,
    kCMFormatDescriptionError_ValueNotAvailable = -12718,
    kCMSampleBufferError_AllocationFailed = -12730,
    kCMSampleBufferError_RequiredParameterMissing = -12731,
    kCMSampleBufferError_BufferHasNoSampleSizes = -12735,
    kCMSampleBufferError_BufferHasNoSampleTimingInfo = -12736,
    kCMSampleBufferError_ArrayTooSmall = -12737,
    kCMSampleBufferError_InvalidEntryCount = -12738,
    kCMSampleBufferError_InvalidSampleData = -12742,
    kCMClockError_InvalidParameter = -12746,
    kCMTimebaseError_InvalidParameter = -12748,
};

extern const CMSampleTimingInfo kCMTimingInfoInvalid;

extern const CFStringRef kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms;
extern const CFStringRef kCMFormatDescriptionExtension_FullRangeVideo;
extern const CFStringRef kCMSampleAttachmentKey_NotSync;
extern const CFStringRef kCMSampleAttachmentKey_DependsOnOthers;
extern const CFStringRef kCMSampleAttachmentKey_IsDependedOnByOthers;
extern const CFStringRef kCMSampleAttachmentKey_DisplayImmediately;
extern const CFStringRef kCMSampleAttachmentKey_DoNotDisplay;

CFTypeID CMBlockBufferGetTypeID(void);
OSStatus CMBlockBufferCreateWithMemoryBlock(CFAllocatorRef structureAllocator, void *memoryBlock, size_t blockLength,
                                            CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource *customBlockSource,
                                            size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags,
                                            CMBlockBufferRef *blockBufferOut);
OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef theSourceBuffer, size_t offsetToData, size_t dataLength, void *destination);
OSStatus CMBlockBufferGetDataPointer(CMBlockBufferRef theBuffer, size_t offset, size_t *lengthAtOffsetOut, size_t *totalLengthOut,
                                     char **dataPointerOut);
size_t CMBlockBufferGetDataLength(CMBlockBufferRef theBuffer);

CFTypeID CMFormatDescriptionGetTypeID(void);
CMMediaType CMFormatDescriptionGetMediaType(CMFormatDescriptionRef desc);
FourCharCode CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef desc);
CFDictionaryRef CMFormatDescriptionGetExtensions(CMFormatDescriptionRef desc);
CFPropertyListRef CMFormatDescriptionGetExtension(CMFormatDescriptionRef desc, CFStringRef extensionKey);
OSStatus CMVideoFormatDescriptionCreate(CFAllocatorRef allocator, CMVideoCodecType codecType, int32_t width, int32_t height,
                                        CFDictionaryRef extensions, CMVideoFormatDescriptionRef *formatDescriptionOut);
OSStatus CMVideoFormatDescriptionCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                      CMVideoFormatDescriptionRef *formatDescriptionOut);
CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef videoDesc);
OSStatus CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef videoDesc, size_t parameterSetIndex,
                                                            const uint8_t **parameterSetPointerOut, size_t *parameterSetSizeOut,
                                                            size_t *parameterSetCountOut, int *NALUnitHeaderLengthOut);

CFTypeID CMSampleBufferGetTypeID(void);
OSStatus CMSampleBufferCreate(CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady,
                              CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon,
                              CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries,
                              const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries,
                              const size_t *sampleSizeArray, CMSampleBufferRef *sampleBufferOut);
OSStatus CMSampleBufferCreateReadyWithImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                  CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming,
                                                  CMSampleBufferRef *sampleBufferOut);
OSStatus CMSampleBufferCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer, Boolean dataReady,
                                            CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon,
                                            CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming,
                                            CMSampleBufferRef *sampleBufferOut);
Boolean CMSampleBufferIsValid(CMSampleBufferRef sbuf);
Boolean CMSampleBufferDataIsReady(CMSampleBufferRef sbuf);
OSStatus CMSampleBufferMakeDataReady(CMSampleBufferRef sbuf);
CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef sbuf);
CVImageBufferRef CMSampleBufferGetImageBuffer(CMSampleBufferRef sbuf);
CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef sbuf);
CMItemCount CMSampleBufferGetNumSamples(CMSampleBufferRef sbuf);
size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef sbuf);
size_t CMSampleBufferGetSampleSize(CMSampleBufferRef sbuf, CMItemIndex sampleIndex);
CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef sbuf);
CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef sbuf);
CMTime CMSampleBufferGetDuration(CMSampleBufferRef sbuf);
CMTime CMSampleBufferGetOutputPresentationTimeStamp(CMSampleBufferRef sbuf);
CMTime CMSampleBufferGetOutputDuration(CMSampleBufferRef sbuf);
CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef sbuf, Boolean createIfNecessary);
OSStatus CMSampleBufferGetSampleTimingInfo(CMSampleBufferRef sbuf, CMItemIndex sampleIndex, CMSampleTimingInfo *timingInfoOut);

void CMSetAttachment(CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode);
CFTypeRef CMGetAttachment(CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode *attachmentModeOut);
void CMRemoveAttachment(CMAttachmentBearerRef target, CFStringRef key);
CFDictionaryRef CMCopyDictionaryOfAttachments(CFAllocatorRef allocator, CMAttachmentBearerRef target, CMAttachmentMode attachmentMode);

CMClockRef CMClockGetHostTimeClock(void);
CMTime CMClockGetTime(CMClockRef clock);
CMTime CMClockMakeHostTimeFromSystemUnits(uint64_t hostTime);
uint64_t CMClockConvertHostTimeToSystemUnits(CMTime hostTime);
OSStatus CMTimebaseCreateWithMasterClock(CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut);
OSStatus CMTimebaseCreateWithSourceClock(CFAllocatorRef allocator, CMClockRef sourceClock, CMTimebaseRef *timebaseOut);
OSStatus CMTimebaseSetRate(CMTimebaseRef timebase, double rate);
double CMTimebaseGetRate(CMTimebaseRef timebase);
OSStatus CMTimebaseSetTime(CMTimebaseRef timebase, CMTime time);
CMTime CMTimebaseGetTime(CMTimebaseRef timebase);

#ifdef __cplusplus
}
#endif

#endif
