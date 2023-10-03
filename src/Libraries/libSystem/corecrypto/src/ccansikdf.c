#include <corecrypto/ccansikdf.h>
#include <corecrypto/ccstubs.h>

int ccansikdf_x963(const struct ccdigest_info *di,
                   const size_t Z_len, const unsigned char *Z,
                   const size_t sharedinfo_byte_len,
		   const void *sharedinfo, const size_t key_len,
		   uint8_t *key) {
	CC_STUB_ERR();
}
