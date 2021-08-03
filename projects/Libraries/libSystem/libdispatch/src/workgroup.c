#include "internal.h"

#include <os/assumes.h>
#include <mach/mach_port.h>

/* Declares struct symbols */
OS_OBJECT_CLASS_DECL(os_workgroup);
#if !USE_OBJC
OS_OBJECT_VTABLE_INSTANCE(os_workgroup,
		(void (*)(_os_object_t))_os_workgroup_xref_dispose,
		(void (*)(_os_object_t))_os_workgroup_dispose);
#endif // USE_OBJC
#define WORKGROUP_CLASS OS_OBJECT_VTABLE(os_workgroup)

OS_OBJECT_CLASS_DECL(os_workgroup_interval);
#if !USE_OBJC
OS_OBJECT_VTABLE_INSTANCE(os_workgroup_interval,
		(void (*)(_os_object_t))_os_workgroup_interval_xref_dispose,
		(void (*)(_os_object_t))_os_workgroup_interval_dispose);
#endif // USE_OBJC
#define WORKGROUP_INTERVAL_CLASS OS_OBJECT_VTABLE(os_workgroup_interval)

OS_OBJECT_CLASS_DECL(os_workgroup_parallel);
#if !USE_OBJC
OS_OBJECT_VTABLE_INSTANCE(os_workgroup_parallel,
		(void (*)(_os_object_t))_os_workgroup_xref_dispose,
		(void (*)(_os_object_t))_os_workgroup_dispose);
#endif // USE_OBJC
#define WORKGROUP_PARALLEL_CLASS OS_OBJECT_VTABLE(os_workgroup_parallel)

#pragma mark Internal functions

/* These are default workgroup attributes to be used when no user attribute is
 * passed in in creation APIs.
 *
 * For all classes, workgroup propagation is currently not supported.
 *
 * Class						Default attribute			Eventually supported
 *
 * os_workgroup_t				propagating					nonpropagating, propagating
 * os_workgroup_interval_t		nonpropagating				nonpropagating, propagating
 * os_workgroup_parallel_t		nonpropagating				nonpropagating
 *
 * Class						Default attribute			supported
 * os_workgroup_t				differentiated				differentiated, undifferentiated
 * os_workgroup_interval_t		differentiated				differentiated
 * os_workgroup_parallel_t		undifferentiated			undifferentiated, differentiated
 */
static const struct os_workgroup_attr_s _os_workgroup_attr_default = {
	.sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT,
	.wg_type = OS_WORKGROUP_TYPE_DEFAULT,
	.wg_attr_flags = 0,
};

static const struct os_workgroup_attr_s _os_workgroup_interval_attr_default = {
	.sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT,
	.wg_type = OS_WORKGROUP_INTERVAL_TYPE_DEFAULT,
	.wg_attr_flags = OS_WORKGROUP_ATTR_NONPROPAGATING
};

static const struct os_workgroup_attr_s _os_workgroup_parallel_attr_default = {
	.sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT,
	.wg_type = OS_WORKGROUP_TYPE_PARALLEL,
	.wg_attr_flags = OS_WORKGROUP_ATTR_NONPROPAGATING |
		OS_WORKGROUP_ATTR_UNDIFFERENTIATED,
};

void
_os_workgroup_xref_dispose(os_workgroup_t wg)
{
	os_workgroup_arena_t arena = wg->wg_arena;

	if (arena == NULL) {
		return;
	}

	arena->destructor(arena->client_arena);
	free(arena);
}

void
_os_workgroup_interval_xref_dispose(os_workgroup_interval_t wgi)
{
	uint64_t wg_state = wgi->wg_state;
	if (wg_state & OS_WORKGROUP_INTERVAL_STARTED) {
		os_crash("BUG IN CLIENT: Releasing last reference to workgroup interval "
			"while an interval has been started");
	}
}

static inline bool
_os_workgroup_is_configurable(uint64_t wg_state)
{
	return (wg_state & OS_WORKGROUP_OWNER) == OS_WORKGROUP_OWNER;
}

void
_os_workgroup_dispose(os_workgroup_t wg)
{
	dispatch_assert(wg->joined_cnt == 0);

	kern_return_t kr;
	uint64_t wg_state = os_atomic_load(&wg->wg_state, relaxed);
	if (_os_workgroup_is_configurable(wg_state)) {
		kr = work_interval_destroy(wg->wi);
	} else {
		kr = mach_port_mod_refs(mach_task_self(), wg->port, MACH_PORT_RIGHT_SEND, -1);
	}
	os_assumes(kr == KERN_SUCCESS);
	if (wg_state & OS_WORKGROUP_LABEL_NEEDS_FREE) {
		free((void *)wg->name);
	}
}

void
_os_workgroup_debug(os_workgroup_t wg, char *buf, size_t size)
{
	snprintf(buf, size, "wg[%p] = {xref = %d, ref = %d, name = %s}",
			(void *) wg, wg->do_xref_cnt + 1, wg->do_ref_cnt + 1, wg->name);
}

void
_os_workgroup_interval_dispose(os_workgroup_interval_t wgi)
{
	work_interval_instance_free(wgi->wii);
}

#define os_workgroup_inc_refcount(wg)  \
	_os_object_retain_internal(wg->_as_os_obj);

#define os_workgroup_dec_refcount(wg)  \
	_os_object_release_internal(wg->_as_os_obj);

void
_os_workgroup_tsd_cleanup(void *ctxt) /* Destructor for the tsd key */
{
	os_workgroup_t wg = (os_workgroup_t) ctxt;
	if (wg != NULL) {
		char buf[512];
		snprintf(buf, sizeof(buf), "BUG IN CLIENT: Thread exiting without leaving workgroup '%s'", wg->name);

		os_crash(buf);
	}
}

static os_workgroup_t
_os_workgroup_get_current(void)
{
	return (os_workgroup_t) pthread_getspecific(_os_workgroup_key);
}

static void
_os_workgroup_set_current(os_workgroup_t new_wg)
{
	if (new_wg != NULL) {
		os_workgroup_inc_refcount(new_wg);
	}

	os_workgroup_t old_wg = _os_workgroup_get_current();
	pthread_setspecific(_os_workgroup_key, new_wg);

	if (old_wg != NULL) {
		os_workgroup_dec_refcount(old_wg);
	}
}

static inline bool
_os_workgroup_attr_is_resolved(os_workgroup_attr_t attr)
{
	return (attr->sig == _OS_WORKGROUP_ATTR_RESOLVED_INIT);
}

static inline bool
_os_workgroup_client_attr_initialized(os_workgroup_attr_t attr)
{
	return (attr->sig == _OS_WORKGROUP_ATTR_SIG_DEFAULT_INIT) ||
			(attr->sig == _OS_WORKGROUP_ATTR_SIG_EMPTY_INIT);
}

static inline bool
_os_workgroup_attr_is_propagating(os_workgroup_attr_t attr)
{
	return (attr->wg_attr_flags & OS_WORKGROUP_ATTR_NONPROPAGATING) == 0;
}

static inline bool
_os_workgroup_attr_is_differentiated(os_workgroup_attr_t attr)
{
	return (attr->wg_attr_flags & OS_WORKGROUP_ATTR_UNDIFFERENTIATED) == 0;
}

static inline bool
_os_workgroup_type_is_interval_type(os_workgroup_type_t wg_type)
{
	return (wg_type >= OS_WORKGROUP_INTERVAL_TYPE_DEFAULT) &&
			(wg_type <= OS_WORKGROUP_INTERVAL_TYPE_COREMEDIA);
}

static bool
_os_workgroup_type_is_audio_type(os_workgroup_type_t wg_type)
{
	return (wg_type == OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO) ||
			(wg_type == OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT);
}

static inline bool
_os_workgroup_type_is_parallel_type(os_workgroup_type_t wg_type)
{
	return wg_type == OS_WORKGROUP_TYPE_PARALLEL;
}

static inline bool
_os_workgroup_type_is_default_type(os_workgroup_type_t wg_type)
{
	return wg_type == OS_WORKGROUP_TYPE_DEFAULT;
}


static inline bool
_os_workgroup_has_backing_workinterval(os_workgroup_t wg)
{
	return wg->wi != NULL;
}

#if !TARGET_OS_SIMULATOR
static os_workgroup_type_t
_wi_flags_to_wg_type(uint32_t wi_flags)
{
	uint32_t type = wi_flags & WORK_INTERVAL_TYPE_MASK;
	bool is_unrestricted = (wi_flags & WORK_INTERVAL_FLAG_UNRESTRICTED);

	switch (type) {
	case WORK_INTERVAL_TYPE_DEFAULT:
		/* Technically, this could be OS_WORKGROUP_INTERVAL_TYPE_DEFAULT
		 * as well but we can't know so we just assume it's a regular
		 * workgroup
		 */
		return OS_WORKGROUP_TYPE_DEFAULT;
	case WORK_INTERVAL_TYPE_COREAUDIO:
		return (is_unrestricted ? OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT :
				OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO);
	case WORK_INTERVAL_TYPE_COREANIMATION:
		/* and WORK_INTERVAL_TYPE_CA_RENDER_SERVER */

		/* We cannot distinguish between
		 * OS_WORKGROUP_INTERVAL_TYPE_COREANIMATION and
		 * OS_WORKGROUP_INTERVAL_TYPE_CA_RENDER_SERVER since
		 * WORK_INTERVAL_TYPE_COREANIMATION and
		 * WORK_INTERVAL_TYPE_CA_RENDER_SERVER have the same value */
		return OS_WORKGROUP_INTERVAL_TYPE_COREANIMATION;
	case WORK_INTERVAL_TYPE_HID_DELIVERY:
		return OS_WORKGROUP_INTERVAL_TYPE_HID_DELIVERY;
	case WORK_INTERVAL_TYPE_COREMEDIA:
		return OS_WORKGROUP_INTERVAL_TYPE_COREMEDIA;
	case WORK_INTERVAL_TYPE_CA_CLIENT:
		return OS_WORKGROUP_INTERVAL_TYPE_CA_CLIENT;
	default:
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "BUG IN DISPATCH: Invalid wi flags = %u", wi_flags);
		os_crash(buf);
	}
	}
}
#endif

static work_interval_t
_os_workgroup_create_work_interval(os_workgroup_attr_t attr)
{
	/* All workgroups are joinable */
	uint32_t flags = WORK_INTERVAL_FLAG_JOINABLE;

	switch (attr->wg_type) {
	case OS_WORKGROUP_INTERVAL_TYPE_DEFAULT:
		flags |= WORK_INTERVAL_TYPE_DEFAULT | WORK_INTERVAL_FLAG_UNRESTRICTED;
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO:
		flags |= (WORK_INTERVAL_TYPE_COREAUDIO |
				WORK_INTERVAL_FLAG_ENABLE_AUTO_JOIN |
				WORK_INTERVAL_FLAG_ENABLE_DEFERRED_FINISH);
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_COREANIMATION:
		flags |= WORK_INTERVAL_TYPE_COREANIMATION;
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_CA_RENDER_SERVER:
		flags |= WORK_INTERVAL_TYPE_CA_RENDER_SERVER;
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_HID_DELIVERY:
		flags |= WORK_INTERVAL_TYPE_HID_DELIVERY;
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_COREMEDIA:
		flags |= WORK_INTERVAL_TYPE_COREMEDIA;
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT:
		flags |= (WORK_INTERVAL_TYPE_COREAUDIO | WORK_INTERVAL_FLAG_UNRESTRICTED |
				WORK_INTERVAL_FLAG_ENABLE_AUTO_JOIN |
				WORK_INTERVAL_FLAG_ENABLE_DEFERRED_FINISH);
		break;
	case OS_WORKGROUP_INTERVAL_TYPE_CA_CLIENT:
		flags |= WORK_INTERVAL_TYPE_CA_CLIENT | WORK_INTERVAL_FLAG_UNRESTRICTED;
		break;
	case OS_WORKGROUP_TYPE_DEFAULT:
		/* Non-interval workgroup types */
		flags |= WORK_INTERVAL_FLAG_UNRESTRICTED;
		break;
	default:
		os_crash("Creating an os_workgroup of unknown type");
	}

	if (_os_workgroup_attr_is_differentiated(attr)) {
		flags |= WORK_INTERVAL_FLAG_GROUP;
	}

	work_interval_t wi;
	int rv = work_interval_create(&wi, flags);
	if (rv) {
		errno = rv;
		return NULL;
	}

	return wi;
}

static inline bool
_os_workgroup_join_token_initialized(os_workgroup_join_token_t token)
{
	return (token->sig == _OS_WORKGROUP_JOIN_TOKEN_SIG_INIT);
}

static inline void
_os_workgroup_set_name(os_workgroup_t wg, const char *name)
{
	if (name) {
		const char *tmp = _dispatch_strdup_if_mutable(name);
		if (tmp != name) {
			wg->wg_state |= OS_WORKGROUP_LABEL_NEEDS_FREE;
			name = tmp;
		}
	}
	wg->name = name;
}

static inline bool
_os_workgroup_client_attr_is_valid(os_workgroup_attr_t attr)
{
	return (attr && _os_workgroup_client_attr_initialized(attr));
}

static inline bool
_start_time_is_in_past(os_clockid_t clock, uint64_t start)
{
	switch (clock) {
		case OS_CLOCK_MACH_ABSOLUTE_TIME:
			return start <= mach_absolute_time();
	}
}

#pragma mark Private functions

int
os_workgroup_attr_set_interval_type(os_workgroup_attr_t attr,
		os_workgroup_interval_type_t interval_type)
{
	int ret = 0;
	if (_os_workgroup_client_attr_is_valid(attr) &&
		 _os_workgroup_type_is_interval_type(interval_type)) {
		attr->wg_type = interval_type;
	} else {
		ret = EINVAL;
	}
	return ret;
}

int
os_workgroup_attr_set_flags(os_workgroup_attr_t attr,
		os_workgroup_attr_flags_t flags)
{
	int ret = 0;
	if (_os_workgroup_client_attr_is_valid(attr)) {
		attr->wg_attr_flags = flags;
	} else {
		ret = EINVAL;
	}

	return ret;
}

os_workgroup_t
os_workgroup_interval_copy_current_4AudioToolbox(void)
{
	os_workgroup_t wg = _os_workgroup_get_current();

	if (wg) {
		if (_os_workgroup_type_is_audio_type(wg->wg_type)) {
			wg = os_retain(wg);
		} else {
			wg = NULL;
		}
	}

	return wg;
}

#pragma mark Public functions

os_workgroup_t
os_workgroup_create(const char *name, os_workgroup_attr_t attr)
{
	os_workgroup_t wg = NULL;
	work_interval_t wi = NULL;

	/* Resolve the input attributes */
	os_workgroup_attr_s wga;
	if (attr == NULL) {
		wga = _os_workgroup_attr_default;
		attr = &wga;
	} else {
		if (!_os_workgroup_client_attr_is_valid(attr)) {
			errno = EINVAL;
			return NULL;
		}

		// Make a local copy of the attr
		wga = *attr;
		attr = &wga;

		switch (attr->sig) {
			case _OS_WORKGROUP_ATTR_SIG_DEFAULT_INIT:
			{
				/* For any fields which are 0, we fill in with default values */
				if (attr->wg_attr_flags == 0) {
					attr->wg_attr_flags = _os_workgroup_attr_default.wg_attr_flags;
				}
				if (attr->wg_type == 0) {
					attr->wg_type = _os_workgroup_attr_default.wg_type;
				}
			}
			// Fallthrough
			case _OS_WORKGROUP_ATTR_SIG_EMPTY_INIT:
				break;
			default:
				errno = EINVAL;
				return NULL;
		}

		/* Mark it as resolved */
		attr->sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT;
	}

	os_assert(_os_workgroup_attr_is_resolved(attr));

	/* Do some sanity checks */
	if (!_os_workgroup_type_is_default_type(attr->wg_type)){
		errno = EINVAL;
		return NULL;
	}

	/* We don't support propagating workgroups yet */
	if (_os_workgroup_attr_is_propagating(attr)) {
		errno = ENOTSUP;
		return NULL;
	}

	wi = _os_workgroup_create_work_interval(attr);
	if (wi == NULL) {
		return NULL;
	}

	wg = (os_workgroup_t) _os_object_alloc(WORKGROUP_CLASS,
			sizeof(struct os_workgroup_s));
	wg->wi = wi;
	wg->wg_state = OS_WORKGROUP_OWNER;
	wg->wg_type = attr->wg_type;

	_os_workgroup_set_name(wg, name);

	return wg;
}

os_workgroup_interval_t
os_workgroup_interval_create(const char *name, os_clockid_t clock,
		os_workgroup_attr_t attr)
{
	os_workgroup_interval_t wgi = NULL;
	work_interval_t wi = NULL;

	/* Resolve the input attributes */
	os_workgroup_attr_s wga;
	if (attr == NULL) {
		wga = _os_workgroup_interval_attr_default;
		attr = &wga;
	} else {
		if (!_os_workgroup_client_attr_is_valid(attr)) {
			errno = EINVAL;
			return NULL;
		}

		// Make a local copy of the attr
		wga = *attr;
		attr = &wga;

		if (attr->sig == _OS_WORKGROUP_ATTR_SIG_EMPTY_INIT) {
			/* Nothing to do, the client built the attr up from scratch */
		} else if (attr->sig == _OS_WORKGROUP_ATTR_SIG_DEFAULT_INIT) {
			/* For any fields which are 0, we fill in with default values */

			if (attr->wg_attr_flags == 0) {
				attr->wg_attr_flags = _os_workgroup_interval_attr_default.wg_attr_flags;
			}
			if (attr->wg_type == 0) {
				attr->wg_type = _os_workgroup_interval_attr_default.wg_type;
			}
		} else {
			errno = EINVAL;
			return NULL;
		}

		/* Mark it as resolved */
		attr->sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT;
	}

	os_assert(_os_workgroup_attr_is_resolved(attr));

	/* Do some sanity checks */
	if (!_os_workgroup_type_is_interval_type(attr->wg_type) ||
		!_os_workgroup_attr_is_differentiated(attr)){
		errno = EINVAL;
		return NULL;
	}

	/* We don't support propagating workgroup yet */
	if (_os_workgroup_attr_is_propagating(attr)) {
		errno = ENOTSUP;
		return NULL;
	}

	wi = _os_workgroup_create_work_interval(attr);
	if (wi == NULL) {
		return NULL;
	}

	wgi = (os_workgroup_interval_t) _os_object_alloc(WORKGROUP_INTERVAL_CLASS,
			sizeof(struct os_workgroup_interval_s));
	wgi->wi = wi;
	wgi->clock = clock;
	wgi->wii = work_interval_instance_alloc(wi);
	wgi->wii_lock = OS_UNFAIR_LOCK_INIT;
	wgi->wg_type = attr->wg_type;
	wgi->wg_state = OS_WORKGROUP_OWNER;

	_os_workgroup_set_name(wgi->_as_wg, name);

	return wgi;
}

int
os_workgroup_join_self(os_workgroup_t wg, os_workgroup_join_token_t token,
		os_workgroup_index * __unused id_out)
{
	return os_workgroup_join(wg, token);
}

void
os_workgroup_leave_self(os_workgroup_t wg, os_workgroup_join_token_t token)
{
	return os_workgroup_leave(wg, token);
}

#pragma mark Public functions

os_workgroup_parallel_t
os_workgroup_parallel_create(const char *name, os_workgroup_attr_t attr)
{
	os_workgroup_parallel_t wgp = NULL;

	// Clients should only specify NULL attributes.
	os_workgroup_attr_s wga;
	if (attr == NULL) {
		wga = _os_workgroup_parallel_attr_default;
		attr = &wga;
	} else {
		// Make a local copy of the attr
		if (!_os_workgroup_client_attr_is_valid(attr)) {
			errno = EINVAL;
			return NULL;
		}

		wga = *attr;
		attr = &wga;

		switch (attr->sig) {
			case _OS_WORKGROUP_ATTR_SIG_DEFAULT_INIT:
			{
				/* For any fields which are 0, we fill in with default values */
				if (attr->wg_attr_flags == 0) {
					attr->wg_attr_flags = _os_workgroup_parallel_attr_default.wg_attr_flags;
				}
				if (attr->wg_type == 0) {
					attr->wg_type = _os_workgroup_parallel_attr_default.wg_type;
				}
			}
			// Fallthrough
			case _OS_WORKGROUP_ATTR_SIG_EMPTY_INIT:
				break;
			default:
				errno = EINVAL;
				return NULL;
		}
		/* Mark it as resolved */
		attr->sig = _OS_WORKGROUP_ATTR_RESOLVED_INIT;
	}

	os_assert(_os_workgroup_attr_is_resolved(attr));

	/* Do some sanity checks */
	if (!_os_workgroup_type_is_parallel_type(attr->wg_type)) {
		errno = EINVAL;
		return NULL;
	}

	/* We don't support propagating workgroups yet */
	if (_os_workgroup_attr_is_propagating(attr)) {
		errno = ENOTSUP;
		return NULL;
	}

	wgp = (os_workgroup_t) _os_object_alloc(WORKGROUP_PARALLEL_CLASS,
			sizeof(struct os_workgroup_parallel_s));
	wgp->wi = NULL;
	wgp->wg_state = OS_WORKGROUP_OWNER;
	wgp->wg_type = attr->wg_type;

	_os_workgroup_set_name(wgp, name);

	return wgp;
}

int
os_workgroup_copy_port(os_workgroup_t wg, mach_port_t *mach_port_out)
{
	os_assert(wg != NULL);
	os_assert(mach_port_out != NULL);

	*mach_port_out = MACH_PORT_NULL;

	uint64_t wg_state = os_atomic_load(&wg->wg_state, relaxed);
	if (wg_state & OS_WORKGROUP_CANCELED) {
		return EINVAL;
	}

	if (!_os_workgroup_has_backing_workinterval(wg)) {
		return EINVAL;
	}

	if (_os_workgroup_is_configurable(wg_state)) {
		return work_interval_copy_port(wg->wi, mach_port_out);
	}

	kern_return_t kr = mach_port_mod_refs(mach_task_self(), wg->port,
			MACH_PORT_RIGHT_SEND, 1);
	os_assumes(kr == KERN_SUCCESS);
	*mach_port_out = wg->port;
	return 0;
}

os_workgroup_t
os_workgroup_create_with_port(const char *name, mach_port_t port)
{
	if (!MACH_PORT_VALID(port)) {
		return NULL;
	}

#if !TARGET_OS_SIMULATOR
	uint32_t wi_flags = 0;
	int ret = work_interval_get_flags_from_port(port, &wi_flags);
	if (ret != 0) {
		errno = ret;
		return NULL;
	}
#endif

	os_workgroup_t wg = NULL;
	wg = (os_workgroup_t) _os_object_alloc(WORKGROUP_CLASS,
			sizeof(struct os_workgroup_s));
	_os_workgroup_set_name(wg, name);

	kern_return_t kr;
	kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
	os_assumes(kr == KERN_SUCCESS);
	wg->port = port;
#if !TARGET_OS_SIMULATOR
	wg->wg_type = _wi_flags_to_wg_type(wi_flags);
#else
	wg->wg_type = OS_WORKGROUP_TYPE_DEFAULT;
#endif

	return wg;
}

os_workgroup_t
os_workgroup_create_with_workgroup(const char *name, os_workgroup_t wg)
{
	uint64_t wg_state = os_atomic_load(&wg->wg_state, relaxed);
	if (wg_state & OS_WORKGROUP_CANCELED) {
		errno = EINVAL;
		return NULL;
	}

	os_workgroup_t new_wg = NULL;

	new_wg = (os_workgroup_t) _os_object_alloc(WORKGROUP_CLASS,
			sizeof(struct os_workgroup_s));
	_os_workgroup_set_name(new_wg, name);
	new_wg->wg_type = wg->wg_type;

	/* We intentionally don't copy the context */

	if (_os_workgroup_has_backing_workinterval(wg)) {

		kern_return_t kr;
		if (_os_workgroup_is_configurable(wg_state)) {
			kr = work_interval_copy_port(wg->wi, &new_wg->port);
		} else {
			kr = mach_port_mod_refs(mach_task_self(), wg->port, MACH_PORT_RIGHT_SEND, 1);
			new_wg->port = wg->port;
		}
		os_assumes(kr == KERN_SUCCESS);

	}

	return new_wg;
}

int
os_workgroup_max_parallel_threads(os_workgroup_t wg, os_workgroup_mpt_attr_t __unused attr)
{
	os_assert(wg != NULL);

	qos_class_t qos = QOS_CLASS_USER_INTERACTIVE;

	switch (wg->wg_type) {
	case OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO:
	case OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT:
		return pthread_time_constraint_max_parallelism(0);
	default:
		return pthread_qos_max_parallelism(qos, 0);
	}
}

int
os_workgroup_join(os_workgroup_t wg, os_workgroup_join_token_t token)
{
	os_workgroup_t cur_wg = _os_workgroup_get_current();
	if (cur_wg) {
		// We currently don't allow joining multiple workgroups at all, period
		return EALREADY;
	}

	uint64_t wg_state = os_atomic_load(&wg->wg_state, relaxed);
	if (wg_state & OS_WORKGROUP_CANCELED) {
		return EINVAL;
	}

	int rv = 0;

	if (_os_workgroup_has_backing_workinterval(wg)) {
		if (_os_workgroup_is_configurable(wg_state)) {
			rv = work_interval_join(wg->wi);
		} else {
			rv = work_interval_join_port(wg->port);
		}
	}

	if (rv) {
		errno = rv;
		return rv;
	}

	os_atomic_inc(&wg->joined_cnt, relaxed);

	bzero(token, sizeof(struct os_workgroup_join_token_s));
	token->sig = _OS_WORKGROUP_JOIN_TOKEN_SIG_INIT;

	token->thread = _dispatch_thread_port();
	token->old_wg = cur_wg; /* should be null */
	token->new_wg = wg;

	_os_workgroup_set_current(wg);
	return 0;
}

void
os_workgroup_leave(os_workgroup_t wg, os_workgroup_join_token_t token)
{
	if (!_os_workgroup_join_token_initialized(token)) {
		os_crash("Join token is corrupt");
	}

	if (token->thread != _dispatch_thread_port()) {
		os_crash("Join token provided is for a different thread");
	}

	os_workgroup_t cur_wg = _os_workgroup_get_current();
	if ((token->new_wg != cur_wg) || (cur_wg != wg)) {
		os_crash("Join token provided is for a different workgroup than the "
				"last one joined by thread");
	}
	os_assert(token->old_wg == NULL);

	if (_os_workgroup_has_backing_workinterval(wg)) {
		dispatch_assume(work_interval_leave() == 0);
	}
	uint32_t old_joined_cnt = os_atomic_dec_orig(&wg->joined_cnt, relaxed);
	if (old_joined_cnt == 0) {
		DISPATCH_INTERNAL_CRASH(0, "Joined count underflowed");
	}
	_os_workgroup_set_current(NULL);
}

int
os_workgroup_set_working_arena(os_workgroup_t wg, void * _Nullable client_arena,
		uint32_t max_workers, os_workgroup_working_arena_destructor_t destructor)
{
	size_t arena_size;
	// We overflowed, we can't allocate this
	if (os_mul_and_add_overflow(sizeof(mach_port_t), max_workers, sizeof(struct os_workgroup_arena_s), &arena_size)) {
		errno = ENOMEM;
		return errno;
	}

	os_workgroup_arena_t wg_arena = calloc(arena_size, 1);
	if (wg_arena == NULL) {
		errno = ENOMEM;
		return errno;
	}
	wg_arena->max_workers = max_workers;
	wg_arena->client_arena = client_arena;
	wg_arena->destructor = destructor;

	_os_workgroup_atomic_flags old_state, new_state;
	os_workgroup_arena_t old_arena = NULL;

	bool success = os_atomic_rmw_loop(&wg->wg_atomic_flags, old_state, new_state, relaxed, {
		if (_wg_joined_cnt(old_state) > 0) { // We can't change the arena while it is in use
			os_atomic_rmw_loop_give_up(break);
		}
		old_arena = _wg_arena(old_state);

		// Remove the old arena and put the new one in
		new_state = old_state;
		new_state &= ~OS_WORKGROUP_ARENA_MASK;
		new_state |= (uint64_t) wg_arena;
	});

	if (!success) {
		errno = EBUSY;
		free(wg_arena);
		return errno;
	}

	if (old_arena) {
		old_arena->destructor(old_arena->client_arena);
		free(old_arena);
	}

	return 0;
}

void *
os_workgroup_get_working_arena(os_workgroup_t wg, os_workgroup_index *_Nullable index_out)
{
	if (_os_workgroup_get_current() != wg) {
		os_crash("Thread is not a member of the workgroup");
	}

	/* At this point, we know that since this thread is a member of the wg, we
	 * won't have the arena replaced out from under us so we can modify it
	 * safely */
	dispatch_assert(wg->joined_cnt > 0);

	os_workgroup_arena_t arena = os_atomic_load(&wg->wg_arena, relaxed);
	if (arena == NULL) {
		return NULL;
	}

	/* if the max_workers was 0 and the client wants an index, then they will
	 * fail */
	if (index_out != NULL && arena->max_workers == 0) {
		os_crash("The arena associated with workgroup is not to be partitioned");
	}

	if (index_out) {
		/* Find the index of the current thread in the arena */
		uint32_t found_index = 0;
		bool found = false;
		for (uint32_t i = 0; i < arena->max_workers; i++) {
			if (arena->arena_indices[i] == _dispatch_thread_port()) {
				found_index = i;
				found = true;
				break;
			}
		}

		if (!found) {
			/* Current thread doesn't already have an index, give it one */
			found_index = os_atomic_inc_orig(&arena->next_worker_index, relaxed);

			if (found_index >= arena->max_workers) {
				os_crash("Exceeded the maximum number of workers who can access the arena");
			}
			arena->arena_indices[found_index] = _dispatch_thread_port();
		}

		*index_out = found_index;
	}

	return arena->client_arena;
}

void
os_workgroup_cancel(os_workgroup_t wg)
{
	os_atomic_or(&wg->wg_state, OS_WORKGROUP_CANCELED, relaxed);
}

bool
os_workgroup_testcancel(os_workgroup_t wg)
{
	return os_atomic_load(&wg->wg_state, relaxed) & OS_WORKGROUP_CANCELED;
}

int
os_workgroup_interval_start(os_workgroup_interval_t wgi, uint64_t start,
		uint64_t deadline, os_workgroup_interval_data_t __unused data)
{
	os_workgroup_t cur_wg = _os_workgroup_get_current();
	if (cur_wg != wgi->_as_wg) {
		os_crash("Thread is not a member of the workgroup");
	}

	if (deadline < start || (!_start_time_is_in_past(wgi->clock, start))) {
		return EINVAL;
	}

	bool success = os_unfair_lock_trylock(&wgi->wii_lock);
	if (!success) {
		// Someone else is concurrently in a start, update or finish method. We
		// can't make progress here
		return EBUSY;
	}

	int rv = 0;
	uint64_t old_state, new_state;
	os_atomic_rmw_loop(&wgi->wg_state, old_state, new_state, relaxed, {
		if (old_state & (OS_WORKGROUP_CANCELED | OS_WORKGROUP_INTERVAL_STARTED)) {
			rv = EINVAL;
			os_atomic_rmw_loop_give_up(break);
		}
		if (!_os_workgroup_is_configurable(old_state)) {
			rv = EPERM;
			os_atomic_rmw_loop_give_up(break);
		}
		new_state = old_state | OS_WORKGROUP_INTERVAL_STARTED;
	});

	if (rv) {
		os_unfair_lock_unlock(&wgi->wii_lock);
		return rv;
	}

	work_interval_instance_t wii = wgi->wii;
	work_interval_instance_clear(wii);

	work_interval_instance_set_start(wii, start);
	work_interval_instance_set_deadline(wii, deadline);
	rv = work_interval_instance_start(wii);
	if (rv != 0) {
		/* If we failed to start the interval in the kernel, clear the started
		 * field */
		rv = errno;
		os_atomic_and(&wgi->wg_state, ~OS_WORKGROUP_INTERVAL_STARTED, relaxed);
	}

	os_unfair_lock_unlock(&wgi->wii_lock);

	return rv;
}

int
os_workgroup_interval_update(os_workgroup_interval_t wgi, uint64_t deadline,
		os_workgroup_interval_data_t __unused data)
{
	os_workgroup_t cur_wg = _os_workgroup_get_current();
	if (cur_wg != wgi->_as_wg) {
		os_crash("Thread is not a member of the workgroup");
	}

	bool success = os_unfair_lock_trylock(&wgi->wii_lock);
	if (!success) {
		// Someone else is concurrently in a start, update or finish method. We
		// can't make progress here
		return EBUSY;
	}

	uint64_t wg_state = os_atomic_load(&wgi->wg_state, relaxed);
	if (!_os_workgroup_is_configurable(wg_state)) {
		os_unfair_lock_unlock(&wgi->wii_lock);
		return EPERM;
	}

	/* Note: We allow updating and finishing an workgroup_interval that has
	 * already started even if the workgroup has been cancelled - since
	 * cancellation happens asynchronously and doesn't care about ongoing
	 * intervals. However a subsequent new interval cannot be started */
	if (!(wg_state & OS_WORKGROUP_INTERVAL_STARTED)) {
		os_unfair_lock_unlock(&wgi->wii_lock);
		return EINVAL;
	}

	work_interval_instance_t wii = wgi->wii;
	work_interval_instance_set_deadline(wii, deadline);
	int rv = work_interval_instance_update(wii);
	if (rv != 0) {
		rv = errno;
	}

	os_unfair_lock_unlock(&wgi->wii_lock);
	return rv;
}

int
os_workgroup_interval_finish(os_workgroup_interval_t wgi,
		os_workgroup_interval_data_t __unused data)
{
	os_workgroup_t cur_wg = _os_workgroup_get_current();
	if (cur_wg != wgi->_as_wg) {
		os_crash("Thread is not a member of the workgroup");
	}

	bool success = os_unfair_lock_trylock(&wgi->wii_lock);
	if (!success) {
		// Someone else is concurrently in a start, update or finish method. We
		// can't make progress here
		return EBUSY;
	}

	uint64_t wg_state = os_atomic_load(&wgi->wg_state, relaxed);
	if (!_os_workgroup_is_configurable(wg_state)) {
		os_unfair_lock_unlock(&wgi->wii_lock);
		return EPERM;
	}
	if (!(wg_state & OS_WORKGROUP_INTERVAL_STARTED)) {
		os_unfair_lock_unlock(&wgi->wii_lock);
		return EINVAL;
	}

	work_interval_instance_t wii = wgi->wii;
	uint64_t current_finish = 0;
	switch (wgi->clock) {
		case OS_CLOCK_MACH_ABSOLUTE_TIME:
			current_finish = mach_absolute_time();
			break;
	}

	work_interval_instance_set_finish(wii, current_finish);
	int rv = work_interval_instance_finish(wii);
	if (rv != 0) {
		rv = errno;
	} else {
		/* If we succeeded in finishing, clear the started bit */
		os_atomic_and(&wgi->wg_state, ~OS_WORKGROUP_INTERVAL_STARTED, relaxed);
	}

	os_unfair_lock_unlock(&wgi->wii_lock);
	return rv;
}
