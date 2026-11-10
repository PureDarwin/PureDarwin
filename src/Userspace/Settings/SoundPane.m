// SoundPane.m - the Sound pane's view of the HAL: devices, defaults, levels and the listeners that keep it live
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import "SoundPane.h"

NSString *const PDSoundChangedNotification = @"PDSoundChangedNotification";

static AudioObjectPropertyAddress addr(AudioObjectPropertySelector sel, AudioObjectPropertyScope scope)
{
    return (AudioObjectPropertyAddress){ sel, scope, kAudioObjectPropertyElementMain };
}

static NSString *nameOf(AudioDeviceID d)
{
    AudioObjectPropertyAddress a = addr(kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal);
    CFStringRef s = NULL;
    UInt32 size = sizeof(s);

    if (AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &s) != noErr || s == NULL)
        return NSLocalizedString(@"Audio device", @"");
    return [(NSString *)s autorelease];
}

NSArray *PDSoundDevices(BOOL input)
{
    AudioObjectPropertyAddress a = addr(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID ids[32];
    UInt32 size = sizeof(ids);
    NSMutableArray *out = [NSMutableArray array];

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, ids) != noErr)
        return out;
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        AudioObjectPropertyAddress s = addr(kAudioDevicePropertyStreams, input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput);
        AudioObjectPropertyAddress r = addr(kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal);
        Float64 rate = 0;
        UInt32 n = 0, rs = sizeof(rate);

        if (AudioObjectGetPropertyDataSize(ids[i], &s, 0, NULL, &n) != noErr || n == 0)
            continue;
        AudioObjectGetPropertyData(ids[i], &r, 0, NULL, &rs, &rate);
        [out addObject:@[ @(ids[i]), nameOf(ids[i]), @(rate) ]];
    }
    return out;
}

AudioDeviceID PDSoundDefault(BOOL input)
{
    AudioObjectPropertyAddress a = addr(input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
                                        kAudioObjectPropertyScopeGlobal);
    AudioDeviceID d = kAudioObjectUnknown;
    UInt32 size = sizeof(d);

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &d);
    return d;
}

void PDSoundSetDefault(BOOL input, AudioDeviceID device)
{
    AudioObjectPropertyAddress a = addr(input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
                                        kAudioObjectPropertyScopeGlobal);

    AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, sizeof(device), &device);
    // alerts follow the output the person picks
    if (!input) {
        a.mSelector = kAudioHardwarePropertyDefaultSystemOutputDevice;
        AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, sizeof(device), &device);
    }
}

Float32 PDSoundVolume(AudioDeviceID device)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
    Float32 v = 0;
    UInt32 size = sizeof(v);

    AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &v);
    return v;
}

void PDSoundSetVolume(AudioDeviceID device, Float32 volume)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);

    AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(volume), &volume);
}

BOOL PDSoundMuted(AudioDeviceID device)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
    UInt32 v = 0, size = sizeof(v);

    AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &v);
    return v != 0;
}

void PDSoundSetMuted(AudioDeviceID device, BOOL muted)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
    UInt32 v = muted;

    AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(v), &v);
}

static void post(id unused)
{
    PDSoundWatch();
    [[NSNotificationCenter defaultCenter] postNotificationName:PDSoundChangedNotification object:nil];
}

@interface PDSoundPoster : NSObject
@end

@implementation PDSoundPoster
- (void)post:(id)unused
{
    post(nil);
}
@end

static PDSoundPoster *poster;

// listeners run on the HAL's thread, the pane redraws on the main one
static OSStatus changed(AudioObjectID object, UInt32 n, const AudioObjectPropertyAddress *a, void *data)
{
    [poster performSelectorOnMainThread:@selector(post:) withObject:nil waitUntilDone:NO];
    return noErr;
}

void PDSoundWatch(void)
{
    static NSMutableSet *watched;
    AudioObjectPropertySelector system[] = { kAudioHardwarePropertyDevices, kAudioHardwarePropertyDefaultOutputDevice,
                                             kAudioHardwarePropertyDefaultInputDevice };

    if (watched == nil) {
        watched = [[NSMutableSet alloc] init];
        poster = [[PDSoundPoster alloc] init];
        for (int k = 0; k < 3; k++) {
            AudioObjectPropertyAddress a = addr(system[k], kAudioObjectPropertyScopeGlobal);
            AudioObjectAddPropertyListener(kAudioObjectSystemObject, &a, changed, NULL);
        }
    }
    for (NSArray *d in PDSoundDevices(NO)) {
        if ([watched containsObject:d[0]])
            continue;
        AudioObjectPropertyAddress v = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
        AudioObjectPropertyAddress m = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
        AudioObjectAddPropertyListener([d[0] unsignedIntValue], &v, changed, NULL);
        AudioObjectAddPropertyListener([d[0] unsignedIntValue], &m, changed, NULL);
        [watched addObject:d[0]];
    }
}
