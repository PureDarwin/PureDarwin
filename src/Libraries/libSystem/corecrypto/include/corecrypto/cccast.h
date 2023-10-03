#ifndef _CORECRYPTO_CCCAST_H_
#define _CORECRYPTO_CCCAST_H_

#include <corecrypto/ccmode.h>

#define CCCAST_BLOCK_SIZE 	8
#define CCCAST_KEY_LENGTH 	16
#define CCCAST_MIN_KEY_LENGTH 	5

const struct ccmode_ecb *cccast_ecb_decrypt_mode();
const struct ccmode_ecb *cccast_ecb_encrypt_mode();

const struct ccmode_cbc *cccast_cbc_decrypt_mode();
const struct ccmode_cbc *cccast_cbc_encrypt_mode();

const struct ccmode_cfb *cccast_cfb_decrypt_mode();
const struct ccmode_cfb *cccast_cfb_encrypt_mode();

const struct ccmode_cfb8 *cccast_cfb8_decrypt_mode();
const struct ccmode_cfb8 *cccast_cfb8_encrypt_mode();

const struct ccmode_ctr *cccast_ctr_crypt_mode();

const struct ccmode_ofb *cccast_ofb_crypt_mode();

extern const struct ccmode_ecb cccast_eay_ecb_decrypt_mode;
extern const struct ccmode_ecb cccast_eay_ecb_encrypt_mode;

#endif
