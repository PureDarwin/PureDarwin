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

/*
 * Keep this file in sync with dt_proc.c ...
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "dt_impl.h"
#include <dt_proc.h>
#include <dt_pid.h>

extern psaddr_t rd_event_mock_addr(struct ps_prochandle *); // See libproc.m

static void
dt_proc_bpmatch(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	dt_bkpt_t *dbp;
	
	assert(DT_MUTEX_HELD(&dpr->dpr_lock));
	
	for (dbp = dt_list_next(&dpr->dpr_bps);
	     dbp != NULL; dbp = dt_list_next(dbp)) {
		if (rd_event_mock_addr(dpr->dpr_proc) == dbp->dbp_addr)
			break;
	}
	
	if (dbp == NULL) {
		dt_dprintf("pid %d: spurious breakpoint wakeup for %lx",
			   (int)dpr->dpr_pid, rd_event_mock_addr(dpr->dpr_proc));
		return;
	}
	
	dt_dprintf("pid %d: hit breakpoint at %lx (%lu)",
		   (int)dpr->dpr_pid, (ulong_t)dbp->dbp_addr, ++dbp->dbp_hits);
	
	dbp->dbp_func(dtp, dpr, dbp->dbp_data);
}

/*
 * Common code for enabling events associated with the run-time linker after
 * attaching to a process or after a victim process completes an exec(2).
 */
static void
dt_proc_attach(dt_proc_t *dpr, int exec)
{
#pragma unused(exec)
	rd_err_e err;
	
	/* exec == B_FALSE coming from initial call in dt_proc_control()
	 exec == B_TRUE arises when the target does an exec() */
	
	if ((dpr->dpr_rtld = Prd_agent(dpr->dpr_proc)) != NULL &&
	    (err = rd_event_enable(dpr->dpr_rtld, B_TRUE)) == RD_OK) {
		dt_proc_rdwatch(dpr, RD_PREINIT, "RD_PREINIT");
		dt_proc_rdwatch(dpr, RD_POSTINIT, "RD_POSTINIT");
		dt_proc_rdwatch(dpr, RD_DLACTIVITY, "RD_DLACTIVITY");
	} else {
		dt_dprintf("pid %d: failed to enable rtld events: %s",
			   (int)dpr->dpr_pid, dpr->dpr_rtld ? rd_errstr(err) :
			   "rtld_db agent initialization failed");
	}
	
	Pupdate_maps(dpr->dpr_proc);
}

typedef struct dt_proc_control_data {
	dtrace_hdl_t *dpcd_hdl;			/* DTrace handle */
	dt_proc_t *dpcd_proc;			/* proccess to control */
} dt_proc_control_data_t;

/*
 * Main loop for all victim process control threads.  We initialize all the
 * appropriate /proc control mechanisms, and then enter a loop waiting for
 * the process to stop on an event or die.  We process any events by calling
 * appropriate subroutines, and exit when the victim dies or we lose control.
 *
 * The control thread synchronizes the use of dpr_proc with other libdtrace
 * threads using dpr_lock.  We hold the lock for all of our operations except
 * waiting while the process is running: this is accomplished by writing a
 * PCWSTOP directive directly to the underlying /proc/<pid>/ctl file.  If the
 * libdtrace client wishes to exit or abort our wait, SIGCANCEL can be used.
 */
void *
dt_proc_control(void *arg)
{
	dt_proc_control_data_t *datap = arg;
	dtrace_hdl_t *dtp = datap->dpcd_hdl;
	dt_proc_t *dpr = datap->dpcd_proc;
	dt_proc_hash_t *dph = dtp->dt_procs;
	struct ps_prochandle *P = dpr->dpr_proc;
	
	int pid = dpr->dpr_pid;
	
	int notify = B_FALSE;
	
	/*
	 * We disable the POSIX thread cancellation mechanism so that the
	 * client program using libdtrace can't accidentally cancel our thread.
	 * dt_proc_destroy() uses SIGCANCEL explicitly to simply poke us out
	 * of PCWSTOP with EINTR, at which point we will see dpr_quit and exit.
	 */
	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	
	/*
	 * Set up the corresponding process for tracing by libdtrace.  We want
	 * to be able to catch breakpoints and efficiently single-step over
	 * them, and we need to enable librtld_db to watch libdl activity.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);
	
	(void) Punsetflags(P, PR_ASYNC);	/* require synchronous mode */
	(void) Psetflags(P, PR_BPTADJ);		/* always adjust eip on x86 */
	(void) Punsetflags(P, PR_FORK);		/* do not inherit on fork */
        
	dt_proc_attach(dpr, B_FALSE);		/* enable rtld breakpoints */
	
	/*
	 * If PR_KLC is set, we created the process; otherwise we grabbed it.
	 * Check for an appropriate stop request and wait for dt_proc_continue.
	 */
	if (Pstatus(P)->pr_flags & PR_KLC)
		dt_proc_stop(dpr, DT_PROC_STOP_CREATE);
	else
		dt_proc_stop(dpr, DT_PROC_STOP_GRAB);

	if (Psetrun(P, 0, 0) == -1) {
		dt_dprintf("pid %d: failed to set running: %s",
			   (int)dpr->dpr_pid, strerror(errno));
	}
		
	(void) pthread_mutex_unlock(&dpr->dpr_lock);
	
	/*
	 * Wait for the process corresponding to this control thread to stop,
	 * process the event, and then set it running again.  We want to sleep
	 * with dpr_lock *unheld* so that other parts of libdtrace can use the
	 * ps_prochandle in the meantime (e.g. ustack()).  To do this, we write
	 * a PCWSTOP directive directly to the underlying /proc/<pid>/ctl file.
	 * Once the process stops, we wake up, grab dpr_lock, and then call
	 * Pwait() (which will return immediately) and do our processing.
	 */
	while (!dpr->dpr_quit) {
		void *activity = Pdequeue_proc_activity(P);
		
		// A NULL activity is used to wakeup the control thread, and force it to check dpr_quit.
		if (!activity)
			continue;

		(void) pthread_mutex_lock(&dpr->dpr_lock);
			
		switch (Pstate(P)) {
			case PS_STOP:		
				dt_dprintf("pid %d: proc stopped", pid);
				dt_proc_bpmatch(dtp, dpr);
				break;
				
			case PS_RUN:
				/*
				 * On Mac OS X Pstate() maps SSLEEP and SRUN to PS_RUN, and both
				 * of those states are beholden to DTrace.
				 */
				dt_proc_bpmatch(dtp, dpr);
				break;
				
			case PS_LOST:
				dt_dprintf("pid %d: proc lost", pid);
				dpr->dpr_quit = B_TRUE;
				notify = B_TRUE;
				break;
				
			case PS_DEAD:
			case PS_UNDEAD:
				dt_dprintf("pid %d: proc died", pid);
				dpr->dpr_quit = B_TRUE;
				notify = B_TRUE;
				break;
				
			default:
				assert(false);
				dt_dprintf("pid %d: proc in unrecognized state, resuming", pid);
				break;		
				
		}
		
		(void) pthread_mutex_unlock(&dpr->dpr_lock);
		
		Pdestroy_proc_activity(activity);
	}
	
	/*
	 * If the control thread detected PS_UNDEAD or PS_LOST, then enqueue
	 * the dt_proc_t structure on the dt_proc_hash_t notification list.
	 */
	if (notify)
		dt_proc_notify(dtp, dph, dpr, NULL);
	
	/*
	 * Destroy and remove any remaining breakpoints, set dpr_done and clear
	 * dpr_tid to indicate the control thread has exited, and notify any
	 * waiting thread in dt_proc_destroy() that we have succesfully exited.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);
	
	dt_proc_bpdestroy(dpr, B_TRUE);
	dpr->dpr_done = B_TRUE;
	dpr->dpr_tid = 0;
	
	(void) pthread_cond_broadcast(&dpr->dpr_cv);
	(void) pthread_mutex_unlock(&dpr->dpr_lock);
	
	return (NULL);
}
