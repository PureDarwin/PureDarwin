// CMSampleBuffer.c - block buffers, format descriptions, sample buffers, attachments and timebases
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "CMSampleBuffer.h"
#include <CoreFoundation/CFRuntime.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const CMSampleTimingInfo kCMTimingInfoInvalid = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

const CFStringRef kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms = CFSTR("SampleDescriptionExtensionAtoms");
const CFStringRef kCMFormatDescriptionExtension_FullRangeVideo = CFSTR("FullRangeVideo");
const CFStringRef kCMSampleAttachmentKey_NotSync = CFSTR("NotSync");
const CFStringRef kCMSampleAttachmentKey_DependsOnOthers = CFSTR("DependsOnOthers");
const CFStringRef kCMSampleAttachmentKey_IsDependedOnByOthers = CFSTR("IsDependedOnByOthers");
const CFStringRef kCMSampleAttachmentKey_DisplayImmediately = CFSTR("DisplayImmediately");
const CFStringRef kCMSampleAttachmentKey_DoNotDisplay = CFSTR("DoNotDisplay");

#define DEFINE_CLASS(type, name, finalize)                                                        \
    static CFTypeID s##type##ID;                                                                  \
    static const CFRuntimeClass s##type##Class = { 0, name, NULL, NULL, finalize, NULL, NULL, NULL, NULL }; \
    static void register##type(void) { s##type##ID = _CFRuntimeRegisterClass(&s##type##Class); } \
    static CFTypeID type##ID(void)                                                                \
    {                                                                                             \
        static pthread_once_t once = PTHREAD_ONCE_INIT;                                           \
        pthread_once(&once, register##type);                                                      \
        return s##type##ID;                                                                       \
    }

static void *createInstance(CFAllocatorRef allocator, CFTypeID type, size_t size)
{
    return (void *)_CFRuntimeCreateInstance(allocator, type, size - sizeof(CFRuntimeBase), NULL);
}

static Boolean isA(CFTypeRef cf, CFTypeID type)
{
    return cf != NULL && CFGetTypeID(cf) == type;
}

// a block buffer is one contiguous block, owned through an allocator, a custom source or not at all
struct OpaqueCMBlockBuffer {
    CFRuntimeBase base;
    uint8_t *block;
    size_t blockLength, offset, length;
    CFAllocatorRef blockAllocator;
    CMBlockBufferCustomBlockSource source;
    Boolean hasSource;
};

static void blockBufferFinalize(CFTypeRef cf)
{
    struct OpaqueCMBlockBuffer *b = (struct OpaqueCMBlockBuffer *)cf;

    if (b->hasSource) {
        if (b->source.FreeBlock)
            b->source.FreeBlock(b->source.refCon, b->block, b->blockLength);
    } else if (b->blockAllocator && b->block) {
        CFAllocatorDeallocate(b->blockAllocator, b->block);
    }
    if (b->blockAllocator)
        CFRelease(b->blockAllocator);
}

DEFINE_CLASS(BlockBuffer, "CMBlockBuffer", blockBufferFinalize)

CFTypeID CMBlockBufferGetTypeID(void)
{
    return BlockBufferID();
}

OSStatus CMBlockBufferCreateWithMemoryBlock(CFAllocatorRef structureAllocator, void *memoryBlock, size_t blockLength,
                                            CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource *customBlockSource,
                                            size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags,
                                            CMBlockBufferRef *blockBufferOut)
{
    (void)flags;
    if (blockBufferOut == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    *blockBufferOut = NULL;
    if (blockLength == 0 || offsetToData > blockLength || dataLength > blockLength - offsetToData)
        return offsetToData > blockLength ? kCMBlockBufferBadOffsetParameterErr : kCMBlockBufferBadLengthParameterErr;
    if (customBlockSource && customBlockSource->version != 0)
        return kCMBlockBufferBadCustomBlockSourceErr;

    struct OpaqueCMBlockBuffer *b = createInstance(structureAllocator, BlockBufferID(), sizeof(*b));
    if (b == NULL)
        return kCMBlockBufferStructureAllocationFailedErr;
    b->blockLength = blockLength;
    b->offset = offsetToData;
    b->length = dataLength;
    if (customBlockSource) {
        b->source = *customBlockSource;
        b->hasSource = true;
    } else {
        // NULL means the default allocator, kCFAllocatorNull that the caller keeps the memory
        CFAllocatorRef a = blockAllocator ? blockAllocator : CFAllocatorGetDefault();
        b->blockAllocator = a == kCFAllocatorNull ? NULL : (CFAllocatorRef)CFRetain(a);
    }
    if (memoryBlock == NULL) {
        memoryBlock = b->hasSource && b->source.AllocateBlock ? b->source.AllocateBlock(b->source.refCon, blockLength)
                    : b->blockAllocator ? CFAllocatorAllocate(b->blockAllocator, blockLength, 0) : NULL;
        if (memoryBlock == NULL) {
            b->hasSource = false;
            CFRelease(b);
            return kCMBlockBufferBlockAllocationFailedErr;
        }
    }
    b->block = memoryBlock;
    *blockBufferOut = b;
    return kCMBlockBufferNoErr;
}

OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef b, size_t offsetToData, size_t dataLength, void *destination)
{
    if (!isA(b, BlockBufferID()) || destination == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    if (offsetToData > b->length || dataLength > b->length - offsetToData)
        return kCMBlockBufferBadLengthParameterErr;
    memcpy(destination, b->block + b->offset + offsetToData, dataLength);
    return kCMBlockBufferNoErr;
}

OSStatus CMBlockBufferGetDataPointer(CMBlockBufferRef b, size_t offset, size_t *lengthAtOffsetOut, size_t *totalLengthOut,
                                     char **dataPointerOut)
{
    if (!isA(b, BlockBufferID()))
        return kCMBlockBufferBadPointerParameterErr;
    if (offset > b->length)
        return kCMBlockBufferBadOffsetParameterErr;
    if (lengthAtOffsetOut)
        *lengthAtOffsetOut = b->length - offset;
    if (totalLengthOut)
        *totalLengthOut = b->length;
    if (dataPointerOut)
        *dataPointerOut = (char *)b->block + b->offset + offset;
    return kCMBlockBufferNoErr;
}

size_t CMBlockBufferGetDataLength(CMBlockBufferRef b)
{
    return isA(b, BlockBufferID()) ? b->length : 0;
}

// format descriptions: media type, codec and the extension dictionary decoders configure from
struct opaqueCMFormatDescription {
    CFRuntimeBase base;
    CMMediaType mediaType;
    FourCharCode subType;
    CMVideoDimensions dimensions;
    CFDictionaryRef extensions;
};

static void formatFinalize(CFTypeRef cf)
{
    struct opaqueCMFormatDescription *f = (struct opaqueCMFormatDescription *)cf;
    if (f->extensions)
        CFRelease(f->extensions);
}

DEFINE_CLASS(Format, "CMFormatDescription", formatFinalize)

CFTypeID CMFormatDescriptionGetTypeID(void)
{
    return FormatID();
}

CMMediaType CMFormatDescriptionGetMediaType(CMFormatDescriptionRef desc)
{
    return isA(desc, FormatID()) ? desc->mediaType : 0;
}

FourCharCode CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef desc)
{
    return isA(desc, FormatID()) ? desc->subType : 0;
}

CFDictionaryRef CMFormatDescriptionGetExtensions(CMFormatDescriptionRef desc)
{
    return isA(desc, FormatID()) ? desc->extensions : NULL;
}

CFPropertyListRef CMFormatDescriptionGetExtension(CMFormatDescriptionRef desc, CFStringRef extensionKey)
{
    return isA(desc, FormatID()) && desc->extensions && extensionKey ? CFDictionaryGetValue(desc->extensions, extensionKey) : NULL;
}

OSStatus CMVideoFormatDescriptionCreate(CFAllocatorRef allocator, CMVideoCodecType codecType, int32_t width, int32_t height,
                                        CFDictionaryRef extensions, CMVideoFormatDescriptionRef *formatDescriptionOut)
{
    if (formatDescriptionOut == NULL || width < 0 || height < 0)
        return kCMFormatDescriptionError_InvalidParameter;
    struct opaqueCMFormatDescription *f = createInstance(allocator, FormatID(), sizeof(*f));
    if (f == NULL)
        return kCMFormatDescriptionError_AllocationFailed;
    f->mediaType = kCMMediaType_Video;
    f->subType = codecType;
    f->dimensions = (CMVideoDimensions){ width, height };
    f->extensions = extensions ? CFDictionaryCreateCopy(allocator, extensions) : NULL;
    *formatDescriptionOut = f;
    return noErr;
}

OSStatus CMVideoFormatDescriptionCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                      CMVideoFormatDescriptionRef *formatDescriptionOut)
{
    if (imageBuffer == NULL || CFGetTypeID(imageBuffer) != CVPixelBufferGetTypeID())
        return kCMFormatDescriptionError_InvalidParameter;
    // the image's propagated attachments (color primaries, matrix) become the extensions
    CFDictionaryRef ext = CVBufferCopyAttachments(imageBuffer, kCVAttachmentMode_ShouldPropagate);
    OSStatus err = CMVideoFormatDescriptionCreate(allocator, CVPixelBufferGetPixelFormatType(imageBuffer),
                                                  (int32_t)CVPixelBufferGetWidth(imageBuffer),
                                                  (int32_t)CVPixelBufferGetHeight(imageBuffer), ext, formatDescriptionOut);
    if (ext)
        CFRelease(ext);
    return err;
}

CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef desc)
{
    return isA(desc, FormatID()) ? desc->dimensions : (CMVideoDimensions){ 0, 0 };
}

// the avcC record: version, profile, compat, level, length size, then counted SPS and PPS arrays
OSStatus CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef desc, size_t index, const uint8_t **pointerOut,
                                                            size_t *sizeOut, size_t *countOut, int *headerLengthOut)
{
    CFDictionaryRef atoms = CMFormatDescriptionGetExtension(desc, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    CFDataRef avcC = atoms && CFGetTypeID(atoms) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(atoms, CFSTR("avcC")) : NULL;

    if (!isA(desc, FormatID()))
        return kCMFormatDescriptionError_InvalidParameter;
    if (desc->subType != kCMVideoCodecType_H264 || avcC == NULL || CFGetTypeID(avcC) != CFDataGetTypeID())
        return kCMFormatDescriptionError_ValueNotAvailable;

    const uint8_t *p = CFDataGetBytePtr(avcC), *end = p + CFDataGetLength(avcC);
    if (end - p < 7)
        return kCMFormatDescriptionError_ValueNotAvailable;
    int lengthSize = (p[4] & 3) + 1;
    const uint8_t *found = NULL;
    size_t foundSize = 0, count = 0;

    p += 5;
    for (int kind = 0; kind < 2 && p < end; kind++) {
        unsigned n = kind == 0 ? (*p++ & 0x1f) : *p++;
        for (unsigned i = 0; i < n; i++) {
            if (end - p < 2)
                return kCMFormatDescriptionError_ValueNotAvailable;
            size_t size = (size_t)p[0] << 8 | p[1];
            p += 2;
            if ((size_t)(end - p) < size)
                return kCMFormatDescriptionError_ValueNotAvailable;
            if (count == index) {
                found = p;
                foundSize = size;
            }
            count++;
            p += size;
        }
    }
    if (countOut)
        *countOut = count;
    if (headerLengthOut)
        *headerLengthOut = lengthSize;
    if (pointerOut == NULL && sizeOut == NULL)
        return noErr;
    if (found == NULL)
        return kCMFormatDescriptionError_InvalidParameter;
    if (pointerOut)
        *pointerOut = found;
    if (sizeOut)
        *sizeOut = foundSize;
    return noErr;
}

// sample buffers hold either a block of samples or one decoded image
struct opaqueCMSampleBuffer {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CMBlockBufferRef data;
    CVImageBufferRef image;
    CMFormatDescriptionRef format;
    Boolean dataReady, invalid;
    CMSampleBufferMakeDataReadyCallback makeReady;
    void *makeReadyRefcon;
    CMItemCount numSamples, numTimings, numSizes;
    CMSampleTimingInfo *timings;
    size_t *sizes;
    CFMutableArrayRef sampleAttachments;
    CFMutableDictionaryRef attachments[2];
};

static void sampleBufferFinalize(CFTypeRef cf)
{
    struct opaqueCMSampleBuffer *s = (struct opaqueCMSampleBuffer *)cf;
    CFTypeRef refs[] = { s->data, s->image, s->format, s->sampleAttachments, s->attachments[0], s->attachments[1] };

    for (size_t i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
        if (refs[i])
            CFRelease(refs[i]);
    }
    free(s->timings);
    free(s->sizes);
    pthread_mutex_destroy(&s->lock);
}

DEFINE_CLASS(SampleBuffer, "CMSampleBuffer", sampleBufferFinalize)

CFTypeID CMSampleBufferGetTypeID(void)
{
    return SampleBufferID();
}

static struct opaqueCMSampleBuffer *createSampleBuffer(CFAllocatorRef allocator)
{
    struct opaqueCMSampleBuffer *s = createInstance(allocator, SampleBufferID(), sizeof(*s));
    if (s == NULL)
        return NULL;
    pthread_mutex_init(&s->lock, NULL);
    for (int i = 0; i < 2; i++)
        s->attachments[i] = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return s;
}

OSStatus CMSampleBufferCreate(CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady,
                              CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon,
                              CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries,
                              const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries,
                              const size_t *sampleSizeArray, CMSampleBufferRef *sampleBufferOut)
{
    if (sampleBufferOut == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    *sampleBufferOut = NULL;
    if (numSamples < 0 || numSampleTimingEntries < 0 || numSampleSizeEntries < 0 ||
        (numSampleTimingEntries > 1 && numSampleTimingEntries != numSamples) ||
        (numSampleSizeEntries > 1 && numSampleSizeEntries != numSamples) ||
        (numSampleTimingEntries && sampleTimingArray == NULL) || (numSampleSizeEntries && sampleSizeArray == NULL))
        return kCMSampleBufferError_InvalidEntryCount;
    if (numSamples > 0 && dataBuffer == NULL && dataReady)
        return kCMSampleBufferError_RequiredParameterMissing;

    struct opaqueCMSampleBuffer *s = createSampleBuffer(allocator);
    if (s == NULL)
        return kCMSampleBufferError_AllocationFailed;
    s->data = dataBuffer ? (CMBlockBufferRef)CFRetain(dataBuffer) : NULL;
    s->format = formatDescription ? (CMFormatDescriptionRef)CFRetain(formatDescription) : NULL;
    s->dataReady = dataReady;
    s->makeReady = makeDataReadyCallback;
    s->makeReadyRefcon = makeDataReadyRefcon;
    s->numSamples = numSamples;
    if (numSampleTimingEntries) {
        s->timings = malloc(sizeof(*s->timings) * numSampleTimingEntries);
        memcpy(s->timings, sampleTimingArray, sizeof(*s->timings) * numSampleTimingEntries);
        s->numTimings = numSampleTimingEntries;
    }
    if (numSampleSizeEntries) {
        s->sizes = malloc(sizeof(*s->sizes) * numSampleSizeEntries);
        memcpy(s->sizes, sampleSizeArray, sizeof(*s->sizes) * numSampleSizeEntries);
        s->numSizes = numSampleSizeEntries;
    }
    *sampleBufferOut = s;
    return noErr;
}

OSStatus CMSampleBufferCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer, Boolean dataReady,
                                            CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon,
                                            CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming,
                                            CMSampleBufferRef *sampleBufferOut)
{
    if (sampleBufferOut == NULL || imageBuffer == NULL || formatDescription == NULL || sampleTiming == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    OSStatus err = CMSampleBufferCreate(allocator, NULL, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription,
                                        1, 1, sampleTiming, 0, NULL, sampleBufferOut);
    if (err == noErr)
        (*sampleBufferOut)->image = (CVImageBufferRef)CFRetain(imageBuffer);
    return err;
}

OSStatus CMSampleBufferCreateReadyWithImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                  CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming,
                                                  CMSampleBufferRef *sampleBufferOut)
{
    return CMSampleBufferCreateForImageBuffer(allocator, imageBuffer, true, NULL, NULL, formatDescription, sampleTiming,
                                              sampleBufferOut);
}

Boolean CMSampleBufferIsValid(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) && !s->invalid;
}

Boolean CMSampleBufferDataIsReady(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) && s->dataReady;
}

OSStatus CMSampleBufferMakeDataReady(CMSampleBufferRef s)
{
    if (!isA(s, SampleBufferID()))
        return kCMSampleBufferError_RequiredParameterMissing;
    if (s->dataReady)
        return noErr;
    if (s->makeReady == NULL)
        return kCMSampleBufferError_InvalidSampleData;
    OSStatus err = s->makeReady(s, s->makeReadyRefcon);
    if (err == noErr)
        s->dataReady = true;
    return err;
}

CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) ? s->data : NULL;
}

CVImageBufferRef CMSampleBufferGetImageBuffer(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) ? s->image : NULL;
}

CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) ? s->format : NULL;
}

CMItemCount CMSampleBufferGetNumSamples(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) ? s->numSamples : 0;
}

size_t CMSampleBufferGetSampleSize(CMSampleBufferRef s, CMItemIndex index)
{
    if (!isA(s, SampleBufferID()) || index < 0 || index >= s->numSamples || s->numSizes == 0)
        return 0;
    return s->sizes[s->numSizes == 1 ? 0 : index];
}

size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef s)
{
    if (!isA(s, SampleBufferID()) || s->numSizes == 0)
        return 0;
    if (s->numSizes == 1)
        return s->sizes[0] * (size_t)s->numSamples;
    size_t total = 0;
    for (CMItemCount i = 0; i < s->numSizes; i++)
        total += s->sizes[i];
    return total;
}

OSStatus CMSampleBufferGetSampleTimingInfo(CMSampleBufferRef s, CMItemIndex index, CMSampleTimingInfo *out)
{
    if (!isA(s, SampleBufferID()) || out == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (s->numTimings == 0)
        return kCMSampleBufferError_BufferHasNoSampleTimingInfo;
    if (index < 0 || index >= s->numSamples)
        return kCMSampleBufferError_InvalidEntryCount;
    if (s->numTimings == 1) {
        // one entry stands for every sample, each a duration further on
        *out = s->timings[0];
        if (index && CMTIME_IS_NUMERIC(out->duration)) {
            CMTime step = CMTimeMultiply(out->duration, (int32_t)index);
            out->presentationTimeStamp = CMTimeAdd(out->presentationTimeStamp, step);
            if (CMTIME_IS_VALID(out->decodeTimeStamp))
                out->decodeTimeStamp = CMTimeAdd(out->decodeTimeStamp, step);
        }
        return noErr;
    }
    *out = s->timings[index];
    return noErr;
}

CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef s)
{
    if (!isA(s, SampleBufferID()) || s->numTimings == 0)
        return kCMTimeInvalid;
    CMTime earliest = s->timings[0].presentationTimeStamp;
    for (CMItemCount i = 1; i < s->numTimings; i++) {
        if (CMTimeCompare(s->timings[i].presentationTimeStamp, earliest) < 0)
            earliest = s->timings[i].presentationTimeStamp;
    }
    return earliest;
}

CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef s)
{
    return isA(s, SampleBufferID()) && s->numTimings ? s->timings[0].decodeTimeStamp : kCMTimeInvalid;
}

CMTime CMSampleBufferGetDuration(CMSampleBufferRef s)
{
    if (!isA(s, SampleBufferID()) || s->numTimings == 0)
        return kCMTimeInvalid;
    if (s->numTimings == 1)
        return CMTimeMultiply(s->timings[0].duration, (int32_t)(s->numSamples ? s->numSamples : 1));
    CMTime total = kCMTimeZero;
    for (CMItemCount i = 0; i < s->numTimings; i++)
        total = CMTimeAdd(total, s->timings[i].duration);
    return total;
}

// there is no trimming or speed change, so output times are the sample times
CMTime CMSampleBufferGetOutputPresentationTimeStamp(CMSampleBufferRef s)
{
    return CMSampleBufferGetPresentationTimeStamp(s);
}

CMTime CMSampleBufferGetOutputDuration(CMSampleBufferRef s)
{
    return CMSampleBufferGetDuration(s);
}

CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef s, Boolean createIfNecessary)
{
    if (!isA(s, SampleBufferID()))
        return NULL;
    pthread_mutex_lock(&s->lock);
    if (s->sampleAttachments == NULL && createIfNecessary) {
        CMItemCount n = s->numSamples ? s->numSamples : 1;
        s->sampleAttachments = CFArrayCreateMutable(NULL, n, &kCFTypeArrayCallBacks);
        for (CMItemCount i = 0; i < n; i++) {
            CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFArrayAppendValue(s->sampleAttachments, d);
            CFRelease(d);
        }
    }
    pthread_mutex_unlock(&s->lock);
    return s->sampleAttachments;
}

// attachments on sample buffers here, on pixel buffers through CoreVideo
void CMSetAttachment(CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode mode)
{
    if (target == NULL || key == NULL || value == NULL || mode > kCMAttachmentMode_ShouldPropagate)
        return;
    if (CFGetTypeID(target) == CVPixelBufferGetTypeID()) {
        CVBufferSetAttachment((CVBufferRef)target, key, value, mode);
        return;
    }
    if (!isA(target, SampleBufferID()))
        return;
    struct opaqueCMSampleBuffer *s = (struct opaqueCMSampleBuffer *)target;
    pthread_mutex_lock(&s->lock);
    CFDictionaryRemoveValue(s->attachments[!mode], key);
    CFDictionarySetValue(s->attachments[mode], key, value);
    pthread_mutex_unlock(&s->lock);
}

CFTypeRef CMGetAttachment(CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode *modeOut)
{
    if (target == NULL || key == NULL)
        return NULL;
    if (CFGetTypeID(target) == CVPixelBufferGetTypeID())
        return CVBufferGetAttachment((CVBufferRef)target, key, modeOut);
    if (!isA(target, SampleBufferID()))
        return NULL;
    struct opaqueCMSampleBuffer *s = (struct opaqueCMSampleBuffer *)target;
    CFTypeRef value = NULL;
    pthread_mutex_lock(&s->lock);
    for (CMAttachmentMode m = 0; m <= kCMAttachmentMode_ShouldPropagate && value == NULL; m++) {
        value = CFDictionaryGetValue(s->attachments[m], key);
        if (value && modeOut)
            *modeOut = m;
    }
    pthread_mutex_unlock(&s->lock);
    return value;
}

void CMRemoveAttachment(CMAttachmentBearerRef target, CFStringRef key)
{
    if (target == NULL || key == NULL)
        return;
    if (CFGetTypeID(target) == CVPixelBufferGetTypeID()) {
        CVBufferRemoveAttachment((CVBufferRef)target, key);
        return;
    }
    if (!isA(target, SampleBufferID()))
        return;
    struct opaqueCMSampleBuffer *s = (struct opaqueCMSampleBuffer *)target;
    pthread_mutex_lock(&s->lock);
    CFDictionaryRemoveValue(s->attachments[0], key);
    CFDictionaryRemoveValue(s->attachments[1], key);
    pthread_mutex_unlock(&s->lock);
}

CFDictionaryRef CMCopyDictionaryOfAttachments(CFAllocatorRef allocator, CMAttachmentBearerRef target, CMAttachmentMode mode)
{
    if (target == NULL || mode > kCMAttachmentMode_ShouldPropagate)
        return NULL;
    if (CFGetTypeID(target) == CVPixelBufferGetTypeID())
        return CVBufferCopyAttachments((CVBufferRef)target, mode);
    if (!isA(target, SampleBufferID()))
        return NULL;
    struct opaqueCMSampleBuffer *s = (struct opaqueCMSampleBuffer *)target;
    pthread_mutex_lock(&s->lock);
    CFDictionaryRef copy = CFDictionaryGetCount(s->attachments[mode]) ? CFDictionaryCreateCopy(allocator, s->attachments[mode]) : NULL;
    pthread_mutex_unlock(&s->lock);
    return copy;
}

// the host time clock is mach_absolute_time in nanoseconds
struct OpaqueCMClock {
    CFRuntimeBase base;
};

DEFINE_CLASS(Clock, "CMClock", NULL)

static mach_timebase_info_data_t timebaseInfo(void)
{
    static mach_timebase_info_data_t info;
    if (info.denom == 0)
        mach_timebase_info(&info);
    return info;
}

static CMClockRef sHostClock;

static void createHostClock(void)
{
    sHostClock = createInstance(NULL, ClockID(), sizeof(struct OpaqueCMClock));
}

CMClockRef CMClockGetHostTimeClock(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, createHostClock);
    return sHostClock;
}

CMTime CMClockMakeHostTimeFromSystemUnits(uint64_t hostTime)
{
    mach_timebase_info_data_t tb = timebaseInfo();
    __int128 ns = (__int128)hostTime * tb.numer / tb.denom;
    return CMTimeMake((int64_t)ns, 1000000000);
}

uint64_t CMClockConvertHostTimeToSystemUnits(CMTime hostTime)
{
    mach_timebase_info_data_t tb = timebaseInfo();
    CMTime ns = CMTimeConvertScale(hostTime, 1000000000, kCMTimeRoundingMethod_Default);
    if (!CMTIME_IS_NUMERIC(ns) || ns.value < 0)
        return 0;
    return (uint64_t)((__int128)ns.value * tb.denom / tb.numer);
}

CMTime CMClockGetTime(CMClockRef clock)
{
    if (!isA(clock, ClockID()))
        return kCMTimeInvalid;
    return CMClockMakeHostTimeFromSystemUnits(mach_absolute_time());
}

// a timebase runs at a rate against its clock from an anchor
struct OpaqueCMTimebase {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CMClockRef clock;
    double rate;
    CMTime anchorTime, anchorClock;
};

static void timebaseFinalize(CFTypeRef cf)
{
    struct OpaqueCMTimebase *t = (struct OpaqueCMTimebase *)cf;
    CFRelease(t->clock);
    pthread_mutex_destroy(&t->lock);
}

DEFINE_CLASS(Timebase, "CMTimebase", timebaseFinalize)

OSStatus CMTimebaseCreateWithSourceClock(CFAllocatorRef allocator, CMClockRef sourceClock, CMTimebaseRef *timebaseOut)
{
    if (timebaseOut == NULL || !isA(sourceClock, ClockID()))
        return kCMTimebaseError_InvalidParameter;
    struct OpaqueCMTimebase *t = createInstance(allocator, TimebaseID(), sizeof(*t));
    if (t == NULL)
        return kCMTimebaseError_InvalidParameter;
    pthread_mutex_init(&t->lock, NULL);
    t->clock = (CMClockRef)CFRetain(sourceClock);
    t->rate = 0;
    t->anchorTime = kCMTimeZero;
    t->anchorClock = CMClockGetTime(sourceClock);
    *timebaseOut = t;
    return noErr;
}

OSStatus CMTimebaseCreateWithMasterClock(CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut)
{
    return CMTimebaseCreateWithSourceClock(allocator, masterClock, timebaseOut);
}

static CMTime timeLocked(struct OpaqueCMTimebase *t)
{
    if (t->rate == 0)
        return t->anchorTime;
    CMTime elapsed = CMTimeSubtract(CMClockGetTime(t->clock), t->anchorClock);
    return CMTimeAdd(t->anchorTime, CMTimeMultiplyByFloat64(elapsed, t->rate));
}

CMTime CMTimebaseGetTime(CMTimebaseRef t)
{
    if (!isA(t, TimebaseID()))
        return kCMTimeInvalid;
    pthread_mutex_lock(&t->lock);
    CMTime now = timeLocked(t);
    pthread_mutex_unlock(&t->lock);
    return now;
}

OSStatus CMTimebaseSetTime(CMTimebaseRef t, CMTime time)
{
    if (!isA(t, TimebaseID()) || !CMTIME_IS_NUMERIC(time))
        return kCMTimebaseError_InvalidParameter;
    pthread_mutex_lock(&t->lock);
    t->anchorTime = time;
    t->anchorClock = CMClockGetTime(t->clock);
    pthread_mutex_unlock(&t->lock);
    return noErr;
}

OSStatus CMTimebaseSetRate(CMTimebaseRef t, double rate)
{
    if (!isA(t, TimebaseID()))
        return kCMTimebaseError_InvalidParameter;
    pthread_mutex_lock(&t->lock);
    t->anchorTime = timeLocked(t);
    t->anchorClock = CMClockGetTime(t->clock);
    t->rate = rate;
    pthread_mutex_unlock(&t->lock);
    return noErr;
}

double CMTimebaseGetRate(CMTimebaseRef t)
{
    return isA(t, TimebaseID()) ? t->rate : 0;
}
