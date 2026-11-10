// pd_dirhelper.c - per-user local, temporary and cache directories, which confstr hands out
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// per-user temporary folders, /private/var/folders/pd/<uid>/{0,T,C}/, named by uid
// the shared parent is sticky and world-writable like /tmp, so each user creates their own
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PD_FOLDERS "/private/var/folders/pd"

enum {
    DIRHELPER_USER_LOCAL = 0,
    DIRHELPER_USER_LOCAL_TEMP,
    DIRHELPER_USER_LOCAL_CACHE,
};

static char userDirSuffix[256];

// a process names its own subdirectory of the temporary and cache directories, once, before using them
bool _set_user_dir_suffix(const char *suffix)
{
    if (suffix == NULL) {
        userDirSuffix[0] = 0;
        return true;
    }
    if (*suffix == 0 || strchr(suffix, '/') || strcmp(suffix, ".") == 0 || strcmp(suffix, "..") == 0 ||
        strlen(suffix) >= sizeof(userDirSuffix)) {
        errno = EINVAL;
        return false;
    }
    strlcpy(userDirSuffix, suffix, sizeof(userDirSuffix));
    return true;
}

static bool makeDirectory(const char *path, mode_t mode)
{
    struct stat st;

    if (mkdir(path, mode) == 0)
        return chmod(path, mode) == 0 || errno == EPERM;
    return errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

char *_dirhelper(int which, char *path, size_t pathlen)
{
    static const char *const leaves[] = { "0", "T", "C" };
    char user[64];
    int length;

    if (which < DIRHELPER_USER_LOCAL || which > DIRHELPER_USER_LOCAL_CACHE || path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    snprintf(user, sizeof(user), PD_FOLDERS "/%u", (unsigned)geteuid());
    if (!makeDirectory("/private/var/folders", 0755) || !makeDirectory(PD_FOLDERS, 01777) || !makeDirectory(user, 0700))
        return NULL;
    length = snprintf(path, pathlen, "%s/%s/", user, leaves[which]);
    if (length < 0 || (size_t)length >= pathlen) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    if (!makeDirectory(path, 0700))
        return NULL;
    if (which != DIRHELPER_USER_LOCAL && userDirSuffix[0]) {
        length = snprintf(path, pathlen, "%s/%s/%s/", user, leaves[which], userDirSuffix);
        if (length < 0 || (size_t)length >= pathlen) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        if (!makeDirectory(path, 0700))
            return NULL;
    }
    return path;
}
