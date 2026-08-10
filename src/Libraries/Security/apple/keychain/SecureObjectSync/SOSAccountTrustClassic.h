//
//  SOSAccountTrustClassic.h
//  Security
//

#ifndef SOSAccountTrustClassic_h
#define SOSAccountTrustClassic_h

#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#import "keychain/SecureObjectSync/SOSAccountTrust.h"
#import "keychain/SecureObjectSync/SOSTypes.h"
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
@class SOSAccount;

@interface SOSAccountTrustClassic : SOSAccountTrust

+(instancetype)trustClassic;

//Gestalt Dictionary
-(bool) updateGestalt:(SOSAccount*)account newGestalt:(CFDictionaryRef) new_gestalt;

//Peers
-(SOSPeerInfoRef) copyPeerWithID:(CFStringRef) peerid err:(CFErrorRef *)error;
-(bool) isAccountIdentity:(SOSPeerInfoRef)peerInfo err:(CFErrorRef *)error;
-(SecKeyRef) copyPublicKeyForPeer:(CFStringRef) peer_id err:(CFErrorRef *)error;
-(CFSetRef) copyPeerSetMatching:(SOSModifyPeerBlock)block;
-(CFArrayRef) copyPeersToListenTo:(SecKeyRef)userPublic err:(CFErrorRef *)error;
-(bool) peerSignatureUpdate:(SecKeyRef)privKey err:(CFErrorRef *)error;
-(bool) updatePeerInfo:(SOSKVSCircleStorageTransport*)circleTransport description:(CFStringRef)updateDescription err:(CFErrorRef *)error update:(SOSModifyPeerInfoBlock)block;
-(bool) addEscrowToPeerInfo:(SOSFullPeerInfoRef) myPeer err:(CFErrorRef *)error;

//Views
-(SOSViewResultCode) updateView:(SOSAccount*)account name:(CFStringRef) viewname code:(SOSViewActionCode) actionCode err:(CFErrorRef *)error;
-(SOSViewResultCode) viewStatus:(SOSAccount*)account name:(CFStringRef) viewname err:(CFErrorRef *)error;
-(bool) updateViewSets:(SOSAccount*)account enabled:(CFSetRef) origEnabledViews disabled:(CFSetRef) origDisabledViews;

-(CFSetRef) copyPeerSetForView:(CFStringRef) viewName;

//DER
-(size_t) getDEREncodedSize:(SOSAccount*)account err:(NSError**)error;
-(uint8_t*) encodeToDER:(SOSAccount*)account err:(NSError**) error start:(const uint8_t*) der end:(uint8_t*)der_end;

//Syncing
-(CFMutableSetRef) CF_RETURNS_RETAINED syncWithPeers:(SOSAccountTransaction*) txn peerIDs:(CFSetRef) /* CFStringRef */ peerIDs err:(CFErrorRef *)error;
-(bool) requestSyncWithAllPeers:(SOSAccountTransaction*) txn key:(SecKeyRef)userPublic err:(CFErrorRef *)error;
-(SOSEngineRef) getDataSourceEngine:(SOSDataSourceFactoryRef)factory;


-(bool) postDebugScope:(SOSCircleStorageTransport*) circle_transport scope:(CFTypeRef) scope err:(CFErrorRef*)error;
-(SecKeyRef) copyDeviceKey:(CFErrorRef *)error;
-(void) addSyncablePeerBlock:(SOSAccountTransaction*)a dsName:(CFStringRef) ds_name change:(SOSAccountSyncablePeersBlock) changeBlock;
-(bool) clientPing:(SOSAccount*)account;
-(void) removeInvalidApplications:(SOSCircleRef) circle userPublic:(SecKeyRef)userPublic;

-(bool) addiCloudIdentity:(SOSCircleRef) circle key:(SecKeyRef) userKey err:(CFErrorRef*)error;
-(bool) removeIncompleteiCloudIdentities:(SOSCircleRef) circle privKey:(SecKeyRef) privKey err:(CFErrorRef *)error;
-(bool) upgradeiCloudIdentity:(SOSCircleRef) circle privKey:(SecKeyRef) privKey;
-(CFMutableSetRef) copyPreApprovedHSA2Info;

-(void) addRingDictionary;
-(void) resetRingDictionary;


@end
#endif /* SOSAccountTrustClassic_h */
