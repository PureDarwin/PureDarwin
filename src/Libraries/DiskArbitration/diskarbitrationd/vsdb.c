/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "vsdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *_vs_fp;
static struct vsdb _vs_vsdb;

static int
vsdbscan()
{
	char *cp, *p;
#define	MAXLINELENGTH	1024
	static char line[MAXLINELENGTH];

	for (;;) {

		if (!(p = fgets(line, sizeof(line), _vs_fp)))
			return(0);
		if (!(cp = strsep(&p, ":")) || *cp == '\0')
			continue;
		_vs_vsdb.vs_spec = cp;
		if (!(cp = strsep(&p, "\n")) || *cp == '\0')
			continue;
		_vs_vsdb.vs_ops = strtol(cp, &p, 16);
		if (*p == '\0')
			return(1);
	}
	/* NOTREACHED */
}

struct vsdb *
getvsent()
{
	if ((!_vs_fp && !setvsent()) || !vsdbscan())
		return((struct vsdb *)NULL);
	return(&_vs_vsdb);
}

struct vsdb *
getvsspec(name)
	const char *name;
{
	if (setvsent())
		while (vsdbscan())
			if (!strcmp(_vs_vsdb.vs_spec, name))
				return(&_vs_vsdb);
	return((struct vsdb *)NULL);
}

int 
setvsent()
{
	if (_vs_fp) {
		rewind(_vs_fp);
		return(1);
	}
	if ((_vs_fp = fopen(_PATH_VSDB, "r")) != NULL) {
		return(1);
	}
	return(0);
}

void
endvsent()
{
	if (_vs_fp) {
		(void)fclose(_vs_fp);
		_vs_fp = NULL;
	}
}
