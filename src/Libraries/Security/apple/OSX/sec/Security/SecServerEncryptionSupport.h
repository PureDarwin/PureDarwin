//
//  SecServerEncryptionSupport.h
//
//

#ifndef _SECURITY_SECSERVERENCRYPTIONSUPPORT_H_
#define _SECURITY_SECSERVERENCRYPTIONSUPPORT_H_

#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <Security/SecTrust.h>

// Deprecating for security motives (28715251).
// Compatible implementation still available in SecKey with
// kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM but should also be
// deprecated for the same reason (28496795).

CFDataRef SecCopyEncryptedToServer(SecTrustRef trustedEvaluation, CFDataRef dataToEncrypt, CFErrorRef *error)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_12, __MAC_10_13, __IPHONE_8_0, __IPHONE_11_0, "Migrate to SecKeyCreateEncryptedData with kSecKeyAlgorithmECIESEncryptionStandardVariableIV* or Security Foundation SFIESOperation for improved security (encryption is not compatible)");

//
// For testing
//
/* Caution: These functions take an iOS SecKeyRef. Careful use is required on OS X. */
CFDataRef SecCopyDecryptedForServer(SecKeyRef serverFullKey, CFDataRef encryptedData, CFErrorRef* error)
    API_DEPRECATED("Migrate to SecKeyCreateEncryptedData with kSecKeyAlgorithmECIESEncryptionStandardVariableIV* or Security Foundation SFIESOperation for improved security (encryption is not compatible)", macos(10.12,10.13), ios(8.0,11.0));
// SFIESCiphertext


CFDataRef SecCopyEncryptedToServerKey(SecKeyRef publicKey, CFDataRef dataToEncrypt, CFErrorRef *error)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_12, __MAC_10_13, __IPHONE_8_0, __IPHONE_11_0,"Migrate to SecKeyCreateEncryptedData with kSecKeyAlgorithmECIESEncryptionStandardVariableIV* or Security Foundation SFIESOperation for improved security (encryption is not compatible)");

#endif
