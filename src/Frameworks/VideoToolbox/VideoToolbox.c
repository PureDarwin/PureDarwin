// VideoToolbox.c - sessions over the codecs this system has, which is none
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// callers learn there is no decoder or encoder for their format and use their own software codecs

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CMSampleBuffer.h>

typedef CFTypeRef VTSessionRef;
typedef struct OpaqueVTDecompressionSession *VTDecompressionSessionRef;
typedef struct OpaqueVTCompressionSession *VTCompressionSessionRef;
typedef uint32_t VTDecodeFrameFlags, VTDecodeInfoFlags, VTEncodeInfoFlags;

enum {
    kVTPropertyNotSupportedErr = -12900,
    kVTParameterErr = -12902,
    kVTInvalidSessionErr = -12903,
    kVTCouldNotFindVideoDecoderErr = -12906,
    kVTCouldNotFindVideoEncoderErr = -12908,
};

#define KEY(name) const CFStringRef name = CFSTR(#name);
KEY(kVTCompressionPropertyKey_AllowFrameReordering)
KEY(kVTCompressionPropertyKey_AverageBitRate)
KEY(kVTCompressionPropertyKey_BaseLayerFrameRateFraction)
KEY(kVTCompressionPropertyKey_ConstantBitRate)
KEY(kVTCompressionPropertyKey_ColorPrimaries)
KEY(kVTCompressionPropertyKey_ExpectedFrameRate)
KEY(kVTCompressionPropertyKey_MaxFrameDelayCount)
KEY(kVTCompressionPropertyKey_MaxKeyFrameInterval)
KEY(kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality)
KEY(kVTCompressionPropertyKey_ProfileLevel)
KEY(kVTCompressionPropertyKey_RealTime)
KEY(kVTCompressionPropertyKey_TransferFunction)
KEY(kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder)
KEY(kVTCompressionPropertyKey_YCbCrMatrix)
KEY(kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder)
KEY(kVTEncodeFrameOptionKey_ForceKeyFrame)
KEY(kVTProfileLevel_H264_Baseline_AutoLevel)
KEY(kVTProfileLevel_H264_Main_AutoLevel)
KEY(kVTProfileLevel_H264_High_AutoLevel)
KEY(kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder)
KEY(kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder)
KEY(kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder)
KEY(kVTVideoEncoderSpecification_EnableLowLatencyRateControl)
KEY(kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder)

Boolean VTIsHardwareDecodeSupported(CMVideoCodecType codecType)
{
    (void)codecType;
    return false;
}

// there are no supplemental decoders to register, VP9 and AV1 included
void VTRegisterSupplementalVideoDecoderIfAvailable(CMVideoCodecType codecType)
{
    (void)codecType;
}

OSStatus VTDecompressionSessionCreate(CFAllocatorRef allocator, CMVideoFormatDescriptionRef videoFormatDescription,
                                      CFDictionaryRef videoDecoderSpecification, CFDictionaryRef destinationImageBufferAttributes,
                                      const void *outputCallback, VTDecompressionSessionRef *decompressionSessionOut)
{
    (void)allocator;
    (void)videoDecoderSpecification;
    (void)destinationImageBufferAttributes;
    (void)outputCallback;
    if (decompressionSessionOut == NULL || videoFormatDescription == NULL)
        return kVTParameterErr;
    *decompressionSessionOut = NULL;
    return kVTCouldNotFindVideoDecoderErr;
}

// no session is ever created, so every session handed in is not one of ours
OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session, CMSampleBufferRef sampleBuffer,
                                           VTDecodeFrameFlags decodeFlags, void *sourceFrameRefCon, VTDecodeInfoFlags *infoFlagsOut)
{
    (void)session;
    (void)sampleBuffer;
    (void)decodeFlags;
    (void)sourceFrameRefCon;
    if (infoFlagsOut)
        *infoFlagsOut = 0;
    return kVTInvalidSessionErr;
}

OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef session)
{
    return session ? kVTInvalidSessionErr : kVTParameterErr;
}

void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session)
{
    (void)session;
}

OSStatus VTCompressionSessionCreate(CFAllocatorRef allocator, int32_t width, int32_t height, CMVideoCodecType codecType,
                                    CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes,
                                    CFAllocatorRef compressedDataAllocator, const void *outputCallback, void *outputCallbackRefCon,
                                    VTCompressionSessionRef *compressionSessionOut)
{
    (void)allocator;
    (void)codecType;
    (void)encoderSpecification;
    (void)sourceImageBufferAttributes;
    (void)compressedDataAllocator;
    (void)outputCallback;
    (void)outputCallbackRefCon;
    if (compressionSessionOut == NULL || width <= 0 || height <= 0)
        return kVTParameterErr;
    *compressionSessionOut = NULL;
    return kVTCouldNotFindVideoEncoderErr;
}

OSStatus VTCompressionSessionEncodeFrame(VTCompressionSessionRef session, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp,
                                         CMTime duration, CFDictionaryRef frameProperties, void *sourceFrameRefcon,
                                         VTEncodeInfoFlags *infoFlagsOut)
{
    (void)session;
    (void)imageBuffer;
    (void)presentationTimeStamp;
    (void)duration;
    (void)frameProperties;
    (void)sourceFrameRefcon;
    if (infoFlagsOut)
        *infoFlagsOut = 0;
    return kVTInvalidSessionErr;
}

OSStatus VTCompressionSessionCompleteFrames(VTCompressionSessionRef session, CMTime completeUntilPresentationTimeStamp)
{
    (void)completeUntilPresentationTimeStamp;
    return session ? kVTInvalidSessionErr : kVTParameterErr;
}

void VTCompressionSessionInvalidate(VTCompressionSessionRef session)
{
    (void)session;
}

OSStatus VTSessionSetProperty(VTSessionRef session, CFStringRef propertyKey, CFTypeRef propertyValue)
{
    (void)propertyKey;
    (void)propertyValue;
    return session ? kVTInvalidSessionErr : kVTParameterErr;
}

OSStatus VTSessionCopyProperty(VTSessionRef session, CFStringRef propertyKey, CFAllocatorRef allocator, void *propertyValueOut)
{
    (void)propertyKey;
    (void)allocator;
    if (propertyValueOut)
        *(CFTypeRef *)propertyValueOut = NULL;
    return session ? kVTInvalidSessionErr : kVTParameterErr;
}

OSStatus VTSessionCopySupportedPropertyDictionary(VTSessionRef session, CFDictionaryRef *supportedPropertyDictionaryOut)
{
    if (supportedPropertyDictionaryOut)
        *supportedPropertyDictionaryOut = NULL;
    return session ? kVTInvalidSessionErr : kVTParameterErr;
}
