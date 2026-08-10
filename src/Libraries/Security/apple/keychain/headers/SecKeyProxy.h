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
 */

/*!
 @header SecKeyProxy
 Declaration of SecKey proxy object allowing SecKeyRef to be accessed remotely through XPC.
 */

#ifndef _SECURITY_SECKEYPROXY_H_
#define _SECURITY_SECKEYPROXY_H_

#import <Foundation/Foundation.h>
#include <Security/SecBase.h>
#include <Security/SecKey.h>

NS_ASSUME_NONNULL_BEGIN

@interface SecKeyProxy : NSObject {
@private
    id _key;
    NSData * _Nullable _certificate;
    NSXPCListener *_listener;
}

// Creates new proxy instance. Proxy holds reference to the target key or identity and allows remote access to that target key as long as the proxy instance is kept alive.
- (instancetype)initWithKey:(SecKeyRef)key;
- (instancetype)initWithIdentity:(SecIdentityRef)identity;

// Retrieve endpoint to this proxy instance.  Endpoint can be transferred over NSXPCConnection and passed to +[createKeyFromEndpoint:error:] method.
@property (readonly, nonatomic) NSXPCListenerEndpoint *endpoint;

// Invalidates all connections to this proxy.
- (void)invalidate;

// Creates new SecKey/SecIdentity object which forwards all operations to the target SecKey identified by endpoint. Returned SecKeyRef can be used as long as target SecKeyProxy instance is kept alive.
+ (nullable SecKeyRef)createKeyFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError **)error;
+ (nullable SecIdentityRef)createIdentityFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END

#endif /* !_SECURITY_SECKEYPROXY_H_ */
