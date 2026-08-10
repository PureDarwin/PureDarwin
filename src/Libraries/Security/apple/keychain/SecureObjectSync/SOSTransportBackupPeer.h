
#ifndef SOSTransportBackupPeer_h
#define SOSTransportBackupPeer_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include "keychain/SecureObjectSync/SOSAccount.h"

typedef struct __OpaqueSOSTransportBackupPeer *SOSTransportBackupPeerRef;


struct __OpaqueSOSTransportBackupPeer {
    CFRuntimeBase   _base;
    CFStringRef     fileLocation;

};

CFIndex SOSTransportBackupPeerGetTransportType(SOSTransportBackupPeerRef transport, CFErrorRef *error);
SOSTransportBackupPeerRef SOSTransportBackupPeerCreate(CFStringRef fileLocation, CFErrorRef *error);

#endif
