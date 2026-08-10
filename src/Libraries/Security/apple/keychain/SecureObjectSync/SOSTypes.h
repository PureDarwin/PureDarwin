/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SOSTYPES_H_
#define _SECURITY_SOSTYPES_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 Reasons
 */

typedef enum SyncWithAllPeersReason {
    kSyncWithAllPeersOtherFail = 0,
    kSyncWithAllPeersSuccess,
    kSyncWithAllPeersLocked,
} SyncWithAllPeersReason;

/*
 * Piggy backing codes
 */

typedef enum{
    kPiggyV0 = 0, //first version of piggybacking, does not pad messages with keys
    kPiggyV1 = 1, //tacks on initial sync credentials
    kPiggyV2 = 2, //Octagon
} PiggyBackProtocolVersion;

typedef enum{
    kPiggyTLKs = 0,
    kPiggyiCloudIdentities = 1
} PiggybackKeyTypes;

typedef enum {
    kTLKUnknown = 0,
    kTLKManatee = 1,
    kTLKEngram = 2,
    kTLKAutoUnlock = 3,
    kTLKHealth = 4,
} kTLKTypes;

/*
 View Result Codes
 */
enum {
    kSOSCCGeneralViewError    = 0,
    kSOSCCViewMember          = 1,
    kSOSCCViewNotMember       = 2,
    kSOSCCViewNotQualified    = 3,
    kSOSCCNoSuchView          = 4,
    kSOSCCViewPending         = 5,
    kSOSCCViewAuthErr         = 6,
};
typedef int SOSViewResultCode;


/*
 View Action Codes
 */
enum {
    kSOSCCViewEnable          = 1,
    kSOSCCViewDisable         = 2,
    kSOSCCViewQuery           = 3,
};
typedef int SOSViewActionCode;

#if __OBJC__

#import <Foundation/Foundation.h>

#define SOSControlInitialSyncFlagTLK                  (1 << 0)
#define SOSControlInitialSyncFlagPCS                  (1 << 1)
#define SOSControlInitialSyncFlagPCSNonCurrent        (1 << 2)
#define SOSControlInitialSyncFlagBluetoothMigration   (1 << 3)

typedef NS_OPTIONS(uint32_t, SOSAccountGhostBustingOptions) {
    SOSGhostBustBySerialNumber = 1,
    SOSGhostBustByMID = 2,
    SOSGhostBustSerialByAge = 4,
    SOSGhostBustTriggerNow = 8,
    SOSGhostBustiCloudIdentities = 16,
};

@protocol SOSControlProtocol <NSObject>
- (void)userPublicKey:(void ((^))(BOOL trusted, NSData *spki, NSError *error))complete;
- (void)kvsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))reply;
- (void)rateLimitingPerformanceCounters:(void(^)(NSDictionary <NSString *, NSString *> *))reply;

- (void)stashedCredentialPublicKey:(void(^)(NSData *, NSError *error))complete;
- (void)assertStashedAccountCredential:(void(^)(BOOL result, NSError *error))complete;
- (void)validatedStashedAccountCredential:(void(^)(NSData *credential, NSError *error))complete;
- (void)stashAccountCredential:(NSData *)credential complete:(void(^)(bool success, NSError *error))complete;
- (void)ghostBust:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete;
- (void)ghostBustPeriodic:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete;
- (void)ghostBustTriggerTimed:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete;
- (void)ghostBustInfo: (void(^)(NSData *json, NSError *error))complete;
- (void)iCloudIdentityStatus_internal: (void(^)(NSDictionary *tableSpid, NSError *error))complete;
- (void)iCloudIdentityStatus: (void (^)(NSData *json, NSError *error))complete;

- (void)myPeerInfo:(void (^)(NSData *, NSError *))complete;
- (void)circleHash:(void (^)(NSString *, NSError *))complete;
- (void)circleJoiningBlob:(NSData *)applicant complete:(void (^)(NSData *blob, NSError *))complete;
- (void)joinCircleWithBlob:(NSData *)blob version:(PiggyBackProtocolVersion)version complete:(void (^)(bool success, NSError *))complete;
- (void)initialSyncCredentials:(uint32_t)flags complete:(void (^)(NSArray *, NSError *))complete;
- (void)importInitialSyncCredentials:(NSArray *)items complete:(void (^)(bool success, NSError *))complete;

- (void)rpcTriggerSync:(NSArray <NSString *> *)peers complete:(void(^)(bool success, NSError *))complete;

- (void)getWatchdogParameters:(void (^)(NSDictionary* parameters, NSError* error))complete;
- (void)setWatchdogParmeters:(NSDictionary*)parameters complete:(void (^)(NSError* error))complete;

- (void)rpcTriggerBackup:(NSArray<NSString *>*)backupPeers complete:(void (^)(NSError *error))complete;
- (void)rpcTriggerRingUpdate:(void (^)(NSError *error))complete;

- (void)removeV0Peers:(void(^)(bool removedV0Peer, NSError *error))reply;

@end
#endif


__END_DECLS

#endif
