/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_QUEUE_H
#define _KERNEL_QUEUE_H

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct queue {
	void *head;
	void *tail;
	int count;
} queue;

int queue_init(queue *q);
int queue_remove_item(queue *q, void *e);
int queue_enqueue(queue *q, void *e);
void *queue_dequeue(queue *q);
void *queue_peek(queue *q);

typedef struct fixed_queue {
	void **table;
	int head;
	int tail;
	int count;
	int size;
} fixed_queue;

int fixed_queue_init(fixed_queue *q, int size);
void fixed_queue_destroy(fixed_queue *q);
int fixed_queue_enqueue(fixed_queue *q, void *e);
void *fixed_queue_dequeue(fixed_queue *q);
void *fixed_queue_peek(fixed_queue *q);

#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL_QUEUE_H */
