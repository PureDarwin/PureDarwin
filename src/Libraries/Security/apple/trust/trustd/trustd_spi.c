/*
 * Copyright (c) 2018-2020 Apple Inc.  All Rights Reserved.
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

#ifdef LIBTRUSTD
#include <CoreFoundation/CoreFoundation.h>

#include "../utilities/SecFileLocations.h"
#include "../utilities/debugging.h"

#include "../sec/ipc/securityd_client.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "trust/trustd/SecOCSPCache.h"
#include "trust/trustd/OTATrustUtilities.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/SecRevocationDb.h"
#include "trust/trustd/SecPinningDb.h"
#include "trust/trustd/SecTrustExceptionResetCount.h"
#include "trustd_spi.h"

#if TARGET_OS_OSX
#include "trust/trustd/macOS/SecTrustOSXEntryPoints.h"
#endif

#endif // LIBTRUSTD

#ifdef LIBTRUSTD
struct trustd trustd_spi = {
    .sec_trust_store_for_domain             = SecTrustStoreForDomainName,
    .sec_trust_store_contains               = _SecTrustStoreContainsCertificate,
    .sec_trust_store_set_trust_settings     = _SecTrustStoreSetTrustSettings,
    .sec_trust_store_remove_certificate     = _SecTrustStoreRemoveCertificate,
    .sec_truststore_remove_all              = _SecTrustStoreRemoveAll,
    .sec_trust_evaluate                     = SecTrustServerEvaluate,
    .sec_ota_pki_trust_store_version        = SecOTAPKIGetCurrentTrustStoreVersion,
    .sec_ota_pki_asset_version              = SecOTAPKIGetCurrentAssetVersion,
    .ota_CopyEscrowCertificates             = SecOTAPKICopyCurrentEscrowCertificates,
    .sec_ota_pki_copy_trusted_ct_logs       = SecOTAPKICopyCurrentTrustedCTLogs,
    .sec_ota_pki_copy_ct_log_for_keyid      = SecOTAPKICopyCTLogForKeyID,
    .sec_ota_pki_get_new_asset              = SecOTAPKISignalNewAsset,
    .sec_ota_secexperiment_get_new_asset    = SecOTASecExperimentGetNewAsset,
    .sec_ota_secexperiment_get_asset        = SecOTASecExperimentCopyAsset,
    .sec_trust_store_copy_all               = _SecTrustStoreCopyAll,
    .sec_trust_store_copy_usage_constraints = _SecTrustStoreCopyUsageConstraints,
    .sec_ocsp_cache_flush                   = SecOCSPCacheFlush,
    .sec_networking_analytics_report        = SecNetworkingAnalyticsReport,
    .sec_trust_store_set_ct_exceptions      = _SecTrustStoreSetCTExceptions,
    .sec_trust_store_copy_ct_exceptions     = _SecTrustStoreCopyCTExceptions,
    .sec_trust_get_exception_reset_count    = SecTrustServerGetExceptionResetCount,
    .sec_trust_increment_exception_reset_count = SecTrustServerIncrementExceptionResetCount,
    .sec_trust_store_set_ca_revocation_additions = _SecTrustStoreSetCARevocationAdditions,
    .sec_trust_store_copy_ca_revocation_additions = _SecTrustStoreCopyCARevocationAdditions,
    .sec_valid_update = SecRevocationDbUpdate,
    .sec_trust_store_set_transparent_connection_pins = _SecTrustStoreSetTransparentConnectionPins,
    .sec_trust_store_copy_transparent_connection_pins = _SecTrustStoreCopyTransparentConnectionPins,
};
#endif

void trustd_init(CFURLRef home_path) {
    if (home_path)
        SecSetCustomHomeURL(home_path);

    trustd_init_server();
}

void trustd_init_server(void) {
    gTrustd = &trustd_spi;
#ifdef LIBTRUSTD
    _SecTrustStoreMigrateConfigurations();
    SecTrustServerMigrateExceptionsResetCount();
#if TARGET_OS_IPHONE
    CFErrorRef error = NULL;
    if (!_SecTrustStoreMigrateUserStore(&error)) {
        secerror("failed to migrate user trust store; new trust store will be empty: %@", error);
    }
    CFReleaseNull(error);
#endif

    SecPolicyServerInitialize();    // set up callbacks for policy checks
    SecRevocationDbInitialize();    // set up revocation database if it doesn't already exist, or needs to be replaced
    SecPinningDbInitialize();       // set up the pinning database
#if TARGET_OS_OSX
    SecTrustLegacySourcesListenForKeychainEvents(); // set up the legacy keychain event listeners (for cache invalidation)
#endif
#endif  // LIBTRUSTD
}
