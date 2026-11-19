// SPDX-License-Identifier: MIT
// FreeBSD's sys/vnode.h, the fields linuxkpi touches
#pragma once
#include <sys/types.h>

// linux inodes are vnodes, only the fields linuxkpi touches exist here
struct cdev;
struct vnode {
	void *v_data;
	struct cdev *v_rdev;
	int v_type;
	// Linux's inode->i_mapping, set by the driver that makes the vnode
	struct address_space *i_mapping;
};
#define VCHR 4
static inline int vget(struct vnode *vp, int flags) { return 0; }
static inline void vrele(struct vnode *vp) { }

struct selinfo {
	void *si_pd;
};
