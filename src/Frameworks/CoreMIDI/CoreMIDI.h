// CoreMIDI.h - MIDI clients, ports, endpoints and packet lists
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef COREMIDI_H
#define COREMIDI_H

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 MIDIObjectRef;
typedef MIDIObjectRef MIDIClientRef;
typedef MIDIObjectRef MIDIPortRef;
typedef MIDIObjectRef MIDIDeviceRef;
typedef MIDIObjectRef MIDIEntityRef;
typedef MIDIObjectRef MIDIEndpointRef;
typedef UInt64 MIDITimeStamp;
typedef unsigned long ItemCount;
typedef unsigned long ByteCount;

#pragma pack(push, 4)
typedef struct MIDIPacket {
    MIDITimeStamp timeStamp;
    UInt16 length;
    Byte data[256];
} MIDIPacket;

typedef struct MIDIPacketList {
    UInt32 numPackets;
    MIDIPacket packet[1];
} MIDIPacketList;
#pragma pack(pop)

typedef struct MIDINotification MIDINotification;
typedef void (*MIDINotifyProc)(const MIDINotification *message, void *refCon);
typedef void (*MIDIReadProc)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);

enum {
    kMIDIInvalidClient = -10830,
    kMIDIInvalidPort = -10831,
    kMIDIWrongEndpointType = -10832,
    kMIDINoConnection = -10833,
    kMIDIUnknownEndpoint = -10834,
    kMIDIUnknownProperty = -10835,
    kMIDIWrongPropertyType = -10836,
    kMIDINoCurrentSetup = -10837,
    kMIDIMessageSendErr = -10838,
    kMIDIServerStartErr = -10839,
    kMIDIObjectNotFound = -10842,
};

extern const CFStringRef kMIDIPropertyName;
extern const CFStringRef kMIDIPropertyManufacturer;
extern const CFStringRef kMIDIPropertyModel;
extern const CFStringRef kMIDIPropertyUniqueID;
extern const CFStringRef kMIDIPropertyDisplayName;
extern const CFStringRef kMIDIPropertyOffline;

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient);
OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, void *notifyBlock);
OSStatus MIDIClientDispose(MIDIClientRef client);
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon, MIDIPortRef *outPort);
OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort, void *readBlock);
OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort);
OSStatus MIDIPortDispose(MIDIPortRef port);
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon);
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source);

ItemCount MIDIGetNumberOfDevices(void);
MIDIDeviceRef MIDIGetDevice(ItemCount deviceIndex0);
ItemCount MIDIGetNumberOfSources(void);
MIDIEndpointRef MIDIGetSource(ItemCount sourceIndex0);
ItemCount MIDIGetNumberOfDestinations(void);
MIDIEndpointRef MIDIGetDestination(ItemCount destIndex0);

OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str);
OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue);

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist);
OSStatus MIDISendSysex(void *request);
OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist);
OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt);
OSStatus MIDIFlushOutput(MIDIEndpointRef dest);
OSStatus MIDIRestart(void);

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist);
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket, MIDITimeStamp time,
                              ByteCount nData, const Byte *data);

// packets are packed back to back, 4-byte aligned on arm64 as on macOS
static inline MIDIPacket *MIDIPacketNext(const MIDIPacket *pkt)
{
#if defined(__arm64__) || defined(__aarch64__)
    return (MIDIPacket *)(((uintptr_t)(&pkt->data[pkt->length]) + 3) & ~(uintptr_t)3);
#else
    return (MIDIPacket *)&pkt->data[pkt->length];
#endif
}

#ifdef __cplusplus
}
#endif

#endif
