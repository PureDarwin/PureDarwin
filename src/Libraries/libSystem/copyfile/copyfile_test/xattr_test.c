//
//  xattr_test.c
//  copyfile_test
//

#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "xattr_test.h"
#include "test_utils.h"

#define SRC_FILE_NAME		"src_file"
#define DST_FILE_NAME		"dst_file"
#define SMALL_XATTR_NAME	"small_xattr"
#define SMALL_XATTR_DATA	"drell"
#define BIG_XATTR_NAME		"big_xattr"
#define BIG_XATTR_SIZE		(20 * 1024 * 1024) // 20MiB

#define DEFAULT_CHAR_MOD	256

static bool copy_and_verify_xattr_contents(const char *src_file, const char *dst_file, int src_file_fd, int dst_file_fd) {
	assert_no_err(copyfile(src_file, dst_file, NULL, COPYFILE_XATTR));

	return verify_fd_xattr_contents(src_file_fd, dst_file_fd);
}

bool do_xattr_test(const char *apfs_test_directory, __unused size_t block_size) {
	char test_dir[BSIZE_B] = {0};
	char src_file[BSIZE_B] = {0}, dst_file[BSIZE_B] = {0};
	char *big_xattr_data = NULL, buf[4096] = {0};
	int test_folder_id;
	int src_file_fd, dst_file_fd;
	bool success = true;

	printf("START [xattr]\n");

	// Get ready for the test.
	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, "xattr", test_folder_id, test_dir);
	assert_no_err(mkdir(test_dir, DEFAULT_MKDIR_PERM));

	// Create path names.
	assert_with_errno(snprintf(src_file, BSIZE_B, "%s/" SRC_FILE_NAME, test_dir) > 0);
	assert_with_errno(snprintf(dst_file, BSIZE_B, "%s/" DST_FILE_NAME, test_dir) > 0);

	// Create our files.
	src_file_fd = open(src_file, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_file_fd >= 0);
	dst_file_fd = open(dst_file, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(dst_file_fd >= 0);

	// Sanity check - empty copy
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	// Write a small xattr to the source file.
	assert_no_err(fsetxattr(src_file_fd, SMALL_XATTR_NAME, SMALL_XATTR_DATA, sizeof(SMALL_XATTR_DATA), 0, XATTR_CREATE));
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	// Create big xattr data
	assert_with_errno(big_xattr_data = malloc(BIG_XATTR_SIZE));
	for (int i = 0; i * sizeof(buf) < BIG_XATTR_SIZE; i++) {
		memset(buf, rand() % DEFAULT_CHAR_MOD, sizeof(buf));
		memcpy(big_xattr_data + (i * sizeof(buf)), buf, sizeof(buf));
	}

	// Write a big xattr to the source file.
	assert_no_err(fsetxattr(src_file_fd, BIG_XATTR_NAME, big_xattr_data, BIG_XATTR_SIZE, 0, XATTR_CREATE));
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	if (success) {
		printf("PASS  [xattr]\n");
	} else {
		printf("FAIL  [xattr]\n");
	}

	free(big_xattr_data);
	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

