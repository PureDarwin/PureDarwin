// CommonCollabKeyGen.c - collaborative key generation entry points
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// stubs that return kCCUnimplemented until corecrypto grows EC support

#include <CommonCrypto/CommonCollabKeyGen.h>

size_t CCCKGGetCommitmentSize(size_t nbits, CCDigestAlgorithm alg) { (void)nbits; (void)alg; return 0; }
size_t CCCKGGetShareSize(size_t nbits, CCDigestAlgorithm alg) { (void)nbits; (void)alg; return 0; }
size_t CCCKGGetOpeningSize(size_t nbits, CCDigestAlgorithm alg) { (void)nbits; (void)alg; return 0; }

CCStatus
CCCKGContributorCreate(size_t nbits, CCDigestAlgorithm alg, CCCKGContributorRef *contrib)
{
    (void)nbits;
    (void)alg;
    if (contrib) *contrib = NULL;
    return kCCUnimplemented;
}

CCStatus
CCCKGOwnerCreate(size_t nbits, CCDigestAlgorithm alg, CCCKGOwnerRef *owner)
{
    (void)nbits;
    (void)alg;
    if (owner) *owner = NULL;
    return kCCUnimplemented;
}

void CCCKGContributorDestroy(CCCKGContributorRef contrib) { (void)contrib; }
void CCCKGOwnerDestroy(CCCKGOwnerRef owner) { (void)owner; }

CCStatus
CCCKGContributorCommit(CCCKGContributorRef contrib, void *commitment, size_t commitmentLen)
{
    (void)contrib; (void)commitment; (void)commitmentLen;
    return kCCUnimplemented;
}

CCStatus
CCCKGOwnerGenerateShare(CCCKGOwnerRef owner, const void *commitment, size_t commitmentLen, void *share, size_t shareLen)
{
    (void)owner; (void)commitment; (void)commitmentLen; (void)share; (void)shareLen;
    return kCCUnimplemented;
}

CCStatus
CCCKGContributorFinish(CCCKGContributorRef contrib, const void *share, size_t shareLen, void *opening, size_t openingLen,
                       void *sharedKey, size_t sharedKeyLen, CCECCryptorRef *publicKey)
{
    (void)contrib; (void)share; (void)shareLen; (void)opening; (void)openingLen; (void)sharedKey; (void)sharedKeyLen;
    if (publicKey) *publicKey = NULL;
    return kCCUnimplemented;
}

CCStatus
CCCKGOwnerFinish(CCCKGOwnerRef owner, const void *opening, size_t openingLen, void *sharedKey, size_t sharedKeyLen,
                 CCECCryptorRef *publicKey)
{
    (void)owner; (void)opening; (void)openingLen; (void)sharedKey; (void)sharedKeyLen;
    if (publicKey) *publicKey = NULL;
    return kCCUnimplemented;
}

CCCKG2Params CCCKG2ParamsP224Sha256Version2(void) { return NULL; }
size_t CCCKG2GetCommitmentSize(CCCKG2Params params) { (void)params; return 0; }
size_t CCCKG2GetShareSize(CCCKG2Params params) { (void)params; return 0; }
size_t CCCKG2GetOpeningSize(CCCKG2Params params) { (void)params; return 0; }

CCStatus
CCCKG2ContributorCreate(CCCKG2Params params, CCCKGContributorRef *contrib)
{
    (void)params;
    if (contrib) *contrib = NULL;
    return kCCUnimplemented;
}

CCStatus
CCCKG2OwnerCreate(CCCKG2Params params, CCCKGOwnerRef *owner)
{
    (void)params;
    if (owner) *owner = NULL;
    return kCCUnimplemented;
}

void CCCKG2ContributorDestroy(CCCKGContributorRef contrib) { (void)contrib; }
void CCCKG2OwnerDestroy(CCCKGOwnerRef owner) { (void)owner; }

CCStatus
CCCKG2ContributorCommit(CCCKGContributorRef contrib, void *commitment, size_t commitmentLen)
{
    return CCCKGContributorCommit(contrib, commitment, commitmentLen);
}

CCStatus
CCCKG2OwnerGenerateShare(CCCKGOwnerRef owner, const void *commitment, size_t commitmentLen, void *share, size_t shareLen)
{
    return CCCKGOwnerGenerateShare(owner, commitment, commitmentLen, share, shareLen);
}

CCStatus
CCCKG2ContributorFinish(CCCKGContributorRef contrib, const void *share, size_t shareLen, void *opening, size_t openingLen,
                        void *sharedKey, size_t sharedKeyLen, CCECCryptorRef *publicKey)
{
    return CCCKGContributorFinish(contrib, share, shareLen, opening, openingLen, sharedKey, sharedKeyLen, publicKey);
}

CCStatus
CCCKG2OwnerFinish(CCCKGOwnerRef owner, const void *opening, size_t openingLen, void *sharedKey, size_t sharedKeyLen,
                  CCECCryptorRef *publicKey)
{
    return CCCKGOwnerFinish(owner, opening, openingLen, sharedKey, sharedKeyLen, publicKey);
}
