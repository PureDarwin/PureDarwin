// argon-root.c - PureDarwin's file system layout for Argon on Linux
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the file calls the frameworks make find /System, /Library and /Applications under $ARGON_ROOT
// libargonobjc loads ahead of libc, so every framework binds to these definitions

// fortify turns open and openat into overloads that cannot be named
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *argon_root(void)
{
    static const char *root;
    static int looked;

    if (!looked) {
        const char *env = getenv("ARGON_ROOT");
        root = (env && *env) ? env : NULL;
        looked = 1;
    }
    return root;
}

// the Darwin trees that live under the root, matched as whole path components
static int darwin_tree(const char *path)
{
    static const char *trees[] = { "/System", "/Library", "/Applications" };

    if (path == NULL || path[0] != '/')
        return 0;
    for (size_t i = 0; i < sizeof(trees) / sizeof(trees[0]); i++) {
        size_t n = strlen(trees[i]);
        if (strncmp(path, trees[i], n) == 0 && (path[n] == '/' || path[n] == '\0'))
            return 1;
    }
    return 0;
}

// the path a Darwin path names on this system, in buffer when it moves
static const char *argon_path(const char *path, char *buffer)
{
    const char *root = argon_root();

    if (root == NULL || !darwin_tree(path) || strncmp(path, root, strlen(root)) == 0)
        return path;
    if (snprintf(buffer, PATH_MAX, "%s%s", root, path) >= PATH_MAX)
        return path;
    return buffer;
}

typedef int open_fn(const char *, int, ...);
typedef int openat_fn(int, const char *, int, ...);
#define REAL_T(name, type) static type *real_##name; if (!real_##name) real_##name = (type *)dlsym(RTLD_NEXT, #name)
#define REAL(name) static __typeof__(name) *real_##name; if (!real_##name) real_##name = (__typeof__(name) *)dlsym(RTLD_NEXT, #name)
#define MAPPED(var) char var##_buf[PATH_MAX]; const char *var##_m = argon_path(var, var##_buf)

int open(const char *path, int flags, ...)
{
    REAL_T(open, open_fn);
    MAPPED(path);
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return real_open(path_m, flags, mode);
}

int open64(const char *path, int flags, ...)
{
    REAL_T(open64, open_fn);
    MAPPED(path);
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return real_open64(path_m, flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...)
{
    REAL_T(openat, openat_fn);
    MAPPED(path);
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return real_openat(dirfd, path_m, flags, mode);
}

FILE *fopen(const char *path, const char *mode) { REAL(fopen); MAPPED(path); return real_fopen(path_m, mode); }
FILE *fopen64(const char *path, const char *mode) { REAL(fopen64); MAPPED(path); return real_fopen64(path_m, mode); }
int stat(const char *path, struct stat *st) { REAL(stat); MAPPED(path); return real_stat(path_m, st); }
int lstat(const char *path, struct stat *st) { REAL(lstat); MAPPED(path); return real_lstat(path_m, st); }
int stat64(const char *path, struct stat64 *st) { REAL(stat64); MAPPED(path); return real_stat64(path_m, st); }
int lstat64(const char *path, struct stat64 *st) { REAL(lstat64); MAPPED(path); return real_lstat64(path_m, st); }
int fstatat(int dirfd, const char *path, struct stat *st, int flags) { REAL(fstatat); MAPPED(path); return real_fstatat(dirfd, path_m, st, flags); }
int fstatat64(int dirfd, const char *path, struct stat64 *st, int flags) { REAL(fstatat64); MAPPED(path); return real_fstatat64(dirfd, path_m, st, flags); }
int statx(int dirfd, const char *path, int flags, unsigned int mask, struct statx *st) { REAL(statx); MAPPED(path); return real_statx(dirfd, path_m, flags, mask, st); }
int access(const char *path, int mode) { REAL(access); MAPPED(path); return real_access(path_m, mode); }
int faccessat(int dirfd, const char *path, int mode, int flags) { REAL(faccessat); MAPPED(path); return real_faccessat(dirfd, path_m, mode, flags); }
DIR *opendir(const char *path) { REAL(opendir); MAPPED(path); return real_opendir(path_m); }
ssize_t readlink(const char *path, char *buf, size_t size) { REAL(readlink); MAPPED(path); return real_readlink(path_m, buf, size); }
int mkdir(const char *path, mode_t mode) { REAL(mkdir); MAPPED(path); return real_mkdir(path_m, mode); }
int rmdir(const char *path) { REAL(rmdir); MAPPED(path); return real_rmdir(path_m); }
int unlink(const char *path) { REAL(unlink); MAPPED(path); return real_unlink(path_m); }
int chdir(const char *path) { REAL(chdir); MAPPED(path); return real_chdir(path_m); }
int truncate(const char *path, off_t length) { REAL(truncate); MAPPED(path); return real_truncate(path_m, length); }
int chmod(const char *path, mode_t mode) { REAL(chmod); MAPPED(path); return real_chmod(path_m, mode); }
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) { REAL(utimensat); MAPPED(path); return real_utimensat(dirfd, path_m, times, flags); }

int rename(const char *from, const char *to)
{
    REAL(rename);
    MAPPED(from);
    MAPPED(to);
    return real_rename(from_m, to_m);
}

int symlink(const char *target, const char *path)
{
    REAL(symlink);
    MAPPED(path);
    return real_symlink(target, path_m);
}

// a resolved path names the real file, which is outside the Darwin trees
char *realpath(const char *path, char *resolved)
{
    REAL(realpath);
    MAPPED(path);
    return real_realpath(path_m, resolved);
}

// launching an app by its Darwin path, as the Dock and Finder do
int execve(const char *path, char *const argv[], char *const envp[]) { REAL(execve); MAPPED(path); return real_execve(path_m, argv, envp); }
int execv(const char *path, char *const argv[]) { REAL(execv); MAPPED(path); return real_execv(path_m, argv); }

int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *actions,
                const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
    REAL(posix_spawn);
    MAPPED(path);
    return real_posix_spawn(pid, path_m, actions, attr, argv, envp);
}

// with _FORTIFY_SOURCE the frameworks call glibc's checked variants of open and openat
typedef int open2_fn(const char *, int);
typedef int openat2_fn(int, const char *, int);
int __open_2(const char *path, int flags) { REAL_T(__open_2, open2_fn); MAPPED(path); return real___open_2(path_m, flags); }
int __open64_2(const char *path, int flags) { REAL_T(__open64_2, open2_fn); MAPPED(path); return real___open64_2(path_m, flags); }
int __openat_2(int dirfd, const char *path, int flags) { REAL_T(__openat_2, openat2_fn); MAPPED(path); return real___openat_2(dirfd, path_m, flags); }
int __openat64_2(int dirfd, const char *path, int flags) { REAL_T(__openat64_2, openat2_fn); MAPPED(path); return real___openat64_2(dirfd, path_m, flags); }
ssize_t __readlink_chk(const char *path, char *buf, size_t size, size_t bufsize) { (void)bufsize; return readlink(path, buf, size); }
char *__realpath_chk(const char *path, char *resolved, size_t resolvedlen) { (void)resolvedlen; return realpath(path, resolved); }
