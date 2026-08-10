//
//  SOSPeerRateLimiter.h
//  Security
//
#import "keychain/ckks/RateLimiter.h"
#include "keychain/SecureObjectSync/SOSPeer.h"

#ifndef SOSPeerRateLimiter_h
#define SOSPeerRateLimiter_h

enum RateLimitState{
    RateLimitStateCanSend = 1,
    RateLimitStateHoldMessage = 2
};

@interface  PeerRateLimiter : RateLimiter
{
    NSString *peerID;
}

@property (retain) NSString *peerID;
@property (retain) NSMutableDictionary *accessGroupRateLimitState;
@property (retain) NSMutableDictionary *accessGroupToTimer;
@property (retain) NSMutableDictionary *accessGroupToNextMessageToSend;

-(instancetype)initWithPeer:(SOSPeerRef)peer;
-(NSDictionary *) setUpConfigForPeer;
-(enum RateLimitState) stateForAccessGroup:(NSString*) accessGroup;
@end

@interface KeychainItem : NSObject
@property (atomic, retain) NSString* accessGroup;
-(instancetype) initWithAccessGroup:(NSString*)accessGroup;
@end

#endif /* SOSPeerRateLimiter_h */
