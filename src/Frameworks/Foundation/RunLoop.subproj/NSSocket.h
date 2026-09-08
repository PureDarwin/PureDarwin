/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSocket_h
#define NSSocket_h

#import <Foundation/NSObject.h>

/* Abstract: a handle the run loop can select() on. */
@interface NSSocket : NSObject

- (int)fileDescriptor;
- (void)close;

@end

#endif /* NSSocket_h */
