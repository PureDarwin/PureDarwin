//
//  SOSConcordanceTrust.h
//  sec
//
//  Created by Richard Murphy on 3/15/15.
//
//

#ifndef _sec_SOSConcordanceTrust_
#define _sec_SOSConcordanceTrust_

#include <CoreFoundation/CoreFoundation.h>

typedef CF_ENUM(uint32_t, SOSConcordanceStatus) {
    kSOSConcordanceTrusted = 0,
    kSOSConcordanceGenOld = 1,     // kSOSErrorReplay
    kSOSConcordanceNoUserSig = 2,  // kSOSErrorBadSignature
    kSOSConcordanceNoUserKey = 3,  // kSOSErrorNoKey
    kSOSConcordanceNoPeer = 4,     // kSOSErrorPeerNotFound
    kSOSConcordanceBadUserSig = 5, // kSOSErrorBadSignature
    kSOSConcordanceBadPeerSig = 6, // kSOSErrorBadSignature
    kSOSConcordanceNoPeerSig = 7,
    kSOSConcordanceWeSigned = 8,
    kSOSConcordanceInvalidMembership = 9, // Only used for BackupRings so far
    kSOSConcordanceMissingMe = 10, // Only used for BackupRings so far
    kSOSConcordanceImNotWorthy = 11, // Only used for BackupRings so far
    kSOSConcordanceError = 99,
};

#endif /* defined(_sec_SOSConcordanceTrust_) */
