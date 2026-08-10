//
//  SOSCircleV2.h
//  sec
//
//  Created by Richard Murphy on 2/12/15.
//
//

#ifndef _sec_SOSCircleV2_
#define _sec_SOSCircleV2_

#include <stdio.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

typedef struct __OpaqueSOSCircleV2 {
    CFRuntimeBase _base;
    CFStringRef             uuid;
    CFMutableSetRef         peers;
    CFMutableDictionaryRef  rings;
} *SOSCircleV2Ref;

SOSPeerInfoRef SOSCircleV2PeerInfoGet(SOSCircleV2Ref circle, CFStringRef peerid);

#endif /* defined(_sec_SOSCircleV2_) */
