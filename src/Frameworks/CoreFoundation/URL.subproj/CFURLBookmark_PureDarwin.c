// CFURLBookmark_PureDarwin.c - bookmark data and alias files
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a bookmark is opaque to callers, ours is a binary plist of the path, volume and file ID.
// a file that moved on its volume is found again by file ID, as on macOS, and reported stale
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYS_fsgetpath
#define SYS_fsgetpath 427
#endif

#define kPDBookmarkMagic CFSTR("org.puredarwin.bookmark")

static CFErrorRef posixError(int code)
{
    return CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, code, NULL);
}

static void setNumber(CFMutableDictionaryRef dict, CFStringRef key, CFNumberType type, const void *value)
{
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, type, value);

    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

CFDataRef CFURLCreateBookmarkData(CFAllocatorRef allocator, CFURLRef url, CFURLBookmarkCreationOptions options,
    CFArrayRef resourcePropertiesToInclude, CFURLRef relativeToURL, CFErrorRef *error)
{
    char path[PATH_MAX];
    struct stat st;
    struct statfs sfs;
    CFMutableDictionaryRef dict;
    CFStringRef pathString;
    CFDataRef data;
    long long fileID;
    int32_t fsid0, fsid1;

    if (!url || !CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path))) {
        if (error)
            *error = posixError(EINVAL);
        return NULL;
    }
    if (lstat(path, &st) != 0 || statfs(path, &sfs) != 0) {
        if (error)
            *error = posixError(errno);
        return NULL;
    }
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    pathString = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    CFDictionarySetValue(dict, CFSTR("Kind"), kPDBookmarkMagic);
    CFDictionarySetValue(dict, CFSTR("Path"), pathString);
    CFRelease(pathString);
    fileID = (long long)st.st_ino;
    fsid0 = sfs.f_fsid.val[0];
    fsid1 = sfs.f_fsid.val[1];
    setNumber(dict, CFSTR("FileID"), kCFNumberLongLongType, &fileID);
    setNumber(dict, CFSTR("VolumeID0"), kCFNumberSInt32Type, &fsid0);
    setNumber(dict, CFSTR("VolumeID1"), kCFNumberSInt32Type, &fsid1);
    CFDictionarySetValue(dict, CFSTR("IsDirectory"), S_ISDIR(st.st_mode) ? kCFBooleanTrue : kCFBooleanFalse);
    data = CFPropertyListCreateData(allocator, dict, kCFPropertyListBinaryFormat_v1_0, 0, error);
    CFRelease(dict);
    return data;
}

static CFDictionaryRef copyBookmark(CFDataRef bookmark)
{
    CFPropertyListRef plist;

    if (!bookmark)
        return NULL;
    plist = CFPropertyListCreateWithData(kCFAllocatorDefault, bookmark, kCFPropertyListImmutable, NULL, NULL);
    if (plist && CFGetTypeID(plist) == CFDictionaryGetTypeID() &&
        CFEqual(CFDictionaryGetValue(plist, CFSTR("Kind")) ?: CFSTR(""), kPDBookmarkMagic))
        return plist;
    if (plist)
        CFRelease(plist);
    return NULL;
}

static Boolean getNumber(CFDictionaryRef dict, CFStringRef key, CFNumberType type, void *value)
{
    CFTypeRef number = CFDictionaryGetValue(dict, key);

    return number && CFGetTypeID(number) == CFNumberGetTypeID() && CFNumberGetValue(number, type, value);
}

CFURLRef CFURLCreateByResolvingBookmarkData(CFAllocatorRef allocator, CFDataRef bookmark,
    CFURLBookmarkResolutionOptions options, CFURLRef relativeToURL, CFArrayRef resourcePropertiesToInclude,
    Boolean *isStale, CFErrorRef *error)
{
    CFDictionaryRef dict = copyBookmark(bookmark);
    CFStringRef pathString;
    char path[PATH_MAX];
    struct stat st;
    long long fileID;
    fsid_t fsid;
    Boolean stale = false, isDir;
    CFURLRef url;

    if (isStale)
        *isStale = false;
    pathString = dict ? CFDictionaryGetValue(dict, CFSTR("Path")) : NULL;
    if (!pathString || CFGetTypeID(pathString) != CFStringGetTypeID() ||
        !CFStringGetFileSystemRepresentation(pathString, path, sizeof(path))) {
        if (dict)
            CFRelease(dict);
        if (error)
            *error = posixError(EINVAL);
        return NULL;
    }
    // the path still names the same file, or the file moved and its ID finds it
    if (lstat(path, &st) != 0 || (getNumber(dict, CFSTR("FileID"), kCFNumberLongLongType, &fileID) &&
        (long long)st.st_ino != fileID)) {
        char found[PATH_MAX];

        if (getNumber(dict, CFSTR("FileID"), kCFNumberLongLongType, &fileID) &&
            getNumber(dict, CFSTR("VolumeID0"), kCFNumberSInt32Type, &fsid.val[0]) &&
            getNumber(dict, CFSTR("VolumeID1"), kCFNumberSInt32Type, &fsid.val[1]) &&
            syscall(SYS_fsgetpath, found, sizeof(found), &fsid, (uint64_t)fileID) > 0 && lstat(found, &st) == 0) {
            strlcpy(path, found, sizeof(path));
            stale = true;
        } else if (lstat(path, &st) != 0) {
            CFRelease(dict);
            if (error)
                *error = posixError(ENOENT);
            return NULL;
        }
    }
    CFRelease(dict);
    isDir = S_ISDIR(st.st_mode);
    url = CFURLCreateFromFileSystemRepresentation(allocator, (const UInt8 *)path, strlen(path), isDir);
    if (isStale)
        *isStale = stale;
    return url;
}

CFDictionaryRef CFURLCreateResourcePropertiesForKeysFromBookmarkData(CFAllocatorRef allocator,
    CFArrayRef resourcePropertiesToReturn, CFDataRef bookmark)
{
    CFDictionaryRef dict = copyBookmark(bookmark);
    CFMutableDictionaryRef out;
    CFStringRef pathString;

    if (!dict)
        return NULL;
    out = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    pathString = CFDictionaryGetValue(dict, CFSTR("Path"));
    for (CFIndex i = 0, n = resourcePropertiesToReturn ? CFArrayGetCount(resourcePropertiesToReturn) : 0; i < n; i++) {
        CFStringRef key = CFArrayGetValueAtIndex(resourcePropertiesToReturn, i);

        if (CFEqual(key, kCFURLPathKey) && pathString)
            CFDictionarySetValue(out, key, pathString);
        else if (CFEqual(key, kCFURLNameKey) && pathString) {
            CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false);
            CFStringRef name = CFURLCopyLastPathComponent(url);

            CFDictionarySetValue(out, key, name);
            CFRelease(name);
            CFRelease(url);
        } else if (CFEqual(key, kCFURLIsDirectoryKey) && CFDictionaryGetValue(dict, CFSTR("IsDirectory")))
            CFDictionarySetValue(out, key, CFDictionaryGetValue(dict, CFSTR("IsDirectory")));
    }
    CFRelease(dict);
    return out;
}

CFTypeRef CFURLCreateResourcePropertyForKeyFromBookmarkData(CFAllocatorRef allocator, CFStringRef resourcePropertyKey,
    CFDataRef bookmark)
{
    CFArrayRef keys = CFArrayCreate(kCFAllocatorDefault, (const void **)&resourcePropertyKey, 1, &kCFTypeArrayCallBacks);
    CFDictionaryRef props = CFURLCreateResourcePropertiesForKeysFromBookmarkData(allocator, keys, bookmark);
    CFTypeRef value = props ? CFDictionaryGetValue(props, resourcePropertyKey) : NULL;

    if (value)
        CFRetain(value);
    if (props)
        CFRelease(props);
    CFRelease(keys);
    return value;
}

// an alias file holds its bookmark as the file contents
Boolean CFURLWriteBookmarkDataToFile(CFDataRef bookmarkRef, CFURLRef fileURL, CFURLBookmarkFileCreationOptions options,
    CFErrorRef *errorRef)
{
    char path[PATH_MAX];
    const UInt8 *bytes;
    CFIndex length;
    int fd;

    CFDictionaryRef dict = copyBookmark(bookmarkRef);

    if (dict)
        CFRelease(dict);
    if (!dict || !CFURLGetFileSystemRepresentation(fileURL, true, (UInt8 *)path, sizeof(path))) {
        if (errorRef)
            *errorRef = posixError(EINVAL);
        return false;
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        if (errorRef)
            *errorRef = posixError(errno);
        return false;
    }
    bytes = CFDataGetBytePtr(bookmarkRef);
    length = CFDataGetLength(bookmarkRef);
    while (length > 0) {
        ssize_t n = write(fd, bytes, length);

        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            int err = errno;

            close(fd);
            if (errorRef)
                *errorRef = posixError(err);
            return false;
        }
        bytes += n;
        length -= n;
    }
    close(fd);
    return true;
}

CFDataRef CFURLCreateBookmarkDataFromFile(CFAllocatorRef allocator, CFURLRef fileURL, CFErrorRef *errorRef)
{
    char path[PATH_MAX];
    struct stat st;
    CFMutableDataRef data;
    CFDictionaryRef dict;
    ssize_t n;
    int fd;

    if (!fileURL || !CFURLGetFileSystemRepresentation(fileURL, true, (UInt8 *)path, sizeof(path))) {
        if (errorRef)
            *errorRef = posixError(EINVAL);
        return NULL;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > 1 << 20) {
        int err = fd < 0 ? errno : EINVAL;

        if (fd >= 0)
            close(fd);
        if (errorRef)
            *errorRef = posixError(err);
        return NULL;
    }
    data = CFDataCreateMutable(allocator, 0);
    CFDataSetLength(data, st.st_size);
    n = read(fd, CFDataGetMutableBytePtr(data), st.st_size);
    close(fd);
    // anything but a bookmark is not an alias file
    dict = n == st.st_size ? copyBookmark(data) : NULL;
    if (!dict) {
        CFRelease(data);
        if (errorRef)
            *errorRef = posixError(EINVAL);
        return NULL;
    }
    CFRelease(dict);
    return data;
}
