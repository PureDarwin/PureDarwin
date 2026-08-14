/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef OTConstants_h
#define OTConstants_h

#include <stdbool.h>

bool OctagonIsEnabled(void);
bool SecErrorIsNestedErrorCappingEnabled(void);

#if __OBJC__

#import <Foundation/Foundation.h>

extern NSString* OTDefaultContext;

extern NSErrorDomain const OctagonErrorDomain;

/* used for defaults writes */
extern NSString* OTDefaultsDomain;
extern NSString* OTDefaultsOctagonEnable;

extern NSString* OTProtocolPairing;
extern NSString* OTProtocolPiggybacking;

extern const char * OTTrustStatusChangeNotification;
extern NSString* OTEscrowRecordPrefix;


BOOL OctagonPlatformSupportsSOS(void);

// Used for testing.
void OctagonSetIsEnabled(BOOL value);
void OctagonSetPlatformSupportsSOS(BOOL value);

BOOL OctagonPerformSOSUpgrade(void);
void OctagonSetSOSUpgrade(BOOL value);

BOOL OctagonRecoveryKeyIsEnabled(void);
void OctagonRecoveryKeySetIsEnabled(BOOL value);

BOOL OctagonAuthoritativeTrustIsEnabled(void);
void OctagonAuthoritativeTrustSetIsEnabled(BOOL value);

BOOL OctagonIsSOSFeatureEnabled(void);
void OctagonSetSOSFeatureEnabled(BOOL value);

BOOL OctagonIsOptimizationEnabled(void);
void OctagonSetOptimizationEnabled(BOOL value);

BOOL OctagonIsEscrowRecordFetchEnabled(void);
void OctagonSetEscrowRecordFetchEnabled(BOOL value);

BOOL SecKVSOnCloudKitIsEnabled(void);
void SecKVSOnCloudKitSetOverrideIsEnabled(BOOL value);

void SecErrorSetOverrideNestedErrorCappingIsEnabled(BOOL value);

typedef NS_ENUM(NSInteger, CuttlefishResetReason) {
    CuttlefishResetReasonUnknown = 0,
    CuttlefishResetReasonUserInitiatedReset = 1,
    CuttlefishResetReasonHealthCheck = 2,
    CuttlefishResetReasonNoBottleDuringEscrowRecovery = 3,
    CuttlefishResetReasonLegacyJoinCircle = 4,
    CuttlefishResetReasonRecoveryKey = 5,
    CuttlefishResetReasonTestGenerated = 6,
};

extern NSString* const CuttlefishErrorDomain;
extern NSString* const CuttlefishErrorRetryAfterKey;

#endif // __OBJC__

#endif /* OTConstants_h */
