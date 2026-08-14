/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utilities/SecFileLocations.h>
#include <utilities/debugging.h>
#include <Security/SecItemInternal.h>

#include "OTATrustUtilities.h"
#include "trustdFileLocations.h"

#if TARGET_OS_OSX
#include <membership.h>
#endif

#if !TARGET_OS_SIMULATOR && (TARGET_OS_IPHONE || TARGET_CPU_ARM64)
#include <System/sys/content_protection.h>
#endif

#define ROOT_ACCOUNT 0

bool SecOTAPKIIsSystemTrustd() {
    static bool result = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#ifdef NO_SERVER
        // Test app running as trustd
        result = true;
#else // !NO_SERVER
        if (getuid() == TRUSTD_ROLE_ACCOUNT ||
            (getuid() == ROOT_ACCOUNT && gTrustd)) { // Test app running as trustd
            result = true;
        }
#endif // !NO_SERVER
    });
    return result;
}

/*
 * trustd's data vault hierarchy:
 *     * /var/protected/trustd - requires an entitlement to write but is available to read by all processes
 *                               used for non-privacy sensitive system data (e.g. Valid DB, pinning DB, MobileAsset files)
 *     * /var/protected/trustd/private - requires an entitlement to read and write, trustds running as every user can rwx directory
 *                                       used for sensitive system data (e.g. administrative configuration and admin trust store)
 *     * /var/protected/trustd/private/<UUID> - inherits the entitlement requirement, but uses unix permissions to restrict rwx to
 *                                              the trustd running as the user corresponding to the uuid. Used for sensitive user
 *                                              data (e.g. OCSP and CA issuer caches, user trust store)
 */

CFURLRef SecCopyURLForFileInProtectedTrustdDirectory(CFStringRef fileName)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WithPathInProtectedDirectory(CFSTR("trustd"), ^(const char *path) {
            mode_t permissions = 0777; // Non-system trustd's create directory with expansive permissions
            int ret = mkpath_np(path, permissions);
            if (!(ret == 0 || ret ==  EEXIST)) {
                secerror("could not create path: %s (%s)", path, strerror(ret));
            }
            if (SecOTAPKIIsSystemTrustd()) { // System trustd fixes them up since only it should be writing
                permissions = 0755;
                uid_t currentUid = getuid();
                chown(path, currentUid, currentUid);
                chmod(path, permissions);
            }
        });
    });
    NSString *path = @"trustd/";
    if (fileName) {
        path = [NSString stringWithFormat:@"trustd/%@", fileName];
    }
    return SecCopyURLForFileInProtectedDirectory((__bridge CFStringRef)path);
}

CFURLRef SecCopyURLForFileInPrivateTrustdDirectory(CFStringRef fileName)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WithPathInProtectedTrustdDirectory(CFSTR("private"), ^(const char *path) {
            mode_t permissions = 0777;
            int ret = mkpath_np(path, permissions);
            if (!(ret == 0 || ret ==  EEXIST)) {
                secerror("could not create path: %s (%s)", path, strerror(ret));
            }
            chmod(path, permissions);
        });
    });
    NSString *path = @"private/";
    if (fileName) {
        path = [NSString stringWithFormat:@"private/%@", fileName];
    }
    return SecCopyURLForFileInProtectedTrustdDirectory((__bridge CFStringRef)path);
}

CFURLRef SecCopyURLForFileInPrivateUserTrustdDirectory(CFStringRef fileName)
{
#if TARGET_OS_OSX
    uid_t euid = geteuid();
    uuid_t currentUserUuid;
    int ret = mbr_uid_to_uuid(euid, currentUserUuid);
    if (ret != 0) {
        secerror("failed to get UUID for user(%d) - %d", euid, ret);
        return SecCopyURLForFileInPrivateTrustdDirectory(fileName);
    }
    NSUUID *userUuid = [[NSUUID alloc] initWithUUIDBytes:currentUserUuid];
    NSString *directory = [NSString stringWithFormat:@"/%@",[userUuid UUIDString]];
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WithPathInPrivateTrustdDirectory((__bridge  CFStringRef)directory, ^(const char *path) {
            mode_t permissions = 0700;
            int mkpath_ret = mkpath_np(path, permissions);
            if (!(mkpath_ret == 0 || mkpath_ret ==  EEXIST)) {
                secerror("could not create path: %s (%s)", path, strerror(ret));
            }
            chmod(path, permissions);
        });
    });
    NSString *path = directory;
    if (fileName) {
        path = [NSString stringWithFormat:@"%@/%@", directory, fileName];
    }
    return SecCopyURLForFileInPrivateTrustdDirectory((__bridge CFStringRef)path);
#else
    return SecCopyURLForFileInPrivateTrustdDirectory(fileName);
#endif
}

CFURLRef SecCopyURLForFileInRevocationInfoDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInProtectedTrustdDirectory(fileName);
}

void WithPathInRevocationInfoDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInRevocationInfoDirectory(fileName), operation);
}

void WithPathInProtectedTrustdDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInProtectedTrustdDirectory(fileName), operation);
}

void WithPathInPrivateTrustdDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInPrivateTrustdDirectory(fileName), operation);
}

void WithPathInPrivateUserTrustdDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInPrivateUserTrustdDirectory(fileName), operation);
}

static BOOL needToFixFilePermissions(void) {
    __block BOOL result = NO;
#if TARGET_OS_SIMULATOR
    // Simulators don't upgrade, so no need to fix file permissions
#elif TARGET_OS_IPHONE
    /* Did trust store already migrate */
    WithPathInPrivateTrustdDirectory(CFSTR("TrustStore.sqlite3"), ^(const char *utf8String) {
        struct stat sb;
        int ret = stat(utf8String, &sb);
        if (ret != 0) {
            secinfo("helper", "failed to stat TrustStore: %s", strerror(errno));
            result = YES;
        }
    });
    /* Can we write to the analytics directory */
    WithPathInKeychainDirectory(CFSTR("Analytics"), ^(const char *utf8String) {
        struct stat sb;
        int ret = stat(utf8String, &sb);
        if (ret != 0) {
            // Check errno. Missing directory or file errors do not require any fixes
            if (errno != ENOTDIR && errno != ENOENT) {
                secinfo("helper", "failed to stat Analytics dir: %s", strerror(errno));
                result = YES;
            }
        } else {
            // Check that all users have rwx for Analytics dir
            if ((sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != (S_IRWXU | S_IRWXG | S_IRWXO)) {
                secinfo("helper", "wrong permissions on Analytics dir: %d", sb.st_mode);
                result = YES;
            }
        }
    });
#else
    /* Can we read/write Valid file? */
    WithPathInRevocationInfoDirectory(CFSTR("valid.sqlite3"), ^(const char *utf8String) {
        struct stat sb;
        int ret = stat(utf8String, &sb);
        if (ret != 0) {
            // Check errno. Missing directory or file errors do not require any fixes
            if (errno != ENOTDIR && errno != ENOENT) {
                secinfo("helper", "failed to stat valid db: %s", strerror(errno));
                result = YES;
            }
        } else {
            // Successful call. Check if the file owner has been changed
            if (sb.st_uid != TRUSTD_ROLE_ACCOUNT) {
                secinfo("helper", "wrong owner for valid db");
                result = YES;
            }
        }
    });
#endif
    return result;
}

static NSXPCConnection* getConnection()
{
#if TARGET_OS_OSX
    NSString *xpcServiceName = @TrustdFileHelperXPCServiceName;
#else
    NSString *xpcServiceName = @"com.apple.securityuploadd";
#endif
    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:xpcServiceName options:0];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(TrustdFileHelper_protocol)];
    [connection resume];
    return connection;
}

void FixTrustdFilePermissions(void)
{
#if TARGET_OS_OSX
    const NSString *processName = @"trustdFileHelper";
#else
    const NSString *processName = @"securityuploadd";
#endif
    @autoreleasepool {
        if (!needToFixFilePermissions()) {
            secinfo("helper", "trustd file permissions already fixed. skipping trustdFileHelper call.");
            return;
        }
        @try {
            NSXPCConnection *connection = getConnection();
            if (!connection) {
                secerror("failed to ceate %{public}@ connection", processName);
            } else {
                id<TrustdFileHelper_protocol> protocol = [connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
                    secerror("failed to talk to %{public}@: %@", processName, error);
                }];
                if (protocol) {
                    secdebug("ipc", "asking the file helper to fix our files");
                    [protocol fixFiles:^(BOOL result, NSError *error) {
                        if (!result) {
                            secerror("%{public}@ failed to fix our files: %@", processName, error);
                        } else {
                            secdebug("ipc", "file helper successfully completed fixes");
                        }
                    }];
                }
            }
        }
        @catch(id anException) {
            secerror("failed to fix files; caught exception: %@", anException);
        }
    }
}

bool TrustdChangeFileProtectionToClassD(const char *filename, CFErrorRef *error) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    bool result = true;
    int file_fd = open(filename, O_RDONLY);
    if (file_fd) {
        int retval = fcntl(file_fd, F_SETPROTECTIONCLASS, PROTECTION_CLASS_D);
        if (retval < 0) {
            NSError *localError = [NSError errorWithDomain:NSPOSIXErrorDomain
                                                      code:errno
                                                  userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to change protection class of %s: %s", filename, strerror(errno)]}];
            if (error && !*error) {
                *error = (CFErrorRef)CFBridgingRetain(localError);
            }
            result = false;
        }
        close(file_fd);
    } else {
        NSError *localError = [NSError errorWithDomain:NSPOSIXErrorDomain
                                                  code:errno
                                              userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to open file for protection class change %s: %s", filename, strerror(errno)]}];
        if (error && !*error) {
            *error = (CFErrorRef)CFBridgingRetain(localError);
        }
        result = false;
    }
    return result;
#else // !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
    return true;
#endif
}

@implementation NSDictionary (trustdAdditions)
- (BOOL)writeToClassDURL:(NSURL *)url permissions:(mode_t)permissions error:(NSError **)error {
    if ([self writeToURL:url error:error]) {
        CFErrorRef localError = NULL;
        if (!TrustdChangeFileProtectionToClassD([url fileSystemRepresentation], &localError)) {
            if (error) {
                *error = CFBridgingRelease(localError);
            } else {
                CFReleaseNull(error);
            }
            return NO;
        }
        int ret = chmod([url fileSystemRepresentation], permissions);
        if (!(ret == 0)) {
            int localErrno = errno;
            secerror("failed to change permissions of %s: %s", [url fileSystemRepresentation], strerror(localErrno));
            if (error) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:localErrno
                                         userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to change permissions of %s: %s", [url fileSystemRepresentation], strerror(localErrno)]}];
            }
            return NO;
        }
        return YES;
    }
    return NO;
}
@end
