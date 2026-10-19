/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUXKPI_LINUX_SYSFS_H_
#define	_LINUXKPI_LINUX_SYSFS_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/errno.h>

#include <linux/kobject.h>
#include <linux/stringify.h>
#include <linux/mm.h>

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct bin_attribute {
	struct attribute	attr;
	size_t			size;
	ssize_t (*read)(struct linux_file *, struct kobject *,
			struct bin_attribute *, char *, loff_t, size_t);
	ssize_t (*write)(struct linux_file *, struct kobject *,
			 struct bin_attribute *, char *, loff_t, size_t);
};

struct attribute_group {
	const char		*name;
	mode_t			(*is_visible)(struct kobject *,
				    struct attribute *, int);
	struct attribute	**attrs;
	struct bin_attribute	**bin_attrs;
};

#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
	.show = _show, .store  = _store,				\
}
#define	__ATTR_RO_MODE(_name, _mode) {					\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
	.show = _name##_show,						\
}
#define	__ATTR_RO(_name)	__ATTR_RO_MODE(_name, 0444)
#define	__ATTR_WO(_name)	__ATTR(_name, 0200, NULL, _name##_store)
#define	__ATTR_RW_MODE(_name, _mode)					\
	__ATTR(_name, _mode, _name##_show, _name##_store)
#define	__ATTR_RW(_name)	__ATTR_RW_MODE(_name, 0644)
#define	__ATTR_NULL	{ .attr = { .name = NULL } }

#define	ATTRIBUTE_GROUPS(_name)						\
	static struct attribute_group _name##_group = {			\
		.name = __stringify(_name),				\
		.attrs = _name##_attrs,					\
	};								\
	static const struct attribute_group *_name##_groups[] = {	\
		&_name##_group,						\
		NULL,							\
	}

#define	__BIN_ATTR(_name, _mode, _read, _write, _size) {		\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
	.read = _read, .write  = _write, .size = _size,			\
}
#define	__BIN_ATTR_RO(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.read = _name##_read, .size = _size,				\
}
#define	__BIN_ATTR_WO(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0200 },		\
	.write = _name##_write, .size = _size,				\
}
#define	__BIN_ATTR_WR(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0644 },		\
	.read = _name##_read, .write = _name##_write, .size = _size,	\
}

#define	BIN_ATTR(_name, _mode, _read, _write, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR(_name, _mode, _read, _write, _size);

#define	BIN_ATTR_RO(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_RO(_name, _size);

#define	BIN_ATTR_WO(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_WO(_name, _size);

#define	BIN_ATTR_WR(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_WR(_name, _size);

// no sysctl tree behind kobjects here, attributes are accepted and not published
static inline int sysfs_create_file(struct kobject *kobj, const struct attribute *attr) { return (0); }
static inline int sysfs_add_file_to_group(struct kobject *kobj, const struct attribute *attr, const char *group) { return (0); }
static inline void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr) { }
static inline void sysfs_remove_file_from_group(struct kobject *kobj, const struct attribute *attr, const char *group) { }
static inline int sysfs_create_bin_file(struct kobject *kobj, const struct bin_attribute *attr) { return (0); }
static inline void sysfs_remove_bin_file(struct kobject *kobj, const struct bin_attribute *attr) { }
static inline int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name) { return (0); }
static inline void sysfs_remove_link(struct kobject *kobj, const char *name) { }
static inline int sysfs_create_files(struct kobject *kobj, const struct attribute * const *attrs) { return (0); }
static inline void sysfs_remove_files(struct kobject *kobj, const struct attribute * const *attrs) { }
static inline int sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp) { return (0); }
static inline void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp) { }
static inline int sysfs_create_groups(struct kobject *kobj, const struct attribute_group **grps) { return (0); }
static inline void sysfs_remove_groups(struct kobject *kobj, const struct attribute_group **grps) { }
static inline int sysfs_merge_group(struct kobject *kobj, const struct attribute_group *grp) { return (0); }
static inline void sysfs_unmerge_group(struct kobject *kobj, const struct attribute_group *grp) { }
static inline int sysfs_create_dir(struct kobject *kobj) { return (0); }
static inline void sysfs_remove_dir(struct kobject *kobj) { }

static inline bool
sysfs_streq(const char *s1, const char *s2)
{
	int l1, l2;

	l1 = strlen(s1);
	l2 = strlen(s2);

	if (l1 != 0 && s1[l1-1] == '\n')
		l1--;
	if (l2 != 0 && s2[l2-1] == '\n')
		l2--;

	return (l1 == l2 && strncmp(s1, s2, l1) == 0);
}

static inline int
sysfs_emit(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	if (!buf || offset_in_page(buf)) {
		pr_warn("invalid sysfs_emit: buf:%p\n", buf);
		return (0);
	}

	va_start(args, fmt);
	i = vscnprintf(buf, PAGE_SIZE, fmt, args);
	va_end(args);

	return (i);
}

static inline int
sysfs_emit_at(char *buf, int at, const char *fmt, ...)
{
	va_list args;
	int i;

	if (!buf || offset_in_page(buf) || at < 0 || at >= PAGE_SIZE) {
		pr_warn("invalid sysfs_emit: buf:%p at:%d\n", buf, at);
		return (0);
	}

	va_start(args, fmt);
	i = vscnprintf(buf + at, PAGE_SIZE - at, fmt, args);
	va_end(args);

	return (i);
}

static inline int
_sysfs_match_string(const char * const *a, size_t l, const char *s)
{
	const char *p;
	int i;

	for (i = 0; i < l; i++) {
		p = a[i];
		if (p == NULL)
			break;
		if (sysfs_streq(p, s))
			return (i);
	}

	return (-ENOENT);
}
#define	sysfs_match_string(a, s)	_sysfs_match_string(a, ARRAY_SIZE(a), s)

#define sysfs_attr_init(attr) do {} while(0)

#endif	/* _LINUXKPI_LINUX_SYSFS_H_ */
