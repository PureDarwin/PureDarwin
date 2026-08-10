//
//  SOSAccountTrustClassic+Expansion_h
//  Security
//
//

#ifndef SOSAccountTrustClassic_Expansion_h
#define SOSAccountTrustClassic_Expansion_h


#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"

#include <Security/SecureObjectSync/SOSViews.h>
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"

@interface SOSAccountTrustClassic (Expansion)

//Expansion Dictionary
//ring handling
-(bool) updateV2Dictionary:(SOSAccount*)account v2:(CFDictionaryRef) newV2Dict;
-(bool) handleUpdateRing:(SOSAccount*)account prospectiveRing:(SOSRingRef)prospectiveRing transport:(SOSKVSCircleStorageTransport*)circleTransport userPublicKey:(SecKeyRef)userPublic writeUpdate:(bool)writeUpdate err:(CFErrorRef *)error;
-(bool) resetRing:(SOSAccount*)account ringName:(CFStringRef) ringName err:(CFErrorRef *)error;
-(bool) resetAccountToEmpty:(SOSAccount*)account transport: (SOSCircleStorageTransport*)circleTransport err:(CFErrorRef*) error;

-(SOSRingRef) copyRing:(CFStringRef) ringName err:(CFErrorRef *)error;
-(CFMutableDictionaryRef) getRings:(CFErrorRef *)error;
-(bool) forEachRing:(RingNameBlock)block;
-(bool) setRing:(SOSRingRef) addRing ringName:(CFStringRef) ringName err:(CFErrorRef*)error;
//generic expansion
-(bool) ensureExpansion:(CFErrorRef *)error;
-(bool) clearValueFromExpansion:(CFStringRef) key err:(CFErrorRef *)error;
-(bool) setValueInExpansion:(CFStringRef) key value:(CFTypeRef) value err:(CFErrorRef *)error;
-(CFTypeRef) getValueFromExpansion:(CFStringRef)key err:(CFErrorRef*)error;
-(void) setRings:(CFMutableDictionaryRef) newrings;
-(bool) valueSetContainsValue:(CFStringRef) key value:(CFTypeRef) value;
-(void) valueUnionWith:(CFStringRef) key valuesToUnion:(CFSetRef) valuesToUnion;
-(void) valueSubtractFrom:(CFStringRef) key valuesToSubtract:(CFSetRef) valuesToSubtract;
-(void) pendEnableViewSet:(CFSetRef) enabledViews;
-(bool) resetAllRings:(SOSAccount*)account err:(CFErrorRef *)error;
-(bool) checkForRings:(CFErrorRef*)error;

@end

#endif /* SOSAccountTrustClassic_Expansion_h */
