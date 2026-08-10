//
//  SOSAccountTrustClassic+Circle.h
//  Security
//

#ifndef SOSAccountTrustClassic_Circle_h
#define SOSAccountTrustClassic_Circle_h

#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"

@interface SOSAccountTrustClassic (Circle)
//Circle
-(SOSCCStatus) getCircleStatusOnly:(CFErrorRef*) error;
-(SOSCircleRef) ensureCircle:(SOSAccount*)account name:(CFStringRef)name err:(CFErrorRef *)error;
-(bool) modifyCircle:(SOSCircleStorageTransport*)circleTransport err:(CFErrorRef*)error action:(SOSModifyCircleBlock)block;
-(SOSCircleRef) getCircle:(CFErrorRef *)error;
-(bool) hasCircle:(CFErrorRef*) error;
-(void) generationSignatureUpdateWith:(SOSAccount*)account key:(SecKeyRef) privKey;
-(bool) isInCircleOnly:(CFErrorRef *)error;
-(void) forEachCirclePeerExceptMe:(SOSIteratePeerBlock)block;
-(bool) leaveCircle:(SOSAccount*)account err:(CFErrorRef*) error;
-(bool) leaveCircleWithAccount:(SOSAccount*)account err:(CFErrorRef*) error;
-(bool) resetToOffering:(SOSAccountTransaction*) aTxn key:(SecKeyRef)userKey err:(CFErrorRef*) error;
-(bool) resetCircleToOffering:(SOSAccountTransaction*) aTxn userKey:(SecKeyRef) user_key err:(CFErrorRef *)error;
-(SOSCCStatus) thisDeviceStatusInCircle:(SOSCircleRef) circle peer:(SOSPeerInfoRef) this_peer;
-(bool) updateCircle:(SOSCircleStorageTransport*)circleTransport newCircle:(SOSCircleRef) newCircle err:(CFErrorRef*)error;

-(bool) updateCircleFromRemote:(SOSCircleStorageTransport*)circleTransport newCircle:(SOSCircleRef)newCircle err:(CFErrorRef*)error;

-(CFArrayRef) copySortedPeerArray:(CFErrorRef *)error
                           action:(SOSModifyPeersInCircleBlock)block;
-(bool) handleUpdateCircle:(SOSCircleRef) prospective_circle transport:(SOSCircleStorageTransport*)circleTransport update:(bool) writeUpdate err:(CFErrorRef*)error;
-(bool) joinCircle:(SOSAccountTransaction*) aTxn userKey:(SecKeyRef)user_key useCloudPeer:(bool)use_cloud_peer err:(CFErrorRef*) error;
-(bool) fixICloudIdentities:(SOSAccount *) account circle: (SOSCircleRef) circle;

@end

#endif /* SOSAccountTrustClassic_Circle_h */
