// ffsyms_test.m - the Foundation and framework pieces Firefox 158 binds that were missing
#import <Foundation/Foundation.h>
#import <Foundation/NSURLFileTypeMappings.h>
#include <CoreFoundation/CoreFoundation.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dlfcn.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

@protocol FFTestService
- (void)ping:(void (^)(int))reply;
@end

static void constantStrings(void) {
    // linked with CoreFoundation first, so the literal's isa binds from CoreFoundation like XUL's
    CFStringRef cf = CFSTR("hello from CF");
    NSString *ns = (NSString *)cf;
    CHECK(object_getClass(ns) != Nil && class_isMetaClass(object_getClass(object_getClass(ns))), "CFSTR isa is a class");
    CHECK([ns length] == 13, "message to a CFSTR, length %lu", (unsigned long)[ns length]);
    CHECK([@"abc" isEqualToString:@"abc"], "message to an @ literal");
}

static void geometry(void) {
    BOOL (*equalRects)(NSRect, NSRect) = dlsym(RTLD_DEFAULT, "NSEqualRects");
    BOOL (*emptyRect)(NSRect) = dlsym(RTLD_DEFAULT, "NSIsEmptyRect");
    BOOL (*pointInRect)(NSPoint, NSRect) = dlsym(RTLD_DEFAULT, "NSPointInRect");
    CHECK(equalRects && emptyRect && pointInRect, "geometry functions exported");
    if (equalRects && emptyRect && pointInRect) {
        CHECK(equalRects(NSMakeRect(1, 2, 3, 4), NSMakeRect(1, 2, 3, 4)) && !equalRects(NSMakeRect(1, 2, 3, 4), NSMakeRect(1, 2, 3, 5)), "NSEqualRects");
        CHECK(emptyRect(NSMakeRect(0, 0, 0, 5)) && !emptyRect(NSMakeRect(0, 0, 1, 1)), "NSIsEmptyRect");
        CHECK(pointInRect(NSMakePoint(1, 1), NSMakeRect(0, 0, 2, 2)) && !pointInRect(NSMakePoint(2, 1), NSMakeRect(0, 0, 2, 2)), "NSPointInRect");
    }
}

static void processInfo(void) {
    CHECK([[NSProcessInfo processInfo] thermalState] == NSProcessInfoThermalStateNominal, "thermal state nominal");
    CHECK([NSProcessInfoThermalStateDidChangeNotification length] > 0, "thermal notification name");
}

static void progress(void) {
    NSProgress *parent = [NSProgress progressWithTotalUnitCount:10];
    [parent becomeCurrentWithPendingUnitCount:5];
    NSProgress *child = [NSProgress progressWithTotalUnitCount:4];
    [parent resignCurrent];
    [child setCompletedUnitCount:2];
    CHECK(fabs([parent fractionCompleted] - 0.25) < 0.001, "child half done is a quarter of the parent: %f", [parent fractionCompleted]);
    [child setCompletedUnitCount:4];
    CHECK([parent completedUnitCount] == 5, "finished child's units move to the parent: %lld", [parent completedUnitCount]);
    [parent setKind:NSProgressKindFile];
    [parent setFileOperationKind:NSProgressFileOperationKindDownloading];
    [parent setFileURL:[NSURL fileURLWithPath:@"/tmp/x"]];
    CHECK([[parent userInfo] objectForKey:NSProgressFileOperationKindKey] == NSProgressFileOperationKindDownloading, "file operation kind in user info");
    __block int cancelled = 0;
    [parent setCancellationHandler:^{ cancelled++; }];
    [parent cancel];
    CHECK(cancelled == 1 && [parent isCancelled], "cancel runs the handler");
    [parent publish];
    [parent unpublish];
}

static void urlLoading(void) {
    [@"file body" writeToFile:@"/tmp/ffsyms.txt" atomically:NO encoding:NSUTF8StringEncoding error:NULL];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block NSData *body = nil;
    __block NSString *type = nil;
    [[[NSURLSession sharedSession] dataTaskWithRequest:[NSURLRequest requestWithURL:[NSURL fileURLWithPath:@"/tmp/ffsyms.txt"]]
                                    completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        body = [data retain];
        type = [[response MIMEType] retain];
        dispatch_semaphore_signal(done);
    }] resume];
    CHECK(dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) == 0, "file task completes");
    CHECK([body length] == 9 && [type isEqualToString:@"text/plain"], "file data and type: %lu %s", (unsigned long)[body length], [type UTF8String]);
    __block NSInteger code = 0;
    [[[NSURLSession sharedSession] dataTaskWithURL:[NSURL URLWithString:@"https://example.com/"]
                                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        code = [error code];
        dispatch_semaphore_signal(done);
    }] resume];
    dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    CHECK(code == NSURLErrorUnsupportedURL, "network URL fails with NSURLErrorUnsupportedURL: %ld", (long)code);

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]];
    [request setHTTPMethod:@"POST"];
    [request setValue:@"text/plain" forHTTPHeaderField:@"Content-Type"];
    [request addValue:@"gzip" forHTTPHeaderField:@"accept-encoding"];
    [request addValue:@"br" forHTTPHeaderField:@"Accept-Encoding"];
    CHECK([[request valueForHTTPHeaderField:@"content-type"] isEqualToString:@"text/plain"], "header lookup ignores case");
    CHECK([[request valueForHTTPHeaderField:@"Accept-Encoding"] isEqualToString:@"gzip,br"], "repeated header joins");
    NSURLQueryItem *item = [NSURLQueryItem queryItemWithName:@"q" value:@"1"];
    CHECK([[item name] isEqualToString:@"q"] && [item isEqual:[NSURLQueryItem queryItemWithName:@"q" value:@"1"]], "query item");
    CHECK([[[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:@"PNG"] isEqualToString:@"image/png"], "MIME type for an extension");
    CHECK([[[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:@"image/jpeg"] isEqualToString:@"jpg"], "extension for a MIME type");
}

static void appleEvents(void) {
    NSAppleEventDescriptor *event = [NSAppleEventDescriptor appleEventWithEventClass:'GURL' eventID:'GURL'
                                                                    targetDescriptor:nil returnID:-1 transactionID:0];
    [event setParamDescriptor:[NSAppleEventDescriptor descriptorWithString:@"https://mozilla.org/"] forKeyword:'----'];
    CHECK([event eventClass] == 'GURL' && [event eventID] == 'GURL', "event class and id");
    CHECK([[[event paramDescriptorForKeyword:'----'] stringValue] isEqualToString:@"https://mozilla.org/"], "direct parameter string");
    NSAppleEventDescriptor *list = [NSAppleEventDescriptor listDescriptor];
    [list insertDescriptor:[NSAppleEventDescriptor descriptorWithInt32:7] atIndex:0];
    [list insertDescriptor:[NSAppleEventDescriptor descriptorWithBoolean:YES] atIndex:0];
    CHECK([list numberOfItems] == 2 && [[list descriptorAtIndex:1] int32Value] == 7 && [[list descriptorAtIndex:2] booleanValue], "list items");
}

static void xpc(void) {
    NSXPCConnection *connection = [[NSXPCConnection alloc] initWithMachServiceName:@"org.mozilla.nothing" options:0];
    [connection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(FFTestService)]];
    __block NSInteger code = 0;
    __block int invalidated = 0;
    [connection setInvalidationHandler:^{ invalidated = 1; }];
    [connection resume];
    [[connection remoteObjectProxyWithErrorHandler:^(NSError *error) { code = [error code]; }] ping:^(int x) {}];
    CHECK(code == 4099, "proxy reports NSXPCConnectionInvalid: %ld", (long)code);
    usleep(200000);
    CHECK(invalidated, "invalidation handler runs");
    [connection release];
}

static void refcounts(void) {
    NSDate *date = [[NSDate alloc] initWithTimeIntervalSince1970:100];
    CHECK(fabs([date timeIntervalSince1970] - 100) < 0.001, "NSDate initializer");
    CFIndex before = CFGetRetainCount((CFTypeRef)date);
    [date retain];
    CHECK(CFGetRetainCount((CFTypeRef)date) == before + 1, "retain on an NSDate reaches CF");
    [date release];
    [date release];
    NSURL *url = [NSURL URLWithString:@"https://a.b/c"];
    before = CFGetRetainCount((CFTypeRef)url);
    [url retain];
    CHECK(CFGetRetainCount((CFTypeRef)url) == before + 1, "retain on an NSURL reaches CF");
    [url release];
    NSSet *set = [NSSet setWithObject:@"x"];
    before = CFGetRetainCount((CFTypeRef)set);
    [set retain];
    CHECK(CFGetRetainCount((CFTypeRef)set) == before + 1, "retain on an NSSet reaches CF");
    [set release];
}

static void misc(void) {
    NSUserActivity *activity = [[[NSUserActivity alloc] initWithActivityType:NSUserActivityTypeBrowsingWeb] autorelease];
    [activity setWebpageURL:[NSURL URLWithString:@"https://mozilla.org/"]];
    [activity becomeCurrent];
    [activity invalidate];
    CHECK([[activity activityType] isEqualToString:NSUserActivityTypeBrowsingWeb], "user activity");
    NSUserNotificationAction *action = [NSUserNotificationAction actionWithIdentifier:@"a" title:@"A"];
    CHECK([[action title] isEqualToString:@"A"] && [NSUserNotificationDefaultSoundName length], "notification action");
    CHECK(objc_getClass("NSLayoutConstraint") != Nil && dlsym(RTLD_DEFAULT, "OBJC_CLASS_$_NSLayoutConstraint") != NULL, "NSLayoutConstraint in Foundation");
    CHECK([NSSocketPortNameServer sharedInstance] != nil, "socket name server");
}

static void frameworks(void) {
    static const char *const checks[][2] = {
        { "/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices", "OBJC_CLASS_$_ASAuthorizationWebBrowserPublicKeyCredentialManager" },
        { "/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices", "ASAuthorizationPublicKeyCredentialAttestationKindDirect" },
        { "/System/Library/Frameworks/ScreenCaptureKit.framework/ScreenCaptureKit", "OBJC_CLASS_$_SCContentSharingPicker" },
        { "/System/Library/Frameworks/ScreenCaptureKit.framework/ScreenCaptureKit", "SCStreamFrameInfoDirtyRects" },
        { "/System/Library/Frameworks/ServiceManagement.framework/ServiceManagement", "OBJC_CLASS_$_SMAppService" },
        { "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox", "kVTCompressionPropertyKey_ConstantBitRate" },
        { "/System/Library/Frameworks/Accessibility.framework/Accessibility", "OBJC_CLASS_$_AXCustomContent" },
        { "/System/Library/Frameworks/Accessibility.framework/Accessibility", "AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification" },
    };
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        void *image = dlopen(checks[i][0], RTLD_NOW);
        CHECK(image && dlsym(image, checks[i][1]), "%s in %s %s", checks[i][1], strrchr(checks[i][0], '/') + 1, image ? "" : dlerror());
    }
    Class manager = objc_getClass("ASAuthorizationWebBrowserPublicKeyCredentialManager");
    id instance = [[[manager alloc] init] autorelease];
    CHECK(instance && ((NSInteger (*)(id, SEL))objc_msgSend)(instance, sel_getUid("authorizationStateForPlatformCredentials")) == 1, "passkey access is denied");
    Class service = objc_getClass("SMAppService");
    NSError *error = nil;
    id main = [service performSelector:sel_getUid("mainAppService")];
    BOOL registered = ((BOOL (*)(id, SEL, NSError **))objc_msgSend)(main, sel_getUid("registerAndReturnError:"), &error);
    CHECK(!registered && error != nil, "login item registration fails with an error");
    Class content = objc_getClass("AXCustomContent");
    id item = ((id (*)(id, SEL, id, id))objc_msgSend)(content, sel_getUid("customContentWithLabel:value:"), @"Label", @"Value");
    CHECK([[item performSelector:sel_getUid("label")] isEqualToString:@"Label"], "AXCustomContent label");
}

int main(void) {
    @autoreleasepool {
        constantStrings();
        geometry();
        processInfo();
        progress();
        urlLoading();
        appleEvents();
        xpc();
        refcounts();
        misc();
        frameworks();
    }
    DONE("ffsyms");
}
