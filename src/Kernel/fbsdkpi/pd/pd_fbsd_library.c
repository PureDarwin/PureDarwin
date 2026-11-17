// SPDX-License-Identifier: MIT
// a library kext's start routine: hands the shared layer its own SYSINIT sets, which only its binary can name
#include <sys/param.h>
#include <sys/kernel.h>

#include "pd_fbsd_driver.h"

// the kmod start and stop signature, without the mach headers this side does not see
struct kmod_info;

SET_DECLARE(__freebsd_sysinit, struct sysinit);
SET_DECLARE(__freebsd_sysuninit, struct sysinit);

int pd_fbsd_library_start(struct kmod_info *ki, void *data);
int pd_fbsd_library_stop(struct kmod_info *ki, void *data);

int
pd_fbsd_library_start(struct kmod_info *ki, void *data)
{
	pd_fbsd_register_library((void **)SET_BEGIN(__freebsd_sysinit), (void **)SET_LIMIT(__freebsd_sysinit));
	return 0;
}

// the library holds state every driver on it shares, it stays
int
pd_fbsd_library_stop(struct kmod_info *ki, void *data)
{
	return 5;
}
