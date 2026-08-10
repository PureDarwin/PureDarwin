

#ifndef sec_SOSTransportCircleKVS_h
#define sec_SOSTransportCircleKVS_h

#import "SOSTransportCircle.h"
@class SOSCircleStorageTransport;

@interface SOSKVSCircleStorageTransport : SOSCircleStorageTransport
{
    NSMutableDictionary *pending_changes;
    NSString            *circleName;
}

@property (retain, nonatomic)   NSMutableDictionary *pending_changes;
@property (retain, nonatomic)   NSString            *circleName;


-(id)init;
-(id)initWithAccount:(SOSAccount*)acct andCircleName:(NSString*)name;
-(NSString*) getCircleName;
-(bool) flushChanges:(CFErrorRef *)error;

-(void)kvsAddToPendingChanges:(CFStringRef) message_key data:(CFDataRef)message_data;
-(bool)kvsSendPendingChanges:(CFErrorRef *)error;

-(bool)kvsAppendKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef) afterFirstUnlockKeys unlocked:(CFMutableArrayRef)unlockedKeys err:(CFErrorRef *)error;
-(bool)kvsAppendRingKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error;
-(bool)kvsAppendDebugKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error;

-(bool) kvsRingFlushChanges:(CFErrorRef*) error;
-(bool) kvsRingPostRing:(CFStringRef) ringName ring:(CFDataRef) ring err:(CFErrorRef *)error;

-(bool) kvssendDebugInfo:(CFStringRef) type debug:(CFTypeRef) debugInfo  err:(CFErrorRef *)error;
-(bool) kvsSendOfficialDSID:(CFStringRef) dsid err:(CFErrorRef *)error;

-(bool) kvsSendAccountChangedWithDSID:(CFStringRef) dsid err:(CFErrorRef *)error;

@end;

#endif
