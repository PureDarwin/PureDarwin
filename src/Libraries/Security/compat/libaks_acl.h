/*
 * The AppleKeyStore ACL operation names. libaks.h is closed source and ships in
 * no SDK; PureDarwin has no AppleKeyStore either, so nothing outside Security
 * ever sees these. They are CFStrings used as keys in the ACL dictionary that
 * Security both writes and reads, so they only have to be stable and distinct.
 */
#ifndef PD_LIBAKS_ACL_H
#define PD_LIBAKS_ACL_H

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

extern const CFStringRef kAKSKeyAcl;
extern const CFStringRef kAKSKeyOpDefaultAcl;
extern const CFStringRef kAKSKeyAclParamRequirePasscode;

extern const CFStringRef kAKSKeyOpEncrypt;
extern const CFStringRef kAKSKeyOpDecrypt;
extern const CFStringRef kAKSKeyOpSign;
extern const CFStringRef kAKSKeyOpDelete;
extern const CFStringRef kAKSKeyOpAttest;
extern const CFStringRef kAKSKeyOpComputeKey;
extern const CFStringRef kAKSKeyOpECIESTranscode;

__END_DECLS

#endif /* PD_LIBAKS_ACL_H */
