//
//  SecXPCUtils.m
//  Security
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecTask.h>
#import <Security/SecEntitlements.h>
#import <objc/objc-class.h>
#import <libproc.h>
#import <sys/proc.h>
#import <bsm/libbsm.h>
#import "SecXPCUtils.h"

@interface SecXPCUtils : NSObject
+ (BOOL)clientCanEditPreferenceOwnership;
+ (CFStringRef)copyApplicationIdentifier;
@end

@implementation SecXPCUtils

// IMPORTANT: These methods are designed to identify the calling process
// for applications which do not rely solely on that identification as a
// security boundary. When there is no NSXPCConnection, the SecTask check
// is made by the calling process on itself. As such, these identifiers can
// only be considered advisory.

+ (BOOL)clientCanEditPreferenceOwnership
{
    NSXPCConnection* connection = [NSXPCConnection currentConnection];
    if (connection) {
        NSArray *accessGroups = [connection valueForEntitlement:(__bridge NSString*)kSecEntitlementKeychainAccessGroups];
        if (accessGroups && [accessGroups isMemberOfClass:[NSArray class]] && [accessGroups containsObject:@"*"]) {
            return YES;
        }
    } else {
        SecTaskRef task = SecTaskCreateFromSelf(NULL);
        if (task) {
            CFTypeRef entitlementValue = SecTaskCopyValueForEntitlement(task,
                kSecEntitlementKeychainAccessGroups, NULL);
            CFRelease(task);
            if (entitlementValue) {
                BOOL result = NO;
                if (CFGetTypeID(entitlementValue) == CFArrayGetTypeID() &&
                    [(__bridge NSArray*)entitlementValue containsObject:@"*"]) {
                        result = YES;
                }
                CFRelease(entitlementValue);
                return result;
            }
        }
    }
    return NO;
}

+ (CFStringRef)copySigningIdentifier:(NSXPCConnection*)connection
{
    CFStringRef result = NULL;
    SecTaskRef task = NULL;
    if (connection) {
        task = SecTaskCreateWithAuditToken(NULL, [connection auditToken]);
    } else {
        task = SecTaskCreateFromSelf(NULL);
    }
    if (task) {
        result = SecTaskCopySigningIdentifier(task, NULL);
        CFRelease(task);
    }
    return result;
}

+ (CFStringRef)copyApplicationIdentifierFromSelf
{
    CFStringRef result = NULL;
    SecTaskRef task = SecTaskCreateFromSelf(NULL);
    if (task) {
        CFTypeRef entitlementValue = SecTaskCopyValueForEntitlement(task,
            kSecEntitlementBasicApplicationIdentifier, NULL);
        if (!entitlementValue) {
            entitlementValue = SecTaskCopyValueForEntitlement(task,
                kSecEntitlementAppleApplicationIdentifier, NULL);
        }
        CFRelease(task);
        if (entitlementValue) {
            if (CFGetTypeID(entitlementValue) == CFStringGetTypeID()) {
                result = entitlementValue;
            } else {
                CFRelease(entitlementValue);
            }
        }
    }
    if (!result) {
        result = [SecXPCUtils copySigningIdentifier:nil];
    }
    return result;
}

+ (CFStringRef)copyApplicationIdentifierFromConnection:(NSXPCConnection*)connection
{
    CFStringRef result = NULL;
    NSString* identifier = [connection valueForEntitlement:(__bridge NSString*)kSecEntitlementBasicApplicationIdentifier];
    if (!identifier) {
        identifier = [connection valueForEntitlement:(__bridge NSString*)kSecEntitlementAppleApplicationIdentifier];
    }
    if (identifier && [identifier isMemberOfClass:[NSString class]]) {
        result = CFStringCreateCopy(NULL, (__bridge CFStringRef)identifier);
    }
    if (!result) {
        result = [SecXPCUtils copySigningIdentifier:connection];
    }
    return result;
}

+ (CFStringRef)copyApplicationIdentifier
{
    NSXPCConnection* connection = [NSXPCConnection currentConnection];
    if (connection) {
        return [SecXPCUtils copyApplicationIdentifierFromConnection:connection];
    }
    return [SecXPCUtils copyApplicationIdentifierFromSelf];
}

@end

Boolean SecXPCClientCanEditPreferenceOwnership(void) {
    Boolean result = false;
    @autoreleasepool {
        if ([SecXPCUtils clientCanEditPreferenceOwnership] == YES) {
            result = true;
        }
    }
    return result;
}

CFStringRef SecXPCCopyClientApplicationIdentifier(void)
{
    CFStringRef appIdStr = NULL;
    @autoreleasepool {
        appIdStr = [SecXPCUtils copyApplicationIdentifier];
    }
    return appIdStr;
}
