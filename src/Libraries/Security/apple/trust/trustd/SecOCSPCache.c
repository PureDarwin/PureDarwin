/*
 * Copyright (c) 2009-2010,2012-2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  SecOCSPCache.c - securityd
 */

#include <CoreFoundation/CFUtilities.h>
#include <CoreFoundation/CFString.h>
#include "trust/trustd/SecOCSPCache.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/trustdFileLocations.h"
#include <utilities/debugging.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecInternal.h>
#include <AssertMacros.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <asl.h>
#include "utilities/SecCFWrappers.h"
#include "utilities/SecDb.h"
#include "utilities/iOSforOSX.h"
#include <os/lock.h>

/* Note that lastUsed is actually time of insert because we don't
   refresh lastUsed on each SELECT. */

#define deleteAllSQL CFSTR("DELETE FROM responses") /* for testing purposes */
#define flushSQL  CFSTR("DELETE FROM ocsp WHERE certStatus!=1; " \
    "DELETE FROM responses WHERE responseId NOT IN (SELECT responseId FROM ocsp WHERE certStatus=1)") /* Delete respones that aren't CS_Revoked */
#define expireSQL  CFSTR("DELETE FROM responses WHERE expires<? AND responseId NOT IN (SELECT responseId FROM ocsp WHERE certStatus=1)") /* Don't expire revoked responses */
#define insertResponseSQL  CFSTR("INSERT INTO responses " \
    "(ocspResponse,responderURI,expires,lastUsed) VALUES (?,?,?,?)")
#define insertLinkSQL  CFSTR("INSERT INTO ocsp (hashAlgorithm," \
    "issuerNameHash,issuerPubKeyHash,serialNum,responseId,certStatus) VALUES (?,?,?,?,?,?)")
#define deleteResponseSQL  CFSTR("DELETE FROM responses WHERE responseId=?")
#define selectHashAlgorithmSQL  CFSTR("SELECT DISTINCT hashAlgorithm " \
    "FROM ocsp WHERE serialNum=?")
#define selectResponseSQL  CFSTR("SELECT ocspResponse,responseId FROM " \
    "responses WHERE lastUsed>? AND responseId=(SELECT responseId FROM ocsp WHERE " \
    "issuerNameHash=? AND issuerPubKeyHash=? AND serialNum=? AND hashAlgorithm=?)" \
    " ORDER BY expires DESC")
#define hasCertStatusSQL CFSTR("SELECT issuerNameHash FROM ocsp WHERE certStatus=0 LIMIT 1")
#define alterOCSPTableSQL CFSTR("ALTER TABLE ocsp ADD COLUMN certStatus INTEGER NOT NULL DEFAULT 255") /* CS_NotParsed */

#define kSecOCSPCacheFileName CFSTR("ocspcache.sqlite3")


// MARK; -
// MARK: SecOCSPCacheDb

static bool SecOCSPCacheDbUpdateTables(SecDbRef db) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformWrite(db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            CFErrorRef readColumnError = NULL;
            if (!SecDbWithSQL(dbconn, hasCertStatusSQL, &readColumnError, NULL) && CFErrorGetCode(readColumnError) == SQLITE_ERROR) {
                ok &= SecDbWithSQL(dbconn, alterOCSPTableSQL, &localError, ^bool(sqlite3_stmt *stmt) {
                    ok = SecDbStep(dbconn, stmt, &localError, NULL);
                    return ok;
                });
                *commit = ok;
            }
            CFReleaseSafe(readColumnError);
        });
    });

    if (!ok) {
        secerror("OCSP table update failed: %@", localError);
        CFIndex errCode = errSecInternalComponent;
        if (localError) {
            errCode = CFErrorGetCode(localError);
        }
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache,
                                                     TAOperationWrite,
                                                     TAFatalError, errCode);
    }
    CFReleaseSafe(localError);
    return ok;
}

static SecDbRef SecOCSPCacheDbCreate(CFStringRef path) {
    return SecDbCreate(path, 0600, true, true, true, true, 1,
            ^bool (SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error) {
        __block bool ok = true;

        CFErrorRef localError = NULL;
        if (!SecDbWithSQL(dbconn, selectHashAlgorithmSQL /* expireSQL */, &localError, NULL) && CFErrorGetCode(localError) == SQLITE_ERROR) {
            /* SecDbWithSQL returns SQLITE_ERROR if the table we are preparing the above statement for doesn't exist. */
            ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
                ok &= SecDbExec(dbconn,
                    CFSTR("CREATE TABLE ocsp("
                          "issuerNameHash BLOB NOT NULL,"
                          "issuerPubKeyHash BLOB NOT NULL,"
                          "serialNum BLOB NOT NULL,"
                          "hashAlgorithm BLOB NOT NULL,"
                          "responseId INTEGER NOT NULL,"
                          "certStatus INTEGER NOT NULL DEFAULT 255" // CS_NotParsed
                          ");"
                          "CREATE INDEX iResponseId ON ocsp(responseId);"
                          "CREATE INDEX iserialNum ON ocsp(serialNum);"
                          "CREATE INDEX iSNumDAlg ON ocsp(serialNum,hashAlgorithm);"
                          "CREATE TABLE responses("
                          "responseId INTEGER PRIMARY KEY,"
                          "ocspResponse BLOB NOT NULL,"
                          "responderURI BLOB,"
                          "expires DOUBLE NOT NULL,"
                          "lastUsed DOUBLE NOT NULL"
                          ");"
                          "CREATE INDEX iexpires ON responses(expires);"
                          "CREATE TRIGGER tocspdel BEFORE DELETE ON responses FOR EACH ROW "
                          "BEGIN "
                          "DELETE FROM ocsp WHERE responseId=OLD.responseId;"
                          " END;"), error);
                *commit = ok;
            });
        }
        CFReleaseSafe(localError);
        if (!ok) {
            secerror("%s failed: %@", didCreate ? "Create" : "Open", error ? *error : NULL);
            CFIndex errCode = errSecInternalComponent;
            if (error && *error) {
                errCode = CFErrorGetCode(*error);
            }
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache,
                                                         didCreate ? TAOperationCreate : TAOperationOpen,
                                                         TAFatalError, errCode);
        }
        return ok;
    });
}

// MARK; -
// MARK: SecOCSPCache

typedef struct __SecOCSPCache *SecOCSPCacheRef;
struct __SecOCSPCache {
	SecDbRef db;
};

static SecOCSPCacheRef SecOCSPCacheCreate(CFStringRef db_name) {
	SecOCSPCacheRef this;

	require(this = (SecOCSPCacheRef)malloc(sizeof(struct __SecOCSPCache)), errOut);
    require(this->db = SecOCSPCacheDbCreate(db_name), errOut);
    require(SecOCSPCacheDbUpdateTables(this->db), errOut);

	return this;

errOut:
	if (this) {
        CFReleaseSafe(this->db);
		free(this);
	}

	return NULL;
}

CFStringRef SecOCSPCacheCopyPath(void) {
    CFStringRef ocspRelPath = kSecOCSPCacheFileName;
    CFURLRef ocspURL = SecCopyURLForFileInPrivateUserTrustdDirectory(ocspRelPath);
    CFStringRef ocspPath = NULL;
    if (ocspURL) {
        ocspPath = CFURLCopyFileSystemPath(ocspURL, kCFURLPOSIXPathStyle);
        CFRelease(ocspURL);
    }
    return ocspPath;
}

static SecOCSPCacheRef kSecOCSPCache = NULL;
static os_unfair_lock cacheLock = OS_UNFAIR_LOCK_INIT;

void SecOCSPCacheCloseDB(void) {
    os_unfair_lock_lock(&cacheLock);
    if (kSecOCSPCache) {
        // Release the DB
        SecDbReleaseAllConnections(kSecOCSPCache->db);
        CFReleaseSafe(kSecOCSPCache->db);

        // free the cache struct
        free(kSecOCSPCache);
        kSecOCSPCache = NULL;
    }
    os_unfair_lock_unlock(&cacheLock);
}

void SecOCSPCacheDeleteCache(void) {
    os_unfair_lock_lock(&cacheLock);
    if (kSecOCSPCache) {
        // Release the DB
        SecDbReleaseAllConnections(kSecOCSPCache->db);
        CFReleaseSafe(kSecOCSPCache->db);

        // free the cache struct
        free(kSecOCSPCache);
        kSecOCSPCache = NULL;
    }

    // remove the file
    CFStringRef path = SecOCSPCacheCopyPath();
    CFStringPerformWithCStringAndLength(path, ^(const char *utf8Str, size_t utf8Length) {
        remove(utf8Str);
    });
    CFReleaseSafe(path);
    os_unfair_lock_unlock(&cacheLock);
}

static void SecOCSPCacheWith(void(^cacheJob)(SecOCSPCacheRef cache)) {
    os_unfair_lock_lock(&cacheLock);
    if (!kSecOCSPCache) {
        CFStringRef dbPath = SecOCSPCacheCopyPath();
        if (dbPath) {
            kSecOCSPCache = SecOCSPCacheCreate(dbPath);
            CFRelease(dbPath);
        }
    }
    if (kSecOCSPCache) {
        cacheJob(kSecOCSPCache);
    }
    os_unfair_lock_unlock(&cacheLock);
}

static bool _SecOCSPCacheExpireWithTransaction(SecDbConnectionRef dbconn, CFAbsoluteTime now, CFErrorRef *error) {
    //if (now > nextExpireTime)
    {
        return SecDbWithSQL(dbconn, expireSQL, error, ^bool(sqlite3_stmt *expire) {
            return SecDbBindDouble(expire, 1, now, error) &&
            SecDbStep(dbconn, expire, error, NULL);
        });
        // TODO: Write now + expireDelay to nextExpireTime;
        // currently we try to expire entries on each cache write
    }
}

/* Instance implementation. */

static void _SecOCSPCacheReplaceResponse(SecOCSPCacheRef this,
    SecOCSPResponseRef oldResponse, SecOCSPResponseRef ocspResponse,
    CFURLRef localResponderURI, CFAbsoluteTime verifyTime) {
    secdebug("ocspcache", "adding response from %@", localResponderURI);
    /* responses.ocspResponse */

    // TODO: Update a latestProducedAt value using date in new entry, to ensure forward movement of time.
    // Set "now" to the new producedAt we are receiving here if localTime is before this date.
    // In addition whenever we run though here, check to see if "now" is more than past
    // the nextCacheExpireDate and expire the cache if it is.
    CFDataRef responseData = SecOCSPResponseGetData(ocspResponse);
    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            __block sqlite3_int64 responseId;
            if (oldResponse && (responseId = SecOCSPResponseGetID(oldResponse)) >= 0) {
                ok &= SecDbWithSQL(dbconn, deleteResponseSQL, &localError, ^bool(sqlite3_stmt *deleteResponse) {
                    ok &= SecDbBindInt64(deleteResponse, 1, responseId, &localError);
                    /* Execute the delete statement. */
                    ok &= SecDbStep(dbconn, deleteResponse, &localError, NULL);
                    return ok;
                });
            }

            ok &= SecDbWithSQL(dbconn, insertResponseSQL, &localError, ^bool(sqlite3_stmt *insertResponse) {
                ok &= SecDbBindBlob(insertResponse, 1,
                                    CFDataGetBytePtr(responseData),
                                    CFDataGetLength(responseData),
                                    SQLITE_TRANSIENT, &localError);

                /* responses.responderURI */
                if (ok) {
                    CFDataRef uriData = NULL;
                    if (localResponderURI) {
                        uriData = CFURLCreateData(kCFAllocatorDefault, localResponderURI,
                                                  kCFStringEncodingUTF8, false);
                    }
                    if (uriData) {
                        ok = SecDbBindBlob(insertResponse, 2,
                                           CFDataGetBytePtr(uriData),
                                           CFDataGetLength(uriData),
                                           SQLITE_TRANSIENT, &localError);
                        CFRelease(uriData);
                    }
                }
                /* responses.expires */
                ok &= SecDbBindDouble(insertResponse, 3,
                                      SecOCSPResponseGetExpirationTime(ocspResponse),
                                      &localError);
                /* responses.lastUsed */
                ok &= SecDbBindDouble(insertResponse, 4,
                                      verifyTime,
                                      &localError);

                /* Execute the insert statement. */
                ok &= SecDbStep(dbconn, insertResponse, &localError, NULL);

                responseId = sqlite3_last_insert_rowid(SecDbHandle(dbconn));
                return ok;
            });

            /* Now add a link record for every singleResponse in the ocspResponse. */
            ok &= SecDbWithSQL(dbconn, insertLinkSQL, &localError, ^bool(sqlite3_stmt *insertLink) {
                SecAsn1OCSPSingleResponse **responses;
                for (responses = ocspResponse->responseData.responses;
                     *responses; ++responses) {
                    SecAsn1OCSPSingleResponse *resp = *responses;
                    SecAsn1OCSPCertID *certId = &resp->certID;
                    SecAsn1OCSPCertStatusTag certStatus = (SecAsn1OCSPCertStatusTag)(resp->certStatus.Data[0] & SEC_ASN1_TAGNUM_MASK);
                    ok &= SecDbBindBlob(insertLink, 1,
                                        certId->algId.algorithm.Data,
                                        certId->algId.algorithm.Length,
                                        SQLITE_TRANSIENT, &localError);
                    ok &= SecDbBindBlob(insertLink, 2,
                                        certId->issuerNameHash.Data,
                                        certId->issuerNameHash.Length,
                                        SQLITE_TRANSIENT, &localError);
                    ok &= SecDbBindBlob(insertLink, 3,
                                        certId->issuerPubKeyHash.Data,
                                        certId->issuerPubKeyHash.Length,
                                        SQLITE_TRANSIENT, &localError);
                    ok &= SecDbBindBlob(insertLink, 4,
                                        certId->serialNumber.Data,
                                        certId->serialNumber.Length,
                                        SQLITE_TRANSIENT, &localError);
                    ok &= SecDbBindInt64(insertLink, 5, responseId, &localError);
                    ok &= SecDbBindInt(insertLink, 6, certStatus, &localError);

                    /* Execute the insert statement. */
                    ok &= SecDbStep(dbconn, insertLink, &localError, NULL);
                    ok &= SecDbReset(insertLink, &localError);
                }
                return ok;
            });

            // Remove expired entries here.
            // TODO: Consider only doing this once per 24 hours or something.
            ok &= _SecOCSPCacheExpireWithTransaction(dbconn, verifyTime, &localError);
            if (!ok)
                *commit = false;
        });
    });
    if (!ok) {
        secerror("_SecOCSPCacheAddResponse failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
        CFReleaseNull(localError);
    }
    CFReleaseSafe(localError);
}

static SecOCSPResponseRef _SecOCSPCacheCopyMatching(SecOCSPCacheRef this,
    SecOCSPRequestRef request, CFURLRef responderURI, CFAbsoluteTime minInsertTime) {
    const DERItem *publicKey;
    CFDataRef issuer = NULL;
    CFDataRef serial = NULL;
    __block SecOCSPResponseRef response = NULL;
    __block CFErrorRef localError = NULL;
    __block bool ok = true;

    require(publicKey = SecCertificateGetPublicKeyData(request->issuer), errOut);
    require(issuer = SecCertificateCopyIssuerSequence(request->certificate), errOut);
    require(serial = SecCertificateCopySerialNumberData(request->certificate, NULL), errOut);

    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectHashAlgorithmSQL, &localError, ^bool(sqlite3_stmt *selectHash) {
            ok = SecDbBindBlob(selectHash, 1, CFDataGetBytePtr(serial), CFDataGetLength(serial), SQLITE_TRANSIENT, &localError);
            ok &= SecDbStep(dbconn, selectHash, &localError, ^(bool *stopHash) {
                SecAsn1Oid algorithm;
                algorithm.Data = (uint8_t *)sqlite3_column_blob(selectHash, 0);
                algorithm.Length = sqlite3_column_bytes(selectHash, 0);

                /* Calculate the issuerKey and issuerName digests using the returned
                 hashAlgorithm. */
                CFDataRef issuerNameHash = SecDigestCreate(kCFAllocatorDefault,
                                                           &algorithm, NULL, CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
                CFDataRef issuerPubKeyHash = SecDigestCreate(kCFAllocatorDefault,
                                                             &algorithm, NULL, publicKey->data, publicKey->length);

                if (issuerNameHash && issuerPubKeyHash && ok) {
                    ok &= SecDbWithSQL(dbconn, selectResponseSQL, &localError, ^bool(sqlite3_stmt *selectResponse) {
                        /* Now we have the serial, algorithm, issuerNameHash and
                         issuerPubKeyHash so let's lookup the db entry. */
                        ok &= SecDbBindDouble(selectResponse, 1, minInsertTime, &localError);
                        ok &= SecDbBindBlob(selectResponse, 2, CFDataGetBytePtr(issuerNameHash),
                                            CFDataGetLength(issuerNameHash), SQLITE_TRANSIENT, &localError);
                        ok &= SecDbBindBlob(selectResponse, 3, CFDataGetBytePtr(issuerPubKeyHash),
                                            CFDataGetLength(issuerPubKeyHash), SQLITE_TRANSIENT, &localError);
                        ok &= SecDbBindBlob(selectResponse, 4, CFDataGetBytePtr(serial),
                                            CFDataGetLength(serial), SQLITE_TRANSIENT, &localError);
                        ok &= SecDbBindBlob(selectResponse, 5, algorithm.Data,
                                            algorithm.Length, SQLITE_TRANSIENT, &localError);
                        ok &= SecDbStep(dbconn, selectResponse, &localError, ^(bool *stopResponse) {
                            /* Found an entry! */
                            secdebug("ocspcache", "found cached response");
                            CFDataRef resp = CFDataCreate(kCFAllocatorDefault,
                                                          sqlite3_column_blob(selectResponse, 0),
                                                          sqlite3_column_bytes(selectResponse, 0));
                            sqlite3_int64 responseID = sqlite3_column_int64(selectResponse, 1);
                            if (resp) {
                                response = SecOCSPResponseCreateWithID(resp, responseID);
                                CFRelease(resp);
                            }
                        });
                        return ok;
                    });
                }

                CFReleaseSafe(issuerNameHash);
                CFReleaseSafe(issuerPubKeyHash);
            });
            return ok;
        });
    });

errOut:
    CFReleaseSafe(serial);
    CFReleaseSafe(issuer);

    if (!ok || localError) {
        secerror("ocsp cache lookup failed: %@", localError);
        if (response) {
            SecOCSPResponseFinalize(response);
            response = NULL;
        }
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    CFReleaseSafe(localError);

    secdebug("ocspcache", "returning %s", (response ? "cached response" : "NULL"));

    return response;
}

static bool _SecOCSPCacheFlush(SecOCSPCacheRef cache, CFErrorRef *error) {
    __block CFErrorRef localError = NULL;
    __block bool ok = true;

    ok &= SecDbPerformWrite(cache->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbExec(dbconn, flushSQL, &localError);
    });
    if (!ok || localError) {
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);

    return ok;
}

static bool _SecOCSPCacheDeleteContent(SecOCSPCacheRef cache, CFErrorRef *error) {
    __block CFErrorRef localError = NULL;
    __block bool ok = true;

    ok &= SecDbPerformWrite(cache->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbExec(dbconn, deleteAllSQL, &localError);
    });
    if (!ok || localError) {
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TAOCSPCache, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);

    return ok;
}


/* Public API */

void SecOCSPCacheReplaceResponse(SecOCSPResponseRef old_response, SecOCSPResponseRef response,
    CFURLRef localResponderURI, CFAbsoluteTime verifyTime) {
    SecOCSPCacheWith(^(SecOCSPCacheRef cache) {
        _SecOCSPCacheReplaceResponse(cache, old_response, response, localResponderURI, verifyTime);
    });
}

SecOCSPResponseRef SecOCSPCacheCopyMatching(SecOCSPRequestRef request,
    CFURLRef localResponderURI /* may be NULL */) {
    __block SecOCSPResponseRef response = NULL;
    SecOCSPCacheWith(^(SecOCSPCacheRef cache) {
        response = _SecOCSPCacheCopyMatching(cache, request, localResponderURI, 0.0);
    });
    return response;
}

SecOCSPResponseRef SecOCSPCacheCopyMatchingWithMinInsertTime(SecOCSPRequestRef request,
    CFURLRef localResponderURI, CFAbsoluteTime minInsertTime) {
    __block SecOCSPResponseRef response = NULL;
    SecOCSPCacheWith(^(SecOCSPCacheRef cache) {
        response = _SecOCSPCacheCopyMatching(cache, request, localResponderURI, minInsertTime);
    });
    return response;
}

bool SecOCSPCacheFlush(CFErrorRef *error) {
    __block bool result = false;
    SecOCSPCacheWith(^(SecOCSPCacheRef cache) {
        result = _SecOCSPCacheFlush(cache, error);
    });
    return result;
}

bool SecOCSPCacheDeleteContent(CFErrorRef *error) {
    __block bool result = false;
    SecOCSPCacheWith(^(SecOCSPCacheRef cache) {
        result = _SecOCSPCacheDeleteContent(cache, error);
    });
    return result;
}
