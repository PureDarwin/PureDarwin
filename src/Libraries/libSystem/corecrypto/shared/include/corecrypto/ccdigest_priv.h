/*
 *  ccdigest_priv.h
 *  corecrypto
 *
 *  Created on 12/07/2010
 *
 *  Copyright (c) 2010,2011,2012,2015 Apple Inc. All rights reserved.
 *
 */

#ifndef _CORECRYPTO_CCDIGEST_PRIV_H_
#define _CORECRYPTO_CCDIGEST_PRIV_H_

#include <corecrypto/ccdigest.h>
#include <corecrypto/ccasn1.h>

void ccdigest_final_common(const struct ccdigest_info *di,
                           ccdigest_ctx_t ctx, void *digest);
void ccdigest_final_64be(const struct ccdigest_info *di, ccdigest_ctx_t,
                         unsigned char *digest);
void ccdigest_final_64le(const struct ccdigest_info *di, ccdigest_ctx_t,
                         unsigned char *digest);

CC_INLINE CC_NONNULL_TU((1))
bool ccdigest_oid_equal(const struct ccdigest_info *di, ccoid_t oid) {
    if(di->oid == NULL && CCOID(oid) == NULL) return true;
    if(di->oid == NULL || CCOID(oid) == NULL) return false;
    return ccoid_equal(di->oid, oid);
}

typedef const struct ccdigest_info *(ccdigest_lookup)(ccoid_t oid);

//#include <stdarg.h>

// NOTE(@facekapow):
// i'm not sure entirely sure what Apple was going for with the varargs,
// especially since there was no documentation in the header
//
// my initial assumption was that it was used to try multiple possible OIDs
// and return the digest info for the first one that was found. (un?)fortunately,
// there's no indication of how to determine the length of the varargs
// (NULL could be used as the last element)
//
// therefore, i'm just going to remove the varargs. besides, it's in a private header
// which means that only our own implementation should use it, so we can modify it
// however we want
const struct ccdigest_info *ccdigest_oid_lookup(ccoid_t oid /*, ...*/);

#endif /* _CORECRYPTO_CCDIGEST_PRIV_H_ */
