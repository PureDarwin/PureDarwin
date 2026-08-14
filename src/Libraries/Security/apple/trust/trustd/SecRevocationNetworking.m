/*
 * Copyright (c) 2017-2019 Apple Inc. All Rights Reserved.
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
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <CFNetwork/CFNSURLConnection.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <mach/mach_time.h>
#include <os/transaction_private.h>
#include <Security/SecCertificateInternal.h>
#include "utilities/debugging.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/SecPLWrappers.h"
#include "utilities/SecFileLocations.h"

#include "SecRevocationDb.h"
#include "SecRevocationServer.h"
#include "SecTrustServer.h"
#include "SecOCSPRequest.h"
#include "SecOCSPResponse.h"
#include "trustdFileLocations.h"

#import "SecTrustLoggingServer.h"
#import "TrustURLSessionDelegate.h"
#import "trust/trustd/TrustURLSessionCache.h"

#import "SecRevocationNetworking.h"

/* MARK: Valid Update Networking */
static CFStringRef kSecPrefsDomain      = CFSTR("com.apple.security");
static CFStringRef kUpdateWiFiOnlyKey   = CFSTR("ValidUpdateWiFiOnly");
static CFStringRef kUpdateBackgroundKey = CFSTR("ValidUpdateBackground");

extern CFAbsoluteTime gUpdateStarted;
extern CFAbsoluteTime gNextUpdate;

static uint64_t systemUptimeInSeconds() {
    struct timeval boottime;
    size_t tv_size = sizeof(boottime);
    time_t now, uptime = 0;
    int mib[2];
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
    (void) time(&now);
    if (sysctl(mib, 2, &boottime, &tv_size, NULL, 0) != -1 &&
        boottime.tv_sec != 0) {
        uptime = now - boottime.tv_sec;
    }
    return (uint64_t)uptime;
}

typedef void (^CompletionHandler)(void);

@interface ValidDelegate : NSObject <NSURLSessionDelegate, NSURLSessionTaskDelegate, NSURLSessionDataDelegate>
@property CompletionHandler handler;
@property dispatch_queue_t revDbUpdateQueue;
@property os_transaction_t transaction;
@property NSString *currentUpdateServer;
@property NSFileHandle *currentUpdateFile;
@property NSURL *currentUpdateFileURL;
@property BOOL finishedDownloading;
@end

@implementation ValidDelegate

- (void)reschedule {
    /* POWER LOG EVENT: operation canceled */
    SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
        @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
        @"event" : (self->_finishedDownloading) ? @"updateCanceled" : @"downloadCanceled"
    });
    secnotice("validupdate", "%s canceled at %f",
        (self->_finishedDownloading) ? "update" : "download",
        (double)CFAbsoluteTimeGetCurrent());

    self->_handler();
    SecRevocationDbComputeAndSetNextUpdateTime();
    if (self->_transaction) {
        self->_transaction = nil;
    }
}

- (void)updateDb:(NSUInteger)version {
    __block NSURL *updateFileURL = self->_currentUpdateFileURL;
    __block NSString *updateServer = self->_currentUpdateServer;
    __block NSFileHandle *updateFile = self->_currentUpdateFile;
    if (!updateFileURL || !updateFile) {
        [self reschedule];
        return;
    }

    /* Hold a transaction until we finish the update */
    __block os_transaction_t transaction = os_transaction_create("com.apple.trustd.valid.updateDb");
    dispatch_async(_revDbUpdateQueue, ^{
        /* POWER LOG EVENT: background update started */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"updateStarted"
        });
        secnotice("validupdate", "update started at %f", (double)CFAbsoluteTimeGetCurrent());

        CFDataRef updateData = NULL;
        const char *updateFilePath = [updateFileURL fileSystemRepresentation];
        int rtn;
        if ((rtn = readValidFile(updateFilePath, &updateData)) != 0) {
            secerror("failed to read %@ with error %d", updateFileURL, rtn);
            TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TAFatalError, rtn);
            [self reschedule];
            transaction = nil;
            return;
        }

        secdebug("validupdate", "verifying and ingesting data from %@", updateFileURL);
        SecValidUpdateVerifyAndIngest(updateData, (__bridge CFStringRef)updateServer, (0 == version));
        if ((rtn = munmap((void *)CFDataGetBytePtr(updateData), CFDataGetLength(updateData))) != 0) {
            secerror("unable to unmap current update %ld bytes at %p (error %d)", CFDataGetLength(updateData), CFDataGetBytePtr(updateData), rtn);
        }
        CFReleaseNull(updateData);

        /* We're done with this file */
        [updateFile closeFile];
        if (updateFilePath) {
            (void)remove(updateFilePath);
        }
        self->_currentUpdateFile = nil;
        self->_currentUpdateFileURL = nil;
        self->_currentUpdateServer = nil;

        /* POWER LOG EVENT: background update finished */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"updateFinished"
        });

        /* Update is complete */
        secnotice("validupdate", "update finished at %f", (double)CFAbsoluteTimeGetCurrent());
        gUpdateStarted = 0;

        self->_handler();
        transaction = nil; // we're all done now
    });
}

- (NSInteger)versionFromTask:(NSURLSessionTask *)task {
    return atol([task.taskDescription cStringUsingEncoding:NSUTF8StringEncoding]);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
didReceiveResponse:(NSURLResponse *)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler {
    /* nsurlsessiond started our download. Create a transaction since we're going to be working for a little bit */
    self->_transaction = os_transaction_create("com.apple.trustd.valid.download");
    secinfo("validupdate", "Session %@ data task %@ returned response %ld (%@), expecting %lld bytes",
            session, dataTask, (long)[(NSHTTPURLResponse *)response statusCode],
            [response MIMEType], [response expectedContentLength]);

    CFURLRef updateFileURL = SecCopyURLForFileInRevocationInfoDirectory(CFSTR("update-current"));
    self->_currentUpdateFileURL = (updateFileURL) ? CFBridgingRelease(updateFileURL) : nil;
    const char *updateFilePath = [self->_currentUpdateFileURL fileSystemRepresentation];
    if (!updateFilePath) {
        secnotice("validupdate", "failed to find revocation info directory. canceling task %@", dataTask);
        completionHandler(NSURLSessionResponseCancel);
        [self reschedule];
        return;
    }

    /* Clean up any old files from previous tasks. */
    (void)remove(updateFilePath);

    int fd;
    fd = open(updateFilePath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        secnotice("validupdate","unable to open %@ (errno %d)", self->_currentUpdateFileURL, errno);
    }
    if (fd >= 0) {
        close(fd);
    }

    /* POWER LOG EVENT: background download actually started */
    SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
        @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
        @"event" : @"downloadStarted"
    });
    secnotice("validupdate", "download started at %f", (double)CFAbsoluteTimeGetCurrent());

    NSError *error = nil;
    self->_currentUpdateFile = [NSFileHandle fileHandleForWritingToURL:self->_currentUpdateFileURL error:&error];
    if (!self->_currentUpdateFile) {
        secnotice("validupdate", "failed to open %@: %@. canceling task %@", self->_currentUpdateFileURL, error, dataTask);
#if ENABLE_TRUSTD_ANALYTICS
        [[TrustAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventValidUpdate hardFailure:NO result:error];
#endif // ENABLE_TRUSTD_ANALYTICS
        completionHandler(NSURLSessionResponseCancel);
        [self reschedule];
        return;
    }

    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {
    secdebug("validupdate", "Session %@ data task %@ returned %lu bytes (%lld bytes so far) out of expected %lld bytes",
             session, dataTask, (unsigned long)[data length], [dataTask countOfBytesReceived], [dataTask countOfBytesExpectedToReceive]);

    if (!self->_currentUpdateFile) {
        secnotice("validupdate", "received data, but output file is not open");
        [dataTask cancel];
        [self reschedule];
        return;
    }

    @try {
        /* Writing can fail and throw an exception, e.g. if we run out of disk space. */
        [self->_currentUpdateFile writeData:data];
    }
    @catch(NSException *exception) {
        secnotice("validupdate", "%s", exception.description.UTF8String);
        TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TARecoverableError, errSecDiskFull);
        [dataTask cancel];
        [self reschedule];
    }
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error {
    if (error) {
        secnotice("validupdate", "Session %@ task %@ failed with error %@", session, task, error);
#if ENABLE_TRUSTD_ANALYTICS
        [[TrustAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventValidUpdate hardFailure:NO result:error];
#endif // ENABLE_TRUSTD_ANALYTICS
        [self reschedule];
        /* close file before we leave */
        [self->_currentUpdateFile closeFile];
        self->_currentUpdateFile = nil;
        self->_currentUpdateServer = nil;
        self->_currentUpdateFileURL = nil;
    } else {
        /* POWER LOG EVENT: background download finished */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"downloadFinished"
        });
        secnotice("validupdate", "download finished at %f", (double)CFAbsoluteTimeGetCurrent());
        secdebug("validupdate", "Session %@ task %@ succeeded", session, task);
        self->_finishedDownloading = YES;
        [self updateDb:[self versionFromTask:task]];
    }
    if (self->_transaction) {
        self->_transaction = nil;
    }
}

@end

@interface ValidUpdateRequest : NSObject
@property NSTimeInterval updateScheduled;
@property NSURLSession *backgroundSession;
@property NSURLSession *ephemeralSession;
@end

static ValidUpdateRequest *request = nil;

@implementation ValidUpdateRequest

- (NSURLSessionConfiguration *)validUpdateConfiguration:(BOOL)background {
    NSURLSessionConfiguration *config = nil;
    if (background) {
        /* preferences to override defaults */
        CFTypeRef value = NULL;
        bool updateOnWiFiOnly = true;
        value = CFPreferencesCopyValue(kUpdateWiFiOnlyKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (isBoolean(value)) {
            updateOnWiFiOnly = CFBooleanGetValue((CFBooleanRef)value);
        }
        CFReleaseNull(value);

        config = [NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier: @"com.apple.trustd.networking.background"];
        config.networkServiceType = NSURLNetworkServiceTypeBackground;
        config.discretionary = YES;
        config._requiresPowerPluggedIn = YES;
        config.allowsCellularAccess = (!updateOnWiFiOnly) ? YES : NO;
    } else {
        config = [NSURLSessionConfiguration ephemeralSessionConfiguration]; // no cookies or data storage
        config.networkServiceType = NSURLNetworkServiceTypeDefault;
        config.discretionary = NO;
    }

    config.HTTPAdditionalHeaders = @{ @"User-Agent" : TrustdUserAgent,
                                      @"Accept" : @"*/*",
                                      @"Accept-Encoding" : @"gzip,deflate,br"};

    config.TLSMinimumSupportedProtocol = kTLSProtocol12;

    return config;
}

- (NSURLSession *)createSession:(BOOL)background queue:(dispatch_queue_t)updateQueue forServer:(NSString *)updateServer
{
    NSURLSessionConfiguration *config = [self validUpdateConfiguration:background];
    ValidDelegate *delegate = [[ValidDelegate alloc] init];
    delegate.handler = ^(void) {
        request.updateScheduled = 0.0;
        secdebug("validupdate", "resetting scheduled time");
    };
    delegate.transaction = NULL;
    delegate.revDbUpdateQueue = updateQueue;
    delegate.finishedDownloading = NO;
    delegate.currentUpdateServer = [updateServer copy];

    /* Callbacks should be on a separate NSOperationQueue.
       We'll then dispatch the work on updateQueue and return from the callback. */
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];
    queue.maxConcurrentOperationCount = 1;
    return [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:queue];
}

- (void) createSessions:(dispatch_queue_t)updateQueue forServer:(NSString *)updateServer {
    self.ephemeralSession = [self createSession:NO queue:updateQueue forServer:updateServer];

    bool updateInBackground = true;
    CFTypeRef value = CFPreferencesCopyValue(kUpdateBackgroundKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateInBackground = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);
    if (updateInBackground) {
        self.backgroundSession = [self createSession:YES queue:updateQueue forServer:updateServer];
    } else {
        self.backgroundSession = self.ephemeralSession;
    }
}

- (BOOL) scheduleUpdateFromServer:(NSString *)server forVersion:(NSUInteger)version withQueue:(dispatch_queue_t)updateQueue {
    if (!server) {
        secnotice("validupdate", "invalid update request");
        return NO;
    }

    if (!updateQueue) {
        secnotice("validupdate", "missing update queue, skipping update");
        return NO;
    }

    /* nsurlsessiond waits for unlock to finish launching, so we can't block trust evaluations
     * on scheduling this background task. Also, we want to wait a sufficient amount of time
     * after system boot before trying to initiate network activity, to avoid the possibility
     * of a performance regression in the boot path. */
    dispatch_async(updateQueue, ^{
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if (self.updateScheduled != 0.0) {
            secdebug("validupdate", "update in progress (scheduled %f)", (double)self.updateScheduled);
            return;
        } else {
            uint64_t uptime = systemUptimeInSeconds();
            const uint64_t minUptime = 180;
            if (uptime < minUptime) {
                gNextUpdate = now + (minUptime - uptime);
                gUpdateStarted = 0;
                secnotice("validupdate", "postponing update until %f", gNextUpdate);
                return;
            } else {
                self.updateScheduled = now;
                secnotice("validupdate", "scheduling update at %f", (double)self.updateScheduled);
            }
        }

        /* we have an update to schedule, so take a transaction while we work */
        os_transaction_t transaction = os_transaction_create("com.apple.trustd.valid.scheduleUpdate");

        /* clear all old sessions and cleanup disk (for previous download tasks) */
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            @autoreleasepool {
                [NSURLSession _obliterateAllBackgroundSessionsWithCompletionHandler:^{
                    secnotice("validupdate", "removing all old sessions for trustd");
                }];
            }
        });

        if (!self.backgroundSession) {
            [self createSessions:updateQueue forServer:server];
        } else {
            ValidDelegate *delegate = (ValidDelegate *)[self.backgroundSession delegate];
            delegate.currentUpdateServer = [server copy];
        }

        /* POWER LOG EVENT: scheduling our background download session now */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"downloadScheduled",
            @"version" : @(version)
        });

        NSURL *validUrl = [NSURL URLWithString:[NSString stringWithFormat:@"https://%@/g3/v%ld",
                                                server, (unsigned long)version]];
        NSURLSessionDataTask *dataTask = [self.backgroundSession dataTaskWithURL:validUrl];
        dataTask.taskDescription = [NSString stringWithFormat:@"%lu",(unsigned long)version];
        [dataTask resume];
        secnotice("validupdate", "scheduled background data task %@ at %f", dataTask, CFAbsoluteTimeGetCurrent());
        (void) transaction; // dead store
        transaction = nil; // ARC releases the transaction
    });

    return YES;
}

- (BOOL)updateNowFromServer:(NSString *)server version:(NSUInteger)version queue:(dispatch_queue_t)updateQueue
{
    if (!server) {
        secnotice("validupdate", "invalid update request");
        return NO;
    }

    if (!updateQueue) {
        secnotice("validupdate", "missing update queue, skipping update");
        return NO;
    }

    if (!self.ephemeralSession) {
        [self createSessions:updateQueue forServer:server];
    } else {
        ValidDelegate *delegate = (ValidDelegate *)[self.ephemeralSession delegate];
        delegate.currentUpdateServer = [server copy];
    }

    /* POWER LOG EVENT: scheduling our background download session now */
    SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
        @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
        @"event" : @"downloadScheduled",
        @"version" : @(version)
    });

    NSURL *validUrl = [NSURL URLWithString:[NSString stringWithFormat:@"https://%@/g3/v%ld",
                                            server, (unsigned long)version]];
    NSURLSessionDataTask *dataTask = [self.ephemeralSession dataTaskWithURL:validUrl];
    dataTask.taskDescription = [NSString stringWithFormat:@"%lu",(unsigned long)version];
    [dataTask resume];
    secnotice("validupdate", "running foreground data task %@ at %f", dataTask, CFAbsoluteTimeGetCurrent());
    return YES;
}

@end

static void SecValidUpdateCreateValidUpdateRequest()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        @autoreleasepool {
            request = [[ValidUpdateRequest alloc] init];
        }
    });
}

bool SecValidUpdateRequest(dispatch_queue_t queue, CFStringRef server, CFIndex version)  {
    SecValidUpdateCreateValidUpdateRequest();
    @autoreleasepool {
        return [request scheduleUpdateFromServer:(__bridge NSString*)server forVersion:version withQueue:queue];
    }
}

bool SecValidUpdateUpdateNow(dispatch_queue_t queue, CFStringRef server, CFIndex version) {
    SecValidUpdateCreateValidUpdateRequest();
    @autoreleasepool {
        return [request updateNowFromServer:(__bridge NSString*)server version:version queue:queue];
    }
}

/* MARK: - */
/* MARK: OCSP Fetch Networking */
#define OCSP_REQUEST_THRESHOLD 10

@interface OCSPFetchDelegate : TrustURLSessionDelegate
@end

@implementation OCSPFetchDelegate
- (BOOL)fetchNext:(NSURLSession *)session context:(TrustURLSessionContext *)urlContext   {
    SecORVCRef orvc = (SecORVCRef)urlContext.context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(orvc->builder);

    BOOL result = true;
    if ((result = [super fetchNext:session context:urlContext])) {
        /* no fetch scheduled */
        orvc->done = true;
    } else {
        if (urlContext.URIix > 0) {
            orvc->responder = (__bridge CFURLRef)urlContext.URIs[urlContext.URIix - 1];
        } else {
            orvc->responder = (__bridge CFURLRef)urlContext.URIs[0];
        }
        if (analytics) {
            analytics->ocsp_fetches++;
        }
    }
    return result;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    /* call the superclass's method to set expiration */
    [super URLSession:session task:task didCompleteWithError:error];

    NSUUID *taskId = [task.originalRequest taskId];
    TrustURLSessionContext *urlContext = [self contextForTask:taskId];
    if (!urlContext) {
        secerror("failed to find context for %@", taskId);
        return;
    }

    __block SecORVCRef orvc = (SecORVCRef)urlContext.context;
    if (!orvc || !orvc->builder) {
        /* We already returned to the PathBuilder state machine. */
        [self removeTask:taskId];
        return;
    }

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(orvc->builder);
    if (error) {
        /* Log the error */
        secnotice("rvc", "Failed to download ocsp response %@, with error %@", task.originalRequest.URL, error);
        if (analytics) {
            analytics->ocsp_fetch_failed++;
        }
    } else {
        SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate((__bridge CFDataRef)urlContext.response);
        if (ocspResponse) {
            SecORVCConsumeOCSPResponse(orvc, ocspResponse, urlContext.expiration, true, false);
            if (analytics && !orvc->done) {
                /* We got an OCSP response that didn't pass validation */
                analytics->ocsp_validation_failed = true;
            }
        } else if (analytics) {
            /* We got something that wasn't an OCSP response (e.g. captive portal) --
             * we consider that a fetch failure */
            analytics->ocsp_fetch_failed++;
        }
    }

    /* If we didn't get a valid OCSP response, try the next URI */
    if (!orvc->done) {
        (void)[self fetchNext:session context:urlContext];
    }

    /* We got a valid OCSP response or couldn't schedule any more fetches.
     * Close the session, update the PVCs, decrement the async count, and callback if we're all done.  */
    if (orvc->done) {
        secdebug("rvc", "builder %p, done with OCSP fetches for cert: %ld", orvc->builder, orvc->certIX);
        urlContext.context = nil;
        SecORVCUpdatePVC(orvc);
        if (0 == SecPathBuilderDecrementAsyncJobCount(orvc->builder)) {
            /* We're the last async job to finish, jump back into the state machine */
            secdebug("rvc", "builder %p, done with all async jobs", orvc->builder);
            dispatch_async(SecPathBuilderGetQueue(orvc->builder), ^{
                SecPathBuilderStep(orvc->builder);
            });
        }
    }
    // We've either kicked off a new task or returned to the builder, so we're done with this task.
    [self removeTask:taskId];
}

- (NSURLRequest *)createNextRequest:(NSURL *)uri context:(TrustURLSessionContext *)urlContext {
    SecORVCRef orvc = (SecORVCRef)urlContext.context;
    CFDataRef ocspDER = CFRetainSafe(SecOCSPRequestGetDER(orvc->ocspRequest));
    NSData *nsOcspDER = CFBridgingRelease(ocspDER);
    NSString *ocspBase64 = [nsOcspDER base64EncodedStringWithOptions:0];

    /* Ensure that we percent-encode specific characters in the base64 path
       which are defined as delimiters in RFC 3986 [2.2].
     */
    static NSMutableCharacterSet *allowedSet = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        allowedSet = [[NSCharacterSet URLPathAllowedCharacterSet] mutableCopy];
        [allowedSet removeCharactersInString:@":/?#[]@!$&'()*+,;="];
    });
    NSString *escapedRequest = [ocspBase64 stringByAddingPercentEncodingWithAllowedCharacters:allowedSet];
    NSURLRequest *request = nil;

    /* Interesting tidbit from rfc5019
     When sending requests that are less than or equal to 255 bytes in
     total (after encoding) including the scheme and delimiters (http://),
     server name and base64-encoded OCSPRequest structure, clients MUST
     use the GET method (to enable OCSP response caching).  OCSP requests
     larger than 255 bytes SHOULD be submitted using the POST method.
     */
    if (([[uri absoluteString] length] + 1 + [escapedRequest length]) < 256) {
        /* Use a GET */
        NSString *requestString = [NSString stringWithFormat:@"%@/%@", [uri absoluteString], escapedRequest];
        NSURL *requestURL = [NSURL URLWithString:requestString];
        request = [super createNextRequest:requestURL context:urlContext];
    } else {
        /* Use a POST */
        NSMutableURLRequest *mutableRequest = [[super createNextRequest:uri context:urlContext] mutableCopy];
        mutableRequest.HTTPMethod = @"POST";
        mutableRequest.HTTPBody = nsOcspDER;
        request = mutableRequest;
    }

    return request;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)taskMetrics {
    NSUUID *taskId = [task.originalRequest taskId];
    TrustURLSessionContext *urlContext = [self contextForTask:taskId];
    if (!urlContext) {
        secerror("failed to find context for %@", taskId);
        return;
    }

    secdebug("rvc", "got metrics with task interval %f", taskMetrics.taskInterval.duration);
    SecORVCRef orvc = (SecORVCRef)urlContext.context;
    if (orvc && orvc->builder) {
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(orvc->builder);
        if (analytics) {
            analytics->ocsp_fetch_time += (uint64_t)(taskMetrics.taskInterval.duration * NSEC_PER_SEC);
        }
    }
}
@end

bool SecORVCBeginFetches(SecORVCRef orvc, SecCertificateRef cert) {
    @autoreleasepool {
        CFArrayRef ocspResponders = CFRetainSafe(SecCertificateGetOCSPResponders(cert));
        NSArray *nsResponders = CFBridgingRelease(ocspResponders);

        NSInteger count = [nsResponders count];
        if (count > OCSP_REQUEST_THRESHOLD) {
            secnotice("rvc", "too may OCSP responder entries (%ld)", (long)count);
            orvc->done = true;
            return true;
        }

        static TrustURLSessionCache *sessionCache = NULL;
        static OCSPFetchDelegate *delegate = NULL;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            delegate = [[OCSPFetchDelegate alloc] init];
            sessionCache = [[TrustURLSessionCache alloc] initWithDelegate:delegate];
        });

        NSData *auditToken = CFBridgingRelease(SecPathBuilderCopyClientAuditToken(orvc->builder));
        NSURLSession *session = [sessionCache sessionForAuditToken:auditToken];
        TrustURLSessionContext *context = [[TrustURLSessionContext alloc] initWithContext:orvc uris:nsResponders];
        return [delegate fetchNext:session context:context];
    }
}
