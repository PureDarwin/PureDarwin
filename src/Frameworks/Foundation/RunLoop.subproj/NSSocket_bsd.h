/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSocket_bsd_h
#define NSSocket_bsd_h

#import <Foundation/NSSocket.h>

@interface NSSocket_bsd : NSSocket {
    int _descriptor;
    BOOL _closeOnDealloc;
}

+ (instancetype)socketWithDescriptor:(int)descriptor;

- (instancetype)initWithDescriptor:(int)descriptor;

@end

#endif /* NSSocket_bsd_h */
