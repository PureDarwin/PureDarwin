// PDAudioServer.h - what CoreAudio's HAL and coreaudiod share: the Mach service, the device table and a client's ring
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDAUDIOSERVER_H
#define PDAUDIOSERVER_H

#include <mach/mach.h>
#include <stdint.h>

// coreaudiod checks this name in from its launchd plist
#define kPDAudioServerName "org.puredarwin.audio.coreaudiod"

enum {
    kPDAudioServerConnect = 0x50444100,
    kPDAudioServerConnectReply,
    kPDAudioServerGetState,
    kPDAudioServerGetStateReply,
    kPDAudioServerSubscribe,
    kPDAudioServerChanged,
    kPDAudioServerSet,
    kPDAudioServerSetReply,
    kPDAudioRingVersion = 2,
};

// a connect to this device id plays on whatever the default output is at the time
#define kPDAudioFollowDefaultOutput 0

#define kPDAudioMaxDevices 16

enum {
    kPDAudioDeviceHasOutput = 1 << 0,
    kPDAudioDeviceHasInput = 1 << 1,
    kPDAudioDeviceHardwareVolume = 1 << 2,
    kPDAudioDeviceHardwareMute = 1 << 3,
    kPDAudioDeviceRunning = 1 << 4,
};

// what changed, in a kPDAudioServerChanged message
enum {
    kPDAudioChangedDevices = 1 << 0,
    kPDAudioChangedDefaults = 1 << 1,
    kPDAudioChangedVolume = 1 << 2,
    kPDAudioChangedRunning = 1 << 3,
};

// what a kPDAudioServerSet message sets
enum {
    kPDAudioSetDefaultOutput = 1,
    kPDAudioSetDefaultSystemOutput,
    kPDAudioSetDefaultInput,
    kPDAudioSetVolume,
    kPDAudioSetMute,
};

struct pd_audio_device_info {
    uint32_t id;
    uint32_t flags;
    uint32_t sampleRate;
    uint32_t outputChannels;
    uint32_t inputChannels;
    uint32_t latencyFrames;
    float volume;
    uint32_t mute;
    char uid[128];
    char name[64];
    char manufacturer[64];
};

// one per client, mapped by both sides: the client writes interleaved float frames at the ring's rate and
// moves writeFrame, coreaudiod mixes from readFrame each period and signals the client's semaphore
struct pd_audio_ring {
    uint32_t version;
    uint32_t channels;
    uint32_t frames;
    uint32_t periodFrames;
    volatile uint32_t running;
    uint32_t sampleRate;
    volatile uint64_t writeFrame;
    volatile uint64_t readFrame;
    // host time at which the frame at readFrame reaches the engine, refreshed every period
    volatile uint64_t readHostTime;
    float data[];
};

static inline size_t pd_audio_ring_size(uint32_t channels, uint32_t frames)
{
    return sizeof(struct pd_audio_ring) + (size_t)channels * frames * sizeof(float);
}

// the client sends this with a send-once reply port
typedef struct {
    mach_msg_header_t header;
    uint32_t device;
    uint32_t pad;
} pd_audio_connect_msg;

// ring is a memory entry for the client's pd_audio_ring, wake a semaphore coreaudiod signals each period,
// session a send right whose death tells coreaudiod the client is gone
typedef struct {
    mach_msg_header_t header;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t ring;
    mach_msg_port_descriptor_t wake;
    mach_msg_port_descriptor_t session;
    NDR_record_t ndr;
    int32_t status;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t ringFrames;
    uint32_t periodFrames;
    uint32_t latencyFrames;
} pd_audio_connect_reply;

typedef struct {
    mach_msg_header_t header;
} pd_audio_get_state_msg;

typedef struct {
    mach_msg_header_t header;
    NDR_record_t ndr;
    int32_t status;
    uint32_t generation;
    uint32_t defaultOutput;
    uint32_t defaultSystemOutput;
    uint32_t defaultInput;
    uint32_t count;
    struct pd_audio_device_info devices[kPDAudioMaxDevices];
} pd_audio_state_reply;

// carries a send right in msgh_local_port (MAKE_SEND), coreaudiod sends kPDAudioServerChanged to it
typedef struct {
    mach_msg_header_t header;
} pd_audio_subscribe_msg;

typedef struct {
    mach_msg_header_t header;
    uint32_t what;
    uint32_t generation;
} pd_audio_changed_msg;

typedef struct {
    mach_msg_header_t header;
    uint32_t what;
    uint32_t device;
    uint32_t value;
    float scalar;
} pd_audio_set_msg;

typedef struct {
    mach_msg_header_t header;
    NDR_record_t ndr;
    int32_t status;
} pd_audio_set_reply;

#endif
