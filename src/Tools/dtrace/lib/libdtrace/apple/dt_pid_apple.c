/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
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

// This must be done *after* any references to Foundation.h!
#define uint_t  __Solaris_uint_t

#include <string.h>

#include <sys/dtrace.h>
#include <dtrace.h>

#include <dt_proc.h>
#include <dt_pid.h>
#include <dt_string.h>

/*
 * OBJC provider methods
 */

static int dt_pid_objc_filt(void *arg, const GElf_Sym *sym, const char *class_name, const char *method_name)
{
	dt_pid_probe_t *pp = arg;

        // Handle the class name first.
	if ((strisglob(pp->dpp_mod) && gmatch(class_name, pp->dpp_mod)) || (strcmp(class_name, pp->dpp_mod) == 0)) {
                // Now check the method name.
                if ((strisglob(pp->dpp_func) && gmatch(method_name, pp->dpp_func)) || (strcmp(method_name, pp->dpp_func) == 0)) {
                        pp->dpp_obj = class_name;
                        // At this point, we can cheat and use the normal pid probe code.
                        return dt_pid_per_sym(pp, sym, method_name);
                }
        }

	return (0);
}

int dt_pid_create_objc_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr, dt_pr_t reason)
{
	dt_pid_probe_t pp;
	int ret = 0;

	/*
	 * Disable breakpoints so they don't interfere with our disassembly.
	 */
	dt_proc_bpdisable(dpr);

	pp.dpp_dtp = dtp;
	pp.dpp_dpr = dpr;
	pp.dpp_pr = dpr->dpr_proc;
	pp.dpp_pcb = pcb;

	/*
	 * We can only trace dynamically-linked executables (since we've
	 * hidden some magic in dyld).
	 */
	prmap_t thread_local_map;
	if (Pname_to_map(pp.dpp_pr, PR_OBJ_LDSO, &thread_local_map) == NULL) {
		return (dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_DYN,
				     "process %s has no dyld, and cannot be instrumented",
				     &pdp->dtpd_provider[3]));
	}

	pp.dpp_provider_type = DTFTP_PROVIDER_OBJC;
	pp.dpp_mod = pdp->dtpd_mod[0] != '\0' ? pdp->dtpd_mod : "*";
	pp.dpp_func = pdp->dtpd_func[0] != '\0' ? pdp->dtpd_func : "*";
	pp.dpp_name = pdp->dtpd_name[0] != '\0' ? pdp->dtpd_name : "*";
	pp.dpp_last_taken = 0;

        /*
         * We set up some default values that normally change per module.
         */
        pp.dpp_lmid = LM_ID_BASE;
        pp.dpp_stret[0] = 0;
        pp.dpp_stret[1] = 0;
        pp.dpp_stret[2] = 0;
        pp.dpp_stret[3] = 0;

        /*
         * We're working in the objc namespace, symbols are in "library, function" style.
         * We have to look at every symbol in every owner, even without globbing. 
         */
	ret = dt_libproc_funcs[reason].objc_method_iter(pp.dpp_pr,
		(proc_objc_f*)dt_pid_objc_filt, &pp);

	dt_proc_bpenable(dpr);

	return (ret);
}

