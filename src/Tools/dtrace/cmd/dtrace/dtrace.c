/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <dtrace.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <alloca.h>
#include <libgen.h>
#include <libproc.h>

#include <mach/mach.h>
#include <mach/machine.h>
#include <sys/sysctl.h>
#include <pthread.h>

#include <IOKit/IOKitLib.h>

#include <System/sys/csr.h>


typedef struct dtrace_cmd {
	void (*dc_func)(struct dtrace_cmd *);	/* function to compile arg */
	dtrace_probespec_t dc_spec;		/* probe specifier context */
	char *dc_arg;				/* argument from main argv */
	const char *dc_name;			/* name for error messages */
	const char *dc_desc;			/* desc for error messages */
	dtrace_prog_t *dc_prog;			/* program compiled from arg */
	char dc_ofile[PATH_MAX];		/* derived output file name */
} dtrace_cmd_t;

#define	DMODE_VERS	0	/* display version information and exit (-V) */
#define	DMODE_EXEC	1	/* compile program for enabling (-a/e/E) */
#define	DMODE_ANON	2	/* compile program for anonymous tracing (-A) */
#define	DMODE_LINK	3	/* compile program for linking with ELF (-G) */
#define	DMODE_LIST	4	/* compile program and list probes (-l) */
#define	DMODE_HEADER	5	/* compile program for headergen (-h) */

#define	E_SUCCESS	0
#define	E_ERROR		1
#define	E_USAGE		2

// XXX TODO: BX
static const char DTRACE_OPTSTR[] =
	":3:6:a:Ab:c:CD:ef:FhHi:I:lL:m:n:o:p:P:qs:SU:vVwW:x:Z";

char *ctf_type_name(ctf_file_t *fp, ctf_id_t type, char *buf, size_t len);

static char **g_argv;
static int g_argc;
static char **g_objv;
static int g_objc;
static dtrace_cmd_t *g_cmdv;
static int g_cmdc;
static struct ps_prochandle **g_psv;
static int g_psc;
static int g_pslive;
static char *g_pname;
static int g_quiet;
static int g_flowindent;
static int g_intr;
static int g_impatient;
static int g_newline;
static int g_total;
static int g_cflags;
static int g_oflags;
static int g_verbose;
static int g_exec = 1;
static int g_mode = DMODE_EXEC;
static int g_status = E_SUCCESS;
static int g_grabanon = 0;
static int g_proc_created_grabbed = 0;
static int g_wait_proc = 0;
static const char *g_ofile = NULL;
static const char *g_script_name = NULL;
static FILE *g_ofp = NULL;
static dtrace_hdl_t *g_dtp;

static io_registry_entry_t registry_entry = 0;

static int
usage(FILE *fp)
{
	static const char predact[] = "[[ predicate ] action ]";

	(void) fprintf(fp, "Usage: %s [-aACeFHlqSvVwZ] "
	    "[-arch i386|x86_64] "
	    "[-b bufsz] [-c cmd] [-D name[=def]]\n\t[-I path] [-L path] "
	    "[-o output] [-p pid] [-s script] [-U name]\n\t"
	    "[-x opt[=val]]\n\n"
	    "\t[-P provider %s]\n"
	    "\t[-m [ provider: ] module %s]\n"
	    "\t[-f [[ provider: ] module: ] func %s]\n"
	    "\t[-n [[[ provider: ] module: ] func: ] name %s]\n"
	    "\t[-i probe-id %s] [ args ... ]\n\n", g_pname,
	    predact, predact, predact, predact, predact);

	(void) fprintf(fp, "\tpredicate -> '/' D-expression '/'\n");
	(void) fprintf(fp, "\t   action -> '{' D-statements '}'\n");

	(void) fprintf(fp, "\n"
	    "\t-arch Generate programs and Mach-O files for the specified architecture\n\n"
	    "\t-a  claim anonymous tracing state\n"
	    "\t-A  generate plist(5) entries for anonymous tracing\n"
	    "\t-b  set trace buffer size\n"
	    "\t-c  run specified command and exit upon its completion\n"
	    "\t-C  run cpp(1) preprocessor on script files\n"
	    "\t-D  define symbol when invoking preprocessor\n"
	    "\t-e  exit after compiling request but prior to enabling probes\n"
	    "\t-f  enable or list probes matching the specified function name\n"
	    "\t-F  coalesce trace output by function\n"
	    "\t-h  generate a header file with definitions for static probes\n"
	    "\t-H  print included files when invoking preprocessor\n"
	    "\t-i  enable or list probes matching the specified probe id\n"
	    "\t-I  add include directory to preprocessor search path\n"
	    "\t-l  list probes matching specified criteria\n"
	    "\t-L  add library directory to library search path\n"
	    "\t-m  enable or list probes matching the specified module name\n"
	    "\t-n  enable or list probes matching the specified probe name\n"
	    "\t-o  set output file\n"
	    "\t-p  grab specified process-ID and cache its symbol tables\n"
	    "\t-P  enable or list probes matching the specified provider name\n"
	    "\t-q  set quiet mode (only output explicitly traced data)\n"
	    "\t-s  enable or list probes according to the specified D script\n"
	    "\t-S  print D compiler intermediate code\n"
	    "\t-U  undefine symbol when invoking preprocessor\n"
	    "\t-v  set verbose mode (report stability attributes, arguments)\n"
	    "\t-V  report DTrace API version\n"
	    "\t-w  permit destructive actions\n"
	    "\t-W  wait for specified process and exit upon its completion\n"
	    "\t-x  enable or modify compiler and tracing options\n"
	    "\t-Z  permit probe descriptions that match zero probes\n");

	return (E_USAGE);
}

static void
verror(const char *fmt, va_list ap)
{
	int error = errno;

	(void) fprintf(stderr, "%s: ", g_pname);
	(void) vfprintf(stderr, fmt, ap);

	if (fmt[strlen(fmt) - 1] != '\n')
		(void) fprintf(stderr, ": %s\n", strerror(error));
}

/*PRINTFLIKE1*/
static void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);

	exit(E_ERROR);
}

/*PRINTFLIKE1*/
static void
dfatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	(void) fprintf(stderr, "%s: ", g_pname);
	if (fmt != NULL)
		(void) vfprintf(stderr, fmt, ap);

	va_end(ap);

	if (fmt != NULL && fmt[strlen(fmt) - 1] != '\n') {
		(void) fprintf(stderr, ": %s\n",
		    dtrace_errmsg(g_dtp, dtrace_errno(g_dtp)));
	} else if (fmt == NULL) {
		(void) fprintf(stderr, "%s\n",
		    dtrace_errmsg(g_dtp, dtrace_errno(g_dtp)));
	}

	if (g_dtp) {
		int i;
		for (i = 0; i < g_psc; i++) {
			dtrace_proc_continue(g_dtp, g_psv[i]);
			dtrace_proc_release(g_dtp, g_psv[i]);
		}
	}

	/*
	 * Close the DTrace handle to ensure that any controlled processes are
	 * correctly restored and continued.
	 */
	dtrace_close(g_dtp);
	
	exit(E_ERROR);
}

/*PRINTFLIKE1*/
static void
error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
}

/*PRINTFLIKE1*/
static void
notice(const char *fmt, ...)
{
	va_list ap;

	if (g_quiet)
		return; /* -q or quiet pragma suppresses notice()s */

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
}

/*PRINTFLIKE1*/
static void
oprintf(const char *fmt, ...)
{
	va_list ap;
	int n;

	if (g_ofp == NULL)
		return;

	va_start(ap, fmt);
	n = vfprintf(g_ofp, fmt, ap);
	va_end(ap);

	if (n < 0) {
		if (errno != EINTR) {
			fatal("failed to write to %s",
			    g_ofile ? g_ofile : "<stdout>");
		}
		clearerr(g_ofp);
	}
}

/*
 * Accommodate embedded escaped whitespace in args.
 */
static char **
make_argv(char *s)
{
     /* const char *ws = "\f\n\r\t\v "; */
    char **argv = malloc(sizeof (char *) * (strlen(s) / 2 + 1));
    int argc = 0;
    char *p = s;
    char *endp = s + strlen(s);
    int i;
    int j;
    const char esc_char = '\\';
    char *current_token;

    if (argv == NULL)
            return (NULL);

    /* Skip over any white space at the beginning of s.  */

    while (p[0] == '\f' || p[0] == '\n'
           || p[0] == '\r' || p[0] == '\t'
           || p[0] == '\v' || p[0] == ' ')
      p++;

    /* Skip over any white space at the end of s.  */

    while (endp[0] == '\f' || endp[0] == '\n'
           || endp[0] == '\r' || endp[0] == '\t'
           || endp[0] == '\v' || endp[0] == ' '
           || endp[0] == '\0')
      endp--;

    /* Now go through p breaking it up into tokens.  */

	/* (endp - p + 1) is number of characters preceding NUL */
    current_token = (char *) malloc ((endp - p + 1) + 1);
    i = 0;  /* Index into p */
    j = 0;  /* Index into current_token  */
    while (i <= endp - p)
      {
        /* Look for escape character.  If found, skip over
           it and copy the character following it into
           current_token.  */

        if (p[i] == esc_char)
          {
            i++;
            current_token[j++] = p[i++];
          }

        /* Otherwise, if any white space character is
           found, we're at the end of the current token.  */

        else if (p[i] == '\f' || p[i] == '\n'
                 || p[i] == '\r' || p[i] == '\t'
                 || p[i] == '\v' || p[i] == ' ')
          {
            current_token[j] = '\0';      /* Terminate current token.        */
            argv[argc++] = current_token; /* Assign cur token to argv list.  */
            p += i + 1;               /* Advance p to start of next token.   */
            i = 0;                    /* Re-set i, j, & current_token        */
            j = 0;
            current_token = (char *) malloc ((endp - p + 1) + 1);
          }

        /* If we've reached the end of the input string, we've
           also reached the end of the current token.  */

        else if (endp - p == i)
          {
            current_token[j++] = p[i++];  /* Copy last char to current token */
            current_token[j]   = '\0';    /* Terminate current token.        */
            argv[argc++] = current_token; /* Assign cur token to argv list.  */
          }

        /* Otherwise, we're in the middle of a token; keep copying the
           characters one at a time.  */

        else
          current_token[j++] = p[i++];
      }

    if (argc == 0)
            argv[argc++] = s;

	argv[argc] = NULL;
	return (argv);
}

static void
print_probe_info(const dtrace_probeinfo_t *p)
{
	char buf[BUFSIZ];
	int i;

	oprintf("\n\tProbe Description Attributes\n");

	oprintf("\t\tIdentifier Names: %s\n",
	    dtrace_stability_name(p->dtp_attr.dtat_name));
	oprintf("\t\tData Semantics:   %s\n",
	    dtrace_stability_name(p->dtp_attr.dtat_data));
	oprintf("\t\tDependency Class: %s\n",
	    dtrace_class_name(p->dtp_attr.dtat_class));

	oprintf("\n\tArgument Attributes\n");

	oprintf("\t\tIdentifier Names: %s\n",
	    dtrace_stability_name(p->dtp_arga.dtat_name));
	oprintf("\t\tData Semantics:   %s\n",
	    dtrace_stability_name(p->dtp_arga.dtat_data));
	oprintf("\t\tDependency Class: %s\n",
	    dtrace_class_name(p->dtp_arga.dtat_class));

	oprintf("\n\tArgument Types\n");

	for (i = 0; i < p->dtp_argc; i++) {
		if (ctf_type_name(p->dtp_argv[i].dtt_ctfp,
		    p->dtp_argv[i].dtt_type, buf, sizeof (buf)) == NULL)
			(void) strlcpy(buf, "(unknown)", sizeof (buf));
		oprintf("\t\targs[%d]: %s\n", i, buf);
	}

	if (p->dtp_argc == 0)
		oprintf("\t\tNone\n");

	oprintf("\n");
}

/*ARGSUSED*/
static int
info_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *stp, dtrace_ecbdesc_t **last)
{
#pragma unused(pgp)
	dtrace_ecbdesc_t *edp = stp->dtsd_ecbdesc;
	dtrace_probedesc_t *pdp = &edp->dted_probe;
	dtrace_probeinfo_t p;

	if (edp == *last)
		return (0);

	oprintf("\n%s:%s:%s:%s\n",
	    pdp->dtpd_provider, pdp->dtpd_mod, pdp->dtpd_func, pdp->dtpd_name);

	if (dtrace_probe_info(dtp, pdp, &p) == 0)
		print_probe_info(&p);

	*last = edp;
	return (0);
}

/*
 * Execute the specified program by enabling the corresponding instrumentation.
 * If -e has been specified, we get the program info but do not enable it.  If
 * -v has been specified, we print a stability report for the program.
 */
static void
exec_prog(const dtrace_cmd_t *dcp)
{
	dtrace_ecbdesc_t *last = NULL;
	dtrace_proginfo_t dpi;

	if (!g_exec) {
		dtrace_program_info(g_dtp, dcp->dc_prog, &dpi);
	} else if (dtrace_program_exec(g_dtp, dcp->dc_prog, &dpi) == -1) {
		dfatal("failed to enable '%s'", dcp->dc_name);
	} else {
		if (!(dpi.dpi_matches == 0 && (g_wait_proc && !g_proc_created_grabbed)))
			notice("%s '%s' matched %u probe%s\n",
			    dcp->dc_desc, dcp->dc_name,
			    dpi.dpi_matches, dpi.dpi_matches == 1 ? "" : "s");
	}

	if (g_verbose) {
		oprintf("\nStability attributes for %s %s:\n",
		    dcp->dc_desc, dcp->dc_name);

		oprintf("\n\tMinimum Probe Description Attributes\n");
		oprintf("\t\tIdentifier Names: %s\n",
		    dtrace_stability_name(dpi.dpi_descattr.dtat_name));
		oprintf("\t\tData Semantics:   %s\n",
		    dtrace_stability_name(dpi.dpi_descattr.dtat_data));
		oprintf("\t\tDependency Class: %s\n",
		    dtrace_class_name(dpi.dpi_descattr.dtat_class));

		oprintf("\n\tMinimum Statement Attributes\n");

		oprintf("\t\tIdentifier Names: %s\n",
		    dtrace_stability_name(dpi.dpi_stmtattr.dtat_name));
		oprintf("\t\tData Semantics:   %s\n",
		    dtrace_stability_name(dpi.dpi_stmtattr.dtat_data));
		oprintf("\t\tDependency Class: %s\n",
		    dtrace_class_name(dpi.dpi_stmtattr.dtat_class));

		if (!g_exec) {
			(void) dtrace_stmt_iter(g_dtp, dcp->dc_prog,
			    (dtrace_stmt_f *)info_stmt, &last);
		} else
			oprintf("\n");
	}

	g_total += dpi.dpi_matches;
}

#include <CoreFoundation/CoreFoundation.h>

static void
dof_prune_all(void)
{
	CFTypeRef value;
	CFStringRef key_str;
	int i;

	assert(registry_entry != 0);

	for (i = 0;; i++) {
		key_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("dof-data-%d"), i);
		value = IORegistryEntryCreateCFProperty(registry_entry, key_str, 0, 0);
		if (value == NULL) {
			CFRelease(key_str);
			break;
		}
		IORegistryEntrySetCFProperty(registry_entry, CFSTR(kIONVRAMDeletePropertyKey), key_str);
		CFRelease(key_str);
	}
}

static void
anon_prog(const dtrace_cmd_t *dcp, dof_hdr_t *dof, int n)
{
	CFDataRef data;
	CFStringRef key_str;
	kern_return_t kr;

	assert(registry_entry != 0);

	if (NULL == dof) {
		dof_prune_all();
		dfatal("failed to create DOF image for '%s'", dcp->dc_name);
	}
	
	data = CFDataCreate( kCFAllocatorDefault, (const UInt8 *)dof, dof->dofh_loadsz );
	if (NULL == data) {
		dof_prune_all();
		errno = EINVAL;
		fatal("failed CFDataCreate for '%s'", dcp->dc_name);
	}
	key_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("dof-data-%d"), n);
	
	
	if ((kr = IORegistryEntrySetCFProperty(registry_entry, key_str, data)) != KERN_SUCCESS) {
		dof_prune_all();
		errno = ENOMEM;
		fatal("Failed to store DOF to nvram");
	}
	
	CFRelease(key_str);
	CFRelease(data);
}

 /*ARGSUSED*/
static int
list_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp, void *arg)
{
#pragma unused(arg)
	dtrace_probeinfo_t p;
	char funcname[DTRACE_FUNCNAMELEN + DTRACE_FUNCNAMELEN + 4];
	char *filtFunc = demangleSymbolCString((const char *)pdp->dtpd_func);
	
	if (NULL == filtFunc)
		strncpy( funcname, pdp->dtpd_func, sizeof(funcname) );
	else
		snprintf( funcname, sizeof(funcname), "%s [%s]", pdp->dtpd_func, filtFunc );

	oprintf("%5d %10s %17s %33s %s\n", pdp->dtpd_id,
	    pdp->dtpd_provider, pdp->dtpd_mod, funcname, pdp->dtpd_name);

	if (g_verbose && dtrace_probe_info(dtp, pdp, &p) == 0)
		print_probe_info(&p);

	if (NULL != filtFunc) 
		free(filtFunc);
	
	return (0);
}

/*ARGSUSED*/
static int
list_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *stp, dtrace_ecbdesc_t **last)
{
#pragma unused(pgp)
	dtrace_ecbdesc_t *edp = stp->dtsd_ecbdesc;

	if (edp == *last)
		return (0);

	if (dtrace_probe_iter(g_dtp, &edp->dted_probe, list_probe, NULL) != 0) {
		error("failed to match %s:%s:%s:%s: %s\n",
		    edp->dted_probe.dtpd_provider, edp->dted_probe.dtpd_mod,
		    edp->dted_probe.dtpd_func, edp->dted_probe.dtpd_name,
		    dtrace_errmsg(dtp, dtrace_errno(dtp)));
	}

	*last = edp;
	return (0);
}

/*
 * List the probes corresponding to the specified program by iterating over
 * each statement and then matching probes to the statement probe descriptions.
 */
static void
list_prog(const dtrace_cmd_t *dcp)
{
	dtrace_ecbdesc_t *last = NULL;

	(void) dtrace_stmt_iter(g_dtp, dcp->dc_prog,
	    (dtrace_stmt_f *)list_stmt, &last);
}

static void
compile_file(dtrace_cmd_t *dcp)
{
	char *arg0;
	FILE *fp;

	if ((fp = fopen(dcp->dc_arg, "r")) == NULL)
		fatal("failed to open %s", dcp->dc_arg);

	arg0 = g_argv[0];
	g_argv[0] = dcp->dc_arg;

	if ((dcp->dc_prog = dtrace_program_fcompile(g_dtp, fp,
	    g_cflags, g_argc, g_argv)) == NULL)
		dfatal("failed to compile script %s", dcp->dc_arg);

	g_argv[0] = arg0;
	(void) fclose(fp);

	dcp->dc_desc = "script";
	dcp->dc_name = dcp->dc_arg;
}

static void
compile_str(dtrace_cmd_t *dcp)
{
	char *p;

	if ((dcp->dc_prog = dtrace_program_strcompile(g_dtp, dcp->dc_arg,
	    dcp->dc_spec, g_cflags, g_argc, g_argv)) == NULL)
		dfatal("invalid probe specifier %s", dcp->dc_arg);

	if ((p = strpbrk(dcp->dc_arg, "{/;")) != NULL)
		*p = '\0'; /* crop name for reporting */

	dcp->dc_desc = "description";
	dcp->dc_name = dcp->dc_arg;
}

/*ARGSUSED*/
static void
prochandler(struct ps_prochandle *P, const char *msg, void *arg)
{
#pragma unused(arg)
#define SIG2STR_MAX 32 /* Not referenced so long as prp just below is NULL. */
#define proc_signame(x,y,z) "Unknown" /* Not referenced so long as prp just below is NULL. */
	typedef struct psinfo { int pr_wstat; } psinfo_t;
	const psinfo_t *prp = NULL;
	int pid = dtrace_proc_status(g_dtp, P)->pr_pid;

	if (msg != NULL) {
		notice("pid %d: %s\n", pid, msg);
		return;
	}

	switch (dtrace_proc_state(g_dtp, P)) {
	case PS_UNDEAD:
		/*
		 * Ideally we would like to always report pr_wstat here, but it
		 * isn't possible given current /proc semantics.  If we grabbed
		 * the process, Ppsinfo() will either fail or return a zeroed
		 * psinfo_t depending on how far the parent is in reaping it.
		 * When /proc provides a stable pr_wstat in the status file,
		 * this code can be improved by examining this new pr_wstat.
		 */
		if (prp != NULL && WIFSIGNALED(prp->pr_wstat)) {
			notice("pid %d terminated by %s\n", pid,
			    proc_signame(WTERMSIG(prp->pr_wstat),
			    name, sizeof (name)));
		} else if (prp != NULL && WEXITSTATUS(prp->pr_wstat) != 0) {
			notice("pid %d exited with status %d\n",
			    pid, WEXITSTATUS(prp->pr_wstat));
		} else {
			notice("pid %d has exited\n", pid);
		}
		g_pslive--;
		break;

	case PS_LOST:
		if (g_pslive)
			notice("pid %d has exited\n", pid);
		else
			notice("pid %d exec'd a set-id or unobservable program\n", pid);
		g_pslive--;
		break;
	}
}

/*ARGSUSED*/
static int
errhandler(const dtrace_errdata_t *data, void *arg)
{
#pragma unused(arg)
	error(data->dteda_msg);
	return (DTRACE_HANDLE_OK);
}

/*ARGSUSED*/
static int
drophandler(const dtrace_dropdata_t *data, void *arg)
{
#pragma unused(arg)
	error(data->dtdda_msg);
	return (DTRACE_HANDLE_OK);
}

/*ARGSUSED*/
static int
setopthandler(const dtrace_setoptdata_t *data, void *arg)
{
#pragma unused(arg)
	if (strcmp(data->dtsda_option, "quiet") == 0)
		g_quiet = data->dtsda_newval != DTRACEOPT_UNSET;

	if (strcmp(data->dtsda_option, "flowindent") == 0)
		g_flowindent = data->dtsda_newval != DTRACEOPT_UNSET;

	return (DTRACE_HANDLE_OK);
}

#define	BUFDUMPHDR(hdr) \
	(void) printf("%s: %s%s\n", g_pname, hdr, strlen(hdr) > 0 ? ":" : "");

#define	BUFDUMPSTR(ptr, field) \
	(void) printf("%s: %20s => ", g_pname, #field);	\
	if ((ptr)->field != NULL) {			\
		const char *ch = (ptr)->field;		\
		(void) printf("\"");			\
		do {					\
			if (*ch == '\n') {		\
				(void) printf("\\n");	\
				continue;		\
			}				\
							\
			(void) printf("%c", *ch);	\
		} while (*ch++ != '\0');			\
		(void) printf("\"\n");			\
	} else {					\
		(void) printf("<NULL>\n");		\
	}

#define	BUFDUMPASSTR(ptr, field, str) \
	(void) printf("%s: %20s => %s\n", g_pname, #field, str);

#define	BUFDUMP(ptr, field) \
	(void) printf("%s: %20s => %lld\n", g_pname, #field, \
	    (long long)(ptr)->field);

#define	BUFDUMPPTR(ptr, field) \
	(void) printf("%s: %20s => %s\n", g_pname, #field, \
	    (ptr)->field != NULL ? "<non-NULL>" : "<NULL>");

/*ARGSUSED*/
static int
bufhandler(const dtrace_bufdata_t *bufdata, void *arg)
{
#pragma unused(arg)
	const dtrace_aggdata_t *agg = bufdata->dtbda_aggdata;
	const dtrace_recdesc_t *rec = bufdata->dtbda_recdesc;
	const dtrace_probedesc_t *pd;
	uint32_t flags = bufdata->dtbda_flags;
	char buf[512], *c = buf, *end = c + sizeof (buf);
	int i, printed;

	struct {
		const char *name;
		uint32_t value;
	} flagnames[] = {
	    { "AGGVAL",		DTRACE_BUFDATA_AGGVAL },
	    { "AGGKEY",		DTRACE_BUFDATA_AGGKEY },
	    { "AGGFORMAT",	DTRACE_BUFDATA_AGGFORMAT },
	    { "AGGLAST",	DTRACE_BUFDATA_AGGLAST },
	    { "???",		UINT32_MAX },
	    { NULL }
	};

	if (bufdata->dtbda_probe != NULL) {
		pd = bufdata->dtbda_probe->dtpda_pdesc;
	} else if (agg != NULL) {
		pd = agg->dtada_pdesc;
	} else {
		pd = NULL;
	}

	BUFDUMPHDR(">>> Called buffer handler");
	BUFDUMPHDR("");

	BUFDUMPHDR("  dtrace_bufdata");
	BUFDUMPSTR(bufdata, dtbda_buffered);
	BUFDUMPPTR(bufdata, dtbda_probe);
	BUFDUMPPTR(bufdata, dtbda_aggdata);
	BUFDUMPPTR(bufdata, dtbda_recdesc);

	(void) snprintf(c, end - c, "0x%x ", bufdata->dtbda_flags);
	c += strlen(c);

	for (i = 0, printed = 0; flagnames[i].name != NULL; i++) {
		if (!(flags & flagnames[i].value))
			continue;

		(void) snprintf(c, end - c,
		    "%s%s", printed++ ? " | " : "(", flagnames[i].name);
		c += strlen(c);
		flags &= ~flagnames[i].value;
	}

	if (printed)
		(void) snprintf(c, end - c, ")");

	BUFDUMPASSTR(bufdata, dtbda_flags, buf);
	BUFDUMPHDR("");

	if (pd != NULL) {
		BUFDUMPHDR("  dtrace_probedesc");
		BUFDUMPSTR(pd, dtpd_provider);
		BUFDUMPSTR(pd, dtpd_mod);
		BUFDUMPSTR(pd, dtpd_func);
		BUFDUMPSTR(pd, dtpd_name);
		BUFDUMPHDR("");
	}

	if (rec != NULL) {
		BUFDUMPHDR("  dtrace_recdesc");
		BUFDUMP(rec, dtrd_action);
		BUFDUMP(rec, dtrd_size);

		if (agg != NULL) {
			uint8_t *data;
			int lim = rec->dtrd_size;

			(void) sprintf(buf, "%d (data: ", rec->dtrd_offset);
			c = buf + strlen(buf);

			if (lim > sizeof (uint64_t))
				lim = sizeof (uint64_t);

			data = (uint8_t *)agg->dtada_data + rec->dtrd_offset;

			for (i = 0; i < lim; i++) {
				(void) snprintf(c, end - c, "%s%02x",
				    i == 0 ? "" : " ", *data++);
				c += strlen(c);
			}

			(void) snprintf(c, end - c,
			    "%s)", lim < rec->dtrd_size ? " ..." : "");
			BUFDUMPASSTR(rec, dtrd_offset, buf);
		} else {
			BUFDUMP(rec, dtrd_offset);
		}

		BUFDUMPHDR("");
	}

	if (agg != NULL) {
		dtrace_aggdesc_t *desc = agg->dtada_desc;

		BUFDUMPHDR("  dtrace_aggdesc");
		BUFDUMPSTR(desc, dtagd_name);
		BUFDUMP(desc, dtagd_varid);
		BUFDUMP(desc, dtagd_id);
		BUFDUMP(desc, dtagd_nrecs);
		BUFDUMPHDR("");
	}

	return (DTRACE_HANDLE_OK);
}

/*ARGSUSED*/
static int
chewrec(const dtrace_probedata_t *data, const dtrace_recdesc_t *rec, void *arg)
{
#pragma unused(arg)
	dtrace_actkind_t act;
	uintptr_t addr;

	if (rec == NULL) {
		/*
		 * We have processed the final record; output the newline if
		 * we're not in quiet mode.
		 */
		if (!g_quiet)
			oprintf("\n");

		return (DTRACE_CONSUME_NEXT);
	}

	act = rec->dtrd_action;
	addr = (uintptr_t)data->dtpda_data;

	if (act == DTRACEACT_EXIT) {
		g_status = *((uint32_t *)addr);
		return (DTRACE_CONSUME_NEXT);
	}

	return (DTRACE_CONSUME_THIS);
}

/*ARGSUSED*/
static int
chew(const dtrace_probedata_t *data, void *arg)
{
#pragma unused(arg)
	dtrace_probedesc_t *pd = data->dtpda_pdesc;
	processorid_t cpu = data->dtpda_cpu;
	static int heading;

	if (g_impatient) {
		g_newline = 0;
		return (DTRACE_CONSUME_ABORT);
	}

	if (heading == 0) {
		if (!g_flowindent) {
			if (!g_quiet) {
				oprintf("%3s %6s %32s\n",
				    "CPU", "ID", "FUNCTION:NAME");
			}
		} else {
			oprintf("%3s %-41s\n", "CPU", "FUNCTION");
		}
		heading = 1;
	}

	if (!g_flowindent) {
		if (!g_quiet) {
			char name[DTRACE_FUNCNAMELEN + DTRACE_NAMELEN + 2];

			(void) snprintf(name, sizeof (name), "%s:%s",
			    pd->dtpd_func, pd->dtpd_name);

			oprintf("%3d %6d %32s ", cpu, pd->dtpd_id, name);
		}
	} else {
		int indent = data->dtpda_indent;
		char *name;
		size_t len;

		if (data->dtpda_flow == DTRACEFLOW_NONE) {
			len = indent + DTRACE_FUNCNAMELEN + DTRACE_NAMELEN + 5;
			name = alloca(len);
			(void) snprintf(name, len, "%*s%s%s:%s", indent, "",
			    data->dtpda_prefix, pd->dtpd_func,
			    pd->dtpd_name);
		} else {
			len = indent + DTRACE_FUNCNAMELEN + 5;
			name = alloca(len);
			(void) snprintf(name, len, "%*s%s%s", indent, "",
			    data->dtpda_prefix, pd->dtpd_func);
		}

		oprintf("%3d %-41s ", cpu, name);
	}

	return (DTRACE_CONSUME_THIS);
}

static void
go(void)
{
	int i;

	struct {
		char *name;
		char *optname;
		dtrace_optval_t val;
	} bufs[] = {
		{ "buffer size", "bufsize", 0 },
		{ "aggregation size", "aggsize", 0 },
		{ "speculation size", "specsize", 0 },
		{ "dynamic variable size", "dynvarsize", 0 },
		{ NULL, NULL, 0 }
	}, rates[] = {
		{ "cleaning rate", "cleanrate", 0 },
		{ "status rate", "statusrate", 0 },
		{ NULL }
	};

	for (i = 0; bufs[i].name != NULL; i++) {
		if (dtrace_getopt(g_dtp, bufs[i].optname, &bufs[i].val) == -1)
			fatal("couldn't get option %s", bufs[i].optname);
	}

	for (i = 0; rates[i].name != NULL; i++) {
		if (dtrace_getopt(g_dtp, rates[i].optname, &rates[i].val) == -1)
			fatal("couldn't get option %s", rates[i].optname);
	}

	if (dtrace_go(g_dtp) == -1)
		dfatal("could not enable tracing");

	for (i = 0; bufs[i].name != NULL; i++) {
		dtrace_optval_t j = 0, mul = 10;
		dtrace_optval_t nsize;

		if (bufs[i].val == DTRACEOPT_UNSET)
			continue;

		(void) dtrace_getopt(g_dtp, bufs[i].optname, &nsize);

		if (nsize == DTRACEOPT_UNSET || nsize == 0)
			continue;

		if (nsize >= bufs[i].val - sizeof (uint64_t))
			continue;

		for (; (INT64_C(1) << mul) <= nsize; j++, mul += 10)
			continue;

		if (!(nsize & ((INT64_C(1) << (mul - 10)) - 1))) {
			error("%s lowered to %lld%c\n", bufs[i].name,
			    (long long)nsize >> (mul - 10), " kmgtpe"[j]);
		} else {
			error("%s lowered to %lld bytes\n", bufs[i].name,
			    (long long)nsize);
		}
	}

	for (i = 0; rates[i].name != NULL; i++) {
		dtrace_optval_t nval;
		char *dir;

		if (rates[i].val == DTRACEOPT_UNSET)
			continue;

		(void) dtrace_getopt(g_dtp, rates[i].optname, &nval);

		if (nval == DTRACEOPT_UNSET || nval == 0)
			continue;

		if (rates[i].val == nval)
			continue;

		dir = nval > rates[i].val ? "reduced" : "increased";

		if (nval <= NANOSEC && (NANOSEC % nval) == 0) {
			error("%s %s to %lld hz\n", rates[i].name, dir,
			    (long long)NANOSEC / (long long)nval);
			continue;
		}

		if ((nval % NANOSEC) == 0) {
			error("%s %s to once every %lld seconds\n",
			    rates[i].name, dir,
			    (long long)nval / (long long)NANOSEC);
			continue;
		}

		error("%s %s to once every %lld nanoseconds\n",
		    rates[i].name, dir, (long long)nval);
	}
}

#define DTRACE_PRIORITY_FOREGROUND 47

static void
set_sched_policy() {
	int policy, err;
	struct sched_param param;

	err = pthread_getschedparam(pthread_self(), &policy, &param);
	if (err) {
		notice("could not set thread priority: cannot retrieve thread scheduling parameters");
		return;
	}

	param.sched_priority = DTRACE_PRIORITY_FOREGROUND;

	err = pthread_setschedparam(pthread_self(), policy, &param);
	if (err) {
		notice("could not set thread priority to %d", param.sched_priority);
	}

	err = pthread_set_fixedpriority_self();
	if (err) {
		notice("could not set thread scheduling priority to fixed");
	}

}

/*ARGSUSED*/
static void
intr(int signo)
{
#pragma unused(signo)
	if (!g_intr)
		g_newline = 1;

	if (g_intr++)
		g_impatient = 1;
}

int
main(int argc, char *argv[])
{
	dtrace_bufdesc_t buf;
	struct sigaction act, oact;
	dtrace_status_t status[2];
	dtrace_optval_t opt;
	dtrace_cmd_t *dcp;

	int done = 0, mode = 0;
	int err, i;
	char c, *p, **v;
	struct ps_prochandle *P;
	pid_t pid;
	int cc, k;

	// This assignment can no longer be done staticly, that causes a compiler error.
	g_ofp = stdout;

	g_pname = basename(argv[0]);

	if (argc == 1)
		return (usage(stderr));

	if ((g_argv = malloc(sizeof (char *) * argc)) == NULL ||
	    (g_cmdv = malloc(sizeof (dtrace_cmd_t) * argc)) == NULL ||
	    (g_psv = malloc(sizeof (struct ps_prochandle *) * argc)) == NULL)
		fatal("failed to allocate memory for arguments");

	g_argv[g_argc++] = argv[0];	/* propagate argv[0] to D as $0/$$0 */
	argv[0] = g_pname;		/* rewrite argv[0] for getopt errors */

	bzero(status, sizeof (status));
	bzero(&buf, sizeof (buf));

	/*
	 * Make an initial pass through argv[] processing any arguments that
	 * affect our behavior mode (g_mode) and flags used for dtrace_open().
	 * We also accumulate arguments that are not affiliated with getopt
	 * options into g_argv[], and abort if any invalid options are found.
	 */
	/* Darwin's getopt(): Think different. */
	optind = 1;
	for (k = 1; k < argc; k++) {
		while ((cc = getopt(argc, argv, DTRACE_OPTSTR)) != -1) {
			c = cc;
			switch (c) {
			case '3':
				if (strcmp(optarg, "2") != 0) {
					(void) fprintf(stderr,
					    "%s: illegal option -- 3%s\n",
					    argv[0], optarg);
					return (usage(stderr));
				}
				(void) fprintf(stderr, "%s: ignored option -- 3%s\n",
					    argv[0], optarg);
				break;

			case '6':
				if (strcmp(optarg, "4") != 0) {
					(void) fprintf(stderr,
					    "%s: illegal option -- 6%s\n",
					    argv[0], optarg);
					return (usage(stderr));
				}
				(void) fprintf(stderr, "%s: ignored option -- 6%s\n",
					    argv[0], optarg);
				break;

			case 'a':
				if (0 == strcmp(optarg, "rch")) {
					if (optind < argc && '-' != argv[optind][0]) {
						const char* arch_string = argv[optind++];
						cpu_type_t arch = dtrace_str2arch(arch_string);
						if (arch == 0) {
							(void) fprintf(stderr,
								"%s: invalid architecture -- %s\n",
								argv[0], arch_string);
							return usage(stderr);
						}
						g_oflags |= (arch & CPU_ARCH_ABI64) ? DTRACE_O_LP64 : DTRACE_O_ILP32;
					}
					else
					{
						(void) fprintf(stderr,
							"%s: option requires an argument -- arch\n",
							argv[0]);
						return usage(stderr);
					}
				}
				else
				{
					g_grabanon++;
					optind--;
				}
				break;

			case 'A':
#if DTRACE_TARGET_APPLE_MAC
				if (csr_check(CSR_ALLOW_UNRESTRICTED_DTRACE) != 0 && csr_check(CSR_ALLOW_APPLE_INTERNAL) != 0) {
					fatal("system integrity protection restricts the use of anonymous tracing");
				}
#endif /* DTRACE_TARGET_APPLE_MAC */
				g_mode = DMODE_ANON;
				g_exec = 0;
				mode++;
				break;

			case 'e':
				g_exec = 0;
				done = 1;
				break;

			case 'h':
				g_mode = DMODE_HEADER;
				g_oflags |= DTRACE_O_NODEV;
				g_cflags |= DTRACE_C_ZDEFS; /* -h implies -Z */
				g_exec = 0;
				mode++;
				break;

			case 'G':
				g_mode = DMODE_LINK;
				g_oflags |= DTRACE_O_NODEV;
				g_cflags |= DTRACE_C_ZDEFS; /* -G implies -Z */
				g_exec = 0;
				mode++;
				break;

			case 'l':
				g_mode = DMODE_LIST;
				g_cflags |= DTRACE_C_ZDEFS; /* -l implies -Z */
				mode++;
				break;

			case 'V':
				g_mode = DMODE_VERS;
				mode++;
				break;

			case ':':
				if ('a' == optopt) { // dangling '-a' without optarg is OK
					g_grabanon++;
					break;
				}
				else
				{
					(void) fprintf(stderr,
						"%s: option requires an argument -- %c\n",
						argv[0], optopt);
					return usage(stderr);
				}
				/* NOTREACHED */

			case 'x':
				if ((p = strchr(optarg, '=')) != NULL)
					*p++ = '\0';
				/*
				 * At that stage, only parse disallow_dsym
				 * that we need to be set before dtrace_open
				 */

				if (strcmp(optarg, "disallow_dsym") == 0) {
					_dtrace_disallow_dsym = 1;
				}
				// Restore the option string
				if (p != NULL)
					*(--p) = '=';

				break;
			default:
				if (strchr(DTRACE_OPTSTR, c) == NULL)
					return (usage(stderr));
			}
		}

		/* 'k' should track the advance of optind through argv.
		 * When getopt() returns -1 and optind freezes, 'k' iterates 
		 * over the remaining elements in argv.
		 */
		if (k < optind)
			k = optind;

		if (k < argc)
			g_argv[g_argc++] = argv[k];
	}

	if (mode > 1) {
		(void) fprintf(stderr, "%s: only one of the [-AGhlV] options "
		    "can be specified at a time\n", g_pname);
		return (E_USAGE);
	}

	if (g_mode == DMODE_VERS)
		return (printf("%s: %s\n", g_pname, _dtrace_version) <= 0);

#if DTRACE_TARGET_APPLE_MAC
	if (g_mode != DMODE_HEADER && csr_check(CSR_ALLOW_UNRESTRICTED_DTRACE) != 0) {
		notice("system integrity protection is on, some features will not be available\n");
	}
#endif /* DTRACE_TARGET_APPLE_MAC */

	/*
	 * Open libdtrace.  If we are not actually going to be enabling any
	 * instrumentation attempt to reopen libdtrace using DTRACE_O_NODEV.
	 */
	while ((g_dtp = dtrace_open(DTRACE_VERSION, g_oflags, &err)) == NULL) {
		if (!(g_oflags & DTRACE_O_NODEV) && !g_exec && !g_grabanon) {
			g_oflags |= DTRACE_O_NODEV;
			continue;
		}

		fatal("failed to initialize dtrace: %s\n",
		    dtrace_errmsg(NULL, err));
	}

	(void) dtrace_setopt(g_dtp, "bufsize", "4m");
	(void) dtrace_setopt(g_dtp, "aggsize", "4m");
	(void) dtrace_setopt(g_dtp, "temporal", "yes");

	(void) dtrace_setopt(g_dtp, "stacksymbols", "enabled");

	/*
	 * If -G is specified, enable -xlink=dynamic and -xunodefs to permit
	 * references to undefined symbols to remain as unresolved relocations.
	 * If -A is specified, enable -xlink=primary to permit static linking
	 * only to kernel symbols that are defined in a primary kernel module.
	 */
	if (g_mode == DMODE_LINK) {
		(void) dtrace_setopt(g_dtp, "linkmode", "dynamic");
		(void) dtrace_setopt(g_dtp, "unodefs", NULL);

		/*
		 * Use the remaining arguments as the list of object files
		 * when in linker mode.
		 */
		g_objc = g_argc - 1;
		g_objv = g_argv + 1;

		/*
		 * We still use g_argv[0], the name of the executable.
		 */
		g_argc = 1;
	} else if (g_mode == DMODE_ANON)
		(void) dtrace_setopt(g_dtp, "linkmode", "primary");

	/*
	 * Now that we have libdtrace open, make a second pass through argv[]
	 * to perform any dtrace_setopt() calls and change any compiler flags.
	 * We also accumulate any program specifications into our g_cmdv[] at
	 * this time; these will compiled as part of the fourth processing pass.
	 */
	optreset = 1;
	optind = 1;
	for (k = 1; k < argc; k++) {
		while ((cc = getopt(argc, argv, DTRACE_OPTSTR)) != -1) {
			c = cc;
			switch (c) {
			case 'a':
				if (0 == strcmp(optarg, "rch")) {
					if (dtrace_setopt(g_dtp, "arch", argv[optind++]) != 0)
						dfatal("failed to set -arch");
				}
				else
				{
					if (dtrace_setopt(g_dtp, "grabanon", 0) != 0)
						dfatal("failed to set -a");
					optind--;
				}
				break;

			case 'b':
				if (dtrace_setopt(g_dtp,
				    "bufsize", optarg) != 0)
					dfatal("failed to set -b %s", optarg);
				break;

			case 'B':
				g_ofp = NULL;
				break;

			case 'C':
				g_cflags |= DTRACE_C_CPP;
				break;

			case 'D':
				if (dtrace_setopt(g_dtp, "define", optarg) != 0)
					dfatal("failed to set -D %s", optarg);
				break;

			case 'f':
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_str;
				dcp->dc_spec = DTRACE_PROBESPEC_FUNC;
				dcp->dc_arg = optarg;
				break;

			case 'F':
				if (dtrace_setopt(g_dtp, "flowindent", 0) != 0)
					dfatal("failed to set -F");
				break;

			case 'h':
				(void) dtrace_setopt(g_dtp, "nolibs", NULL); /* In case /usr/lib/dtrace/ is broken, -h can succeed. */

			case 'H':
				if (dtrace_setopt(g_dtp, "cpphdrs", 0) != 0)
					dfatal("failed to set -H");
				break;

			case 'i':
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_str;
				dcp->dc_spec = DTRACE_PROBESPEC_NAME;
				dcp->dc_arg = optarg;
				break;

			case 'I':
				if (dtrace_setopt(g_dtp, "incdir", optarg) != 0)
					dfatal("failed to set -I %s", optarg);
				break;

			case 'L':
				if (dtrace_setopt(g_dtp, "libdir", optarg) != 0)
					dfatal("failed to set -L %s", optarg);
				break;

			case 'm':
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_str;
				dcp->dc_spec = DTRACE_PROBESPEC_MOD;
				dcp->dc_arg = optarg;
				break;

			case 'n':
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_str;
				dcp->dc_spec = DTRACE_PROBESPEC_NAME;
				dcp->dc_arg = optarg;
				break;

			case 'P':
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_str;
				dcp->dc_spec = DTRACE_PROBESPEC_PROVIDER;
				dcp->dc_arg = optarg;
				break;

			case 'q':
				if (dtrace_setopt(g_dtp, "quiet", 0) != 0)
					dfatal("failed to set -q");
				break;

			case 'o':
				g_ofile = optarg;
				break;

			case 's':
				if(g_mode == DMODE_LINK) {
					g_script_name = optarg;
				}
				else {
				dcp = &g_cmdv[g_cmdc++];
				dcp->dc_func = compile_file;
				dcp->dc_spec = DTRACE_PROBESPEC_NONE;
				dcp->dc_arg = optarg;
				}
				break;

			case 'S':
				g_cflags |= DTRACE_C_DIFV;
				break;

			case 'U':
				if (dtrace_setopt(g_dtp, "undef", optarg) != 0)
					dfatal("failed to set -U %s", optarg);
				break;

			case 'v':
				g_verbose++;
				break;

			case 'w':
				if (dtrace_setopt(g_dtp, "destructive", 0) != 0)
					dfatal("failed to set -w");
				break;

			case 'x':
				if ((p = strchr(optarg, '=')) != NULL)
					*p++ = '\0';

				if (dtrace_setopt(g_dtp, optarg, p) != 0)
					dfatal("failed to set -x %s", optarg);
				break;

			case 'X':
				if (dtrace_setopt(g_dtp, "stdc", optarg) != 0)
					dfatal("failed to set -X %s", optarg);
				break;

			case 'W':
				/* Using -W automatically implies -Z. */
				g_cflags |= DTRACE_C_ZDEFS;
				break;

			case 'Z':
				g_cflags |= DTRACE_C_ZDEFS;
				break;

			case ':':
				if ('a' == optopt) { // dangling '-a' without optarg is OK
					if (dtrace_setopt(g_dtp, "grabanon", 0) != 0)
						dfatal("failed to set -a");
					break;
				}
				else
				{
					(void) fprintf(stderr,
                                   "%s: option requires an argument -- %c\n",
                                   g_pname, optopt);
					return usage(stderr);
				}
				/* NOTREACHED */

			default:
				if (strchr(DTRACE_OPTSTR, c) == NULL)
					return (usage(stderr));
			}
		}
	}

	if (g_ofp == NULL && g_mode != DMODE_EXEC) {
		(void) fprintf(stderr, "%s: -B not valid in combination"
		    " with [-AGl] options\n", g_pname);
		return (E_USAGE);
	}

	if (g_ofile != NULL && g_mode == DMODE_ANON) {
		(void) fprintf(stderr, "%s: -o not valid in combination"
		    " with -A option\n", g_pname);
		return (E_USAGE);
	}

	if (g_ofp == NULL && g_ofile != NULL) {
		(void) fprintf(stderr, "%s: -B not valid in combination"
		    " with -o option\n", g_pname);
		return (E_USAGE);
	}

	/*
	 * In our third pass we handle any command-line options related to
	 * grabbing or creating victim processes.  The behavior of these calls
	 * may been affected by any library options set by the second pass.
	 */
	optreset = 1;
	optind = 1;
	for (k = 1; k < argc; k++) {
		while ((cc = getopt(argc, argv, DTRACE_OPTSTR)) != -1) {
			c = cc;
			switch (c) {
			case 'c':
				if ((v = make_argv(optarg)) == NULL)
					fatal("failed to allocate memory");

				P = dtrace_proc_create(g_dtp, v[0], v);
				if (P == NULL)
					dfatal(NULL); /* dtrace_errmsg() only */

				g_psv[g_psc++] = P;
				free(v);
				g_proc_created_grabbed++;
				break;

			case 'p':
				errno = 0;
				pid = strtol(optarg, &p, 10);

				if (errno != 0 || p == optarg || p[0] != '\0')
					fatal("invalid pid: %s\n", optarg);

				P = dtrace_proc_grab(g_dtp, pid, 0);
				if (P == NULL)
					dfatal(NULL); /* dtrace_errmsg() only */

				g_psv[g_psc++] = P;
				g_proc_created_grabbed++;
				break;

			case 'W':
				P = dtrace_proc_waitfor(g_dtp, optarg);
				if (P == NULL)
					dfatal(NULL);
				g_psv[g_psc++] = P;
				g_wait_proc++;
				break;

			case 'a':
				if (0 == strcmp(optarg, "rch")) {
					optind++;
				}
				break;
			}
		}
	}

	/*
	 * In our fourth pass we finish g_cmdv[] by calling dc_func to convert
	 * each string or file specification into a compiled program structure.
	 */
	for (i = 0; i < g_cmdc; i++)
		g_cmdv[i].dc_func(&g_cmdv[i]);

	if (g_mode != DMODE_LIST) {
		if (dtrace_handle_err(g_dtp, &errhandler, NULL) == -1)
			dfatal("failed to establish error handler");

		if (dtrace_handle_drop(g_dtp, &drophandler, NULL) == -1)
			dfatal("failed to establish drop handler");

		if (dtrace_handle_proc(g_dtp, &prochandler, NULL) == -1)
			dfatal("failed to establish proc handler");

		if (dtrace_handle_setopt(g_dtp, &setopthandler, NULL) == -1)
			dfatal("failed to establish setopt handler");

		if (g_ofp == NULL &&
		    dtrace_handle_buffered(g_dtp, &bufhandler, NULL) == -1)
			dfatal("failed to establish buffered handler");
	}

	(void) dtrace_getopt(g_dtp, "flowindent", &opt);
	g_flowindent = opt != DTRACEOPT_UNSET;

	(void) dtrace_getopt(g_dtp, "grabanon", &opt);
	g_grabanon = opt != DTRACEOPT_UNSET;

	(void) dtrace_getopt(g_dtp, "quiet", &opt);
	g_quiet = opt != DTRACEOPT_UNSET;

	/*
	 * Now make a fifth and final pass over the options that have been
	 * turned into programs and saved in g_cmdv[], performing any mode-
	 * specific processing.  If g_mode is DMODE_EXEC, we will break out
	 * of the switch() and continue on to the data processing loop.  For
	 * other modes, we will exit dtrace once mode-specific work is done.
	 */
	switch (g_mode) {
	case DMODE_EXEC:
		if (g_ofile != NULL && (g_ofp = fopen(g_ofile, "a")) == NULL)
			fatal("failed to open output file '%s'", g_ofile);

		for (i = 0; i < g_cmdc; i++)
			exec_prog(&g_cmdv[i]);

		if (done && !g_grabanon) {
			dtrace_close(g_dtp);
			return (g_status);
		}
		break;

	case DMODE_ANON:
		registry_entry = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
		if (registry_entry == 0) {
			error("nvram is not supported on this system\n");
			dtrace_close(g_dtp);
			return (g_status);
		}

		dof_prune_all();

		if (g_cmdc == 0) {
			notice("removed anonymous enabling from nvram\n");
			dtrace_close(g_dtp);
			return (g_status);
		}


		for (i = 0; i < g_cmdc; i++) {
			anon_prog(&g_cmdv[i],
				dtrace_dof_create(g_dtp, g_cmdv[i].dc_prog, 0), i);
		}

		anon_prog(NULL, dtrace_geterr_dof(g_dtp), i++);
		anon_prog(NULL, dtrace_getopt_dof(g_dtp), i++);

		IOObjectRelease(registry_entry);

		notice("saved anonymous enabling in nvram\n");
		dtrace_close(g_dtp);
		return (g_status);

	case DMODE_LINK:
		if (g_cmdc == 0) {
			(void) fprintf(stderr, "%s: -G requires one or more "
			    "scripts or enabling options\n", g_pname);
			dtrace_close(g_dtp);
			return (E_USAGE);
		}

		dtrace_close(g_dtp);
		return (g_status);

	case DMODE_LIST:
		if (g_ofile != NULL && (g_ofp = fopen(g_ofile, "a")) == NULL)
			fatal("failed to open output file '%s'", g_ofile);

		oprintf("%5s %10s %17s %33s %s\n",
		    "ID", "PROVIDER", "MODULE", "FUNCTION", "NAME");

		for (i = 0; i < g_cmdc; i++)
			list_prog(&g_cmdv[i]);

		if (g_cmdc == 0)
			(void) dtrace_probe_iter(g_dtp, NULL, list_probe, NULL);

		dtrace_close(g_dtp);
		return (g_status);

	case DMODE_HEADER:
		if (g_cmdc == 0) {
			(void) fprintf(stderr, "%s: -h requires one or more "
			    "scripts or enabling options\n", g_pname);
			dtrace_close(g_dtp);
			return (E_USAGE);
		}

		if (g_ofile == NULL) {
			if (g_cmdc > 1) {
				(void) fprintf(stderr, "%s: -h requires an "
				    "output file if multiple scripts are "
				    "specified\n", g_pname);
				dtrace_close(g_dtp);
				return (E_USAGE);
			}

			if ((p = strrchr(g_cmdv[0].dc_arg, '.')) == NULL ||
			    strcmp(p, ".d") != 0) {
				(void) fprintf(stderr, "%s: -h requires an "
				    "output file if no scripts are "
				    "specified\n", g_pname);
				dtrace_close(g_dtp);
				return (E_USAGE);
			}

			p[0] = '\0'; /* strip .d suffix */
			g_ofile = p = g_cmdv[0].dc_ofile;
			(void) snprintf(p, sizeof (g_cmdv[0].dc_ofile),
			    "%s.h", basename(g_cmdv[0].dc_arg));
		}

		if ((g_ofp = fopen(g_ofile, "w")) == NULL)
			fatal("failed to open header file '%s'", g_ofile);

		oprintf("/*\n * Generated by dtrace(1M).\n */\n\n");

		if (dtrace_program_header(g_dtp, g_ofp, g_ofile) != 0 ||
		    fclose(g_ofp) == EOF)
			dfatal("failed to create header file %s", g_ofile);

		dtrace_close(g_dtp);
		return (g_status);
	}

	/*
	 * If -a and -Z were not specified and no probes have been matched, no
	 * probe criteria was specified on the command line and we abort.
	 */
	if (g_total == 0 && !g_grabanon && !(g_cflags & DTRACE_C_ZDEFS))
		dfatal("no probes %s\n", g_cmdc ? "matched" : "specified");

	/**
	 * Set our scheduling policy
	 */
	set_sched_policy();

	/*
	 * Start tracing.  Once we dtrace_go(), reload any options that affect
	 * our globals in case consuming anonymous state has changed them.
	 */
	go();
	(void) dtrace_getopt(g_dtp, "flowindent", &opt);
	g_flowindent = opt != DTRACEOPT_UNSET;

	(void) dtrace_getopt(g_dtp, "grabanon", &opt);
	g_grabanon = opt != DTRACEOPT_UNSET;

	(void) dtrace_getopt(g_dtp, "quiet", &opt);
	g_quiet = opt != DTRACEOPT_UNSET;

	(void) dtrace_getopt(g_dtp, "destructive", &opt);
	if (opt != DTRACEOPT_UNSET)
		notice("allowing destructive actions\n");

	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = intr;

	if (sigaction(SIGINT, NULL, &oact) == 0 && oact.sa_handler != SIG_IGN)
		(void) sigaction(SIGINT, &act, NULL);

	if (sigaction(SIGTERM, NULL, &oact) == 0 && oact.sa_handler != SIG_IGN)
		(void) sigaction(SIGTERM, &act, NULL);

	/*
	 * Now that tracing is active and we are ready to consume trace data,
	 * continue any grabbed or created processes, setting them running
	 * using the /proc control mechanism inside of libdtrace.
	 */
	for (i = 0; i < g_psc; i++)
		dtrace_proc_continue(g_dtp, g_psv[i]);

	g_pslive = g_psc; /* count for prochandler() */

	do {
		if (!g_intr && !done)
			dtrace_sleep(g_dtp);

		if (g_newline) {
			/*
			 * Output a newline just to make the output look
			 * slightly cleaner.  Note that we do this even in
			 * "quiet" mode...
			 */
			oprintf("\n");
			g_newline = 0;
		}

		if (done || g_intr || (g_psc != 0 && g_pslive == 0)) {
			done = 1;
			if (dtrace_stop(g_dtp) == -1)
				dfatal("couldn't stop tracing");
		}

		switch (dtrace_work(g_dtp, g_ofp, chew, chewrec, NULL)) {
		case DTRACE_WORKSTATUS_DONE:
			done = 1;
			break;
		case DTRACE_WORKSTATUS_OKAY:
			break;
		default:
			if (!g_impatient && dtrace_errno(g_dtp) != EINTR)
				dfatal("processing aborted");
		}

		if (g_ofp != NULL && fflush(g_ofp) == EOF)
			clearerr(g_ofp);
	} while (!done);

	oprintf("\n");

	if (!g_impatient) {
		if (dtrace_aggregate_print(g_dtp, g_ofp, NULL) == -1 &&
		    dtrace_errno(g_dtp) != EINTR)
			dfatal("failed to print aggregations");
	}

	dtrace_close(g_dtp);
	return (g_status);
}
