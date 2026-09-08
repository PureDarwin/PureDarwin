/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * AppKit imports <AppKit/NSRaise.h> in ~110 sources, but the file was missing
 * from the tree. The macros themselves belong to Foundation, so this is the
 * shim that lets those imports resolve.
 */

#ifndef APPKIT_NSRAISE_H
#define APPKIT_NSRAISE_H

#import <Foundation/NSRaise.h>

#endif /* APPKIT_NSRAISE_H */
