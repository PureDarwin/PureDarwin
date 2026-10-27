// PDAudioClient.h - this process's connections to coreaudiod, shared by the HAL's property and IO code
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDAUDIOCLIENT_H
#define PDAUDIOCLIENT_H

#include <CoreAudio/AudioHardware.h>
#include "PDAudioServer.h"

struct pd_audio_session {
    struct pd_audio_ring *ring;
    semaphore_t wake;
    mach_port_t session;
    UInt32 sampleRate;
    UInt32 channels;
    UInt32 latencyFrames;
};

// connects on first use, NULL when coreaudiod is not running or has no such device
const struct pd_audio_session *PDAudioSessionForDevice(AudioObjectID device);
const struct pd_audio_session *PDAudioSessionIfConnected(AudioObjectID device);
const struct pd_audio_session *PDAudioSession(void);
Boolean PDAudioDeviceIsRunning(AudioObjectID device);

// the device table coreaudiod keeps, cached here and refreshed whenever coreaudiod says it changed
Boolean PDAudioCopyState(pd_audio_state_reply *out);
Boolean PDAudioCopyDeviceInfo(AudioObjectID device, struct pd_audio_device_info *out);
OSStatus PDAudioServerSetValue(UInt32 what, AudioObjectID device, UInt32 value, Float32 scalar);
mach_port_t PDAudioServerPort(void);

// the alias device stands for the default output, every other device is a real one
AudioObjectID PDAudioResolveDevice(AudioObjectID device);

#endif
