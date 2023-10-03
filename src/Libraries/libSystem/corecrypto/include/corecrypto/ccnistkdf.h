#ifndef _CORECRYPTO_CCNIST_KDF_H_
#define _CORECRYPTO_CCNIST_KDF_H_

#include <corecrypto/ccdigest.h>

int ccnistkdf_ctr_hmac(const struct ccdigest_info *di,
                       size_t kdkLen, const void *kdk,
                       size_t labelLen, const void *label,
                       size_t contextLen, const void *context,
                       size_t dkLen, void *dk);

int ccnistkdf_ctr_hmac_fixed(const struct ccdigest_info *di,
                             size_t kdkLen, const void *kdk,
                             size_t fixedDataLen, const void *fixedData,
                             size_t dkLen, void *dk);

int ccnistkdf_fb_hmac(const struct ccdigest_info *di, int use_counter,
                      size_t kdkLen, const void *kdk,
                      size_t labelLen, const void *label,
                      size_t contextLen, const void *context,
                      size_t ivLen, const void *iv,
                      size_t dkLen, void *dk);

int ccnistkdf_fb_hmac_fixed(CC_UNUSED const struct ccdigest_info *di, int use_counter,
                            CC_UNUSED size_t kdkLen, CC_UNUSED const void *kdk,
                            CC_UNUSED size_t fixedDataLen, CC_UNUSED const void *fixedData,
                            CC_UNUSED size_t ivLen, CC_UNUSED const void *iv,
                            CC_UNUSED size_t dkLen, CC_UNUSED void *dk);

int ccnistkdf_dpi_hmac(const struct ccdigest_info *di,
                       size_t kdkLen, const void *kdk,
                       size_t labelLen, const void *label,
                       size_t contextLen, const void *context,
                       size_t dkLen, void *dk);

#endif

