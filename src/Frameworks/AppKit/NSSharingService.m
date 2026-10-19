// NSSharingService.m - sharing items with other services, none of which this system offers yet
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSSharingService.h>
#import <AppKit/NSImage.h>
#import <Foundation/Foundation.h>

NSString *const NSSharingServiceNameComposeEmail = @"com.apple.share.Mail.compose";
NSString *const NSSharingServiceNameComposeMessage = @"com.apple.messages.ShareExtension";
NSString *const NSSharingServiceNameSendViaAirDrop = @"com.apple.share.AirDrop.send";
NSString *const NSSharingServiceNameAddToSafariReadingList = @"com.apple.share.System.add-to-safari-reading-list";
NSString *const NSSharingServiceNameCloudSharing = @"com.apple.share.CloudSharing";

@implementation NSSharingService {
    void (^_handler)(void);
}

@synthesize delegate = _delegate, title = _title, image = _image, alternateImage = _alternateImage;
@synthesize menuItemTitle = _menuItemTitle, recipients = _recipients, subject = _subject;

// no share extensions are installed, so no named service exists and items have none to go to
+ (NSArray *)sharingServicesForItems:(NSArray *)items
{
    return @[];
}

+ (NSSharingService *)sharingServiceNamed:(NSString *)serviceName
{
    return nil;
}

- (instancetype)initWithTitle:(NSString *)title image:(NSImage *)image alternateImage:(NSImage *)alternateImage handler:(void (^)(void))block
{
    if ((self = [super init]) == nil)
        return nil;
    _title = [title copy];
    _menuItemTitle = [title copy];
    _image = [image retain];
    _alternateImage = [alternateImage retain];
    _handler = [block copy];
    return self;
}

- (void)dealloc
{
    [_title release];
    [_menuItemTitle release];
    [_image release];
    [_alternateImage release];
    [_recipients release];
    [_subject release];
    [_handler release];
    [super dealloc];
}

- (NSString *)messageBody
{
    return nil;
}

- (NSURL *)permanentLink
{
    return nil;
}

- (NSArray *)accountName
{
    return nil;
}

- (NSArray *)attachmentFileURLs
{
    return nil;
}

- (BOOL)canPerformWithItems:(NSArray *)items
{
    return _handler != nil;
}

// a service made with a handler runs it, as on macOS
- (void)performWithItems:(NSArray *)items
{
    if (_handler)
        _handler();
}

@end
