// NSServicesMenu.m - the private Services menu classes applications look up and hook to filter entries
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

@class NSMenu;

// one service a provider offers, PD has no service providers so none are ever made
@interface _NSServiceEntry : NSObject
- (NSString *)bundleIdentifier;
@end

@implementation _NSServiceEntry
- (NSString *)bundleIdentifier {
    return nil;
}
@end

// fills the Services menu from the entries that apply, with no providers there is nothing to add
@interface _NSServicesMenuUpdater : NSObject
- (void)populateMenu:(NSMenu *)menu withServiceEntries:(NSArray *)entries forDisplay:(BOOL)display;
@end

@implementation _NSServicesMenuUpdater
- (void)populateMenu:(NSMenu *)menu withServiceEntries:(NSArray *)entries forDisplay:(BOOL)display {
    (void)menu;
    (void)entries;
    (void)display;
}
@end
