// SharedFileList.c - LSSharedFileList over preferences, and the MDItem handle
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// one array per list in com.apple.LSSharedFileList, and the MDItem handle Spotlight callers create
// session login items are stored for a session launcher, nothing on PureDarwin starts them yet
#include <CoreServices/SharedFileList.h>
#include <pthread.h>

const CFStringRef kLSSharedFileListSessionLoginItems = CFSTR("com.apple.LSSharedFileList.SessionLoginItems");
const CFStringRef kLSSharedFileListGlobalLoginItems = CFSTR("com.apple.LSSharedFileList.GlobalLoginItems");
const CFStringRef kLSSharedFileListRecentApplicationItems = CFSTR("com.apple.LSSharedFileList.RecentApplications");
const CFStringRef kLSSharedFileListRecentDocumentItems = CFSTR("com.apple.LSSharedFileList.RecentDocuments");
const CFStringRef kLSSharedFileListFavoriteItems = CFSTR("com.apple.LSSharedFileList.FavoriteItems");

const CFStringRef kMDItemWhereFroms = CFSTR("kMDItemWhereFroms");
const CFStringRef kMDItemPath = CFSTR("kMDItemPath");
const CFStringRef kMDItemDisplayName = CFSTR("kMDItemDisplayName");

// insertion positions, compared by address only
static struct __LSSharedFileListItem { int tag; } sLast = { 1 }, sBeforeFirst = { 2 };
LSSharedFileListItemRef kLSSharedFileListItemLast = (LSSharedFileListItemRef)&sLast;
LSSharedFileListItemRef kLSSharedFileListItemBeforeFirst = (LSSharedFileListItemRef)&sBeforeFirst;

#define kPrefsDomain CFSTR("com.apple.LSSharedFileList")

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

// a list is a dictionary holding its name and its items, an item a dictionary with URL and Name
static CFMutableArrayRef itemsOf(LSSharedFileListRef list)
{
    return (CFMutableArrayRef)CFDictionaryGetValue((CFDictionaryRef)list, CFSTR("items"));
}

static void save(LSSharedFileListRef list)
{
    CFStringRef name = CFDictionaryGetValue((CFDictionaryRef)list, CFSTR("name"));
    CFMutableArrayRef items = itemsOf(list);
    CFMutableArrayRef stored = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    for (CFIndex i = 0; i < CFArrayGetCount(items); i++) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
        CFURLRef url = CFDictionaryGetValue(item, CFSTR("URL"));
        CFMutableDictionaryRef entry = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(entry, CFSTR("URL"), CFURLGetString(url));
        CFStringRef display = CFDictionaryGetValue(item, CFSTR("Name"));
        if (display)
            CFDictionarySetValue(entry, CFSTR("Name"), display);
        CFArrayAppendValue(stored, entry);
        CFRelease(entry);
    }
    CFPreferencesSetAppValue(name, stored, kPrefsDomain);
    CFPreferencesAppSynchronize(kPrefsDomain);
    CFRelease(stored);
}

static CFDictionaryRef createItem(CFURLRef url, CFStringRef displayName)
{
    CFMutableDictionaryRef item = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(item, CFSTR("URL"), url);
    if (displayName)
        CFDictionarySetValue(item, CFSTR("Name"), displayName);
    return item;
}

LSSharedFileListRef LSSharedFileListCreate(CFAllocatorRef inAllocator, CFStringRef inListType, CFTypeRef listOptions)
{
    (void)listOptions;
    if (inListType == NULL)
        return NULL;

    CFMutableDictionaryRef list = CFDictionaryCreateMutable(inAllocator, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef items = CFArrayCreateMutable(inAllocator, 0, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(list, CFSTR("name"), inListType);
    CFDictionarySetValue(list, CFSTR("items"), items);
    CFRelease(items);

    pthread_mutex_lock(&sLock);
    CFPreferencesAppSynchronize(kPrefsDomain);
    CFPropertyListRef stored = CFPreferencesCopyAppValue(inListType, kPrefsDomain);
    if (stored != NULL && CFGetTypeID(stored) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount(stored); i++) {
            CFDictionaryRef entry = CFArrayGetValueAtIndex(stored, i);
            CFStringRef string = CFGetTypeID(entry) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(entry, CFSTR("URL")) : NULL;
            CFURLRef url = string && CFGetTypeID(string) == CFStringGetTypeID() ? CFURLCreateWithString(NULL, string, NULL) : NULL;
            if (url == NULL)
                continue;
            CFDictionaryRef item = createItem(url, CFDictionaryGetValue(entry, CFSTR("Name")));
            CFArrayAppendValue(items, item);
            CFRelease(item);
            CFRelease(url);
        }
    }
    if (stored)
        CFRelease(stored);
    pthread_mutex_unlock(&sLock);
    return (LSSharedFileListRef)list;
}

CFArrayRef LSSharedFileListCopySnapshot(LSSharedFileListRef inList, UInt32 *outSnapshotSeed)
{
    if (inList == NULL)
        return NULL;
    pthread_mutex_lock(&sLock);
    CFArrayRef snapshot = CFArrayCreateCopy(kCFAllocatorDefault, itemsOf(inList));
    if (outSnapshotSeed)
        *outSnapshotSeed = (UInt32)CFArrayGetCount(snapshot);
    pthread_mutex_unlock(&sLock);
    return snapshot;
}

LSSharedFileListItemRef LSSharedFileListInsertItemURL(LSSharedFileListRef inList, LSSharedFileListItemRef insertAfterThisItem,
                                                      CFStringRef inDisplayName, IconRef inIconRef, CFURLRef inURL,
                                                      CFDictionaryRef inPropertiesToSet, CFArrayRef inPropertiesToClear)
{
    (void)inIconRef;
    (void)inPropertiesToSet;
    (void)inPropertiesToClear;
    if (inList == NULL || inURL == NULL)
        return NULL;

    pthread_mutex_lock(&sLock);
    CFMutableArrayRef items = itemsOf(inList);
    // an existing entry for the same URL moves rather than appearing twice
    for (CFIndex i = CFArrayGetCount(items) - 1; i >= 0; i--) {
        if (CFEqual(CFDictionaryGetValue(CFArrayGetValueAtIndex(items, i), CFSTR("URL")), inURL))
            CFArrayRemoveValueAtIndex(items, i);
    }
    CFDictionaryRef item = createItem(inURL, inDisplayName);
    CFIndex at = CFArrayGetCount(items);
    if (insertAfterThisItem == kLSSharedFileListItemBeforeFirst) {
        at = 0;
    } else if (insertAfterThisItem != kLSSharedFileListItemLast && insertAfterThisItem != NULL) {
        CFIndex found = CFArrayGetFirstIndexOfValue(items, CFRangeMake(0, CFArrayGetCount(items)), insertAfterThisItem);
        if (found != kCFNotFound)
            at = found + 1;
    }
    CFArrayInsertValueAtIndex(items, at, item);
    save(inList);
    pthread_mutex_unlock(&sLock);
    // the list keeps the item alive, the caller owns a reference as with Apple's
    return (LSSharedFileListItemRef)item;
}

OSStatus LSSharedFileListItemRemove(LSSharedFileListRef inList, LSSharedFileListItemRef inItem)
{
    if (inList == NULL || inItem == NULL)
        return paramErr;
    pthread_mutex_lock(&sLock);
    CFMutableArrayRef items = itemsOf(inList);
    CFURLRef url = CFDictionaryGetValue((CFDictionaryRef)inItem, CFSTR("URL"));
    Boolean removed = false;
    for (CFIndex i = CFArrayGetCount(items) - 1; i >= 0; i--) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
        if (item == (CFDictionaryRef)inItem || (url && CFEqual(CFDictionaryGetValue(item, CFSTR("URL")), url))) {
            CFArrayRemoveValueAtIndex(items, i);
            removed = true;
        }
    }
    if (removed)
        save(inList);
    pthread_mutex_unlock(&sLock);
    return removed ? noErr : fnfErr;
}

CFURLRef LSSharedFileListItemCopyResolvedURL(LSSharedFileListItemRef inItem, LSSharedFileListResolutionFlags inFlags,
                                             CFErrorRef *outError)
{
    (void)inFlags;
    CFURLRef url = inItem ? CFDictionaryGetValue((CFDictionaryRef)inItem, CFSTR("URL")) : NULL;

    if (url == NULL) {
        if (outError)
            *outError = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, fnfErr, NULL);
        return NULL;
    }
    return CFRetain(url);
}

CFStringRef LSSharedFileListItemCopyDisplayName(LSSharedFileListItemRef inItem)
{
    CFStringRef name = inItem ? CFDictionaryGetValue((CFDictionaryRef)inItem, CFSTR("Name")) : NULL;
    CFURLRef url = inItem ? CFDictionaryGetValue((CFDictionaryRef)inItem, CFSTR("URL")) : NULL;

    if (name)
        return CFRetain(name);
    return url ? CFURLCopyLastPathComponent(url) : NULL;
}

// a metadata item for a path, the attributes come from the file itself through MDItemCopyAttribute
MDItemRef MDItemCreate(CFAllocatorRef allocator, CFStringRef path)
{
    if (path == NULL)
        return NULL;
    CFMutableDictionaryRef item = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(item, kMDItemPath, path);
    return (MDItemRef)item;
}

CFTypeRef MDItemCopyAttribute(MDItemRef item, CFStringRef name)
{
    CFTypeRef value = item && name ? CFDictionaryGetValue((CFDictionaryRef)item, name) : NULL;

    if (value == NULL && name && CFEqual(name, kMDItemDisplayName)) {
        CFStringRef path = CFDictionaryGetValue((CFDictionaryRef)item, kMDItemPath);
        CFURLRef url = path ? CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, false) : NULL;
        if (url) {
            value = CFURLCopyLastPathComponent(url);
            CFRelease(url);
            return value;
        }
    }
    return value ? CFRetain(value) : NULL;
}
