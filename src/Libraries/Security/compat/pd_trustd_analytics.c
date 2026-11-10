// pd_trustd_analytics.c - trustd's health reporting, to the log
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// database and update failures go to the log, successes and networking statistics are not kept
#include "pd_trustd_prefix.h"
#include <utilities/debugging.h>

static const char *const databaseNames[] = { "OCSP cache", "CA issuer cache", "revocation db", "trust store", "pinning db" };
static const char *const operationNames[] = { "read", "write", "create", "open" };

void TrustdHealthAnalyticsLogErrorCodeForDatabase(TAAffectedDatabase database, TADatabaseOperation operation,
    TAFailureType failure, CFIndex errorCode)
{
    secerror("trustd: %s %s failed (%s): %ld", databaseNames[database], operationNames[operation],
        failure == TAFatalError ? "fatal" : "recoverable", (long)errorCode);
}

void TrustdHealthAnalyticsLogErrorCode(TAEvent event, TAFailureType failure, CFIndex errorCode)
{
    secerror("trustd: valid update failed (%s): %ld", failure == TAFatalError ? "fatal" : "recoverable", (long)errorCode);
}

void TrustdHealthAnalyticsLogSuccess(TAEvent event)
{
}

void TrustdHealthAnalyticsLogEvaluationCompleted(void)
{
}

bool SecNetworkingAnalyticsReport(CFStringRef eventName, void *attributes, CFErrorRef *error)
{
    return true;
}
