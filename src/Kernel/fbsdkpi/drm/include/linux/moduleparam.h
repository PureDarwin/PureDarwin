// SPDX-License-Identifier: MIT
// module parameters are plain globals on PureDarwin, set from boot-args later
#ifndef _PD_LINUX_MODULEPARAM_H_
#define _PD_LINUX_MODULEPARAM_H_

#include <linux/types.h>

#define module_param_named(name, var, type, mode)
#define module_param(var, type, mode)
#define module_param_named_unsafe(name, var, type, mode)
#define module_param_unsafe(var, type, mode)
#define module_param_array(var, type, addr_argc, mode)
#define module_param_array_named(name, var, type, addr_argc, mode)
#define module_param_string(name, string, len, perm)
#define module_param_cb(name, ops, arg, perm)
#define MODULE_PARM_DESC(name, desc)

struct kernel_param;
struct kernel_param_ops {
	int (*set)(const char *val, const struct kernel_param *kp);
	int (*get)(char *buffer, const struct kernel_param *kp);
	void (*free)(void *arg);
};
struct kernel_param {
	const char *name;
	const struct kernel_param_ops *ops;
	void *arg;
};

static inline int param_set_int(const char *val, const struct kernel_param *kp) { return 0; }
static inline int param_get_int(char *buffer, const struct kernel_param *kp) { return 0; }

#endif
