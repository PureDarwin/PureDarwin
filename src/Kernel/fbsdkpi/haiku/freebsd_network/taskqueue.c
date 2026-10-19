/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include "device.h"

#include <stdio.h>

#include <compat/sys/callout.h>
#include <compat/sys/taskqueue.h>
#include <compat/sys/haiku-module.h>


static int _taskqueue_start_threads(struct taskqueue **taskQueue,
	int count, int priority, const char *name);
static void taskqueue_terminate(struct thread **pp, struct taskqueue *tq);


#define malloc kernel_malloc
#define free kernel_free
#include "fbsd_subr_taskqueue.c"
#undef malloc
#undef free


struct taskqueue *taskqueue_fast = NULL;
struct taskqueue *taskqueue_swi = NULL;
struct taskqueue *taskqueue_thread = NULL;


static int32
tq_handle_thread(void *data)
{
	struct taskqueue *tq = data;

	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_INIT);

	TQ_LOCK(tq);
	sem_id sem = tq->tq_sem;
	TQ_UNLOCK(tq);

	while (acquire_sem(sem) == B_NO_ERROR) {
		TQ_LOCK(tq);
		taskqueue_run_locked(tq);
		TQ_UNLOCK(tq);
	}

	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_SHUTDOWN);

	return 0;
}


static int
_taskqueue_start_threads(struct taskqueue **taskQueue, int count, int priority,
	const char *name)
{
	struct taskqueue *tq = (*taskQueue);
	int i, j;

	if (count == 0)
		return -1;

	if (tq->tq_threads != NULL)
		return -1;

	if (count == 1) {
		tq->tq_threads = &tq->tq_thread_storage;
	} else {
		tq->tq_threads = malloc(sizeof(thread_id) * count);
		if (tq->tq_threads == NULL)
			return B_NO_MEMORY;
	}

	tq->tq_sem = create_sem(0, tq->tq_name);
	if (tq->tq_sem < B_OK) {
		if (count > 1)
			free(tq->tq_threads);
		tq->tq_threads = NULL;
		return tq->tq_sem;
	}

	for (i = 0; i < count; i++) {
		tq->tq_threads[i] = spawn_kernel_thread(tq_handle_thread, tq->tq_name,
			priority, tq);
		if (tq->tq_threads[i] < B_OK) {
			status_t status = tq->tq_threads[i];
			for (j = 0; j < i; j++)
				kill_thread(tq->tq_threads[j]);
			if (count > 1)
				free(tq->tq_threads);
			tq->tq_threads = NULL;
			delete_sem(tq->tq_sem);
			return status;
		}
	}

	tq->tq_threadcount = count;

	for (i = 0; i < count; i++)
		resume_thread(tq->tq_threads[i]);

	return 0;
}


static void
taskqueue_terminate(struct thread **pp, struct taskqueue *tq)
{
	if (tq->tq_sem == -1)
		return;

	TQ_UNLOCK(tq);

	delete_sem(tq->tq_sem);
	tq->tq_sem = -1;

	for (int i = 0; i < tq->tq_threadcount; i++) {
		status_t status;
		wait_for_thread(tq->tq_threads[i], &status);
	}

	if (tq->tq_threadcount > 1)
		free(tq->tq_threads);
	tq->tq_threads = NULL;

	TQ_LOCK(tq);
}


void
taskqueue_drain(struct taskqueue *taskQueue, struct task *task)
{
	if (taskQueue == NULL)
		return;

	TQ_LOCK(taskQueue);
	while (task->ta_pending != 0 || task_is_running(taskQueue, task)) {
		TQ_UNLOCK(taskQueue);
		snooze(0);
		TQ_LOCK(taskQueue);
	}
	TQ_UNLOCK(taskQueue);
}


void
taskqueue_drain_all(struct taskqueue *taskQueue)
{
	struct task t_barrier;

	if (taskQueue == NULL) {
		printf("taskqueue_drain_all called with NULL taskqueue\n");
		return;
	}

	TASK_INIT(&t_barrier, USHRT_MAX, taskqueue_task_nop_fn, &t_barrier);
	taskqueue_enqueue(taskQueue, &t_barrier);
	taskqueue_drain(taskQueue, &t_barrier);
}


void
taskqueue_thread_enqueue(void *context)
{
	struct taskqueue **tqp = context;
	release_sem_etc((*tqp)->tq_sem, 1, B_DO_NOT_RESCHEDULE);
}


void
_task_init(struct task *task, int prio, task_fn_t handler, void *context)
{
	task->ta_priority = prio;
	task->ta_flags = 0;
	task->ta_func = handler;
	task->ta_context = context;
	task->ta_pending = 0;
}


status_t
init_taskqueues()
{
	status_t status = B_NO_MEMORY;

	if (HAIKU_DRIVER_REQUIRES(FBSD_FAST_TASKQUEUE)) {
		taskqueue_fast = taskqueue_create_fast("fast taskq", 0,
			taskqueue_thread_enqueue, &taskqueue_fast);
		if (taskqueue_fast == NULL)
			return B_NO_MEMORY;

		status = taskqueue_start_threads(&taskqueue_fast, 1,
			B_REAL_TIME_PRIORITY, "fast taskq thread");
		if (status < B_OK)
			goto err_1;
	}

	if (HAIKU_DRIVER_REQUIRES(FBSD_SWI_TASKQUEUE)) {
		taskqueue_swi = taskqueue_create_fast("swi taskq", 0,
			taskqueue_thread_enqueue, &taskqueue_swi);
		if (taskqueue_swi == NULL) {
			status = B_NO_MEMORY;
			goto err_1;
		}

		status = taskqueue_start_threads(&taskqueue_swi, 1,
			B_REAL_TIME_PRIORITY, "swi taskq");
		if (status < B_OK)
			goto err_2;
	}

	if (HAIKU_DRIVER_REQUIRES(FBSD_THREAD_TASKQUEUE)) {
		taskqueue_thread = taskqueue_create("thread taskq", 0,
			taskqueue_thread_enqueue, &taskqueue_thread);
		if (taskqueue_thread == NULL) {
			status = B_NO_MEMORY;
			goto err_2;
		}

		status = taskqueue_start_threads(&taskqueue_thread, 1,
			B_REAL_TIME_PRIORITY, "swi taskq");
		if (status < B_OK)
			goto err_3;
	}

	return B_OK;

err_3:
	if (taskqueue_thread)
		taskqueue_free(taskqueue_thread);

err_2:
	if (taskqueue_swi)
		taskqueue_free(taskqueue_swi);

err_1:
	if (taskqueue_fast)
		taskqueue_free(taskqueue_fast);

	return status;
}


void
uninit_taskqueues()
{
	if (HAIKU_DRIVER_REQUIRES(FBSD_THREAD_TASKQUEUE))
		taskqueue_free(taskqueue_thread);

	if (HAIKU_DRIVER_REQUIRES(FBSD_SWI_TASKQUEUE))
		taskqueue_free(taskqueue_swi);

	if (HAIKU_DRIVER_REQUIRES(FBSD_FAST_TASKQUEUE))
		taskqueue_free(taskqueue_fast);
}
