//
//  SecSignatureVerificationSupport.h
//
//

#ifndef _SECURITY_SECSIGNATUREVERIFICATION_H_
#define _SECURITY_SECSIGNATUREVERIFICATION_H_

#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <Security/SecAsn1Types.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool SecVerifySignatureWithPublicKey(SecKeyRef publicKey, const SecAsn1AlgId *publicKeyAlgId,
                                     const uint8_t *dataToHash, size_t amountToHash,
                                     const uint8_t *signatureStart, size_t signatureSize,
                                     CFErrorRef *error)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);
#pragma clang diagnostic pop


#endif /* _SECURITY_SECSIGNATUREVERIFICATION_H_ */
