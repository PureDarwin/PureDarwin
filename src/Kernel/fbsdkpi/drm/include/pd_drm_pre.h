// SPDX-License-Identifier: MIT
// pulled in ahead of every drm-kmod file on PureDarwin
#ifndef _PD_DRM_PRE_H_
#define _PD_DRM_PRE_H_

// sysctl knobs are not exported, so these declare nothing
#define SYSCTL_LONG(...)
#define SYSCTL_ULONG(...)
#define SYSCTL_U8(...)
#define SYSCTL_S16(...)
#define SYSCTL_U16(...)
#define SYSCTL_STRING(...)
#define SYSCTL_U64(...)
#define SYSCTL_S64(...)
#define SYSCTL_COUNTER_U64(...)

// process and file-mode names the drm and swap headers test
// the few process fields drm reads, filled per xnu thread by the layer
struct proc {
	int p_pid;
	char p_comm[32];
};
extern struct proc *pageproc;
#ifndef curproc
#define curproc ((struct proc *)0)
#endif
#ifndef FREAD
#define FREAD 0x0001
#define FWRITE 0x0002
#endif
#define UID_ROOT 0
#define GID_WHEEL 0
#define GID_VIDEO 44

// malloc(9) types used without a declaration in scope, defined in pd_drm_glue.c
extern int M_TEMP[1];

#define __read_mostly
#ifndef LOG_NOTICE
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#endif

// a global sx lock set up when the kext starts
#define SX_SYSINIT_FLAGS(name, sxa, desc, flags) \
	static void name##_sx_sysinit(void *arg) { sx_init_flags((sxa), (desc), (flags)); } \
	SYSINIT(name##_sx_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE, name##_sx_sysinit, NULL)

#define SX_SYSINIT(name, sxa, desc) SX_SYSINIT_FLAGS(name, sxa, desc, 0)

// haiku's pcivar has only D0 to D3
#define PCI_POWERSTATE_D3_HOT 3
#define PCI_POWERSTATE_D3_COLD 4

// kthread_run starts threads stopped, amdgpu_ctx checks a scheduling privilege
#ifndef RFSTOPPED
#define RFSTOPPED (1 << 17)
#endif
#define PRIV_SCHED_SETPRIORITY 201
// PureDarwin boots through EFI but exposes no EFI services to drivers
#define efi_boot 0

// no fbdev emulation, PDDRMFramebuffer is the console client
#define CONFIG_DRM_CLIENT_DEFAULT ""

// libkern helpers the drm core calls, from the compiler builtins
#ifndef bitcount64
#define bitcount64(x) ((uint64_t)__builtin_popcountll((unsigned long long)(x)))
#endif
#define rounddown_pow_of_two(n) \
	((__typeof__(n))(1ULL << (63 - __builtin_clzll((unsigned long long)(n)))))
#include <linux/ctype.h>

// loader tunables come from boot-args, with the dots spelled as underscores
long pd_drm_tunable_long(const char *path, long def);
#define TUNABLE_LONG_FETCH(path, var) (*(var) = pd_drm_tunable_long(path, *(var)), 1)

// the /dev/dri aliases wait for devfs cloning, until then this fails
struct cdev;
int make_dev_alias_p(int flags, struct cdev **cdev, struct cdev *old,
    const char *fmt, ...);

// device naming and devd notifications drm_print and drm_sysfs report through
struct _device;
int device_print_prettyname(struct _device *dev);
void devctl_notify(const char *system, const char *subsystem, const char *type,
    const char *data);

// libkern and kernel calls freebsd provides, implemented in pd_drm_glue.c
char *strnstr(const char *s, const char *find, size_t slen);
void shutdown_nice(int howto);
struct vm_page;
void pmap_copy_page(struct vm_page *src, struct vm_page *dst);

// the kernel printf knows nothing of linux's %p extensions (%pV above all, which drm_err
// passes nested messages through), so drm code prints through pd_drm_printf.c
int pd_drm_printf(const char *fmt, ...);
int pd_drm_vprintf(const char *fmt, __builtin_va_list ap);
#define printf(...) pd_drm_printf(__VA_ARGS__)
#define vprintf(...) pd_drm_vprintf(__VA_ARGS__)

#endif
