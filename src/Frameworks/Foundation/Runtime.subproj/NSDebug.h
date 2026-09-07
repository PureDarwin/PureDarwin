/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDebug_h
#define NSDebug_h

#import <Foundation/NSObjCRuntime.h>

FOUNDATION_EXPORT BOOL NSDebugEnabled;
FOUNDATION_EXPORT BOOL NSZombieEnabled;
FOUNDATION_EXPORT BOOL NSDeallocateZombies;
FOUNDATION_EXPORT BOOL NSKeepAllocationStatistics;

#endif /* NSDebug_h */
