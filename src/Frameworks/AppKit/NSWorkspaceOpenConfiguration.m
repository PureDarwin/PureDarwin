// NSWorkspaceOpenConfiguration.m - how NSWorkspace opens an application, and the modern NSWorkspace API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSWorkspaceOpenConfiguration.h>
#import <AppKit/NSRunningApplication.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSScreen.h>
#import <Foundation/Foundation.h>
#include <CoreServices/LaunchServices.h>
#include <libproc.h>
#include <unistd.h>

// cocoa error codes this Foundation does not declare
enum {
    PDFileReadUnknownError = 256,
    PDFileReadNoSuchFileError = 260,
    PDFeatureUnsupportedError = 3328,
};

NSString *const NSWorkspaceDesktopImageScalingKey = @"NSWorkspaceDesktopImageScalingKey";
NSString *const NSWorkspaceDesktopImageAllowClippingKey = @"NSWorkspaceDesktopImageAllowClippingKey";
NSString *const NSWorkspaceDesktopImageFillColorKey = @"NSWorkspaceDesktopImageFillColorKey";

// where the Dock keeps the desktop picture, a name in the system folder or a full path
static NSString *const PDDockDomain = @"org.puredarwin.Dock";
static NSString *const PDDesktopPictures = @"/System/Library/Desktop Pictures";

@implementation NSWorkspaceOpenConfiguration

@synthesize promptsUserIfNeeded = _promptsUserIfNeeded, addsToRecentItems = _addsToRecentItems, activates = _activates;
@synthesize hides = _hides, hidesOthers = _hidesOthers, createsNewApplicationInstance = _createsNewApplicationInstance;
@synthesize allowsRunningApplicationSubstitution = _allowsRunningApplicationSubstitution;
@synthesize arguments = _arguments, environment = _environment, appleEvent = _appleEvent;

+ (instancetype)configuration
{
    return [[[self alloc] init] autorelease];
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _promptsUserIfNeeded = YES;
    _addsToRecentItems = YES;
    _activates = YES;
    _allowsRunningApplicationSubstitution = YES;
    return self;
}

- (void)dealloc
{
    [_arguments release];
    [_environment release];
    [_appleEvent release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    NSWorkspaceOpenConfiguration *copy = [[[self class] allocWithZone:zone] init];

    copy.promptsUserIfNeeded = _promptsUserIfNeeded;
    copy.addsToRecentItems = _addsToRecentItems;
    copy.activates = _activates;
    copy.hides = _hides;
    copy.hidesOthers = _hidesOthers;
    copy.createsNewApplicationInstance = _createsNewApplicationInstance;
    copy.allowsRunningApplicationSubstitution = _allowsRunningApplicationSubstitution;
    copy.arguments = _arguments;
    copy.environment = _environment;
    copy.appleEvent = _appleEvent;
    return copy;
}

@end

static NSError *PDWorkspaceError(NSInteger code, NSString *what)
{
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:@{ NSLocalizedDescriptionKey: what }];
}

// the newest process running this bundle, the one an open request just started or reused
static NSRunningApplication *PDRunningApplicationForBundle(NSURL *bundleURL)
{
    NSBundle *bundle = [NSBundle bundleWithURL:bundleURL];
    NSArray *apps = [bundle bundleIdentifier] ? [NSRunningApplication runningApplicationsWithBundleIdentifier:[bundle bundleIdentifier]] : nil;
    NSRunningApplication *newest = nil;

    for (NSRunningApplication *app in apps) {
        if (newest == nil || [app processIdentifier] > [newest processIdentifier])
            newest = app;
    }
    return newest;
}

@implementation NSWorkspace (OpenConfiguration)

- (NSArray *)runningApplications
{
    NSMutableArray *result = [NSMutableArray array];
    int count = proc_listallpids(NULL, 0);
    pid_t *pids;

    if (count <= 0)
        return result;
    pids = calloc((size_t)count + 16, sizeof(pid_t));
    count = proc_listallpids(pids, (int)(((size_t)count + 16) * sizeof(pid_t)));
    for (int i = 0; i < count; i++) {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pids[i]];
        if (app != nil && [app bundleURL] != nil)
            [result addObject:app];
    }
    free(pids);
    return result;
}

// this process when it is active, other processes' activation is not visible from here
- (NSRunningApplication *)frontmostApplication
{
    return [NSApp isActive] ? [NSRunningApplication currentApplication] : nil;
}

- (NSRunningApplication *)menuBarOwningApplication
{
    return [self frontmostApplication];
}

// the user's display preferences, kept as in macOS's universal access domain
- (BOOL)_universalAccessFlag:(NSString *)key
{
    id value = [(id)CFPreferencesCopyAppValue((CFStringRef)key, CFSTR("com.apple.universalaccess")) autorelease];

    return [value respondsToSelector:@selector(boolValue)] && [value boolValue];
}

- (BOOL)accessibilityDisplayShouldIncreaseContrast
{
    return [self _universalAccessFlag:@"increaseContrast"];
}

- (BOOL)accessibilityDisplayShouldDifferentiateWithoutColor
{
    return [self _universalAccessFlag:@"differentiateWithoutColor"];
}

- (BOOL)accessibilityDisplayShouldReduceTransparency
{
    return [self _universalAccessFlag:@"reduceTransparency"];
}

- (BOOL)accessibilityDisplayShouldReduceMotion
{
    return [self _universalAccessFlag:@"reduceMotion"];
}

- (BOOL)accessibilityDisplayShouldInvertColors
{
    return [self _universalAccessFlag:@"whiteOnBlack"];
}

- (BOOL)isVoiceOverEnabled
{
    return NO;
}

- (BOOL)isSwitchControlEnabled
{
    return NO;
}

- (void)openURLs:(NSArray *)urls withApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration
    completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler
{
    NSRunningApplication *app = nil;
    NSError *error = nil;
    OSStatus status;

    if (applicationURL == nil) {
        error = PDWorkspaceError(PDFileReadNoSuchFileError, @"No application was given.");
    } else {
        status = LSOpenApplicationAtURL((CFURLRef)applicationURL, (CFArrayRef)urls, NULL);
        if (status != noErr)
            error = PDWorkspaceError(PDFileReadUnknownError, [NSString stringWithFormat:@"The application could not be opened (%d).", (int)status]);
        else
            app = PDRunningApplicationForBundle(applicationURL);
    }
    if (completionHandler)
        completionHandler(app, error);
}

- (void)openApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration
           completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler
{
    [self openURLs:nil withApplicationAtURL:applicationURL configuration:configuration completionHandler:completionHandler];
}

- (void)openURL:(NSURL *)url configuration:(NSWorkspaceOpenConfiguration *)configuration
    completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler
{
    CFURLRef launched = NULL;
    OSStatus status = LSOpenCFURLRef((CFURLRef)url, &launched);
    NSRunningApplication *app = nil;

    if (status == noErr && launched != NULL)
        app = PDRunningApplicationForBundle((NSURL *)launched);
    if (launched != NULL)
        CFRelease(launched);
    if (completionHandler)
        completionHandler(app, status == noErr ? nil : PDWorkspaceError(PDFileReadUnknownError, @"The URL could not be opened."));
}

- (NSRunningApplication *)launchApplicationAtURL:(NSURL *)url options:(NSUInteger)options configuration:(NSDictionary *)configuration error:(NSError **)error
{
    OSStatus status = LSOpenApplicationAtURL((CFURLRef)url, NULL, NULL);

    if (status != noErr) {
        if (error)
            *error = PDWorkspaceError(PDFileReadUnknownError, @"The application could not be opened.");
        return nil;
    }
    return PDRunningApplicationForBundle(url);
}

- (BOOL)launchAppWithBundleIdentifier:(NSString *)bundleIdentifier options:(NSUInteger)options
    additionalEventParamDescriptor:(id)descriptor launchIdentifier:(NSNumber **)identifier
{
    NSURL *url = [self URLForApplicationWithBundleIdentifier:bundleIdentifier];

    if (identifier)
        *identifier = nil;
    return url != nil && LSOpenApplicationAtURL((CFURLRef)url, NULL, NULL) == noErr;
}

- (BOOL)openURLs:(NSArray *)urls withAppBundleIdentifier:(NSString *)bundleIdentifier options:(NSUInteger)options
    additionalEventParamDescriptor:(id)descriptor launchIdentifiers:(NSArray **)identifiers
{
    NSURL *app = bundleIdentifier ? [self URLForApplicationWithBundleIdentifier:bundleIdentifier] : nil;

    if (identifiers)
        *identifiers = nil;
    if (app != nil)
        return LSOpenApplicationAtURL((CFURLRef)app, (CFArrayRef)urls, NULL) == noErr;
    for (NSURL *url in urls) {
        if (LSOpenCFURLRef((CFURLRef)url, NULL) != noErr)
            return NO;
    }
    return YES;
}

- (NSURL *)URLForApplicationWithBundleIdentifier:(NSString *)bundleIdentifier
{
    NSString *path = [self absolutePathForAppBundleWithIdentifier:bundleIdentifier];

    return path ? [NSURL fileURLWithPath:path] : nil;
}

- (NSURL *)URLForApplicationToOpenURL:(NSURL *)url
{
    return [(NSURL *)LSCopyDefaultApplicationURLForURL((CFURLRef)url, kLSRolesAll, NULL) autorelease];
}

- (NSArray *)URLsForApplicationsToOpenURL:(NSURL *)url
{
    return [(NSArray *)LSCopyApplicationURLsForURL((CFURLRef)url, kLSRolesAll) autorelease] ?: @[];
}

// a UTType or a type identifier string, both answer the identifier
- (NSURL *)URLForApplicationToOpenContentType:(id)contentType
{
    NSString *identifier = [contentType respondsToSelector:@selector(identifier)] ? [contentType identifier] : contentType;

    if (![identifier isKindOfClass:[NSString class]])
        return nil;
    return [(NSURL *)LSCopyDefaultApplicationURLForContentType((CFStringRef)identifier, kLSRolesAll, NULL) autorelease];
}

// LaunchServices here has no setter for default handlers yet, so the request is refused
- (void)setDefaultApplicationAtURL:(NSURL *)applicationURL toOpenContentType:(id)contentType completionHandler:(void (^)(NSError *error))completionHandler
{
    if (completionHandler)
        completionHandler(PDWorkspaceError(PDFeatureUnsupportedError, @"The default application can't be changed."));
}

- (void)setDefaultApplicationAtURL:(NSURL *)applicationURL toOpenURLsWithScheme:(NSString *)urlScheme completionHandler:(void (^)(NSError *error))completionHandler
{
    if (completionHandler)
        completionHandler(PDWorkspaceError(PDFeatureUnsupportedError, @"The default application can't be changed."));
}

// the user's trash, with a numbered name when the item's name is already taken there
- (void)recycleURLs:(NSArray *)urls completionHandler:(void (^)(NSDictionary *newURLs, NSError *error))handler
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *trash = [NSHomeDirectory() stringByAppendingPathComponent:@".Trash"];
    NSMutableDictionary *moved = [NSMutableDictionary dictionary];
    NSError *error = nil;

    [fm createDirectoryAtPath:trash withIntermediateDirectories:YES attributes:nil error:NULL];
    for (NSURL *url in urls) {
        NSString *name = [[url path] lastPathComponent];
        NSString *target = [trash stringByAppendingPathComponent:name];

        for (int n = 2; [fm fileExistsAtPath:target]; n++) {
            NSString *base = [name stringByDeletingPathExtension], *ext = [name pathExtension];
            NSString *numbered = [NSString stringWithFormat:@"%@ %d", base, n];
            target = [trash stringByAppendingPathComponent:[ext length] ? [numbered stringByAppendingPathExtension:ext] : numbered];
        }
        if ([fm moveItemAtPath:[url path] toPath:target error:&error])
            [moved setObject:[NSURL fileURLWithPath:target] forKey:url];
        else
            break;
    }
    if (handler)
        handler(moved, error);
}

- (NSURL *)desktopImageURLForScreen:(NSScreen *)screen
{
    NSString *name;

    CFPreferencesAppSynchronize((CFStringRef)PDDockDomain);
    name = [(id)CFPreferencesCopyAppValue(CFSTR("DesktopPicture"), (CFStringRef)PDDockDomain) autorelease] ?: @"Sunset";
    if (![name isAbsolutePath])
        name = [[PDDesktopPictures stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"jpg"];
    return [NSURL fileURLWithPath:name];
}

- (NSDictionary *)desktopImageOptionsForScreen:(NSScreen *)screen
{
    return @{ NSWorkspaceDesktopImageScalingKey: @(3), NSWorkspaceDesktopImageAllowClippingKey: @YES };
}

// the Dock rereads its preferences every second and redraws the desktop from them
- (BOOL)setDesktopImageURL:(NSURL *)url forScreen:(NSScreen *)screen options:(NSDictionary *)options error:(NSError **)error
{
    NSString *path = [url path];

    if (![url isFileURL] || ![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        if (error)
            *error = PDWorkspaceError(PDFileReadNoSuchFileError, @"The picture does not exist.");
        return NO;
    }
    if ([[path stringByDeletingLastPathComponent] isEqualToString:PDDesktopPictures] && [[path pathExtension] isEqualToString:@"jpg"])
        path = [[path lastPathComponent] stringByDeletingPathExtension];
    CFPreferencesSetAppValue(CFSTR("DesktopPicture"), (CFStringRef)path, (CFStringRef)PDDockDomain);
    return CFPreferencesAppSynchronize((CFStringRef)PDDockDomain);
}

@end
