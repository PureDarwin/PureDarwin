/*
 * <os/activity.h> for PureDarwin.
 *
 * Activities give os_log entries a causal chain across threads and processes;
 * that chain is only ever consumed by logd/Instruments. The implementation in
 * libsystem_trace/activity.c is therefore a set of no-ops, and this header
 * declares the same API callers are compiled against elsewhere so that
 * os_activity_create()/os_activity_scope() keep working unchanged.
 */
#ifndef __os_activity_h
#define __os_activity_h

#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* The image's own Mach-O header, which the linker synthesises. Spelled the same
 * way <os/log.h> already declares it, so the two agree. */
extern void *__dso_handle;

typedef struct os_activity_s *os_activity_t;
typedef uint64_t os_activity_id_t;
typedef uint32_t os_activity_flag_t;

#define OS_ACTIVITY_FLAG_DEFAULT           0x00
#define OS_ACTIVITY_FLAG_DETACHED          0x01
#define OS_ACTIVITY_FLAG_IF_NONE_PRESENT   0x02

typedef struct os_activity_scope_state_s {
	uint64_t opaque[2];
} *os_activity_scope_state_t;

typedef void (^os_block_t)(void);
typedef void (*os_function_t)(void *);

struct os_activity_s { uint64_t opaque[2]; };

extern struct os_activity_s _os_activity_current;
extern struct os_activity_s _os_activity_none;

#define OS_ACTIVITY_CURRENT ((os_activity_t)&_os_activity_current)
#define OS_ACTIVITY_NONE    ((os_activity_t)&_os_activity_none)

os_activity_t _os_activity_create(void *dso, const char *description,
		os_activity_t activity, os_activity_flag_t flags);
void _os_activity_initiate(void *dso, const char *description,
		os_activity_flag_t flags, os_block_t activity_block);
void _os_activity_initiate_f(void *dso, const char *description,
		os_activity_flag_t flags, void *context, os_function_t function);
void _os_activity_label_useraction(void *dso, const char *name);
void os_activity_scope_enter(os_activity_t activity,
		os_activity_scope_state_t state);
void os_activity_scope_leave(os_activity_scope_state_t state);
os_activity_id_t os_activity_get_identifier(os_activity_t activity,
		os_activity_id_t *parent_id);

#define os_activity_create(description, parent_activity, flags) \
	_os_activity_create(&__dso_handle, description, parent_activity, flags)

#define os_activity_initiate(description, flags, block) \
	_os_activity_initiate(&__dso_handle, description, flags, block)

#define os_activity_initiate_f(description, flags, context, function) \
	_os_activity_initiate_f(&__dso_handle, description, flags, context, function)

#define os_activity_label_useraction(name) \
	_os_activity_label_useraction(&__dso_handle, name)

/* The cleanup attribute hands the function a pointer to the variable, so the
 * scope state is the struct itself rather than the typedef'd pointer. */
__attribute__((always_inline))
static inline void
_os_activity_scope_leave_cleanup(struct os_activity_scope_state_s *state)
{
	os_activity_scope_leave(state);
}

#define os_activity_scope(activity) \
	struct os_activity_scope_state_s __os_activity_scope_state \
		__attribute__((cleanup(_os_activity_scope_leave_cleanup), unused)); \
	os_activity_scope_enter((activity), &__os_activity_scope_state)

__END_DECLS

#endif /* __os_activity_h */
