/*
 * Copyright (c) 2018-2020 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_TRUSTURLSESSIONDELEGATE_H_
#define _SECURITY_TRUSTURLSESSIONDELEGATE_H_

#if __OBJC__
#include <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface TrustURLSessionContext : NSObject
@property (assign, nullable) void *context;
@property NSArray <NSURL *>*URIs;
@property NSUInteger URIix;
@property (nullable) NSMutableData *response;
@property NSTimeInterval expiration;
@property NSUInteger numTasks;

- (instancetype)initWithContext:(void *)context uris:(NSArray <NSURL *>*)uris;
@end

@interface NSURLRequest (TrustURLRequest)
- (NSUUID * _Nullable)taskId;
@end

/* This is our abstract NSURLSessionDelegate that handles the elements common to
 * fetching data over the network during a trust evaluation */
@interface TrustURLSessionDelegate : NSObject <NSURLSessionDelegate, NSURLSessionTaskDelegate, NSURLSessionDataDelegate>

/* The delegate superclass keeps track of all the tasks that have been kicked off (via fetchNext);
 * it is the responsibility of the subclass to remove tasks when it is done with them. */
- (TrustURLSessionContext *)contextForTask:(NSUUID *)taskId;
- (void)removeTask:(NSUUID *)taskId;

- (BOOL)fetchNext:(NSURLSession *)session context:(TrustURLSessionContext *)context;
- (NSURLRequest *)createNextRequest:(NSURL *)uri context:(TrustURLSessionContext *)context;
@end

NS_ASSUME_NONNULL_END
#endif // __OBJC__

#endif /* _SECURITY_TRUSTURLSESSIONDELEGATE_H_ */
