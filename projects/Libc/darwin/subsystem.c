/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <subsystem.h>
#include <sys/errno.h>
#include <sys/syslimits.h>
#include <_simple.h>

#define SUBSYSTEM_ROOT_PATH_KEY "subsystem_root_path"

void _subsystem_init(const char *apple[]);

static char * subsystem_root_path = NULL;
static size_t subsystem_root_path_len = 0;

/*
 * Takes the apple array, and initializes subsystem
 * support in Libc.
 */
void
_subsystem_init(const char **apple)
{
	char * subsystem_root_path_string = _simple_getenv(apple, SUBSYSTEM_ROOT_PATH_KEY);
	if (subsystem_root_path_string) {
		subsystem_root_path = subsystem_root_path_string;
		subsystem_root_path_len = strnlen(subsystem_root_path, PATH_MAX);
	}
}

/*
 * Takes a buffer, a subsystem path, and a file path, and constructs the
 * subsystem path for the given file path.  Assumes that the subsystem root
 * path will be "/" terminated.
 */
static bool
construct_subsystem_path(char * buf, size_t buf_size, const char * subsystem_root_path, const char * file_path)
{
	size_t return_a = strlcpy(buf, subsystem_root_path, buf_size);
	size_t return_b = strlcat(buf, file_path, buf_size);

	if ((return_a >= buf_size) || (return_b >= buf_size)) {
		return false;
	}

	return true;
}

int
open_with_subsystem(const char * path, int oflag)
{
	/* Don't support file creation. */
	if (oflag & O_CREAT){
		errno = EINVAL;
		return -1;
	}

	int result;

	result = open(path, oflag);

	if ((result < 0) && (errno == ENOENT) && (subsystem_root_path)) {
		/*
		 * If the file doesn't exist relative to root, search
		 * for it relative to the subsystem root.
		 */
		char subsystem_path[PATH_MAX];

		if (construct_subsystem_path(subsystem_path, sizeof(subsystem_path), subsystem_root_path, path)) {
			result = open(subsystem_path, oflag);
		} else {
			errno = ENAMETOOLONG;
		}
	}

	return result;
}

int
stat_with_subsystem(const char *restrict path, struct stat *restrict buf)
{
	int result;

	result = stat(path, buf);

	if ((result < 0) && (errno == ENOENT) && (subsystem_root_path)) {
		/*
		 * If the file doesn't exist relative to root, search
		 * for it relative to the subsystem root.
		 */
		char subsystem_path[PATH_MAX];

		if (construct_subsystem_path(subsystem_path, sizeof(subsystem_path), subsystem_root_path, path)) {
			result = stat(subsystem_path, buf);
		} else {
			errno = ENAMETOOLONG;
		}
	}

	return result;
}

