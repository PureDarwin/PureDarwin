/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPipe_h
#define NSPipe_h

#import <Foundation/NSObject.h>

@class NSFileHandle;

@interface NSPipe : NSObject {
    NSFileHandle *_readHandle;
    NSFileHandle *_writeHandle;
}

+ (instancetype)pipe;

- (NSFileHandle *)fileHandleForReading;
- (NSFileHandle *)fileHandleForWriting;

@end

#endif /* NSPipe_h */
