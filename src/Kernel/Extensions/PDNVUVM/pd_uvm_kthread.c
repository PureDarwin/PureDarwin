// pd_uvm_kthread.c - Linux's kthread_create for nvidia-uvm: the thread is made, then waits for wake_up_process
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// linuxkpi's kthread_run takes the thread kthread_add makes as a FreeBSD struct thread, which PD's kthread_add (from
// Haiku) does not give: it gives a thread id. Here the new thread takes its own linux task on PD's curthread, hands it
// back, and waits to be started.
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/compat.h>
#include <sys/kthread.h>

// a thread kthread_create made, until it is started
struct pd_uvm_kthread {
    struct list_head link;
    struct task_struct *task;
    int (*fn)(void *);
    void *data;
    struct completion ready;    // the thread has its task
    struct completion go;       // wake_up_process has started it
};
static LIST_HEAD(pd_uvm_kthreads);
static DEFINE_SPINLOCK(pd_uvm_kthreads_lock);

static void
pd_uvm_kthread_main(void *arg)
{
    struct pd_uvm_kthread *k = arg;
    struct task_struct *task;
    int (*fn)(void *);
    void *data;

    linux_set_current(curthread);
    task = current;
    task->flags |= PF_KTHREAD;
    // the stopper's reference: kthread_stop puts one, this thread's exit puts the other
    get_task_struct(task);
    k->task = task;
    complete(&k->ready);

    // started by wake_up_process, or stopped before it ever ran: linuxkpi's kthread_stop wakes with its own
    // wake_up_process, which does not complete go, so the stop flag is looked at too
    while (!try_wait_for_completion(&k->go) && !kthread_should_stop())
        schedule_timeout_uninterruptible(HZ / 50 + 1);

    fn = k->fn;
    data = k->data;
    spin_lock(&pd_uvm_kthreads_lock);
    if (!list_empty(&k->link))
        list_del_init(&k->link);
    spin_unlock(&pd_uvm_kthreads_lock);
    kfree(k);

    task->task_ret = kthread_should_stop() ? -EINTR : fn(data);
    complete(&task->exited);
    kthread_exit();
}

struct task_struct *
pd_uvm_kthread_create(int (*fn)(void *), void *data, const char *fmt, ...)
{
    struct pd_uvm_kthread *k = kzalloc(sizeof(*k), GFP_KERNEL);
    struct thread *td = NULL;
    char name[MAXCOMLEN + 1];
    va_list ap;

    if (!k)
        return ERR_PTR(-ENOMEM);
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    k->fn = fn;
    k->data = data;
    INIT_LIST_HEAD(&k->link);
    init_completion(&k->ready);
    init_completion(&k->go);

    spin_lock(&pd_uvm_kthreads_lock);
    list_add(&k->link, &pd_uvm_kthreads);
    spin_unlock(&pd_uvm_kthreads_lock);
    if (kthread_add(pd_uvm_kthread_main, k, NULL, &td, RFSTOPPED, 0, "%s", name) != 0) {
        spin_lock(&pd_uvm_kthreads_lock);
        list_del(&k->link);
        spin_unlock(&pd_uvm_kthreads_lock);
        kfree(k);
        return ERR_PTR(-ENOMEM);
    }
    // PD's kthread_add makes the thread suspended, sched_add resumes it
    sched_add(td, 0);
    wait_for_completion(&k->ready);
    return k->task;
}

// starts a thread kthread_create made, or wakes any other task as Linux's wake_up_process does
bool
pd_uvm_wake_up_process(struct task_struct *task)
{
    struct pd_uvm_kthread *k;

    spin_lock(&pd_uvm_kthreads_lock);
    list_for_each_entry(k, &pd_uvm_kthreads, link) {
        if (k->task == task) {
            list_del_init(&k->link);
            complete(&k->go);
            spin_unlock(&pd_uvm_kthreads_lock);
            return true;
        }
    }
    spin_unlock(&pd_uvm_kthreads_lock);
    return linux_wake_up_state(task, TASK_NORMAL);
}
