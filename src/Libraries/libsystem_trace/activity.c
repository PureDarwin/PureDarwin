/*
 * os_activity: the tracing side of libsystem_trace.
 *
 * Activities exist to give os_log entries a causal chain across threads and
 * processes, and that chain is only ever consumed by the logd/Instruments
 * infrastructure Apple ships and PureDarwin does not. Nothing about program
 * behaviour depends on them, so these are honest no-ops rather than stubs
 * standing in for something we intend to implement: a caller that creates an
 * activity, scopes to it and leaves it gets exactly the behaviour it would on
 * a system where activity tracing is turned off.
 *
 * The one contract that must hold is os_activity_scope_enter/leave pairing:
 * callers keep the state on the stack and always leave what they entered, so
 * the state is zeroed on entry to keep it deterministic.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Declared here rather than pulled from <os/activity.h>: libsystem_trace builds
 * against its own os-include/ shims, not the SDK's os/ headers. These match the
 * SDK's declarations, which is what callers are compiled against. */
typedef struct os_activity_s *os_activity_t;
typedef uint64_t os_activity_id_t;
typedef uint32_t os_activity_flag_t;
typedef struct os_activity_scope_state_s {
    uint64_t opaque[2];
} *os_activity_scope_state_t;
typedef void (^os_block_t)(void);
typedef void (*os_function_t)(void *);

struct os_activity_s { uint64_t opaque[2]; };

/* OS_ACTIVITY_CURRENT and OS_ACTIVITY_NONE resolve to the address of these,
 * so they only ever have to exist and be distinct. */
struct os_activity_s _os_activity_current;
struct os_activity_s _os_activity_none;

os_activity_t
_os_activity_create(void *dso, const char *description,
                    os_activity_t activity, os_activity_flag_t flags)
{
    (void)dso;
    (void)description;
    (void)activity;
    (void)flags;
    return (os_activity_t)&_os_activity_none;
}

void
_os_activity_initiate(void *dso, const char *description,
                      os_activity_flag_t flags, os_block_t activity_block)
{
    (void)dso;
    (void)description;
    (void)flags;
    /* The block is the caller's actual work, not instrumentation - it has to
     * run whether or not anything is tracing. */
    if (activity_block != NULL)
        activity_block();
}

void
_os_activity_initiate_f(void *dso, const char *description,
                        os_activity_flag_t flags, void *context,
                        os_function_t function)
{
    (void)dso;
    (void)description;
    (void)flags;
    if (function != NULL)
        function(context);
}

void
os_activity_scope_enter(os_activity_t activity, os_activity_scope_state_t state)
{
    (void)activity;
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

void
os_activity_scope_leave(os_activity_scope_state_t state)
{
    (void)state;
}

void
_os_activity_label_useraction(void *dso, const char *name)
{
    (void)dso;
    (void)name;
}

os_activity_id_t
os_activity_get_identifier(os_activity_t activity, os_activity_id_t *parent_id)
{
    (void)activity;
    if (parent_id != NULL)
        *parent_id = 0;
    return 0;
}
