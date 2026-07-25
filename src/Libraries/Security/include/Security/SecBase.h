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
