#include <sys/types.h>
#include <mach/mach.h>
#include <xpc/launchd.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <dispatch/dispatch.h>
#include "xpc_internal.h"

/* _pd_xpc_release dispatches teardown on the isa. */
OS_OBJECT_OBJC_CLASS_DECL(xpc_connection);


/*
 * One log object for the whole process, created on first use and never
 * released - see debugf() in xpc_internal.h. dispatch_once rather than a bare
 * static so concurrent first calls cannot each calloc one and leak.
 */
os_log_t
_pd_xpc_log(void)
{
	static dispatch_once_t once;
	static os_log_t log;

	dispatch_once(&once, ^{
		log = os_log_create("org.puredarwin.libxpc", "Debug");
	});
	return log;
}

_os_object_t
_pd_xpc_object_alloc(const void *cls, size_t size)
{
    struct xpc_object_header *hdr = calloc(1, sizeof(struct xpc_object_header) + size);

    if (hdr == NULL) {
        return NULL;
    }
    hdr->isa = cls;
    hdr->ref_cnt = 1;
    hdr->xref_cnt = 1;
    return (_os_object_t)hdr;
}

/*
 * Also renamed away from os_retain/os_release for the reason above: libdispatch
 * exports those too, and its versions call objc_retain/objc_release on an isa
 * these objects do not have.
 */
void *
_pd_xpc_retain(void *obj)
{
    struct xpc_object_header *hdr = obj;

    if (hdr != NULL && hdr->ref_cnt != _OS_OBJECT_GLOBAL_REFCNT) {
        atomic_fetch_add_explicit((_Atomic int *)&hdr->ref_cnt, 1, memory_order_relaxed);
    }
    return obj;
}

void
_pd_xpc_release(void *obj)
{
    struct xpc_object_header *hdr = obj;

    if (hdr == NULL) {
        return;
    }
    if (hdr->ref_cnt == _OS_OBJECT_GLOBAL_REFCNT) {
        return;
    }
    if (atomic_fetch_sub_explicit((_Atomic int *)&hdr->ref_cnt, 1, memory_order_release) == 1) {
        atomic_thread_fence(memory_order_acquire);
        if (hdr->isa == &OS_xpc_connection_class) {
            xpc_connection_destroy((struct xpc_connection *)obj);
        } else {
            xpc_object_destroy((struct xpc_object *)obj);
        }
        free(obj);
    }
}

/*
 * See xpc/private.h: with no code signing there are no entitlements to report,
 * so this correctly finds none rather than fabricating one. Callers read NULL as
 * "not entitled".
 *
 * One visible consequence: configd's IPMonitorControl server gates interface
 * rank and advisory changes on an entitlement, so those requests are refused.
 * The DNS configuration path does not go through it.
 */
/*
 * The connection a message arrived on, recorded by
 * xpc_connection_recv_message() as it dispatches to the handler. A server uses
 * this to address its reply. NULL for any object this process created itself,
 * which is what a caller should expect.
 */
xpc_connection_t
xpc_dictionary_get_remote_connection(xpc_object_t xdict)
{
    struct xpc_object *xo = xdict;

    if (xo == NULL) {
        return NULL;
    }
    return xo->xo_remote_connection;
}

xpc_object_t
xpc_connection_copy_entitlement_value(xpc_connection_t connection,
                                      const char *entitlement)
{
    (void)connection;
    (void)entitlement;
    return NULL;
}

/*
 * The uid an XPC connection's peer should be created as. launchd applies this
 * when it spawns the service; PureDarwin's launchd does not implement per-service
 * target uids, and every daemon here runs as root, so recording it would have no
 * effect. diskarbitrationd sets it on its DAAgent connection.
 */
void
xpc_connection_set_target_uid(xpc_connection_t connection, uid_t uid)
{
    (void)connection;
    (void)uid;
}
