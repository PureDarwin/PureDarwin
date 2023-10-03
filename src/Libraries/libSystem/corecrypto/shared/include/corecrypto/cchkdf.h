#ifndef _CCHKDF_H_
#define _CCHKDF_H_

#include <corecrypto/ccdigest.h>

int cchkdf(const struct ccdigest_info *di, size_t ikmLen, const void *ikm,
	size_t saltLen, const void *salt, size_t infoLen, const void *info,
	size_t dkLen, void *dk);

int cchkdf_extract(const struct ccdigest_info *di, size_t saltLen, const void *salt,
	size_t keyDerivationKeyLen, const void *keyDerivationKey, void *prk);

int cchkdf_expand(const struct ccdigest_info *di, size_t prkLen, const void *prk,
	size_t contextLen, const void *context, size_t derivedKeyLen, void *derivedKey);

#endif
