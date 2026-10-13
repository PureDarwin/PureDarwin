/*
 * mount_ext4: mount an ext4 volume with PureDarwin's ext4.kext on macOS
 *
 *     mount_ext4 [-o options] special node
 *
 * ext4.kext registers as a local volume, so mount(2) takes the device path
 * as its argument and opens the device itself.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// the mount data a local volume file system reads, the device path first
struct ext4_args {
	char *fspec;
};

static const struct {
	const char *name;
	int flag;
} options[] = {
	{ "ro", MNT_RDONLY }, { "rdonly", MNT_RDONLY }, { "nosuid", MNT_NOSUID },
	{ "nodev", MNT_NODEV }, { "noexec", MNT_NOEXEC }, { "nobrowse", MNT_DONTBROWSE },
	{ "noowners", MNT_IGNORE_OWNERSHIP }, { "sync", MNT_SYNCHRONOUS },
	{ "async", MNT_ASYNC }, { "quarantine", MNT_QUARANTINE }, { "noatime", MNT_NOATIME },
};

static void
usage(void)
{
	fprintf(stderr, "usage: mount_ext4 [-o options] special node\n");
	exit(1);
}

// a comma list from -o, unknown names are ignored as mount(8) passes some of its own
static void
parse(char *list, int *flags)
{
	char *opt;

	while ((opt = strsep(&list, ",")) != NULL) {
		int clear = strncmp(opt, "no", 2) == 0 && strcmp(opt, "nosuid") && strcmp(opt, "nodev") &&
		    strcmp(opt, "noexec") && strcmp(opt, "nobrowse") && strcmp(opt, "noowners") && strcmp(opt, "noatime");

		if (strcmp(opt, "rw") == 0) {
			*flags &= ~MNT_RDONLY;
			continue;
		}
		for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
			if (strcmp(opt + (clear ? 2 : 0), options[i].name) == 0) {
				if (clear)
					*flags &= ~options[i].flag;
				else
					*flags |= options[i].flag;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	struct ext4_args args;
	char dev[MAXPATHLEN], dir[MAXPATHLEN];
	int flags = 0, ch;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		if (ch == 'o')
			parse(optarg, &flags);
		else
			usage();
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	// DiskArbitration passes a bare disk name such as disk4s1
	if (argv[0][0] != '/')
		snprintf(dev, sizeof(dev), "/dev/%s", argv[0]);
	else
		strlcpy(dev, argv[0], sizeof(dev));
	if (realpath(argv[1], dir) == NULL)
		err(1, "%s", argv[1]);

	args.fspec = dev;
	if (mount("ext4", dir, flags, &args) < 0)
		err(1, "%s on %s", dev, dir);
	return 0;
}
