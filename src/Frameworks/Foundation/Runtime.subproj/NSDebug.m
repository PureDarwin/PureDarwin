/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDebug.h>
#include <stdlib.h>

/* Seeded from the environment on first use, matching the NSDebug knobs the
 * ported ObjC frameworks expect. */
BOOL NSDebugEnabled = NO;
BOOL NSZombieEnabled = NO;
BOOL NSDeallocateZombies = NO;
BOOL NSKeepAllocationStatistics = NO;

__attribute__((constructor))
static void _NSDebugInitialize(void) {
    NSDebugEnabled = getenv("NSDebugEnabled") != NULL;
    NSZombieEnabled = getenv("NSZombieEnabled") != NULL;
    NSDeallocateZombies = getenv("NSDeallocateZombies") != NULL;
    NSKeepAllocationStatistics = getenv("NSKeepAllocationStatistics") != NULL;
}
