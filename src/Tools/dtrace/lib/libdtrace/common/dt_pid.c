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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <assert.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <libgen.h>
#include <stddef.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_pid.h>
#include <dt_string.h>

struct dt_libproc_fn dt_libproc_funcs[DT_PR_MAX] = {
	[DT_PR_CREATE] = {
		.lookup_by_name = Pxlookup_by_name,
		.object_iter = Pobject_iter,
		.objc_method_iter = Pobjc_method_iter,
		.symbol_iter_by_addr = Psymbol_iter_by_addr

	},
	[DT_PR_LINK] = {
		.lookup_by_name = Pxlookup_by_name_new_syms,
		.object_iter = Pobject_iter_new_syms,
		.objc_method_iter = Pobjc_method_iter_new_syms,
		.symbol_iter_by_addr = Psymbol_iter_by_addr_new_syms
	}
};

void dt_proc_bpdisable(dt_proc_t *dpr);

/*
 * Compose the lmid and object name into the canonical representation. We
 * omit the lmid for the default link map for convenience.
 */
static void
dt_pid_objname(char *buf, size_t len, Lmid_t lmid, const char *obj)
{
	if (lmid == LM_ID_BASE)
		(void) strncpy(buf, obj, len);
	else
		(void) snprintf(buf, len, "LM%llx`%s", lmid, obj);
}

int
dt_pid_error(dtrace_hdl_t *dtp, dt_pcb_t *pcb, dt_proc_t *dpr,
    fasttrap_probe_spec_t *ftp, dt_errtag_t tag, const char *fmt, ...)
{
	va_list ap;
	int len;

	if (ftp != NULL)
		dt_free(dtp, ftp);

	va_start(ap, fmt);
	if (pcb == NULL) {
		assert(dpr != NULL);
		len = vsnprintf(dpr->dpr_errmsg, sizeof (dpr->dpr_errmsg),
		    fmt, ap);
		assert(len >= 2);
		if (dpr->dpr_errmsg[len - 2] == '\n')
			dpr->dpr_errmsg[len - 2] = '\0';
	} else {
		dt_set_errmsg(dtp, dt_errtag(tag), pcb->pcb_region,
		    pcb->pcb_filetag, pcb->pcb_fileptr ? yylineno : 0, fmt, ap);
	}
	va_end(ap);

	return (1);
}

static char*
dt_pid_provider(fasttrap_provider_type_t type)
{
	switch (type) {
		case DTFTP_PROVIDER_PID:
			return FASTTRAP_PID_NAME;
		case DTFTP_PROVIDER_OBJC:
			return FASTTRAP_OBJC_NAME;
		case DTFTP_PROVIDER_ONESHOT:
			return FASTTRAP_ONESHOT_NAME;
		default:
			return "";
	}
}

int
dt_pid_per_sym(dt_pid_probe_t *pp, const GElf_Sym *symp, const char *func)
{
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	fasttrap_probe_spec_t *ftp;
	uint64_t off;
	char *end;
	uint_t nmatches = 0;
	ulong_t sz;
	int glob, err;
	int isdash = strcmp("-", func) == 0;
	pid_t pid;

	pid = Pstatus(pp->dpp_pr)->pr_pid;

	dt_dprintf_debug("creating probe %s%d:%s:%s:%s",
	    dt_pid_provider(pp->dpp_provider_type), (int)pid, pp->dpp_obj,
	    func, pp->dpp_name);

	sz = sizeof (fasttrap_probe_spec_t) + (isdash ? 4 :
	    (symp->st_size - 1) * sizeof (ftp->ftps_offs[0]));

	if ((ftp = dt_alloc(dtp, sz)) == NULL) {
		dt_dprintf("proc_per_sym: dt_alloc(%lu) failed", sz);
		return (1); /* errno is set for us */
	}

	ftp->ftps_pid = pid;
	ftp->ftps_provider_type = pp->dpp_provider_type;
	(void) strncpy(ftp->ftps_func, func, sizeof (ftp->ftps_func));

	dt_pid_objname(ftp->ftps_mod, sizeof (ftp->ftps_mod), pp->dpp_lmid,
	    pp->dpp_obj);

        // This isn't an Apple specific change, but it isn't in the mainline code yet, so we need to preserve it.
        ftp->ftps_func[sizeof(ftp->ftps_func) - 1] = 0;
        ftp->ftps_mod[sizeof(ftp->ftps_mod) - 1] = 0;

	if (!isdash && gmatch("return", pp->dpp_name)) {
		if (dt_pid_create_return_probe(pp->dpp_pr, dtp, ftp, symp,
		    pp->dpp_stret) < 0) {
			return (dt_pid_error(dtp, pcb, dpr, ftp,
			    D_PROC_CREATEFAIL, "failed to create return probe "
			    "for '%s': %s", func,
			    dtrace_errmsg(dtp, dtrace_errno(dtp))));
		}

		nmatches++;
	}

	if (!isdash && gmatch("entry", pp->dpp_name)) {
		if (dt_pid_create_entry_probe(pp->dpp_pr, dtp, ftp, symp) < 0) {
			return (dt_pid_error(dtp, pcb, dpr, ftp,
			    D_PROC_CREATEFAIL, "failed to create entry probe "
			    "for '%s': %s", func,
			    dtrace_errmsg(dtp, dtrace_errno(dtp))));
		}

		nmatches++;			
	}
#if !defined(__arm__) && !defined(__arm64__)
	/*
	 * Not supported on arm. The calls to create the probes themselves are
	 * stubbed out by returning an error condition, which then causes an
	 * error here. Instead we just won't call the offset functions at all.
	 */

	glob = strisglob(pp->dpp_name);
	if (!glob && nmatches == 0) {
		off = strtoull(pp->dpp_name, &end, 16);
		if (*end != '\0') {
			return (dt_pid_error(dtp, pcb, dpr, ftp, D_PROC_NAME,
			    "'%s' is an invalid probe name", pp->dpp_name));
		}

		if (off >= symp->st_size) {
			return (dt_pid_error(dtp, pcb, dpr, ftp, D_PROC_OFF,
			    "offset 0x%llx outside of function '%s'",
			    (u_longlong_t)off, func));
		}

		err = dt_pid_create_offset_probe(pp->dpp_pr, pp->dpp_dtp, ftp,
		    symp, off);

		if (err == DT_PROC_ERR) {
			return (dt_pid_error(dtp, pcb, dpr, ftp,
			    D_PROC_CREATEFAIL, "failed to create probe at "
			    "'%s+0x%llx': %s", func, (u_longlong_t)off,
			    dtrace_errmsg(dtp, dtrace_errno(dtp))));
		}

		if (err == DT_PROC_ALIGN) {
			return (dt_pid_error(dtp, pcb, dpr, ftp, D_PROC_ALIGN,
			    "offset 0x%llx is not aligned on an instruction",
			    (u_longlong_t)off));
		}

		nmatches++;

	} else if (glob && !isdash) {
		if (dt_pid_create_glob_offset_probes(pp->dpp_pr,
		    pp->dpp_dtp, ftp, symp, pp->dpp_name) < 0) {
			return (dt_pid_error(dtp, pcb, dpr, ftp,
			    D_PROC_CREATEFAIL,
			    "failed to create offset probes in '%s': %s", func,
			    dtrace_errmsg(dtp, dtrace_errno(dtp))));
		}

		nmatches++;
	}
#endif /* !defined(__arm__) && !defined(__arm64__) */

	pp->dpp_nmatches += nmatches;

	dt_free(dtp, ftp);

	return (0);
}

static int
dt_pid_sym_filt(void *arg, const GElf_Sym *symp, const char *func)
{
	/*
	 * Due to 14310776, libobjc.A.dylib contains functions causing
	 * crashing in the app target during order file generation. Like
	 * for the functions above, we do not prevent uses to specify them
	 * explicitly.
	 */
	static char const* const blacklist[] = {
		"_a1a2_firsttramp",
		"_a1a2_nexttramp",
		"_a1a2_trampend",
		"_a1a2_tramphead",
		"_a2a3_firsttramp",
		"_a2a3_nexttramp",
		"_a2a3_trampend",
		"_a2a3_tramphead",
	};

	dt_pid_probe_t *pp = arg;

	if (symp->st_shndx == SHN_UNDEF)
		return (0);

	if (symp->st_size == 0) {
		dt_dprintf("st_size of %s is zero", func);
		return (0);
	}

	if (pp->dpp_last_taken == 0 ||
	    symp->st_value != pp->dpp_last.st_value ||
	    symp->st_size != pp->dpp_last.st_size) {
		/*
		 * Silently ignore the blacklisted functions.
		 */
		for (size_t i = 0; i < sizeof(blacklist) / sizeof(char const*); ++i) {
			if (strcmp(func, blacklist[i]) == 0) {
				return 0;
			}
		}

		if ((pp->dpp_last_taken = gmatch(func, pp->dpp_func)) != 0) {
			pp->dpp_last = *symp;
			return (dt_pid_per_sym(pp, symp, func));
		}
	}

	return (0);
}

static int
dt_pid_per_mod(void *arg, const prmap_t *pmp, const char *obj,
     dt_pr_t reason)
{
	dt_pid_probe_t *pp = arg;
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	GElf_Sym sym;
	int err;

	if (obj == NULL) {
		return (0);
	}
	(void) Plmid(pp->dpp_pr, pmp->pr_vaddr, &pp->dpp_lmid);

	if ((pp->dpp_obj = strrchr(obj, '/')) == NULL)
		pp->dpp_obj = obj;
	else
		pp->dpp_obj++;

	/*
	 * libobjc_trampoline.dylib contain trampoline functions that get
	 * remapped, which causes crashes when they are traced as dtrace
	 * cannot associate the pc it trapped to back to a probe it enabled.
	 */
	if (strcmp(obj, "libobjc-trampolines.dylib") == 0)
		return (0);

	/*
	 * We're not doing anything with structure returns yet. Needs investigation...
	 */
	pp->dpp_stret[0] = 0;
        pp->dpp_stret[1] = 0;
        pp->dpp_stret[2] = 0;
        pp->dpp_stret[3] = 0;

	/*
	 * If pp->dpp_func contains any globbing meta-characters, we need
	 * to iterate over the symbol table and compare each function name
	 * against the pattern.
	 */
	if (!strisglob(pp->dpp_func)) {
		/*
		 * If we fail to lookup the symbol, try interpreting the
		 * function as the special "-" function that indicates that the
		 * probe name should be interpreted as a absolute virtual
		 * address. If that fails and we were matching a specific
		 * function in a specific module, report the error, otherwise
		 * just fail silently in the hopes that some other object will
		 * contain the desired symbol.
		 */
		err = dt_libproc_funcs[reason].lookup_by_name(pp->dpp_pr,
			pp->dpp_lmid, obj, pp->dpp_func, &sym, NULL);
		if (err != 0) {
			if (strcmp("-", pp->dpp_func) == 0) {
				sym.st_name = 0;
				sym.st_info =
				    GELF_ST_INFO(STB_LOCAL, STT_FUNC);
				sym.st_other = 0;
				sym.st_value = 0;
				sym.st_size = Pstatus(pp->dpp_pr)->pr_dmodel ==
				    PR_MODEL_ILP32 ? -1U : -1ULL;

				// This works by accident on Solaris. The call to Pxlookup_by_name always fills in at least
				// one entry to sym, which is almost always !SHN_UNDEF. This means the test below does not fail,
				// most of the time.
				sym.st_shndx = SHN_MACHO;
			} else if (!strisglob(pp->dpp_mod) && err == PLOOKUP_NOT_FOUND) {
				return (dt_pid_error(dtp, pcb, dpr, NULL,
				    D_PROC_FUNC,
				    "failed to lookup '%s' in module '%s'",
				    pp->dpp_func, pp->dpp_mod));
			} else {
				return (0);
			}
		}

		/*
		 * Only match defined functions of non-zero size.
		 */
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC ||
		    sym.st_shndx == SHN_UNDEF || sym.st_size == 0)
			return (0);

		/*
		 * We don't instrument PLTs -- they're dynamically rewritten,
		 * and, so, inherently dicey to instrument.
		 */
		if (Ppltdest(pp->dpp_pr, sym.st_value) != NULL)
			return (0);

		(void) Plookup_by_addr(pp->dpp_pr, sym.st_value, pp->dpp_func,
		    DTRACE_FUNCNAMELEN, &sym);

		return (dt_pid_per_sym(pp, &sym, pp->dpp_func));
	} else {
		uint_t nmatches = pp->dpp_nmatches;

		if (dt_libproc_funcs[reason].symbol_iter_by_addr(pp->dpp_pr, 
		    obj, PR_SYMTAB, BIND_ANY | TYPE_FUNC, dt_pid_sym_filt, pp)) {
			return (1);
		}

		if (nmatches == pp->dpp_nmatches) {
			/*
			 * If we didn't match anything in the PR_SYMTAB, try
			 * the PR_DYNSYM.
			 */
			if (dt_libproc_funcs[reason].symbol_iter_by_addr(
			    pp->dpp_pr, obj, PR_DYNSYM,
			    BIND_ANY | TYPE_FUNC, dt_pid_sym_filt, pp)) {
				return (1);
			}
		}
	}

	return (0);
}

static int
dt_pid_mod_filt(void *arg, const prmap_t *pmp, const char *obj,
    dt_pr_t reason)
{
	char name[DTRACE_MODNAMELEN];
	dt_pid_probe_t *pp = arg;

	if (gmatch(obj, pp->dpp_mod))
		return (dt_pid_per_mod(pp, pmp, obj, reason));

	(void) Plmid(pp->dpp_pr, pmp->pr_vaddr, &pp->dpp_lmid);

	if ((pp->dpp_obj = strrchr(obj, '/')) == NULL)
		pp->dpp_obj = obj;
	else
		pp->dpp_obj++;

	dt_pid_objname(name, sizeof (name), pp->dpp_lmid, obj);

	if (gmatch(name, pp->dpp_mod))
		return (dt_pid_per_mod(pp, pmp, obj, reason));

	return (0);
}

static int
dt_pid_mod_filt_link(void *arg, const prmap_t *pmp, const char *obj)
{
	return dt_pid_mod_filt(arg, pmp, obj, DT_PR_LINK);
}

static int
dt_pid_mod_filt_create(void *arg, const prmap_t *pmp, const char *obj)
{
	return dt_pid_mod_filt(arg, pmp, obj, DT_PR_CREATE);
}

static int (*dt_pid_mod_filt_fns[])(void *arg, const prmap_t *pmp, const char *obj) = {
	dt_pid_mod_filt_create,	/* DT_PR_CREATE */
	dt_pid_mod_filt_link	/* DT_PR_LINK */
};

static const prmap_t *
dt_pid_fix_mod(dtrace_probedesc_t *pdp, struct ps_prochandle *P, prmap_t* thread_local_map)
{
	char m[MAXPATHLEN];
	Lmid_t lmid = PR_LMID_EVERY;
	const char *obj;
	const prmap_t *pmp;
	
	/*
	 * Pick apart the link map from the library name.
	 */
	if (strchr(pdp->dtpd_mod, '`') != NULL) {
		char *end;

		if (strncmp(pdp->dtpd_mod, "LM", 2) != 0 ||
		    !isdigit(pdp->dtpd_mod[2]))
			return (NULL);

		lmid = strtoul(&pdp->dtpd_mod[2], &end, 16);

		obj = end + 1;

		if (*end != '`' || strchr(obj, '`') != NULL)
			return (NULL);

	} else {
		obj = pdp->dtpd_mod;
	}

	if ((pmp = Plmid_to_map(P, lmid, obj, thread_local_map)) == NULL)
		return (NULL);
	
	(void) Pobjname(P, pmp->pr_vaddr, m, sizeof (m));
	if ((obj = strrchr(m, '/')) == NULL)
		obj = &m[0];
	else
		obj++;

	(void) Plmid(P, pmp->pr_vaddr, &lmid);
	dt_pid_objname(pdp->dtpd_mod, sizeof (pdp->dtpd_mod), lmid, obj);

	return (pmp);
}


static int
dt_pid_create_provider_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr, fasttrap_provider_type_t provider,
    dt_pr_t reason)
{
	dt_pid_probe_t pp;
	int ret = 0;
	
	pp.dpp_dtp = dtp;
	pp.dpp_dpr = dpr;
	pp.dpp_pr = dpr->dpr_proc;
	pp.dpp_pcb = pcb;

	/*
	 * We can only trace dynamically-linked executables (since we've
	 * hidden some magic in dyld)
	 */
	prmap_t thread_local_map;
	if (Pname_to_map(pp.dpp_pr, PR_OBJ_LDSO, &thread_local_map) == NULL) {
		return (dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_DYN,
				     "process %s has no dyld, and cannot be instrumented",
				     &pdp->dtpd_provider[3]));
	}

	pp.dpp_provider_type = provider;
	pp.dpp_mod = pdp->dtpd_mod[0] != '\0' ? pdp->dtpd_mod : "*";
	pp.dpp_func = pdp->dtpd_func[0] != '\0' ? pdp->dtpd_func : "*";
	pp.dpp_name = pdp->dtpd_name[0] != '\0' ? pdp->dtpd_name : "*";
	pp.dpp_last_taken = 0;

	if (strcmp(pp.dpp_func, "-") == 0) {
		const prmap_t *aout, *pmp;
		prmap_t aout_thread_local_map;
		prmap_t pmp_thread_local_map;

		if (pdp->dtpd_mod[0] == '\0') {
			pp.dpp_mod = pdp->dtpd_mod;
			(void) strcpy(pdp->dtpd_mod, "a.out");
		} else if (strisglob(pp.dpp_mod) ||
		    (aout = Pname_to_map(pp.dpp_pr, "a.out", &aout_thread_local_map)) == NULL ||
		    (pmp = Pname_to_map(pp.dpp_pr, pp.dpp_mod, &pmp_thread_local_map)) == NULL ||
		    aout->pr_vaddr != pmp->pr_vaddr) {
			return (dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_LIB,
			    "only the a.out module is valid with the "
			    "'-' function"));
		}

		if (strisglob(pp.dpp_name)) {
			return (dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_NAME,
			    "only individual addresses may be specified "
			    "with the '-' function"));
		}
	}

	/*
	 * If pp.dpp_mod contains any globbing meta-characters, we need
	 * to iterate over each module and compare its name against the
	 * pattern. An empty module name is treated as '*'.
	 */
	if (strisglob(pp.dpp_mod)) {
		ret = dt_libproc_funcs[reason].object_iter(pp.dpp_pr,
			dt_pid_mod_filt_fns[reason], &pp);
	} else {
		const prmap_t *pmp;
		char *obj;

		/*
		 * If we can't find a matching module, don't sweat it -- either
		 * we'll fail the enabling because the probes don't exist or
		 * we'll wait for that module to come along.
		 */
		if ((pmp = dt_pid_fix_mod(pdp, pp.dpp_pr, &thread_local_map)) != NULL) {
			if ((obj = strchr(pdp->dtpd_mod, '`')) == NULL)
				obj = pdp->dtpd_mod;
			else
				obj++;

			ret = dt_pid_per_mod(&pp, pmp, obj, reason);
		}
	}

	return (ret);
}

static int
dt_pid_create_pid_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr, dt_pr_t reason)
{
	return dt_pid_create_provider_probes(pdp, dtp, pcb, dpr,
		DTFTP_PROVIDER_PID, reason);
}

static int
dt_pid_create_oneshot_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr, dt_pr_t reason)
{
	return dt_pid_create_provider_probes(pdp, dtp, pcb, dpr,
		DTFTP_PROVIDER_ONESHOT, reason);
}

#if !defined(__APPLE__)
static int
dt_pid_usdt_mapping(void *data, const prmap_t *pmp, const char *oname)
{
	struct ps_prochandle *P = data;
	GElf_Sym sym;
	prsyminfo_t sip;
	dof_helper_t dh;
	GElf_Half e_type;
	const char *mname;
	const char *syms[] = { "___SUNW_dof", "__SUNW_dof" };
	int i, fd = -1;

	/*
	 * The symbol ___SUNW_dof is for lazy-loaded DOF sections, and
	 * __SUNW_dof is for actively-loaded DOF sections. We try to force
	 * in both types of DOF section since the process may not yet have
	 * run the code to instantiate these providers.
	 */
	for (i = 0; i < 2; i++) {
		if (Pxlookup_by_name(P, PR_LMID_EVERY, oname, syms[i], &sym,
		    &sip) != 0) {
			continue;
		}

		if ((mname = strrchr(oname, '/')) == NULL)
			mname = oname;
		else
			mname++;

		dt_dprintf("lookup of %s succeeded for %s", syms[i], mname);

		if (Pread(P, &e_type, sizeof (e_type), pmp->pr_vaddr +
		    offsetof(Elf64_Ehdr, e_type)) != sizeof (e_type)) {
			dt_dprintf("read of ELF header failed");
			continue;
		}

		dh.dofhp_dof = sym.st_value;
		dh.dofhp_addr = (e_type == ET_EXEC) ? 0 : pmp->pr_vaddr;

		dt_pid_objname(dh.dofhp_mod, sizeof (dh.dofhp_mod),
		    sip.prs_lmid, mname);

		if (fd == -1 &&
		    (fd = pr_open(P, "/dev/dtrace/helper", O_RDWR, 0)) < 0) {
			dt_dprintf("pr_open of helper device failed: %s",
			    strerror(errno));
			return (-1); /* errno is set for us */
		}

		if (pr_ioctl(P, fd, DTRACEHIOC_ADDDOF, &dh, sizeof (dh)) < 0)
			dt_dprintf("DOF was rejected for %s", dh.dofhp_mod);
	}

	if (fd != -1)
		(void) pr_close(P, fd);

	return (0);
}

static int
dt_pid_create_usdt_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr)
{
	struct ps_prochandle *P = dpr->dpr_proc;
	int ret = 0;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	(void) Pupdate_maps(P);
	if (Pobject_iter(P, dt_pid_usdt_mapping, P) != 0) {
		ret = -1;
		(void) dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_USDT,
		    "failed to instantiate probes for pid %d: %s",
		    (int)Pstatus(P)->pr_pid, strerror(errno));
	}

	/*
	 * Put the module name in its canonical form.
	 */
	prmap_t thread_local_map;
	(void) dt_pid_fix_mod(pdp, P, &thread_local_map);

	return (ret);
}
#endif

static pid_t
dt_pid_get_pid(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb,
    dt_proc_t *dpr)
{
	pid_t pid;
	char *c, *last = NULL, *end;

	for (c = &pdp->dtpd_provider[0]; *c != '\0'; c++) {
		if (!isdigit(*c))
			last = c;
	}

	if (last == NULL || (*(++last) == '\0')) {
		return (-1);
	}

	errno = 0;
	pid = strtol(last, &end, 10);

	if (errno != 0 || end == last || end[0] != '\0' || pid <= 0) {
		(void) dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_BADPID,
		    "'%s' does not contain a valid pid", pdp->dtpd_provider);
		return (-1);
	}

	return (pid);
}

int
dt_pid_create_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb,
    dt_pr_t reason)
{
	char provname[DTRACE_PROVNAMELEN];
	struct ps_prochandle *P;
	dt_proc_t *dpr;
	pid_t pid;
	int err = 0;
	
	assert(pcb != NULL);

	if ((pid = dt_pid_get_pid(pdp, dtp, pcb, NULL)) == -1)
		return (-1);

	if (dtp->dt_ftfd == -1) {
		if (dtp->dt_fterr == ENOENT) {
			(void) dt_pid_error(dtp, pcb, NULL, NULL, D_PROC_NODEV,
			    "pid provider is not installed on this system");
		} else {
			(void) dt_pid_error(dtp, pcb, NULL, NULL, D_PROC_NODEV,
			    "pid provider is not available: %s",
			    strerror(dtp->dt_fterr));
		}

		return (-1);
	}

	fasttrap_provider_type_t provider_type = DTFTP_PROVIDER_NONE;
	
	(void) snprintf(provname, sizeof (provname), "%s%d", FASTTRAP_PID_NAME, (int)pid);
	
	if (gmatch(provname, pdp->dtpd_provider) != 0) {
		provider_type = DTFTP_PROVIDER_PID;
	} else {
		(void) snprintf(provname, sizeof (provname), "%s%d", FASTTRAP_OBJC_NAME, (int)pid);
		if (gmatch(provname, pdp->dtpd_provider) != 0) {
			provider_type = DTFTP_PROVIDER_OBJC;
		} else {
			(void) snprintf(provname, sizeof (provname), "%s%d", FASTTRAP_ONESHOT_NAME, (int)pid);
			if (gmatch(provname, pdp->dtpd_provider) != 0) {
				provider_type = DTFTP_PROVIDER_ONESHOT;
			}
		}
	}
	
	if (provider_type != DTFTP_PROVIDER_NONE) {
		if ((P = dt_proc_grab(dtp, pid, 0,
				      0)) == NULL) {
			return (-1);
		}
		
		dpr = dt_proc_lookup(dtp, P, 0);
		assert(dpr != NULL);
		(void) pthread_mutex_lock(&dpr->dpr_lock);
		
		switch (provider_type) {
		        case DTFTP_PROVIDER_PID:
			        err = dt_pid_create_pid_probes(pdp, dtp, pcb, dpr, reason);
			        break;
		        case DTFTP_PROVIDER_OBJC:
			        err = dt_pid_create_objc_probes(pdp, dtp, pcb, dpr, reason);
			        break;
		        case DTFTP_PROVIDER_ONESHOT:
			        err = dt_pid_create_oneshot_probes(pdp, dtp, pcb, dpr, reason);
			        break;
		        default:
			        err = -1; 
		}
		
		if (err == 0) {
			/*
			 * Alert other retained enablings which may match
			 * against the newly created probes.
			 */
			(void) dt_ioctl(dtp, DTRACEIOC_ENABLE, NULL);
		}
		
		(void) pthread_mutex_unlock(&dpr->dpr_lock);
		dt_proc_release(dtp, P);
	} 

	(void) snprintf(provname, sizeof (provname), "pid%d", (int)pid);

	/*
	 * APPLE NOTE: Our "lazy DOF" is handled in the kernel.
	 * There is no need to poke around processes and look for DOF to load.
	 */

	return (err ? -1 : 0);
}

int
dt_pid_create_probes_module(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	dtrace_prog_t *pgp;
	dt_stmt_t *stp;
	dtrace_probedesc_t *pdp, pd;
	pid_t pid;
	int ret = 0, found = B_FALSE;
	char provname[DTRACE_PROVNAMELEN];

	for (pgp = dt_list_next(&dtp->dt_programs); pgp != NULL;
	     pgp = dt_list_next(pgp)) {
		
		for (stp = dt_list_next(&pgp->dp_stmts); stp != NULL;
		     stp = dt_list_next(stp)) {
			
			pdp = &stp->ds_desc->dtsd_ecbdesc->dted_probe;
			pid = dt_pid_get_pid(pdp, dtp, NULL, dpr);
			if (pid != dpr->dpr_pid)
				continue;
			
			found = B_TRUE;
			
			pd = *pdp;

			if (snprintf(provname, sizeof (provname), FASTTRAP_PID_NAME "%d", (int)dpr->dpr_pid) &&
			    gmatch(pdp->dtpd_provider, provname) != 0) {
				if (dt_pid_create_pid_probes(&pd, dtp, NULL, dpr, DT_PR_LINK) != 0)
					ret = 1;
			} else if (snprintf(provname, sizeof (provname), FASTTRAP_OBJC_NAME "%d", (int)dpr->dpr_pid) &&
				   gmatch(pdp->dtpd_provider, provname) != 0) {
				if (dt_pid_create_objc_probes(&pd, dtp, NULL, dpr, DT_PR_LINK) != 0)
					ret = 1;
			} else if (snprintf(provname, sizeof (provname), FASTTRAP_ONESHOT_NAME "%d", (int)dpr->dpr_pid) &&
				   gmatch(pdp->dtpd_provider, provname) != 0) {
				if (dt_pid_create_oneshot_probes(&pd, dtp, NULL, dpr, DT_PR_LINK) != 0)
					ret = 1;
			}  else {
				// APPLE NOTE!
				//
				// We do not have the same type of lazy dof as sun, there is no point in checking for it.
				// Leaving the comment in place as a placeholder.
				
				/*
				 * If it's not strictly a pid provider, we might match
				 * a USDT provider.
				 */
			}
		}
	}
	
	if (found) {
		/*
		 * Give DTrace a shot to the ribs to get it to check
		 * out the newly created probes.
		 */
		(void) dt_ioctl(dtp, DTRACEIOC_ENABLE, NULL);
	}
	
	return (ret);
}

