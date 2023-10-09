#ifndef _CORECRYPTO_CCANSIKDF_H_
#define _CORECRYPTO_CCANSIKDF_H_

#include <corecrypto/ccdigest.h>
#include <corecrypto/cc_priv.h>

CC_NONNULL((1, 3, 7))
int ccansikdf_x963(const struct ccdigest_info *di,
                   const size_t Z_len, const unsigned char *Z,
                   const size_t sharedinfo_byte_len,
		   const void *sharedinfo, const size_t key_len,
		   uint8_t *key);

#endif
