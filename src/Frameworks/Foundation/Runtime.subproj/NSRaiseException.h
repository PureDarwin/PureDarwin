/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRaiseException_h
#define NSRaiseException_h

#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>

/* Raise naming the object and selector that failed, then the caller's reason.
 * Cocotron spells this as a function taking self and _cmd explicitly. */
#define NSRaiseException(name, obj, sel, fmt, ...) \
    [NSException raise:(name) \
                format:@"-[%@ %s] " fmt, \
                       NSStringFromClass([(obj) class]), sel_getName(sel), ##__VA_ARGS__]

#endif /* NSRaiseException_h */
