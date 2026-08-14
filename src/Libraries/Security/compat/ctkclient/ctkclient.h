/*
 * CryptoTokenKit's client SPI. The framework is closed source and PureDarwin
 * has no token daemon, so this declares the surface Security uses and the
 * implementation reports "no token" for every request; SecItem falls back to
 * ordinary keychain storage on that path.
 */
#ifndef PD_CTKCLIENT_H
#define PD_CTKCLIENT_H

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

typedef struct __TKToken *TKTokenRef;

#define kTKErrorDomain "com.apple.CryptoTokenKit"

enum {
    kTKErrorCodeBadParameter          = -1,
    kTKErrorCodeNotImplemented        = -2,
    kTKErrorCodeCommunicationError    = -3,
    kTKErrorCodeCorruptedData         = -4,
    kTKErrorCodeCanceledByUser        = -5,
    kTKErrorCodeAuthenticationFailed  = -6,
    kTKErrorCodeObjectNotFound        = -7,
    kTKErrorCodeTokenNotFound         = -8,
    kTKErrorCodeAuthenticationNeeded  = -9,
};

#define kTKTokenCreateAttributeAuxParams        "auxParams"
#define kTKTokenCreateAttributeTestMode         "testMode"

#define kTKTokenControlAttribAttestingKey       "attestingKey"
#define kTKTokenControlAttribKeyToAttest        "keyToAttest"
#define kTKTokenControlAttribAttestationData    "attestationData"
#define kTKTokenControlAttribLifetimeControlKey "lifetimeControlKey"
#define kTKTokenControlAttribLifetimeType       "lifetimeType"

TKTokenRef TKTokenCreate(CFDictionaryRef attributes, CFErrorRef *error);
CFDataRef TKTokenCopyObjectData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error);
CFDataRef TKTokenCopyObjectAccessControl(TKTokenRef token, CFTypeRef objectOrAttributes, CFErrorRef *error);
CFDataRef TKTokenCopyPublicKeyData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error);
CFDataRef TKTokenCreateOrUpdateObject(TKTokenRef token, CFDataRef objectID,
                                      CFMutableDictionaryRef attributes, CFErrorRef *error);
bool TKTokenDeleteObject(TKTokenRef token, CFDataRef objectID, CFErrorRef *error);
CFTypeRef TKTokenCopyOperationResult(TKTokenRef token, CFDataRef objectID, CFIndex operation,
                                     CFArrayRef algorithms, CFIndex mode,
                                     CFTypeRef in1, CFTypeRef in2, CFErrorRef *error);
CFDictionaryRef TKTokenControl(TKTokenRef token, CFDictionaryRef attributes, CFErrorRef *error);

__END_DECLS

#endif /* PD_CTKCLIENT_H */
