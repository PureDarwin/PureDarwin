/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

/*!
 @header SecPinningDb
 The functions in SecPinningDb.h provide an interface to look up
 pinning rules based on hostname.
 */

#ifndef _SECURITY_SECPINNINGDB_H_
#define _SECURITY_SECPINNINGDB_H_

#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecDb.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

extern const uint64_t PinningDbSchemaVersion;

extern const CFStringRef kSecPinningDbKeyHostname;
extern const CFStringRef kSecPinningDbKeyPolicyName;
extern const CFStringRef kSecPinningDbKeyRules;
extern const CFStringRef kSecPinningDbKeyTransparentConnection;

CFDictionaryRef _Nullable SecPinningDbCopyMatching(CFDictionaryRef _Nonnull query);
void SecPinningDbInitialize(void);

#if __OBJC__
#if !TARGET_OS_BRIDGE
/* Updating the pinning DB isn't supported on BridgeOS because we treat the disk as read-only. */
bool SecPinningDbUpdateFromURL(NSURL *url, NSError **error);
#endif

@interface SecPinningDb : NSObject
@property (assign) SecDbRef db;
+ (NSURL *)pinningDbPath;
- (NSNumber *)getContentVersion:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error;
- (NSNumber *)getSchemaVersion:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error;
- (BOOL) installDbFromURL:(NSURL *)localURL error:(NSError **)nserror;
@end
#endif // __OBJC__

CFNumberRef SecPinningDbCopyContentVersion(void);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS


#endif /* _SECURITY_SECPINNINGDB_H_ */
