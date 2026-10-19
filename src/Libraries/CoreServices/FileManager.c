// FileManager.c - the Carbon File Manager pieces still in use
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// FSRef as a volume and file id, FSFindFolder over the standard domain folders
// an FSRef is {'PDFR', reserved, fsid, ino} as CFURLGetFSRef shares, paths come back through fsgetpath(2)
#include <CoreServices/FileManager.h>
#include <CoreFoundation/CFURL.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PD_FSREF_MAGIC 0x50444652u // 'PDFR'

struct pd_fsref {
    uint32_t magic;
    uint32_t reserved;
    fsid_t fsid;
    uint64_t ino;
};

// the volume part compared between refs, fsid's first word is the device number
#define DEV(p) ((uint32_t)(p)->fsid.val[0])

// paths this process has turned into FSRefs, for objects fsgetpath can't name
struct cached {
    uint32_t dev;
    uint64_t ino;
    char *path;
    struct cached *next;
};

static pthread_mutex_t sCacheLock = PTHREAD_MUTEX_INITIALIZER;
static struct cached *sCache;

static void rememberPath(uint32_t dev, uint64_t ino, const char *path)
{
    pthread_mutex_lock(&sCacheLock);
    for (struct cached *c = sCache; c != NULL; c = c->next) {
        if (c->dev == dev && c->ino == ino) {
            if (strcmp(c->path, path) != 0) {
                char *copy = strdup(path);
                if (copy != NULL) {
                    free(c->path);
                    c->path = copy;
                }
            }
            pthread_mutex_unlock(&sCacheLock);
            return;
        }
    }
    struct cached *c = calloc(1, sizeof(*c));
    if (c != NULL && (c->path = strdup(path)) != NULL) {
        c->dev = dev;
        c->ino = ino;
        c->next = sCache;
        sCache = c;
    } else {
        free(c);
    }
    pthread_mutex_unlock(&sCacheLock);
}

static int cachedPath(uint32_t dev, uint64_t ino, char *out, size_t size)
{
    int found = 0;

    pthread_mutex_lock(&sCacheLock);
    for (struct cached *c = sCache; c != NULL; c = c->next) {
        if (c->dev == dev && c->ino == ino) {
            found = strlcpy(out, c->path, size) < size;
            break;
        }
    }
    pthread_mutex_unlock(&sCacheLock);
    return found;
}

OSStatus FSPathMakeRefWithOptions(const UInt8 *path, OptionBits options, FSRef *ref, Boolean *isDirectory)
{
    struct stat st;
    char resolved[PATH_MAX];

    if (path == NULL || ref == NULL)
        return paramErr;
    int r = (options & kFSPathMakeRefDoNotFollowLeafSymlink) ? lstat((const char *)path, &st) : stat((const char *)path, &st);
    if (r != 0)
        return errno == ENOENT || errno == ENOTDIR ? fnfErr : ioErr;
    if (realpath((const char *)path, resolved) == NULL)
        strlcpy(resolved, (const char *)path, sizeof(resolved));

    struct pd_fsref *p = (struct pd_fsref *)ref;
    memset(ref, 0, sizeof(*ref));
    p->magic = PD_FSREF_MAGIC;
    struct statfs sfs;
    p->fsid = statfs(resolved, &sfs) == 0 ? sfs.f_fsid : (fsid_t){ { (int32_t)st.st_dev, 0 } };
    p->fsid.val[0] = (int32_t)st.st_dev;
    p->ino = (uint64_t)st.st_ino;
    // made through CoreFoundation when it can, so CFURLCreateFromFSRef knows the ref and the fsid matches
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)resolved, (CFIndex)strlen(resolved), S_ISDIR(st.st_mode));
    if (url != NULL) {
        FSRef shared;

        if (CFURLGetFSRef(url, (struct FSRef *)&shared) && ((struct pd_fsref *)&shared)->ino == p->ino)
            *ref = shared;
        CFRelease(url);
    }
    rememberPath(DEV(p), p->ino, resolved);
    if (isDirectory != NULL)
        *isDirectory = S_ISDIR(st.st_mode);
    return noErr;
}

OSStatus FSPathMakeRef(const UInt8 *path, FSRef *ref, Boolean *isDirectory)
{
    return FSPathMakeRefWithOptions(path, 0, ref, isDirectory);
}

OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 maxPathSize)
{
    const struct pd_fsref *p = (const struct pd_fsref *)ref;
    struct stat st;

    if (ref == NULL || path == NULL || maxPathSize == 0)
        return paramErr;
    if (p->magic != PD_FSREF_MAGIC)
        return paramErr;
    if (cachedPath(DEV(p), p->ino, (char *)path, maxPathSize) && stat((char *)path, &st) == 0 &&
        (uint32_t)st.st_dev == DEV(p) && (uint64_t)st.st_ino == p->ino)
        return noErr;

    // the kernel rebuilds the path from the volume and the file id, as macOS does for an FSRef
    fsid_t fsid = p->fsid;
    ssize_t n = syscall(SYS_fsgetpath, (char *)path, (size_t)maxPathSize, &fsid, p->ino);
    if (n > 0)
        return noErr;
    return errno == ENOSPC || errno == ERANGE ? pathTooLongErr : fnfErr;
}

OSErr FSCompareFSRefs(const FSRef *ref1, const FSRef *ref2)
{
    const struct pd_fsref *a = (const struct pd_fsref *)ref1, *b = (const struct pd_fsref *)ref2;

    if (ref1 == NULL || ref2 == NULL || a->magic != PD_FSREF_MAGIC || b->magic != PD_FSREF_MAGIC)
        return paramErr;
    if (DEV(a) != DEV(b))
        return diffVolErr;
    return a->ino == b->ino ? noErr : errFSRefsDifferent;
}

// the home folder of the user running the process, $HOME first as the shell sees it
static int homeFolder(char *out, size_t size)
{
    const char *home = getenv("HOME");
    struct passwd *pw;

    if ((home == NULL || home[0] == '\0') && (pw = getpwuid(getuid())) != NULL)
        home = pw->pw_dir;
    return home != NULL && strlcpy(out, home, size) < size;
}

// where each folder type lives, relative to the domain's root: the user's home or /
static const char *folderPath(OSType type, short domain, int *userOnly)
{
    *userOnly = 0;
    switch (type) {
    case kApplicationSupportFolderType: return "Library/Application Support";
    case kCachedDataFolderType: return "Library/Caches";
    case kPreferencesFolderType: return "Library/Preferences";
    case kDomainLibraryFolderType: return "Library";
    case kFontsFolderType: return "Library/Fonts";
    case kInternetPlugInFolderType: return "Library/Internet Plug-Ins";
    case kLogsFolderType: return "Library/Logs";
    case kApplicationsFolderType: return "Applications";
    case kDesktopFolderType: *userOnly = 1; return "Desktop";
    case kDocumentsFolderType: *userOnly = 1; return "Documents";
    case kDownloadsFolderType: *userOnly = 1; return "Downloads";
    case kMovieDocumentsFolderType: *userOnly = 1; return "Movies";
    case kMusicDocumentsFolderType: *userOnly = 1; return "Music";
    case kPictureDocumentsFolderType: *userOnly = 1; return "Pictures";
    case kPublicFolderType: *userOnly = 1; return "Public";
    case kTrashFolderType: *userOnly = 1; return ".Trash";
    case kCurrentUserFolderType: *userOnly = 1; return "";
    case kSystemFolderType: return domain == kSystemDomain ? "System" : NULL;
    default: return NULL;
    }
}

OSErr FSFindFolder(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, FSRef *foundRef)
{
    char path[PATH_MAX];
    int userOnly;
    short domain = vRefNum;

    if (foundRef == NULL)
        return paramErr;
    // a volume number or kOnAppropriateDisk means the user's own folders, as for a single boot volume
    if (domain == kOnAppropriateDisk || domain >= 0)
        domain = kUserDomain;

    // temporary items live under the per-user temporary directory confstr hands out
    if (folderType == kTemporaryFolderType || folderType == kChewableItemsFolderType) {
        size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, path, sizeof(path));
        if (n == 0 || n > sizeof(path))
            strlcpy(path, "/tmp", sizeof(path));
    } else {
        const char *rel = folderPath(folderType, domain, &userOnly);
        char root[PATH_MAX];

        if (rel == NULL)
            return fnfErr;
        if (domain == kUserDomain) {
            if (!homeFolder(root, sizeof(root)))
                return fnfErr;
        } else if (userOnly) {
            return fnfErr;
        } else if (domain == kSystemDomain && folderType != kSystemFolderType) {
            strlcpy(root, "/System", sizeof(root));
        } else if (domain == kLocalDomain || domain == kSystemDomain) {
            strlcpy(root, "", sizeof(root));
        } else if (domain == kNetworkDomain) {
            strlcpy(root, "/Network", sizeof(root));
        } else {
            return paramErr;
        }
        if ((size_t)snprintf(path, sizeof(path), "%s%s%s", root, rel[0] ? "/" : "", rel) >= sizeof(path))
            return bdNamErr;
        if (path[0] == '\0')
            strlcpy(path, "/", sizeof(path));
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        if (!createFolder)
            return fnfErr;
        // create the missing tail one component at a time, as FSFindFolder does for user folders
        for (char *s = path + 1; *s; s++) {
            if (*s == '/') {
                *s = '\0';
                mkdir(path, 0755);
                *s = '/';
            }
        }
        if (mkdir(path, 0700) != 0 && errno != EEXIST)
            return errno == EACCES || errno == EPERM ? permErr : ioErr;
    }
    return FSPathMakeRef((const UInt8 *)path, foundRef, NULL);
}
