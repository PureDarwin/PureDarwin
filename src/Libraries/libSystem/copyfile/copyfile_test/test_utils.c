//
//  test_utils.c
//  copyfile_test
//

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <removefile.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "test_utils.h"
#include "systemx.h"

bool verify_path_missing_xattr(const char *path, const char *xattr_name) {
	ssize_t size_or_error = getxattr(path, xattr_name, NULL, 0, 0, XATTR_SHOWCOMPRESSION);
	if (size_or_error != -1) {
		printf("xattr %s: unexpectedly present on %s\n", xattr_name, path);
		return false;
	} else if (errno != ENOATTR) {
		printf("xattr %s: getxattr(2) got unexpected error %d\n", xattr_name, errno);
		return false;
	}

	return true;
}

static bool verify_xattr_content(const char *xattr_name, const char *expected, char *actual,
								 size_t size) {
	bool equal = (memcmp(actual, expected, size) == 0);
	if (!equal) {
		printf("xattr %s: content does not match expected\n", xattr_name);

		// Find the first non-matching byte and print it out.
		for (size_t bad_off = 0; bad_off < size; bad_off++) {
			if (expected[bad_off] != actual[bad_off]) {
				printf("first mismatch is at offset %zu, original 0x%llx expected 0x%llx\n",
					   bad_off, (unsigned long long)expected[bad_off],
					   (unsigned long long)actual[bad_off]);
				break;
			}
		}
	}

	return equal;
}

bool verify_path_xattr_content(const char *path, const char *xattr_name, const char *expected,
							   size_t size) {
	// Verify that the file referenced by the path `path` has an xattr named `xattr_name`
	// of size `size` and with contents equal to `expected`.
	char *actual = NULL;
	bool equal;

	assert_with_errno(actual = malloc(size));
	assert_with_errno(getxattr(path, xattr_name, actual, size, 0, XATTR_SHOWCOMPRESSION) == (ssize_t)size);
	equal = verify_xattr_content(xattr_name, expected, actual, size);

	free(actual);
	return equal;
}

static bool verify_fd_xattr_content(int fd, const char *xattr_name, const char *expected,
									size_t size) {
	// Verify that the file referenced by `fd` has an xattr named `xattr_name`
	// of size `size` and with contents equal to `expected`.
	char *actual = NULL;
	bool equal;

	assert(fd > 0 && xattr_name && expected);
	assert_with_errno(actual = malloc(size));
	assert_with_errno(fgetxattr(fd, xattr_name, actual, size, 0, XATTR_SHOWCOMPRESSION) == (ssize_t)size);
	equal = verify_xattr_content(xattr_name, expected, actual, size);

	free(actual);
	return equal;
}

bool verify_fd_xattr_contents(int orig_fd, int copy_fd) {
	// Verify that both fd's have the same xattrs.
	// We do so by first verifying that `flistxattr()` returns the same size
	// for both, and then validating that `copy_fd` has each of the xattrs
	// that `orig_fd` has.
	char *namebuf = NULL, *xa_buf = NULL, *name, *end;
	ssize_t orig_size, copy_size, xa_size;
	bool equal = true;

	assert((orig_fd > 0) && (copy_fd > 0));

	orig_size = flistxattr(orig_fd, 0, 0, XATTR_SHOWCOMPRESSION);
	copy_size = flistxattr(copy_fd, 0, 0, XATTR_SHOWCOMPRESSION);
	if (orig_size != copy_size) {
		printf("xattrlist size: orig_size(%zu) != (%zu)copy_size\n", orig_size, copy_size);
		return false;
	}

	if (orig_size == 0) {
		return true;
	}

	assert_with_errno(namebuf = malloc(orig_size));

	assert_with_errno(flistxattr(orig_fd, namebuf, orig_size, XATTR_SHOWCOMPRESSION) == orig_size);

	end = namebuf + orig_size - 1;
	if (*end != 0) {
		*end = 0;
	}

	for (name = namebuf; name <= end; name += strlen(name) + 1) {
		xa_size = fgetxattr(orig_fd, name, 0, 0, 0, XATTR_SHOWCOMPRESSION);
		assert(xa_size >= 0);
		size_t xa_size_u = (size_t) xa_size;

		assert_with_errno(xa_buf = malloc(xa_size_u));
		assert_with_errno(fgetxattr(orig_fd, name, xa_buf, xa_size_u, 0, XATTR_SHOWCOMPRESSION) == xa_size);
		equal = equal && verify_fd_xattr_content(copy_fd, name, xa_buf, xa_size_u);
		free(xa_buf);

		if (!equal) {
			break;
		}
	}
	free(namebuf);

	return equal;
}

bool verify_st_flags(struct stat *sb, uint32_t flags_to_expect) {
	// Verify that sb's flags include flags_to_expect.
	if (((sb->st_flags & flags_to_expect)) != flags_to_expect) {
		printf("st_flags (%u) do not include expected flags (%u)\n",
			sb->st_flags, flags_to_expect);
		return false;
	}

	return true;
}

bool verify_contents_with_buf(int orig_fd, off_t orig_pos, const char *expected, size_t length)
{
	// Read *length* bytes from a file descriptor at a specified position
	// and assert that they match *length* bytes from expected.
	char orig_contents[length];
	bool equal;

	assert(orig_fd > 0 && orig_pos >= 0);
	memset(orig_contents, 0, length);

	errno = 0;
	ssize_t pread_res = pread(orig_fd, orig_contents, length, orig_pos);
	assert_with_errno(pread_res == (off_t) length);
	equal = (memcmp(orig_contents, expected, length) == 0);
	if (!equal) {
		printf("fd (%lld - %lld) did not match expected contents\n", orig_pos, orig_pos + length);

		// Find the first non-matching byte and print it out.
		for (size_t bad_off = 0; bad_off < length; bad_off++) {
			if (orig_contents[bad_off] != expected[bad_off]) {
				printf("first mismatch is at offset %zu, original 0x%llx expected 0x%llx\n",
					   bad_off, (unsigned long long)orig_contents[bad_off],
					   (unsigned long long)expected[bad_off]);
				break;
			}
		}
	}

	return equal;
}

bool verify_fd_contents(int orig_fd, off_t orig_pos, int copy_fd, off_t copy_pos, size_t length) {
	// Read *length* contents of the two fds and make sure they compare as equal.
	// Don't alter the position of either fd.
	char orig_contents[length], copy_contents[length];
	bool equal;

	assert(orig_fd > 0 && copy_fd > 0);
	assert(orig_pos >= 0);
	memset(orig_contents, 0, length);
	memset(copy_contents, 0, length);

	// Read the contents into our temporary buffers, and call memcmp().
	errno = 0;
	ssize_t pread_res = pread(orig_fd, orig_contents, length, 0);
	assert_with_errno(pread_res == (off_t) length);
	assert_with_errno(pread(copy_fd, copy_contents, length, copy_pos) >= 0);
	equal = (memcmp(orig_contents, copy_contents, length) == 0);
	if (!equal) {
		printf("original fd (%lld - %lld) did not match copy (%lld - %lld)\n",
			   orig_pos, orig_pos + length, copy_pos, copy_pos + length);

		// Find the first non-matching byte and print it out.
		for (size_t bad_off = 0; bad_off < length; bad_off++) {
			if (orig_contents[bad_off] != copy_contents[bad_off]) {
				printf("first mismatch is at offset %zu, original 0x%llx COPY 0x%llx\n",
					   bad_off, (unsigned long long)orig_contents[bad_off],
					   (unsigned long long)copy_contents[bad_off]);
				break;
			}
		}
	}

	return equal;
}

bool verify_copy_contents(const char *orig_name, const char *copy_name) {
	// Verify that the copy and the source have identical contents.
	// Here, we just call out to 'diff' to do the work for us.
	int rc = systemx(DIFF_PATH,	orig_name, copy_name, SYSTEMX_QUIET, SYSTEMX_QUIET_STDERR, NULL);
	if (rc != 0) {
		printf("%s and %s are not identical: diff returned %d\n", orig_name, copy_name, rc);
		exit(1);
	}

	return !rc;
}

bool verify_copy_sizes(struct stat *orig_sb, struct stat *copy_sb, copyfile_state_t cpf_state,
					   bool do_sparse, off_t src_start) {
	off_t cpf_bytes_copied, blocks_offset;
	bool result = true;

	// If requested, verify that the copy is a sparse file.
	if (do_sparse) {
		if (orig_sb->st_size - src_start != copy_sb->st_size) {
			printf("original size - offset (%lld) != copy size (%lld)\n",
				   orig_sb->st_size - src_start, copy_sb->st_size);
			result = false;
		}

		blocks_offset = src_start / S_BLKSIZE;
		if (orig_sb->st_blocks - blocks_offset < copy_sb->st_blocks) {
			printf("original blocks - offset (%lld) < copy blocks (%lld)\n",
				   orig_sb->st_blocks - blocks_offset, copy_sb->st_blocks);
			result = false;
		}
	}

	// Verify that the copyfile_state_t for the copy returns that all bytes were copied.
	if (cpf_state) {
		assert_no_err(copyfile_state_get(cpf_state, COPYFILE_STATE_COPIED, &cpf_bytes_copied));
		if (orig_sb->st_size - src_start != cpf_bytes_copied) {
			printf("original size - start (%lld) != copied bytes (%lld)\n",
				   orig_sb->st_size - src_start, cpf_bytes_copied);
			result = false;
		}
	}

	return result;
}

int create_hole_in_fd(int fd, off_t offset, off_t length) {
	struct fpunchhole punchhole_args = {
		.fp_flags = 0,
		.reserved = 0,
		.fp_offset = offset,
		.fp_length = length
	};

	return fcntl(fd, F_PUNCHHOLE, &punchhole_args);
}

void write_compressible_data(int fd) {
	// adapted from test_clonefile
	char dbuf[4096];

	// write some easily compressible data
	memset(dbuf + 0*(sizeof(dbuf)/4), 'A', sizeof(dbuf)/4);
	memset(dbuf + 1*(sizeof(dbuf)/4), 'B', sizeof(dbuf)/4);
	memset(dbuf + 2*(sizeof(dbuf)/4), 'C', sizeof(dbuf)/4);
	memset(dbuf + 3*(sizeof(dbuf)/4), 'D', sizeof(dbuf)/4);
	for (int idx = 0; idx < 32; idx++) {
		check_io(write(fd, dbuf, sizeof(dbuf)), sizeof(dbuf));
	}
}

void compress_file(const char *path, const char *type) {
	assert_no_err(systemx(AFSCUTIL_PATH, SYSTEMX_QUIET, "-c", "-t", type, path, NULL));
}

void create_test_file_name(const char *dir, const char *postfix, int id, char *string_out) {
	// Make a name for this new file and put it in string_out, which should be BSIZE_B bytes.
	assert_with_errno(snprintf(string_out, BSIZE_B, "%s/testfile-%d.%s", dir, id, postfix) > 0);
}

void disk_image_create(const char *fstype, size_t size_in_mb) {
	char size[BSIZE_B];

	// Set up good default values.
	if (!fstype) {
		fstype = DEFAULT_FSTYPE;
	}
	if (size_in_mb > MAX_DISK_IMAGE_SIZE_MB) {
		size_in_mb = MAX_DISK_IMAGE_SIZE_MB;
	}
	assert_with_errno(snprintf(size, BSIZE_B, "%zum", size_in_mb) >= 3);

	// Unmount and remove the sparseimage if it already exists.
	disk_image_destroy(true);
	if (removefile(DISK_IMAGE_PATH, NULL, REMOVEFILE_RECURSIVE) < 0) {
		assert_with_errno(errno == ENOENT);
	}

	// Make the disk image.
	assert_no_err(systemx(HDIUTIL_PATH, SYSTEMX_QUIET, "create", "-fs", fstype,
						  "-size", size, "-type", "SPARSE", "-volname", "apfs_sparse",
						  DISK_IMAGE_PATH, NULL));

	// Attach the disk image.
	assert_no_err(systemx(HDIUTIL_PATH, SYSTEMX_QUIET, "attach", DISK_IMAGE_PATH, NULL));
}

void disk_image_destroy(bool allow_failure) {
	// If the caller allows, ignore any failures (also silence stderr).
	if (allow_failure) {
		systemx(HDIUTIL_PATH, "eject", MOUNT_PATH, SYSTEMX_QUIET, SYSTEMX_QUIET_STDERR, NULL);
	} else {
		assert_no_err(systemx(HDIUTIL_PATH, "eject", MOUNT_PATH, SYSTEMX_QUIET, NULL));
	}
}
