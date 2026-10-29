/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCFString_h
#define NSCFString_h

#import <Foundation/NSString.h>

// a mutable string class as on macOS, so categories on NSMutableString reach every CF string
@interface NSCFString : NSMutableString
@end

#endif /* NSCFString_h */
