/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <TargetConditionals.h>

#if TARGET_OS_OSX

#include <CoreFoundation/CoreFoundation.h>
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>

#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include "iOSforOSX.h"
#include <pwd.h>
#include <unistd.h>

#include ".././libsecurity_keychain/lib/SecBase64P.c"

CFURLRef SecCopyKeychainDirectoryFile(CFStringRef file)
{
    struct passwd *passwd = getpwuid(getuid());
    if (!passwd)
        return NULL;
    
    CFURLRef pathURL = NULL;
    CFURLRef fileURL = NULL;
    CFStringRef home = NULL;
    CFStringRef filePath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/%@"), "Library/Keychains", file);
    require(filePath, xit);
    
    if (passwd->pw_dir)
        home = CFStringCreateWithCString(NULL, passwd->pw_dir, kCFStringEncodingUTF8);

    pathURL = CFURLCreateWithFileSystemPath(NULL, home?home:CFSTR("/"), kCFURLPOSIXPathStyle, true);
    if (pathURL)
        fileURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, pathURL, filePath, false);

xit:
    CFReleaseSafe(filePath);
    CFReleaseSafe(pathURL);
    CFReleaseSafe(home);
    return fileURL;
}

// XXX: do we still need this?  see securityd_files?
CFURLRef PortableCFCopyHomeDirectoryURL(void)
{
    char *path = getenv("HOME");
    if (!path) {
        struct passwd *pw = getpwuid(getuid());
        path = pw->pw_dir;
    }
    CFStringRef path_cf = CFStringCreateWithCStringNoCopy(NULL, path, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFURLRef path_url = CFURLCreateWithFileSystemPath(NULL, path_cf, kCFURLPOSIXPathStyle, true);
    
    CFRelease(path_cf);
    return path_url;
}

#endif
