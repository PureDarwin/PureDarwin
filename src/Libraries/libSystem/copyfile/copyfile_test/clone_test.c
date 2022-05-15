//
//  clone_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/xattr.h>

#include "../copyfile.h"
#include "../copyfile_private.h"
#include "../xattr_properties.h"
#include "clone_test.h"
#include "test_utils.h"

#define XATTR_1_NAME	"com.apple.quarantine"
#define XATTR_1_DATA	"dib"
#define XATTR_2_NAME	"zim"
#define XATTR_2_DATA	"gir"

static bool verify_test_xattrs(const char *dest_name) {
	// Verify that `xattr_2` exists on the destination and has the correct content.
	bool success = verify_path_xattr_content(dest_name, XATTR_2_NAME, XATTR_2_DATA,
		sizeof(XATTR_2_DATA));

	// Verify that `xattr_1` does not exist on the destination.
	return success && verify_path_missing_xattr(dest_name, XATTR_1_NAME);
}

static int do_copy(const char *source_name, const char *dest_name, uint32_t flags) {
	CopyOperationIntent_t intent = XATTR_OPERATION_INTENT_SHARE;
	copyfile_state_t cpf_state;
	int error;

	assert_with_errno((cpf_state = copyfile_state_alloc()) != NULL);
	assert_no_err(copyfile_state_set(cpf_state, COPYFILE_STATE_INTENT, &intent));
	error = copyfile(source_name, dest_name, cpf_state, flags);

	assert_no_err(copyfile_state_free(cpf_state));
	return error;
}

bool do_clone_copy_intent_test(const char *apfs_test_directory, __unused size_t block_size) {
	char source_name[BSIZE_B], dest_name[BSIZE_B];
	int source_fd, test_file_id;
	bool success = true;

	// (1) Create a file with two xattrs, `xattr_1` and `xattr_2`.
	// (2) Test that copying the file with a copyIntent that rejects
	// only `xattr_1` succeeds without COPYFILE_CLONE.
	// (3) Test that copying the file with the same copyIntent AND
	// COPYFILE_CLONE also works as expected.
	// (4) Re-do (3) but with COPYFILE_CLONE_FORCE, and verify
	// that we fail in this case.
	printf("START [clone_copy_intent]\n");

	// Get ready for the test.
	test_file_id = rand() % DEFAULT_NAME_MOD;

	create_test_file_name(apfs_test_directory, "clone_xattrs_src", test_file_id, source_name);
	create_test_file_name(apfs_test_directory, "clone_xattrs_dst", test_file_id, dest_name);

	// (1) Create the test file with the two xattrs.
	source_fd = open(source_name, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(source_fd >= 0);

	// Write two xattrs.
	assert_no_err(fsetxattr(source_fd, XATTR_1_NAME, XATTR_1_DATA, sizeof(XATTR_1_DATA),
		0, XATTR_CREATE));
	assert_no_err(fsetxattr(source_fd, XATTR_2_NAME, XATTR_2_DATA, sizeof(XATTR_2_DATA),
		0, XATTR_CREATE));

	// (2) Test coping the file with the copyIntent (but not COPYFILE_CLONE).
	assert_no_err(do_copy(source_name, dest_name, COPYFILE_ALL));

	// Verify that the xattrs are correct on the destination.
	success = success && verify_test_xattrs(dest_name);

	// (3) Repeat the test with COPYFILE_CLONE set.
	assert_no_err(removefile(dest_name, NULL, 0));
	assert_no_err(do_copy(source_name, dest_name, COPYFILE_ALL|COPYFILE_CLONE));

	success = success && verify_test_xattrs(dest_name);

	// (4) Repeat the test with COPYFILE_CLONE|COPYFILE_CLONE_FORCE set,
	// and verify that it fails.
	assert_no_err(removefile(dest_name, NULL, 0));
	assert_equal_int(do_copy(source_name, dest_name,
		COPYFILE_ALL|COPYFILE_CLONE|COPYFILE_CLONE_FORCE), -1);
	assert_equal_int(errno, ENOTSUP);

	if (success) {
		printf("PASS  [clone_copy_intent]\n");
	} else {
		printf("FAIL  [clone_copy_intent]\n");
	}

	// Post-test cleanup.
	assert_no_err(close(source_fd));
	(void)removefile(source_name, NULL, 0);
	(void)removefile(dest_name, NULL, 0);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
