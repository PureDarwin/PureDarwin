#include <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

// a 440 Hz sine in float32 mono at 44.1 kHz, the converter resamples it to the device
static double phase;
static volatile UInt32 rendered, calls;

static OSStatus render(void *ref, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts, UInt32 bus, UInt32 frames, AudioBufferList *io)
{
    float *out = io->mBuffers[0].mData;
    for (UInt32 i = 0; i < frames; i++) {
        out[i] = 0.5f * (float)sin(phase);
        phase += 2 * M_PI * 440.0 / 44100.0;
    }
    rendered += frames;
    calls++;
    return noErr;
}

static const int16_t pcm[] = { 0, 16384, -16384, 32767, 0, 16384, -16384, 32767 };
static OSStatus feed(AudioConverterRef c, UInt32 *packets, AudioBufferList *l, AudioStreamPacketDescription **d, void *user)
{
    int *done = user;
    if (*done) {
        *packets = 0;
        return noErr;
    }
    l->mBuffers[0].mData = (void *)pcm;
    l->mBuffers[0].mDataByteSize = sizeof(pcm);
    *packets = 4;
    *done = 1;
    return noErr;
}

int main(int argc, char **argv)
{
    double seconds = argc > 1 ? atof(argv[1]) : 3.0;
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, 0 };
    AudioDeviceID dev = 0;
    UInt32 size = sizeof(dev);
    CHECK(AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &dev) == noErr, "default output device %u", (unsigned)dev);
    Boolean present = dev != 0;
    if (present) {
        Float64 rate = 0;
        a = (AudioObjectPropertyAddress){ kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, 0 };
        size = sizeof(rate);
        CHECK(AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, &rate) == noErr && rate == 48000, "device rate %.0f", rate);
        CFStringRef name = NULL;
        a.mSelector = kAudioObjectPropertyName;
        size = sizeof(name);
        CHECK(AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, &name) == noErr && name, "device name");
        a = (AudioObjectPropertyAddress){ kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, 0 };
        CHECK(AudioObjectGetPropertyDataSize(dev, &a, 0, NULL, &size) == noErr && size == offsetof(AudioBufferList, mBuffers), "no input streams");
    } else {
        printf("info no /dev/dsp0, output unit tests expect errors\n");
    }
    a = (AudioObjectPropertyAddress){ 'zzzz', kAudioObjectPropertyScopeGlobal, 0 };
    CHECK(AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &dev) == kAudioHardwareUnknownPropertyError, "unknown property");
    UInt64 h = AudioGetCurrentHostTime();
    usleep(10000);
    UInt64 ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime() - h);
    CHECK(ns > 5000000 && ns < 500000000, "host time advances %llu ns", (unsigned long long)ns);

    // converter: s16 stereo 44.1 kHz to float mono 88.2 kHz doubles the frame count
    AudioStreamBasicDescription in = { 44100, kAudioFormatLinearPCM, kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked, 4, 1, 4, 2, 16, 0 };
    AudioStreamBasicDescription out = { 88200, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked, 4, 1, 4, 1, 32, 0 };
    AudioConverterRef conv = NULL;
    CHECK(AudioConverterNew(&in, &out, &conv) == noErr, "PCM converter");
    float result[32];
    AudioBufferList bl = { 1, { { 1, sizeof(result), result } } };
    UInt32 frames = 32;
    int done = 0;
    CHECK(AudioConverterFillComplexBuffer(conv, feed, &done, &frames, &bl, NULL) == noErr && frames >= 7 && frames <= 8,
          "converted 4 input frames into %u", (unsigned)frames);
    CHECK(fabsf(result[2] - 0.25f) < 0.01f, "stereo folded to mono: %.3f", result[2]);
    AudioConverterDispose(conv);
    AudioStreamBasicDescription aac = { 44100, kAudioFormatMPEG4AAC, 0, 0, 1024, 0, 2, 0, 0 };
    CHECK(AudioConverterNew(&aac, &out, &conv) == kAudioConverterErr_FormatNotSupported, "AAC decoding is not available");

    AudioComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    AudioUnit unit = NULL;
    CHECK(comp && AudioComponentInstanceNew(comp, &unit) == noErr, "HAL output unit");
    AudioStreamBasicDescription client = { 44100, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked, 4, 1, 4, 1, 32, 0 };
    CHECK(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &client, sizeof(client)) == noErr, "client format");
    UInt32 enable = 1;
    CHECK(AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enable, sizeof(enable)) != noErr,
          "input cannot be enabled");
    AURenderCallbackStruct cb = { render, NULL };
    CHECK(AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb)) == noErr, "render callback");
    OSStatus err = AudioUnitInitialize(unit);
    if (!present) {
        CHECK(err == kAudioUnitErr_NoConnection, "initialize without a device fails");
        DONE("Audio");
    }
    CHECK(err == noErr, "initialize");
    CHECK(AudioOutputUnitStart(unit) == noErr, "start");
    sleep((unsigned)seconds);
    CHECK(AudioOutputUnitStop(unit) == noErr, "stop");
    double expected = 44100 * seconds;
    printf("info rendered %u frames in %u callbacks, expected about %.0f\n", (unsigned)rendered, (unsigned)calls, expected);
    // dsp0 takes about 1.3 s ahead of real time before writes block
    CHECK(rendered > expected * 0.8 && rendered < expected + 1.6 * 44100, "callback paced at the device rate");
    AudioUnitUninitialize(unit);
    AudioComponentInstanceDispose(unit);
    DONE("Audio");
}
