//
//  SOSCirclePriv.h
//  sec
//
//  Created by Richard Murphy on 12/4/14.
//
//

#ifndef sec_SOSCirclePriv_h
#define sec_SOSCirclePriv_h

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include "keychain/SecureObjectSync/SOSGenCount.h"

#include <security_utilities/simulatecrash_assert.h>

enum {
    kOnlyCompatibleVersion = 1, // Sometime in the future this name will be improved to reflect history.
    kAlwaysIncompatibleVersion = UINT64_MAX,
};

struct __OpaqueSOSCircle {
    CFRuntimeBase _base;
    CFStringRef name;
    SOSGenCountRef generation;
    CFMutableSetRef peers;
    CFMutableSetRef applicants;
    CFMutableSetRef rejected_applicants;
    CFMutableDictionaryRef signatures;
};


static inline void SOSCircleAssertStable(SOSCircleRef circle) {
    assert(circle);
    assert(circle->name);
    assert(circle->generation);
    assert(circle->peers);
    assert(circle->applicants);
    assert(circle->rejected_applicants);
    assert(circle->signatures);
}


static inline SOSCircleRef SOSCircleConvertAndAssertStable(CFTypeRef circleAsType) {
    if (CFGetTypeID(circleAsType) != SOSCircleGetTypeID()) return NULL;
    SOSCircleRef circle = (SOSCircleRef) circleAsType;
    SOSCircleAssertStable(circle);
    return circle;
}


static inline bool SOSCircleIsOffering(SOSCircleRef circle) {
    return SOSCircleCountRetiredPeers(circle) == 0 &&  SOSCircleCountPeers(circle) == 1;
}

#endif
