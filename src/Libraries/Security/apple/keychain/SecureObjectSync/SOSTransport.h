

#ifndef SOSTransport_h
#define SOSTransport_h
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSTransportKeyParameter.h"
#include "keychain/SecureObjectSync/SOSAccount.h"

CF_RETURNS_RETAINED CFMutableArrayRef SOSTransportDispatchMessages(SOSAccountTransaction* txn, CFDictionaryRef updates, CFErrorRef *error);

void SOSRegisterTransportMessage(SOSMessage* additional);
void SOSUnregisterTransportMessage(SOSMessage* removal);

void SOSRegisterTransportCircle(SOSCircleStorageTransport* additional);
void SOSUnregisterTransportCircle(SOSCircleStorageTransport* removal);

void SOSRegisterTransportKeyParameter(CKKeyParameter* additional);
void SOSUnregisterTransportKeyParameter(CKKeyParameter* removal);
void SOSUnregisterAllTransportMessages(void);
void SOSUnregisterAllTransportCircles(void);
void SOSUnregisterAllTransportKeyParameters(void);


void SOSUpdateKeyInterest(SOSAccount* account);

enum TransportType{
    kUnknown = 0,
    kKVS = 1,
    kIDS = 2,
    kBackupPeer = 3,
    kIDSTest = 4,
    kKVSTest = 5,
    kCK = 6
};

static inline CFMutableDictionaryRef CFDictionaryEnsureCFDictionaryAndGetCurrentValue(CFMutableDictionaryRef dict, CFTypeRef key)
{
    CFMutableDictionaryRef result = (CFMutableDictionaryRef) CFDictionaryGetValue(dict, key);

    if (!isDictionary(result)) {
        result = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(dict, key, result);
        CFReleaseSafe(result);
    }

    return result;
}

#endif
