/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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

#ifndef	_DT_PID_H
#define	_DT_PID_H

#include <libproc.h>
#include <sys/fasttrap.h>

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	DT_PR_CREATE = 0,
	DT_PR_LINK = 1,
	DT_PR_MAX = 2
} dt_pr_t;

struct dt_libproc_fn {
	int (*lookup_by_name)(struct ps_prochandle *, Lmid_t, const char *,
		const char *, GElf_Sym *, prsyminfo_t *);
	int (*object_iter)(struct ps_prochandle *, proc_map_f *, void *);
	int (*objc_method_iter)(struct ps_prochandle *, proc_objc_f* , void *);
	int (*symbol_iter_by_addr)(struct ps_prochandle *, const char *, int,
		int, proc_sym_f *, void *);
};

typedef struct dt_pid_probe {
	dtrace_hdl_t *dpp_dtp;
	dt_pcb_t *dpp_pcb;
	dt_proc_t *dpp_dpr;
	struct ps_prochandle *dpp_pr;
	fasttrap_provider_type_t dpp_provider_type;
	const char *dpp_mod;
	char *dpp_func;
	const char *dpp_name;
	const char *dpp_obj;
	uintptr_t dpp_pc;
	size_t dpp_size;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
	uint64_t dpp_stret[4];
	GElf_Sym dpp_last;
	uint_t dpp_last_taken;
} dt_pid_probe_t;

extern struct dt_libproc_fn dt_libproc_funcs[DT_PR_MAX];

#define	DT_PROC_ERR	(-1)
#define	DT_PROC_ALIGN	(-2)

extern int dt_pid_create_probes(dtrace_probedesc_t *, dtrace_hdl_t *,
    dt_pcb_t *pcb, dt_pr_t);
extern int dt_pid_create_probes_module(dtrace_hdl_t *, dt_proc_t *);

extern int dt_pid_create_entry_probe(struct ps_prochandle *, dtrace_hdl_t *,
    fasttrap_probe_spec_t *, const GElf_Sym *);

extern int dt_pid_create_return_probe(struct ps_prochandle *, dtrace_hdl_t *,
    fasttrap_probe_spec_t *, const GElf_Sym *, uint64_t *);

extern int dt_pid_create_offset_probe(struct ps_prochandle *, dtrace_hdl_t *,
    fasttrap_probe_spec_t *, const GElf_Sym *, ulong_t);

extern int dt_pid_create_glob_offset_probes(struct ps_prochandle *,
    dtrace_hdl_t *, fasttrap_probe_spec_t *, const GElf_Sym *, const char *);

extern int dt_pid_create_objc_probes(dtrace_probedesc_t *, dtrace_hdl_t *,
    dt_pcb_t *, dt_proc_t *, dt_pr_t);

extern int dt_pid_per_sym(dt_pid_probe_t *, const GElf_Sym *, const char *);

extern int dt_pid_error(dtrace_hdl_t *, dt_pcb_t *, dt_proc_t *,
    fasttrap_probe_spec_t *, dt_errtag_t, const char *, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PID_H */
