// SoundPane.h - what the Sound pane reads and sets, through the CoreAudio HAL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

// posted on the main thread when a device comes or goes, a default moves or a level changes
extern NSString *const PDSoundChangedNotification;

// @[ device id, name, rate ] for each device with streams in that direction
NSArray *PDSoundDevices(BOOL input);
AudioDeviceID PDSoundDefault(BOOL input);
void PDSoundSetDefault(BOOL input, AudioDeviceID device);
Float32 PDSoundVolume(AudioDeviceID device);
void PDSoundSetVolume(AudioDeviceID device, Float32 volume);
BOOL PDSoundMuted(AudioDeviceID device);
void PDSoundSetMuted(AudioDeviceID device, BOOL muted);
// listens to the system and every device it knows, again after each change so new devices are heard
void PDSoundWatch(void);
