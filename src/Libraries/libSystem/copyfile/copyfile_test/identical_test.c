//
//  identical_test.c
//  copyfile_test
//

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "../copyfile.h"
#include "identical_test.h"
#include "test_utils.h"

#define REGULAR_FILE_NAME	"regular_file"
#define REGULAR_DIR_NAME 	"regular_dir"
#define DUMMY_XATTR_NAME 	"dummy_xattr"
#define DUMMY_XATTR_DATA 	"drell"
#define TEST_FILE_DATA   	"krogan"

static bool verify_src_dst_identical(const char *apfs_test_directory, __unused size_t block_size) {
	char regular_file[BSIZE_B] = {0}, folder[BSIZE_B] = {0}, file_inside_folder[BSIZE_B] = {0};
	int regular_file_fd, file_inside_folder_fd;
	bool success = true;

	// The idea here is to verify that copyfile(file1, file1) returns success
	// without doing anything.
	// There are a few wrinkles - COPYFILE_CHECK still needs to work on these files,
	// and we need to make sure that our identity check works on filesystems without
	// persistent object identifiers. The first we can easily verify but the second
	// is not tested here. Nor are negative tests included (there are an infinite
	// number of those, so we rely on the other tests to verify that behavior).

	// Create path names.
	assert_with_errno(snprintf(regular_file, BSIZE_B, "%s/" REGULAR_FILE_NAME, apfs_test_directory) > 0);
	assert_with_errno(snprintf(folder, BSIZE_B, "%s/" REGULAR_DIR_NAME, apfs_test_directory) > 0);
	assert_with_errno(snprintf(file_inside_folder, BSIZE_B, "%s/" REGULAR_FILE_NAME, folder) > 0);

	// First, verify copyfile(file1, file1),
	// where file1 is a regular file.

	// Create our regular file.
	regular_file_fd = open(regular_file, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(regular_file_fd >= 0);

	// Write some data to the test file so that we can verify it is not empty.
	assert(write(regular_file_fd, TEST_FILE_DATA, sizeof(TEST_FILE_DATA)) > 0);

	// Verify copyfile(file1, file1) does nothing.
	assert_no_err(copyfile(regular_file, regular_file, NULL, COPYFILE_ALL));
	success = success && verify_contents_with_buf(regular_file_fd, 0, (const char *)TEST_FILE_DATA, sizeof(TEST_FILE_DATA));

	// Verify copyfile(file1, file1, COPYFILE_EXCL) returns an error.
	assert(copyfile(regular_file, regular_file, NULL, COPYFILE_ALL|COPYFILE_EXCL) == -1);
	assert(errno == EEXIST);

	// Write an dummy xattr to the file to verify COPYFILE_CHECK.
	assert_no_err(fsetxattr(regular_file_fd, DUMMY_XATTR_NAME, DUMMY_XATTR_DATA, sizeof(DUMMY_XATTR_DATA), 0, XATTR_CREATE));

	// Verify copyfile(file1, file1, ..., COPYFILE_CHECK) works.
	assert_no_err(copyfile(regular_file, regular_file, NULL, COPYFILE_CHECK) == COPYFILE_XATTR);

	// Now, verify that copyfile(dir1, dir1, COPYFILE_RECURSIVE)
	// also returns early. Do this by making sure the contents of a file inside the directory
	// do not change after copyfile(COPYFILE_RECURSIVE).

	// Create our directory.
	assert_no_err(mkdir(folder, DEFAULT_MKDIR_PERM));

	// Create a regular file inside that directory.

	file_inside_folder_fd = open(file_inside_folder, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(file_inside_folder_fd >= 0);

	// Write some data to the interior file so that we can verify it is not empty.
	assert(write(file_inside_folder_fd, (const char *)TEST_FILE_DATA, sizeof(TEST_FILE_DATA)) > 0);

	// Verify copyfile(dir1, dir1, ... COPYFILE_RECURSIVE).
	assert_no_err(copyfile(folder, folder, NULL, COPYFILE_RECURSIVE));
	success = success && verify_contents_with_buf(file_inside_folder_fd, 0, TEST_FILE_DATA, sizeof(TEST_FILE_DATA));

	// Post-test cleanup.
	assert_no_err(close(file_inside_folder_fd));
	assert_no_err(close(regular_file_fd));
	(void)removefile(folder, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(regular_file, NULL, 0);

	return success;
}

bool do_src_dst_identical_test(const char *apfs_test_directory, __unused size_t block_size) {
	char test_dir[BSIZE_B] = {0};
	int test_folder_id;
	bool success = true;

	printf("START [identical]\n");

	// Get ready for the test.
	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, "identical", test_folder_id, test_dir);
	assert_no_err(mkdir(test_dir, DEFAULT_MKDIR_PERM));

	success = verify_src_dst_identical(test_dir, block_size);

	if (success) {
		printf("PASS  [identical]\n");
	} else {
		printf("FAIL  [identical]\n");
	}

	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}