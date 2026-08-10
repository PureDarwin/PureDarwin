
#import <Foundation/Foundation.h>
#import "SecKnownFilePaths.h"
#import "OSX/utilities/SecCFRelease.h"

// This file is separate from SecFileLocation.c because it has a global variable.
// We need exactly one of those per address space, so it needs to live in the Security framework.
static CFURLRef sCustomHomeURL = NULL;

CFURLRef SecCopyHomeURL(void)
{
    // This returns a CFURLRef so that it can be passed as the second parameter
    // to CFURLCreateCopyAppendingPathComponent

    CFURLRef homeURL = sCustomHomeURL;
    if (homeURL) {
        CFRetain(homeURL);
    } else {
        homeURL = CFCopyHomeDirectoryURL();
    }

    return homeURL;
}

CFURLRef SecCopyBaseFilesURL(bool system)
{
    CFURLRef baseURL = sCustomHomeURL;
    if (baseURL) {
        CFRetain(baseURL);
    } else {
#if TARGET_OS_OSX
        if (system) {
            baseURL = CFURLCreateWithFileSystemPath(NULL, CFSTR("/"), kCFURLPOSIXPathStyle, true);
        } else {
            baseURL = SecCopyHomeURL();
        }
#elif TARGET_OS_SIMULATOR
        baseURL = SecCopyHomeURL();
#else
        baseURL = CFURLCreateWithFileSystemPath(NULL, CFSTR("/"), kCFURLPOSIXPathStyle, true);
#endif
    }
    return baseURL;
}

void SecSetCustomHomeURL(CFURLRef url)
{
    sCustomHomeURL = CFRetainSafe(url);
}

void SecSetCustomHomeURLString(CFStringRef home_path)
{
    CFReleaseNull(sCustomHomeURL);
    if (home_path) {
        sCustomHomeURL = CFURLCreateWithFileSystemPath(NULL, home_path, kCFURLPOSIXPathStyle, true);
    }
}
