#ifndef _CORECRYPTO_CCWRAP_PRIV_H_
#define _CORECRYPTO_CCWRAP_PRIV_H_

#include <corecrypto/ccmode.h>

#define CCWRAP_SEMIBLOCK 8

int ccwrap_auth_decrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t wrappedLen, const void* wrappedKey, size_t* unwrappedLen, void* unwrappedKey, const void* iv);

int ccwrap_auth_encrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t unwrappedLen, const void* unwrappedKey, size_t* wrappedLen, void* wrappedKey, const void* iv);

size_t ccwrap_unwrapped_size(size_t wrappedLen);

size_t ccwrap_wrapped_size(size_t unwrappedLen);

#endif /* _CORECRYPTO_CCWRAP_PRIV_H_ */
