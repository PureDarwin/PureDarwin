#ifndef _PD_SECRANDOM_H
#define _PD_SECRANDOM_H

#include <Security/SecBase.h>

CF_ASSUME_NONNULL_BEGIN

typedef const struct __SecRandom * SecRandomRef;

CF_EXPORT const SecRandomRef kSecRandomDefault;

CF_EXPORT int SecRandomCopyBytes(SecRandomRef __nullable rnd,
                                  size_t count, void * bytes)
    __attribute__((warn_unused_result));

CF_ASSUME_NONNULL_END

#endif /* _PD_SECRANDOM_H */
