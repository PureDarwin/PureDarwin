/*-
 * Copyright (c) 1990, 1993, 1994
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)keyword.c	8.5 (Berkeley) 4/2/94";
#else
static const char rcsid[] =
	"$FreeBSD: keyword.c,v 1.23 1999/01/26 02:38:09 julian Exp $";
#endif /* not lint */
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ps.h"

VAR *findvar(char *, int, char **header);
static int  vcmp(const void *, const void *);

/* Compute offset in common structures. */
#define	POFF(x)	offsetof(struct extern_proc, x)
#define	EOFF(x)	offsetof(struct eproc, x)
#define	UOFF(x)	offsetof(struct usave, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	EMULLEN	13		/* enough for "FreeBSD ELF32" */
#define	LWPFMT	"d"
#define	LWPLEN	6
#define	NLWPFMT	"d"
#define	NLWPLEN	4
#ifdef __APPLE__
#define	UIDFMT	"d"
#else
#define	UIDFMT	"u"
#endif
#define	UIDLEN	5
#define	PIDFMT	"d"
#define	PIDLEN	5
#define USERLEN 16

/* PLEASE KEEP THE TABLE BELOW SORTED ALPHABETICALLY!!! */
static VAR var[] = {
	/* 4133537: 5 characters to accomodate 100% or more */
	{"%cpu", "%CPU", NULL, 0, pcpu, NULL, 5, 0, CHAR, NULL, 0},
	{"%mem", "%MEM", NULL, 0, pmem, NULL, 4},
	{"acflag", "ACFLG",
		NULL, 0, pvar, NULL, 3, POFF(p_acflag), USHORT, "x"},
	{"acflg", "", "acflag"},
	{"args", "ARGS", NULL, COMM|LJUST|USER|DSIZ, args, s_args, 64},
	{"blocked", "", "sigmask"},
	{"caught", "", "sigcatch"},
	{"comm", "COMM", NULL, COMM|LJUST|USER|DSIZ, just_command, s_just_command, 16},
	{"command", "COMMAND", NULL, COMM|LJUST|USER|DSIZ, command, s_command, 16},
	{"cpu", "CPU", NULL, 0, pvar, NULL, 3, POFF(p_estcpu), UINT, "d"},
	{"cputime", "", "time"},
	{"etime", "ELAPSED", NULL, USER|DSIZ, p_etime, s_etime, 20},
	/* 5861775: Make F column 8 characters. */
	{"f", "F", NULL, 0, pvar, NULL, 8, POFF(p_flag), INT, "x"},
	{"flags", "", "f"},
	{"gid", "GID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_ucred.cr_gid),
		UINT, UIDFMT},
	{"group", "GROUP", "gid"},
	{"ignored", "", "sigignore"},
	{"inblk", "INBLK",
		NULL, USER, rvar, NULL, 4, ROFF(ru_inblock), LONG, "ld"},
	{"inblock", "", "inblk"},
	{"jobc", "JOBC", NULL, 0, evar, NULL, 4, EOFF(e_jobc), SHORT, "d"},
	{"ktrace", "KTRACE",
		NULL, 0, pvar, NULL, 8, POFF(p_traceflag), INT, "x"},
	{"ktracep", "KTRACEP",
		NULL, 0, pvar, NULL, 8, POFF(p_tracep), LONG, "lx"},
	{"lim", "LIM", NULL, 0, maxrss, NULL, 5},
	{"login", "LOGIN", NULL, LJUST, logname, NULL, MAXLOGNAME-1},
	{"logname", "", "login"},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, NULL, 28},
	{"majflt", "MAJFLT",
		NULL, USER, rvar, NULL, 4, ROFF(ru_majflt), LONG, "ld"},
	{"minflt", "MINFLT",
		NULL, USER, rvar, NULL, 4, ROFF(ru_minflt), LONG, "ld"},
	{"msgrcv", "MSGRCV",
		NULL, USER, rvar, NULL, 4, ROFF(ru_msgrcv), LONG, "ld"},
	{"msgsnd", "MSGSND",
		NULL, USER, rvar, NULL, 4, ROFF(ru_msgsnd), LONG, "ld"},
	{"ni", "", "nice"},
	{"nice", "NI", NULL, 0, pvar, NULL, 2, POFF(p_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW",
		NULL, USER, rvar, NULL, 5, ROFF(ru_nivcsw), LONG, "ld"},
	{"nsignals", "", "nsigs"},
	{"nsigs", "NSIGS",
		NULL, USER, rvar, NULL, 4, ROFF(ru_nsignals), LONG, "ld"},
	{"nswap", "NSWAP",
		NULL, USER, rvar, NULL, 4, ROFF(ru_nswap), LONG, "ld"},
	{"nvcsw", "NVCSW",
		NULL, USER, rvar, NULL, 5, ROFF(ru_nvcsw), LONG, "ld"},
	{"nwchan", "WCHAN", NULL, 0, pvar, NULL, 6, POFF(p_wchan), KPTR, "lx"},
	{"oublk", "OUBLK",
		NULL, USER, rvar, NULL, 4, ROFF(ru_oublock), LONG, "ld"},
	{"oublock", "", "oublk"},
	{"p_ru", "P_RU", NULL, 0, pvar, NULL, 6, POFF(p_ru), KPTR, "lx"},
	{"paddr", "PADDR", NULL, 0, evar, NULL, sizeof(void *) * 2, EOFF(e_paddr), KPTR, "lx"},
	{"pagein", "PAGEIN", NULL, USER, pagein, NULL, 6},
	{"pcpu", "", "%cpu"},
	{"pending", "", "sig"},
	{"pgid", "PGID",
		NULL, 0, evar, NULL, PIDLEN, EOFF(e_pgid), UINT, PIDFMT},
	{"pid", "PID", NULL, 0, pvar, NULL, PIDLEN, POFF(p_pid), UINT, PIDFMT},
	{"pmem", "", "%mem"},
	{"ppid", "PPID",
		NULL, 0, evar, NULL, PIDLEN, EOFF(e_ppid), UINT, PIDFMT},
	{"pri", "PRI", NULL, 0, pri, NULL, 3},
	{"pstime", "", "stime"},
	{"putime", "", "utime"},
	{"re", "RE", NULL, 0, pvar, NULL, 3, POFF(p_swtime), UINT, "d"},
	{"rgid", "RGID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_pcred.p_rgid),
		UINT, UIDFMT},
	{"rgroup", "RGROUP", "rgid"},
	{"rss", "RSS", NULL, 0, p_rssize, NULL, 6},
#if FIXME
	{"rtprio", "RTPRIO", NULL, 0, rtprior, NULL, 7, POFF(p_rtprio)},
#endif /* FIXME */
	{"ruid", "RUID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_pcred.p_ruid),
		UINT, UIDFMT},
	{"ruser", "RUSER", NULL, LJUST|DSIZ, runame, s_runame, USERLEN},
	{"sess", "SESS", NULL, 0, evar, NULL, 6, EOFF(e_sess), KPTR, "lx"},
	{"sig", "PENDING", NULL, 0, pvar, NULL, 8, POFF(p_siglist), INT, "x"},
#if FIXME
	{"sigcatch", "CAUGHT",
		NULL, 0, evar, NULL, 8, EOFF(e_procsig.ps_sigcatch), UINT, "x"},
	{"sigignore", "IGNORED",
		NULL, 0, evar, NULL, 8, EOFF(e_procsig.ps_sigignore), UINT, "x"},
#endif /* FIXME */
	{"sigmask", "BLOCKED",
		NULL, 0, pvar, NULL, 8, POFF(p_sigmask), UINT, "x"},
	{"sl", "SL", NULL, 0, pvar, NULL, 3, POFF(p_slptime), UINT, "d"},
	{"start", "STARTED", NULL, LJUST|USER, started, NULL, 7},
	{"stat", "", "state"},
	{"state", "STAT", NULL, 0, state, NULL, 4},
	{"stime", "STIME", NULL, USER, pstime, NULL, 9},
	{"svgid", "SVGID", NULL, 0,
		evar, NULL, UIDLEN, EOFF(e_pcred.p_svgid), UINT, UIDFMT},
	{"svuid", "SVUID", NULL, 0,
		evar, NULL, UIDLEN, EOFF(e_pcred.p_svuid), UINT, UIDFMT},
	{"tdev", "TDEV", NULL, 0, tdev, NULL, 4},
	{"time", "TIME", NULL, USER, cputime, NULL, 9},
	{"tpgid", "TPGID",
		NULL, 0, evar, NULL, 4, EOFF(e_tpgid), UINT, PIDFMT},
	{"tsess", "TSESS", NULL, 0, evar, NULL, 6, EOFF(e_tsess), KPTR, "lx"},
	{"tsiz", "TSIZ", NULL, 0, tsize, NULL, 8},
	{"tt", "TT ", NULL, 0, tname, NULL, 5},
	{"tty", "TTY", NULL, LJUST, longtname, NULL, 8},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, NULL, MAXCOMLEN},
	{"uid", "UID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_ucred.cr_uid),
		UINT, UIDFMT},
	{"upr", "UPR", NULL, 0, pvar, NULL, 3, POFF(p_usrpri), CHAR, "d"},
	{"user", "USER", NULL, LJUST|DSIZ, uname, s_uname, USERLEN},
	{"usrpri", "", "upr"},
	{"utime", "UTIME", NULL, USER, putime, NULL, 9},
	{"vsize", "", "vsz"},
	{"vsz", "VSZ", NULL, 0, vsize, NULL, 8},
	{"wchan", "WCHAN", NULL, LJUST, wchan, NULL, 6},
	{"wq", "WQ", NULL, 0, wq, NULL, 4, 0, CHAR, NULL, 0},
	{"wqb", "WQB", NULL, 0, wq, NULL, 4, 0, CHAR, NULL, 0},
	{"wql", "WQL", NULL, 0, wq, NULL, 3, 0, CHAR, NULL, 0},
	{"wqr", "WQR", NULL, 0, wq, NULL, 4, 0, CHAR, NULL, 0},
	{"xstat", "XSTAT", NULL, 0, pvar, NULL, 4, POFF(p_xstat), USHORT, "x"},
	{""},
};

void
showkey(void)
{
	VAR *v;
	int i;
	const char *p, *sep;

	i = 0;
	sep = "";
	for (v = var; *(p = v->name); ++v) {
		int len = strlen(p);
		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		(void) printf("%s%s", sep, p);
		sep = " ";
	}
	(void) printf("\n");
}

void
parsefmt(const char *p, int user)
{
	char *tempstr, *tempstr1;

#define		FMTSEP	" \t,\n"
	tempstr1 = tempstr = strdup(p);
	while (tempstr && *tempstr) {
		char *cp, *hp;
		VAR *v;
		struct varent *vent;

#ifndef __APPLE__
		/*
		 * If an item contains an equals sign, it specifies a column
		 * header, may contain embedded separator characters and
		 * is always the last item.	
		 */
		if (tempstr[strcspn(tempstr, "="FMTSEP)] != '=')
#endif /* !__APPLE__ */
			while ((cp = strsep(&tempstr, FMTSEP)) != NULL &&
			    *cp == '\0')
				/* void */;
#ifndef __APPLE__
		else {
			cp = tempstr;
			tempstr = NULL;
		}
#endif /* !__APPLE__ */
		if (cp == NULL || !(v = findvar(cp, user, &hp)))
			continue;
		if (!user) {
			/*
			 * If the user is NOT adding this field manually,
			 * get on with our lives if this VAR is already
			 * represented in the list.
			 */
			vent = find_varentry(v);
			if (vent != NULL)
				continue;
		}
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			errx(1, "malloc failed");
		vent->header = v->header;
		if (hp) {
			hp = strdup(hp);
			if (hp)
				vent->header = hp;
		}
		vent->var = malloc(sizeof(*vent->var));
		if (vent->var == NULL)
			errx(1, "malloc failed");
		memcpy(vent->var, v, sizeof(*vent->var));
		STAILQ_INSERT_TAIL(&varlist, vent, next_ve);
	}
	free(tempstr1);
	if (STAILQ_EMPTY(&varlist)) {
		warnx("no valid keywords; valid keywords:");
		showkey();
		exit(1);
	}
}

VAR *
findvar(char *p, int user, char **header)
{
	size_t rflen;
	VAR *v, key;
	char *hp, *realfmt;

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = bsearch(&key, var, sizeof(var)/sizeof(VAR) - 1, sizeof(VAR), vcmp);

	if (v && v->alias) {
		/*
		 * If the user specified an alternate-header for this
		 * (aliased) format-name, then we need to copy that
		 * alternate-header when making the recursive call to
		 * process the alias.
		 */
		if (hp == NULL)
			parsefmt(v->alias, user);
		else {
			/*
			 * XXX - This processing will not be correct for
			 * any alias which expands into a list of format
			 * keywords.  Presently there are no aliases
			 * which do that.
			 */
			rflen = strlen(v->alias) + strlen(hp) + 2;
			realfmt = malloc(rflen);
			snprintf(realfmt, rflen, "%s=%s", v->alias, hp);
			parsefmt(realfmt, user);
		}
		return ((VAR *)NULL);
	}
	if (!v) {
		warnx("%s: keyword not found", p);
		eval = 1;
	}
	if (header)
		*header = hp;
	return (v);
}

static int
vcmp(const void *a, const void *b)
{
        return (strcmp(((const VAR *)a)->name, ((const VAR *)b)->name));
}
