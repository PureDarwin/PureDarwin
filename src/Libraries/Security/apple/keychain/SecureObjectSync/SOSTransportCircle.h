
#ifndef SOSTransportCircle_h
#define SOSTransportCircle_h
#import "keychain/SecureObjectSync/SOSPeerInfo.h"

@class SOSAccount;

@interface SOSCircleStorageTransport : NSObject
{
    SOSAccount* account;
}

@property (retain, nonatomic) SOSAccount* account;

-(id) init;
-(SOSCircleStorageTransport*) initWithAccount:(SOSAccount*)account;

-(CFIndex)circleGetTypeID;
-(CFIndex)getTransportType;
-(SOSAccount*)getAccount;

-(bool) expireRetirementRecords:(CFDictionaryRef) retirements err:(CFErrorRef *)error;

-(bool) flushChanges:(CFErrorRef *)error;
-(bool) postCircle:(CFStringRef)circleName circleData:(CFDataRef)circle_data err:(CFErrorRef *)error;
-(bool) postRetirement:(CFStringRef) circleName peer:(SOSPeerInfoRef) peer err:(CFErrorRef *)error;

-(CFDictionaryRef) CF_RETURNS_RETAINED handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error;
-(CFArrayRef)CF_RETURNS_RETAINED handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error;

@end
#endif
