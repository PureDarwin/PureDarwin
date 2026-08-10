/*
 * Copyright (c) 2012-2017 Apple Inc. All Rights Reserved.
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


#include "SecDb.h"
#include "SecDbInternal.h"
#include "debugging.h"

#include <sqlite3.h>
#include <sqlite3_private.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <sys/csr.h>
#include <sys/stat.h>
#include <AssertMacros.h>
#include "SecCFWrappers.h"
#include "SecCFError.h"
#include "SecIOFormat.h"
#include <stdio.h>
#include "Security/SecBase.h"
#include "SecAutorelease.h"
#include <os/assumes.h>
#include <xpc/private.h>    // xpc_transaction_exit_clean()


//
// Architecturally inverted files
// These are in SecureObjectSync but utilities depends on them
// <rdar://problem/20802079> Fix layer violation (SOSDigestVector, SOSManifest, SecDB.c)
//
#include "keychain/SecureObjectSync/SOSDigestVector.h"
#include "keychain/SecureObjectSync/SOSManifest.h"

#define SECDB_DEBUGGING 0

struct __OpaqueSecDbStatement {
    CFRuntimeBase _base;

    SecDbConnectionRef dbconn;
    sqlite3_stmt *stmt;
};

struct __OpaqueSecDbConnection {
    CFRuntimeBase _base;

    //CFMutableDictionaryRef statements;

    SecDbRef db;     // NONRETAINED, since db or block retains us
    bool readOnly;
    bool inTransaction;
    SecDbTransactionSource source;
    bool isCorrupted;
    int maybeCorruptedCode;
    bool hasIOFailure;
    CFErrorRef corruptionError;
    sqlite3 *handle;
    // Pending deletions and additions for the current transaction
    // Entires are either:
    // 1) a CFArrayRef of 1 element representing a deletion,
    // 2) a CFArrayRef of 2 elements representing the element 0 having been replaced with element 1
    // 3) a CFTypeRef that is not a CFArrayRef, representing an add of the element in question.
    CFMutableArrayRef changes;
};

struct __OpaqueSecDb {
    CFRuntimeBase _base;

    CFStringRef db_path;
    dispatch_queue_t queue;
    dispatch_queue_t commitQueue;

    CFMutableArrayRef idleWriteConnections;     // up to kSecDbMaxWriters of them (currently 1, requires locking change for >1)
    CFMutableArrayRef idleReadConnections;      // up to kSecDbMaxReaders of them
    pthread_mutex_t writeMutex;
    pthread_mutexattr_t writeMutexAttrs;
    // TODO: Replace after we have rdar://problem/60961964
    dispatch_semaphore_t readSemaphore;

    bool didFirstOpen;
    bool (^opened)(SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error);
    bool callOpenedHandlerForNextConnection;
    CFMutableArrayRef notifyPhase; /* array of SecDBNotifyBlock */
    mode_t mode; /* database file permissions */
    bool readWrite; /* open database read-write */
    bool allowRepair; /* allow database repair */
    bool useWAL; /* use WAL mode */
    bool useRobotVacuum; /* use if SecDB should manage vacuum behind your back */
    uint8_t maxIdleHandles;
    void (^corruptionReset)(void);
};

// MARK: Error domains and error helper functions

CFStringRef kSecDbErrorDomain = CFSTR("com.apple.utilities.sqlite3");

bool SecDbError(int sql_code, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;

    if (error) {
        va_list args;
        CFIndex code = sql_code;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(code, kSecDbErrorDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

bool SecDbErrorWithDb(int sql_code, sqlite3 *db, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;
    if (error) {
        va_list args;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);
        CFStringRef errno_code = NULL;

        if (sql_code == SQLITE_CANTOPEN) {
            int errno_number = sqlite3_system_errno(db);
            errno_code = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), errno_number);
        } else {
            errno_code = CFRetain(CFSTR(""));
        }

        int extended_code = sqlite3_extended_errcode(db);
        if (sql_code == extended_code)
            SecDbError(sql_code, error, CFSTR("%@: [%d]%@ %s"), message, sql_code, errno_code, sqlite3_errmsg(db));
        else
            SecDbError(sql_code, error, CFSTR("%@: [%d->%d]%@ %s"), message, sql_code, extended_code, errno_code, sqlite3_errmsg(db));
        CFReleaseSafe(message);
        CFReleaseSafe(errno_code);
    }
    return false;
}

bool SecDbErrorWithStmt(int sql_code, sqlite3_stmt *stmt, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;
    if (error) {
        va_list args;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);

        sqlite3 *db = sqlite3_db_handle(stmt);
        const char *sql = sqlite3_sql(stmt);
        int extended_code = sqlite3_extended_errcode(db);
        if (sql_code == extended_code)
            SecDbError(sql_code, error, CFSTR("%@: [%d] %s sql: %s"), message, sql_code, sqlite3_errmsg(db), sql);
        else
            SecDbError(sql_code, error, CFSTR("%@: [%d->%d] %s sql: %s"), message, sql_code, extended_code, sqlite3_errmsg(db), sql);
        CFReleaseSafe(message);
    }
    return false;
}

// A callback for the sqlite3_log() interface.
static void sqlite3Log(void *pArg, int iErrCode, const char *zMsg){
    secdebug("sqlite3", "(%d) %s", iErrCode, zMsg);
}

void _SecDbServerSetup(void)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        int rx = sqlite3_config(SQLITE_CONFIG_LOG, sqlite3Log, NULL);
        if (SQLITE_OK != rx) {
            secwarning("Could not set up sqlite global error logging to syslog: %d", rx);
        }
    });
}


// MARK: -
// MARK: Static helper functions

static bool SecDbOpenHandle(SecDbConnectionRef dbconn, bool *created, CFErrorRef *error);
static bool SecDbHandleCorrupt(SecDbConnectionRef dbconn, int rc, CFErrorRef *error);

#pragma mark -
#pragma mark SecDbRef

static CFStringRef
SecDbCopyFormatDescription(CFTypeRef value, CFDictionaryRef formatOptions)
{
    SecDbRef db = (SecDbRef)value;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecDb path:%@ connections: %@>"), db->db_path, db->idleReadConnections);
}


static void
SecDbDestroy(CFTypeRef value)
{
    SecDbRef db = (SecDbRef)value;

    CFReleaseNull(db->db_path);
    dispatch_sync(db->queue, ^{
        CFReleaseNull(db->idleWriteConnections);
        CFReleaseNull(db->idleReadConnections);
    });

    if (db->queue) {
        dispatch_release(db->queue);
        db->queue = NULL;
    }
    if (db->commitQueue) {
        dispatch_release(db->commitQueue);
        db->commitQueue = NULL;
    }

    pthread_mutex_destroy(&(db->writeMutex));

    if (db->readSemaphore) {
        dispatch_release(db->readSemaphore);
        db->readSemaphore = NULL;
    }

    if (db->opened) {
        Block_release(db->opened);
        db->opened = NULL;
    }
    CFReleaseNull(db->notifyPhase);
}

CFGiblisFor(SecDb)

SecDbRef
SecDbCreate(CFStringRef dbName, mode_t mode, bool readWrite, bool allowRepair, bool useWAL, bool useRobotVacuum, uint8_t maxIdleHandles,
                       bool (^opened)(SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error))
{
    SecDbRef db = NULL;

    db = CFTypeAllocate(SecDb, struct __OpaqueSecDb, kCFAllocatorDefault);
    require(db != NULL, done);

    CFStringPerformWithCString(dbName, ^(const char *dbNameStr) {
        db->queue = dispatch_queue_create(dbNameStr, DISPATCH_QUEUE_SERIAL);
    });
    CFStringRef commitQueueStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-commit"), dbName);
    CFStringPerformWithCString(commitQueueStr, ^(const char *cqNameStr) {
        db->commitQueue = dispatch_queue_create(cqNameStr, DISPATCH_QUEUE_CONCURRENT);
    });
    CFReleaseNull(commitQueueStr);
    db->readSemaphore = dispatch_semaphore_create(kSecDbMaxReaders);

    bool mutexAttrSuccess =  (0 == pthread_mutexattr_init(&(db->writeMutexAttrs)));
    if(mutexAttrSuccess) {
        mutexAttrSuccess = (0 == pthread_mutexattr_setpolicy_np(&(db->writeMutexAttrs), PTHREAD_MUTEX_POLICY_FAIRSHARE_NP));
    }

    if(!mutexAttrSuccess) {
        seccritical("SecDb: SecDbCreate failed to create attributes for the write mutex; fairness properties are no longer present");
    }

    if (pthread_mutex_init(&(db->writeMutex), (mutexAttrSuccess ? &(db->writeMutexAttrs) : NULL)) != 0) {
        seccritical("SecDb: SecDbCreate failed to init the write mutex, this will end badly");
    }

    db->idleWriteConnections = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    db->idleReadConnections = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    db->opened = opened ? Block_copy(opened) : NULL;
    if (getenv("__OSINSTALL_ENVIRONMENT") != NULL) {
        // TODO: Move this code out of this layer
        secinfo("#SecDB", "SecDB: running from installer");

        db->db_path = CFSTR("file::memory:?cache=shared");
    } else {
        db->db_path = CFStringCreateCopy(kCFAllocatorDefault, dbName);
    }
    db->mode = mode;
    db->readWrite = readWrite;
    db->allowRepair = allowRepair;
    db->useWAL = useWAL;
    db->useRobotVacuum = useRobotVacuum;
    db->maxIdleHandles = maxIdleHandles;
    db->corruptionReset = NULL;

done:
    return db;
}

CFIndex
SecDbIdleConnectionCount(SecDbRef db) {
    __block CFIndex count = 0;
    dispatch_sync(db->queue, ^{
        count = CFArrayGetCount(db->idleReadConnections);
        count += CFArrayGetCount(db->idleWriteConnections);
    });
    return count;
}

void SecDbAddNotifyPhaseBlock(SecDbRef db, SecDBNotifyBlock notifyPhase)
{
#if !TARGET_OS_BRIDGE
    // SecDbNotifyPhase seems to mostly be called on the db's commitQueue, and not the db's queue. Therefore, protect the array with that queue.
    dispatch_sync(db->commitQueue, ^{
        SecDBNotifyBlock block = Block_copy(notifyPhase); /* Force the block off the stack */
        if (db->notifyPhase == NULL) {
            db->notifyPhase = CFArrayCreateMutableForCFTypes(NULL);
        }
        CFArrayAppendValue(db->notifyPhase, block);
        Block_release(block);
    });
#endif
}

static void SecDbNotifyPhase(SecDbConnectionRef dbconn, SecDbTransactionPhase phase) {
#if !TARGET_OS_BRIDGE
    if (CFArrayGetCount(dbconn->changes)) {
        CFArrayRef changes = dbconn->changes;
        dbconn->changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        if (dbconn->db->notifyPhase) {
            CFArrayForEach(dbconn->db->notifyPhase, ^(const void *value) {
                SecDBNotifyBlock notifyBlock = (SecDBNotifyBlock)value;
                notifyBlock(dbconn, phase, dbconn->source, changes);
            });
        }
        CFReleaseSafe(changes);
    }
#endif
}

static void SecDbOnNotify(SecDbConnectionRef dbconn, void (^perform)(void)) {
    perform();
}

CFStringRef SecDbGetPath(SecDbRef db) {
    if(!db) {
        return NULL;
    }
    return db->db_path;
}


#pragma mark -
#pragma mark SecDbConnectionRef

static bool SecDbCheckCorrupted(SecDbConnectionRef dbconn)
{
    __block bool checkDidRun = false;
    __block bool isCorrupted = false;
    __block CFErrorRef error = NULL;
    SecDbPrepare(dbconn, CFSTR("PRAGMA integrity_check"), &error, ^(sqlite3_stmt *stmt) {
        SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
            const char * result = (const char*)sqlite3_column_text(stmt, 0);
            if (!result || strncasecmp(result, "ok", 3) != 0) {
                isCorrupted = true;
                secerror("SecDBCheckCorrupted integrity_check returned %s", (result) ? result : "NULL");
            }
            checkDidRun = true;
        });
    });
    if (!checkDidRun) {
        // An error occurred in SecDbPrepare before we could run the block.
        if (error) {
            CFIndex code = CFErrorGetCode(error);
            if (SQLITE_CORRUPT == code || SQLITE_NOTADB == code) {
                isCorrupted = true;
            }
            secinfo("#SecDB", "#SecDB warning error %{public}@ when running integrity check", error);
        } else {
            // We don't have an error ref if SecDbPrepare has called SecDbConnectionCheckCode,
            // which then called SecDbHandleCorrupt. That code path is only entered when the
            // original error was SQLITE_CORRUPT or SQLITE_NOTADB. On other errors, the
            // CFErrorRef is not cleared and we can just check the code above.
            isCorrupted = true;
            secinfo("#SecDB", "#SecDB warning: failed to run integrity check due to corruption");
        }
    }
    if (isCorrupted) {
        if (checkDidRun) {
            secerror("SecDBCheckCorrupted ran integrity_check, and that didn't return ok");
        } else {
            secerror("SecDBCheckCorrupted failed to run integrity check");
        }
    }
    CFReleaseNull(error);

    return isCorrupted;
}

static bool SecDbDidCreateFirstConnection(SecDbConnectionRef dbconn, bool didCreate, CFErrorRef *error)
{
    secinfo("#SecDB", "#SecDB starting maintenance");
    bool ok = true;

    // Historical note: this used to check for integrity but that became too slow and caused panics at boot.
    // Now, just react to SQLite errors when doing an operation. If file on disk is borked it'll tell us right away.

    if (!dbconn->isCorrupted && dbconn->db->opened) {
        CFErrorRef localError = NULL;

        dbconn->db->callOpenedHandlerForNextConnection = false;
        ok = dbconn->db->opened(dbconn->db, dbconn, didCreate, &dbconn->db->callOpenedHandlerForNextConnection, &localError);

        if (!ok)
            secerror("opened block failed: %@", localError);

        if (!dbconn->isCorrupted && error && *error == NULL) {
            *error = localError;
            localError = NULL;
        } else {
            if (localError)
                secerror("opened block failed: error (%@) is being released and lost", localError);
            CFReleaseNull(localError);
        }
    }

    if (dbconn->isCorrupted) {
        ok = SecDbHandleCorrupt(dbconn, 0, error);
    }

    secinfo("#SecDB", "#SecDB starting maintenance");
    return ok;
}

void SecDbCorrupt(SecDbConnectionRef dbconn, CFErrorRef error)
{
    if (__security_simulatecrash_enabled()) {
        os_log_fault(secLogObjForScope("SecEmergency"), "SecDBCorrupt: %@", error);
    }
    dbconn->isCorrupted = true;
    CFRetainAssign(dbconn->corruptionError, error);
}


static uint8_t knownDbPathIndex(SecDbConnectionRef dbconn)
{

    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/keychain-2.db")))
        return 1;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/ocspcache.sqlite3")))
        return 2;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/TrustStore.sqlite3")))
        return 3;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/caissuercache.sqlite3")))
        return 4;

    /* Unknown DB path */
    return 0;
}

static bool SecDbConnectionCheckCode(SecDbConnectionRef dbconn, int code, CFErrorRef *error, CFStringRef desc, ...)
    CF_FORMAT_FUNCTION(4, 5);

// Return true if there was no error, returns false otherwise and set *error to an appropriate CFErrorRef.
static bool SecDbConnectionCheckCode(SecDbConnectionRef dbconn, int code, CFErrorRef *error, CFStringRef desc, ...) {
    if (code == SQLITE_OK || code == SQLITE_DONE)
        return true;

    if (error) {
        va_list args;
        va_start(args, desc);
        CFStringRef msg = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, desc, args);
        va_end(args);
        SecDbErrorWithDb(code, dbconn->handle, error, CFSTR("%@"), msg);
        CFRelease(msg);
    }

    dbconn->hasIOFailure |= (SQLITE_IOERR == code);

    /* If it's already corrupted, don't try to recover */
    if (dbconn->isCorrupted) {
        CFStringRef reason = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                      CFSTR("SQL DB %@ is corrupted already. Corruption error was: %d (previously %d)"),
                                                      dbconn->db->db_path, code, dbconn->maybeCorruptedCode);
        secerror("%@",reason);
        __security_simulatecrash(reason, __sec_exception_code_TwiceCorruptDb(knownDbPathIndex(dbconn)));
        CFReleaseSafe(reason);
        // We can't fall through to the checking case because it eventually calls SecDbConnectionCheckCode again.
        // However, this is the second time we're seeing corruption so let's take the ultimate measure.
        if ((SQLITE_CORRUPT == code) || (SQLITE_NOTADB == code)) {
            secerror("SecDbConnectionCheckCode detected corruption twice: going to handle corrupt DB");
            (void)SecDbHandleCorrupt(dbconn, code, error);
        }
        return false;
    }

    // NOTADB means file is garbage, so it's functionally equivalent to corruption
    dbconn->isCorrupted = (SQLITE_CORRUPT == code) || (SQLITE_NOTADB == code);
    if (dbconn->isCorrupted) {
        /* Run integrity check and only make dbconn->isCorrupted true and
           run the corruption handler if the integrity check conclusively fails. */
        dbconn->maybeCorruptedCode = code;
        dbconn->isCorrupted = SecDbCheckCorrupted(dbconn);
        if (dbconn->isCorrupted) {
            secerror("operation returned code: %d integrity check=fail", code);
            (void)SecDbHandleCorrupt(dbconn, code, error);
        } else {
            secerror("operation returned code: %d: integrity check=pass", code);
        }
    }

    return false;
}

#define BUSY_TIMEOUT_MS (5 * 60 * 1000)  /* 5 minutes */

static int sleepBackoff[] = { 10, 20, 50, 100, 250 };
static int sumBackoff[]   = { 10, 30, 80, 180, 430 };
static int NumberOfSleepBackoff = sizeof(sleepBackoff)/sizeof(sleepBackoff[0]);

// Use these as silly hacks to encode the SQLite return code in the backtrace, for hang debugging purposes
static void __attribute__((noinline)) SecDbLockSleep(int ms) {
    sqlite3_sleep(ms);
}

static void __attribute__((noinline)) SecDbBusySleep(int ms) {
    sqlite3_sleep(ms);
}

// Return true causes the operation to be tried again.
// Note that we set sqlite3_busy_timeout on the connection, so anytime you're in here, it's likely due to SQLITE_LOCKED.
static bool SecDbWaitIfNeeded(SecDbConnectionRef dbconn, int s3e, sqlite3_stmt *stmt, CFStringRef desc, int nTries, CFErrorRef *error) {
    if (((0xFF & s3e) == SQLITE_BUSY) || ((0xFF & s3e) == SQLITE_LOCKED)) {
        int totaltimeout, timeout;

        _Static_assert(sizeof(sumBackoff) == sizeof(sleepBackoff), "matching arrays not matching");
        _Static_assert(sizeof(sumBackoff[0]) == sizeof(sleepBackoff[0]), "matching arrays not matching");

        if (nTries < NumberOfSleepBackoff) {
            timeout = sleepBackoff[nTries];
            totaltimeout = sumBackoff[nTries];
        } else {
            timeout = sleepBackoff[NumberOfSleepBackoff - 1];
            totaltimeout = sumBackoff[NumberOfSleepBackoff - 1] + (timeout * (nTries - NumberOfSleepBackoff));
        }
        if (totaltimeout < BUSY_TIMEOUT_MS) {
            secinfo("#SecDB", "sqlite busy/locked: %d ntries: %d totaltimeout: %d", s3e, nTries, totaltimeout);
            if(((0xFF & s3e) == SQLITE_LOCKED)) {
                SecDbLockSleep(timeout);
            } else {
                SecDbBusySleep(timeout);
            }
            return true;
        } else {
            secinfo("#SecDB", "sqlite busy/locked: too long: %d ms, giving up", totaltimeout);
        }
    }

    return SecDbConnectionCheckCode(dbconn, s3e, error, CFSTR("%@"), desc);
}

enum SecDbStepResult {
    kSecDbErrorStep = 0,
    kSecDbRowStep = 1,
    kSecDbDoneStep = 2,
};
typedef enum SecDbStepResult SecDbStepResult;

static SecDbStepResult _SecDbStep(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error) {
    assert(stmt != NULL);
    int s3e;
    int ntries = 0;
    for (;;) {
        if (SecDbConnectionIsReadOnly(dbconn) && !sqlite3_stmt_readonly(stmt)) {
            secerror("_SecDbStep: SecDbConnection is readonly but we're about to write: %s", sqlite3_sql(stmt));
        }
        s3e = sqlite3_step(stmt);
        if (s3e == SQLITE_ROW) {
            return kSecDbRowStep;
        } else if (s3e == SQLITE_DONE) {
            /*
             ** ^[SQLITE_DONE] means that the statement has finished executing
             ** successfully.  sqlite3_step() should not be called again on this virtual
             ** machine without first calling [] to reset the virtual
             ** machine back to its initial state.
             */
            sqlite3_reset(stmt);
            return kSecDbDoneStep;
        } else if (!SecDbWaitIfNeeded(dbconn, s3e, stmt, CFSTR("step"), ntries, error)) {
            return kSecDbErrorStep;
        }
        ntries++;
    };
}

bool
SecDbExec(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error)
{
    bool ok = true;
    CFRetain(sql);
    while (sql) {
        CFStringRef tail = NULL;
        if (ok) {
            sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, &tail, error);
            ok = stmt != NULL;
            if (stmt) {
                SecDbStepResult sr;
                while ((sr = _SecDbStep(dbconn, stmt, error)) == kSecDbRowStep);
                if (sr == kSecDbErrorStep)
                    ok = false;
                ok &= SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
            }
        } else {
            // TODO We already have an error here we really just want the left over sql in it's userData
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("Error with unexecuted sql remaining %@"), sql);
        }
        CFRelease(sql);
        sql = tail;
    }
    return ok;
}

static int SecDBGetInteger(SecDbConnectionRef dbconn, CFStringRef sql)
{
    __block int number = -1;
    __block CFErrorRef error = NULL;

    (void)SecDbWithSQL(dbconn, sql, &error, ^bool(sqlite3_stmt *sqlStmt) {
        (void)SecDbStep(dbconn, sqlStmt, &error, ^(bool *stop) {
            number = sqlite3_column_int(sqlStmt, 0);
            *stop = true;
        });
        return true;
    });
    CFReleaseNull(error);
    return number;
}


void SecDBManagementTasks(SecDbConnectionRef dbconn)
{
    int64_t page_count = SecDBGetInteger(dbconn, CFSTR("pragma page_count"));
    if (page_count <= 0) {
        return;
    }
    int64_t free_count = SecDBGetInteger(dbconn, CFSTR("pragma freelist_count"));
    if (free_count < 0) {
        return;
    }

    int64_t max_free = 8192;

    int64_t pages_in_use = page_count - free_count;
    double loadFactor = ((double)pages_in_use/(double)page_count);
    if (0.85 < loadFactor && free_count < max_free) {
        /* no work yet */
    } else {
        int64_t pages_to_free = (int64_t)(0.2 * free_count);
        if (0.4 > loadFactor) {
            pages_to_free = free_count;
        }

        char *formatString = NULL;
        asprintf(&formatString, "pragma incremental_vacuum(%d)", (int)pages_to_free);
        if (formatString) {
            char *sqlerror = NULL;
            int rc = sqlite3_exec(dbconn->handle, formatString, NULL, NULL, &sqlerror);
            if (rc) {
                secerror("incremental_vacuum failed with: (%d) %{public}s", rc, sqlerror);
            }
            sqlite3_free(sqlerror);
            free(formatString);
        }
    }
}


static bool SecDbBeginTransaction(SecDbConnectionRef dbconn, SecDbTransactionType type, CFErrorRef *error)
{
    bool ok = true;
    CFStringRef query;
    switch (type) {
        case kSecDbImmediateTransactionType:
            secdebug("db", "SecDbBeginTransaction SecDbBeginTransaction %p", dbconn);
            query = CFSTR("BEGIN IMMEDIATE");
            break;
        case kSecDbExclusiveRemoteSOSTransactionType:
            secdebug("db", "SecDbBeginTransaction kSecDbExclusiveRemoteSOSTransactionType %p", dbconn);
            dbconn->source = kSecDbSOSTransaction;
            query = CFSTR("BEGIN EXCLUSIVE");
            break;
        case kSecDbExclusiveRemoteCKKSTransactionType:
            secdebug("db", "SecDbBeginTransaction kSecDbExclusiveRemoteCKKSTransactionType %p", dbconn);
            dbconn->source = kSecDbCKKSTransaction;
            query = CFSTR("BEGIN EXCLUSIVE");
            break;
        case kSecDbExclusiveTransactionType:
            if (type==kSecDbExclusiveTransactionType)
                secdebug("db", "SecDbBeginTransaction kSecDbExclusiveTransactionType %p", dbconn);
            query = CFSTR("BEGIN EXCLUSIVE");
            break;
        case kSecDbNormalTransactionType:
            secdebug("db", "SecDbBeginTransaction kSecDbNormalTransactionType %p", dbconn);
            query = CFSTR("BEGIN");
            break;
        default:
            secdebug("db", "SecDbBeginTransaction invalid transaction type %lu", type);
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("invalid transaction type %d"), (int)type);
            query = NULL;
            break;
    }

    if (query != NULL && sqlite3_get_autocommit(dbconn->handle) != 0) {
        ok = SecDbExec(dbconn, query, error);
    }
    if (ok)
        dbconn->inTransaction = true;

    return ok;
}

static bool SecDbEndTransaction(SecDbConnectionRef dbconn, bool commit, CFErrorRef *error)
{
    __block bool ok = true;
    __block bool commited = false;

    dispatch_block_t notifyAndExec = ^{
        if (commit) {
            //secdebug("db", "SecDbEndTransaction kSecDbTransactionWillCommit %p", dbconn);
            SecDbNotifyPhase(dbconn, kSecDbTransactionWillCommit);
            commited = ok = SecDbExec(dbconn, CFSTR("END"), error);
            //secdebug("db", "SecDbEndTransaction kSecDbTransactionWillCommit %p (after notify)", dbconn);
        } else {
            ok = SecDbExec(dbconn, CFSTR("ROLLBACK"), error);
            commited = false;
        }
        dbconn->inTransaction = false;
        SecDbNotifyPhase(dbconn, commited ? kSecDbTransactionDidCommit : kSecDbTransactionDidRollback);
        secdebug("db", "SecDbEndTransaction %s %p", commited ? "kSecDbTransactionDidCommit" : "kSecDbTransactionDidRollback", dbconn);
        dbconn->source = kSecDbAPITransaction;

        if (commit && dbconn->db->useRobotVacuum) {
            SecDBManagementTasks(dbconn);
        }
    };

    SecDbPerformOnCommitQueue(dbconn, true, notifyAndExec);

    return ok;
}

bool SecDbTransaction(SecDbConnectionRef dbconn, SecDbTransactionType type,
                      CFErrorRef *error, void (^transaction)(bool *commit))
{
    bool ok = true;
    bool commit = true;

    if (dbconn->inTransaction) {
        transaction(&commit);
        if (!commit) {
            secinfo("#SecDB", "#SecDB nested transaction asked to not be committed");
        }
    } else {
        ok = SecDbBeginTransaction(dbconn, type, error);
        if (ok) {
            transaction(&commit);
            ok = SecDbEndTransaction(dbconn, commit, error);
        }
    }

    return ok && commit;
}

sqlite3 *SecDbHandle(SecDbConnectionRef dbconn) {
    return dbconn->handle;
}

bool SecDbStep(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error, void (^row)(bool *stop)) {
    for (;;) {
        switch (_SecDbStep(dbconn, stmt, error)) {
            case kSecDbErrorStep:
                secdebug("db", "kSecDbErrorStep %@", error ? *error : NULL);
                return false;
            case kSecDbRowStep:
#if SECDB_DEBUGGING
                secdebug("db", "kSecDbRowStep %@", error ? *error : NULL);
#endif
                if (row) {
                    __block bool stop = false;
                    SecAutoreleaseInvokeWithPool(^{
                        row(&stop);
                    });
                    if (stop)
                        return true;
                    break;
                }
                SecDbError(SQLITE_ERROR, error, CFSTR("SecDbStep SQLITE_ROW returned without a row handler"));
                return false;
            case kSecDbDoneStep:
#if SECDB_DEBUGGING
                secdebug("db", "kSecDbDoneStep %@", error ? *error : NULL);
#endif
                return true;
        }
    }
}

bool SecDbCheckpoint(SecDbConnectionRef dbconn, CFErrorRef *error)
{
    return SecDbConnectionCheckCode(dbconn,
                                    sqlite3_wal_checkpoint_v2(dbconn->handle, NULL, SQLITE_CHECKPOINT_FULL, NULL, NULL),
                                    error,
                                    CFSTR("wal_checkpoint(FULL)"));
}

static sqlite3 *_SecDbOpenV2(const char *path,
                             int flags,
                             int useWAL,
                             int useRobotVacuum,
                             CFErrorRef *error) {
    sqlite3 *handle = NULL;
    int s3e = sqlite3_open_v2(path, &handle, flags, NULL);
    if (s3e) {
        if (handle) {
            SecDbErrorWithDb(s3e, handle, error, CFSTR("open_v2 \"%s\" 0x%X"), path, flags);
            sqlite3_close(handle);
            handle = NULL;
        } else {
            SecDbError(s3e, error, CFSTR("open_v2 \"%s\" 0x%X"), path, flags);
        }
    } else if (SQLITE_OPEN_READWRITE == (flags & SQLITE_OPEN_READWRITE)) {
        if (useRobotVacuum) {
#define SECDB_SQLITE_AUTO_VACUUM_INCREMENTAL 2
            sqlite3_stmt *stmt = NULL;
            int vacuumMode = -1;

            /*
             * Setting auto_vacuum = incremental on a database that is not empty requires
             * a VACCUUM, so check if the vacuum mode is not INCREMENTAL, and if its not,
             * set it to incremental and vacuum.
             */

            s3e = sqlite3_prepare_v2(handle, "PRAGMA auto_vacuum", -1, &stmt, NULL);
            if (s3e == 0) {
                s3e = sqlite3_step(stmt);
                if (s3e == SQLITE_ROW) {
                    vacuumMode = sqlite3_column_int(stmt, 0);
                }
                (void)sqlite3_finalize(stmt);
            }

            if (vacuumMode != SECDB_SQLITE_AUTO_VACUUM_INCREMENTAL) {
                (void)sqlite3_exec(handle, "PRAGMA auto_vacuum = incremental", NULL, NULL, NULL);
                (void)sqlite3_exec(handle, "VACUUM", NULL, NULL, NULL);
            }
        }
        if (useWAL) {
            (void)sqlite3_exec(handle, "PRAGMA journal_mode = WAL", NULL, NULL, NULL);
        }

        // Let SQLite handle timeouts.
        sqlite3_busy_timeout(handle, 5*1000);
    }
    return handle;
}

static bool SecDbOpenV2(SecDbConnectionRef dbconn, const char *path, int flags, CFErrorRef *error) {
    return (dbconn->handle = _SecDbOpenV2(path, flags, dbconn->db->useWAL, dbconn->db->useRobotVacuum, error)) != NULL;
}

// This construction lets tests not exit here
static void SecDbProductionCorruptionExitHandler(void)
{
    exit(EXIT_FAILURE);
}
void (*SecDbCorruptionExitHandler)(void) = SecDbProductionCorruptionExitHandler;

void SecDbResetCorruptionExitHandler(void)
{
    SecDbCorruptionExitHandler = SecDbProductionCorruptionExitHandler;
}

/*
 There's not much to do in here because we should only ever be here when
 SQLite tells us the DB is corrupt, or the DB is unrecoverable because of
 some fatal logic problem. But we can't shoot it dead either due to client
 connections. So, first we create a marker to tell ourselves things are bad,
 then we'll die. When we come back up we'll notice the marker and remove the DB.
 */
static bool SecDbHandleCorrupt(SecDbConnectionRef dbconn, int rc, CFErrorRef *error)
{
    if (!dbconn->db->allowRepair) {
        SecCFCreateErrorWithFormat(rc, kSecErrnoDomain, NULL, error, NULL,
                                   CFSTR("SecDbHandleCorrupt not allowed to repair, handled error: [%d] %s"), rc, strerror(rc));
        dbconn->isCorrupted = false;
        return false;
    }

    CFStringPerformWithCString(dbconn->db->db_path, ^(const char *db_path) {
        char marker[PATH_MAX+1];
        snprintf(marker, sizeof(marker), "%s-iscorrupt", db_path);
        struct stat info = {};
        if (0 == stat(marker, &info)) {
            secerror("SecDbHandleCorrupt: Tried to write corruption marker %s but one already exists", marker);
        }

        FILE* file = fopen(marker, "w");
        if (file == NULL) {
            secerror("SecDbHandleCorrupt: Unable (%{darwin.errno}d) to create corruption marker %{public}s", errno, marker);
        } else {
            fclose(file);
        }
    });

    secwarning("SecDbHandleCorrupt: killing self so that successor might cleanly delete corrupt db");

    // Call through function pointer so tests can replace it and call a SecKeychainDbReset instead
    SecDbCorruptionExitHandler();
    return true;
}

static bool SecDbProcessCorruptionMarker(CFStringRef db_path) {
    __block bool ok = true;
    CFStringPerformWithCString(db_path, ^(const char *db_path) {
        char marker[PATH_MAX+1];
        snprintf(marker, sizeof(marker), "%s-iscorrupt", db_path);
        struct stat info = {};
        int result = stat(marker, &info);
        if (result != 0 && errno == ENOENT) {
            return;
        } else if (result != 0) {
            secerror("SecDbSecDbProcessCorruptionMarker: Unable to check for corruption marker: %{darwin.errno}d", errno);
            return;
        }

        secwarning("SecDbSecDbProcessCorruptionMarker: found corruption marker %s", marker);
        if (remove(marker)) {
            secerror("SecDbSecDbProcessCorruptionMarker: Unable (%{darwin.errno}d) to delete corruption marker", errno);
            ok = false;
        } else if (remove(db_path) && errno != ENOENT) {    // Not sure how we'd get ENOENT but it would suit us just fine
            secerror("SecDbSecDbProcessCorruptionMarker: Unable (%{darwin.errno}d) to delete db %{public}s", errno, db_path);
            ok = false;
        } else {
            secwarning("SecDbSecDbProcessCorruptionMarker: deleted corrupt db %{public}s", db_path);
        }
    });
    return ok;
}

void
SecDbSetCorruptionReset(SecDbRef db, void (^corruptionReset)(void))
{
    if (db->corruptionReset) {
        Block_release(db->corruptionReset);
        db->corruptionReset = NULL;
    }
    if (corruptionReset) {
        db->corruptionReset = Block_copy(corruptionReset);
    }
}

static bool SecDbLoggingEnabled(CFStringRef type)
{
    CFTypeRef profile = NULL;
    bool enabled = false;

    if (csr_check(CSR_ALLOW_APPLE_INTERNAL) != 0)
        return false;

    profile = (CFNumberRef)CFPreferencesCopyValue(CFSTR("SQLProfile"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesAnyHost);

    if (profile == NULL)
        return false;

    if (CFGetTypeID(profile) == CFBooleanGetTypeID()) {
        enabled = CFBooleanGetValue((CFBooleanRef)profile);
    } else if (CFGetTypeID(profile) == CFNumberGetTypeID()) {
        int32_t num = 0;
        CFNumberGetValue(profile, kCFNumberSInt32Type, &num);
        enabled = !!num;
    }

    CFReleaseSafe(profile);

    return enabled;
}

static unsigned
SecDbProfileMask(void)
{
    static dispatch_once_t onceToken;
    static unsigned profile_mask = 0;

    // sudo defaults write /Library/Preferences/com.apple.security SQLProfile -bool true
    dispatch_once(&onceToken, ^{
        if (SecDbLoggingEnabled(CFSTR("SQLProfile")))
            profile_mask = SQLITE_TRACE_PROFILE;
#if DEBUG
        profile_mask |= SQLITE_TRACE_STMT;
#else
        if (SecDbLoggingEnabled(CFSTR("SQLTrace")))
            profile_mask = SQLITE_TRACE_STMT;
#endif
        if (SecDbLoggingEnabled(CFSTR("SQLRow")))
            profile_mask = SQLITE_TRACE_ROW;
        secinfo("#SecDB", "sqlDb: sql trace mask: 0x%08x", profile_mask);
    });
    return profile_mask;
}

static int
SecDbTraceV2(unsigned mask, void *ctx, void *p, void *x) {
    SecDbConnectionRef dbconn __unused = ctx;

#if SECDB_DEBUGGING
    const char *trace = "unknown";
    char *tofree = NULL;

    if (mask == SQLITE_TRACE_PROFILE)
        trace = sqlite3_sql(p);
    else if (mask == SQLITE_TRACE_STMT) {
        trace = sqlite3_sql(p);
    } else if (mask == SQLITE_TRACE_ROW) {
        trace = tofree = sqlite3_expanded_sql(p);
    }

    secinfo("#SecDB", "#SecDB %{public}s", trace);

    sqlite3_free(tofree);
#endif

    return 0;
}


static bool SecDbOpenHandle(SecDbConnectionRef dbconn, bool *created, CFErrorRef *error)
{
    __block bool ok = true;

    // This is pretty terrible because now what? We know we have a corrupt DB
    // and now we can't get rid of it.
    if (!SecDbProcessCorruptionMarker(dbconn->db->db_path)) {
        SecCFCreateErrorWithFormat(errno, kSecErrnoDomain, NULL, error, NULL, CFSTR("Unable to process corruption marker: %{darwin.errno}d"), errno);
        return false;
    }

    CFStringPerformWithCString(dbconn->db->db_path, ^(const char *db_path) {
#if TARGET_OS_IPHONE
        int flags = SQLITE_OPEN_FILEPROTECTION_NONE;
#else
        int flags = 0;
#endif
        flags |= (dbconn->db->readWrite) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
        ok = created && SecDbOpenV2(dbconn, db_path, flags, NULL);
        if (!ok) {
            ok = true;
            if (created) {
                char *tmp = dirname((char *)db_path);
                if (tmp) {
                    mode_t omode = dbconn->db->mode;
                    if (omode & S_IRUSR) { omode |= S_IXUSR; } // owner can read
                    if (omode & S_IRGRP) { omode |= S_IXGRP; } // group can read
                    if (omode & S_IROTH) { omode |= S_IXOTH; } // other can read
                    int errnum = mkpath_np(tmp, omode);
                    if (errnum != 0 && errnum != EEXIST) {
                        SecCFCreateErrorWithFormat(errnum, kSecErrnoDomain, NULL, error, NULL,
                                                   CFSTR("mkpath_np %s: [%d] %s"), tmp, errnum, strerror(errnum));
                        ok = false;
                    }
                }
            }
            // if the enclosing directory is ok, try to create the database.
            // this forces us to open it read-write, so we'll need to be the owner here.
            flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
#if TARGET_OS_IPHONE
            flags |= SQLITE_OPEN_FILEPROTECTION_NONE;
#endif
            ok = ok && SecDbOpenV2(dbconn, db_path, flags, error);
            if (ok) {
                chmod(db_path, dbconn->db->mode); // default: 0600 (S_IRUSR | S_IWUSR)
                if (created)
                    *created = true;
            }
        }

        if (ok) {
            unsigned mask = SecDbProfileMask();
            if (mask) {
                (void)sqlite3_trace_v2(dbconn->handle,
                                       mask,
                                       SecDbTraceV2,
                                       dbconn);
            }
        }
    });

    return ok;
}

static SecDbConnectionRef
SecDbConnectionCreate(SecDbRef db, bool readOnly, CFErrorRef *error)
{
    SecDbConnectionRef dbconn = NULL;

    dbconn = CFTypeAllocate(SecDbConnection, struct __OpaqueSecDbConnection, kCFAllocatorDefault);
    require(dbconn != NULL, done);

    dbconn->db = db;
    dbconn->readOnly = readOnly;
    dbconn->inTransaction = false;
    dbconn->source = kSecDbInvalidTransaction;
    dbconn->isCorrupted = false;
    dbconn->maybeCorruptedCode = 0;
    dbconn->hasIOFailure = false;
    dbconn->corruptionError = NULL;
    dbconn->handle = NULL;
    dbconn->changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

done:
    return dbconn;
}

bool SecDbConnectionIsReadOnly(SecDbConnectionRef dbconn) {
    return dbconn->readOnly;
}

static void SecDbConectionSetReadOnly(SecDbConnectionRef dbconn, bool readOnly) {
    dbconn->readOnly = readOnly;
}

SecDbConnectionRef SecDbConnectionAcquire(SecDbRef db, bool readOnly, CFErrorRef *error) {
    SecDbConnectionRef dbconn = NULL;
    SecDbConnectionAcquireRefMigrationSafe(db, readOnly, &dbconn, error);
    return dbconn;
}

static void SecDbConnectionConsumeResource(SecDbRef db, bool readOnly) {
    if (readOnly) {
        dispatch_semaphore_wait(db->readSemaphore, DISPATCH_TIME_FOREVER);
    } else {
        pthread_mutex_lock(&(db->writeMutex));
    }
}

static void SecDbConnectionMakeResourceAvailable(SecDbRef db, bool readOnly) {
    if (readOnly) {
        dispatch_semaphore_signal(db->readSemaphore);
    } else {
        pthread_mutex_unlock(&(db->writeMutex));
    }
}

bool SecDbConnectionAcquireRefMigrationSafe(SecDbRef db, bool readOnly, SecDbConnectionRef* dbconnRef, CFErrorRef *error)
{
    CFRetain(db);
#if SECDB_DEBUGGING
    secinfo("dbconn", "acquire %s connection", readOnly ? "ro" : "rw");
#endif
    SecDbConnectionConsumeResource(db, readOnly);

    __block SecDbConnectionRef dbconn = NULL;
    __block bool ok = true;
    __block bool ranOpenedHandler = false;

    bool (^assignDbConn)(SecDbConnectionRef) = ^bool(SecDbConnectionRef connection) {
        dbconn = connection;
        if (dbconnRef) {
            *dbconnRef = connection;
        }

        return dbconn != NULL;
    };

    dispatch_sync(db->queue, ^{
        if (!db->didFirstOpen) {
            bool didCreate = false;
            ok = assignDbConn(SecDbConnectionCreate(db, false, error));
            CFErrorRef localError = NULL;
            if (ok && !SecDbOpenHandle(dbconn, &didCreate, &localError)) {
                secerror("Unable to create database: %@", localError);
                if (localError && CFEqual(CFErrorGetDomain(localError), kSecDbErrorDomain)) {
                    int code = (int)CFErrorGetCode(localError);
                    dbconn->isCorrupted = (SQLITE_CORRUPT == code) || (SQLITE_NOTADB == code);
                }
                // If the open failure isn't due to corruption, propagate the error.
                ok = dbconn->isCorrupted;
                if (!ok && error && *error == NULL) {
                    *error = localError;
                    localError = NULL;
                }
            }
            CFReleaseNull(localError);

            if (ok) {
                db->didFirstOpen = ok = SecDbDidCreateFirstConnection(dbconn, didCreate, error);
                ranOpenedHandler = true;
                SecDbConectionSetReadOnly(dbconn, readOnly);    // first connection always created "rw", so set to real value
            } else {
                CFReleaseNull(dbconn);
            }
        } else {
            /* Try to get one from the cache */
            CFMutableArrayRef cache = readOnly ? db->idleReadConnections : db->idleWriteConnections;
            if (CFArrayGetCount(cache) && !dbconn) {
                if (assignDbConn((SecDbConnectionRef)CFArrayGetValueAtIndex(cache, 0))) {
                    CFRetainSafe(dbconn);
                }
                CFArrayRemoveValueAtIndex(cache, 0);
            }
        }
    });

    if (ok && !dbconn) {
        /* Nothing found in cache, create a new connection */
        bool created = false;
        if (assignDbConn(SecDbConnectionCreate(db, readOnly, error)) && !SecDbOpenHandle(dbconn, &created, error)) {
            CFReleaseNull(dbconn);
        }
    }

    if (dbconn && !ranOpenedHandler && dbconn->db->opened) {
        dispatch_sync(db->queue, ^{
            if (dbconn->db->callOpenedHandlerForNextConnection) {
                dbconn->db->callOpenedHandlerForNextConnection = false;
                if (!dbconn->db->opened(db, dbconn, false, &dbconn->db->callOpenedHandlerForNextConnection, error)) {
                    if (!dbconn->isCorrupted || !SecDbHandleCorrupt(dbconn, 0, error)) {
                        CFReleaseNull(dbconn);
                    }
                }
            }
        });
    }

    if (dbconnRef) {
        *dbconnRef = dbconn;
    }

    if (!dbconn) {
        // Caller doesn't get (to use) a connection so the backing synchronization primitive is available again
        SecDbConnectionMakeResourceAvailable(db, readOnly);
        CFRelease(db);
    }

    return dbconn ? true : false;
}

void SecDbConnectionRelease(SecDbConnectionRef dbconn) {
    if (!dbconn) {
        secerror("SecDbConnectionRelease called with NULL dbconn");
        return;
    }
    SecDbRef db = dbconn->db;
#if SECDB_DEBUGGING
    secinfo("dbconn", "release %@", dbconn);
#endif

    bool readOnly = SecDbConnectionIsReadOnly(dbconn);
    dispatch_sync(db->queue, ^{
        if (dbconn->hasIOFailure) {
            // Something wrong on the file layer (e.g. revoked file descriptor for networked home)
            secwarning("SecDbConnectionRelease: IO failure reported in connection, throwing away currently idle caches");
            // Any other checked-out connections are beyond our grasp. If they did not have IO failures they'll come back,
            // otherwise this branch gets taken more than once and gradually those connections die off
            CFArrayRemoveAllValues(db->idleWriteConnections);
            CFArrayRemoveAllValues(db->idleReadConnections);
        } else {
            CFMutableArrayRef cache = readOnly ? db->idleReadConnections : db->idleWriteConnections;
            CFIndex count = CFArrayGetCount(cache);
            if ((unsigned long)count < (readOnly ? kSecDbMaxReaders : kSecDbMaxWriters)) {
                CFArrayAppendValue(cache, dbconn);
            } else {
                secerror("dbconn: did not expect to run out of room in the %s cache when releasing connection", readOnly ? "ro" : "rw");
            }
        }
    });

    // Signal after we have put the connection back in the pool of connections
    SecDbConnectionMakeResourceAvailable(db, readOnly);
    CFRelease(dbconn);
    CFRelease(db);
}

void SecDbReleaseAllConnections(SecDbRef db) {
    // Force all connections to be removed (e.g. file descriptor no longer valid)
    if (!db) {
        secerror("called with NULL db");
        return;
    }
    dispatch_sync(db->queue, ^{
        CFArrayRemoveAllValues(db->idleReadConnections);
        CFArrayRemoveAllValues(db->idleWriteConnections);
    });
}

static void onQueueSecDbForceCloseForCache(CFMutableArrayRef cache) {
    CFArrayForEach(cache, ^(const void* ptr) {
        SecDbConnectionRef connection = (SecDbConnectionRef)ptr;

        // this pointer is claimed to be nonretained
        connection->db = NULL;

        if(connection->handle) {
            sqlite3_close(connection->handle);
            connection->handle = NULL;
        }
    });
    CFArrayRemoveAllValues(cache);
}

// Please make sure you want to do this. Any use of the outstanding connections to this DB will cause a crash.
void SecDbForceClose(SecDbRef db) {
    dispatch_sync(db->queue, ^{
        onQueueSecDbForceCloseForCache(db->idleReadConnections);
        onQueueSecDbForceCloseForCache(db->idleWriteConnections);
    });
}

bool SecDbPerformRead(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn)) {
    SecDbConnectionRef dbconn = SecDbConnectionAcquire(db, true, error);
    bool success = false;
    if (dbconn) {
        perform(dbconn);
        success = true;
        SecDbConnectionRelease(dbconn);
    }
    return success;
}

bool SecDbPerformWrite(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn)) {
    if(!db) {
        SecError(errSecNotAvailable, error, CFSTR("failed to get a db handle"));
        return false;
    }
    SecDbConnectionRef dbconn = SecDbConnectionAcquire(db, false, error);
    bool success = false;
    if (dbconn) {
        perform(dbconn);
        success = true;
        SecDbConnectionRelease(dbconn);
    }
    return success;
}

static CFStringRef
SecDbConnectionCopyFormatDescription(CFTypeRef value, CFDictionaryRef formatOptions)
{
    SecDbConnectionRef dbconn = (SecDbConnectionRef)value;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecDbConnection %s %s>"),
                                    dbconn->readOnly ? "ro" : "rw", dbconn->handle ? "open" : "closed");
}

static void
SecDbConnectionDestroy(CFTypeRef value)
{
    SecDbConnectionRef dbconn = (SecDbConnectionRef)value;
    if (dbconn->handle) {
        int s3e = sqlite3_close(dbconn->handle);
        if (s3e != SQLITE_OK) {
            secerror("failed to close database connection (%d) for %@: %s", s3e, dbconn->db->db_path, sqlite3_errmsg(dbconn->handle));
        }
        os_assert(s3e == SQLITE_OK); // Crash now or jetsam later
    }
    dbconn->db = NULL;
    CFReleaseNull(dbconn->changes);
    CFReleaseNull(dbconn->corruptionError);

}

void SecDbPerformOnCommitQueue(SecDbConnectionRef dbconn, bool barrier, dispatch_block_t perform) {
    if (barrier) {
        dispatch_barrier_sync(dbconn->db->commitQueue, ^{
            perform();
        });
    } else {
        dispatch_sync(dbconn->db->commitQueue, ^{
            perform();
        });
    }
}

// MARK: -
// MARK: Bind helpers

// Logging binds is very spammy when debug logging is on (~90% of log lines), and isn't often useful.
// Enable this in your local build if you actually want every single SQL variable bind logged for debugging.
#define LOG_SECDB_BINDS 0

bool SecDbBindBlob(sqlite3_stmt *stmt, int param, const void *zData, size_t n, void(*xDel)(void*), CFErrorRef *error) {
    if (n > INT_MAX) {
        return SecDbErrorWithStmt(SQLITE_TOOBIG, stmt, error,
                                  CFSTR("bind_blob[%d]: blob bigger than INT_MAX"), param);
    }
    bool ok = SecDbErrorWithStmt(sqlite3_bind_blob(stmt, param, zData, (int)n, xDel),
                                 stmt, error, CFSTR("bind_blob[%d]"), param);
#if LOG_SECDB_BINDS
    secinfo("bind", "bind_blob[%d]: %.*P: %@", param, (int)n, zData, error ? *error : NULL);
#endif
    return ok;
}

bool SecDbBindText(sqlite3_stmt *stmt, int param, const char *zData, size_t n, void(*xDel)(void*), CFErrorRef *error) {
    if (n > INT_MAX) {
        return SecDbErrorWithStmt(SQLITE_TOOBIG, stmt, error,
                                  CFSTR("bind_text[%d]: text bigger than INT_MAX"), param);
    }
    bool ok = SecDbErrorWithStmt(sqlite3_bind_text(stmt, param, zData, (int)n, xDel), stmt, error,
                                 CFSTR("bind_text[%d]"), param);
#if LOG_SECDB_BINDS
    secinfo("bind", "bind_text[%d]: \"%s\" error: %@", param, zData, error ? *error : NULL);
#endif
    return ok;
}

bool SecDbBindDouble(sqlite3_stmt *stmt, int param, double value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_double(stmt, param, value), stmt, error,
                                 CFSTR("bind_double[%d]"), param);
#if LOG_SECDB_BINDS
    secinfo("bind", "bind_double[%d]: %f error: %@", param, value, error ? *error : NULL);
#endif
    return ok;
}

bool SecDbBindInt(sqlite3_stmt *stmt, int param, int value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_int(stmt, param, value), stmt, error,
                                 CFSTR("bind_int[%d]"), param);
#if LOG_SECDB_BINDS
    secinfo("bind", "bind_int[%d]: %d error: %@", param, value, error ? *error : NULL);
#endif
    return ok;
}

bool SecDbBindInt64(sqlite3_stmt *stmt, int param, sqlite3_int64 value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_int64(stmt, param, value), stmt, error,
                                 CFSTR("bind_int64[%d]"), param);
#if LOG_SECDB_BINDS
    secinfo("bind", "bind_int64[%d]: %lld error: %@", param, value, error ? *error : NULL);
#endif
    return ok;
}


/* AUDIT[securityd](done):
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
bool SecDbBindObject(sqlite3_stmt *stmt, int param, CFTypeRef value, CFErrorRef *error) {
    CFTypeID valueId;
    __block bool result = false;

	/* TODO: Can we use SQLITE_STATIC below everwhere we currently use
     SQLITE_TRANSIENT since we finalize the statement before the value
     goes out of scope? */
    if (!value || (valueId = CFGetTypeID(value)) == CFNullGetTypeID()) {
        /* Skip bindings for NULL values.  sqlite3 will interpret unbound
         params as NULL which is exactly what we want. */
        result = true;
    } else if (valueId == CFStringGetTypeID()) {
        CFStringPerformWithCStringAndLength(value, ^(const char *cstr, size_t clen) {
            result = SecDbBindText(stmt, param, cstr, clen, SQLITE_TRANSIENT, error);
        });
    } else if (valueId == CFDataGetTypeID()) {
        CFIndex len = CFDataGetLength(value);
        if (len) {
            result = SecDbBindBlob(stmt, param, CFDataGetBytePtr(value),
                                   len, SQLITE_TRANSIENT, error);
        } else {
            result = SecDbBindText(stmt, param, "", 0, SQLITE_TRANSIENT, error);
        }
    } else if (valueId == CFDateGetTypeID()) {
        CFAbsoluteTime abs_time = CFDateGetAbsoluteTime(value);
        result = SecDbBindDouble(stmt, param, abs_time, error);
    } else if (valueId == CFBooleanGetTypeID()) {
        int bval = CFBooleanGetValue(value);
        result = SecDbBindInt(stmt, param, bval, error);
    } else if (valueId == CFNumberGetTypeID()) {
        Boolean convertOk;
        if (CFNumberIsFloatType(value)) {
            double nval;
            convertOk = CFNumberGetValue(value, kCFNumberDoubleType, &nval);
            result = SecDbBindDouble(stmt, param, nval, error);
        } else {
            sqlite_int64 nval64;
            convertOk = CFNumberGetValue(value, kCFNumberSInt64Type, &nval64);
            if (convertOk) {
                result = SecDbBindInt64(stmt, param, nval64, error);
            }
        }
        if (!convertOk) {
            result = SecDbError(SQLITE_INTERNAL, error, CFSTR("bind CFNumberGetValue failed for %@"), value);
        }
    } else {
        if (error) {
            CFStringRef valueDesc = CFCopyTypeIDDescription(valueId);
            SecDbError(SQLITE_MISMATCH, error, CFSTR("bind unsupported type %@"), valueDesc);
            CFReleaseSafe(valueDesc);
        }
    }

	return result;
}

// MARK: -
// MARK: SecDbStatementRef

bool SecDbReset(sqlite3_stmt *stmt, CFErrorRef *error) {
    return SecDbErrorWithStmt(sqlite3_reset(stmt), stmt, error, CFSTR("reset"));
}

bool SecDbClearBindings(sqlite3_stmt *stmt, CFErrorRef *error) {
    return SecDbErrorWithStmt(sqlite3_clear_bindings(stmt), stmt, error, CFSTR("clear bindings"));
}

bool SecDbFinalize(sqlite3_stmt *stmt, CFErrorRef *error) {
    sqlite3 *handle = sqlite3_db_handle(stmt);
    int s3e = sqlite3_finalize(stmt);
    return s3e == SQLITE_OK ? true : SecDbErrorWithDb(s3e, handle, error, CFSTR("finalize: %p"), stmt);
}

sqlite3_stmt *SecDbPrepareV2(SecDbConnectionRef dbconn, const char *sql, size_t sqlLen, const char **sqlTail, CFErrorRef *error) {
    sqlite3 *db = SecDbHandle(dbconn);
    if (sqlLen > INT_MAX) {
        SecDbErrorWithDb(SQLITE_TOOBIG, db, error, CFSTR("prepare_v2: sql bigger than INT_MAX"));
        return NULL;
    }
    int ntries = 0;
    for (;;) {
        sqlite3_stmt *stmt = NULL;
        int s3e = sqlite3_prepare_v2(db, sql, (int)sqlLen, &stmt, sqlTail);
        if (s3e == SQLITE_OK)
            return stmt;
        else if (!SecDbWaitIfNeeded(dbconn, s3e, NULL, CFSTR("preparev2"), ntries, error))
            return NULL;
        ntries++;
    }
}

static sqlite3_stmt *SecDbCopyStatementWithTailRange(SecDbConnectionRef dbconn, CFStringRef sql, CFRange *sqlTail, CFErrorRef *error) {
    __block sqlite3_stmt *stmt = NULL;
    if (sql) CFStringPerformWithCStringAndLength(sql, ^(const char *sqlStr, size_t sqlLen) {
        const char *tail = NULL;
        stmt = SecDbPrepareV2(dbconn, sqlStr, sqlLen, &tail, error);
        if (sqlTail && sqlStr < tail && tail < sqlStr + sqlLen) {
            sqlTail->location = tail - sqlStr;
            sqlTail->length = sqlLen - sqlTail->location;
        }
    });

    return stmt;
}

sqlite3_stmt *SecDbCopyStmt(SecDbConnectionRef dbconn, CFStringRef sql, CFStringRef *tail, CFErrorRef *error) {
    // TODO: Add caching and cache lookup of statements
    CFRange sqlTail = {};
    sqlite3_stmt *stmt = SecDbCopyStatementWithTailRange(dbconn, sql, &sqlTail, error);
    if (sqlTail.length > 0) {
        CFStringRef excess = CFStringCreateWithSubstring(CFGetAllocator(sql), sql, sqlTail);
        if (tail) {
            *tail = excess;
        } else {
            SecDbError(SQLITE_INTERNAL, error,
                       CFSTR("prepare_v2: %@ unused sql: %@"),
                       sql, excess);
            CFReleaseSafe(excess);
            SecDbFinalize(stmt, error);
            stmt = NULL;
        }
    }
    return stmt;
}

/*
 TODO: Could do a hack here with a custom kCFAllocatorNULL allocator for a second CFRuntimeBase inside a SecDbStatement,
 TODO: Better yet make a full blow SecDbStatement instance whenever SecDbCopyStmt is called.  Then, when the statement is released, in the Dispose method, we Reset and ClearBindings the sqlite3_stmt * and hand it back to the SecDb with the original CFStringRef for the sql (or hash thereof) as an argument. */
bool SecDbReleaseCachedStmt(SecDbConnectionRef dbconn, CFStringRef sql, sqlite3_stmt *stmt, CFErrorRef *error) {
    if (stmt) {
        return SecDbFinalize(stmt, error);
    }
    return true;
}

bool SecDbPrepare(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, void(^exec)(sqlite3_stmt *stmt)) {
    assert(sql != NULL);
    sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, NULL, error);
    if (!stmt)
        return false;

    exec(stmt);
    return SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
}

bool SecDbWithSQL(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, bool(^perform)(sqlite3_stmt *stmt)) {
    bool ok = true;
    CFRetain(sql);
    while (sql) {
        CFStringRef tail = NULL;
        if (ok) {
            sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, &tail, error);
            ok = stmt != NULL;
            if (stmt) {
                if (perform) {
                    ok = perform(stmt);
                } else {
                    // TODO: Use a different error scope here.
                    ok = SecError(-50 /* errSecParam */, error, CFSTR("SecDbWithSQL perform block missing"));
                }
                ok &= SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
            }
        } else {
            // TODO We already have an error here we really just want the left over sql in it's userData
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("Error with unexecuted sql remaining %@"), sql);
        }
        CFRelease(sql);
        sql = tail;
    }
    return ok;
}

/* SecDbForEach returns true if all SQLITE_ROW returns of sqlite3_step() return true from the row block.
 If the row block returns false and doesn't set an error (to indicate it has reached a limit),
 this entire function returns false. In that case no error will be set. */
bool SecDbForEach(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error, bool(^row)(int row_index)) {
    bool result = false;
    for (int row_ix = 0;;++row_ix) {
        if (SecDbConnectionIsReadOnly(dbconn) && !sqlite3_stmt_readonly(stmt)) {
            secerror("SecDbForEach: SecDbConnection is readonly but we're about to write: %s", sqlite3_sql(stmt));
        }
        int s3e = sqlite3_step(stmt);
        if (s3e == SQLITE_ROW) {
            if (row) {
                if (!row(row_ix)) {
                    break;
                }
            } else {
                // If we have no row block then getting SQLITE_ROW is an error
                SecDbError(s3e, error,
                           CFSTR("step[%d]: %s returned SQLITE_ROW with NULL row block"),
                           row_ix, sqlite3_sql(stmt));
            }
        } else {
            if (s3e == SQLITE_DONE) {
                result = true;
            } else {
                SecDbConnectionCheckCode(dbconn, s3e, error, CFSTR("SecDbForEach step[%d]"), row_ix);
            }
            break;
        }
    }
    return result;
}

void SecDbRecordChange(SecDbConnectionRef dbconn, CFTypeRef deleted, CFTypeRef inserted) {
    if (!dbconn->db->notifyPhase) return;
    CFTypeRef entry = SecDbEventCreateWithComponents(deleted, inserted);
    if (entry) {
        CFArrayAppendValue(dbconn->changes, entry);
        CFRelease(entry);

        if (!dbconn->inTransaction) {
            secerror("db %@ changed outside txn", dbconn);
            // Only notify of DidCommit, since WillCommit code assumes
            // we are in a txn.
            SecDbOnNotify(dbconn, ^{
                SecDbNotifyPhase(dbconn, kSecDbTransactionDidCommit);
            });
        }
    }
}


CFGiblisFor(SecDbConnection)

//
// SecDbEvent Creation and consumption
//

static SecDbEventRef SecDbEventCreateInsert(CFTypeRef inserted) {
    return CFRetainSafe(inserted);
}

static SecDbEventRef SecDbEventCreateDelete(CFTypeRef deleted) {
    return CFArrayCreate(kCFAllocatorDefault, &deleted, 1, &kCFTypeArrayCallBacks);
}

static SecDbEventRef SecDbEventCreateUpdate(CFTypeRef deleted, CFTypeRef inserted) {
    const void *values[2] = { deleted, inserted };
    return CFArrayCreate(kCFAllocatorDefault, values, 2, &kCFTypeArrayCallBacks);
}

SecDbEventRef SecDbEventCreateWithComponents(CFTypeRef deleted, CFTypeRef inserted) {
    if (deleted && inserted)
        return SecDbEventCreateUpdate(deleted, inserted);
    else if (deleted)
        return SecDbEventCreateDelete(deleted);
    else if (inserted)
        return SecDbEventCreateInsert(inserted);
    else
        return NULL;
}

void SecDbEventTranslateComponents(SecDbEventRef item, CFTypeRef* deleted, CFTypeRef* inserted) {
    if(CFGetTypeID(item) == CFArrayGetTypeID()) {
        // One item: deletion. Two: update.
        CFIndex arraySize = CFArrayGetCount(item);
        if(arraySize == 1) {
            if(deleted) { *deleted = CFArrayGetValueAtIndex(item, 0); }
            if(inserted) { *inserted = NULL; }
        } else if(arraySize == 2) {
            if(deleted) { *deleted = CFArrayGetValueAtIndex(item, 0); }
            if(inserted) { *inserted = CFArrayGetValueAtIndex(item, 1); }
        } else {
            if(deleted) { *deleted = NULL; }
            if(inserted) { *inserted = NULL; }
        }
    } else {
        if(deleted) { *deleted = NULL; }
        if(inserted) { *inserted = item; }
    }

}

bool SecDbEventGetComponents(SecDbEventRef event, CFTypeRef *deleted, CFTypeRef *inserted, CFErrorRef *error) {
    if (isArray(event)) {
        CFArrayRef array = event;
        switch (CFArrayGetCount(array)) {
            case 2:
                *deleted = CFArrayGetValueAtIndex(array, 0);
                *inserted = CFArrayGetValueAtIndex(array, 1);
                break;
            case 1:
                *deleted = CFArrayGetValueAtIndex(array, 0);
                *inserted = NULL;
                break;
            default:
                SecError(errSecParam, error, NULL, CFSTR("invalid entry in changes array: %@"), array);
                break;
        }
    } else {
        *deleted = NULL;
        *inserted = event;
    }
    return true;
}
