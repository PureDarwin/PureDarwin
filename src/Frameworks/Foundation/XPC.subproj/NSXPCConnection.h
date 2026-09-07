/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * NSXPCConnection is not implemented: PureDarwin has libxpc but not the
 * Foundation object-proxy layer on top of it. The declarations exist so that
 * headers naming these types compile; anything that actually messages them
 * will fail to link, which is the honest outcome until the layer exists.
 */
#ifndef NSXPCConnection_h
#define NSXPCConnection_h

#import <Foundation/NSObject.h>

@class NSXPCListener;
@class NSXPCListenerEndpoint;

@interface NSXPCInterface : NSObject
@end

@interface NSXPCConnection : NSObject
@end

@protocol NSXPCProxyCreating
@end

#endif /* NSXPCConnection_h */
