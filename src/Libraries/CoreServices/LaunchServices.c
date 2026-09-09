/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* LaunchServices over a directory scan.
 *
 * Apple's version is backed by a registration database maintained by lsd. There
 * is no such daemon here, so the application folders are scanned on first use
 * and the result cached; LSRefreshApplicationRegistry() drops the cache. Each
 * .app contributes its CFBundleDocumentTypes extensions and CFBundleURLTypes
 * schemes, which is what document and URL opening dispatch on.
 */

#include <CoreServices/LaunchServices.h>
#include <spawn.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

extern char **environ;

static const char * const kApplicationFolders[] = {
    "/Applications",
    "/System/Applications",
    "/System/Library/CoreServices",
    "/Network/Applications",
    NULL
};

/* url -> CFBundleRef, built once and reused. */
static CFMutableArrayRef sApplicationURLs;      /* CFURLRef of each .app */
static CFMutableDictionaryRef sExtensionMap;    /* lowercase ext -> CFURLRef */
static CFMutableDictionaryRef sSchemeMap;       /* lowercase scheme -> CFURLRef */
static CFMutableDictionaryRef sIdentifierMap;   /* bundle id -> CFURLRef */

static CFStringRef copyLowercase(CFStringRef string)
{
    if (string == NULL) {
        return NULL;
    }
    CFMutableStringRef lower = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, string);
    if (lower != NULL) {
        CFStringLowercase(lower, NULL);
    }
    return lower;
}

/* Record every extension and scheme the bundle claims. First writer wins, so a
 * bundle found earlier in the folder order stays the preferred handler. */
static void registerBundleTypes(CFBundleRef bundle, CFURLRef appURL)
{
    CFArrayRef documentTypes =
        CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleDocumentTypes"));

    if (documentTypes != NULL && CFGetTypeID(documentTypes) == CFArrayGetTypeID()) {
        CFIndex count = CFArrayGetCount(documentTypes);

        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef entry = CFArrayGetValueAtIndex(documentTypes, i);
            if (entry == NULL || CFGetTypeID(entry) != CFDictionaryGetTypeID()) {
                continue;
            }

            CFArrayRef extensions = CFDictionaryGetValue(entry, CFSTR("CFBundleTypeExtensions"));
            if (extensions == NULL || CFGetTypeID(extensions) != CFArrayGetTypeID()) {
                continue;
            }

            CFIndex extensionCount = CFArrayGetCount(extensions);
            for (CFIndex j = 0; j < extensionCount; j++) {
                CFStringRef extension = CFArrayGetValueAtIndex(extensions, j);
                if (extension == NULL || CFGetTypeID(extension) != CFStringGetTypeID()) {
                    continue;
                }
                CFStringRef key = copyLowercase(extension);
                if (key != NULL) {
                    if (!CFDictionaryContainsKey(sExtensionMap, key)) {
                        CFDictionarySetValue(sExtensionMap, key, appURL);
                    }
                    CFRelease(key);
                }
            }
        }
    }

    CFArrayRef urlTypes =
        CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleURLTypes"));

    if (urlTypes != NULL && CFGetTypeID(urlTypes) == CFArrayGetTypeID()) {
        CFIndex count = CFArrayGetCount(urlTypes);

        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef entry = CFArrayGetValueAtIndex(urlTypes, i);
            if (entry == NULL || CFGetTypeID(entry) != CFDictionaryGetTypeID()) {
                continue;
            }

            CFArrayRef schemes = CFDictionaryGetValue(entry, CFSTR("CFBundleURLSchemes"));
            if (schemes == NULL || CFGetTypeID(schemes) != CFArrayGetTypeID()) {
                continue;
            }

            CFIndex schemeCount = CFArrayGetCount(schemes);
            for (CFIndex j = 0; j < schemeCount; j++) {
                CFStringRef scheme = CFArrayGetValueAtIndex(schemes, j);
                if (scheme == NULL || CFGetTypeID(scheme) != CFStringGetTypeID()) {
                    continue;
                }
                CFStringRef key = copyLowercase(scheme);
                if (key != NULL) {
                    if (!CFDictionaryContainsKey(sSchemeMap, key)) {
                        CFDictionarySetValue(sSchemeMap, key, appURL);
                    }
                    CFRelease(key);
                }
            }
        }
    }
}

static void registerApplication(CFStringRef path)
{
    CFURLRef appURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path,
                                                    kCFURLPOSIXPathStyle, true);
    if (appURL == NULL) {
        return;
    }

    CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, appURL);
    if (bundle == NULL) {
        CFRelease(appURL);
        return;
    }

    CFArrayAppendValue(sApplicationURLs, appURL);

    CFStringRef identifier = CFBundleGetIdentifier(bundle);
    if (identifier != NULL && !CFDictionaryContainsKey(sIdentifierMap, identifier)) {
        CFDictionarySetValue(sIdentifierMap, identifier, appURL);
    }

    registerBundleTypes(bundle, appURL);

    CFRelease(bundle);
    CFRelease(appURL);
}

static void scanFolder(const char *folder)
{
    CFStringRef folderPath = CFStringCreateWithCString(kCFAllocatorDefault, folder,
                                                        kCFStringEncodingUTF8);
    if (folderPath == NULL) {
        return;
    }

    CFURLRef folderURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, folderPath,
                                                        kCFURLPOSIXPathStyle, true);
    CFRelease(folderPath);
    if (folderURL == NULL) {
        return;
    }

    /* Read the directory directly; there is no CFURLEnumerator here. */
    CFRelease(folderURL);

    DIR *dir = opendir(folder);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t length = strlen(name);

        if (length < 5 || strcmp(name + length - 4, ".app") != 0) {
            continue;
        }

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", folder, name);

        CFStringRef path = CFStringCreateWithCString(kCFAllocatorDefault, full,
                                                      kCFStringEncodingUTF8);
        if (path != NULL) {
            registerApplication(path);
            CFRelease(path);
        }
    }

    closedir(dir);
}

static void buildRegistryIfNeeded(void)
{
    if (sApplicationURLs != NULL) {
        return;
    }

    sApplicationURLs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    sExtensionMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    sSchemeMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    sIdentifierMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    for (int i = 0; kApplicationFolders[i] != NULL; i++) {
        scanFolder(kApplicationFolders[i]);
    }

    /* ~/Applications last, so a user copy does not silently shadow the system. */
    const char *home = getenv("HOME");
    if (home != NULL) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/Applications", home);
        scanFolder(path);
    }
}

void LSRefreshApplicationRegistry(void)
{
    if (sApplicationURLs != NULL) {
        CFRelease(sApplicationURLs);
        CFRelease(sExtensionMap);
        CFRelease(sSchemeMap);
        CFRelease(sIdentifierMap);
        sApplicationURLs = NULL;
        sExtensionMap = NULL;
        sSchemeMap = NULL;
        sIdentifierMap = NULL;
    }
}

CFArrayRef LSCopyAllApplicationURLs(void)
{
    buildRegistryIfNeeded();
    return CFArrayCreateCopy(kCFAllocatorDefault, sApplicationURLs);
}

/* Does this URL point at a bundle we can launch directly? */
static Boolean urlIsApplication(CFURLRef url)
{
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (path == NULL) {
        return false;
    }

    Boolean isApp = CFStringHasSuffix(path, CFSTR(".app"));
    CFRelease(path);
    return isApp;
}

static CFURLRef copyHandlerForURL(CFURLRef inURL)
{
    buildRegistryIfNeeded();

    if (inURL == NULL) {
        return NULL;
    }

    CFStringRef scheme = CFURLCopyScheme(inURL);
    if (scheme != NULL && !CFStringHasPrefix(scheme, CFSTR("file"))) {
        CFStringRef key = copyLowercase(scheme);
        CFURLRef handler = key ? CFDictionaryGetValue(sSchemeMap, key) : NULL;

        if (key != NULL) {
            CFRelease(key);
        }
        CFRelease(scheme);

        return handler ? (CFURLRef)CFRetain(handler) : NULL;
    }
    if (scheme != NULL) {
        CFRelease(scheme);
    }

    CFStringRef extension = CFURLCopyPathExtension(inURL);
    if (extension == NULL) {
        return NULL;
    }

    CFStringRef key = copyLowercase(extension);
    CFRelease(extension);
    if (key == NULL) {
        return NULL;
    }

    CFURLRef handler = CFDictionaryGetValue(sExtensionMap, key);
    CFRelease(key);

    return handler ? (CFURLRef)CFRetain(handler) : NULL;
}

CFURLRef LSCopyDefaultApplicationURLForURL(CFURLRef inURL, LSRolesMask inRoleMask,
                                           CFErrorRef *outError)
{
    (void)inRoleMask;

    CFURLRef handler = copyHandlerForURL(inURL);

    if (handler == NULL && outError != NULL) {
        *outError = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus,
                                  kLSApplicationNotFoundErr, NULL);
    }
    return handler;
}

CFURLRef LSCopyDefaultApplicationURLForContentType(CFStringRef inContentType,
                                                   LSRolesMask inRoleMask,
                                                   CFErrorRef *outError)
{
    (void)inRoleMask;
    buildRegistryIfNeeded();

    CFStringRef key = copyLowercase(inContentType);
    CFURLRef handler = key ? CFDictionaryGetValue(sExtensionMap, key) : NULL;

    if (key != NULL) {
        CFRelease(key);
    }

    if (handler == NULL) {
        if (outError != NULL) {
            *outError = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus,
                                      kLSApplicationNotFoundErr, NULL);
        }
        return NULL;
    }
    return (CFURLRef)CFRetain(handler);
}

CFArrayRef LSCopyApplicationURLsForURL(CFURLRef inURL, LSRolesMask inRoleMask)
{
    CFURLRef handler = LSCopyDefaultApplicationURLForURL(inURL, inRoleMask, NULL);
    if (handler == NULL) {
        return NULL;
    }

    const void *values[1] = { handler };
    CFArrayRef result = CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);
    CFRelease(handler);

    return result;
}

CFArrayRef LSCopyApplicationURLsForBundleIdentifier(CFStringRef inBundleIdentifier,
                                                    CFErrorRef *outError)
{
    buildRegistryIfNeeded();

    CFURLRef appURL = inBundleIdentifier ?
        CFDictionaryGetValue(sIdentifierMap, inBundleIdentifier) : NULL;

    if (appURL == NULL) {
        if (outError != NULL) {
            *outError = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus,
                                      kLSApplicationNotFoundErr, NULL);
        }
        return NULL;
    }

    const void *values[1] = { appURL };
    return CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);
}

/* Spawn the bundle's executable with the item paths as arguments. */
static OSStatus spawnApplication(CFURLRef appURL, CFArrayRef itemURLs)
{
    CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, appURL);
    if (bundle == NULL) {
        return kLSApplicationNotFoundErr;
    }

    CFURLRef executableURL = CFBundleCopyExecutableURL(bundle);
    CFRelease(bundle);
    if (executableURL == NULL) {
        return kLSApplicationNotFoundErr;
    }

    CFStringRef executablePath = CFURLCopyFileSystemPath(executableURL, kCFURLPOSIXPathStyle);
    CFRelease(executableURL);
    if (executablePath == NULL) {
        return kLSApplicationNotFoundErr;
    }

    char executable[PATH_MAX];
    Boolean ok = CFStringGetCString(executablePath, executable, sizeof(executable),
                                     kCFStringEncodingUTF8);
    CFRelease(executablePath);
    if (!ok) {
        return kLSDataErr;
    }

    CFIndex itemCount = itemURLs ? CFArrayGetCount(itemURLs) : 0;
    char **argv = calloc((size_t)itemCount + 2, sizeof(*argv));
    if (argv == NULL) {
        return kLSUnknownErr;
    }

    argv[0] = executable;

    int argc = 1;
    for (CFIndex i = 0; i < itemCount; i++) {
        CFURLRef item = CFArrayGetValueAtIndex(itemURLs, i);
        CFStringRef itemPath = CFURLCopyFileSystemPath(item, kCFURLPOSIXPathStyle);
        if (itemPath == NULL) {
            continue;
        }

        char buffer[PATH_MAX];
        if (CFStringGetCString(itemPath, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            argv[argc++] = strdup(buffer);
        }
        CFRelease(itemPath);
    }
    argv[argc] = NULL;

    pid_t pid = 0;
    int status = posix_spawn(&pid, executable, NULL, NULL, argv, environ);

    for (int i = 1; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);

    return (status == 0) ? noErr : kLSUnknownErr;
}

OSStatus LSOpenApplicationAtURL(CFURLRef inAppURL, CFArrayRef inItemURLs,
                                CFURLRef *outLaunchedURL)
{
    if (inAppURL == NULL) {
        return kLSApplicationNotFoundErr;
    }

    OSStatus status = spawnApplication(inAppURL, inItemURLs);

    if (status == noErr && outLaunchedURL != NULL) {
        *outLaunchedURL = (CFURLRef)CFRetain(inAppURL);
    }
    return status;
}

OSStatus LSOpenCFURLRef(CFURLRef inURL, CFURLRef *outLaunchedURL)
{
    if (inURL == NULL) {
        return kLSApplicationNotFoundErr;
    }

    /* An application opens itself; anything else goes to its handler. */
    if (urlIsApplication(inURL)) {
        return LSOpenApplicationAtURL(inURL, NULL, outLaunchedURL);
    }

    CFURLRef handler = copyHandlerForURL(inURL);
    if (handler == NULL) {
        return kLSApplicationNotFoundErr;
    }

    const void *items[1] = { inURL };
    CFArrayRef itemURLs = CFArrayCreate(kCFAllocatorDefault, items, 1, &kCFTypeArrayCallBacks);

    OSStatus status = LSOpenApplicationAtURL(handler, itemURLs, outLaunchedURL);

    if (itemURLs != NULL) {
        CFRelease(itemURLs);
    }
    CFRelease(handler);

    return status;
}

CFStringRef LSCopyDisplayNameForURL(CFURLRef inURL)
{
    if (inURL == NULL) {
        return NULL;
    }

    if (urlIsApplication(inURL)) {
        CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, inURL);
        if (bundle != NULL) {
            CFStringRef name =
                CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleDisplayName"));
            if (name == NULL) {
                name = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleName"));
            }
            CFStringRef result = name ? CFStringCreateCopy(kCFAllocatorDefault, name) : NULL;
            CFRelease(bundle);
            if (result != NULL) {
                return result;
            }
        }
    }

    return CFURLCopyLastPathComponent(inURL);
}

CFStringRef LSCopyKindStringForURL(CFURLRef inURL)
{
    if (inURL == NULL) {
        return NULL;
    }

    if (urlIsApplication(inURL)) {
        return CFStringCreateCopy(kCFAllocatorDefault, CFSTR("Application"));
    }

    CFStringRef extension = CFURLCopyPathExtension(inURL);
    if (extension == NULL || CFStringGetLength(extension) == 0) {
        if (extension != NULL) {
            CFRelease(extension);
        }
        return CFStringCreateCopy(kCFAllocatorDefault, CFSTR("Document"));
    }

    CFStringRef upper = copyLowercase(extension);
    CFRelease(extension);

    CFStringRef kind = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                 CFSTR("%@ document"), upper);
    if (upper != NULL) {
        CFRelease(upper);
    }
    return kind;
}
