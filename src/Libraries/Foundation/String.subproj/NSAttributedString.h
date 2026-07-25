/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSAttributedString_h
#define NSAttributedString_h

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRange.h>

// Just enough for CoreFoundation's CFAttributedString.c to have a defined class to extend
@interface NSAttributedString : NSObject

- (NSString *)string;

@end

#endif /* NSAttributedString_h */
