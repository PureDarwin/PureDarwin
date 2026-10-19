#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include "common.h"

typedef struct { char *name, *instance; int is_default, num_options; void *options; } cups_dest_t;
typedef struct { UInt64 timeStamp; UInt16 length; UInt8 data[256]; } __attribute__((packed, aligned(4))) MIDIPacket;
typedef struct { UInt32 numPackets; MIDIPacket packet[1]; } __attribute__((packed, aligned(4))) MIDIPacketList;

int main(void)
{
    void *cups = dlopen("/usr/lib/libcups.2.dylib", RTLD_NOW);
    CHECK(cups != NULL, "libcups loads");
    int (*getDests)(cups_dest_t **) = cups ? dlsym(cups, "cupsGetDests") : NULL;
    cups_dest_t *dests = (void *)1;
    CHECK(getDests && getDests(&dests) == 0 && dests == NULL, "no print destinations");
    void *bsm = dlopen("/usr/lib/libbsm.0.dylib", RTLD_NOW);
    CHECK(bsm && dlsym(bsm, "audit_token_to_pid"), "libbsm has audit_token_to_pid");

    void *midi = dlopen("/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI", RTLD_NOW);
    OSStatus (*clientCreate)(CFStringRef, void *, void *, UInt32 *) = midi ? dlsym(midi, "MIDIClientCreate") : NULL;
    OSStatus (*outPort)(UInt32, CFStringRef, UInt32 *) = midi ? dlsym(midi, "MIDIOutputPortCreate") : NULL;
    OSStatus (*send)(UInt32, UInt32, const MIDIPacketList *) = midi ? dlsym(midi, "MIDISend") : NULL;
    long (*sources)(void) = midi ? dlsym(midi, "MIDIGetNumberOfSources") : NULL;
    MIDIPacket *(*init)(MIDIPacketList *) = midi ? dlsym(midi, "MIDIPacketListInit") : NULL;
    MIDIPacket *(*add)(MIDIPacketList *, long, MIDIPacket *, UInt64, long, const UInt8 *) = midi ? dlsym(midi, "MIDIPacketListAdd") : NULL;
    UInt32 client = 0, port = 0;
    CHECK(clientCreate && clientCreate(CFSTR("t"), NULL, NULL, &client) == noErr && client, "MIDI client");
    CHECK(outPort && outPort(client, CFSTR("o"), &port) == noErr && port, "MIDI output port");
    CHECK(sources && sources() == 0, "no MIDI sources");
    UInt8 buf[1024];
    MIDIPacketList *list = (MIDIPacketList *)buf;
    const UInt8 on[] = { 0x90, 60, 100 }, off[] = { 0x80, 60, 0 };
    MIDIPacket *p = init ? init(list) : NULL;
    p = p && add ? add(list, sizeof(buf), p, 0, 3, on) : NULL;
    p = p && add ? add(list, sizeof(buf), p, 10, 3, off) : NULL;
    CHECK(p && list->numPackets == 2, "packet list holds two packets");
    CHECK(send && send(port, 0, list) == -10842, "sending to no destination is object not found");
    DONE("libcups/libbsm/CoreMIDI");
}
