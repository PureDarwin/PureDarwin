/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include <sys/param.h>
#include <sys/priv.h>


/*
 * FreeBSD has a more sophisticated privilege checking system.
 * We only check for superuser rights.
 */
int
priv_check(struct thread *thread, int privilegeLevel)
{
	// Note: The thread parameter is ignored intentionally (cf. the comment in
	// pcpu.h). Currently calling this function is only valid for the current
	// thread.
	if (geteuid() == 0)
		return ENOERR;

	return EPERM;
}
