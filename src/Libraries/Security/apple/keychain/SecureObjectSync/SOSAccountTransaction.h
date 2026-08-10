//
//  SOSAccountTransaction.h
//  sec
//
//
//

#ifndef SOSAccountTransaction_h
#define SOSAccountTransaction_h

#include <CoreFoundation/CoreFoundation.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include <CoreFoundation/CFRuntime.h>

NS_ASSUME_NONNULL_BEGIN

@class SOSAccountTransaction;

@interface SOSAccount (Transaction)

+ (void)performOnQuietAccountQueue:(void (^)(void))action;

- (void) performTransaction: (void (^)(SOSAccountTransaction* txn)) action;
- (void) performTransaction_Locked: (void (^)(SOSAccountTransaction* txn)) action;

@end

@interface SOSAccountTransaction : NSObject

- (instancetype) init NS_UNAVAILABLE;
- (instancetype) initWithAccount: (SOSAccount*) account quiet:(bool)quiet NS_DESIGNATED_INITIALIZER;

- (void) finish;
- (void) restart;

- (void) requestSyncWith: (NSString*) peerID;
- (void) requestSyncWithPeers: (NSSet<NSString*>*) peerList;

@property SOSAccount *account;

@property (readonly) NSString* description;

@end

NS_ASSUME_NONNULL_END

#endif /* SOSAccountTransaction_h */
