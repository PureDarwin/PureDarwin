// SPDX-License-Identifier: MIT
// FreeBSD's sys/conf.h, enough for linux/cdev.h to compile
#pragma once
#include <sys/types.h>

// character devices are never created here, these let linux/cdev.h compile
struct cdevsw;
struct ucred;
struct cdev {
	void *si_drv1;
	void *si_drv2;
	const char *si_name;
};

struct make_dev_args {
	size_t mda_size;
	int mda_flags;
	struct cdevsw *mda_devsw;
	struct ucred *mda_cr;
	uid_t mda_uid;
	gid_t mda_gid;
	int mda_mode;
	int mda_unit;
	void *mda_si_drv1;
	void *mda_si_drv2;
};

#define MAKEDEV_WAITOK 0x08
#define MAKEDEV_CHECKNAME 0x10

static inline void make_dev_args_init(struct make_dev_args *a) { __builtin_memset(a, 0, sizeof(*a)); a->mda_size = sizeof(*a); }
static inline int make_dev_s(struct make_dev_args *a, struct cdev **cdev, const char *fmt, ...) { return 19; }
static inline void destroy_dev(struct cdev *dev) { }
