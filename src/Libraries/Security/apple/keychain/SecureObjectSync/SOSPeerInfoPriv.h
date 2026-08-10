//
//  SOSPeerInfoPriv.h
//  sec
//
//  Created by Richard Murphy on 12/4/14.
//
//

#ifndef sec_SOSPeerInfoPriv_h
#define sec_SOSPeerInfoPriv_h

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFWrappers.h>

struct __OpaqueSOSPeerInfo {
    CFRuntimeBase           _base;
    //
    CFMutableDictionaryRef  description;
    CFDataRef               signature;
    
    // Cached data
    CFDictionaryRef         gestalt;
    CFStringRef             peerID;
    CFStringRef             spid;
    CFIndex                 version;
    CFStringRef             verifiedAppKeyID;
    bool                    verifiedResult;

    /* V2 and beyond are listed below */
    CFMutableDictionaryRef  v2Dictionary;
};

CF_RETURNS_RETAINED SOSPeerInfoRef SOSPeerInfoAllocate(CFAllocatorRef allocator);
bool SOSPeerInfoSign(SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSPeerInfoVerify(SOSPeerInfoRef peer, CFErrorRef *error);
void SOSPeerInfoSetVersionNumber(SOSPeerInfoRef pi, int version);

SOSPeerInfoRef SOSPeerInfoCopyWithModification(CFAllocatorRef allocator, SOSPeerInfoRef original,
                                               SecKeyRef signingKey, CFErrorRef *error,
                                               bool (^modification)(SOSPeerInfoRef peerToModify, CFErrorRef *error));

extern const CFStringRef peerIDLengthKey;

#endif
