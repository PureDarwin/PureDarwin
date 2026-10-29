// pd_cc_digest_bridge.c - CC_MD5 digest API over corecrypto for callers without CommonDigest.c
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// ld64 needs CC_MD5 for its output UUID and builds this file alone

#include <corecrypto/ccmd5.h>
#include <corecrypto/ccdigest.h>
#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>

// CC_MD5_CTX is a fixed 88-byte legacy layout and the corecrypto MD5 context needs 92
// the context lives on the heap and its pointer sits in the first 8 bytes of CC_MD5_CTX
struct pd_cc_md5_shim {
	void *real_ctx;
};

int
CC_MD5_Init(CC_MD5_CTX *c)
{
	struct pd_cc_md5_shim *shim = (struct pd_cc_md5_shim *)(void *)c;
	const struct ccdigest_info *di = ccmd5_di();

	shim->real_ctx = malloc(ccdigest_di_size(di));
	if (shim->real_ctx == NULL) return 0;
	ccdigest_init(di, (struct ccdigest_ctx *)shim->real_ctx);
	return 1;
}

int
CC_MD5_Update(CC_MD5_CTX *c, const void *data, CC_LONG len)
{
	struct pd_cc_md5_shim *shim = (struct pd_cc_md5_shim *)(void *)c;

	ccdigest_update(ccmd5_di(), (struct ccdigest_ctx *)shim->real_ctx, len, data);
	return 1;
}

int
CC_MD5_Final(unsigned char *md, CC_MD5_CTX *c)
{
	struct pd_cc_md5_shim *shim = (struct pd_cc_md5_shim *)(void *)c;

	ccdigest_final(ccmd5_di(), (struct ccdigest_ctx *)shim->real_ctx, md);
	free(shim->real_ctx);
	shim->real_ctx = NULL;
	return 1;
}

unsigned char *
CC_MD5(const void *data, CC_LONG len, unsigned char *md)
{
	CC_MD5_CTX c;

	CC_MD5_Init(&c);
	CC_MD5_Update(&c, data, len);
	CC_MD5_Final(md, &c);
	return md;
}
