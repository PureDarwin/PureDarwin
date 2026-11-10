// pd_trustd_prefix.h - trustd's health analytics declarations, whose header this tree does not carry
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// pd_trustd_analytics.c reports database errors to the log, as trustd does alongside analytics
#ifndef PD_TRUSTD_PREFIX_H
#define PD_TRUSTD_PREFIX_H

#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>

typedef enum {
    TAOCSPCache,
    TACAIssuerCache,
    TARevocationDb,
    TATrustStore,
    TAPinningDb,
} TAAffectedDatabase;

typedef enum {
    TAOperationRead,
    TAOperationWrite,
    TAOperationCreate,
    TAOperationOpen,
} TADatabaseOperation;

typedef enum {
    TANoError,
    TARecoverableError,
    TAFatalError,
} TAFailureType;

typedef enum {
    TAEventValidUpdate,
} TAEvent;

// TAValidStatus is SecTrustServer.h's own

void TrustdHealthAnalyticsLogErrorCodeForDatabase(TAAffectedDatabase database, TADatabaseOperation operation,
    TAFailureType failure, CFIndex errorCode);
void TrustdHealthAnalyticsLogErrorCode(TAEvent event, TAFailureType failure, CFIndex errorCode);
void TrustdHealthAnalyticsLogSuccess(TAEvent event);
void TrustdHealthAnalyticsLogEvaluationCompleted(void);
bool SecNetworkingAnalyticsReport(CFStringRef eventName, void *attributes, CFErrorRef *error);

#endif
