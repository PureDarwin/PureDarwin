// SPDX-License-Identifier: MIT
// linuxkpi entry points whose FreeBSD sources need firmware tables, the fpu
// or user memory, none of which a wifi driver reaches here

#include <sys/param.h>
#include <sys/domainset.h>

#include <linux/compat.h>
#include <linux/dmi.h>
#include <linux/sched.h>

#include "../../pd_fbsd_os.h"
#include <linux/uaccess.h>

struct domainset domainset_prefer[MAXMEMDOM];
struct domainset domainset_fixed[MAXMEMDOM];
struct domainset domainset_roundrobin;

int
linux_dmi_check_system(const struct dmi_system_id *sysid)
{
	return (0);
}

bool
linux_dmi_match(enum dmi_field f, const char *str)
{
	return (false);
}

const struct dmi_system_id *
linux_dmi_first_match(const struct dmi_system_id *sysid)
{
	return (NULL);
}

const char *
linux_dmi_get_system_info(int field)
{
	return (NULL);
}

// kernel code here never touches the fpu, so the callback runs as is
void
lkpi_fpu_safe_exec(fpu_safe_exec_cb_t func, void *ctx)
{
	func(ctx);
}

// ioctl and read arguments arrive already copied into the kernel, the linux
// code is handed a small fake user address inside this window instead
#define LINUX_IOCTL_MIN_PTR	0x10000UL
#define LINUX_IOCTL_MAX_PTR	(LINUX_IOCTL_MIN_PTR + 0x100000UL)

static int
linux_remap_address(void **uaddr, size_t len)
{
	uintptr_t value = (uintptr_t)*uaddr;
	struct task_struct *task;

	if (value < LINUX_IOCTL_MIN_PTR || value >= LINUX_IOCTL_MAX_PTR)
		return 0;
	task = current;
	value -= LINUX_IOCTL_MIN_PTR;
	if (task == NULL || task->bsd_ioctl_data == NULL || value + len > task->bsd_ioctl_len) {
		*uaddr = NULL;
		return 1;
	}
	*uaddr = (char *)task->bsd_ioctl_data + value;
	return 1;
}

int
linux_copyin(const void *uaddr, void *kaddr, size_t len)
{
	if (linux_remap_address(__DECONST(void **, &uaddr), len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(kaddr, uaddr, len);
		return (0);
	}
	return (-pdos_copyin(uaddr, kaddr, len));
}

int
linux_copyout(const void *kaddr, void *uaddr, size_t len)
{
	if (linux_remap_address(&uaddr, len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(uaddr, kaddr, len);
		return (0);
	}
	return (-pdos_copyout(kaddr, uaddr, len));
}

// xnu does not export its panic string. Without a kext-local one the kernel
// collection binds it to the panic trampoline and KERNEL_PANICKED() is always true.
const char *panicstr;
