/*
 * Copyright (c) 2006-2010,2012-2017 Apple Inc. All Rights Reserved.
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
 *
 * SecTrustServer.c - certificate trust evaluation engine
 *
 *
 */

#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/SecCertificateSource.h"
#include "trust/trustd/SecRevocationServer.h"
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/SecPinningDb.h"
#include "trust/trustd/md.h"

#include <utilities/SecIOFormat.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecAppleAnchorPriv.h>

#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecTask.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <AssertMacros.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <sys/codesign.h>
#include <Security/SecBase.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include <CommonCrypto/CommonDigest.h>
#include "OTATrustUtilities.h"
#include "personalization.h"
#include <utilities/SecInternalReleasePriv.h>
#include <mach/mach_time.h>
#include <dispatch/private.h>

#if TARGET_OS_OSX
#include <Security/SecTaskPriv.h>
#endif

#define MAX_CHAIN_LENGTH  15
#define MAX_NUM_CHAINS    100
#define ACCEPT_PATH_SCORE 10000000

/* Forward declaration for use in SecCertificateSource. */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents);

// MARK: -
// MARK: SecPathBuilder
/********************************************************
 *************** SecPathBuilder object ******************
 ********************************************************/
struct SecPathBuilder {
    dispatch_queue_t        queue;
    uint64_t                startTime;
    CFDataRef               clientAuditToken;
    SecCertificateSourceRef certificateSource;
    SecCertificateSourceRef itemCertificateSource;
    SecCertificateSourceRef anchorSource;
    SecCertificateSourceRef appleAnchorSource;
    CFMutableArrayRef       anchorSources;
    CFIndex                 nextParentSource;
    CFMutableArrayRef       parentSources;
    CFArrayRef              ocspResponses;               // Stapled OCSP responses
    CFArrayRef              signedCertificateTimestamps; // Stapled SCTs
    CFDictionaryRef         trustedLogs;                 // Trusted CT logs
    CFAbsoluteTime          verifyTime;
    CFArrayRef              exceptions;

    /* Hashed set of all paths we've constructed so far, used to prevent
       re-considering a path that was already constructed once before.
       Note that this is the only container in which certificatePath
       objects are retained.
       Every certificatePath being considered is always in allPaths and in at
       least one of partialPaths, rejectedPaths, or candidatePath,
       all of which don't retain their values.  */
    CFMutableSetRef         allPaths;

    /* No trusted anchor, satisfies the linking to intermediates for all
       policies (unless considerRejected is true). */
    CFMutableArrayRef       partialPaths;
    /* No trusted anchor, does not satisfy linking to intermediates for all
       policies. */
    CFMutableArrayRef       rejectedPaths;
    /* Trusted anchor, satisfies the policies so far. */
    CFMutableArrayRef       candidatePaths;

    CFIndex                 partialIX;

    bool                    considerRejected;
    bool                    considerPartials;
    bool                    canAccessNetwork;

    SecPVCRef *             pvcs;
    CFIndex                 pvcCount;

    SecCertificatePathVCRef path;
    _Atomic unsigned int    asyncJobCount;
    bool                    online_revocation;
    bool                    trusted_revocation;
    CFStringRef             revocation_check_method;

    SecCertificatePathVCRef bestPath;
    CFMutableDictionaryRef  info;

    CFIndex                 activations;
    bool (*state)(SecPathBuilderRef);
    SecPathBuilderCompleted completed;
    const void *context;
    TrustAnalyticsBuilder * analyticsData;
};

/* State functions.  Return false if a async job was scheduled, return
   true to execute the next state. */
static bool SecPathBuilderProcessLeaf(SecPathBuilderRef builder);
static bool SecPathBuilderGetNext(SecPathBuilderRef builder);
static bool SecPathBuilderValidatePath(SecPathBuilderRef builder);
static bool SecPathBuilderComputeDetails(SecPathBuilderRef builder);

/* Forward declarations. */
static bool SecPathBuilderIsAnchor(SecPathBuilderRef builder,
	SecCertificateRef certificate, SecCertificateSourceRef *foundInSource);

static void SecPathBuilderInit(SecPathBuilderRef builder, dispatch_queue_t builderQueue,
    CFDataRef clientAuditToken, CFArrayRef certificates,
    CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed,
    CFArrayRef policies, CFArrayRef ocspResponses,
    CFArrayRef signedCertificateTimestamps, CFArrayRef trustedLogs,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups, CFArrayRef exceptions,
    SecPathBuilderCompleted completed, const void *context) {
    secdebug("alloc", "builder %p", builder);
    CFAllocatorRef allocator = kCFAllocatorDefault;

    builder->analyticsData = calloc(1, sizeof(TrustAnalyticsBuilder));
    builder->analyticsData->start_time = mach_absolute_time();

    builder->clientAuditToken = (CFDataRef)
        ((clientAuditToken) ? CFRetain(clientAuditToken) : NULL);

    if (!builderQueue) {
        /* make our own queue if caller fails to provide one */
        builder->queue = dispatch_queue_create("com.apple.trustd.evaluation.builder", DISPATCH_QUEUE_SERIAL);
    } else {
        dispatch_retain_safe(builderQueue);
        builder->queue = builderQueue;
    }

    builder->nextParentSource = 1;
#if !TARGET_OS_WATCH
    /* <rdar://32728029> */
    builder->canAccessNetwork = true;
#endif
    atomic_init(&builder->asyncJobCount, 0);

    builder->anchorSources = CFArrayCreateMutable(allocator, 0, NULL);
    builder->parentSources = CFArrayCreateMutable(allocator, 0, NULL);

    builder->allPaths = CFSetCreateMutable(allocator, 0, &kCFTypeSetCallBacks);
    builder->partialPaths = CFArrayCreateMutable(allocator, 0, NULL);   // Does not retain, allPaths retains members. See declaration.
    builder->rejectedPaths = CFArrayCreateMutable(allocator, 0, NULL);  // Does not retain, allPaths retains members. See declaration.
    builder->candidatePaths = CFArrayCreateMutable(allocator, 0, NULL); // Does not retain, allPaths retains members. See declaration.

    /* Init the policy verification context. */
    builder->pvcs = malloc(sizeof(SecPVCRef));
    builder->pvcs[0] = malloc(sizeof(struct OpaqueSecPVC));
    SecPVCInit(builder->pvcs[0], builder, policies);
    builder->pvcCount = 1;
    builder->verifyTime = verifyTime;
    builder->exceptions = CFRetainSafe(exceptions);

	/* Let's create all the certificate sources we might want to use. */
	builder->certificateSource =
		SecMemoryCertificateSourceCreate(certificates);
    if (anchors) {
		builder->anchorSource = SecMemoryCertificateSourceCreate(anchors);
    }

    bool allowNonProduction = false;
    builder->appleAnchorSource = SecMemoryCertificateSourceCreate(SecGetAppleTrustAnchors(allowNonProduction));


    /** Parent Sources
     ** The order here avoids the most expensive methods if the cheaper methods
     ** produce an acceptable chain: client-provided, keychains, network-fetched.
     **/
#if !TARGET_OS_BRIDGE
    CFArrayAppendValue(builder->parentSources, builder->certificateSource);
    builder->itemCertificateSource = SecItemCertificateSourceCreate(accessGroups);
    if (keychainsAllowed) {
        CFArrayAppendValue(builder->parentSources, builder->itemCertificateSource);
 #if TARGET_OS_OSX
        /* On OS X, need additional parent source to search legacy keychain files. */
        if (kSecLegacyCertificateSource->contains && kSecLegacyCertificateSource->copyParents) {
            CFArrayAppendValue(builder->parentSources, kSecLegacyCertificateSource);
        }
 #endif
    }
    if (anchorsOnly) {
        /* Add the Apple, system, and user anchor certificate db to the search list
         if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, builder->appleAnchorSource);
        CFArrayAppendValue(builder->parentSources, kSecSystemAnchorSource);
        CFArrayAppendValue(builder->parentSources, kSecUserAnchorSource);
    }
    if (keychainsAllowed && builder->canAccessNetwork) {
        CFArrayAppendValue(builder->parentSources, kSecCAIssuerSource);
    }
#else /* TARGET_OS_BRIDGE */
    /* Bridge can only access memory sources. */
    CFArrayAppendValue(builder->parentSources, builder->certificateSource);
    if (anchorsOnly) {
        /* Add the Apple, system, and user anchor certificate db to the search list
         if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, builder->appleAnchorSource);
    }
#endif /* !TARGET_OS_BRIDGE */

    /** Anchor Sources
     ** The order here allows a client-provided anchor to overrule
     ** a user or admin trust setting which can overrule the system anchors.
     ** Apple's anchors cannot be overriden by a trust setting.
     **/
#if !TARGET_OS_BRIDGE
    if (builder->anchorSource) {
        CFArrayAppendValue(builder->anchorSources, builder->anchorSource);
    }
    if (!anchorsOnly) {
        /* Only add the system and user anchor certificate db to the
         anchorSources if we are supposed to trust them. */
        CFArrayAppendValue(builder->anchorSources, builder->appleAnchorSource);
        if (keychainsAllowed) {
#if TARGET_OS_OSX
            if (kSecLegacyAnchorSource->contains && kSecLegacyAnchorSource->copyParents) {
                CFArrayAppendValue(builder->anchorSources, kSecLegacyAnchorSource);
            }
#endif
            CFArrayAppendValue(builder->anchorSources, kSecUserAnchorSource);
        }
        CFArrayAppendValue(builder->anchorSources, kSecSystemAnchorSource);
    }
#else /* TARGET_OS_BRIDGE */
    /* Bridge can only access memory sources. */
    if (builder->anchorSource) {
        CFArrayAppendValue(builder->anchorSources, builder->anchorSource);
    }
    if (!anchorsOnly) {
        CFArrayAppendValue(builder->anchorSources, builder->appleAnchorSource);
    }
#endif /* !TARGET_OS_BRIDGE */

    builder->ocspResponses = CFRetainSafe(ocspResponses);
    builder->signedCertificateTimestamps = CFRetainSafe(signedCertificateTimestamps);

    if(trustedLogs) {
        builder->trustedLogs = SecOTAPKICreateTrustedCTLogsDictionaryFromArray(trustedLogs);
    }

    /* Now let's get the leaf cert and turn it into a path. */
    SecCertificateRef leaf =
    (SecCertificateRef)CFArrayGetValueAtIndex(certificates, 0);
    SecCertificatePathVCRef path = SecCertificatePathVCCreate(NULL, leaf, NULL);
    CFSetAddValue(builder->allPaths, path);
    CFArrayAppendValue(builder->partialPaths, path);

    builder->path = CFRetainSafe(path);
    SecPathBuilderSetPath(builder, path);
    CFRelease(path);

    /* Next step is to process the leaf. We do that work on the builder queue
     * to avoid blocking the main thread with database lookups. */
    builder->state = SecPathBuilderProcessLeaf;
    builder->completed = completed;
    builder->context = context;
}

SecPathBuilderRef SecPathBuilderCreate(dispatch_queue_t builderQueue, CFDataRef clientAuditToken,
    CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly,
    bool keychainsAllowed, CFArrayRef policies, CFArrayRef ocspResponses,
    CFArrayRef signedCertificateTimestamps, CFArrayRef trustedLogs,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups, CFArrayRef exceptions,
    SecPathBuilderCompleted completed, const void *context) {
    SecPathBuilderRef builder = malloc(sizeof(*builder));
    memset(builder, 0, sizeof(*builder));
    SecPathBuilderInit(builder, builderQueue, clientAuditToken, certificates,
        anchors, anchorsOnly, keychainsAllowed, policies, ocspResponses,
        signedCertificateTimestamps, trustedLogs, verifyTime,
        accessGroups, exceptions, completed, context);
    return builder;
}

/* Don't use this if you're going to modify the PVC array in the operation. */
static void SecPathBuilderForEachPVC(SecPathBuilderRef builder,void (^operation)(SecPVCRef pvc, bool *stop)) {
    if (!builder->pvcs) { return; }
    bool stop = false;
    CFIndex ix;
    for (ix = 0; ix < builder->pvcCount; ix++) {
        if (!builder->pvcs[ix]) { continue; }
        operation(builder->pvcs[ix], &stop);
        if (stop) { break; }
    }
}

void SecPathBuilderDestroy(SecPathBuilderRef builder) {
    secdebug("alloc", "destroy builder %p", builder);
    dispatch_release_null(builder->queue);
    if (builder->anchorSource) {
        SecMemoryCertificateSourceDestroy(builder->anchorSource);
        builder->anchorSource = NULL;
    }
    if (builder->certificateSource) {
        SecMemoryCertificateSourceDestroy(builder->certificateSource);
        builder->certificateSource = NULL;
    }
    if (builder->itemCertificateSource) {
        SecItemCertificateSourceDestroy(builder->itemCertificateSource);
        builder->itemCertificateSource = NULL;
    }
    if (builder->appleAnchorSource) {
        SecMemoryCertificateSourceDestroy(builder->appleAnchorSource);
        builder->appleAnchorSource = NULL;
    }
	CFReleaseNull(builder->clientAuditToken);
	CFReleaseNull(builder->anchorSources);
	CFReleaseNull(builder->parentSources);
	CFReleaseNull(builder->allPaths);
	CFReleaseNull(builder->partialPaths);
	CFReleaseNull(builder->rejectedPaths);
	CFReleaseNull(builder->candidatePaths);
    CFReleaseNull(builder->ocspResponses);
    CFReleaseNull(builder->signedCertificateTimestamps);
    CFReleaseNull(builder->trustedLogs);
    CFReleaseNull(builder->path);
    CFReleaseNull(builder->revocation_check_method);
    CFReleaseNull(builder->info);
    CFReleaseNull(builder->exceptions);

    free(builder->analyticsData);
    builder->analyticsData = NULL;

    if (builder->pvcs) {
        CFIndex ix;
        for (ix = 0; ix < builder->pvcCount; ix++) {
            if (builder->pvcs[ix]) {
                SecPVCDelete(builder->pvcs[ix]);
                free(builder->pvcs[ix]);
            }
        }
        free(builder->pvcs);
        builder->pvcs = NULL;
    }
}

void SecPathBuilderSetPath(SecPathBuilderRef builder, SecCertificatePathVCRef path) {
    bool samePath = ((!path && !builder->path) || (path && builder->path && CFEqual(path, builder->path)));
    if (!samePath) {
        CFRetainAssign(builder->path, path);
    }
    CFReleaseNull(builder->info);

    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCSetPath(pvc, path);
    });
}


bool SecPathBuilderCanAccessNetwork(SecPathBuilderRef builder) {
    return builder->canAccessNetwork;
}

void SecPathBuilderSetCanAccessNetwork(SecPathBuilderRef builder, bool allow) {
    if (builder->canAccessNetwork != allow) {
        builder->canAccessNetwork = allow;
        if (allow) {
#if !TARGET_OS_WATCH
            secinfo("http", "network access re-enabled by policy");
            /* re-enabling network_access re-adds kSecCAIssuerSource as
               a parent source. */
            CFArrayAppendValue(builder->parentSources, kSecCAIssuerSource);
#else
            /* <rdar://32728029> */
            secnotice("http", "network access not allowed on WatchOS");
            builder->canAccessNetwork = false;
#endif
        } else {
            secinfo("http", "network access disabled by policy");
            /* disabling network_access removes kSecCAIssuerSource from
               the list of parent sources. */
            CFIndex ix = CFArrayGetFirstIndexOfValue(builder->parentSources,
                CFRangeMake(0, CFArrayGetCount(builder->parentSources)),
                kSecCAIssuerSource);
            if (ix >= 0)
                CFArrayRemoveValueAtIndex(builder->parentSources, ix);
        }
    }
}

CFArrayRef SecPathBuilderCopyOCSPResponses(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->ocspResponses);
}

CFArrayRef SecPathBuilderCopySignedCertificateTimestamps(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->signedCertificateTimestamps);
}

CFDictionaryRef SecPathBuilderCopyTrustedLogs(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->trustedLogs);
}

SecCertificateSourceRef SecPathBuilderGetAppAnchorSource(SecPathBuilderRef builder)
{
    return builder->anchorSource;
}

CFSetRef SecPathBuilderGetAllPaths(SecPathBuilderRef builder)
{
    return builder->allPaths;
}

TrustAnalyticsBuilder *SecPathBuilderGetAnalyticsData(SecPathBuilderRef builder)
{
    if (!builder) {
        return NULL;
    }
    return builder->analyticsData;
}

SecCertificatePathVCRef SecPathBuilderGetBestPath(SecPathBuilderRef builder)
{
    return builder->bestPath;
}

SecCertificatePathVCRef SecPathBuilderGetPath(SecPathBuilderRef builder) {
    return builder->path;
}

CFAbsoluteTime SecPathBuilderGetVerifyTime(SecPathBuilderRef builder) {
    return builder->verifyTime;
}

bool SecPathBuilderHasTemporalParentChecks(SecPathBuilderRef builder) {
    __block bool validIntermediates = false;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool *stop) {
        CFArrayForEach(pvc->policies, ^(const void *value) {
            SecPolicyRef policy = (SecPolicyRef)value;
            if (CFDictionaryContainsKey(policy->_options, kSecPolicyCheckTemporalValidity)) {
                validIntermediates = true;
                *stop = true;
            }
        });
    });
    return validIntermediates;
}

CFIndex SecPathBuilderGetCertificateCount(SecPathBuilderRef builder) {
    return SecCertificatePathVCGetCount(builder->path);
}

SecCertificateRef SecPathBuilderGetCertificateAtIndex(SecPathBuilderRef builder, CFIndex ix) {
    return SecCertificatePathVCGetCertificateAtIndex(builder->path, ix);
}

bool SecPathBuilderIsAnchored(SecPathBuilderRef builder) {
    return SecCertificatePathVCIsAnchored(builder->path);
}

unsigned int SecPathBuilderDecrementAsyncJobCount(SecPathBuilderRef builder) {
    unsigned int result = atomic_fetch_sub(&builder->asyncJobCount, 1);
    secdebug("rvc", "%p: decrement asyncJobCount from %d", builder, result);
    /* atomic_fetch_sub returns the original value, but we want this function to return the
     * value after the operation. */
    return --result;
}

void SecPathBuilderSetAsyncJobCount(SecPathBuilderRef builder, unsigned int jobCount) {
    atomic_store(&builder->asyncJobCount, jobCount);
    secdebug("rvc", "%p: set asyncJobCount to %d", builder, jobCount);
}

unsigned int SecPathBuilderGetAsyncJobCount(SecPathBuilderRef builder) {
    unsigned int count = atomic_load(&builder->asyncJobCount);
    secdebug("rvc", "%p: current asyncJobCount is %d", builder, count);
    return count;
}

CFMutableDictionaryRef SecPathBuilderGetInfo(SecPathBuilderRef builder) {
    return builder->info;
}

CFStringRef SecPathBuilderGetRevocationMethod(SecPathBuilderRef builder) {
    return builder->revocation_check_method;
}

void SecPathBuilderSetRevocationMethod(SecPathBuilderRef builder, CFStringRef method) {
    CFRetainAssign(builder->revocation_check_method, method);
    secdebug("rvc", "deferred revocation checking enabled using %@ method", method);
}

bool SecPathBuilderGetCheckRevocationOnline(SecPathBuilderRef builder) {
    return builder->online_revocation;
}

void SecPathBuilderSetCheckRevocationOnline(SecPathBuilderRef builder) {
    builder->online_revocation = true;
    secdebug("rvc", "revocation force online check");
}

bool SecPathBuilderGetCheckRevocationIfTrusted(SecPathBuilderRef builder) {
    return builder->trusted_revocation;
}

void SecPathBuilderSetCheckRevocationIfTrusted(SecPathBuilderRef builder) {
    builder->trusted_revocation = true;
    secdebug("rvc", "revocation check only if trusted");
}

CFArrayRef SecPathBuilderGetExceptions(SecPathBuilderRef builder) {
    return builder->exceptions;
}

CFIndex SecPathBuilderGetPVCCount(SecPathBuilderRef builder) {
    return builder->pvcCount;
}

SecPVCRef SecPathBuilderGetPVCAtIndex(SecPathBuilderRef builder, CFIndex ix) {
    if (ix > (builder->pvcCount - 1)) {
        return NULL;
    }
    return builder->pvcs[ix];
}

void SecPathBuilderSetResultInPVCs(SecPathBuilderRef builder, CFStringRef key,
                                   CFIndex ix, CFTypeRef result, bool force) {
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCSetResultForced(pvc, key, ix, result, force);
    });
}

static bool SecPathBuilderIsOkResult(SecPathBuilderRef builder) {
    /* If any of the PVCs passed, we accept the path. */
    __block bool acceptPath = false;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        acceptPath |= SecPVCIsOkResult(pvc);
    });
    return acceptPath;
}

SecPVCRef SecPathBuilderGetResultPVC(SecPathBuilderRef builder) {
    /* Return the first PVC that passed */
    __block SecPVCRef resultPVC = NULL;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool *stop) {
        if (SecPVCIsOkResult(pvc)) {
            resultPVC = pvc;
            *stop = true;
        }
    });
    if (resultPVC) { return resultPVC; }

    /* If we didn't return a passing PVC, return the first PVC. */
    return builder->pvcs[0];
}

/* This function assumes that the input source is an anchor source */
static bool SecPathBuilderIsAnchorPerConstraints(SecPathBuilderRef builder, SecCertificateSourceRef source,
    SecCertificateRef certificate) {

    /* Get the trust settings result for the PVCs. Only one PVC need match to
     * trigger the anchor behavior -- policy validation will handle whether the
     * path is truly anchored for that PVC. */
    __block bool result = false;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        if (SecPVCIsAnchorPerConstraints(pvc, source, certificate)) {
            result = true;
            *stop = true;
        }
    });

    return result;
}

/* Source returned in foundInSource has the same lifetime as the builder. */
static bool SecPathBuilderIsAnchor(SecPathBuilderRef builder,
    SecCertificateRef certificate, SecCertificateSourceRef *foundInSource) {
    /* We look through the anchor sources in order. They are ordered in
       SecPathBuilderInit so that process anchors override user anchors which
       override system anchors. */
    CFIndex count = CFArrayGetCount(builder->anchorSources);
    CFIndex ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateSourceRef source = (SecCertificateSourceRef)
        CFArrayGetValueAtIndex(builder->anchorSources, ix);
        if (SecCertificateSourceContains(source, certificate)) {
            if (foundInSource)
                *foundInSource = source;
            if (SecPathBuilderIsAnchorPerConstraints(builder, source, certificate)) {
                return true;
            }
        }
    }
    return false;
}

bool SecPathBuilderIsAnchorSource(SecPathBuilderRef builder, SecCertificateSourceRef source) {
    CFIndex anchorCount = CFArrayGetCount(builder->anchorSources);
    return CFArrayContainsValue(builder->anchorSources, CFRangeMake(0,anchorCount), source);
}

/* Return false if path is not a partial, if path was a valid candidate it
   will have been added to builder->candidatePaths, if path was rejected
   by the parent certificate checks (because it's expired or some other
   static chaining check failed) it will have been added to rejectedPaths.
   Return true path if path is a partial. */
static bool SecPathBuilderIsPartial(SecPathBuilderRef builder,
	SecCertificatePathVCRef path) {
    SecPathBuilderSetPath(builder, path);
    __block bool parentChecksFail = true;

    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        /* The parent checks aren't actually PVC-dependent, so theoretically,
         * we only need to run this once per path, but we want to set the
         * results in all PVCs. */
        parentChecksFail &= !SecPVCParentCertificateChecks(pvc,
                                                           SecCertificatePathVCGetCount(path) - 1);
    });

    if (!builder->considerRejected && parentChecksFail) {
        secdebug("trust", "Found rejected path %@", path);
		CFArrayAppendValue(builder->rejectedPaths, path);
		return false;
	}

	SecPathVerifyStatus vstatus = SecCertificatePathVCVerify(path);
	/* Candidate paths with failed signatures are discarded. */
	if (vstatus == kSecPathVerifyFailed) {
        secdebug("trust", "Verify failed for path %@", path);
		return false;
	}

	if (vstatus == kSecPathVerifySuccess) {
		/* The signature chain verified successfully, now let's find
		   out if we have an anchor for path.  */
		if (SecCertificatePathVCIsAnchored(path)) {
            secdebug("trust", "Adding candidate %@", path);
			CFArrayAppendValue(builder->candidatePaths, path);
		}
        /* The path is not partial if the last cert is self-signed.
         * The path is also not partial if the issuer of the last cert was the subject
         * of a previous cert in the chain, indicating a cycle in the graph. See <rdar://33136765>. */
        if (((SecCertificatePathVCSelfSignedIndex(path) >= 0) &&
            (SecCertificatePathVCSelfSignedIndex(path) == SecCertificatePathVCGetCount(path)-1)) ||
            SecCertificatePathVCIsCycleInGraph(path)) {
            if (!builder->considerRejected) {
                secdebug("trust", "Adding non-partial non-anchored reject %@", path);
                CFArrayAppendValue(builder->rejectedPaths, path);
            } else {
                /* This path was previously rejected as unanchored non-partial, but now that
                 * we're considering rejected paths, this is a candidate. */
                secdebug("trust", "Adding non-partial non-anchored candidate %@", path);
                CFArrayAppendValue(builder->candidatePaths, path);
            }
            return false;
        }
	}

	return true;
}

static void addOptionsToPolicy(SecPolicyRef policy, CFDictionaryRef newOptions) {
    __block CFMutableDictionaryRef oldOptions = CFDictionaryCreateMutableCopy(NULL, 0, policy->_options);
    CFDictionaryForEach(newOptions, ^(const void *key, const void *value) {
        CFDictionaryAddValue(oldOptions, key, value);
    });
    CFAssignRetained(policy->_options, oldOptions);
}

static CF_RETURNS_RETAINED CFArrayRef addTransparentConnectionsRules(CFArrayRef rules, CFNumberRef transparentConnection) {
    /* Not a Transparent Connection, no change */
    int transparentConnectionValue = 0;
    if (!transparentConnection || !CFNumberGetValue(transparentConnection, kCFNumberIntType, &transparentConnectionValue) || transparentConnectionValue != 1) {
        return CFRetainSafe(rules);
    }
    /* Transparent Connections not configured on this device, no change */
    CFArrayRef pins = _SecTrustStoreCopyTransparentConnectionPins(NULL, NULL);
    if (!pins || CFArrayGetCount(pins) == 0) {
        CFReleaseNull(pins);
        return CFRetainSafe(rules);
    }

    /* Create the transparent connection rules */
    CFMutableDictionaryRef transparentConnectionOptions = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef caSPKIsha256s = CFArrayCreateMutable(NULL, CFArrayGetCount(pins), &kCFTypeArrayCallBacks);
    CFArrayForEach(pins, ^(const void *value) {
        CFDictionaryRef pin = (CFDictionaryRef)value;
        CFStringRef hashAlgorithm = CFDictionaryGetValue(pin, kSecTrustStoreHashAlgorithmKey);
        if (!hashAlgorithm || !isString(hashAlgorithm) ||
            CFStringCompare(CFSTR("sha256"), hashAlgorithm, 0) != kCFCompareEqualTo) {
            return;
        }
        CFDataRef hash = CFDictionaryGetValue(pin, kSecTrustStoreSPKIHashKey);
        if (!hash || !isData(hash)) {
            return;
        }
        CFArrayAppendValue(caSPKIsha256s, hash);
    });
    if (CFArrayGetCount(caSPKIsha256s) == 0) {
        CFReleaseNull(caSPKIsha256s);
        CFReleaseNull(transparentConnectionOptions);
        CFReleaseNull(pins);
        return CFRetainSafe(rules);
    }
    secnotice("SecPinningDb", "Adding %lu CA pins for Transparent Connection", CFArrayGetCount(caSPKIsha256s));
    CFDictionaryAddValue(transparentConnectionOptions, kSecPolicyCheckCAspkiSHA256, caSPKIsha256s);
    CFReleaseNull(caSPKIsha256s);

    /* Add the transparent connection rules as another option in the rules */
    CFMutableArrayRef newRules = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(rules) + 1, rules);
    CFArrayAppendValue(newRules, transparentConnectionOptions);
    CFReleaseNull(transparentConnectionOptions);
    CFReleaseNull(pins);
    return newRules;
}

static void SecPathBuilderAddPinningPolicies(SecPathBuilderRef builder) {
    CFIndex ix, initialPVCCount = builder->pvcCount;
    for (ix = 0; ix < initialPVCCount; ix++) {
        CFArrayRef policies = CFRetainSafe(builder->pvcs[ix]->policies);
        CFIndex policyIX;
        for (policyIX = 0; policyIX < CFArrayGetCount(policies); policyIX++) {
            SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
            CFStringRef policyName = SecPolicyGetName(policy);
            CFStringRef hostname = CFDictionaryGetValue(policy->_options, kSecPolicyCheckSSLHostname);
            if (!hostname) { continue; } //No hostname to look up; probably not an SSL policy, skip

            /* Query the pinning database for this policy */
            CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionaryAddValue(query, kSecPinningDbKeyPolicyName, policyName);
            CFDictionaryAddValue(query, kSecPinningDbKeyHostname, hostname);
            CFDictionaryRef results = SecPinningDbCopyMatching(query);
            CFReleaseNull(query);
            if (!results) { continue; } // No rules for this hostname or policyName

            /* Found pinning policies. Apply them to the path builder. */
            CFArrayRef newRules = CFDictionaryGetValue(results, kSecPinningDbKeyRules);
            CFStringRef dbPolicyName = CFDictionaryGetValue(results, kSecPinningDbKeyPolicyName);
            CFNumberRef transparentConnection = CFDictionaryGetValue(results, kSecPinningDbKeyTransparentConnection);
            secinfo("SecPinningDb", "found pinning %lu %@ policies for hostname %@, policyName %@",
                    (unsigned long)CFArrayGetCount(newRules), dbPolicyName, hostname, policyName);
            /* If, applicable, add transparent connection rules to the new rules */
            newRules = addTransparentConnectionsRules(newRules, transparentConnection);
            CFIndex newRulesIX;
            for (newRulesIX = 0; newRulesIX < CFArrayGetCount(newRules); newRulesIX++) {
                if (!isDictionary(CFArrayGetValueAtIndex(newRules, newRulesIX))) {
                    continue;
                }

                /* Create the new policies with pinning rules (preserving other ANDed policies). */
                CFDictionaryRef newOptions = (CFDictionaryRef)CFArrayGetValueAtIndex(newRules, newRulesIX);
                SecPolicyRef newPolicy = SecPolicyCreateSSL(true, hostname);
                if (!newPolicy) { continue; }
                addOptionsToPolicy(newPolicy, newOptions);
                SecPolicySetName(newPolicy, dbPolicyName);
                CFMutableArrayRef newPolicies = CFArrayCreateMutableCopy(NULL, 0, policies);
                if (!newPolicies) { CFReleaseNull(newPolicy); continue; }
                CFArrayReplaceValues(newPolicies, CFRangeMake(policyIX, 1), (const void **)&newPolicy, 1);

                if (newRulesIX == 0) {
                    /* For the first set of pinning rules, replace this PVC's policies */
                    CFRetainAssign(builder->pvcs[ix]->policies, newPolicies);
                } else {
                    /* If there were two or more dictionaries of rules, we need to treat them as an "OR".
                     * Create another PVC for this dicitionary. */
                    builder->pvcs = realloc(builder->pvcs, (builder->pvcCount + 1) * sizeof(SecPVCRef));
                    builder->pvcs[builder->pvcCount] = malloc(sizeof(struct OpaqueSecPVC));
                    SecPVCInit(builder->pvcs[builder->pvcCount], builder, newPolicies);
                    builder->pvcCount++;
                }
                CFReleaseNull(newPolicy);
                CFReleaseNull(newPolicies);
            }
            CFReleaseNull(results);
            CFReleaseNull(newRules);
        }
        CFReleaseNull(policies);
    }
}

static bool SecPathBuilderProcessLeaf(SecPathBuilderRef builder) {
    SecPathBuilderAddPinningPolicies(builder);

    /* We need to find and set constraints on the leaf-only path */
    SecCertificatePathVCRef path = builder->path;
    SecCertificateRef leaf = SecCertificatePathVCGetCertificateAtIndex(path, 0);

    SecCertificateSourceRef source = NULL;
    bool isAnchor = false;
    CFArrayRef constraints = NULL;
    if (SecPathBuilderIsAnchor(builder, leaf, &source)) {
        isAnchor = true;
    }
    if (source) {
        constraints = SecCertificateSourceCopyUsageConstraints(source, leaf);
    }
    SecCertificatePathVCSetUsageConstraintsAtIndex(path, constraints, 0);
    CFReleaseSafe(constraints);
    if (isAnchor) {
        SecCertificatePathVCSetIsAnchored(path);
        CFArrayAppendValue(builder->candidatePaths, path);
    }

    __block bool leafChecksFail = true;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCLeafChecks(pvc);
        leafChecksFail &= !SecPVCIsOkResult(pvc);
    });
    builder->considerRejected = leafChecksFail;

    builder->state = SecPathBuilderGetNext;
    return true;
}

/* Given the builder, a partial chain partial and the parents array, construct
   a SecCertificatePath for each parent.  After discarding previously
   considered paths and paths with cycles, sort out which array each path
   should go in, if any. */
static void SecPathBuilderProcessParents(SecPathBuilderRef builder,
    SecCertificatePathVCRef partial, CFArrayRef parents) {
    CFIndex rootIX = SecCertificatePathVCGetCount(partial) - 1;
    CFIndex num_parents = parents ? CFArrayGetCount(parents) : 0;
    CFIndex parentIX;
    for (parentIX = 0; parentIX < num_parents; ++parentIX) {
        SecCertificateRef parent = (SecCertificateRef)
            CFArrayGetValueAtIndex(parents, parentIX);
        CFIndex ixOfParent = SecCertificatePathVCGetIndexOfCertificate(partial,
            parent);
        if (ixOfParent != kCFNotFound) {
            /* partial already contains parent.  Let's not add the same
               certificate again. */
            if (ixOfParent == rootIX) {
                /* parent is equal to the root of the partial, so partial
                   looks to be self issued. */
                SecCertificatePathVCSetSelfIssued(partial);
            }
            continue;
        }

        /* FIXME Add more sanity checks to see that parent really can be
           a parent of partial_root.  subjectKeyID == authorityKeyID,
           signature algorithm matches public key algorithm, etc. */
        SecCertificateSourceRef source = NULL;
        bool is_anchor = SecPathBuilderIsAnchor(builder, parent, &source);
        CFArrayRef constraints = (source) ? SecCertificateSourceCopyUsageConstraints(source, parent) : NULL;
        SecCertificatePathVCRef path = SecCertificatePathVCCreate(partial, parent, constraints);
        CFReleaseSafe(constraints);
        if (!path)
            continue;
        if (!CFSetContainsValue(builder->allPaths, path)) {
            CFSetAddValue(builder->allPaths, path);
            if (is_anchor)
                SecCertificatePathVCSetIsAnchored(path);
            if (SecPathBuilderIsPartial(builder, path)) {
                /* Insert path right at the current position since it's a new
                   candiate partial. */
                CFArrayInsertValueAtIndex(builder->partialPaths,
                    ++builder->partialIX, path);
                secdebug("trust", "Adding partial for parent %" PRIdCFIndex "/%" PRIdCFIndex " %@",
                    parentIX + 1, num_parents, path);
            }
            secdebug("trust", "found new path %@", path);
        }
        CFRelease(path);
    }
}

/* Callback for the SecPathBuilderGetNext() functions call to
   SecCertificateSourceCopyParents(). */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents) {
    SecPathBuilderRef builder = (SecPathBuilderRef)context;
    SecCertificatePathVCRef partial = (SecCertificatePathVCRef)
        CFArrayGetValueAtIndex(builder->partialPaths, builder->partialIX);
    secdebug("async", "%@ parents %@", partial, parents);
    SecPathBuilderProcessParents(builder, partial, parents);

    builder->state = SecPathBuilderGetNext;
    SecPathBuilderStep(builder);
}

static bool SecPathBuilderGetNext(SecPathBuilderRef builder) {
    /* If we have any candidates left to go return those first. */
    if (CFArrayGetCount(builder->candidatePaths)) {
        SecCertificatePathVCRef path = (SecCertificatePathVCRef)
            CFArrayGetValueAtIndex(builder->candidatePaths, 0);
        CFArrayRemoveValueAtIndex(builder->candidatePaths, 0);
        secdebug("trust", "SecPathBuilderGetNext returning candidate %@",
            path);
        SecPathBuilderSetPath(builder, path);
        builder->state = SecPathBuilderValidatePath;
        return true;
    }

    /* If we are considering rejected chains we check each rejected path
       with SecPathBuilderIsPartial() which checks the signature chain and
       either drops the path if it's not properly signed, add it as a
       candidate if it has a trusted anchor, or adds it as a partial
       to be considered once we finish considering all the rejects. */
    if (builder->considerRejected) {
        CFIndex rejectedIX = CFArrayGetCount(builder->rejectedPaths);
        if (rejectedIX) {
            rejectedIX--;
            SecCertificatePathVCRef path = (SecCertificatePathVCRef)
                CFArrayGetValueAtIndex(builder->rejectedPaths, rejectedIX);
            if (SecPathBuilderIsPartial(builder, path)) {
                CFArrayInsertValueAtIndex(builder->partialPaths,
                    ++builder->partialIX, path);
            }
            CFArrayRemoveValueAtIndex(builder->rejectedPaths, rejectedIX);

            /* Keep going until we have moved all rejected partials into
               the regular partials or candidates array. */
            return true;
        }
    }

    /* If builder->partialIX is < 0 we have considered all partial chains
       this block must ensure partialIX >= 0 if execution continues past
       it's end. */
    if (builder->partialIX < 0) {
        CFIndex num_sources = CFArrayGetCount(builder->parentSources);
        if (builder->nextParentSource < num_sources) {
            builder->nextParentSource++;
            secdebug("trust", "broading search to %" PRIdCFIndex "/%" PRIdCFIndex " sources",
                builder->nextParentSource, num_sources);
        } else {
            /* We've run out of new sources to consider so let's look at
               rejected chains and after that even consider partials
               directly.
               FIXME we might not want to consider partial paths that
               are subsets of other partial paths, or not consider them
               at all if we already have an (unpreferred) accept or anchored reject */
            if (!builder->considerRejected) {
                builder->considerRejected = true;
                secdebug("trust", "considering rejected paths");
            } else if (!builder->considerPartials) {
                builder->considerPartials = true;
                secdebug("trust", "considering partials");
            } else {
                /* We're all out of options, so we can't produce any more
                   candidates.  Let's calculate details and return the best
                   path we found. */
                builder->state = SecPathBuilderComputeDetails;
                return true;
            }
        }
        builder->partialIX = CFArrayGetCount(builder->partialPaths) - 1;
        secdebug("trust", "re-checking %" PRIdCFIndex " partials", builder->partialIX + 1);
        return true;
    }

    /* We know builder->partialIX >= 0 if we get here.  */
    SecCertificatePathVCRef partial = (SecCertificatePathVCRef)
        CFArrayGetValueAtIndex(builder->partialPaths, builder->partialIX);
    /* Don't try to extend partials anymore once we are in the considerPartials
       state, since at this point every partial has been extended with every
       possible parentSource already. */
    if (builder->considerPartials) {
        --builder->partialIX;
        SecPathBuilderSetPath(builder, partial);
        builder->state = SecPathBuilderValidatePath;
        return true;
    }

    /* Don't try to extend partials anymore if we already have too many chains. */
    if (CFSetGetCount(builder->allPaths) > MAX_NUM_CHAINS) {
        secnotice("trust", "not building any more paths, already have %" PRIdCFIndex,
                  CFSetGetCount(builder->allPaths));
        builder->partialIX = -1;
        return true;
    }

    /* Attempt to extend this partial path with another certificate. This
       should give us a list of potential parents to consider. */
    secdebug("trust", "looking for parents of partial %" PRIdCFIndex "/%" PRIdCFIndex ": %@",
        builder->partialIX + 1, CFArrayGetCount(builder->partialPaths),
        partial);

    /* Attempt to extend partial, leaving all possible extended versions
       of partial in builder->extendedPaths. */
    CFIndex sourceIX = SecCertificatePathVCGetNextSourceIndex(partial);
    CFIndex num_anchor_sources = CFArrayGetCount(builder->anchorSources);
    if (sourceIX < num_anchor_sources + builder->nextParentSource) {
        SecCertificateSourceRef source;
        if (sourceIX < num_anchor_sources) {
            source = (SecCertificateSourceRef)
                CFArrayGetValueAtIndex(builder->anchorSources, sourceIX);
            secdebug("trust", "searching anchor source %" PRIdCFIndex "/%" PRIdCFIndex, sourceIX + 1,
                     num_anchor_sources);
        } else {
            CFIndex parentIX = sourceIX - num_anchor_sources;
            source = (SecCertificateSourceRef)
                CFArrayGetValueAtIndex(builder->parentSources, parentIX);
            secdebug("trust", "searching parent source %" PRIdCFIndex "/%" PRIdCFIndex, parentIX + 1,
                     builder->nextParentSource);
        }
        SecCertificatePathVCSetNextSourceIndex(partial, sourceIX + 1);
        SecCertificateRef root = SecCertificatePathVCGetRoot(partial);
        return SecCertificateSourceCopyParents(source, root,
            builder, SecPathBuilderExtendPaths);
    } else {
        --builder->partialIX;
    }

    return true;
}

/* One or more of the policies did not accept the candidate path. */
static void SecPathBuilderReject(SecPathBuilderRef builder) {
    check(builder);

    builder->state = SecPathBuilderGetNext;

    bool bestPathIsEV = SecCertificatePathVCIsEV(builder->bestPath);
    bool isEV = SecCertificatePathVCIsEV(builder->path);

    if (bestPathIsEV && !isEV) {
        /* We never replace an ev reject with a non ev reject. */
        return;
    }

    CFIndex bestPathScore = SecCertificatePathVCGetScore(builder->bestPath);
    CFIndex score = SecCertificatePathVCScore(builder->path, builder->verifyTime);
    SecCertificatePathVCSetScore(builder->path, score);

    /* The current chain is valid for EV, but revocation checking failed.  We
       replace any previously accepted or rejected non EV chains with the
       current one. */
    if (isEV && !bestPathIsEV) {
        bestPathScore = 0;
    }
    if (!builder->bestPath || score > bestPathScore) {
        if (builder->bestPath) {
            secinfo("reject",
                "replacing %sev %s score: %ld with %sev score: %" PRIdCFIndex " %@",
                (bestPathIsEV ? "" : "non "),
                (bestPathScore > ACCEPT_PATH_SCORE ? "accept" : "reject"),
                bestPathScore,
                (isEV ? "" : "non "), (long)score, builder->path);
        } else {
            secinfo("reject", "%sev score: %" PRIdCFIndex " %@",
                (isEV ? "" : "non "), score, builder->path);
        }

        builder->bestPath = builder->path;
    } else {
        secinfo("reject", "%sev score: %" PRIdCFIndex " lower than %" PRIdCFIndex " %@",
            (isEV ? "" : "non "), score, bestPathScore, builder->path);
    }
}

/* All policies accepted the candidate path. */
static void SecPathBuilderAccept(SecPathBuilderRef builder) {
    if (!builder) { return; }
    bool isSHA2 = !SecCertificatePathVCHasWeakHash(builder->path);
    bool isOptionallySHA2 = !SecCertificateIsWeakHash(SecPathBuilderGetCertificateAtIndex(builder, 0));
    bool isEV = SecCertificatePathVCIsEV(builder->path);
    bool isOptionallyEV = SecCertificatePathVCIsOptionallyEV(builder->path);
    CFIndex bestScore = SecCertificatePathVCGetScore(builder->bestPath);
    /* Score this path. Note that all points awarded or deducted in
     * SecCertificatePathScore are < 100,000 */
    CFIndex currScore = (SecCertificatePathVCScore(builder->path, builder->verifyTime) +
                         ACCEPT_PATH_SCORE  + // 10,000,000 points for accepting
                         (isEV ? 1000000 : 0)); //1,000,000 points for EV
    SecCertificatePathVCSetScore(builder->path, currScore);
    if (currScore > bestScore) {
        // current path is better than existing best path
        secinfo("accept", "replacing %sev %s score: %ld with %sev score: %" PRIdCFIndex " %@",
                 (SecCertificatePathVCIsEV(builder->bestPath) ? "" : "non "),
                 (bestScore > ACCEPT_PATH_SCORE ? "accept" : "reject"),
                 bestScore,
                 (isEV ? "" : "non "), (long)currScore, builder->path);

        builder->bestPath = builder->path;
    }

    /* If we found the best accept we can, we want to switch directly to the
       SecPathBuilderComputeDetails state here, since we're done. */
    if ((isEV || !isOptionallyEV) && (isSHA2 || !isOptionallySHA2))
        builder->state = SecPathBuilderComputeDetails;
    else
        builder->state = SecPathBuilderGetNext;
}

/* Return true iff a given path satisfies all the specified policies at
   verifyTime. */
static bool SecPathBuilderValidatePath(SecPathBuilderRef builder) {

    if (builder->considerRejected) {
        SecPathBuilderReject(builder);
        return true;
    }

    builder->state = SecPathBuilderDidValidatePath;

    /* Revocation checking is now done before path checks, to ensure that
       we have OCSP responses for CT checking and that isAllowlisted is
       appropriately set for other checks. */
    bool completed = SecPathBuilderCheckRevocation(builder);

    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCPathChecks(pvc);
    });

    return completed;
}

bool SecPathBuilderDidValidatePath(SecPathBuilderRef builder) {
    /* We perform the revocation required policy checks here because
     * this is the state we call back into once all the asynchronous
     * revocation check calls are done. */
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCPathCheckRevocationResponsesReceived(pvc);
    });

    if (SecPathBuilderIsOkResult(builder)) {
        SecPathBuilderAccept(builder);
    } else {
        SecPathBuilderReject(builder);
    }
    assert(builder->state != SecPathBuilderDidValidatePath);
    return true;
}

static bool SecPathBuilderComputeDetails(SecPathBuilderRef builder) {
    /* We have to re-do all the checks so that the results get set in the
     * PVC for the best path, as the last path checked may not have been the best. */
    SecPathBuilderSetPath(builder, builder->bestPath);
    __block CFIndex ix, pathLength = SecCertificatePathVCGetCount(builder->bestPath);

    __block bool completed = true;
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCComputeDetails(pvc, builder->bestPath);
        completed &= SecPathBuilderCheckRevocation(builder);
        for (ix = 1; ix < pathLength; ++ix) {
            SecPVCParentCertificateChecks(pvc, ix);
        }
        SecPVCPathChecks(pvc);
    });

    builder->state = SecPathBuilderReportResult;

    /* Check revocation responses. */
    SecPathBuilderForEachPVC(builder, ^(SecPVCRef pvc, bool * __unused stop) {
        SecPVCPathCheckRevocationResponsesReceived(pvc);
    });

    /* Reject the certificate if it was accepted before but we failed it now. (Should not happen anymore.) */
    if (SecCertificatePathVCGetScore(builder->bestPath) > ACCEPT_PATH_SCORE && !SecPathBuilderIsOkResult(builder)) {
        SecCertificatePathVCResetScore(builder->bestPath);
        secwarning("In ComputeDetails, we got a reject after an accept in DidValidatePath.");
    }

    return completed;
}

static CFAbsoluteTime SecPathBuilderCalculateTrustNotBefore(SecPathBuilderRef builder) {
    /* TrustResult Not Before is the maxmimum of the cert chain NotBefores, ocsp response thisUpdates, and
     * the current time minus the leeway*/
    CFAbsoluteTime latestNotBefore = SecCertificatePathVCGetMaximumNotBefore(builder->bestPath);
    if (SecCertificatePathVCIsRevocationDone(builder->bestPath)) {
        CFAbsoluteTime thisUpdate = SecCertificatePathVCGetLatestThisUpdate(builder->bestPath);
        if (thisUpdate > latestNotBefore) {
            latestNotBefore = thisUpdate;
        }
    }

    CFAbsoluteTime before_leeway = CFAbsoluteTimeGetCurrent() - TRUST_TIME_LEEWAY;
    if (before_leeway > latestNotBefore) {
        latestNotBefore = before_leeway;
    }

    return latestNotBefore;
}

static CFAbsoluteTime SecPathBuilderCalculateTrustNotAfter(SecPathBuilderRef builder) {
    /* TrustResult Not After is the minimum of the cert chain NotAfters, ocsp response nextUpdates, and
     * the current time plus the leeway*/
    CFAbsoluteTime earliestNotAfter = SecCertificatePathVCGetMinimumNotAfter(builder->bestPath);
    if (SecCertificatePathVCIsRevocationDone(builder->bestPath)) {
        CFAbsoluteTime nextUpdate = SecCertificatePathVCGetEarliestNextUpdate(builder->bestPath);
        if (nextUpdate != 0 && nextUpdate < earliestNotAfter) { // 0.0 is a (really dumb) sentinel for "not checked"
            earliestNotAfter = nextUpdate;
        }
    }

    CFAbsoluteTime after_leeway = CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY;
    if (after_leeway < earliestNotAfter) {
        earliestNotAfter = after_leeway;
    }
    return earliestNotAfter;
}

static void SecPathBuilderReportTrustValidityPeriod(SecPathBuilderRef builder) {
    if (!builder->info) {
        return;
    }

    CFAbsoluteTime resultNotBefore = SecPathBuilderCalculateTrustNotBefore(builder);
    CFAbsoluteTime resultNotAfter = SecPathBuilderCalculateTrustNotAfter(builder);
    /* Special cases where validity window is non-existent */
    if (resultNotBefore > resultNotAfter) {
        /* "now" can be before notAfter, between notAfter and notBefore, or after notBefore. We want to adjust
         * the result validity period to ensure we have some valid period, ensuring we capture when the trust
         * result could change. */
        if (CFAbsoluteTimeGetCurrent() < resultNotAfter) {
            /* "now" < notAfter < notBefore:
             *      - caused by a cert with a future validity
             *      - keep the notAfter date and move the notBefore to "now" */
            resultNotBefore = CFAbsoluteTimeGetCurrent();
        } else if (CFAbsoluteTimeGetCurrent() < resultNotBefore) {
            /* notAfter <= "now" < notBefore:
             *      - caused by a cert with a future validity and either an expired (revoked) OCSP response or cert
             *      - swap the dates -- the result won't change between these two points */
            CFAbsoluteTime temp = resultNotBefore;
            resultNotBefore = resultNotAfter;
            resultNotAfter = temp;
        } else {
            /* notAfter < notBefore <= "now":
             *      - caused by expired certs (as notBefore will have been adjusted by the leeway)
             *      - keep the notBefore and move notAfter to "now + leeway" */
            resultNotAfter = CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY;
        }
    }

    /* Special case where the entire window is in the past. In order to avoid triggering
     * trust evaluations with every trust object access, we're setting the validity period to
     * be between notAfter and now + leeway. */
    if (resultNotAfter < CFAbsoluteTimeGetCurrent()) {
        resultNotBefore = resultNotAfter;
        resultNotAfter = CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY;
    }

    CFDateRef notBeforeDate = CFDateCreate(NULL, resultNotBefore);
    CFDateRef notAfterDate = CFDateCreate(NULL, resultNotAfter);
    CFDictionarySetValue(builder->info, kSecTrustInfoResultNotBefore, notBeforeDate);
    CFDictionarySetValue(builder->info, kSecTrustInfoResultNotAfter, notAfterDate);
    CFReleaseNull(notBeforeDate);
    CFReleaseNull(notAfterDate);

}

bool SecPathBuilderReportResult(SecPathBuilderRef builder) {
    builder->info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                              0, &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);

    SecPathBuilderReportTrustValidityPeriod(builder);
    /* isEV is not set unless also CT verified. Here, we need to check that we
     * got a revocation response as well. */
    if (builder->info && SecCertificatePathVCIsEV(builder->bestPath) && SecPathBuilderIsOkResult(builder)) {
#if !TARGET_OS_WATCH
        /* <rdar://32728029> We don't do networking on watchOS, so we can't require OCSP for EV */
        if (SecCertificatePathVCIsRevocationDone(builder->bestPath))
#endif
        {
#if !TARGET_OS_WATCH
            CFAbsoluteTime nextUpdate = SecCertificatePathVCGetEarliestNextUpdate(builder->bestPath);
            if (nextUpdate != 0)
#endif
            {
                /* Successful revocation check, so this cert is EV */
                CFDictionarySetValue(builder->info, kSecTrustInfoExtendedValidationKey,
                                     kCFBooleanTrue); /* iOS key */
                CFDictionarySetValue(builder->info, kSecTrustExtendedValidation,
                                     kCFBooleanTrue); /* unified API key */
                SecCertificateRef leaf = SecPathBuilderGetCertificateAtIndex(builder, 0);
                CFStringRef leafCompanyName = SecCertificateCopyCompanyName(leaf);
                if (leafCompanyName) {
                    CFDictionarySetValue(builder->info, kSecTrustInfoCompanyNameKey,
                                         leafCompanyName); /* iOS key */
                    CFDictionarySetValue(builder->info, kSecTrustOrganizationName,
                                         leafCompanyName); /* unified API key */
                    CFRelease(leafCompanyName);
                }
            }
        }
    }

    if (builder->info && SecPathBuilderIsOkResult(builder) && SecCertificatePathVCIsRevocationDone(builder->bestPath)) {
        CFAbsoluteTime nextUpdate = SecCertificatePathVCGetEarliestNextUpdate(builder->bestPath);
        if (nextUpdate != 0) {
            /* always populate revocation info for successful revocation check */
            CFDateRef validUntil = CFDateCreate(kCFAllocatorDefault, nextUpdate);
            CFDictionarySetValue(builder->info, kSecTrustInfoRevocationValidUntilKey,
                                 validUntil); /* iOS key */
            CFDictionarySetValue(builder->info, kSecTrustRevocationValidUntilDate,
                                 validUntil); /* unified API key */
            CFRelease(validUntil);
            CFDictionarySetValue(builder->info, kSecTrustInfoRevocationKey,
                                 kCFBooleanTrue); /* iOS key */
            CFDictionarySetValue(builder->info, kSecTrustRevocationChecked,
                                 kCFBooleanTrue); /* unified API key */
        } else if (SecCertificatePathVCIsEV(builder->bestPath)) {
            /* populate revocation info for failed revocation check with EV */
            CFDictionarySetValue(builder->info, kSecTrustInfoRevocationKey,
                                 kCFBooleanFalse); /* iOS key */
            CFDictionarySetValue(builder->info, kSecTrustRevocationChecked,
                                 kCFBooleanFalse); /* unified API key */
        } else if (SecCertificatePathVCRevocationCheckedAllCerts(builder->bestPath)) {
            CFDictionarySetValue(builder->info, kSecTrustInfoRevocationKey,
                                 kCFBooleanTrue); /* iOS key */
            CFDictionarySetValue(builder->info, kSecTrustRevocationChecked,
                                 kCFBooleanTrue); /* unified API key */
        }
    }

    /* If revoked, set the revocation reason */
    if (builder->info && !SecPathBuilderIsOkResult(builder) && SecCertificatePathVCIsRevocationDone(builder->bestPath)
        && SecCertificatePathVCGetRevocationReason(builder->bestPath)) {
        CFNumberRef reason = SecCertificatePathVCGetRevocationReason(builder->bestPath);
        CFDictionarySetValue(builder->info, kSecTrustRevocationReason, reason);
    }

    /* Set CT marker in the info */
    if (builder->info && SecCertificatePathVCIsCT(builder->bestPath) && SecPathBuilderIsOkResult(builder)) {
        CFDictionarySetValue(builder->info, kSecTrustInfoCertificateTransparencyKey,
                             kCFBooleanTrue);
    }


    /* This will trigger the outer step function to call the completion
       function. */
    builder->state = NULL;
    return false;
}

/* @function SecPathBuilderStep
   @summary This is the core of the async engine.
   @description Return false iff job is complete, true if a network request
   is pending.
   builder->state is a function pointer which is to be invoked.
   If you call this function from within a builder->state invocation it
   immediately returns true.
   Otherwise the following steps are repeated endlessly (unless a step returns)
   builder->state is invoked.  If it returns true and builder->state is still
   non NULL this proccess is repeated.
   If a state returns false, SecPathBuilder will return true
   if builder->state is non NULL.
   If builder->state is NULL then regardless of what the state function returns
   the completion callback will be invoked and the builder will be deallocated.
 */
bool SecPathBuilderStep(SecPathBuilderRef builder) {
    secdebug("async", "step builder %p", builder);
    if (builder->activations) {
        secdebug("async", "activations: %lu returning true",
                 builder->activations);
        return true;
    }

    secdebug("async", "activations: %lu", builder->activations);
    builder->activations++;
    while (builder->state && builder->state(builder));
    --builder->activations;

    if (builder->state) {
        secdebug("async", "waiting for async reply, exiting");
        /* A state returned false, it's waiting for network traffic.  Let's
         return. */
        return true;
    }

    if (builder->activations) {
        /* There is still at least one other running instance of this builder
         somewhere on the stack, we let that instance take care of sending
         the client a response. */
        return false;
    }

    SecPVCRef pvc = SecPathBuilderGetResultPVC(builder);
    SecTrustResultType result  = pvc->result;

    if (builder->exceptions && pvc->result == kSecTrustResultUnspecified) {
        result = kSecTrustResultProceed;
    }

    secinfo("trust", "completed: %@ details: %@ result: %d",
        builder->bestPath, pvc->details, result);

    if (builder->completed) {
        /* We want to retain just the data we need to return to our caller
         * and free the rest of the builder before doing the callback.
         * Since the callback may end an XPC transaction that made us active, we
         * want to retain as little residual memory as possible. */
        CFArrayRef resultPath = SecCertificatePathVCCopyCertificates(builder->bestPath);
        CFDictionaryRef info = CFRetainSafe(builder->info);
        CFArrayRef details = CFRetainSafe(pvc->details);
        const void *context = builder->context;
        SecPathBuilderCompleted completed = builder->completed;

        secdebug("async", "free builder");
        SecPathBuilderDestroy(builder);
        free(builder);

        secdebug("async", "returning to caller");
        completed(context, resultPath, details, info, result);
        CFReleaseNull(resultPath);
        CFReleaseNull(info);
        CFReleaseNull(details);
    } else {
        SecPathBuilderDestroy(builder);
        free(builder);
    }

    return false;
}

dispatch_queue_t SecPathBuilderGetQueue(SecPathBuilderRef builder) {
    return (builder) ? builder->queue : NULL;
}

CFDataRef SecPathBuilderCopyClientAuditToken(SecPathBuilderRef builder) {
    return (builder) ? (CFDataRef)CFRetainSafe(builder->clientAuditToken) : NULL;
}

// MARK: -
// MARK: SecTrustServer
/********************************************************
 ****************** SecTrustServer **********************
 ********************************************************/

typedef void (^SecTrustServerEvaluationCompleted)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, CFArrayRef chain, CFErrorRef error);

static void
SecTrustServerEvaluateCompleted(const void *userData,
                                CFArrayRef chain, CFArrayRef details, CFDictionaryRef info,
                                SecTrustResultType result) {
    SecTrustServerEvaluationCompleted evaluated = (SecTrustServerEvaluationCompleted)userData;
    TrustdHealthAnalyticsLogEvaluationCompleted();
    evaluated(result, details, info, chain, NULL);
    Block_release(evaluated);
}

void
SecTrustServerEvaluateBlock(dispatch_queue_t builderQueue, CFDataRef clientAuditToken, CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, CFArrayRef accessGroups, CFArrayRef exceptions, void (^evaluated)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, CFArrayRef chain, CFErrorRef error)) {
    /* We need an array containing at least one certificate to proceed. */
    if (!isArray(certificates) || !(CFArrayGetCount(certificates) > 0)) {
        CFErrorRef certError = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
        evaluated(kSecTrustResultInvalid, NULL, NULL, NULL, certError);
        CFReleaseSafe(certError);
        return;
    }
    SecTrustServerEvaluationCompleted userData = Block_copy(evaluated);
    /* Call the actual evaluator function. */
    SecPathBuilderRef builder = SecPathBuilderCreate(builderQueue, clientAuditToken,
                                                     certificates, anchors,
                                                     anchorsOnly, keychainsAllowed, policies,
                                                     responses, SCTs, trustedLogs,
                                                     verifyTime, accessGroups, exceptions,
                                                     SecTrustServerEvaluateCompleted, userData);
    SecPathBuilderStep(builder);
}

static CFDataRef SecTrustServerCopySelfAuditToken(void)
{
    audit_token_t token;
    kern_return_t kr;
    mach_msg_type_number_t token_size = TASK_AUDIT_TOKEN_COUNT;
    kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)&token, &token_size);
    if (kr != KERN_SUCCESS) {
        secwarning("failed to get audit token for ourselves");
        return NULL;
    }

    return CFDataCreate(NULL, (uint8_t *)&token, sizeof(token));
}


// NO_SERVER Shim code only, xpc interface should call SecTrustServerEvaluateBlock() directly
SecTrustResultType SecTrustServerEvaluate(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef exceptions, CFArrayRef *pdetails, CFDictionaryRef *pinfo, CFArrayRef *pchain, CFErrorRef *perror) {
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block SecTrustResultType result = kSecTrustResultInvalid;
    __block dispatch_queue_t queue = dispatch_queue_create("com.apple.trustd.evaluation.recursive", DISPATCH_QUEUE_SERIAL);

    /* make an audit token for ourselves */
    CFDataRef audit_token = SecTrustServerCopySelfAuditToken();

    /* We need to use the async call with the semaphore here instead of a synchronous call because we may return from
     * SecPathBuilderStep while waiting for an asynchronous network call in order to complete the evaluation. That return
     * is necessary in the XPC interface in order to free up the workloop for other trust evaluations while we wait for
     * the networking to complete, but here, we need to make sure we wait for the network call (which will async back
     * onto our queue) to complete and signal us before we return to the "inline" caller. */
    dispatch_async(queue, ^{
        SecTrustServerEvaluateBlock(queue, audit_token, certificates, anchors, anchorsOnly, keychainsAllowed, policies, responses, SCTs, trustedLogs, verifyTime, accessGroups, exceptions, ^(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, CFArrayRef chain, CFErrorRef error) {
            result = tr;
            if (tr == kSecTrustResultInvalid) {
                if (perror) {
                    *perror = error;
                    CFRetainSafe(error);
                }
            } else {
                if (pdetails) {
                    *pdetails = details;
                    CFRetainSafe(details);
                }
                if (pinfo) {
                    *pinfo = info;
                    CFRetainSafe(info);
                }
                if (pchain) {
                    *pchain = chain;
                    CFRetainSafe(chain);
                }
            }
            dispatch_semaphore_signal(done);
        });
    });
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    dispatch_release(done);
    dispatch_release_null(queue);
    CFReleaseNull(audit_token);

    return result;
}
