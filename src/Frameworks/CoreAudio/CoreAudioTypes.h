// CoreAudioTypes.h - stream descriptions, buffer lists, time stamps and channel layouts
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef COREAUDIOTYPES_H
#define COREAUDIOTYPES_H

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioFormatID;
typedef UInt32 AudioFormatFlags;
typedef UInt32 AudioChannelLayoutTag;
typedef UInt32 AudioChannelLabel;
typedef UInt32 AudioChannelBitmap;
typedef UInt32 AudioChannelFlags;

#pragma pack(push, 8)
typedef struct AudioStreamBasicDescription {
    Float64 mSampleRate;
    AudioFormatID mFormatID;
    AudioFormatFlags mFormatFlags;
    UInt32 mBytesPerPacket;
    UInt32 mFramesPerPacket;
    UInt32 mBytesPerFrame;
    UInt32 mChannelsPerFrame;
    UInt32 mBitsPerChannel;
    UInt32 mReserved;
} AudioStreamBasicDescription;

typedef struct AudioStreamPacketDescription {
    SInt64 mStartOffset;
    UInt32 mVariableFramesInPacket;
    UInt32 mDataByteSize;
} AudioStreamPacketDescription;

typedef struct AudioBuffer {
    UInt32 mNumberChannels;
    UInt32 mDataByteSize;
    void *mData;
} AudioBuffer;

typedef struct AudioBufferList {
    UInt32 mNumberBuffers;
    AudioBuffer mBuffers[1];
} AudioBufferList;

typedef struct SMPTETime {
    SInt16 mSubframes;
    SInt16 mSubframeDivisor;
    UInt32 mCounter;
    UInt32 mType;
    UInt32 mFlags;
    SInt16 mHours;
    SInt16 mMinutes;
    SInt16 mSeconds;
    SInt16 mFrames;
} SMPTETime;

typedef struct AudioTimeStamp {
    Float64 mSampleTime;
    UInt64 mHostTime;
    Float64 mRateScalar;
    UInt64 mWordClockTime;
    SMPTETime mSMPTETime;
    UInt32 mFlags;
    UInt32 mReserved;
} AudioTimeStamp;

typedef struct AudioValueRange {
    Float64 mMinimum;
    Float64 mMaximum;
} AudioValueRange;

typedef struct AudioChannelDescription {
    AudioChannelLabel mChannelLabel;
    AudioChannelFlags mChannelFlags;
    Float32 mCoordinates[3];
} AudioChannelDescription;

typedef struct AudioChannelLayout {
    AudioChannelLayoutTag mChannelLayoutTag;
    AudioChannelBitmap mChannelBitmap;
    UInt32 mNumberChannelDescriptions;
    AudioChannelDescription mChannelDescriptions[1];
} AudioChannelLayout;
#pragma pack(pop)

enum {
    kAudio_UnimplementedError = -4,
    kAudio_FileNotFoundError = -43,
    kAudio_ParamError = -50,
    kAudio_MemFullError = -108,
};

enum {
    kAudioFormatLinearPCM = 'lpcm',
    kAudioFormatMPEG4AAC = 'aac ',
    kAudioFormatMPEGLayer3 = '.mp3',
    kAudioFormatOpus = 'opus',
    kAudioFormatFLAC = 'flac',
};

enum {
    kAudioFormatFlagIsFloat = 1U << 0,
    kAudioFormatFlagIsBigEndian = 1U << 1,
    kAudioFormatFlagIsSignedInteger = 1U << 2,
    kAudioFormatFlagIsPacked = 1U << 3,
    kAudioFormatFlagIsAlignedHigh = 1U << 4,
    kAudioFormatFlagIsNonInterleaved = 1U << 5,
    kAudioFormatFlagIsNonMixable = 1U << 6,
    kLinearPCMFormatFlagIsFloat = kAudioFormatFlagIsFloat,
    kLinearPCMFormatFlagIsSignedInteger = kAudioFormatFlagIsSignedInteger,
    kLinearPCMFormatFlagIsPacked = kAudioFormatFlagIsPacked,
    kLinearPCMFormatFlagIsNonInterleaved = kAudioFormatFlagIsNonInterleaved,
    kAudioFormatFlagsNativeFloatPacked = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
};

enum {
    kAudioTimeStampSampleTimeValid = 1U << 0,
    kAudioTimeStampHostTimeValid = 1U << 1,
    kAudioTimeStampRateScalarValid = 1U << 2,
    kAudioTimeStampSampleHostTimeValid = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid,
};

enum {
    kAudioChannelLayoutTag_UseChannelDescriptions = (0U << 16) | 0,
    kAudioChannelLayoutTag_UseChannelBitmap = (1U << 16) | 0,
    kAudioChannelLayoutTag_Mono = (100U << 16) | 1,
    kAudioChannelLayoutTag_Stereo = (101U << 16) | 2,
    kAudioChannelLayoutTag_Unknown = 0xFFFF0000,
};

enum {
    kAudioChannelLabel_Unknown = 0xFFFFFFFF,
    kAudioChannelLabel_Unused = 0,
    kAudioChannelLabel_Left = 1,
    kAudioChannelLabel_Right = 2,
    kAudioChannelLabel_Center = 3,
    kAudioChannelLabel_Mono = 42,
};

#define AudioChannelLayoutTag_GetNumberOfChannels(tag) ((UInt32)((tag) & 0x0000FFFF))

#ifdef __cplusplus
}
#endif

#endif
