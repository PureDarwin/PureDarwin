//
//  SOSAuthKitHelpers.h
//  Security
//

#ifndef SOSAuthKitHelpers_h
#define SOSAuthKitHelpers_h

#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSTrustedDeviceAttributes.h"

@interface SOSAuthKitHelpers : NSObject
+ (NSString * _Nullable)machineID;
+ (void) activeMIDs:(void(^_Nonnull)(NSSet <SOSTrustedDeviceAttributes *> * _Nullable activeMIDs, NSError * _Nullable error))complete;
+ (bool) updateMIDInPeerInfo: (SOSAccount *_Nonnull) account;
+ (bool) peerinfoHasMID: (SOSAccount *_Nonnull) account;
+ (bool) accountIsHSA2;
- (id _Nullable) initWithActiveMIDS: (NSSet *_Nullable) theMidList;
- (bool) midIsValidInList: (NSString *_Nullable) machineId;
- (bool) serialIsValidInList: (NSString *_Nullable) serialNumber;
- (bool) isUseful;

#if __OBJC2__

@property (nonatomic, retain) NSSet * _Nullable midList;
@property (nonatomic, retain) NSSet * _Nullable machineIDs;
@property (nonatomic, retain) NSSet * _Nullable serialNumbers;

#endif /* __OBJC2__ */

@end

#endif /* SOSAuthKitHelpers_h */
