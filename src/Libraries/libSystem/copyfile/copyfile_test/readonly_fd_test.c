//
//  Copyright (c) 2020 Apple Inc. All rights reserved.
//

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <unistd.h>
#include "readonly_fd_test.h"
#include "test_utils.h"


static
bool test_readonly_fd_metadata(const char *basedir)
{
	char filename[] = ".readonly-ops-XXXXXX";
	bool created = false;
	bool success = true;
	int dirfd = -1;
	int tmpfd = -1;
	int fd = -1;
	acl_t acl = NULL;

	static const char test_name[] = "readonly_fd_metadata";
	printf("START [%s]\n", test_name);

	assert_with_errno((dirfd = open(basedir, O_RDONLY | O_DIRECTORY)) != -1);
	assert_with_errno((tmpfd = mkstempsat_np(dirfd, filename, 0)) != -1);
	created = true;

	assert_with_errno((fd = openat(dirfd, filename, O_RDONLY)) != -1);
	close(tmpfd);
	tmpfd = -1;

	// confirm that writes are disallowed
	const char data[] = "failure";
	assert(write(fd, data, sizeof(data) - 1) == -1);

	// check fchown()
	const uid_t uid = geteuid();
	const gid_t gid = getegid();
	assert_no_err(fchown(fd, uid, gid));

	// check fchmod()
	assert_no_err(fchmod(fd, 0644));
	assert_no_err(fchmod(fd, 0600));

	// check fchflags()
	assert_no_err(fchflags(fd, UF_HIDDEN));

	// check setting timestamps with fsetattrlist
	const time_t mtime = 978307200;
	const time_t atime = mtime + 1;

	struct timeval matimes_usec[] = {{mtime, 0}, {atime, 0}};
	assert_no_err(futimes(fd, matimes_usec));

	struct attrlist attrlist = {
	    .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .commonattr = ATTR_CMN_MODTIME | ATTR_CMN_ACCTIME,
	};
	struct {
	    struct timespec mtime;
	    struct timespec atime;
	} matimes_nsec = {{mtime, 0}, {atime, 0}};
	assert_no_err(fsetattrlist(fd, &attrlist, &matimes_nsec, sizeof(matimes_nsec), 0));

	// check adding and removing xattrs
	static const char key[] = "local.test-xattr";
	static const char value[] = "local.test-xattr.value";
	assert_no_err(fsetxattr(fd, key, value, sizeof(value)-1, 0, 0));
	assert_no_err(fremovexattr(fd, key, 0));

	// check setting ACLs
	assert_with_errno((acl = acl_init(1)) != NULL);
	assert_no_err(acl_set_fd(fd, acl));

	// log pass/fail before cleanup
	if (success) {
		printf("PASS  [%s]\n", test_name);
	} else {
		printf("FAIL  [%s]\n", test_name);
	}

	// clean up resources
	if (acl) {
		acl_free(acl);
	}
	if (fd != -1) {
	    close(fd);
	}
	if (tmpfd != -1) {
	    close(tmpfd);
	}
	if (created) {
	    unlinkat(dirfd, filename, 0);
	}
	if (dirfd != -1) {
	    close(dirfd);
	}

	return success;
}


bool do_readonly_fd_test(const char *apfs_test_directory, size_t block_size __unused)
{
	// These tests verify the underlying calls needed for COPYFILE_METADATA
	// operations are safe with O_RDONLY file descriptors. If this fails,
	// expect <rdar://60074298> to cause many other copyfile() failures.
	bool success = true;
	success = success && test_readonly_fd_metadata(apfs_test_directory);
	return !success; // caller expects nonzero to mean failure
}
