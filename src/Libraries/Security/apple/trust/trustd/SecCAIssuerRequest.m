/*
 * Copyright (c) 2009-2018 Apple Inc. All Rights Reserved.
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
 * SecCAIssuerRequest.m - asynchronous CAIssuer request fetching engine.
 */


#include "SecCAIssuerRequest.h"
#include "SecCAIssuerCache.h"

#import <Foundation/Foundation.h>
#import <CFNetwork/CFNSURLConnection.h>
#include <Security/SecInternal.h>
#include <Security/SecCMS.h>
#include <CoreFoundation/CFURL.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <utilities/debugging.h>
#include <Security/SecCertificateInternal.h>
#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/TrustURLSessionDelegate.h"
#include "trust/trustd/TrustURLSessionCache.h"
#include <stdlib.h>
#include <mach/mach_time.h>

#define MAX_CA_ISSUERS 3
#define CA_ISSUERS_REQUEST_THRESHOLD 10

typedef void (*CompletionHandler)(void *context, CFArrayRef parents);

@interface CAIssuerDelegate: TrustURLSessionDelegate
@end

@interface CAIssuerContext : TrustURLSessionContext
@property (assign) CompletionHandler callback;
@end

@implementation CAIssuerContext
@end

static NSArray *certificatesFromData(NSData *data) {
    /* RFC5280 4.2.2.1:
     "accessLocation MUST be a uniformResourceIdentifier and the URI
     MUST point to either a single DER encoded certificate as speci-
     fied in [RFC2585] or a collection of certificates in a BER or
     DER encoded "certs-only" CMS message as specified in [RFC2797]." */

    /* DER-encoded certificate */
    SecCertificateRef parent = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (parent) {
        NSArray *result = @[(__bridge id)parent];
        CFReleaseNull(parent);
        return result;
    }

    /* "certs-only" CMS Message */
    CFArrayRef certificates = SecCMSCertificatesOnlyMessageCopyCertificates((__bridge CFDataRef)data);
    if (certificates) {
        return CFBridgingRelease(certificates);
    }

    /* Retry in case the certificate is in PEM format. Some CAs
     incorrectly return a PEM encoded cert, despite RFC 5280 4.2.2.1 */
    parent = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)data);
    if (parent) {
        NSArray *result = @[(__bridge id)parent];
        CFReleaseNull(parent);
        return result;
    }
    return nil;
}

@implementation CAIssuerDelegate
- (BOOL)fetchNext:(NSURLSession *)session context:(TrustURLSessionContext *)urlContext  {
    SecPathBuilderRef builder = (SecPathBuilderRef)urlContext.context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);

    BOOL result = false;
    if (!(result = [super fetchNext:session context:urlContext]) && analytics) {
        analytics->ca_issuer_fetches++;
    }
    return result;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    /* call the superclass's method to set taskTime and expiration */
    [super URLSession:session task:task didCompleteWithError:error];

    NSUUID *taskId = [task.originalRequest taskId];
    CAIssuerContext *urlContext = (CAIssuerContext *)[self contextForTask:taskId];
    if (!urlContext) {
        secerror("failed to find context for %@", taskId);
        return;
    }

    __block SecPathBuilderRef builder =(SecPathBuilderRef)urlContext.context;
    if (!builder) {
        /* We already returned to the PathBuilder state machine. */
        [self removeTask:taskId];
        return;
    }

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    __block NSArray *parents = nil;
    if (error) {
        /* Log the error */
        secnotice("caissuer", "Failed to download issuer %@, with error %@", task.originalRequest.URL, error);
        if (analytics) {
            analytics->ca_issuer_fetch_failed++;
        }
    } else if (urlContext.response) {
        /* Get the parent cert from the data */
        parents = certificatesFromData(urlContext.response);
        if (analytics && !parents) {
            analytics->ca_issuer_unsupported_data = true;
        } else if (analytics && [parents count] > 1) {
            analytics->ca_issuer_multiple_certs = true;
        }
    }

    if (parents)  {
        /* Found some parents, add to cache, close session, and return to SecPathBuilder */
        secdebug("caissuer", "found parents for %@", task.originalRequest.URL);
        SecCAIssuerCacheAddCertificates((__bridge CFArrayRef)parents, (__bridge CFURLRef)task.originalRequest.URL, urlContext.expiration);
        urlContext.context = nil; // set the context to NULL before we call back because the callback may free the builder
        dispatch_async(SecPathBuilderGetQueue(builder), ^{
            urlContext.callback(builder, (__bridge CFArrayRef)parents);
        });
    } else {
        secdebug("caissuer", "no parents for %@", task.originalRequest.URL);
        if ([self fetchNext:session context:urlContext]) { // Try the next CAIssuer URI
            /* no fetch scheduled, jump back into the state machine on the builder's queue */
            secdebug("caissuer", "no more fetches. returning to builder");
            urlContext.context = nil; // set the context to NULL before we call back because the callback may free the builder
            dispatch_async(SecPathBuilderGetQueue(builder), ^{
                urlContext.callback(builder, NULL);
            });
        }
    }
    // We've either kicked off a new task or returned to the builder, so we're done with this task.
    [self removeTask:taskId];
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)taskMetrics {
    secdebug("caissuer", "got metrics with task interval %f", taskMetrics.taskInterval.duration);

    NSUUID *taskId = [task.originalRequest taskId];
    TrustURLSessionContext *urlContext = [self contextForTask:taskId];
    if (!urlContext) {
        secerror("failed to find context for %@", taskId);
        return;
    }

    SecPathBuilderRef builder =(SecPathBuilderRef)urlContext.context;
    if (builder) {
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
        if (analytics) {
            analytics->ca_issuer_fetch_time += (uint64_t)(taskMetrics.taskInterval.duration * NSEC_PER_SEC);
        }
    }
}
@end

/* Releases parent unconditionally, and return a CFArrayRef containing
 parent if the normalized subject of parent matches the normalized issuer
 of certificate. */
static CF_RETURNS_RETAINED CFArrayRef SecCAIssuerConvertToParents(SecCertificateRef certificate, CFArrayRef CF_CONSUMED putativeParents) {
    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    NSMutableArray *parents = [NSMutableArray array];
    NSArray *possibleParents = CFBridgingRelease(putativeParents);
    for (id parent in possibleParents) {
        CFDataRef parent_nic = SecCertificateGetNormalizedSubjectContent((__bridge SecCertificateRef)parent);
        if (nic && parent_nic && CFEqual(nic, parent_nic)) {
            [parents addObject:parent];
        }
    }

    if ([parents count] > 0) {
        return CFBridgingRetain(parents);
    } else {
        return nil;
    }
}

static CFArrayRef SecCAIssuerRequestCacheCopyParents(SecCertificateRef cert,
    CFArrayRef issuers) {
    CFIndex ix = 0, ex = CFArrayGetCount(issuers);
    for (;ix < ex; ++ix) {
        CFURLRef issuer = CFArrayGetValueAtIndex(issuers, ix);
        CFStringRef scheme = CFURLCopyScheme(issuer);
        if (scheme) {
            if (CFEqual(CFSTR("http"), scheme)) {
                CFArrayRef parents = SecCAIssuerConvertToParents(cert,
                    SecCAIssuerCacheCopyMatching(issuer));
                if (parents) {
                    secdebug("caissuer", "cache hit, for %@. no request issued", issuer);
                    CFRelease(scheme);
                    return parents;
                }
            }
            CFRelease(scheme);
        }
    }
    return NULL;
}

bool SecCAIssuerCopyParents(SecCertificateRef certificate, void *context, void (*callback)(void *, CFArrayRef)) {
    @autoreleasepool {
        CFArrayRef issuers = CFRetainSafe(SecCertificateGetCAIssuers(certificate));
        NSArray *nsIssuers = CFBridgingRelease(issuers);
        if (!issuers) {
            /* certificate has no caissuer urls, we're done. */
            callback(context, NULL);
            return true;
        }

        SecPathBuilderRef builder = (SecPathBuilderRef)context;
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
        CFArrayRef parents = SecCAIssuerRequestCacheCopyParents(certificate, (__bridge CFArrayRef)nsIssuers);
        if (parents) {
            if (analytics) {
                /* We found parents in the cache */
                analytics->ca_issuer_cache_hit = true;
            }
            callback(context, parents);
            CFReleaseSafe(parents);
            return true;
        }
        if (analytics) {
            /* We're going to have to make a network call */
            analytics->ca_issuer_network = true;
        }

        NSInteger count = [nsIssuers count];
        if (count >= CA_ISSUERS_REQUEST_THRESHOLD) {
            secnotice("caissuer", "too many caIssuer entries (%ld)", (long)count);
            callback(context, NULL);
            return true;
        }

        static TrustURLSessionCache *sessionCache = NULL;
        static CAIssuerDelegate *delegate = NULL;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            delegate = [[CAIssuerDelegate alloc] init];
            sessionCache = [[TrustURLSessionCache alloc] initWithDelegate:delegate];
        });

        NSData *auditToken = CFBridgingRelease(SecPathBuilderCopyClientAuditToken(builder));
        NSURLSession *session = [sessionCache sessionForAuditToken:auditToken];
        CAIssuerContext *urlContext = [[CAIssuerContext alloc] initWithContext:context uris:nsIssuers];
        urlContext.callback = callback;
        return [delegate fetchNext:session context:urlContext];
    }
}

