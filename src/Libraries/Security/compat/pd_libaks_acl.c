/* Storage for the ACL operation names declared in libaks_acl.h. */
#include <libaks_acl.h>

const CFStringRef kAKSKeyAcl                     = CFSTR("acl");
const CFStringRef kAKSKeyOpDefaultAcl            = CFSTR("dacl");
const CFStringRef kAKSKeyAclParamRequirePasscode = CFSTR("rpc");

const CFStringRef kAKSKeyOpEncrypt        = CFSTR("oe");
const CFStringRef kAKSKeyOpDecrypt        = CFSTR("od");
const CFStringRef kAKSKeyOpSign           = CFSTR("os");
const CFStringRef kAKSKeyOpDelete         = CFSTR("odel");
const CFStringRef kAKSKeyOpAttest         = CFSTR("oa");
const CFStringRef kAKSKeyOpComputeKey     = CFSTR("ock");
const CFStringRef kAKSKeyOpECIESTranscode = CFSTR("oet");
