// sudo.c - PureDarwin native sudo
// Copyright (c) 2026 The PureDarwin Project
// SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "pdaccounts.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static void
usage(void)
{
    fprintf(stderr, "usage: sudo command [arguments ...]\n"
                    "       sudo -i\n");
    exit(2);
}

static int
is_admin(uid_t uid, char *name, size_t name_len)
{
    CFMutableDictionaryRef record = pda_copy_user_by_uid(uid);
    if (!record)
        return 0;

    int ok = pda_attr(record, "name", name, name_len) != 0 &&
             name[0] != '\0' &&
             pda_group_has_member("admin", name);

    CFRelease(record);
    return ok;
}

static int
authenticate(const char *name)
{
    CFMutableDictionaryRef record = pda_copy_user(name);
    if (!record)
        return 0;

    char *password = getpass("Password: ");
    int accepted = password && pda_check_password(record, password);
    if (password)
        memset(password, 0, strlen(password));

    CFRelease(record);
    return accepted;
}

static void
clean_environment(void)
{
    // Remove dynamic-loader and shell startup overrides.
    for (char **p = environ; *p;) {
        const char *entry = *p;
        const char *equals = strchr(entry, '=');
        size_t len = equals ? (size_t)(equals - entry) : strlen(entry);

        int remove =
            (len >= 5 && !strncmp(entry, "DYLD_", 5)) ||
            (len >= 3 && !strncmp(entry, "LD_", 3)) ||
            (len == 3 && !strncmp(entry, "IFS", 3)) ||
            (len == 3 && !strncmp(entry, "ENV", 3)) ||
            (len == 8 && !strncmp(entry, "BASH_ENV", 8)) ||
            (len == 6 && !strncmp(entry, "CDPATH", 6));

        if (remove) {
            char *name = strndup(entry, len);
            if (name) {
                unsetenv(name);
                free(name);
            }
        } else {
            ++p;
        }
    }
}

static void
become_root(void)
{
    struct passwd *root = getpwnam("root");
    if (!root) {
        fprintf(stderr, "sudo: cannot find root account\n");
        exit(1);
    }

    if (initgroups("root", 0) != 0 || setgid(0) != 0 || setuid(0) != 0) {
        fprintf(stderr, "sudo: cannot switch to root: %s\n", strerror(errno));
        exit(1);
    }
}

int
main(int argc, char **argv)
{
    int login_shell = argc == 2 && strcmp(argv[1], "-i") == 0;

    if (login_shell && argc != 2)
        usage();
    if (!login_shell && (argc < 2 || strcmp(argv[1], "-i") == 0))
        usage();

    uid_t invoking_uid = getuid();
    char username[256] = {0};

    if (invoking_uid != 0) {
        if (!is_admin(invoking_uid, username, sizeof(username))) {
            fprintf(stderr, "sudo: %s is not an administrator\n",
                    username[0] ? username : "current user");
            return 1;
        }

        if (!authenticate(username)) {
            fprintf(stderr, "sudo: authentication failed\n");
            return 1;
        }
    }

    clean_environment();
    become_root();

    if (login_shell) {
        struct passwd *root = getpwnam("root");
        if (!root || !root->pw_shell || !root->pw_shell[0]) {
            fprintf(stderr, "sudo: root has no configured login shell\n");
            return 1;
        }

        if (chdir(root->pw_dir && root->pw_dir[0] ? root->pw_dir : "/var/root") != 0) {
            fprintf(stderr, "sudo: cannot change to root's home: %s\n",
                    strerror(errno));
            return 1;
        }

        setenv("HOME", root->pw_dir && root->pw_dir[0] ? root->pw_dir : "/var/root", 1);
        setenv("USER", "root", 1);
        setenv("LOGNAME", "root", 1);

        const char *base = strrchr(root->pw_shell, '/');
        base = base ? base + 1 : root->pw_shell;

        char *login_name = malloc(strlen(base) + 2);
        if (!login_name) {
            perror("sudo");
            return 1;
        }
        login_name[0] = '-';
        strcpy(login_name + 1, base);

        char *const shell_argv[] = { login_name, "-i", NULL };
        execv(root->pw_shell, shell_argv);
        fprintf(stderr, "sudo: cannot execute %s: %s\n",
                root->pw_shell, strerror(errno));
        return 1;
    }

    execvp(argv[1], &argv[1]);
    fprintf(stderr, "sudo: cannot execute %s: %s\n", argv[1], strerror(errno));
    return errno == ENOENT ? 127 : 126;
}
