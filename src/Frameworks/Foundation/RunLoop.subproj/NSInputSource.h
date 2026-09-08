/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSInputSource_h
#define NSInputSource_h

#import <Foundation/NSObject.h>

/* Abstract: something a run loop can wait on and then fire. */
@interface NSInputSource : NSObject

- (void)fire;

@end

#endif /* NSInputSource_h */
