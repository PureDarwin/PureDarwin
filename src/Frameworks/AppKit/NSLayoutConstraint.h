// NSLayoutConstraint.h - the AppKit side of Auto Layout: priorities and constraint-based view layout
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSView.h>
#import <Foundation/NSLayoutConstraint.h>

@class NSArray, NSString;


APPKIT_EXPORT const NSLayoutPriority NSLayoutPriorityRequired;
APPKIT_EXPORT const NSLayoutPriority NSLayoutPriorityDefaultHigh;
APPKIT_EXPORT const NSLayoutPriority NSLayoutPriorityDefaultLow;



// the views' side: constraints a view holds, laid out by a small solver (equalities between edges, sizes and
// centres of a view and its subviews, required priority only). Views keep their archived frames otherwise
@interface NSView (NSConstraintBasedLayout)
- (NSArray *)constraints;
- (void)addConstraint:(NSLayoutConstraint *)constraint;
- (void)addConstraints:(NSArray *)constraints;
- (void)removeConstraint:(NSLayoutConstraint *)constraint;
- (void)removeConstraints:(NSArray *)constraints;
- (BOOL)translatesAutoresizingMaskIntoConstraints;
- (void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)flag;
- (void)layoutSubtreeIfNeeded;
- (void)setNeedsLayout:(BOOL)flag;
- (void)_pdLayoutConstraints;
@end
