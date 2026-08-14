/*
 * os_state: the state-dump registry side of libsystem_trace.
 */

#include <Block.h>
#include <dispatch/dispatch.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct os_state_hints_s *os_state_hints_t;
typedef uint64_t os_state_handle_t;
typedef struct os_state_data_s *os_state_data_t;
typedef os_state_data_t (^os_state_block_t)(os_state_hints_t hints);

struct pd_state_handler {
	struct pd_state_handler	*next;
	os_state_handle_t	handle;
	dispatch_queue_t	queue;
	os_state_block_t	block;
};

static struct pd_state_handler	*pd_state_handlers;
static os_state_handle_t	pd_state_next_handle = 1;
static os_unfair_lock		pd_state_lock = OS_UNFAIR_LOCK_INIT;

os_state_handle_t
os_state_add_handler(dispatch_queue_t queue, os_state_block_t block)
{
	struct pd_state_handler *h;

	if (block == NULL) {
		return 0;
	}

	h = calloc(1, sizeof(*h));
	if (h == NULL) {
		return 0;
	}

	h->queue = queue;
	if (queue != NULL) {
		dispatch_retain(queue);
	}
	h->block = Block_copy(block);
	if (h->block == NULL) {
		if (queue != NULL) {
			dispatch_release(queue);
		}
		free(h);
		return 0;
	}

	os_unfair_lock_lock(&pd_state_lock);
	h->handle = pd_state_next_handle++;
	h->next = pd_state_handlers;
	pd_state_handlers = h;
	os_unfair_lock_unlock(&pd_state_lock);

	return h->handle;
}

void
os_state_remove_handler(os_state_handle_t handle)
{
	struct pd_state_handler *h = NULL;
	struct pd_state_handler **pp;

	if (handle == 0) {
		return;
	}

	os_unfair_lock_lock(&pd_state_lock);
	for (pp = &pd_state_handlers; *pp != NULL; pp = &(*pp)->next) {
		if ((*pp)->handle == handle) {
			h = *pp;
			*pp = h->next;
			break;
		}
	}
	os_unfair_lock_unlock(&pd_state_lock);

	if (h == NULL) {
		return;
	}

	/* Released outside the lock: neither call needs it held. */
	if (h->queue != NULL) {
		dispatch_release(h->queue);
	}
	Block_release(h->block);
	free(h);
}
