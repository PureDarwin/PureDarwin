/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/sysctlbyname.c,v 1.5 2002/02/01 00:57:29 obrien Exp $");

#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>
#include <sys/errno.h>
#include <TargetConditionals.h>

#include "sysctl_internal.h"


int
sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
	     size_t newlen)
{
	int error;


	error = __sysctlbyname(name, strlen(name), oldp, oldlenp, newp, newlen);
	if (error < 0) {
		return error;
	}


	return error;
}

