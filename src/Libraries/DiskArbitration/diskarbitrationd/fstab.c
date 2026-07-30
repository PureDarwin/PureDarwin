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
/*
 * Copyright (c) 1980, 1988, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <errno.h>
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *_fs_fp;
static struct fstab _fs_fstab;
static int LineNo = 0;

static void error(int);
static void fixspace(char *);
static int fstabscan(void);

static void
fixspace(s)
	char *s;
{
	char c, *cp;

	for (cp = s; (c = *s) != '\0'; s++) {
		if (c == '\\') {
			if (*(s+1) == '\\') {
				s += 1;
			} else if (*(s+1) == '0' && *(s+2) == '4' && *(s+3) == '0') {
				c = ' ';
				s += 3;
			}
		}
		*cp++ = c;
	}
	*cp = '\0';
}

static int
fstabscan()
{
	char *cp, *p;
#define	MAXLINELENGTH	1024
	static char line[MAXLINELENGTH];
	char subline[MAXLINELENGTH];
	int typexx;

	for (;;) {

		if (!(p = fgets(line, sizeof(line), _fs_fp)))
			return(0);
		++LineNo;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_spec = cp;
		if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
			continue;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_file = cp;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_vfstype = cp;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_mntops = cp;
		if (_fs_fstab.fs_mntops == NULL)
			goto bad;
		fixspace(_fs_fstab.fs_spec);
		fixspace(_fs_fstab.fs_file);
		_fs_fstab.fs_freq = 0;
		_fs_fstab.fs_passno = 0;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		if (cp != NULL) {
			_fs_fstab.fs_freq = atoi(cp);
			while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
				;
			if (cp != NULL)
				_fs_fstab.fs_passno = atoi(cp);
		}
		_fs_fstab.fs_type = "??";
		strlcpy(subline, _fs_fstab.fs_mntops, sizeof(subline));
		p = subline;
		for (typexx = 0, cp = strsep(&p, ","); cp;
		     cp = strsep(&p, ",")) {
			if (strlen(cp) != 2)
				continue;
			if (!strcmp(cp, FSTAB_RW)) {
				_fs_fstab.fs_type = FSTAB_RW;
				break;
			}
			if (!strcmp(cp, FSTAB_RQ)) {
				_fs_fstab.fs_type = FSTAB_RQ;
				break;
			}
			if (!strcmp(cp, FSTAB_RO)) {
				_fs_fstab.fs_type = FSTAB_RO;
				break;
			}
			if (!strcmp(cp, FSTAB_SW)) {
				_fs_fstab.fs_type = FSTAB_SW;
				break;
			}
			if (!strcmp(cp, FSTAB_XX)) {
				_fs_fstab.fs_type = FSTAB_XX;
				typexx++;
				break;
			}
		}
		if (typexx)
			continue;
		return(1);

bad:		/* no way to distinguish between EOF and syntax error */
		error(EFTYPE);
	}
	/* NOTREACHED */
}

struct fstab *
getfsent()
{
	if ((!_fs_fp && !setfsent()) || !fstabscan())
		return((struct fstab *)NULL);
	return(&_fs_fstab);
}

struct fstab *
getfsspec(name)
	const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_spec, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

struct fstab *
getfsfile(name)
	const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_file, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

int 
setfsent()
{
	if (_fs_fp) {
		rewind(_fs_fp);
		LineNo = 0;
		return(1);
	}
	if ((_fs_fp = fopen(_PATH_FSTAB, "r")) != NULL) {
		LineNo = 0;
		return(1);
	}
	error(errno);
	return(0);
}

void
endfsent()
{
	if (_fs_fp) {
		(void)fclose(_fs_fp);
		_fs_fp = NULL;
	}
}

static void
error(err)
	int err;
{
	char *p;
	char num[30];

	(void)write(STDERR_FILENO, "fstab: ", 7);
	(void)write(STDERR_FILENO, _PATH_FSTAB, sizeof(_PATH_FSTAB) - 1);
	(void)write(STDERR_FILENO, ":", 1);
	snprintf(num, sizeof(num), "%d: ", LineNo);
	(void)write(STDERR_FILENO, num, strlen(num));
	p = strerror(err);
	(void)write(STDERR_FILENO, p, strlen(p));
	(void)write(STDERR_FILENO, "\n", 1);
}
