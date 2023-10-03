#include <corecrypto/ccmode.h>
#include <corecrypto/ccmode_factory.h>

void ccmode_factory_cbc_encrypt(struct ccmode_cbc* cbc, const struct ccmode_ecb* ecb) {
	*cbc = (struct ccmode_cbc) CCMODE_FACTORY_CBC_ENCRYPT(ecb);
};

void ccmode_factory_cbc_decrypt(struct ccmode_cbc* cbc, const struct ccmode_ecb* ecb) {
	*cbc = (struct ccmode_cbc) CCMODE_FACTORY_CBC_DECRYPT(ecb);
};

void ccmode_factory_gcm_encrypt(struct ccmode_gcm* gcm, const struct ccmode_ecb* ecb_encrypt) {
	*gcm = (struct ccmode_gcm) {
		.size = sizeof(struct _ccmode_gcm_key) + GCM_ECB_KEY_SIZE(ecb_encrypt),
		.encdec = CCMODE_GCM_ENCRYPTOR,
		.block_size = 1,
		.init = ccmode_gcm_init,
		.set_iv = ccmode_gcm_set_iv,
		.gmac = ccmode_gcm_aad,
		.gcm = ccmode_gcm_encrypt,
		.finalize = ccmode_gcm_finalize,
		.reset = ccmode_gcm_reset,
		.custom = ecb_encrypt,
	};
};

void ccmode_factory_gcm_decrypt(struct ccmode_gcm* gcm, const struct ccmode_ecb* ecb_encrypt) {
	*gcm = (struct ccmode_gcm) {
		.size = sizeof(struct _ccmode_gcm_key) + GCM_ECB_KEY_SIZE(ecb_encrypt),
		.encdec = CCMODE_GCM_DECRYPTOR,
		.block_size = 1,
		.init = ccmode_gcm_init,
		.set_iv = ccmode_gcm_set_iv,
		.gmac = ccmode_gcm_aad,
		.gcm = ccmode_gcm_decrypt,
		.finalize = ccmode_gcm_finalize,
		.reset = ccmode_gcm_reset,
		.custom = ecb_encrypt,
	};
};

void ccmode_factory_cfb_encrypt(struct ccmode_cfb* cfb, const struct ccmode_ecb* ecb) {
	*cfb = (struct ccmode_cfb) CCMODE_FACTORY_CFB_ENCRYPT(ecb);
};

void ccmode_factory_cfb_decrypt(struct ccmode_cfb* cfb, const struct ccmode_ecb* ecb) {
	*cfb = (struct ccmode_cfb) CCMODE_FACTORY_CFB_DECRYPT(ecb);
};

void ccmode_factory_cfb8_encrypt(struct ccmode_cfb8* cfb8, const struct ccmode_ecb* ecb) {
	*cfb8 = (struct ccmode_cfb8) CCMODE_FACTORY_CFB8_ENCRYPT(ecb);
};

void ccmode_factory_cfb8_decrypt(struct ccmode_cfb8* cfb8, const struct ccmode_ecb* ecb) {
	*cfb8 = (struct ccmode_cfb8) CCMODE_FACTORY_CFB8_DECRYPT(ecb);
};

void ccmode_factory_ctr_crypt(struct ccmode_ctr* ctr, const struct ccmode_ecb* ecb_encrypt) {
	*ctr = (struct ccmode_ctr) CCMODE_FACTORY_CTR_CRYPT(ecb_encrypt);
};

void ccmode_factory_ccm_encrypt(struct ccmode_ccm* ccm, const struct ccmode_ecb* ecb_encrypt) {
	*ccm = (struct ccmode_ccm) CCMODE_FACTORY_CCM_ENCRYPT(ecb_encrypt);
};

void ccmode_factory_ccm_decrypt(struct ccmode_ccm* ccm, const struct ccmode_ecb* ecb_encrypt) {
	*ccm = (struct ccmode_ccm) CCMODE_FACTORY_CCM_DECRYPT(ecb_encrypt);
};

void ccmode_factory_ofb_crypt(struct ccmode_ofb* ofb, const struct ccmode_ecb* ecb) {
	*ofb = (struct ccmode_ofb) CCMODE_FACTORY_OFB_CRYPT(ecb);
};

void ccmode_factory_xts_encrypt(struct ccmode_xts* xts, const struct ccmode_ecb* ecb, const struct ccmode_ecb* ecb_encrypt) {
	*xts = (struct ccmode_xts) CCMODE_FACTORY_XTS_ENCRYPT(ecb, ecb_encrypt);
};

void ccmode_factory_xts_decrypt(struct ccmode_xts* xts, const struct ccmode_ecb* ecb, const struct ccmode_ecb* ecb_encrypt) {
	*xts = (struct ccmode_xts) CCMODE_FACTORY_XTS_DECRYPT(ecb, ecb_encrypt);
};
