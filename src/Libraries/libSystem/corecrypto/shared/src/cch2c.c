#include <corecrypto/cch2c.h>
#include <stdio.h>
// #include <stdlib.h>

const struct cch2c_info cch2c_p256_sha256_sswu_ro_info = {0};
const struct cch2c_info cch2c_p384_sha512_sswu_ro_info = {0};
const struct cch2c_info cch2c_p521_sha512_sswu_ro_info = {0};

int cch2c(const struct cch2c_info *h2c_info, size_t dst_nbytes, const void *dst, size_t data_nbytes, const void *data, ccec_pub_ctx_t public) {
    printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}