#ifndef _CORECRYPTO_CCH2C_H_
#define _CORECRYPTO_CCH2C_H_

#include <corecrypto/ccec.h>
#include <stddef.h>

struct cch2c_info {
    int dummy;
};

extern const struct cch2c_info cch2c_p256_sha256_sswu_ro_info;
extern const struct cch2c_info cch2c_p384_sha512_sswu_ro_info;
extern const struct cch2c_info cch2c_p521_sha512_sswu_ro_info;

int cch2c(const struct cch2c_info *h2c_info, size_t dst_nbytes, const void *dst, size_t data_nbytes, const void *data, ccec_pub_ctx_t public);

#endif // _CORECRYPTO_CCH2C_H_