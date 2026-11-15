// pd_nv_kthread_q.c - the nv_kthread_q API nvidia-drm queues its work on, one linuxkpi kthread per queue
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include "nv-kthread-q.h"

// takes the next item off the queue, NULL when it is empty
static nv_kthread_q_item_t *
q_next(nv_kthread_q_t *q)
{
	nv_kthread_q_item_t *item = NULL;
	unsigned long flags;

	spin_lock_irqsave(&q->q_lock, flags);
	if (!list_empty(&q->q_list_head)) {
		item = list_first_entry(&q->q_list_head, nv_kthread_q_item_t, q_list_node);
		list_del_init(&item->q_list_node);
	}
	spin_unlock_irqrestore(&q->q_lock, flags);
	return item;
}

// one up on q_sem for each item queued, and one more to stop
static int
q_main(void *arg)
{
	nv_kthread_q_t *q = arg;
	nv_kthread_q_item_t *item;

	for (;;) {
		down(&q->q_sem);
		if (atomic_read(&q->main_loop_should_exit))
			break;
		if ((item = q_next(q)) != NULL)
			item->function_to_run(item->function_args);
	}
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

int
nv_kthread_q_init_on_node(nv_kthread_q_t *q, const char *qname, int preferred_node)
{
	memset(q, 0, sizeof(*q));
	INIT_LIST_HEAD(&q->q_list_head);
	spin_lock_init(&q->q_lock);
	sema_init(&q->q_sem, 0);
	q->q_kthread = kthread_run(q_main, q, "%s", qname);
	return q->q_kthread == NULL ? -ENOMEM : 0;
}

int
nv_kthread_q_init(nv_kthread_q_t *q, const char *qname)
{
	return nv_kthread_q_init_on_node(q, qname, NV_KTHREAD_NO_NODE);
}

void
nv_kthread_q_item_init(nv_kthread_q_item_t *q_item, nv_q_func_t function_to_run, void *function_args)
{
	INIT_LIST_HEAD(&q_item->q_list_node);
	q_item->function_to_run = function_to_run;
	q_item->function_args = function_args;
}

// 1 when queued, 0 when the item is already waiting or the queue is going away
int
nv_kthread_q_schedule_q_item(nv_kthread_q_t *q, nv_kthread_q_item_t *q_item)
{
	unsigned long flags;
	int queued = 0;

	spin_lock_irqsave(&q->q_lock, flags);
	if (!q->is_unload_flush_ongoing && list_empty(&q_item->q_list_node)) {
		list_add_tail(&q_item->q_list_node, &q->q_list_head);
		queued = 1;
	}
	spin_unlock_irqrestore(&q->q_lock, flags);
	if (queued)
		up(&q->q_sem);
	return queued;
}

static void
q_flush_done(void *arg)
{
	up((struct semaphore *)arg);
}

// an item at the back of the queue, waited for: everything in front of it has run by then
static void
q_flush(nv_kthread_q_t *q)
{
	nv_kthread_q_item_t item;
	struct semaphore done;
	unsigned long flags;

	sema_init(&done, 0);
	nv_kthread_q_item_init(&item, q_flush_done, &done);
	spin_lock_irqsave(&q->q_lock, flags);
	list_add_tail(&item.q_list_node, &q->q_list_head);
	spin_unlock_irqrestore(&q->q_lock, flags);
	up(&q->q_sem);
	down(&done);
}

void
nv_kthread_q_flush(nv_kthread_q_t *q)
{
	if (q->q_kthread == NULL)
		return;
	// twice, for items that queue another as they run
	q_flush(q);
	q_flush(q);
}

void
nv_kthread_q_stop(nv_kthread_q_t *q)
{
	unsigned long flags;

	if (q->q_kthread == NULL)
		return;
	spin_lock_irqsave(&q->q_lock, flags);
	q->is_unload_flush_ongoing = true;
	spin_unlock_irqrestore(&q->q_lock, flags);
	q_flush(q);
	atomic_set(&q->main_loop_should_exit, 1);
	up(&q->q_sem);
	kthread_stop(q->q_kthread);
	q->q_kthread = NULL;
}

int
nv_kthread_q_run_self_test(void)
{
	return 0;
}
