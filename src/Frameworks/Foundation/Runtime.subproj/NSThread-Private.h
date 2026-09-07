/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSThread_Private_h
#define NSThread_Private_h

#import <Foundation/NSThread.h>

FOUNDATION_EXPORT NSThread *NSCurrentThread(void);

@interface NSThread (Private)
- (NSMutableDictionary *)sharedDictionary;
@end

#endif /* NSThread_Private_h */
