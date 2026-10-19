// CoreMIDI.c - MIDI clients and ports in this process, with no MIDI drivers behind them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// there are no sources or destinations, so connecting and sending report the object as not found

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef UInt32 MIDIObjectRef;
typedef MIDIObjectRef MIDIClientRef, MIDIPortRef, MIDIEndpointRef;
typedef UInt64 MIDITimeStamp;

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

typedef void (*MIDINotifyProc)(const void *message, void *refCon);
typedef void (*MIDIReadProc)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);

enum { paramErr = -50 };

enum {
    kMIDIInvalidClient = -10830,
    kMIDIInvalidPort = -10831,
    kMIDIWrongEndpointType = -10832,
    kMIDINoConnection = -10833,
    kMIDIUnknownEndpoint = -10834,
    kMIDIUnknownProperty = -10835,
    kMIDIObjectNotFound = -10842,
};

const CFStringRef kMIDIPropertyName = CFSTR("name");
const CFStringRef kMIDIPropertyDisplayName = CFSTR("displayName");
const CFStringRef kMIDIPropertyManufacturer = CFSTR("manufacturer");
const CFStringRef kMIDIPropertyModel = CFSTR("model");
const CFStringRef kMIDIPropertyUniqueID = CFSTR("uniqueID");
const CFStringRef kMIDIPropertyOffline = CFSTR("offline");

// clients and ports by reference, their kind in the low bits
enum { kClient = 1, kPort = 2 };

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableDictionaryRef sObjects;
static MIDIObjectRef sNext = 0x100;

static MIDIObjectRef addObject(int kind, MIDIObjectRef owner)
{
    pthread_mutex_lock(&sLock);
    if (sObjects == NULL)
        sObjects = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    MIDIObjectRef ref = (sNext += 4) | kind;
    CFDictionarySetValue(sObjects, (const void *)(uintptr_t)ref, (const void *)(uintptr_t)(owner + 1));
    pthread_mutex_unlock(&sLock);
    return ref;
}

static Boolean hasObject(MIDIObjectRef ref, int kind)
{
    pthread_mutex_lock(&sLock);
    Boolean found = sObjects && (int)(ref & 3) == kind && CFDictionaryContainsKey(sObjects, (const void *)(uintptr_t)ref);
    pthread_mutex_unlock(&sLock);
    return found;
}

static void removeObject(MIDIObjectRef ref)
{
    pthread_mutex_lock(&sLock);
    if (sObjects)
        CFDictionaryRemoveValue(sObjects, (const void *)(uintptr_t)ref);
    pthread_mutex_unlock(&sLock);
}

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient)
{
    (void)name;
    (void)notifyProc;
    (void)notifyRefCon;
    if (outClient == NULL)
        return paramErr;
    *outClient = addObject(kClient, 0);
    return noErr;
}

OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, void *notifyBlock)
{
    (void)notifyBlock;
    return MIDIClientCreate(name, NULL, NULL, outClient);
}

OSStatus MIDIClientDispose(MIDIClientRef client)
{
    if (!hasObject(client, kClient))
        return kMIDIInvalidClient;
    removeObject(client);
    return noErr;
}

static OSStatus createPort(MIDIClientRef client, MIDIPortRef *outPort)
{
    if (outPort == NULL)
        return paramErr;
    if (!hasObject(client, kClient))
        return kMIDIInvalidClient;
    *outPort = addObject(kPort, client);
    return noErr;
}

OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon, MIDIPortRef *outPort)
{
    (void)portName;
    (void)readProc;
    (void)refCon;
    return createPort(client, outPort);
}

OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort, void *readBlock)
{
    (void)portName;
    (void)readBlock;
    return createPort(client, outPort);
}

OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort)
{
    (void)portName;
    return createPort(client, outPort);
}

OSStatus MIDIPortDispose(MIDIPortRef port)
{
    if (!hasObject(port, kPort))
        return kMIDIInvalidPort;
    removeObject(port);
    return noErr;
}

ItemCount MIDIGetNumberOfSources(void) { return 0; }
ItemCount MIDIGetNumberOfDestinations(void) { return 0; }
ItemCount MIDIGetNumberOfDevices(void) { return 0; }
MIDIEndpointRef MIDIGetSource(ItemCount sourceIndex0) { (void)sourceIndex0; return 0; }
MIDIEndpointRef MIDIGetDestination(ItemCount destIndex0) { (void)destIndex0; return 0; }

OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon)
{
    (void)connRefCon;
    if (!hasObject(port, kPort))
        return kMIDIInvalidPort;
    (void)source;
    return kMIDIObjectNotFound;
}

OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source)
{
    (void)source;
    return hasObject(port, kPort) ? kMIDINoConnection : kMIDIInvalidPort;
}

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist)
{
    if (!hasObject(port, kPort))
        return kMIDIInvalidPort;
    (void)dest;
    return pktlist ? kMIDIObjectNotFound : paramErr;
}

OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist)
{
    (void)src;
    return pktlist ? kMIDIObjectNotFound : paramErr;
}

OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt)
{
    (void)endpt;
    return kMIDIObjectNotFound;
}

OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str)
{
    (void)obj;
    if (str)
        *str = NULL;
    return propertyID && str ? kMIDIObjectNotFound : paramErr;
}

OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue)
{
    (void)obj;
    return propertyID && outValue ? kMIDIObjectNotFound : paramErr;
}

// packet lists are plain memory layout, packets follow each other at 4-byte alignment on arm64
// and packed on x86_64
#if defined(__arm64__) || defined(__aarch64__)
#define NEXT(p) ((MIDIPacket *)(((uintptr_t)&(p)->data[(p)->length] + 3) & ~(uintptr_t)3))
#else
#define NEXT(p) ((MIDIPacket *)&(p)->data[(p)->length])
#endif

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist)
{
    pktlist->numPackets = 0;
    return &pktlist->packet[0];
}

MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket, MIDITimeStamp time,
                              ByteCount nData, const Byte *data)
{
    const size_t header = offsetof(MIDIPacket, data);
    Byte *end = (Byte *)pktlist + listSize;
    MIDIPacket *p = curPacket;

    // a non-sysex event at the same time joins the current packet when it fits
    if (pktlist->numPackets > 0 && curPacket->timeStamp == time && data[0] != 0xF0 && curPacket->data[0] != 0xF0 &&
        curPacket->length + nData <= sizeof(curPacket->data) && &curPacket->data[curPacket->length] + nData <= end) {
        memcpy(&curPacket->data[curPacket->length], data, nData);
        curPacket->length += (UInt16)nData;
        return curPacket;
    }
    if (pktlist->numPackets > 0)
        p = NEXT(curPacket);
    if ((Byte *)p + header + nData > end || nData > 0xFFFF)
        return NULL;
    p->timeStamp = time;
    p->length = (UInt16)nData;
    memcpy(p->data, data, nData);
    pktlist->numPackets++;
    return p;
}
