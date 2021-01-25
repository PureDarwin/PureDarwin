//
//  eventlink.c
//  libdispatch
//
//  Created by Rokhini Prabhu on 12/13/19.
//

#include "internal.h"
#include <os/assumes.h>

#if OS_EVENTLINK_USE_MACH_EVENTLINK

OS_OBJECT_CLASS_DECL(os_eventlink);
#if !USE_OBJC
OS_OBJECT_VTABLE_INSTANCE(os_eventlink,
		(void (*)(_os_object_t))_os_eventlink_xref_dispose,
		(void (*)(_os_object_t))_os_eventlink_dispose);
#endif // USE_OBJC
#define EVENTLINK_CLASS OS_OBJECT_VTABLE(os_eventlink)

/* Convenience macros for accessing into the struct os_eventlink_s */
#define ev_local_port port_pair.pair[0]
#define ev_remote_port port_pair.pair[1]
#define ev_port_pair port_pair.desc

#pragma mark Internal functions

void
_os_eventlink_xref_dispose(os_eventlink_t ev) {
	return _os_object_release_internal(ev->_as_os_obj);
}

void
_os_eventlink_dispose(os_eventlink_t ev) {
	if (ev->ev_state & OS_EVENTLINK_LABEL_NEEDS_FREE) {
		free((void *) ev->name);
	}

	if (MACH_PORT_VALID(ev->ev_local_port)) {
		mach_port_deallocate(mach_task_self(), ev->ev_local_port);
	}
	if (MACH_PORT_VALID(ev->ev_remote_port)) {
		mach_port_deallocate(mach_task_self(), ev->ev_remote_port);
	}
}

static inline os_eventlink_t
_os_eventlink_create_internal(const char *name)
{
	os_eventlink_t ev = NULL;
	ev = (os_eventlink_t) _os_object_alloc(EVENTLINK_CLASS,
			sizeof(struct os_eventlink_s));
	if (ev == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (name) {
		const char *tmp = _dispatch_strdup_if_mutable(name);
		if (tmp != name) {
			ev->ev_state |= OS_EVENTLINK_LABEL_NEEDS_FREE;
		}
		ev->name = tmp;
	}

	return ev;
}

static inline int
_mach_error_to_errno(kern_return_t kr)
{
	int ret = 0;

	switch (kr) {
	case KERN_NAME_EXISTS:
		ret = EALREADY;
		break;
	case KERN_INVALID_ARGUMENT:
		ret = EINVAL;
		break;
	case KERN_OPERATION_TIMED_OUT:
		ret = ETIMEDOUT;
		break;
	case KERN_INVALID_NAME:
		/* This is most likely due to waiting on a cancelled eventlink but also
		 * possible to hit this if there is a bug and a double free of the port. */
	case KERN_TERMINATED: /* Other side died */
		ret = ECANCELED;
		break;
	case KERN_ABORTED:
		ret = ECONNABORTED;
		break;
	case KERN_SUCCESS:
		ret = 0;
		break;
	default:
		return -1;
	}

	errno = ret;
	return ret;
}

static uint64_t
_os_clockid_normalize_to_machabs(os_clockid_t inclock, uint64_t intimeout)
{
	uint64_t timeout = 0;

	switch (inclock) {
	case OS_CLOCK_MACH_ABSOLUTE_TIME:
		timeout = intimeout;
		break;
	}

	return timeout;
}

static int
os_eventlink_wait_until_internal(os_eventlink_t ev, os_clockid_t clock,
		uint64_t deadline, uint64_t *signals_consumed_out)
{
	int ret = 0;
	os_assert(clock == OS_CLOCK_MACH_ABSOLUTE_TIME);

	// These checks are racy but allows us to shortcircuit in userspace
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	kern_return_t kr = KERN_SUCCESS;
	uint64_t count_to_exceed = ev->local_count;

	kr = mach_eventlink_wait_until(ev->ev_local_port, &ev->local_count,
			MELSW_OPTION_NONE, KERN_CLOCK_MACH_ABSOLUTE_TIME, deadline);
	if (kr == KERN_SUCCESS && (signals_consumed_out != NULL)) {
		*signals_consumed_out = ev->local_count - count_to_exceed;
	} else if (kr == KERN_INVALID_NAME) {
		/* This means that the eventlink got cancelled after the cancel check
		 * above but before we waited --> assert that that is indeed the case */
		os_assert(_os_eventlink_is_cancelled(ev->ev_state));
	}

	return _mach_error_to_errno(kr);
}

static int
os_eventlink_signal_and_wait_until_internal(os_eventlink_t ev, os_clockid_t clock,
		uint64_t deadline, uint64_t * _Nullable signals_consumed_out)
{
	int ret = 0;
	kern_return_t kr = KERN_SUCCESS;
	os_assert(clock == OS_CLOCK_MACH_ABSOLUTE_TIME);

	// These checks are racy but allows us to shortcircuit in userspace
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	uint64_t count_to_exceed = ev->local_count;
	kr = mach_eventlink_signal_wait_until(ev->ev_local_port, &ev->local_count, 0,
			MELSW_OPTION_NONE, KERN_CLOCK_MACH_ABSOLUTE_TIME, deadline);

	if (kr == KERN_SUCCESS && (signals_consumed_out != NULL)) {
		*signals_consumed_out = ev->local_count - count_to_exceed;
	} else if (kr == KERN_INVALID_NAME) {
		/* This means that the eventlink got cancelled after the cancel check
		 * above but before we signal and waited --> assert that that is indeed
		 * the case */
		os_assert(_os_eventlink_is_cancelled(ev->ev_state));
	}

	return _mach_error_to_errno(kr);
}


#pragma mark Private functions

os_eventlink_t
os_eventlink_create(const char *name)
{
	return _os_eventlink_create_internal(name);
}

int
os_eventlink_activate(os_eventlink_t ev)
{
	int ret = 0;

	// These checks are racy but allow us to shortcircuit before we make the syscall
	if (MACH_PORT_VALID(ev->ev_local_port)) {
		return ret;
	}

	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	struct os_eventlink_s tmp_ev;
	bzero(&tmp_ev, sizeof(tmp_ev));

	kern_return_t kr = mach_eventlink_create(mach_task_self(), MELC_OPTION_NO_COPYIN, &tmp_ev.ev_local_port);
	if (kr == KERN_SUCCESS) {
		// Only atomically store the new ports if we have
		// EVENTLINK_INACTIVE_PORT there. The only reason this would fail is
		// cause it was concurrently activated.
		uint64_t dummy;
		bool success = os_atomic_cmpxchgv(&ev->ev_port_pair, EVENTLINK_INACTIVE_PORT, tmp_ev.ev_port_pair, &dummy, relaxed);
		if (!success) {
			// tmp_ev still has valid ports that need to be released
			if (MACH_PORT_VALID(tmp_ev.ev_local_port)) {
				mach_port_deallocate(mach_task_self(), tmp_ev.ev_local_port);
			}
			if (MACH_PORT_VALID(tmp_ev.ev_remote_port)) {
				mach_port_deallocate(mach_task_self(), tmp_ev.ev_remote_port);
			}
			return EINVAL;
		}
	}

	return _mach_error_to_errno(kr);
}

int
os_eventlink_extract_remote_port(os_eventlink_t ev, mach_port_t *port_out)
{
	int ret = 0;

	// These checks are racy but allows us to shortcircuit and give the right
	// errors
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	/* We're giving away our +1 to the remote port */
	mach_port_t port = os_atomic_xchg(&ev->ev_remote_port, EVENTLINK_CLEARED_PORT, relaxed);
	if (!MACH_PORT_VALID(port)) {
		errno = ret = EINVAL;
		return ret;
	}
	*port_out = port;

	return ret;
}

os_eventlink_t
os_eventlink_create_with_port(const char *name, mach_port_t port)
{
	os_eventlink_t ev = _os_eventlink_create_internal(name);
	if (ev == NULL) {
		return NULL;
	}
	/* Take our own +1 on the port */
	kern_return_t kr;
	kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
	os_assert(kr == KERN_SUCCESS);

	os_assert(ev->ev_local_port ==  EVENTLINK_INACTIVE_PORT);
	ev->ev_local_port = port;
	return ev;
}

os_eventlink_t
os_eventlink_create_remote_with_eventlink(const char *name, os_eventlink_t template)
{
	mach_port_t mp;
	int ret = os_eventlink_extract_remote_port(template, &mp);
	if (ret) {
		errno = ret;
		return NULL;
	}

	os_eventlink_t ev = os_eventlink_create_with_port(name, mp);

	/* os_eventlink_create_with_port doesn't consume the right it was given, we
	 * should release our reference */
	mach_port_mod_refs(mach_task_self(), mp, MACH_PORT_RIGHT_SEND, -1);

	return ev;
}

int
os_eventlink_associate(os_eventlink_t ev, os_eventlink_associate_options_t
		options)
{
	int ret = 0;

	// These checks are racy but allows us to shortcircuit in userspace
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	mach_eventlink_associate_option_t mela_options;
	mela_options = (options == OE_ASSOCIATE_ON_WAIT) ?
			MELA_OPTION_ASSOCIATE_ON_WAIT : MELA_OPTION_NONE;
	mach_port_t thread_port = (options == OE_ASSOCIATE_ON_WAIT) ? MACH_PORT_NULL : _dispatch_thread_port();

	kern_return_t kr = KERN_SUCCESS;
	kr = mach_eventlink_associate(ev->ev_local_port, thread_port, 0, 0, 0, 0, mela_options);
	return _mach_error_to_errno(kr);
}

int
os_eventlink_disassociate(os_eventlink_t ev)
{
	int ret = 0;

	// These checks are racy but allows us to shortcircuit in userspace
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		/* Don't bother call mach_eventlink_disassociate since the backing
		 * eventlink object in the kernel will be gone */
		return ret;
	}

	/* TODO: Track the associated thread in the eventlink object and error out
	 * in user space if the thread calling disassociate isn't the same thread.
	 * The kernel doesn't enforce this */
	kern_return_t kr = KERN_SUCCESS;
	kr = mach_eventlink_disassociate(ev->ev_local_port, MELD_OPTION_NONE);

	if (kr == KERN_TERMINATED) {
		/* Absorb this error in libdispatch, knowing that the other side died
		 * first is not helpful here */
		return 0;
	}

	return _mach_error_to_errno(kr);
}


int
os_eventlink_wait_until(os_eventlink_t ev, os_clockid_t clock,
		uint64_t timeout, uint64_t *signals_consumed_out)
{
	uint64_t machabs_timeout = _os_clockid_normalize_to_machabs(clock, timeout);

	/* Convert timeout to deadline */
	return os_eventlink_wait_until_internal(ev, clock, mach_absolute_time() + machabs_timeout,
			signals_consumed_out);
}

int
os_eventlink_wait(os_eventlink_t ev, uint64_t *signals_consumed_out)
{
	/* Passing in deadline = 0 means wait forever */
	return os_eventlink_wait_until_internal(ev, OS_CLOCK_MACH_ABSOLUTE_TIME, 0,
			signals_consumed_out);
}

int
os_eventlink_signal(os_eventlink_t ev)
{
	int ret = 0;

	// This is racy but allows us to shortcircuit in userspace
	if (_os_eventlink_inactive(ev->ev_local_port)) {
		errno = ret = EINVAL;
		return ret;
	}
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		errno = ret = ECANCELED;
		return ret;
	}

	kern_return_t kr = KERN_SUCCESS;
	kr = mach_eventlink_signal(ev->ev_local_port, 0);

	return _mach_error_to_errno(kr);
}

int
os_eventlink_signal_and_wait(os_eventlink_t ev, uint64_t *signals_consumed_out)
{
	/* Passing in deadline = 0 means wait forever */
	return os_eventlink_signal_and_wait_until_internal(ev, OS_CLOCK_MACH_ABSOLUTE_TIME, 0,
			signals_consumed_out);
}

int
os_eventlink_signal_and_wait_until(os_eventlink_t ev, os_clockid_t clock,
		uint64_t timeout, uint64_t * _Nullable signals_consumed_out)
{
	uint64_t machabs_timeout = _os_clockid_normalize_to_machabs(clock, timeout);

	/* Converts timeout to deadline */
	return os_eventlink_signal_and_wait_until_internal(ev, clock, mach_absolute_time() + machabs_timeout,
			signals_consumed_out);
}

void
os_eventlink_cancel(os_eventlink_t ev)
{
	if (_os_eventlink_is_cancelled(ev->ev_state)) {
		return;
	}

	os_atomic_or(&ev->ev_state, OS_EVENTLINK_CANCELLED, relaxed);


	mach_port_t p = ev->ev_local_port;
	if (MACH_PORT_VALID(p)) {
		/* mach_eventlink_destroy consumes a ref on the ports. We therefore take
		 * +1 on the local port so that other threads using the ev_local_port have valid
		 * ports even if it isn't backed by an eventlink object. The last ref of
		 * the port in the eventlink object will be dropped in xref dispose */
		kern_return_t kr = mach_port_mod_refs(mach_task_self(), p, MACH_PORT_RIGHT_SEND, 1);
		os_assert(kr == KERN_SUCCESS);
		mach_eventlink_destroy(p);
	}

	// If the remote port was valid, then we already called destroy on the
	// local port and we don't need to call it again on the remote port. We keep
	// the reference we already have on the remote port (if any) and deallocate
	// it in xref dispose

}

#else /* OS_EVENTLINK_USE_MACH_EVENTLINK */
#pragma mark Simulator

void
_os_eventlink_dispose(os_eventlink_t __unused ev) {
}

os_eventlink_t
os_eventlink_create(const char * __unused name)
{
	return NULL;
}

int
os_eventlink_activate(os_eventlink_t __unused ev)
{
	return ENOTSUP;
}

int
os_eventlink_extract_remote_port(os_eventlink_t __unused eventlink, mach_port_t *port_out)
{
	*port_out = MACH_PORT_NULL;
	return ENOTSUP;
}

os_eventlink_t
os_eventlink_create_with_port(const char * __unused name, mach_port_t __unused mach_port)
{
	errno = ENOTSUP;
	return NULL;
}

os_eventlink_t
os_eventlink_create_remote_with_eventlink(const char * __unused name, os_eventlink_t __unused eventlink)
{
	errno = ENOTSUP;
	return NULL;
}

int
os_eventlink_associate(os_eventlink_t __unused eventlink, os_eventlink_associate_options_t __unused options)
{
	int ret = errno = ENOTSUP;
	return ret;
}

int
os_eventlink_disassociate(os_eventlink_t __unused eventlink)
{
	int ret = errno = ENOTSUP;
	return ret;
}

int
os_eventlink_wait(os_eventlink_t __unused eventlink, uint64_t * _Nullable signals_consumed_out)
{
	int ret = errno = ENOTSUP;
	*signals_consumed_out = 0;
	return ret;
}

int
os_eventlink_wait_until(os_eventlink_t __unused eventlink, os_clockid_t __unused clock,
		uint64_t __unused timeout, uint64_t * _Nullable signals_consumed_out)
{
	int ret = errno = ENOTSUP;
	*signals_consumed_out = 0;
	return ret;
}

int
os_eventlink_signal(os_eventlink_t  __unused eventlink)
{
	int ret = errno = ENOTSUP;
	return ret;
}

int
os_eventlink_signal_and_wait(os_eventlink_t  __unused eventlink, uint64_t * _Nullable signals_consumed_out)
{
	int ret = errno = ENOTSUP;
	*signals_consumed_out = 0;
	return ret;
}

int
os_eventlink_signal_and_wait_until(os_eventlink_t __unused eventlink, os_clockid_t __unused clock,
		uint64_t __unused timeout, uint64_t * _Nullable signals_consumed_out)
{
	int ret = errno = ENOTSUP;
	*signals_consumed_out = 0;
	return ret;
}

void
os_eventlink_cancel(os_eventlink_t __unused ev)
{
}

#endif /* OS_EVENTLINK_USE_MACH_EVENTLINK */
