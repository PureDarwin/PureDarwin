

#ifndef SOSKVSKEYS_H
#define SOSKVSKEYS_H

#include "keychain/SecureObjectSync/SOSCircle.h"
#include "keychain/SecureObjectSync/SOSTransportMessageKVS.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
//
// MARK: Key formation
//

typedef enum {
    kCircleKey = 0,
    kMessageKey,
    kParametersKey,
    kInitialSyncKey,
    kRetirementKey,
    kAccountChangedKey,
    kDebugInfoKey,
    kRingKey,
    kLastCircleKey,
    kLastKeyParameterKey,
    kUnknownKey,
} SOSKVSKeyType;

extern const CFStringRef kSOSKVSKeyParametersKey;
extern const CFStringRef kSOSKVSInitialSyncKey;
extern const CFStringRef kSOSKVSAccountChangedKey;
extern const CFStringRef kSOSKVSRequiredKey;
extern const CFStringRef kSOSKVSOfficialDSIDKey;
extern const CFStringRef kSOSKVSLastCleanupTimestampKey;
extern const CFStringRef kSOSKVSOTRConfigVersion;
extern const CFStringRef kSOSKVSWroteLastKeyParams;

extern const CFStringRef sCirclePrefix;
extern const CFStringRef sRetirementPrefix;
extern const CFStringRef sDebugInfoPrefix;
extern const CFStringRef sRingPrefix;

SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key);
bool SOSKVSKeyParse(SOSKVSKeyType keyType, CFStringRef key, CFStringRef *circle, CFStringRef *peerInfo, CFStringRef *ring, CFStringRef *backupName, CFStringRef *from, CFStringRef *to);
SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *peerInfo, CFStringRef *ring, CFStringRef *backupName, CFStringRef *from, CFStringRef *to);

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error);
CFStringRef SOSRingKeyCreateWithName(CFStringRef ring_name, CFErrorRef *error);


CFStringRef SOSCircleKeyCreateWithName(CFStringRef name, CFErrorRef *error);
CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleNameAndPeerNames(CFStringRef circleName, CFStringRef from_peer_name, CFStringRef to_peer_name);

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer);

CFStringRef SOSRetirementKeyCreateWithCircleNameAndPeer(CFStringRef circle_name, CFStringRef retirement_peer_name);
CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name);

CFStringRef SOSMessageKeyCreateFromTransportToPeer(SOSMessage* transport, CFStringRef myID, CFStringRef peer_name);
CFStringRef SOSMessageKeyCreateFromPeerToTransport(SOSMessage* transport, CFStringRef myID, CFStringRef peer_name);
CFStringRef SOSLastKeyParametersPushedKeyCreateWithAccountGestalt(SOSAccount* account);

CFStringRef SOSRingKeyCreateWithRingName(CFStringRef ring_name);
CFStringRef SOSLastKeyParametersPushedKeyCreateWithPeerID(CFStringRef peerID);
CFStringRef SOSDebugInfoKeyCreateWithTypeName(CFStringRef type_name);

#endif
