/*-
 * Copyright (c) 2017 Hans Petter Selasky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// linux_current.c for PureDarwin: one task per xnu thread, made on first use
// of current, with one mm shared by every task since there is no process

#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <vm/uma.h>

static uma_zone_t linux_current_zone;
static struct mm_struct linux_kernel_mm;

int
linux_alloc_current(struct thread *td, int flags)
{
	struct task_struct *ts;

	MPASS(td->td_lkpi_task == NULL);

	ts = uma_zalloc(linux_current_zone, flags | M_ZERO);
	if (ts == NULL)
		return (ENOMEM);

	atomic_set(&ts->kthread_flags, 0);
	ts->task_thread = td;
	ts->comm = td->td_name;
	ts->pid = td->td_tid;
	ts->group_leader = ts;
	atomic_set(&ts->usage, 1);
	atomic_set(&ts->state, TASK_RUNNING);
	init_completion(&ts->parked);
	init_completion(&ts->exited);

	atomic_inc(&linux_kernel_mm.mm_users);
	ts->mm = &linux_kernel_mm;

	td->td_lkpi_task = ts;
	return (0);
}

struct mm_struct *
linux_get_task_mm(struct task_struct *task)
{
	struct mm_struct *mm;

	mm = task->mm;
	if (mm != NULL) {
		atomic_inc(&mm->mm_users);
		return (mm);
	}
	return (NULL);
}

void
linux_mm_dtor(struct mm_struct *mm)
{
	// the shared kernel mm is never freed
}

void
linux_free_current(struct task_struct *ts)
{
	mmput(ts->mm);
	uma_zfree(linux_current_zone, ts);
}

// called when an xnu thread with a struct thread exits
void
linux_free_current_pd(struct thread *td)
{
	struct task_struct *ts;

	ts = td->td_lkpi_task;
	td->td_lkpi_task = NULL;
	ts->task_thread = NULL;
	put_task_struct(ts);
}

// pids name tasks only for debugging here, lookups find nothing
struct task_struct *
linux_pid_task(pid_t pid)
{
	return (NULL);
}

struct task_struct *
linux_get_pid_task(pid_t pid)
{
	return (NULL);
}

bool
linux_task_exiting(struct task_struct *task)
{
	return (false);
}

static void
linux_current_init(void *arg __unused)
{
	linux_current_zone = uma_zcreate("lkpicurr",
	    sizeof(struct task_struct), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	init_rwsem(&linux_kernel_mm.mmap_sem);
	atomic_set(&linux_kernel_mm.mm_count, 1);
	atomic_set(&linux_kernel_mm.mm_users, 1);

	atomic_thread_fence_seq_cst();
	lkpi_alloc_current = linux_alloc_current;
}
SYSINIT(linux_current, SI_SUB_EVENTHANDLER, SI_ORDER_LAST, linux_current_init,
    NULL);

static void
linux_current_uninit(void *arg __unused)
{
	lkpi_alloc_current = linux_alloc_current_noop;
	atomic_thread_fence_seq_cst();

	// tasks still attached to live threads keep their zone memory
	uma_zdestroy(linux_current_zone);
}
SYSUNINIT(linux_current, SI_SUB_EVENTHANDLER, SI_ORDER_LAST,
    linux_current_uninit, NULL);
