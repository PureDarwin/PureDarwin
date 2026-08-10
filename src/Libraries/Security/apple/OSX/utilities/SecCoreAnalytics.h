/*
* Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>

void SecCoreAnalyticsSendValue(CFStringRef _Nonnull eventName, int64_t value);
void SecCoreAnalyticsSendKernEntropyHealth(void);

#if __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString* const SecCoreAnalyticsValue;

@interface SecCoreAnalytics : NSObject

+ (void)sendEvent:(NSString*) eventName event:(NSDictionary<NSString*,NSObject*> *)event;
+ (void)sendEventLazy:(NSString*) eventName builder:(NSDictionary<NSString*,NSObject*>* (^)(void))builder;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBCJ__ */
