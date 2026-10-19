// SPDX-License-Identifier: MIT
// FreeBSD's sys/file.h, the struct file parts linuxkpi reads
#pragma once
#include_next <sys/file.h>
#include <sys/types.h>
#include <sys/refcount.h>

// the parts of FreeBSD's struct file that linuxkpi reads
struct fileops;
struct file {
	void *f_data;
	struct fileops *f_ops;
	struct vnode *f_vnode;
	volatile unsigned int f_count;
	short f_type;
	unsigned int f_flag;
	long long f_offset;
};
#define DTYPE_DEV 7
