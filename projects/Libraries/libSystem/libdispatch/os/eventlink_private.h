#ifndef __OS_EVENTLINK__
#define __OS_EVENTLINK__

#include <os/object.h>
#include <mach/mach.h>
#include <os/clock.h>

__BEGIN_DECLS

OS_OBJECT_ASSUME_NONNULL_BEGIN

/*!
 * @typedef os_eventlink_t
 *
 * @abstract
 * A reference counted os_object representing a directed paired link of "wake" events
 * between two designated threads, the link `source` and the link `target`.
 * The target thread may optionally inherit properties of the source thread upon
 * return from wait (such as membership in a workgroup).
 *
 * @discussion
 * Threads explicitly associate themselves with an an eventlink, only one source
 * and one target may exist per eventlink.
 */
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
typedef struct os_eventlink_s *os_eventlink_t;
#else
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_OBJECT_DECL_CLASS(os_eventlink);
#endif

/*!
 * @function os_eventlink_create
 *
 * @abstract
 * Creates an inactive refcounted os_object representing an os_eventlink_t.
 *
 * This function creates only 1 endpoint of an eventlink object.  The other
 * endpoint of the eventlink needs to be created from this eventlink object
 * using one of the other creator functions -
 * os_eventlink_create_remote_with_eventlink() or
 * os_eventlink_create_with_port()
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED
os_eventlink_t _Nullable
os_eventlink_create(const char *name);

#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)

/* TODO: API for the future when we make a variant of eventlink that does
 * copyin */

/*!
 * @typedef os_eventlink_shared_data_t
 *
 * @abstract
 * Pointer to an opaque structure identifying the data that is used to
 * synchronize between the two endpoints of an eventlink.
 *
 * It is the client's responsibility to allocate this structure such that both
 * threads on the two endpoints of the eventlink can synchronize with it ie. If
 * the eventlink is between 2 threads in 2 processes, os_eventlink_shared_data_t
 * should be allocated in shared memory between the two processes.
 */
typedef struct os_eventlink_shared_data_s {
	uint64_t local_count;
	uint64_t remote_count;
} os_eventlink_shared_data_s, *os_eventlink_shared_data_t;
#define OS_EVENTLINK_SHARED_DATA_INITIALIZER { 0 }

/*!
 * @function os_eventlink_set_shared_data
 *
 * @abstract
 * Associates a shared data structure with the os_eventlink.
 *
 * As a performance enhancement, clients may choose to provide an opaque shared
 * data structure in memory visible to both ends of the eventlink based on the
 * usage pattern of the os eventlink.
 *
 * Passing in NULL for shared data is recommended if the eventlink is to be used
 * for the typical RPC ping-pong case whereby one side of the eventlink is
 * always blocked waiting on a signal from the other side. In this case, each
 * signal causes a single wakeup.
 *
 * Passing in shared data is recommended when one side of the eventlink is not
 * necessarily always waiting for the other's signal in order to work. Passing
 * in the shared data allows for more efficient signalling - potentially without
 * any system calls.
 */
int
os_eventlink_set_shared_data(os_eventlink_t eventlink,
		os_eventlink_shared_data_t data);

#endif

/*!
 * @function os_eventlink_activate
 *
 * @abstract
 * Activates the os_eventlink object for use. No further configuration can be
 * done on the eventlink object after it has been activated. This API is not
 * real-time safe.
 *
 * If an error is encountered, errno is set and returned.
 */
OS_EXPORT OS_OBJECT_WARN_UNUSED_RESULT
int
os_eventlink_activate(os_eventlink_t eventlink);

/*!
 * @function os_eventlink_extract_remote_port
 *
 * @abstract
 * Returns a reference to a send right representing the remote endpoint of the
 * eventlink. This port is to be passed to os_eventlink_create_with_port() to
 * create an eventlink object.
 *
 * Calling this function multiple times on an eventlink object will result in an
 * error.
 *
 * @param eventlink
 * An eventlink returns from a previous call to os_eventlink_create(). This
 * evenlink must have been activated.
 */
OS_EXPORT OS_OBJECT_WARN_UNUSED_RESULT
int
os_eventlink_extract_remote_port(os_eventlink_t eventlink, mach_port_t *port_out);

/*!
 * @function os_eventlink_create_with_port
 *
 * @abstract
 * Creates an inactive eventlink from a port returned from a previous call to
 * os_eventlink_extract_remote_port. This function does not consume a reference
 * on the specified send right.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED
os_eventlink_t _Nullable
os_eventlink_create_with_port(const char *name, mach_port_t mach_port);

/*!
 * @function os_eventlink_create_remote_with_eventlink
 *
 * @abstract
 * Creates an inactive refcounted os_object representing an os_eventlink_t
 * remote endpoint. Each eventlink has exactly one remote endpoint that can be
 * created from it. Calling this function on an eventlink object returned from
 * os_eventlink_create(), more than once will return in an error.
 *
 * @param eventlink
 * An eventlink returned from a previous call to os_eventlink_create(). This
 * eventlink must have been activated.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED
os_eventlink_t _Nullable
os_eventlink_create_remote_with_eventlink(const char *name, os_eventlink_t eventlink);

/*!
 * @function os_eventlink_associate
 *
 * @abstract
 * Associate a thread with the eventlink endpoint provided. The eventlink
 * provided should be activated before this call. This API is not real
 * time safe.
 *
 * If a thread is already associated with the eventlink, errno is set and
 * returned.
 */

OS_ENUM(os_eventlink_associate_options, uint64_t,
	OE_ASSOCIATE_CURRENT_THREAD = 0,
	OE_ASSOCIATE_ON_WAIT = 0x1,
);

OS_EXPORT OS_OBJECT_WARN_UNUSED_RESULT
int
os_eventlink_associate(os_eventlink_t eventlink,
		os_eventlink_associate_options_t options);

/*!
 * @function os_eventlink_disassociate
 *
 * @abstract
 * Disassociate the current thread with the eventlink endpoint provided. This
 * API is not real time safe.
 *
 * If the current thread is not associated with the eventlink via a previous
 * call to os_eventlink_associate, errno is set and returned.
 */
OS_EXPORT
int
os_eventlink_disassociate(os_eventlink_t eventlink);

/*!
 * @function os_eventlink_wait
 *
 * @abstract
 * Wait on the eventlink endpoint for a signal from the other endpoint. If there
 * are outstanding signals, this function will consume them and return
 * immediately.
 *
 * Upon receiving a signal, the function returns the number of signals that have
 * been consumed by the waiter in the out parameter if specified.
 *
 * If the eventlink has not been previously associated via a call to
 * os_eventlink_associate or if there is a mismatch between the associated
 * thread and the current thread, the process will abort. This API call is
 * real-time safe.
 */
OS_EXPORT
int
os_eventlink_wait(os_eventlink_t eventlink, uint64_t * _Nullable signals_consumed_out);

/*!
 * @function os_eventlink_wait_until
 *
 * @abstract
 * Wait on the eventlink endpoint for a signal or until the timeout specified is
 * hit. If there are outstanding signals, this function will consume them and
 * return immediately.
 *
 * Upon success, the function returns the number of signals that have been
 * consumed by the waiter in the out parameter, if provided. If the timeout is
 * hit, then 0 signals are said to have been consumed by the waiter. This API
 * call is real time safe.
 */
OS_EXPORT
int
os_eventlink_wait_until(os_eventlink_t eventlink, os_clockid_t clock,
		uint64_t timeout, uint64_t * _Nullable signals_consumed_out);

/*!
 * @function os_eventlink_signal
 *
 * @abstract
 * Signal the other endpoint of an eventlink. This API call is real time safe.
 *
 * If an error is encountered, errno will be set and returned.
 */
OS_EXPORT
int
os_eventlink_signal(os_eventlink_t eventlink);

/*!
 * @function os_eventlink_signal_and_wait
 *
 * @abstract
 * Signals on an eventlink endpoint and then proceeds to wait on it until the
 * eventlink is signalled. Returns the number of signals consumed by the waiter
 * through the out parameter if provided. This API call is real time safe.
 */
OS_EXPORT
int
os_eventlink_signal_and_wait(os_eventlink_t eventlink, uint64_t * _Nullable signals_consumed_out);

/*!
 * @function os_eventlink_signal_and_wait_until
 *
 * @abstract
 * Signals on an eventlink endpoint and then proceeds to wait on it until the
 * evenlink is signalled or the timeout is hit.  Returns the number of signals
 * consumed by the waiter through the out parameter if provided, with 0
 * indicating that a timeout has been hit. This API call is real time safe.
 */
OS_EXPORT
int
os_eventlink_signal_and_wait_until(os_eventlink_t eventlink, os_clockid_t clock,
		uint64_t timeout, uint64_t * _Nullable signals_consumed_out);

/*
 * @function os_eventlink_cancel
 *
 * @abstract
 * Invalidates an eventlink. The only follow up actions possible on the eventlink
 * after it has been invalidated, are to disassociate from the eventlink and
 * dispose of it.
 *
 * If the eventlink had a remote endpoint created, the remote side will get an
 * ECANCELED when it tries to wait or signal on it. Existing waiters on the
 * eventlink will get the same result as well. The only valid follow up
 * actions possible on a remote endpoint are to disassociate from the eventlink
 * and dispose of it.
 *
 * This API is idempotent. It is not required to call this API before dropping
 * the last reference count of an eventlink.
 */
OS_EXPORT
void
os_eventlink_cancel(os_eventlink_t eventlink);

OS_OBJECT_ASSUME_NONNULL_END

__END_DECLS

#endif /* __OS_EVENTLINK__ */
