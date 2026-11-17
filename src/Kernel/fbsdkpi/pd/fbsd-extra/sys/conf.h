// SPDX-License-Identifier: MIT
// FreeBSD's sys/conf.h, enough for linux/cdev.h and a driver's cdevsw to compile
#pragma once
#include <sys/types.h>

// character devices are never created here, these let linux/cdev.h and driver cdevsws compile
struct cdevsw;
struct ucred;
struct thread;
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

#define D_VERSION 0x17122009
#ifndef UID_ROOT
#define UID_ROOT 0
#endif
#ifndef GID_WHEEL
#define GID_WHEEL 0
#endif

typedef int d_open_t(struct cdev *dev, int oflags, int devtype, struct thread *td);
typedef int d_close_t(struct cdev *dev, int fflag, int devtype, struct thread *td);
typedef int d_ioctl_t(struct cdev *dev, unsigned long cmd, char *data, int fflag, struct thread *td);

struct cdevsw {
	int d_version;
	unsigned int d_flags;
	const char *d_name;
	d_open_t *d_open;
	d_close_t *d_close;
	d_ioctl_t *d_ioctl;
};

// a driver's debug device, the caller treats NULL as no device
static inline struct cdev *make_dev(struct cdevsw *sw, int unit, uid_t uid, gid_t gid, int perms, const char *fmt, ...) { return (struct cdev *)0; }
