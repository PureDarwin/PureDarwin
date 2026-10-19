// NSSharingService.h - sharing items with other services, none of which this system offers yet
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

@class NSString, NSArray, NSImage, NSURL;

typedef NSString *NSSharingServiceName;

APPKIT_EXPORT NSString *const NSSharingServiceNameComposeEmail;
APPKIT_EXPORT NSString *const NSSharingServiceNameComposeMessage;
APPKIT_EXPORT NSString *const NSSharingServiceNameSendViaAirDrop;
APPKIT_EXPORT NSString *const NSSharingServiceNameAddToSafariReadingList;
APPKIT_EXPORT NSString *const NSSharingServiceNameCloudSharing;

@interface NSSharingService : NSObject
+ (NSArray *)sharingServicesForItems:(NSArray *)items;
+ (NSSharingService *)sharingServiceNamed:(NSString *)serviceName;
- (instancetype)initWithTitle:(NSString *)title image:(NSImage *)image alternateImage:(NSImage *)alternateImage handler:(void (^)(void))block;
@property(assign) id delegate;
@property(readonly, copy) NSString *title;
@property(readonly, retain) NSImage *image;
@property(readonly, retain) NSImage *alternateImage;
@property(copy) NSString *menuItemTitle;
@property(copy) NSArray *recipients;
@property(copy) NSString *subject;
@property(readonly, copy) NSString *messageBody;
@property(readonly, copy) NSURL *permanentLink;
@property(readonly, copy) NSArray *accountName;
@property(readonly, copy) NSArray *attachmentFileURLs;
- (BOOL)canPerformWithItems:(NSArray *)items;
- (void)performWithItems:(NSArray *)items;
@end
