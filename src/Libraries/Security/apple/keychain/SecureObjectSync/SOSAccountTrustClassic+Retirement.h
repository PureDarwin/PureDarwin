//
//  SOSAccountTrustClassic+Retirement.h
//  Security
//

#ifndef SOSAccountTrustClassic_Retirement_h
#define SOSAccountTrustClassic_Retirement_h

#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"

@interface SOSAccountTrustClassic (Retirement)
//Retirement
-(bool) cleanupAfterPeer:(SOSMessageKVS*)kvsTransport circleTransport:(SOSCircleStorageTransport*)circleTransport seconds:(size_t) seconds circle:(SOSCircleRef) circle cleanupPeer:(SOSPeerInfoRef) cleanupPeer err:(CFErrorRef*) error;
-(bool) cleanupRetirementTickets:(SOSAccount*)account circle:(SOSCircleRef)circle time:(size_t) seconds err:(CFErrorRef*) error;
@end
#endif /* SOSAccountTrustClassic_Retirement_h */
