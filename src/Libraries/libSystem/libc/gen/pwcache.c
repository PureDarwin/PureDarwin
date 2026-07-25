/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	NCACHE	64			/* power of 2 */
#define	MASK	(NCACHE - 1)		/* bits to store with */

char *
user_from_uid(uid_t uid, int nouser)
{
	static struct ncache {
		uid_t	uid;
		char	name[MAXLOGNAME];
	} *c_uid[NCACHE];
	static int initialized;
	struct passwd *pw;
	struct ncache *cp;

	if (!initialized) {
		memset(c_uid, 0, sizeof(c_uid));
		initialized = 1;
	}

	cp = c_uid[uid & MASK];
	if (cp == NULL || cp->uid != uid) {
		if ((pw = getpwuid(uid)) == NULL) {
			if (nouser)
				return (NULL);
			cp = malloc(sizeof(*cp));
			if (cp == NULL)
				return (NULL);
			(void)snprintf(cp->name, sizeof(cp->name),
			    "%u", uid);
		} else {
			cp = malloc(sizeof(*cp));
			if (cp == NULL)
				return (NULL);
			(void)strncpy(cp->name, pw->pw_name,
			    sizeof(cp->name) - 1);
			cp->name[sizeof(cp->name) - 1] = '\0';
		}
		cp->uid = uid;
		free(c_uid[uid & MASK]);
		c_uid[uid & MASK] = cp;
	}
	return (cp->name);
}

char *
group_from_gid(gid_t gid, int nogroup)
{
	static struct gcache {
		gid_t	gid;
		char	name[MAXLOGNAME];
	} *c_gid[NCACHE];
	static int initialized;
	struct group *gr;
	struct gcache *cp;

	if (!initialized) {
		memset(c_gid, 0, sizeof(c_gid));
		initialized = 1;
	}

	cp = c_gid[gid & MASK];
	if (cp == NULL || cp->gid != gid) {
		if ((gr = getgrgid(gid)) == NULL) {
			if (nogroup)
				return (NULL);
			cp = malloc(sizeof(*cp));
			if (cp == NULL)
				return (NULL);
			(void)snprintf(cp->name, sizeof(cp->name),
			    "%u", gid);
		} else {
			cp = malloc(sizeof(*cp));
			if (cp == NULL)
				return (NULL);
			(void)strncpy(cp->name, gr->gr_name,
			    sizeof(cp->name) - 1);
			cp->name[sizeof(cp->name) - 1] = '\0';
		}
		cp->gid = gid;
		free(c_gid[gid & MASK]);
		c_gid[gid & MASK] = cp;
	}
	return (cp->name);
}
