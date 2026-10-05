// host stand-in for the kernel AES-CBC API over OpenSSL
#include <openssl/aes.h>
#include <string.h>
typedef AES_KEY aes_encrypt_ctx;
typedef AES_KEY aes_decrypt_ctx;
static inline int aes_encrypt_key128(const unsigned char *k, aes_encrypt_ctx *c) { return AES_set_encrypt_key(k, 128, c); }
static inline int aes_decrypt_key128(const unsigned char *k, aes_decrypt_ctx *c) { return AES_set_decrypt_key(k, 128, c); }
static inline int aes_encrypt_cbc(const unsigned char *in, const unsigned char *iv, unsigned int n, unsigned char *out, aes_encrypt_ctx *c)
{ unsigned char v[16]; memcpy(v, iv, 16); AES_cbc_encrypt(in, out, 16 * n, c, v, AES_ENCRYPT); return 0; }
static inline int aes_decrypt_cbc(const unsigned char *in, const unsigned char *iv, unsigned int n, unsigned char *out, aes_decrypt_ctx *c)
{ unsigned char v[16]; memcpy(v, iv, 16); AES_cbc_encrypt(in, out, 16 * n, c, v, AES_DECRYPT); return 0; }
