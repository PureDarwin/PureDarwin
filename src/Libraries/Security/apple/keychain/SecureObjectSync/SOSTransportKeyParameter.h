
#ifndef SOSTransportKeyParameter_h
#define SOSTransportKeyParameter_h

#include "keychain/SecureObjectSync/SOSAccountPriv.h"

@interface CKKeyParameter : NSObject
{
    SOSAccount* account;
}

@property (atomic) SOSAccount* account;

-(id) initWithAccount:(SOSAccount*) account;

-(bool) SOSTransportKeyParameterPublishCloudParameters:(CKKeyParameter*) transport data:(CFDataRef)newParameters err:(CFErrorRef*) error;

-(bool) SOSTransportKeyParameterHandleKeyParameterChanges:(CKKeyParameter*) transport  data:(CFDataRef) data err:(CFErrorRef) error;
-(void) SOSTransportKeyParameterHandleNewAccount:(CKKeyParameter*) transport acct:(SOSAccount*) account;

-(SOSAccount*) SOSTransportKeyParameterGetAccount:(CKKeyParameter*) transport;
-(CFIndex) SOSTransportKeyParameterGetTransportType:(CKKeyParameter*) transport err:(CFErrorRef *)error;

-(bool) SOSTransportKeyParameterKVSAppendKeyInterests:(CKKeyParameter*)transport ak:(CFMutableArrayRef)alwaysKeys firstUnLock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error;

@end


#endif
