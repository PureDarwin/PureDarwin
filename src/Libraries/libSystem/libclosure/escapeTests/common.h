/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import <Foundation/Foundation.h>
#import <Block.h>

#define HASBYREF 1
#if HASBYREF
#define BYREF __block
#else
#define BYREF
#endif

typedef void (^vv)(void);

// do we have a non-gc escaping compiler?
#define FARIBORZ 1

extern void lastUse(int param);

