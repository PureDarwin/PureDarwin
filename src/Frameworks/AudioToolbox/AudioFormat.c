// AudioFormat.c - format and channel layout queries for linear PCM, and the file stream parser
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// no compressed codec or container parser exists here, so those report their unsupported errors

#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <string.h>

static UInt32 layoutSize(UInt32 descriptions)
{
    return (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) + descriptions * sizeof(AudioChannelDescription));
}

static UInt32 channelsInLayout(const AudioChannelLayout *l)
{
    if (l->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
        return l->mNumberChannelDescriptions;
    if (l->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
        return (UInt32)__builtin_popcount(l->mChannelBitmap);
    return AudioChannelLayoutTag_GetNumberOfChannels(l->mChannelLayoutTag);
}

// a property's size for its specifier, and its value when out is given
static OSStatus formatProperty(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *size, void *out)
{
    switch (id) {
    case kAudioFormatProperty_FormatInfo: {
        if (spec == NULL || specSize < sizeof(AudioStreamBasicDescription))
            return kAudioFormatBadSpecifierSizeError;
        AudioStreamBasicDescription f = *(const AudioStreamBasicDescription *)spec;
        *size = sizeof(f);
        if (f.mFormatID != kAudioFormatLinearPCM)
            return kAudioFormatUnsupportedDataFormatError;
        if (f.mBitsPerChannel == 0)
            f.mBitsPerChannel = (f.mFormatFlags & kAudioFormatFlagIsFloat) ? 32 : 16;
        UInt32 sample = f.mBitsPerChannel / 8;
        f.mFramesPerPacket = 1;
        if (f.mBytesPerFrame == 0)
            f.mBytesPerFrame = (f.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? sample : sample * f.mChannelsPerFrame;
        f.mBytesPerPacket = f.mBytesPerFrame;
        if (out)
            memcpy(out, &f, sizeof(f));
        return noErr;
    }
    case kAudioFormatProperty_FormatIsVBR:
    case kAudioFormatProperty_FormatIsExternallyFramed: {
        if (spec == NULL || specSize < sizeof(AudioStreamBasicDescription))
            return kAudioFormatBadSpecifierSizeError;
        const AudioStreamBasicDescription *f = spec;
        *size = sizeof(UInt32);
        if (f->mFormatID != kAudioFormatLinearPCM)
            return kAudioFormatUnsupportedDataFormatError;
        if (out)
            *(UInt32 *)out = 0;
        return noErr;
    }
    case kAudioFormatProperty_DecodeFormatIDs:
    case kAudioFormatProperty_EncodeFormatIDs:
        *size = sizeof(UInt32);
        if (out)
            *(UInt32 *)out = kAudioFormatLinearPCM;
        return noErr;
    case kAudioFormatProperty_FormatList:
    case kAudioFormatProperty_OutputFormatList:
        // the specifier is a compressed format's cookie, and no codec reads one
        return kAudioFormatUnsupportedDataFormatError;
    case kAudioFormatProperty_ChannelLayoutForTag: {
        if (spec == NULL || specSize < sizeof(AudioChannelLayoutTag))
            return kAudioFormatBadSpecifierSizeError;
        AudioChannelLayoutTag tag = *(const AudioChannelLayoutTag *)spec;
        if (tag == kAudioChannelLayoutTag_UseChannelDescriptions || tag == kAudioChannelLayoutTag_UseChannelBitmap)
            return kAudioFormatUnknownFormatError;
        *size = layoutSize(0);
        if (out) {
            memset(out, 0, *size);
            ((AudioChannelLayout *)out)->mChannelLayoutTag = tag;
        }
        return noErr;
    }
    case kAudioFormatProperty_NumberOfChannelsForLayout:
        if (spec == NULL || specSize < layoutSize(0))
            return kAudioFormatBadSpecifierSizeError;
        *size = sizeof(UInt32);
        if (out)
            *(UInt32 *)out = channelsInLayout(spec);
        return noErr;
    case kAudioFormatProperty_TagForChannelLayout: {
        if (spec == NULL || specSize < layoutSize(0))
            return kAudioFormatBadSpecifierSizeError;
        const AudioChannelLayout *l = spec;
        UInt32 n = channelsInLayout(l);
        AudioChannelLayoutTag tag = l->mChannelLayoutTag;
        if (tag == kAudioChannelLayoutTag_UseChannelDescriptions || tag == kAudioChannelLayoutTag_UseChannelBitmap)
            tag = n == 1 ? kAudioChannelLayoutTag_Mono : n == 2 ? kAudioChannelLayoutTag_Stereo : 0;
        if (tag == 0)
            return kAudioFormatUnknownFormatError;
        *size = sizeof(UInt32);
        if (out)
            *(UInt32 *)out = tag;
        return noErr;
    }
    }
    return kAudioFormatUnsupportedPropertyError;
}

OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *outSize)
{
    UInt32 size = 0;
    if (outSize == NULL)
        return kAudio_ParamError;
    OSStatus err = formatProperty(id, specSize, spec, &size, NULL);
    if (err == noErr)
        *outSize = size;
    return err;
}

OSStatus AudioFormatGetProperty(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *ioSize, void *out)
{
    UInt32 size = 0;
    if (ioSize == NULL)
        return kAudio_ParamError;
    OSStatus err = formatProperty(id, specSize, spec, &size, NULL);
    if (err != noErr)
        return err;
    if (out == NULL || *ioSize < size)
        return kAudioFormatBadPropertySizeError;
    void *buffer = calloc(1, size);
    if (buffer == NULL)
        return kAudio_MemFullError;
    formatProperty(id, specSize, spec, &size, buffer);
    memcpy(out, buffer, size);
    free(buffer);
    *ioSize = size;
    return noErr;
}

// the parser accepts a stream, then finds no container it can read in the bytes
struct OpaqueAudioFileStreamID {
    void *clientData;
    AudioFileStream_PropertyListenerProc listener;
    AudioFileStream_PacketsProc packets;
    AudioFileTypeID hint;
};

OSStatus AudioFileStreamOpen(void *clientData, AudioFileStream_PropertyListenerProc listener, AudioFileStream_PacketsProc packets,
                             AudioFileTypeID hint, AudioFileStreamID *outStream)
{
    if (listener == NULL || packets == NULL || outStream == NULL)
        return kAudio_ParamError;
    AudioFileStreamID s = calloc(1, sizeof(*s));
    if (s == NULL)
        return kAudio_MemFullError;
    *s = (struct OpaqueAudioFileStreamID){ clientData, listener, packets, hint };
    *outStream = s;
    return noErr;
}

OSStatus AudioFileStreamParseBytes(AudioFileStreamID s, UInt32 size, const void *data, AudioFileStreamParseFlags flags)
{
    (void)flags;
    if (s == NULL || (data == NULL && size))
        return kAudio_ParamError;
    return size ? kAudioFileStreamError_UnsupportedFileType : noErr;
}

OSStatus AudioFileStreamGetPropertyInfo(AudioFileStreamID s, AudioFileStreamPropertyID id, UInt32 *outSize, Boolean *outWritable)
{
    (void)id;
    if (s == NULL)
        return kAudio_ParamError;
    if (outSize)
        *outSize = 0;
    if (outWritable)
        *outWritable = false;
    return kAudioFileStreamError_UnsupportedProperty;
}

OSStatus AudioFileStreamGetProperty(AudioFileStreamID s, AudioFileStreamPropertyID id, UInt32 *ioSize, void *out)
{
    (void)id;
    (void)ioSize;
    (void)out;
    return s ? kAudioFileStreamError_UnsupportedProperty : kAudio_ParamError;
}

OSStatus AudioFileStreamClose(AudioFileStreamID s)
{
    if (s == NULL)
        return kAudio_ParamError;
    free(s);
    return noErr;
}
