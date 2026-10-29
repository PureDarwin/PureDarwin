// CommonCollabKeyGen.h - private CommonCrypto collaborative key generation interfaces
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _CC_COLLAB_KEY_GEN_H_
#define _CC_COLLAB_KEY_GEN_H_

#include <stddef.h>
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonECCryptor.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CCCKGContributor *CCCKGContributorRef;
typedef struct CCCKGOwner *CCCKGOwnerRef;
typedef const struct CCCKG2ParamsInfo *CCCKG2Params;

size_t CCCKGGetCommitmentSize(size_t nbits, CCDigestAlgorithm alg);
size_t CCCKGGetShareSize(size_t nbits, CCDigestAlgorithm alg);
size_t CCCKGGetOpeningSize(size_t nbits, CCDigestAlgorithm alg);
CCStatus CCCKGContributorCreate(size_t nbits, CCDigestAlgorithm alg, CCCKGContributorRef *contrib);
void CCCKGContributorDestroy(CCCKGContributorRef contrib);
CCStatus CCCKGOwnerCreate(size_t nbits, CCDigestAlgorithm alg, CCCKGOwnerRef *owner);
void CCCKGOwnerDestroy(CCCKGOwnerRef owner);
CCStatus CCCKGContributorCommit(CCCKGContributorRef contrib, void *commitment, size_t commitmentLen);
CCStatus CCCKGOwnerGenerateShare(CCCKGOwnerRef owner, const void *commitment, size_t commitmentLen,
                                 void *share, size_t shareLen);
CCStatus CCCKGContributorFinish(CCCKGContributorRef contrib, const void *share, size_t shareLen,
                                void *opening, size_t openingLen, void *sharedKey, size_t sharedKeyLen,
                                CCECCryptorRef *publicKey);
CCStatus CCCKGOwnerFinish(CCCKGOwnerRef owner, const void *opening, size_t openingLen,
                          void *sharedKey, size_t sharedKeyLen, CCECCryptorRef *publicKey);

CCCKG2Params CCCKG2ParamsP224Sha256Version2(void);
size_t CCCKG2GetCommitmentSize(CCCKG2Params params);
size_t CCCKG2GetShareSize(CCCKG2Params params);
size_t CCCKG2GetOpeningSize(CCCKG2Params params);
CCStatus CCCKG2ContributorCreate(CCCKG2Params params, CCCKGContributorRef *contrib);
void CCCKG2ContributorDestroy(CCCKGContributorRef contrib);
CCStatus CCCKG2OwnerCreate(CCCKG2Params params, CCCKGOwnerRef *owner);
void CCCKG2OwnerDestroy(CCCKGOwnerRef owner);
CCStatus CCCKG2ContributorCommit(CCCKGContributorRef contrib, void *commitment, size_t commitmentLen);
CCStatus CCCKG2OwnerGenerateShare(CCCKGOwnerRef owner, const void *commitment, size_t commitmentLen,
                                  void *share, size_t shareLen);
CCStatus CCCKG2ContributorFinish(CCCKGContributorRef contrib, const void *share, size_t shareLen,
                                 void *opening, size_t openingLen, void *sharedKey, size_t sharedKeyLen,
                                 CCECCryptorRef *publicKey);
CCStatus CCCKG2OwnerFinish(CCCKGOwnerRef owner, const void *opening, size_t openingLen,
                           void *sharedKey, size_t sharedKeyLen, CCECCryptorRef *publicKey);

#ifdef __cplusplus
}
#endif

#endif
