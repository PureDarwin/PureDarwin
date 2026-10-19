// CFURLResources_PureDarwin.c - file resource properties, directory enumeration and FSRefs
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// CFURL.c forwards resource properties to the Swift bridge only, these read the file system directly
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include "CFURLEnumerator.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <sys/attr.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <os/lock.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/syscall.h>
#ifndef SYS_fsgetpath
#define SYS_fsgetpath 427
#endif

// libSystem does not export the fsgetpath wrapper yet
static ssize_t pdFsgetpath(char *buf, size_t size, fsid_t *fsid, uint64_t objid) {
    return syscall(SYS_fsgetpath, buf, size, fsid, objid);
}

static bool urlPath(CFURLRef url, char *path) {
    return url && CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, PATH_MAX);
}

static CFErrorRef createPOSIXError(int code, CFURLRef url) {
    const void *key = kCFErrorURLKey;
    CFDictionaryRef info = url ? CFDictionaryCreate(NULL, &key, (const void **)&url, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) : NULL;
    CFErrorRef error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, code, info);

    if (info) CFRelease(info);
    return error;
}

static CFDateRef createDate(struct timespec ts) {
    return CFDateCreate(NULL, (CFAbsoluteTime)ts.tv_sec - kCFAbsoluteTimeIntervalSince1970 + ts.tv_nsec / 1e9);
}

static CFNumberRef createNumber64(int64_t value) {
    return CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
}

static CFTypeRef retainedBool(bool value) {
    return CFRetain(value ? kCFBooleanTrue : kCFBooleanFalse);
}

static bool hasExtension(const char *path, const char *const *extensions) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');

    if (dot == NULL || (slash && dot < slash)) return false;
    for (int i = 0; extensions[i]; i++)
        if (strcasecmp(dot + 1, extensions[i]) == 0) return true;
    return false;
}

static const char *const packageExtensions[] = { "app", "bundle", "framework", "plugin", "kext", "pkg", "mpkg", "xpc", "appex", "qlgenerator", "mdimporter", "prefPane", "saver", "rtfd", NULL };
static const char *const applicationExtensions[] = { "app", NULL };

// temporary properties live per URL string for the life of the process
static os_unfair_lock temporaryLock = OS_UNFAIR_LOCK_INIT;
static CFMutableDictionaryRef temporaryProperties;

static CFTypeRef copyTemporary(CFURLRef url, CFStringRef key) {
    CFTypeRef value = NULL;

    os_unfair_lock_lock(&temporaryLock);
    if (temporaryProperties) {
        CFDictionaryRef d = CFDictionaryGetValue(temporaryProperties, CFURLGetString(url));

        if (d) value = CFDictionaryGetValue(d, key);
        if (value) CFRetain(value);
    }
    os_unfair_lock_unlock(&temporaryLock);
    return value;
}

void CFURLSetTemporaryResourcePropertyForKey(CFURLRef url, CFStringRef key, CFTypeRef propertyValue) {
    CFMutableDictionaryRef d;

    if (url == NULL || key == NULL) return;
    os_unfair_lock_lock(&temporaryLock);
    if (temporaryProperties == NULL)
        temporaryProperties = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    d = (CFMutableDictionaryRef)CFDictionaryGetValue(temporaryProperties, CFURLGetString(url));
    if (d == NULL) {
        d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(temporaryProperties, CFURLGetString(url), d);
        CFRelease(d);
    }
    if (propertyValue) CFDictionarySetValue(d, key, propertyValue);
    else CFDictionaryRemoveValue(d, key);
    os_unfair_lock_unlock(&temporaryLock);
}

// values are read fresh each time, so there is nothing cached to clear
void CFURLClearResourcePropertyCacheForKey(CFURLRef url, CFStringRef key) {
}

void CFURLClearResourcePropertyCache(CFURLRef url) {
}

static CFStringRef copyLastComponent(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *name = slash && slash[1] ? slash + 1 : path;

    if (strcmp(path, "/") == 0) name = "/";
    return CFStringCreateWithFileSystemRepresentation(NULL, name);
}

static CFTypeRef copyVolumeProperty(CFStringRef key, const char *path, CFURLRef url, bool *known) {
    struct statfs fs;

    *known = true;
    if (statfs(path, &fs) != 0) return NULL;
    if (CFEqual(key, kCFURLVolumeURLKey))
        return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)fs.f_mntonname, strlen(fs.f_mntonname), true);
    if (CFEqual(key, kCFURLVolumeNameKey) || CFEqual(key, kCFURLVolumeLocalizedNameKey))
        return copyLastComponent(fs.f_mntonname);
    if (CFEqual(key, kCFURLVolumeIdentifierKey))
        return CFDataCreate(NULL, (const UInt8 *)&fs.f_fsid, sizeof(fs.f_fsid));
    if (CFEqual(key, kCFURLVolumeTotalCapacityKey))
        return createNumber64((int64_t)fs.f_blocks * fs.f_bsize);
    if (CFEqual(key, kCFURLVolumeAvailableCapacityKey) || CFEqual(key, kCFURLVolumeAvailableCapacityForImportantUsageKey) ||
        CFEqual(key, kCFURLVolumeAvailableCapacityForOpportunisticUsageKey))
        return createNumber64((int64_t)fs.f_bavail * fs.f_bsize);
    if (CFEqual(key, kCFURLVolumeResourceCountKey))
        return createNumber64((int64_t)(fs.f_files - fs.f_ffree));
    if (CFEqual(key, kCFURLVolumeIsReadOnlyKey)) return retainedBool(fs.f_flags & MNT_RDONLY);
    if (CFEqual(key, kCFURLVolumeIsLocalKey)) return retainedBool(fs.f_flags & MNT_LOCAL);
    if (CFEqual(key, kCFURLVolumeIsRootFileSystemKey)) return retainedBool(fs.f_flags & MNT_ROOTFS);
    if (CFEqual(key, kCFURLVolumeIsAutomountedKey)) return retainedBool(fs.f_flags & MNT_AUTOMOUNTED);
    if (CFEqual(key, kCFURLVolumeIsBrowsableKey)) return retainedBool(!(fs.f_flags & MNT_DONTBROWSE));
    if (CFEqual(key, kCFURLVolumeIsJournalingKey)) return retainedBool(fs.f_flags & MNT_JOURNALED);
    if (CFEqual(key, kCFURLVolumeIsInternalKey)) return retainedBool(fs.f_flags & MNT_LOCAL);
    if (CFEqual(key, kCFURLVolumeIsEjectableKey) || CFEqual(key, kCFURLVolumeIsRemovableKey) || CFEqual(key, kCFURLVolumeIsEncryptedKey))
        return retainedBool(false);
    if (CFEqual(key, kCFURLVolumeLocalizedFormatDescriptionKey))
        return CFStringCreateWithCString(NULL, fs.f_fstypename, kCFStringEncodingUTF8);
    if (CFEqual(key, kCFURLVolumeSupportsSymbolicLinksKey) || CFEqual(key, kCFURLVolumeSupportsHardLinksKey) ||
        CFEqual(key, kCFURLVolumeSupportsCasePreservedNamesKey) || CFEqual(key, kCFURLVolumeSupportsVolumeSizesKey) ||
        CFEqual(key, kCFURLVolumeSupportsRenamingKey) || CFEqual(key, kCFURLVolumeSupportsAdvisoryFileLockingKey) ||
        CFEqual(key, kCFURLVolumeSupportsAccessPermissionsKey) || CFEqual(key, kCFURLVolumeSupportsPersistentIDsKey))
        return retainedBool(true);
    if (CFEqual(key, kCFURLVolumeSupportsCaseSensitiveNamesKey))
        return retainedBool(strcmp(fs.f_fstypename, "ext4") == 0);
    *known = false;
    return NULL;
}

static CFTypeRef copyProperty(CFURLRef url, CFStringRef key, CFErrorRef *error, bool *failed) {
    char path[PATH_MAX];
    struct stat st;
    CFTypeRef temporary = copyTemporary(url, key);
    bool known;

    *failed = false;
    if (temporary) return temporary;
    if (!CFURLIsFileReferenceURL(url) && !urlPath(url, path)) {
        *failed = true;
        if (error) *error = createPOSIXError(EINVAL, url);
        return NULL;
    }
    if (!urlPath(url, path)) {
        *failed = true;
        return NULL;
    }
    if (CFEqual(key, kCFURLPathKey) || CFEqual(key, kCFURLCanonicalPathKey))
        return CFStringCreateWithFileSystemRepresentation(NULL, path);
    if (lstat(path, &st) != 0) {
        *failed = true;
        if (error) *error = createPOSIXError(errno, url);
        return NULL;
    }
    // a symbolic link answers for itself, the link is never followed
    if (CFEqual(key, kCFURLNameKey) || CFEqual(key, kCFURLLocalizedNameKey)) return copyLastComponent(path);
    if (CFEqual(key, kCFURLIsRegularFileKey)) return retainedBool(S_ISREG(st.st_mode));
    if (CFEqual(key, kCFURLIsDirectoryKey)) return retainedBool(S_ISDIR(st.st_mode));
    if (CFEqual(key, kCFURLIsSymbolicLinkKey)) return retainedBool(S_ISLNK(st.st_mode));
    if (CFEqual(key, kCFURLIsAliasFileKey)) return retainedBool(S_ISLNK(st.st_mode));
    if (CFEqual(key, kCFURLIsVolumeKey)) {
        struct statfs fs;

        return retainedBool(statfs(path, &fs) == 0 && strcmp(fs.f_mntonname, path) == 0);
    }
    if (CFEqual(key, kCFURLIsPackageKey)) return retainedBool(S_ISDIR(st.st_mode) && hasExtension(path, packageExtensions));
    if (CFEqual(key, kCFURLIsApplicationKey)) return retainedBool(S_ISDIR(st.st_mode) && hasExtension(path, applicationExtensions));
    if (CFEqual(key, kCFURLApplicationIsScriptableKey)) return retainedBool(false);
    if (CFEqual(key, kCFURLIsSystemImmutableKey)) return retainedBool(st.st_flags & SF_IMMUTABLE);
    if (CFEqual(key, kCFURLIsUserImmutableKey)) return retainedBool(st.st_flags & UF_IMMUTABLE);
    if (CFEqual(key, kCFURLIsHiddenKey)) {
        const char *slash = strrchr(path, '/');

        return retainedBool((st.st_flags & UF_HIDDEN) || (slash && slash[1] == '.'));
    }
    if (CFEqual(key, kCFURLHasHiddenExtensionKey)) return retainedBool(false);
    if (CFEqual(key, kCFURLCreationDateKey)) return createDate(st.st_birthtimespec.tv_sec ? st.st_birthtimespec : st.st_ctimespec);
    if (CFEqual(key, kCFURLContentAccessDateKey)) return createDate(st.st_atimespec);
    if (CFEqual(key, kCFURLContentModificationDateKey)) return createDate(st.st_mtimespec);
    if (CFEqual(key, kCFURLAttributeModificationDateKey)) return createDate(st.st_ctimespec);
    if (CFEqual(key, kCFURLLinkCountKey)) return createNumber64(st.st_nlink);
    if (CFEqual(key, kCFURLParentDirectoryURLKey)) {
        CFURLRef parent = CFURLCreateCopyDeletingLastPathComponent(NULL, url);

        return parent;
    }
    if (CFEqual(key, kCFURLFileResourceIdentifierKey)) {
        struct { dev_t dev; ino_t ino; } id = { st.st_dev, st.st_ino };

        return CFDataCreate(NULL, (const UInt8 *)&id, sizeof(id));
    }
    if (CFEqual(key, kCFURLGenerationIdentifierKey)) return createNumber64(st.st_gen);
    if (CFEqual(key, kCFURLDocumentIdentifierKey)) return NULL;
    if (CFEqual(key, kCFURLPreferredIOBlockSizeKey)) return createNumber64(st.st_blksize);
    if (CFEqual(key, kCFURLIsReadableKey)) return retainedBool(access(path, R_OK) == 0);
    if (CFEqual(key, kCFURLIsWritableKey)) return retainedBool(access(path, W_OK) == 0);
    if (CFEqual(key, kCFURLIsExecutableKey)) return retainedBool(access(path, X_OK) == 0);
    if (CFEqual(key, kCFURLIsExcludedFromBackupKey)) return retainedBool(getxattr(path, "com.apple.metadata:com_apple_backup_excludeItem", NULL, 0, 0, 0) > 0);
    if (CFEqual(key, kCFURLTagNamesKey)) return NULL;
    if (CFEqual(key, kCFURLIsMountTriggerKey)) return retainedBool(false);
    if (CFEqual(key, kCFURLIsUbiquitousItemKey)) return retainedBool(false);
    if (CFEqual(key, kCFURLFileResourceTypeKey)) {
        CFStringRef type = S_ISREG(st.st_mode) ? kCFURLFileResourceTypeRegular : S_ISDIR(st.st_mode) ? kCFURLFileResourceTypeDirectory :
                           S_ISLNK(st.st_mode) ? kCFURLFileResourceTypeSymbolicLink : S_ISFIFO(st.st_mode) ? kCFURLFileResourceTypeNamedPipe :
                           S_ISCHR(st.st_mode) ? kCFURLFileResourceTypeCharacterSpecial : S_ISBLK(st.st_mode) ? kCFURLFileResourceTypeBlockSpecial :
                           S_ISSOCK(st.st_mode) ? kCFURLFileResourceTypeSocket : kCFURLFileResourceTypeUnknown;

        return CFRetain(type);
    }
    if (CFEqual(key, kCFURLFileSizeKey) || CFEqual(key, kCFURLTotalFileSizeKey))
        return S_ISDIR(st.st_mode) ? NULL : createNumber64(st.st_size);
    if (CFEqual(key, kCFURLFileAllocatedSizeKey) || CFEqual(key, kCFURLTotalFileAllocatedSizeKey))
        return S_ISDIR(st.st_mode) ? NULL : createNumber64(st.st_blocks * 512);
    if (CFEqual(key, kCFURLQuarantinePropertiesKey)) {
        char value[512];
        ssize_t n = getxattr(path, "com.apple.quarantine", value, sizeof(value) - 1, 0, 0);

        if (n <= 0) return NULL;
        value[n] = 0;
        // agent name and flags travel in the attribute as flags;timestamp;agent;uuid
        CFMutableDictionaryRef q = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFStringRef raw = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);

        if (raw) {
            CFDictionarySetValue(q, CFSTR("LSQuarantineRaw"), raw);
            CFRelease(raw);
        }
        return q;
    }
    if (CFEqual(key, kCFURLTypeIdentifierKey) || CFEqual(key, kCFURLLocalizedTypeDescriptionKey) || CFEqual(key, kCFURLEffectiveIconKey) ||
        CFEqual(key, kCFURLCustomIconKey) || CFEqual(key, kCFURLLabelColorKey) || CFEqual(key, kCFURLLocalizedLabelKey) ||
        CFEqual(key, kCFURLFileSecurityKey) || CFEqual(key, kCFURLAddedToDirectoryDateKey))
        return NULL;
    if (CFEqual(key, kCFURLLabelNumberKey)) return createNumber64(0);
    {
        CFTypeRef v = copyVolumeProperty(key, path, url, &known);

        if (known) return v;
    }
    return NULL;
}

Boolean CFURLCopyResourcePropertyForKey(CFURLRef url, CFStringRef key, void *propertyValueTypeRefPtr, CFErrorRef *error) {
    bool failed;
    CFTypeRef value;

    if (propertyValueTypeRefPtr == NULL || url == NULL || key == NULL) return false;
    value = copyProperty(url, key, error, &failed);
    *(CFTypeRef *)propertyValueTypeRefPtr = value;
    return !failed;
}

CFDictionaryRef CFURLCopyResourcePropertiesForKeys(CFURLRef url, CFArrayRef keys, CFErrorRef *error) {
    CFMutableDictionaryRef result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    for (CFIndex i = 0; keys && i < CFArrayGetCount(keys); i++) {
        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
        bool failed;
        CFTypeRef value = copyProperty(url, key, error, &failed);

        if (failed) {
            CFRelease(result);
            return NULL;
        }
        if (value) {
            CFDictionarySetValue(result, key, value);
            CFRelease(value);
        }
    }
    return result;
}

static struct timespec timespecOf(CFDateRef date) {
    CFAbsoluteTime t = CFDateGetAbsoluteTime(date) + kCFAbsoluteTimeIntervalSince1970;
    struct timespec ts = { (time_t)t, (long)((t - (time_t)t) * 1e9) };

    return ts;
}

Boolean CFURLSetResourcePropertyForKey(CFURLRef url, CFStringRef key, CFTypeRef value, CFErrorRef *error) {
    char path[PATH_MAX];
    struct stat st;
    int rc = 0;

    if (!urlPath(url, path) || lstat(path, &st) != 0) {
        if (error) *error = createPOSIXError(errno ? errno : EINVAL, url);
        return false;
    }
    if (CFEqual(key, kCFURLContentModificationDateKey) || CFEqual(key, kCFURLContentAccessDateKey)) {
        struct timespec times[2] = { st.st_atimespec, st.st_mtimespec };
        struct timeval tv[2];

        if (value == NULL || CFGetTypeID(value) != CFDateGetTypeID()) return false;
        times[CFEqual(key, kCFURLContentAccessDateKey) ? 0 : 1] = timespecOf(value);
        for (int i = 0; i < 2; i++) {
            tv[i].tv_sec = times[i].tv_sec;
            tv[i].tv_usec = (int)(times[i].tv_nsec / 1000);
        }
        rc = lutimes(path, tv);
    } else if (CFEqual(key, kCFURLIsHiddenKey)) {
        rc = lchflags(path, value == kCFBooleanTrue ? (st.st_flags | UF_HIDDEN) : (st.st_flags & ~UF_HIDDEN));
    } else if (CFEqual(key, kCFURLIsUserImmutableKey)) {
        rc = lchflags(path, value == kCFBooleanTrue ? (st.st_flags | UF_IMMUTABLE) : (st.st_flags & ~UF_IMMUTABLE));
    } else if (CFEqual(key, kCFURLIsExcludedFromBackupKey)) {
        if (value == kCFBooleanTrue) rc = setxattr(path, "com.apple.metadata:com_apple_backup_excludeItem", "com.apple.backupd", 17, 0, XATTR_NOFOLLOW);
        else { removexattr(path, "com.apple.metadata:com_apple_backup_excludeItem", XATTR_NOFOLLOW); rc = 0; }
    } else if (CFEqual(key, kCFURLQuarantinePropertiesKey)) {
        // the quarantine attribute records the agent and download time in the usual layout
        if (value == NULL || CFGetTypeID(value) != CFDictionaryGetTypeID()) {
            removexattr(path, "com.apple.quarantine", XATTR_NOFOLLOW);
        } else {
            char agent[128] = "unknown", line[256];
            CFStringRef name = CFDictionaryGetValue(value, CFSTR("LSQuarantineAgentName"));

            if (name && CFGetTypeID(name) == CFStringGetTypeID()) CFStringGetCString(name, agent, sizeof(agent), kCFStringEncodingUTF8);
            snprintf(line, sizeof(line), "0083;%08lx;%s;", (unsigned long)time(NULL), agent);
            rc = setxattr(path, "com.apple.quarantine", line, strlen(line), 0, XATTR_NOFOLLOW);
            // file systems without extended attributes still accept the request, as Apple's do for quarantine
            if (rc != 0 && errno == ENOTSUP) rc = 0;
        }
    } else {
        CFURLSetTemporaryResourcePropertyForKey(url, key, value);
    }
    if (rc != 0) {
        if (error) *error = createPOSIXError(errno, url);
        return false;
    }
    return true;
}

Boolean CFURLSetResourcePropertiesForKeys(CFURLRef url, CFDictionaryRef keyedValues, CFErrorRef *error) {
    CFIndex n = keyedValues ? CFDictionaryGetCount(keyedValues) : 0;
    const void *keys[n > 0 ? n : 1], *values[n > 0 ? n : 1];

    if (keyedValues) CFDictionaryGetKeysAndValues(keyedValues, keys, values);
    for (CFIndex i = 0; i < n; i++)
        if (!CFURLSetResourcePropertyForKey(url, keys[i], values[i], error)) return false;
    return true;
}

#pragma mark FSRef

// FSRef layout shared with CoreServices: magic, fsid, object id, rest zero
typedef struct {
    uint32_t magic;
    uint32_t reserved;
    fsid_t fsid;
    uint64_t objectID;
    UInt8 padding[80 - 24];
} PDFSRef;

#define PDFSRefMagic 0x50444652

// object ids seen in this process, so file systems without fsgetpath still map back
static os_unfair_lock fsrefLock = OS_UNFAIR_LOCK_INIT;
static CFMutableDictionaryRef fsrefPaths;

Boolean CFURLGetFSRef(CFURLRef url, struct FSRef *fsRef) {
    char path[PATH_MAX];
    struct stat st;
    struct statfs fs;
    PDFSRef *ref = (PDFSRef *)fsRef;
    CFDataRef key;
    CFStringRef value;

    if (fsRef == NULL || !urlPath(url, path) || stat(path, &st) != 0 || statfs(path, &fs) != 0) return false;
    memset(ref, 0, sizeof(*ref));
    ref->magic = PDFSRefMagic;
    ref->fsid = fs.f_fsid;
    ref->objectID = st.st_ino;
    key = CFDataCreate(NULL, (const UInt8 *)ref, 24);
    value = CFStringCreateWithFileSystemRepresentation(NULL, path);
    os_unfair_lock_lock(&fsrefLock);
    if (fsrefPaths == NULL) fsrefPaths = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (value) CFDictionarySetValue(fsrefPaths, key, value);
    os_unfair_lock_unlock(&fsrefLock);
    CFRelease(key);
    if (value) CFRelease(value);
    return true;
}

CFURLRef CFURLCreateFromFSRef(CFAllocatorRef allocator, const struct FSRef *fsRef) {
    const PDFSRef *ref = (const PDFSRef *)fsRef;
    char path[PATH_MAX];
    struct stat st;
    CFDataRef key;
    CFStringRef known = NULL;

    if (ref == NULL || ref->magic != PDFSRefMagic) return NULL;
    key = CFDataCreate(NULL, (const UInt8 *)ref, 24);
    os_unfair_lock_lock(&fsrefLock);
    if (fsrefPaths) known = CFDictionaryGetValue(fsrefPaths, key);
    if (known) CFRetain(known);
    os_unfair_lock_unlock(&fsrefLock);
    CFRelease(key);
    if (known) {
        bool ok = CFStringGetFileSystemRepresentation(known, path, sizeof(path));

        CFRelease(known);
        if (!ok || stat(path, &st) != 0 || st.st_ino != ref->objectID) known = NULL;
    }
    if (known == NULL) {
        fsid_t fsid = ref->fsid;

        if (pdFsgetpath(path, sizeof(path), &fsid, ref->objectID) < 0 || stat(path, &st) != 0) return NULL;
    }
    return CFURLCreateFromFileSystemRepresentation(allocator, (const UInt8 *)path, strlen(path), S_ISDIR(st.st_mode));
}

#pragma mark CFURLEnumerator

typedef struct {
    DIR *dir;
    char *path;
} EnumeratorLevel;

struct __CFURLEnumerator {
    CFRuntimeBase base;
    CFURLRef root;
    CFURLEnumeratorOptions options;
    EnumeratorLevel *levels;
    CFIndex depth;
    CFIndex capacity;
    CFURLRef current;
    char *pendingPostOrder;
    bool skipCurrent;
    char *currentDir;
    CFArrayRef volumes;
    CFIndex volumeIndex;
};

static void enumeratorDeallocate(CFTypeRef cf) {
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)cf;

    for (CFIndex i = 0; i < e->depth; i++) {
        closedir(e->levels[i].dir);
        free(e->levels[i].path);
    }
    free(e->levels);
    free(e->pendingPostOrder);
    free(e->currentDir);
    if (e->current) CFRelease(e->current);
    if (e->root) CFRelease(e->root);
    if (e->volumes) CFRelease(e->volumes);
}

static CFTypeID enumeratorTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass enumeratorClass = {
    0, "CFURLEnumerator", NULL, NULL, enumeratorDeallocate, NULL, NULL, NULL, NULL
};

CFTypeID CFURLEnumeratorGetTypeID(void) {
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        enumeratorTypeID = _CFRuntimeRegisterClass(&enumeratorClass);
    });
    return enumeratorTypeID;
}

static struct __CFURLEnumerator *createEnumerator(CFAllocatorRef allocator, CFURLEnumeratorOptions options) {
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)_CFRuntimeCreateInstance(allocator, CFURLEnumeratorGetTypeID(),
        sizeof(struct __CFURLEnumerator) - sizeof(CFRuntimeBase), NULL);

    if (e == NULL) return NULL;
    e->options = options;
    return e;
}

static bool pushLevel(struct __CFURLEnumerator *e, const char *path) {
    DIR *dir = opendir(path);

    if (dir == NULL) return false;
    if (e->depth == e->capacity) {
        e->capacity = e->capacity ? e->capacity * 2 : 8;
        e->levels = realloc(e->levels, e->capacity * sizeof(EnumeratorLevel));
    }
    e->levels[e->depth].dir = dir;
    e->levels[e->depth].path = strdup(path);
    e->depth++;
    return true;
}

CFURLEnumeratorRef CFURLEnumeratorCreateForDirectoryURL(CFAllocatorRef allocator, CFURLRef directoryURL, CFURLEnumeratorOptions option, CFArrayRef propertyKeys) {
    char path[PATH_MAX];
    struct __CFURLEnumerator *e;

    if (!urlPath(directoryURL, path)) return NULL;
    e = createEnumerator(allocator, option);
    if (e == NULL) return NULL;
    e->root = CFRetain(directoryURL);
    // an unreadable root shows up as an error on the first call
    pushLevel(e, path);
    return (CFURLEnumeratorRef)e;
}

CFURLEnumeratorRef CFURLEnumeratorCreateForMountedVolumes(CFAllocatorRef allocator, CFURLEnumeratorOptions option, CFArrayRef propertyKeys) {
    struct __CFURLEnumerator *e = createEnumerator(allocator, option);
    CFMutableArrayRef volumes;
    struct statfs *mounts;
    int n;

    if (e == NULL) return NULL;
    volumes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    n = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < n; i++) {
        CFURLRef url;

        if ((option & kCFURLEnumeratorSkipInvisibles) && (mounts[i].f_flags & MNT_DONTBROWSE)) continue;
        url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)mounts[i].f_mntonname, strlen(mounts[i].f_mntonname), true);
        CFArrayAppendValue(volumes, url);
        CFRelease(url);
    }
    e->volumes = volumes;
    return (CFURLEnumeratorRef)e;
}

static void setCurrent(struct __CFURLEnumerator *e, const char *path, bool isDirectory) {
    if (e->current) CFRelease(e->current);
    e->current = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), isDirectory);
}

CFURLEnumeratorResult CFURLEnumeratorGetNextURL(CFURLEnumeratorRef enumerator, CFURLRef *url, CFErrorRef *error) {
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)enumerator;

    if (url) *url = NULL;
    if (e->volumes) {
        if (e->volumeIndex >= CFArrayGetCount(e->volumes)) return kCFURLEnumeratorEnd;
        if (e->current) CFRelease(e->current);
        e->current = CFRetain(CFArrayGetValueAtIndex(e->volumes, e->volumeIndex++));
        if (url) *url = e->current;
        return kCFURLEnumeratorSuccess;
    }
    // descend into the directory returned last time unless the caller skipped it
    if (e->currentDir) {
        char *dir = e->currentDir;

        e->currentDir = NULL;
        if (!e->skipCurrent && !pushLevel(e, dir)) {
            if (error) *error = createPOSIXError(errno, e->current);
            free(dir);
            return kCFURLEnumeratorError;
        }
        free(dir);
    }
    e->skipCurrent = false;
    if (e->depth == 0) {
        char path[PATH_MAX];

        if (e->root && urlPath(e->root, path) && access(path, R_OK) != 0 && e->current == NULL) {
            if (error) *error = createPOSIXError(errno, e->root);
            CFRelease(e->root);
            e->root = NULL;
            return kCFURLEnumeratorError;
        }
        return kCFURLEnumeratorEnd;
    }
    for (;;) {
        EnumeratorLevel *level = &e->levels[e->depth - 1];
        struct dirent *entry = readdir(level->dir);
        char path[PATH_MAX];
        struct stat st;
        bool isDirectory;

        if (entry == NULL) {
            closedir(level->dir);
            e->depth--;
            if ((e->options & kCFURLEnumeratorIncludeDirectoriesPostOrder) && e->depth > 0) {
                setCurrent(e, level->path, true);
                free(level->path);
                if (url) *url = e->current;
                return kCFURLEnumeratorDirectoryPostOrderSuccess;
            }
            free(level->path);
            if (e->depth == 0) return kCFURLEnumeratorEnd;
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if ((e->options & kCFURLEnumeratorSkipInvisibles) && entry->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s%s%s", level->path, level->path[strlen(level->path) - 1] == '/' ? "" : "/", entry->d_name);
        isDirectory = lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
        setCurrent(e, path, isDirectory);
        if (isDirectory && (e->options & kCFURLEnumeratorDescendRecursively)) {
            bool package = hasExtension(path, packageExtensions);

            if (!(package && (e->options & kCFURLEnumeratorSkipPackageContents)))
                e->currentDir = strdup(path);
        }
        if (url) *url = e->current;
        return kCFURLEnumeratorSuccess;
    }
}

void CFURLEnumeratorSkipDescendents(CFURLEnumeratorRef enumerator) {
    ((struct __CFURLEnumerator *)enumerator)->skipCurrent = true;
}

CFIndex CFURLEnumeratorGetDescendentLevel(CFURLEnumeratorRef enumerator) {
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)enumerator;

    return e->volumes ? 1 : e->depth;
}

Boolean CFURLEnumeratorGetSourceDidChange(CFURLEnumeratorRef enumerator) {
    return false;
}
