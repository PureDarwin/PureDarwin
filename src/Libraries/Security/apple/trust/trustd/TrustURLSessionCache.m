/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <CFNetwork/CFNSURLConnection.h>
#include <Security/SecInternalReleasePriv.h>
#include <utilities/debugging.h>

#include "trust/trustd/TrustURLSessionDelegate.h"
#include "trust/trustd/TrustURLSessionCache.h"

#define MAX_CACHED_SESSIONS 20
const NSString *TrustdUserAgent = @"com.apple.trustd/2.1";

static NSTimeInterval TrustURLSessionGetResourceTimeout(void) {
    return (NSTimeInterval)3.0;
}


@interface TrustURLSessionCache()
@property TrustURLSessionDelegate *delegate;
@property NSMutableDictionary <NSData *, NSURLSession *>* _clientSessionMap;
@property NSMutableArray <NSData *>* _clientLRUList;
@end

@implementation TrustURLSessionCache

- (instancetype)initWithDelegate:(TrustURLSessionDelegate *)delegate
{
    if (self = [super init]) {
        self.delegate = delegate;
        self._clientSessionMap = [NSMutableDictionary dictionaryWithCapacity:MAX_CACHED_SESSIONS];
        self._clientLRUList = [NSMutableArray arrayWithCapacity:(MAX_CACHED_SESSIONS + 1)];
    }
    return self;
}

- (NSURLSession *)createSessionForAuditToken:(NSData *)auditToken
{
    NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.timeoutIntervalForResource = TrustURLSessionGetResourceTimeout();
    config.HTTPAdditionalHeaders = @{@"User-Agent" : TrustdUserAgent};
    config._sourceApplicationAuditTokenData = auditToken;

    NSOperationQueue *queue = [[NSOperationQueue alloc] init];

    NSURLSession *session = [NSURLSession sessionWithConfiguration:config delegate:self.delegate delegateQueue:queue];
    return session;
}

- (NSURLSession *)sessionForAuditToken:(NSData *)auditToken
{
    @synchronized (self._clientLRUList) {
        NSURLSession *result = [self._clientSessionMap objectForKey:auditToken];
        if (result) {
            /* insert the client to the front of the LRU list */
            [self._clientLRUList removeObject:auditToken];
            [self._clientLRUList insertObject:auditToken atIndex:0];
            secdebug("http", "re-using session for %@", auditToken);
            return result;
        }
        /* Cache miss: create new session */
        result = [self createSessionForAuditToken:auditToken];
        [self._clientLRUList insertObject:auditToken atIndex:0];
        [self._clientSessionMap setObject:result forKey:auditToken];
        secdebug("http", "creating session for %@", auditToken);
        if (self._clientLRUList.count > MAX_CACHED_SESSIONS) {
            /* close the excess NSURLSession and remove it from our cache */
            NSData *removeToken = [self._clientLRUList objectAtIndex:(self._clientLRUList.count - 1)];
            NSURLSession *removeSession = [self._clientSessionMap objectForKey:removeToken];
            [removeSession finishTasksAndInvalidate];
            [self._clientSessionMap removeObjectForKey:removeToken];
            [self._clientLRUList removeLastObject];
            secdebug("http", "removing session for %@", removeToken);
        }
        return result;
    }
}

@end
