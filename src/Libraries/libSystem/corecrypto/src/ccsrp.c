#include <corecrypto/ccsrp.h>
#include <stdio.h>

bool ccsrp_client_verify_session(ccsrp_ctx_t srp, const uint8_t *HAMK_bytes) {
    printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccsrp_generate_verifier(ccsrp_ctx_t srp, const char *username, size_t password_len,
                            const void *password, size_t salt_len, const void *salt,
                            void *verifier) {
    printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccsrp_client_start_authentication(ccsrp_ctx_t srp, struct ccrng_state *rng, void *A_bytes) {
    printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccsrp_client_process_challenge(ccsrp_ctx_t srp, const void *username, size_t password_len,
                                   const void *password, size_t salt_len, const void *salt,
                                   const void *B_bytes, void *M_bytes) {
    printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
