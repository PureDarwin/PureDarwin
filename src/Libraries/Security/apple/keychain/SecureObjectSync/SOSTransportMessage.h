
#ifndef SOSTransportMessage_h
#define SOSTransportMessage_h

#import <Foundation/Foundation.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"

@interface SOSMessage : NSObject
{
    CFTypeRef engine;
    SOSAccount *account;
    NSString *circleName;
}
@property (atomic)  CFTypeRef engine;
@property (strong, atomic)  SOSAccount* account;
@property (strong, atomic)  NSString *circleName;

-(id) initWithAccount:(SOSAccount*)acct andName:(NSString*)name;

-(CFIndex) SOSTransportMessageGetTransportType;
-(CFStringRef) SOSTransportMessageGetCircleName;
-(CFTypeRef) SOSTransportMessageGetEngine;
-(SOSAccount*) SOSTransportMessageGetAccount;

-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessage*) transport peers:(CFDictionaryRef) peers err:(CFErrorRef*) error;

-(bool) SOSTransportMessageSendMessage:(SOSMessage*) transport id:(CFStringRef) peerID messageToSend:(CFDataRef) message err:(CFErrorRef *)error;
-(bool) SOSTransportMessageSendMessages:(SOSMessage*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error;
-(bool) SOSTransportMessageFlushChanges:(SOSMessage*) transport err:(CFErrorRef *)error;

-(bool) SOSTransportMessageSyncWithPeers:(SOSMessage*) transport p:(CFSetRef) peers err:(CFErrorRef *)error;

-(CFDictionaryRef)CF_RETURNS_RETAINED SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessage*) transport peerMessages:(CFMutableDictionaryRef) circle_peer_messages_table err:(CFErrorRef *)error;

-(bool) SOSTransportMessageHandlePeerMessage:(SOSMessage*) transport id:(CFStringRef) peer_id cm:(CFDataRef) codedMessage err:(CFErrorRef *)error;
-(bool) SOSTransportMessageSendMessageIfNeeded:(SOSMessage*) transport circleName:(CFStringRef) circle_id pID:(CFStringRef) peer_id err:(CFErrorRef *)error;
//for testing
bool SOSEngineHandleCodedMessage(SOSAccount* account, SOSEngineRef engine, CFStringRef peerID, CFDataRef codedMessage, CFErrorRef*error);
    
@end

#endif
