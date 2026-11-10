// stub.c - a stub kext's start and stop, so the image has code in __TEXT_EXEC
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <mach/mach_types.h>
#include <mach/kmod.h>

kern_return_t pd_inferno_stub_start(kmod_info_t *ki, void *data)
{
	(void)ki; (void)data;
	return KERN_SUCCESS;
}

kern_return_t pd_inferno_stub_stop(kmod_info_t *ki, void *data)
{
	(void)ki; (void)data;
	return KERN_SUCCESS;
}
