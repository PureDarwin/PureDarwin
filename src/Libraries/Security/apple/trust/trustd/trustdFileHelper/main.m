//
//  main.m
//  trustdFileHelper
//
//  Copyright © 2020 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Foundation/NSError_Private.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecTask.h>

#import <xpc/private.h>
#import <xpc/xpc.h>
#import <xpc/activity_private.h>

#import <dirhelper_priv.h>
#include <sandbox.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>

#include "trust/trustd/trustdFileLocations.h"
#include "trust/trustd/trustdFileHelper/trustdFileHelper.h"

@interface ServiceDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation ServiceDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    if(![[newConnection valueForEntitlement:@"com.apple.private.trustd.FileHelp"] boolValue]) {
        SecTaskRef clientTask = SecTaskCreateWithAuditToken(NULL, [newConnection auditToken]);
        secerror("rejecting client %@ due to lack of entitlement", clientTask);
        CFReleaseNull(clientTask);
        return NO;
    }

    secdebug("ipc", "opening connection for %d", [newConnection processIdentifier]);
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(TrustdFileHelper_protocol)];
    TrustdFileHelper *exportedObject = [[TrustdFileHelper alloc] init];
    newConnection.exportedObject = exportedObject;
    [newConnection resume];
    return YES;
}

@end

static void enter_sandbox(void) {
    char buf[PATH_MAX] = "";

    if (!_set_user_dir_suffix("com.apple.trustd") ||
        confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *tempdir = realpath(buf, NULL);
    if (tempdir == NULL) {
        secerror("failed to resolve temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (confstr(_CS_DARWIN_USER_CACHE_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *cachedir = realpath(buf, NULL);
    if (cachedir == NULL) {
        secerror("failed to resolve cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    const char *parameters[] = {
        "_TMPDIR", tempdir,
        "_DARWIN_CACHE_DIR", cachedir,
        NULL
    };

    char *sberror = NULL;
    if (sandbox_init_with_parameters("com.apple.trustdFileHelper", SANDBOX_NAMED, parameters, &sberror) != 0) {
        secerror("Failed to enter trustdFileHelper sandbox: %{public}s", sberror);
        exit(EXIT_FAILURE);
    }

    free(tempdir);
    free(cachedir);
}

int
main(int argc, const char *argv[])
{
    [NSError _setFileNameLocalizationEnabled:NO];
    enter_sandbox();

    static NSXPCListener *listener = nil;

    ServiceDelegate *delegate = [ServiceDelegate new];
    listener = [[NSXPCListener alloc] initWithMachServiceName:@TrustdFileHelperXPCServiceName];
    listener.delegate = delegate;

    // We're always launched in response to client activity and don't want to sit around idle.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5ull * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        secnotice("lifecycle", "will exit when clean");
        xpc_transaction_exit_clean();
    });

    [listener resume];
    secdebug("ipc", "trustdFileHelper accepting work");

    dispatch_main();
}
