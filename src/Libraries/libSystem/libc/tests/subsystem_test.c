/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
*
* @APPLE_OSREFERENCE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. The rights granted to you under the License
* may not be used to create, or enable the creation or redistribution of,
* unlawful or unlicensed copies of an Apple operating system, or to
* circumvent, violate, or enable the circumvention or violation of, any
* terms of an Apple operating system software license agreement.
*
* Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_OSREFERENCE_LICENSE_HEADER_END@
*/

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <spawn_private.h>
#include <sys/wait.h>
#include <unistd.h>
#include "subsystem_test.h"

#include <darwintest.h>
#include <darwintest_utils.h>

#define RANDOM_STRING_LEN 33

#define HELPER_PATH "./subsystem_test_helper"

/* Create the given file.  Doesn't create directories. */
static bool
_create_file(char * const filepath)
{
    bool success = false;
    int fd = open(filepath, O_CREAT | O_EXCL, 0666);
    
    if (fd >= 0) {
        close(fd);
        success = true;
    }
    
    return success;
}

/* Fills the given buffer with a random alphanumeric string. */
static void
_generate_random_string(char * buf, size_t buf_len)
{
   
    static char _alphanumeric[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t cur_byte = 0;
    
    if (buf_len == 0) {
        return;
    }
    
    for (cur_byte = 0; ((buf_len - cur_byte) > 1); cur_byte++) {
        buf[cur_byte] = _alphanumeric[rand() % (sizeof(_alphanumeric) - 1)];
    }
    
    buf[cur_byte] = 0;
    
    return;
}

/* Compares the contents of the given file with the buffer. */
static int
_check_file_contents(char * const filepath, char * const buf, size_t buf_len)
{
    int result = 1;
    int fd = -1;
    char file_buf[buf_len];
    
    fd = open(filepath, O_RDONLY);
    
    if (fd >= 0) {
        read(fd, file_buf, buf_len);
        close(fd);
        
        result = memcmp(buf, file_buf, buf_len);
    }
    
    return result;
}

/* Spawn with the given args and attributes, and waits for the child. */
static int
_spawn_and_wait(char ** args, posix_spawnattr_t *attr)
{
    int pid;
    int status;

    if (posix_spawn(&pid, args[0], NULL, attr, args, NULL)) {
        return -1;
    }
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }

    if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
        return 0;
    }

    return -1;
}

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

T_DECL(subsystem,
    "Test the subsystem-related functions",
    T_META_CHECK_LEAKS(false))
{
    int result = 0;
    int pid = 0;
    posix_spawnattr_t attr = NULL;
    
    char file_name[RANDOM_STRING_LEN];
    char overflow_file_name[PATH_MAX - RANDOM_STRING_LEN];
    char subsystem_name[RANDOM_STRING_LEN];
    
    char file_path[PATH_MAX];
    char subsystem_path[PATH_MAX];
    char subsystem_tmp_path[PATH_MAX];
    char subsystem_file_path[PATH_MAX];
    char overflow_file_path[PATH_MAX];
    char overflow_subsystem_file_path[PATH_MAX];
    
    char * args[] = { HELPER_PATH, HELPER_BEHAVIOR_NOT_SET, overflow_file_path, "", NULL};
    
    /* Seed rand() from /dev/random, and generate our random file names. */
    sranddev();
    _generate_random_string(file_name, sizeof(file_name));
    _generate_random_string(overflow_file_name, sizeof(overflow_file_name));
    _generate_random_string(subsystem_name, sizeof(subsystem_name));

    /* Generate pathnames. */
    sprintf(file_path, "/tmp/%s", file_name);
    sprintf(overflow_file_path, "/tmp/%s", overflow_file_name);
    sprintf(subsystem_path, "/tmp/%s", subsystem_name);
    sprintf(subsystem_tmp_path, "%s/tmp", subsystem_path);
    sprintf(subsystem_file_path, "%s/%s", subsystem_path, file_path);
    
    /*
     * Initial setup for the test; we'll need our subsystem
     * directory and a /tmp/ for it.
     */
    T_QUIET; T_ASSERT_POSIX_SUCCESS(mkdir(subsystem_path, 0777), "Create subsystem directory");
    T_QUIET; T_ASSERT_POSIX_SUCCESS(mkdir(subsystem_tmp_path, 0777), "Create subsystem /tmp/ directory");
    T_QUIET; T_ASSERT_POSIX_SUCCESS(posix_spawnattr_init(&attr), "posix_spawnattr_init");

    /* open and stat with no subsystem. */
    args[1] = HELPER_BEHAVIOR_OPEN_AND_WRITE;
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), -1, "open_with_subsystem with no subsystem");

    args[1] = HELPER_BEHAVIOR_STAT_NONE;
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with no subsystem");

    T_QUIET; T_ASSERT_POSIX_SUCCESS(posix_spawnattr_set_subsystem_root_path_np(&attr, subsystem_path), "Set subsystem root path");
    
    /*
     * Test behavior when there is no main file and the subsystem
     * file path is longer than PATH_MAX.
     */
    args[1] = HELPER_BEHAVIOR_OPEN_OVERFLOW;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "open_with_subsystem with overflow");

    args[1] = HELPER_BEHAVIOR_STAT_OVERFLOW;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with overflow");
    
    /* O_CREAT is strictly disallowed; test this. */
    args[1] = HELPER_BEHAVIOR_OPEN_O_CREAT;
    args[2] = file_path;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "open_with_subsystem with O_CREAT");
    
    /*
     * Test valid use of open_with_subsystem and
     * stat_with_subsystem.  We've got 4 cases to
     * test: neither file exists, the subsystem
     * file exists, both files exist, and the
     * main fail exists.
     */
    /* Neither file exists. */
    args[1] = HELPER_BEHAVIOR_OPEN_AND_WRITE;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), -1, "open_with_subsystem with no file");
    
    args[1] = HELPER_BEHAVIOR_STAT_NONE;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with no file");

    /* Subsystem file exists. */
    T_QUIET; T_ASSERT_TRUE(_create_file(subsystem_file_path), "Create subsystem file");
    
    args[1] = HELPER_BEHAVIOR_OPEN_AND_WRITE;
    args[3] = "with subsystem file";
    
    result = _spawn_and_wait(args, &attr);

    if (!result) {
        result = _check_file_contents(subsystem_file_path, args[3], strlen(args[3]));
    }
    
    T_ASSERT_EQ_INT(result, 0, "open_with_subsystem with subsystem file");
    
    args[1] = HELPER_BEHAVIOR_STAT_NOT_MAIN;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with subsystem file");
    
    /* Both files exist. */
    T_QUIET; T_ASSERT_TRUE(_create_file(file_path), "Create main file");
    
    args[1] = HELPER_BEHAVIOR_OPEN_AND_WRITE;
    args[3] = "with both files";
    
    result = _spawn_and_wait(args, &attr);

    if (!result) {
        result = _check_file_contents(file_path, args[3], strlen(args[3]));
    }
    
    T_ASSERT_EQ_INT(result, 0, "open_with_subsystem with both files");
    
    args[1] = HELPER_BEHAVIOR_STAT_MAIN;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with both files");
    
    /* Main file exists. */
    T_QUIET; T_EXPECT_POSIX_SUCCESS(unlink(subsystem_file_path), "Delete subsystem file");
    
    args[1] = HELPER_BEHAVIOR_OPEN_AND_WRITE;
    args[3] = "with main file";
    
    result = _spawn_and_wait(args, &attr);

    if (!result) {
        result = _check_file_contents(file_path, args[3], strlen(args[3]));
    }
    
    T_ASSERT_EQ_INT(result, 0, "open_with_subsystem with main file");
    
    args[1] = HELPER_BEHAVIOR_STAT_MAIN;
    
    T_ASSERT_EQ_INT(_spawn_and_wait(args, &attr), 0, "stat_with_subsystem with main file");

    /* We're done; clean everything up. */
    T_QUIET; T_EXPECT_POSIX_SUCCESS(unlink(file_path), "Delete main file");
    T_QUIET; T_EXPECT_POSIX_SUCCESS(rmdir(subsystem_tmp_path), "Remove subsystem /tmp/ directory");
    T_QUIET; T_EXPECT_POSIX_SUCCESS(rmdir(subsystem_path), "Remove subsystem directory");
    T_QUIET; T_ASSERT_POSIX_SUCCESS(posix_spawnattr_destroy(&attr), "posix_spawnattr_destroy");
}
