/*
 * libaks_acl_cf_keys.h - compatibility shim for the AppleKeyStore ACL keys.
 */

#ifndef _PD_LIBAKS_ACL_CF_KEYS_H_
#define _PD_LIBAKS_ACL_CF_KEYS_H_

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

/* The ACL itself, and the one constraint parameter Security references. */
extern const CFStringRef kAKSKeyAcl;
extern const CFStringRef kAKSKeyAclParamRequirePasscode;

/* Operations an ACL may permit. kAKSKeyOpDefaultAcl stands for "the operation
 * set applied when none is given explicitly". */
extern const CFStringRef kAKSKeyOpDefaultAcl;
extern const CFStringRef kAKSKeyOpEncrypt;
extern const CFStringRef kAKSKeyOpDecrypt;
extern const CFStringRef kAKSKeyOpSign;
extern const CFStringRef kAKSKeyOpComputeKey;
extern const CFStringRef kAKSKeyOpAttest;
extern const CFStringRef kAKSKeyOpTranscrypt;
extern const CFStringRef kAKSKeyOpECIESTranscode;
extern const CFStringRef kAKSKeyOpDelete;

__END_DECLS

#endif /* _PD_LIBAKS_ACL_CF_KEYS_H_ */
