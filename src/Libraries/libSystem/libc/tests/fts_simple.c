#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <fts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <struct.h>
#include <darwintest.h>
#include <darwintest_utils.h>

static void
_create_random_file(int root_fd, char *path)
{
	int fd = openat(root_fd, path, O_WRONLY | O_CREAT);
	T_ASSERT_POSIX_SUCCESS(fd, NULL);
	T_ASSERT_POSIX_SUCCESS(dprintf(fd, "Random File at: %s", path), NULL);
	T_ASSERT_POSIX_SUCCESS(close(fd), NULL);
}

static void
_create_symlink(char *root, char *destination, char *source)
{
	char *absolute_destination = NULL;
	T_ASSERT_POSIX_SUCCESS(asprintf(&absolute_destination, "%s/%s", root, destination), NULL);
	char *absolute_source = NULL;
	T_ASSERT_POSIX_SUCCESS(asprintf(&absolute_source, "%s/%s", root, source), NULL);
	T_ASSERT_POSIX_SUCCESS(symlink(absolute_destination, absolute_source), NULL);
	free(absolute_destination);
	free(absolute_source);
}

static char *
_remove_prefix(char *prefix, char *input) {
	char *start = strstr(input, prefix);
	T_QUIET;
	T_ASSERT_NOTNULL(start, "prefix: %s input: %s", prefix, input);
	char *end = start + strlen(prefix) + 1;
	return end;
}

T_DECL(fts_simple, "Simple fts_read test")
{
	T_LOG("prog: %s", getprogname());
	char *tmp_path = NULL;
	T_ASSERT_POSIX_SUCCESS(asprintf(&tmp_path, "%s/%s-XXXXXX", dt_tmpdir(), T_NAME), NULL);
	T_ASSERT_NOTNULL(mktemp(tmp_path), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdir(tmp_path, 0777), NULL);
	int tmp_fd = open(tmp_path, O_RDONLY | O_DIRECTORY);
	T_LOG("tmp: %s", tmp_path);
	T_ASSERT_POSIX_SUCCESS(tmp_fd, NULL);

	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "A", 0777), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "A/B", 0777), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "A/C", 0777), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "A/B/D", 0777), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "A/C/empty", 0777), NULL);
	_create_random_file(tmp_fd, "root");
	_create_random_file(tmp_fd, "A/fileA1");
	_create_random_file(tmp_fd, "A/fileA2");
	_create_random_file(tmp_fd, "A/B/fileB1");
	_create_random_file(tmp_fd, "A/C/fileC1");
	_create_random_file(tmp_fd, "A/B/D/fileD1");
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "LINK", 0777), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdirat(tmp_fd, "LINK/Z", 0777), NULL);
	_create_random_file(tmp_fd, "LINK/fileL1");
	_create_random_file(tmp_fd, "LINK/Z/fileZ1");
	_create_symlink(tmp_path, "LINK", "A/link");

	char original_cwd[MAXPATHLEN];
	T_ASSERT_NOTNULL(getcwd(original_cwd, sizeof(original_cwd)), NULL);

	struct {
		const char *cwd;
		const char *path;
		int info;
		bool found;
	} expected[] = {
		{
			.cwd = original_cwd,
			.path = "A",
			.info = FTS_D,
		},
		{
			.cwd = "A",
			.path = "A/fileA2",
			.info = FTS_F,
		},
		{
			.cwd = "A",
			.path = "A/B",
			.info = FTS_D,
		},
		{
			.cwd = "A/B",
			.path = "A/B/D",
			.info = FTS_D,
		},
		{
			.cwd = "A/B/D",
			.path = "A/B/D/fileD1",
			.info = FTS_F,
		},
		{
			.cwd = "A/B",
			.path = "A/B/D",
			.info = FTS_DP,
		},
		{
			.cwd = "A/B",
			.path = "A/B/fileB1",
			.info = FTS_F,
		},
		{
			.cwd = "A",
			.path = "A/B",
			.info = FTS_DP,
		},
		{
			.cwd = "A",
			.path = "A/C",
			.info = FTS_D,
		},
		{
			.cwd = "A/C",
			.path = "A/C/empty",
			.info = FTS_D,
		},
		{
			.cwd = "A/C",
			.path = "A/C/empty",
			.info = FTS_DP,
		},
		{
			.cwd = "A/C",
			.path = "A/C/fileC1",
			.info = FTS_F,
		},
		{
			.cwd = "A",
			.path = "A/C",
			.info = FTS_DP,
		},
		{
			.cwd = "A",
			.path = "A/link",
			.info = FTS_SL,
		},
		{
			.cwd = "A",
			.path = "A/link",
			.info = FTS_D,
		},
		{
			.cwd = "LINK",
			.path = "A/link/fileL1",
			.info = FTS_F,
		},
		{
			.cwd = "LINK",
			.path = "A/link/Z",
			.info = FTS_D,
		},
		{
			.cwd = "LINK/Z",
			.path = "A/link/Z/fileZ1",
			.info = FTS_F,
		},
		{
			.cwd = "LINK",
			.path = "A/link/Z",
			.info = FTS_DP,
		},
		{
			.cwd = "A",
			.path = "A/link",
			.info = FTS_DP,
		},
		{
			.cwd = "A",
			.path = "A/fileA1",
			.info = FTS_F,
		},
		{
			.cwd = original_cwd,
			.path = "A",
			.info = FTS_DP,
		},
	};

	const char *LABELS[] = {
		[0] = "None",
		[FTS_D] = "FTS_D",		 		/* preorder directory */
		[FTS_DC] = "FTS_DC",		 		/* directory that causes cycles */
		[FTS_DEFAULT] = "FTS_DEFAULT",	 		/* none of the above */
		[FTS_DNR] = "FTS_DNR",		 		/* unreadable directory */
		[FTS_DOT] = "FTS_DOT",		 		/* dot or dot-dot */
		[FTS_DP] = "FTS_DP",		 		/* postorder directory */
		[FTS_ERR] = "FTS_ERR",		 		/* error; errno is set */
		[FTS_F] = "FTS_F",		 		/* regular file */
		[FTS_INIT] = "FTS_INIT",	 		/* initialized only */
		[FTS_NS] = "FTS_NS",				/* stat(2) failed */
		[FTS_NSOK] = "FTS_NSOK",			/* no stat(2) requested */
		[FTS_SL] = "FTS_SL",				/* symbolic link */
		[FTS_SLNONE] = "FTS_SLNONE",			/* symbolic link without target */
	};

	char *root_path = NULL;
	T_ASSERT_POSIX_SUCCESS(asprintf(&root_path, "%s/A", tmp_path), NULL);
	const char *paths[] = {
		root_path,
		NULL,
	};
	FTS *tree = fts_open(paths, FTS_PHYSICAL, NULL);
	T_ASSERT_NOTNULL(tree, NULL);
	FTSENT *node;
	int found_count = 0;
	while ((node = fts_read(tree))) {
		char cwd[MAXPATHLEN];
		T_QUIET;
		T_ASSERT_NOTNULL(getcwd(cwd, sizeof(cwd)), NULL);

		switch (node->fts_info) {
		case FTS_ERR:
			T_FAIL("FTS_ERR(%d)", node->fts_errno);
			break;
		case FTS_SL:
			fts_set(tree, node, FTS_FOLLOW);
			/* fall through */
		default: {
			bool found = false;
			for (size_t index = 0; index < countof(expected) && !found; index++) {
				if (expected[index].found) {
					continue;
				}
				if (expected[index].info != node->fts_info) {
					// Wrong type, skip
					continue;
				}

				char *expected_path = expected[index].path;
				char *actual_path = node->fts_path;
				if (expected_path[0] != '/') {
					actual_path = _remove_prefix(tmp_path, actual_path);
				}

				if (strcmp(actual_path, expected_path) == 0) {
					char *expected_cwd = expected[index].cwd;
					char *actual_cwd = cwd;
					if (expected_cwd[0] != '/') {
						// Relative path
						actual_cwd = _remove_prefix(tmp_path, actual_cwd);
					}
					T_QUIET;
					T_EXPECT_EQ_STR(actual_cwd, expected_cwd, NULL);
					found = true;
					expected[index].found = true;
					found_count++;
				}
			}
			T_EXPECT_TRUE(found, "path: %s info: %d [%s] cwd: %s", node->fts_path, node->fts_info, LABELS[node->fts_info], cwd);
		}
		}
	}
	T_QUIET;
	T_EXPECT_EQ(found_count, countof(expected), NULL);
	for (size_t index = 0; index < countof(expected); index++) {
		T_QUIET;
		T_EXPECT_TRUE(expected[index].found, "missing: path: %s info %d [%s]", expected[index].path, expected[index].info, LABELS[expected[index].info]);
	}
	fts_close(tree);
	free(tmp_path);
	free(root_path);
}
