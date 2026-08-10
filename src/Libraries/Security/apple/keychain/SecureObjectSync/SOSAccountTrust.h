//
//  SOSAccountTrust_h
//  Security

#ifndef SOSAccountTrust_h
#define SOSAccountTrust_h

#import <Foundation/Foundation.h>
#import "keychain/SecureObjectSync/SOSCircle.h"
#import "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#import "keychain/SecureObjectSync/SOSRing.h"

typedef bool (^SOSModifyCircleBlock)(SOSCircleRef circle);
typedef void (^SOSIteratePeerBlock)(SOSPeerInfoRef peerInfo);
typedef bool (^SOSModifyPeerBlock)(SOSPeerInfoRef peerInfo);
typedef bool (^SOSModifyPeerInfoBlock)(SOSFullPeerInfoRef fpi, CFErrorRef *error);
typedef SOSRingRef(^RingNameBlock)(CFStringRef name, SOSRingRef ring);
typedef void (^SOSModifyPeersInCircleBlock)(SOSCircleRef circle, CFMutableArrayRef appendPeersTo);

@interface SOSAccountTrust : NSObject
{
   NSMutableDictionary *   expansion;

    SOSFullPeerInfoRef      fullPeerInfo;
    SOSPeerInfoRef          peerInfo;
    NSString*               peerID;

    SOSCircleRef            trustedCircle;
    NSMutableSet *          retirees;
    enum DepartureReason    departureCode;

    SecKeyRef               _cachedOctagonSigningKey;
    SecKeyRef               _cachedOctagonEncryptionKey;
}
@property (strong, nonatomic)   NSMutableDictionary *   expansion;

@property (nonatomic)           SOSFullPeerInfoRef      fullPeerInfo;

// Convenince getters
@property (nonatomic, readonly) SOSPeerInfoRef          peerInfo;
@property (nonatomic, readonly) NSString*               peerID;


@property (nonatomic)           SOSCircleRef            trustedCircle;
@property (strong, nonatomic)   NSMutableSet *          retirees;
@property (nonatomic)           enum DepartureReason    departureCode;

@property (assign)              SecKeyRef               cachedOctagonSigningKey;
@property (assign)              SecKeyRef               cachedOctagonEncryptionKey;

+(instancetype)trust;

-(id)init;
-(id)initWithRetirees:(NSMutableSet*)retirees fpi:(SOSFullPeerInfoRef)identity circle:(SOSCircleRef) trusted_circle
        departureCode:(enum DepartureReason)code peerExpansion:(NSMutableDictionary*)expansion;


@end

#endif /* Trust_h */
