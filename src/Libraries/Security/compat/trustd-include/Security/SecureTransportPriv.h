// SecureTransportPriv.h - trustd's certificate transparency code imports this for the SecureTransport types only
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "../../../apple/OSX/libsecurity_ssl/lib/SecureTransport.h"

// the TLS hash and signature algorithm code points (RFC 5246, 7.4.1.4.1)
typedef enum {
    SSL_HashAlgorithmNone = 0,
    SSL_HashAlgorithmMD5 = 1,
    SSL_HashAlgorithmSHA1 = 2,
    SSL_HashAlgorithmSHA224 = 3,
    SSL_HashAlgorithmSHA256 = 4,
    SSL_HashAlgorithmSHA384 = 5,
    SSL_HashAlgorithmSHA512 = 6,
} SSL_HashAlgorithm;

typedef enum {
    SSL_SignatureAlgorithmAnonymous = 0,
    SSL_SignatureAlgorithmRSA = 1,
    SSL_SignatureAlgorithmDSA = 2,
    SSL_SignatureAlgorithmECDSA = 3,
} SSL_SignatureAlgorithm;
