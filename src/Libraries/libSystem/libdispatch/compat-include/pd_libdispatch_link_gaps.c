/*
 * PureDarwin: fills genuine link-time gaps between real libdispatch and
 * what this tree's from-scratch pthread/xnu actually implement, rather
 * than pretending those subsystems exist:
 *
 * - pthread_static (src/Libraries/libSystem/pthread) is deliberately the
 *   reduced VARIANT_DYLD build (dyld's own static-link needs), not full
 *   pthread - it never compiles the real QoS/workqueue-override machinery
 *   in pthread/src/qos.c (which itself needs real kernel workqueue
 *   support this project doesn't have). Stub the handful of QoS/workqueue
 *   entry points libdispatch actually calls as safe no-ops: single-
 *   threaded/no-QoS-class behavior degrades gracefully (queues still run,
 *   just without real QoS-based scheduling), matching the
 *   work_interval_instance_t stub's approach.
 * - _strerror$UNIX2003 is a small, genuinely-missing plain utility
 *   function.
 */

#include <errno.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <pthread/qos_private.h>
#include <pthread/workqueue_private.h>
#include <sys/qos.h>
#include <string.h>
#include <stdio.h>
#include <mach/mach_types.h>

int
pthread_attr_set_qos_class_np(pthread_attr_t *attr, qos_class_t qos_class, int relative_priority)
{
	(void)attr; (void)qos_class; (void)relative_priority;
	return 0;
}

int
pthread_attr_get_qos_class_np(pthread_attr_t *attr, qos_class_t *qos_class, int *relative_priority)
{
	(void)attr;
	if (qos_class) *qos_class = QOS_CLASS_UNSPECIFIED;
	if (relative_priority) *relative_priority = 0;
	return 0;
}

pthread_priority_t
_pthread_qos_class_encode(qos_class_t qos_class, int relative_priority, unsigned long flags)
{
	(void)qos_class; (void)relative_priority; (void)flags;
	return 0;
}

int
pthread_qos_max_parallelism(qos_class_t qos, unsigned long flags)
{
	(void)qos; (void)flags;
	return 1;
}

int
_pthread_qos_override_start_direct(mach_port_t thread, pthread_priority_t priority, void *resource)
{
	(void)thread; (void)priority; (void)resource;
	return 0;
}

int
_pthread_qos_override_end_direct(mach_port_t thread, void *resource)
{
	(void)thread; (void)resource;
	return 0;
}

int
_pthread_set_properties_self(_pthread_set_flags_t flags, pthread_priority_t priority, mach_port_t voucher)
{
	(void)flags; (void)priority; (void)voucher;
	return 0;
}

int
_pthread_workqueue_override_reset(void)
{
	return 0;
}

int
_pthread_workqueue_override_start_direct(mach_port_t thread, pthread_priority_t priority)
{
	(void)thread; (void)priority;
	return 0;
}

int
_pthread_workqueue_override_start_direct_check_owner(mach_port_t thread, pthread_priority_t priority, mach_port_t *ulock_addr)
{
	(void)thread; (void)priority; (void)ulock_addr;
	return 0;
}

qos_class_t
qos_class_main(void)
{
	return QOS_CLASS_UNSPECIFIED;
}

extern char *__pd_strerror_unix2003(int errnum) __asm("_strerror$UNIX2003");
char *
__pd_strerror_unix2003(int errnum)
{
	return strerror(errnum);
}
