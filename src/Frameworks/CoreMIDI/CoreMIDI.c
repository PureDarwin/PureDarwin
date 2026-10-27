// CoreMIDI.c - MIDI clients and ports in this process, with no MIDI drivers installed there are no endpoints yet
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CoreMIDI/CoreMIDI.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>

const CFStringRef kMIDIPropertyName = CFSTR("name");
const CFStringRef kMIDIPropertyManufacturer = CFSTR("manufacturer");
const CFStringRef kMIDIPropertyModel = CFSTR("model");
const CFStringRef kMIDIPropertyUniqueID = CFSTR("uniqueID");
const CFStringRef kMIDIPropertyDisplayName = CFSTR("displayName");
const CFStringRef kMIDIPropertyOffline = CFSTR("offline");

#define MAX_OBJECTS 256

enum { kClient = 1, kInputPort, kOutputPort };

struct object {
    UInt32 kind;
    MIDIObjectRef owner;
    CFStringRef name;
    MIDIReadProc readProc;
    void *refCon;
};

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static struct object sObjects[MAX_OBJECTS];

// refs are the slot plus one, so 0 stays the invalid ref
static MIDIObjectRef newObject(UInt32 kind, MIDIObjectRef owner, CFStringRef name)
{
    MIDIObjectRef ref = 0;

    pthread_mutex_lock(&sLock);
    for (UInt32 i = 0; i < MAX_OBJECTS; i++) {
        if (!sObjects[i].kind) {
            sObjects[i] = (struct object){ kind, owner, name ? CFRetain(name) : NULL, NULL, NULL };
            ref = i + 1;
            break;
        }
    }
    pthread_mutex_unlock(&sLock);
    return ref;
}

static struct object *lookup(MIDIObjectRef ref, UInt32 kind)
{
    if (ref == 0 || ref > MAX_OBJECTS || !sObjects[ref - 1].kind)
        return NULL;
    if (kind && sObjects[ref - 1].kind != kind)
        return NULL;
    return &sObjects[ref - 1];
}

static void freeObject(struct object *o)
{
    if (o->name)
        CFRelease(o->name);
    memset(o, 0, sizeof(*o));
}

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient)
{
    (void)notifyProc;
    (void)notifyRefCon;
    if (outClient == NULL)
        return kMIDIInvalidClient;
    *outClient = newObject(kClient, 0, name);
    return *outClient ? noErr : kMIDIServerStartErr;
}

// the notifications a block would get are never sent, so it is not kept
OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, void *notifyBlock)
{
    (void)notifyBlock;
    return MIDIClientCreate(name, NULL, NULL, outClient);
}

// a client takes its ports with it
OSStatus MIDIClientDispose(MIDIClientRef client)
{
    pthread_mutex_lock(&sLock);
    struct object *c = lookup(client, kClient);
    if (c == NULL) {
        pthread_mutex_unlock(&sLock);
        return kMIDIInvalidClient;
    }
    for (UInt32 i = 0; i < MAX_OBJECTS; i++) {
        if (sObjects[i].kind && sObjects[i].owner == client)
            freeObject(&sObjects[i]);
    }
    freeObject(c);
    pthread_mutex_unlock(&sLock);
    return noErr;
}

static OSStatus createPort(MIDIClientRef client, UInt32 kind, CFStringRef name, MIDIReadProc readProc, void *refCon,
                           MIDIPortRef *outPort)
{
    if (outPort == NULL)
        return kMIDIInvalidPort;
    pthread_mutex_lock(&sLock);
    Boolean valid = lookup(client, kClient) != NULL;
    pthread_mutex_unlock(&sLock);
    if (!valid)
        return kMIDIInvalidClient;
    *outPort = newObject(kind, client, name);
    if (!*outPort)
        return kMIDIInvalidPort;
    pthread_mutex_lock(&sLock);
    sObjects[*outPort - 1].readProc = readProc;
    sObjects[*outPort - 1].refCon = refCon;
    pthread_mutex_unlock(&sLock);
    return noErr;
}

OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon, MIDIPortRef *outPort)
{
    return createPort(client, kInputPort, portName, readProc, refCon, outPort);
}

// no source can be connected, so the block would never run
OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort, void *readBlock)
{
    (void)readBlock;
    return createPort(client, kInputPort, portName, NULL, NULL, outPort);
}

OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort)
{
    return createPort(client, kOutputPort, portName, NULL, NULL, outPort);
}

OSStatus MIDIPortDispose(MIDIPortRef port)
{
    pthread_mutex_lock(&sLock);
    struct object *p = lookup(port, 0);
    OSStatus err = p && p->kind != kClient ? noErr : kMIDIInvalidPort;
    if (err == noErr)
        freeObject(p);
    pthread_mutex_unlock(&sLock);
    return err;
}

// no MIDI driver publishes devices or endpoints, so every endpoint ref is unknown
ItemCount MIDIGetNumberOfDevices(void)
{
    return 0;
}

MIDIDeviceRef MIDIGetDevice(ItemCount deviceIndex0)
{
    (void)deviceIndex0;
    return 0;
}

ItemCount MIDIGetNumberOfSources(void)
{
    return 0;
}

MIDIEndpointRef MIDIGetSource(ItemCount sourceIndex0)
{
    (void)sourceIndex0;
    return 0;
}

ItemCount MIDIGetNumberOfDestinations(void)
{
    return 0;
}

MIDIEndpointRef MIDIGetDestination(ItemCount destIndex0)
{
    (void)destIndex0;
    return 0;
}

OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon)
{
    (void)connRefCon;
    pthread_mutex_lock(&sLock);
    Boolean valid = lookup(port, kInputPort) != NULL;
    pthread_mutex_unlock(&sLock);
    if (!valid)
        return kMIDIInvalidPort;
    return source ? kMIDIUnknownEndpoint : kMIDIObjectNotFound;
}

OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source)
{
    return MIDIPortConnectSource(port, source, NULL);
}

// clients and ports have their own names, endpoints do not exist
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str)
{
    if (str == NULL)
        return kMIDIUnknownProperty;
    pthread_mutex_lock(&sLock);
    struct object *o = lookup(obj, 0);
    OSStatus err = o == NULL ? kMIDIObjectNotFound : kMIDIUnknownProperty;
    if (o && o->name && CFEqual(propertyID, kMIDIPropertyName)) {
        *str = CFRetain(o->name);
        err = noErr;
    }
    pthread_mutex_unlock(&sLock);
    return err;
}

OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue)
{
    (void)propertyID;
    (void)outValue;
    pthread_mutex_lock(&sLock);
    OSStatus err = lookup(obj, 0) ? kMIDIUnknownProperty : kMIDIObjectNotFound;
    pthread_mutex_unlock(&sLock);
    return err;
}

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist)
{
    pthread_mutex_lock(&sLock);
    Boolean valid = lookup(port, kOutputPort) != NULL;
    pthread_mutex_unlock(&sLock);
    if (!valid)
        return kMIDIInvalidPort;
    if (pktlist == NULL)
        return kMIDIMessageSendErr;
    return dest ? kMIDIUnknownEndpoint : kMIDIObjectNotFound;
}

// virtual endpoints are not created here, so there is none to receive on or dispose
OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist)
{
    (void)src;
    return pktlist ? kMIDIObjectNotFound : kMIDIMessageSendErr;
}

OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt)
{
    (void)endpt;
    return kMIDIObjectNotFound;
}

OSStatus MIDISendSysex(void *request)
{
    return request ? kMIDIUnknownEndpoint : kMIDIMessageSendErr;
}

OSStatus MIDIFlushOutput(MIDIEndpointRef dest)
{
    (void)dest;
    return noErr;
}

OSStatus MIDIRestart(void)
{
    return noErr;
}

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist)
{
    pktlist->numPackets = 0;
    return &pktlist->packet[0];
}

// appends to the last packet when it shares the time stamp, as Apple's does, NULL once the list is full
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *cur, MIDITimeStamp time, ByteCount nData,
                              const Byte *data)
{
    Byte *end = (Byte *)pktlist + listSize;
    MIDIPacket *last = NULL;

    if (pktlist->numPackets > 0) {
        last = &pktlist->packet[0];
        for (UInt32 i = 1; i < pktlist->numPackets; i++)
            last = MIDIPacketNext(last);
    }
    if (last && last == cur && last->timeStamp == time && data[0] < 0xF0 && last->data[0] < 0xF0 &&
        (Byte *)&last->data[last->length + nData] <= end && last->length + nData <= sizeof(last->data)) {
        memcpy(&last->data[last->length], data, nData);
        last->length += (UInt16)nData;
        return last;
    }

    MIDIPacket *next = last ? MIDIPacketNext(last) : &pktlist->packet[0];
    if ((Byte *)&next->data[nData] > end || nData > 65535)
        return NULL;
    next->timeStamp = time;
    next->length = (UInt16)nData;
    memcpy(next->data, data, nData);
    pktlist->numPackets++;
    return next;
}
