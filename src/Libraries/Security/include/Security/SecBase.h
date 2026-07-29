#ifndef _PD_SECBASE_H
#define _PD_SECBASE_H

#include <CoreFoundation/CoreFoundation.h>

typedef int32_t OSStatus;

CF_ASSUME_NONNULL_BEGIN

enum {
    errSecSuccess           = 0,
    errSecUnimplemented     = -4,
    errSecParam             = -50,
    errSecAllocate          = -108,
    errSecNotAvailable      = -25291,
    errSecDuplicateItem     = -25299,
    errSecItemNotFound      = -25300,
    errSecInteractionNotAllowed = -25308,
};

CF_ASSUME_NONNULL_END

#endif /* _PD_SECBASE_H */

/*
 * Keychain Services. PureDarwin has no securityd and therefore no keychain, so
 * only the opaque type is declared - SystemConfiguration's
 * SCPreferencesKeychainPrivate.h needs it to parse.
 */
typedef struct OpaqueSecKeychainRef *SecKeychainRef;
typedef struct OpaqueSecKeychainItemRef *SecKeychainItemRef;
typedef struct OpaqueSecAccessRef *SecAccessRef;
