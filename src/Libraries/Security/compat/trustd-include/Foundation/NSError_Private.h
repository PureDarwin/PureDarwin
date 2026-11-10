// NSError_Private.h - Foundation's private NSError header, for the one SPI trustd's util.m calls
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSError.h>

@interface NSError (PDPrivate)
+ (void)_setFileNameLocalizationEnabled:(BOOL)enabled;
@end
