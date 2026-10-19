/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Augustin Cavalier <waddlesplash>
 */


extern "C" {
#	include <sys/kernel.h>
}


int
config_intrhook_establish(struct intr_config_hook *hook)
{
	// By the time we get here, interrupts have already been set up, so
	// unlike FreeBSD we do not need to store the hooks in a queue for later,
	// but we can call them immediately. This is the same behavior that
	// FreeBSD has as of 11.1 (however, they have a comment stating that
	// it should probably not happen this way...)
	(*hook->ich_func)(hook->ich_arg);
	return 0;
}


void
config_intrhook_disestablish(struct intr_config_hook *hook)
{
	// We don't store the hooks, so we don't need to do anything here.
}
