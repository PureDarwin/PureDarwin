// SPDX-License-Identifier: MIT
// the FreeBSD kernel services linuxkpi expects, on top of the fbsdkpi seam

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sleepqueue.h>
#include <sys/capsicum.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <vm/vm_page.h>

#include <string.h>

#include "../../pd_fbsd_os.h"

typedef int32_t pd_thread_id;
void **pd_thread_fbsd_slot(const char **name, pd_thread_id *id);
extern void (*pd_thread_fbsd_release)(void *fbsd_thread);

char osrelease[] = "26.5";
cap_rights_t cap_no_rights;
int pci_enable_aspm = 0;

int
linux_alloc_current_noop(struct thread *td, int flags)
{
	return 0;
}

int (*lkpi_alloc_current)(struct thread *, int) = linux_alloc_current_noop;
void linux_free_current_pd(struct thread *td);

static void
pd_lkpi_thread_release(void *fbsd_thread)
{
	struct thread *td = fbsd_thread;

	// the task_struct is dropped by linuxkpi's own release path
	if (td->td_lkpi_task != NULL)
		linux_free_current_pd(td);
	pdos_free(td);
}

struct thread *
pd_curthread(void)
{
	const char *name;
	pd_thread_id id;
	void **slot = pd_thread_fbsd_slot(&name, &id);
	if (slot == NULL)
		pdos_panic("fbsdkpi: no thread slot for curthread");
	if (*slot != NULL)
		return *slot;

	// first use on this thread, interrupt filters cannot get here
	struct thread *td = pdos_malloc(sizeof(*td), 1, 0);
	if (td == NULL)
		pdos_panic("fbsdkpi: out of memory for struct thread");

	td->td_tid = id;
	strlcpy(td->td_name, name[0] != '\0' ? name : "xnu", sizeof(td->td_name));
	pd_thread_fbsd_release = pd_lkpi_thread_release;
	*slot = td;
	return td;
}

void
pd_lkpi_yield(void)
{
	pdos_sleep_us(1);
}

unsigned long
pd_lkpi_physmem(void)
{
	// in pages, linuxkpi only sizes caches from it
	return (unsigned long)(4ULL << 30) / PAGE_SIZE;
}

char *
pd_lkpi_strdup(const char *string)
{
	size_t len = strlen(string) + 1;
	char *copy = malloc(len, M_DEVBUF, M_NOWAIT);
	if (copy != NULL)
		memcpy(copy, string, len);
	return copy;
}

char *
pd_lkpi_strndup(const char *string, size_t size)
{
	size_t len = strnlen(string, size);
	char *copy = malloc(len + 1, M_DEVBUF, M_NOWAIT);
	if (copy != NULL) {
		memcpy(copy, string, len);
		copy[len] = '\0';
	}
	return copy;
}

int
pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t pr, int flags)
{
	int64_t us = (sbt * 1000000) >> 32;
	pdos_sleep_us(us > 0 ? us : 1);
	return EWOULDBLOCK;
}

// sleepqueue(9) over the global wait lock, which a thread holds from
// sleepq_lock until it sleeps or calls sleepq_release

void
sleepq_lock(const void *wchan)
{
	curthread->td_pd_wait_state = pdos_wait_lock();
}

void
sleepq_release(const void *wchan)
{
	pdos_wait_unlock(curthread->td_pd_wait_state);
}

void
sleepq_add(const void *wchan, struct lock_object *lock, const char *wmesg,
	int flags, int queue)
{
	curthread->td_pd_timeout_us = 0;
}

void
sleepq_set_timeout(const void *wchan, int timo)
{
	curthread->td_pd_timeout_us = (long long)timo * 1000000 / hz;
	if (curthread->td_pd_timeout_us <= 0)
		curthread->td_pd_timeout_us = 1;
}

void
sleepq_set_timeout_sbt(const void *wchan, sbintime_t sbt, sbintime_t pr, int flags)
{
	curthread->td_pd_timeout_us = (sbt * 1000000) >> 32;
	if (curthread->td_pd_timeout_us <= 0)
		curthread->td_pd_timeout_us = 1;
}

static int
sleepq_sleep(const void *wchan, int interruptible)
{
	struct thread *td = curthread;
	int state = td->td_pd_wait_state;
	int result = pdos_wait_sleep(wchan, td->td_pd_timeout_us, interruptible, &state);
	td->td_pd_timeout_us = 0;
	pdos_wait_unlock(state);

	if (result == PDOS_WAIT_TIMEOUT)
		return EWOULDBLOCK;
	if (result == PDOS_WAIT_INTR)
		return EINTR;
	return 0;
}

void
sleepq_wait(const void *wchan, int pri)
{
	sleepq_sleep(wchan, 0);
}

int
sleepq_wait_sig(const void *wchan, int pri)
{
	return sleepq_sleep(wchan, 1);
}

int
sleepq_timedwait(const void *wchan, int pri)
{
	return sleepq_sleep(wchan, 0);
}

int
sleepq_timedwait_sig(const void *wchan, int pri)
{
	return sleepq_sleep(wchan, 1);
}

int
sleepq_signal(const void *wchan, int flags, int pri, int queue)
{
	pdos_wakeup(wchan, 0);
	return 0;
}

int
sleepq_broadcast(const void *wchan, int flags, int pri, int queue)
{
	pdos_wakeup(wchan, 1);
	return 0;
}

u_int
sleepq_sleepcnt(const void *wchan, int queue)
{
	return 0;
}
