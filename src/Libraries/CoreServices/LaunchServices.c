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
#include <CoreServices/FileManager.h>
#include <CoreServices/UTType.h>
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
static CFMutableDictionaryRef sMIMEMap;         // lowercase MIME type -> CFURLRef
static CFMutableDictionaryRef sContentTypeMap;  // lowercase UTI -> CFURLRef

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

// the strings of a document type's array under key, lowercased into map, first writer wins
static void addStrings(CFMutableDictionaryRef map, CFDictionaryRef entry, CFStringRef key, CFURLRef appURL)
{
    CFArrayRef list = CFDictionaryGetValue(entry, key);

    if (list == NULL || CFGetTypeID(list) != CFArrayGetTypeID())
        return;
    for (CFIndex j = 0; j < CFArrayGetCount(list); j++) {
        CFStringRef value = CFArrayGetValueAtIndex(list, j);
        if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID())
            continue;
        CFStringRef lower = copyLowercase(value);
        if (lower != NULL) {
            if (!CFDictionaryContainsKey(map, lower))
                CFDictionarySetValue(map, lower, appURL);
            CFRelease(lower);
        }
    }
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

            addStrings(sMIMEMap, entry, CFSTR("CFBundleTypeMIMETypes"), appURL);
            addStrings(sContentTypeMap, entry, CFSTR("LSItemContentTypes"), appURL);

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
    sMIMEMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    sContentTypeMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
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
        CFRelease(sMIMEMap);
        CFRelease(sContentTypeMap);
        sMIMEMap = NULL;
        sContentTypeMap = NULL;
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

// the user's chosen handlers, kept as macOS keeps them: LSHandlers dictionaries in LaunchServices' preferences
#define kLSPrefsDomain CFSTR("com.apple.LaunchServices")

static CFArrayRef copyHandlerOverrides(void)
{
    CFPropertyListRef list = CFPreferencesCopyAppValue(CFSTR("LSHandlers"), kLSPrefsDomain);

    if (list != NULL && CFGetTypeID(list) != CFArrayGetTypeID()) {
        CFRelease(list);
        list = NULL;
    }
    return list;
}

// the bundle the user chose for a scheme or content type, kind being LSHandlerURLScheme or LSHandlerContentType
static CFURLRef copyOverrideHandler(CFStringRef kind, CFStringRef value)
{
    CFArrayRef list = copyHandlerOverrides();
    CFURLRef result = NULL;

    if (list == NULL || value == NULL) {
        if (list)
            CFRelease(list);
        return NULL;
    }
    buildRegistryIfNeeded();
    for (CFIndex i = 0; i < CFArrayGetCount(list) && result == NULL; i++) {
        CFDictionaryRef entry = CFArrayGetValueAtIndex(list, i);
        if (CFGetTypeID(entry) != CFDictionaryGetTypeID())
            continue;
        CFStringRef v = CFDictionaryGetValue(entry, kind);
        if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID() ||
            CFStringCompare(v, value, kCFCompareCaseInsensitive) != kCFCompareEqualTo)
            continue;
        CFStringRef bundleID = CFDictionaryGetValue(entry, CFSTR("LSHandlerRoleAll"));
        if (bundleID == NULL)
            bundleID = CFDictionaryGetValue(entry, CFSTR("LSHandlerRoleViewer"));
        if (bundleID != NULL && CFGetTypeID(bundleID) == CFStringGetTypeID()) {
            CFURLRef app = CFDictionaryGetValue(sIdentifierMap, bundleID);
            if (app != NULL)
                result = CFRetain(app);
        }
    }
    CFRelease(list);
    return result;
}

static OSStatus setOverrideHandler(CFStringRef kind, CFStringRef value, CFStringRef role, CFStringRef bundleID)
{
    CFArrayRef old = copyHandlerOverrides();
    CFMutableArrayRef list = old ? CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, old)
                                 : CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    if (old)
        CFRelease(old);
    if (list == NULL || value == NULL || bundleID == NULL) {
        if (list)
            CFRelease(list);
        return paramErr;
    }
    for (CFIndex i = CFArrayGetCount(list) - 1; i >= 0; i--) {
        CFDictionaryRef entry = CFArrayGetValueAtIndex(list, i);
        CFStringRef v = CFGetTypeID(entry) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(entry, kind) : NULL;
        if (v != NULL && CFGetTypeID(v) == CFStringGetTypeID() &&
            CFStringCompare(v, value, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            CFArrayRemoveValueAtIndex(list, i);
    }
    const void *keys[2] = { kind, role };
    const void *values[2] = { value, bundleID };
    CFDictionaryRef entry = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFArrayAppendValue(list, entry);
    CFRelease(entry);
    CFPreferencesSetAppValue(CFSTR("LSHandlers"), list, kLSPrefsDomain);
    Boolean saved = CFPreferencesAppSynchronize(kLSPrefsDomain);
    CFRelease(list);
    return saved ? noErr : kLSUnknownErr;
}

static CFURLRef copyHandlerForURL(CFURLRef inURL)
{
    buildRegistryIfNeeded();

    if (inURL == NULL) {
        return NULL;
    }

    CFStringRef scheme = CFURLCopyScheme(inURL);
    if (scheme != NULL && !CFStringHasPrefix(scheme, CFSTR("file"))) {
        CFURLRef chosen = copyOverrideHandler(CFSTR("LSHandlerURLScheme"), scheme);
        if (chosen != NULL) {
            CFRelease(scheme);
            return chosen;
        }
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

    // CFBundle hands back the executable relative to the bundle's MacOS folder, so resolve it against that base
    char executable[PATH_MAX];
    Boolean ok = CFURLGetFileSystemRepresentation(executableURL, true, (UInt8 *)executable, sizeof(executable));
    CFRelease(executableURL);
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
        char buffer[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(item, true, (UInt8 *)buffer, sizeof(buffer))) {
            argv[argc++] = strdup(buffer);
        }
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

// the Carbon-era LaunchServices calls, FSRef and launch specifications over the same registry

const CFStringRef kLSItemDisplayName = CFSTR("LSItemDisplayName");
const CFStringRef kLSItemContentType = CFSTR("LSItemContentType");
const CFStringRef kLSItemExtension = CFSTR("LSItemExtension");
const CFStringRef kLSQuarantineTypeKey = CFSTR("LSQuarantineType");
const CFStringRef kLSQuarantineOriginURLKey = CFSTR("LSQuarantineOriginURL");
const CFStringRef kLSQuarantineDataURLKey = CFSTR("LSQuarantineDataURL");
const CFStringRef kLSQuarantineAgentNameKey = CFSTR("LSQuarantineAgentName");
const CFStringRef kLSQuarantineAgentBundleIdentifierKey = CFSTR("LSQuarantineAgentBundleIdentifier");
const CFStringRef kLSQuarantineTimeStampKey = CFSTR("LSQuarantineTimeStamp");
const CFStringRef kLSQuarantineTypeWebDownload = CFSTR("LSQuarantineTypeWebDownload");
const CFStringRef kLSQuarantineTypeOtherDownload = CFSTR("LSQuarantineTypeOtherDownload");

static CFURLRef copyURLForFSRef(const FSRef *ref)
{
    UInt8 path[PATH_MAX];

    if (ref == NULL || FSRefMakePath(ref, path, sizeof(path)) != noErr)
        return NULL;
    return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, path, (CFIndex)strlen((char *)path), false);
}

static Boolean fsRefForURL(CFURLRef url, FSRef *ref)
{
    UInt8 path[PATH_MAX];

    return url != NULL && ref != NULL && CFURLGetFileSystemRepresentation(url, true, path, sizeof(path)) &&
           FSPathMakeRef(path, ref, NULL) == noErr;
}

// a bundle the user chose, then what the installed bundles declare, for a MIME type
static CFURLRef copyHandlerForMIMEType(CFStringRef mime)
{
    CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mime, NULL);
    CFURLRef handler = uti ? copyOverrideHandler(CFSTR("LSHandlerContentType"), uti) : NULL;

    buildRegistryIfNeeded();
    if (handler == NULL) {
        CFStringRef key = copyLowercase(mime);
        CFURLRef app = key ? CFDictionaryGetValue(sMIMEMap, key) : NULL;
        if (key)
            CFRelease(key);
        if (app == NULL && uti != NULL) {
            CFStringRef lowerUTI = copyLowercase(uti);
            app = lowerUTI ? CFDictionaryGetValue(sContentTypeMap, lowerUTI) : NULL;
            if (lowerUTI)
                CFRelease(lowerUTI);
        }
        if (app == NULL && uti != NULL) {
            CFStringRef ext = UTTypeCopyPreferredTagWithClass(uti, kUTTagClassFilenameExtension);
            CFStringRef lowerExt = ext ? copyLowercase(ext) : NULL;
            app = lowerExt ? CFDictionaryGetValue(sExtensionMap, lowerExt) : NULL;
            if (ext)
                CFRelease(ext);
            if (lowerExt)
                CFRelease(lowerExt);
        }
        if (app != NULL)
            handler = CFRetain(app);
    }
    if (uti)
        CFRelease(uti);
    return handler;
}

OSStatus LSCopyApplicationForMIMEType(CFStringRef inMIMEType, LSRolesMask inRoleMask, CFURLRef *outAppURL)
{
    (void)inRoleMask;
    if (inMIMEType == NULL || outAppURL == NULL)
        return paramErr;
    *outAppURL = copyHandlerForMIMEType(inMIMEType);
    return *outAppURL != NULL ? noErr : kLSApplicationNotFoundErr;
}

CFStringRef LSCopyDefaultHandlerForURLScheme(CFStringRef inURLScheme)
{
    CFURLRef app = copyOverrideHandler(CFSTR("LSHandlerURLScheme"), inURLScheme);
    CFStringRef bundleID = NULL;

    buildRegistryIfNeeded();
    if (app == NULL && inURLScheme != NULL) {
        CFStringRef key = copyLowercase(inURLScheme);
        CFURLRef found = key ? CFDictionaryGetValue(sSchemeMap, key) : NULL;
        if (key)
            CFRelease(key);
        if (found)
            app = CFRetain(found);
    }
    if (app != NULL) {
        CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, app);
        if (bundle != NULL) {
            CFStringRef identifier = CFBundleGetIdentifier(bundle);
            if (identifier)
                bundleID = CFStringCreateCopy(kCFAllocatorDefault, identifier);
            CFRelease(bundle);
        }
        CFRelease(app);
    }
    return bundleID;
}

OSStatus LSSetDefaultHandlerForURLScheme(CFStringRef inURLScheme, CFStringRef inHandlerBundleID)
{
    if (inURLScheme == NULL || inHandlerBundleID == NULL)
        return paramErr;
    CFStringRef scheme = copyLowercase(inURLScheme);
    OSStatus status = setOverrideHandler(CFSTR("LSHandlerURLScheme"), scheme, CFSTR("LSHandlerRoleAll"), inHandlerBundleID);
    CFRelease(scheme);
    return status;
}

OSStatus LSSetDefaultRoleHandlerForContentType(CFStringRef inContentType, LSRolesMask inRole, CFStringRef inHandlerBundleID)
{
    CFStringRef role = inRole == kLSRolesViewer ? CFSTR("LSHandlerRoleViewer")
                     : inRole == kLSRolesEditor ? CFSTR("LSHandlerRoleEditor")
                     : inRole == kLSRolesShell ? CFSTR("LSHandlerRoleShell") : CFSTR("LSHandlerRoleAll");

    if (inContentType == NULL || inHandlerBundleID == NULL)
        return paramErr;
    return setOverrideHandler(CFSTR("LSHandlerContentType"), inContentType, role, inHandlerBundleID);
}

OSStatus LSGetApplicationForItem(const FSRef *inItemRef, LSRolesMask inRoleMask, FSRef *outAppRef, CFURLRef *outAppURL)
{
    CFURLRef item = copyURLForFSRef(inItemRef);
    CFURLRef app;

    if (item == NULL)
        return paramErr;
    app = urlIsApplication(item) ? CFRetain(item) : LSCopyDefaultApplicationURLForURL(item, inRoleMask, NULL);
    CFRelease(item);
    if (app == NULL)
        return kLSApplicationNotFoundErr;
    if (outAppRef != NULL && !fsRefForURL(app, outAppRef)) {
        CFRelease(app);
        return kLSApplicationNotFoundErr;
    }
    if (outAppURL != NULL)
        *outAppURL = app;
    else
        CFRelease(app);
    return noErr;
}

OSStatus LSGetApplicationForInfo(OSType inType, OSType inCreator, CFStringRef inExtension, LSRolesMask inRoleMask,
                                 FSRef *outAppRef, CFURLRef *outAppURL)
{
    CFURLRef app = NULL;

    (void)inType;
    (void)inRoleMask;
    buildRegistryIfNeeded();
    if (inExtension != NULL) {
        CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, inExtension, NULL);
        if (uti != NULL) {
            app = copyOverrideHandler(CFSTR("LSHandlerContentType"), uti);
            CFRelease(uti);
        }
        if (app == NULL) {
            CFStringRef key = copyLowercase(inExtension);
            CFURLRef found = key ? CFDictionaryGetValue(sExtensionMap, key) : NULL;
            if (key)
                CFRelease(key);
            if (found)
                app = CFRetain(found);
        }
    }
    // creator codes name an application by its four-character signature, which no bundle here declares
    (void)inCreator;
    if (app == NULL)
        return kLSApplicationNotFoundErr;
    if (outAppRef != NULL && !fsRefForURL(app, outAppRef)) {
        CFRelease(app);
        return kLSApplicationNotFoundErr;
    }
    if (outAppURL != NULL)
        *outAppURL = app;
    else
        CFRelease(app);
    return noErr;
}

OSStatus LSCopyItemAttribute(const FSRef *inItem, LSRolesMask inRoles, CFStringRef inAttributeName, CFTypeRef *outValue)
{
    CFURLRef item = copyURLForFSRef(inItem);

    (void)inRoles;
    if (item == NULL || inAttributeName == NULL || outValue == NULL) {
        if (item)
            CFRelease(item);
        return paramErr;
    }
    *outValue = NULL;
    if (CFEqual(inAttributeName, kLSItemDisplayName)) {
        *outValue = LSCopyDisplayNameForURL(item);
    } else if (CFEqual(inAttributeName, kLSItemExtension)) {
        *outValue = CFURLCopyPathExtension(item);
    } else if (CFEqual(inAttributeName, kLSItemContentType)) {
        CFStringRef ext = CFURLCopyPathExtension(item);
        *outValue = ext ? UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, ext, NULL)
                        : CFStringCreateCopy(kCFAllocatorDefault, CFSTR("public.data"));
        if (ext)
            CFRelease(ext);
    }
    CFRelease(item);
    return *outValue != NULL ? noErr : kLSAttributeNotFoundErr;
}

OSStatus LSRegisterURL(CFURLRef inURL, Boolean inUpdate)
{
    CFStringRef path;

    (void)inUpdate;
    if (inURL == NULL || !urlIsApplication(inURL))
        return paramErr;
    path = CFURLCopyFileSystemPath(inURL, kCFURLPOSIXPathStyle);
    if (path == NULL)
        return paramErr;
    buildRegistryIfNeeded();
    // a later registration of the same bundle refreshes its claims, earlier bundles keep priority
    if (!CFArrayContainsValue(sApplicationURLs, CFRangeMake(0, CFArrayGetCount(sApplicationURLs)), inURL))
        registerApplication(path);
    CFRelease(path);
    return noErr;
}

// open each item with the given or default application, launching it once with all of its items
static OSStatus openItems(CFArrayRef itemURLs, CFURLRef appURL, LSLaunchFlags flags, CFURLRef *outLaunched)
{
    CFIndex count = itemURLs ? CFArrayGetCount(itemURLs) : 0;

    (void)flags;
    if (appURL != NULL)
        return LSOpenApplicationAtURL(appURL, itemURLs, outLaunched);
    if (count == 0)
        return paramErr;
    OSStatus status = noErr;
    for (CFIndex i = 0; i < count; i++) {
        OSStatus s = LSOpenCFURLRef(CFArrayGetValueAtIndex(itemURLs, i), i == 0 ? outLaunched : NULL);
        if (s != noErr)
            status = s;
    }
    return status;
}

OSStatus LSOpenFromURLSpec(const LSLaunchURLSpec *inLaunchSpec, CFURLRef *outLaunchedURL)
{
    if (inLaunchSpec == NULL || (inLaunchSpec->appURL == NULL && inLaunchSpec->itemURLs == NULL))
        return paramErr;
    return openItems(inLaunchSpec->itemURLs, inLaunchSpec->appURL, inLaunchSpec->launchFlags, outLaunchedURL);
}

OSStatus LSOpenFromRefSpec(const LSLaunchFSRefSpec *inLaunchSpec, FSRef *outLaunchedRef)
{
    CFMutableArrayRef items;
    CFURLRef app = NULL, launched = NULL;
    OSStatus status;

    if (inLaunchSpec == NULL)
        return paramErr;
    items = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    for (ItemCount i = 0; i < inLaunchSpec->numDocs; i++) {
        CFURLRef url = copyURLForFSRef(&inLaunchSpec->itemRefs[i]);
        if (url != NULL) {
            CFArrayAppendValue(items, url);
            CFRelease(url);
        }
    }
    if (inLaunchSpec->appRef != NULL && (app = copyURLForFSRef(inLaunchSpec->appRef)) == NULL) {
        CFRelease(items);
        return kLSApplicationNotFoundErr;
    }
    status = openItems(items, app, inLaunchSpec->launchFlags, &launched);
    CFRelease(items);
    if (app)
        CFRelease(app);
    if (status == noErr && launched != NULL && outLaunchedRef != NULL && !fsRefForURL(launched, outLaunchedRef))
        status = kLSApplicationNotFoundErr;
    if (launched)
        CFRelease(launched);
    return status;
}
