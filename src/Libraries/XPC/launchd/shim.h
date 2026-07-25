#ifndef _SHIM_H_
#define _SHIM_H_

/* random defines needed for core.c to compile */
#include <sys/event.h>
#include <mach/task_policy.h>
#include <mach/host_special_ports.h> // was <sys/mach/host_special_ports.h>
#include <sys/proc_info.h>
#include <sys/kern_event.h>
#include <sys/fileport.h>
#include <spawn.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/host_priv.h>
#include <xpc/private.h>

// _sjc_ this is to get a version of malloc_size which doesn't rely on the missing sallocx()
#include <malloc/malloc.h>

// _sjc_ commented these out in favour of the values from sys/reboot.h
//#define RB_UPSDELAY (RB_PAUSE << 1)
//#define RB_SAFEBOOT (RB_PAUSE << 2)
//#define RB_UNIPROC (RB_PAUSE << 3)
//#define RB_ALTBOOT (RB_PAUSE << 4)

typedef unsigned int xpc_service_type_t;

typedef int xpc_jetsam_band_t;

#ifndef LAUNCHD_EVENT_NAME_T_DEFINED
#define LAUNCHD_EVENT_NAME_T_DEFINED
typedef char event_name_t[64];
#endif
#define vm_allocate mach_vm_allocate
#define vm_deallocate mach_vm_deallocate

#define     O_EVTONLY       0x8000          /* descriptor requested for event notifications only */

#define        _CS_DARWIN_USER_TEMP_DIR                65537
#define EBADARCH	86	/* Bad CPU type in executable */


//#define VQ_UPDATE VQ_FLAG0100
// found this value for VQ_FLAG0100 in bsd mount.h
#define VQ_UPDATE 0x0100

#define XPC_EVENT_ROUTINE_KEY_STREAM "XPC key stream"
#define XPC_EVENT_ROUTINE_KEY_TOKEN "XPC key token"
#define XPC_EVENT_ROUTINE_KEY_NAME "XPC key name"
#define XPC_EVENT_ROUTINE_KEY_ENTITLEMENTS "XPC key entitlements"
#define XPC_EVENT_ROUTINE_KEY_EVENT "XPC key event"
#define XPC_EVENT_ROUTINE_KEY_EVENTS "XPC key events"
#define XPC_EVENT_ROUTINE_KEY_FLAGS "XPC key flags"
#define XPC_EVENT_ROUTINE_KEY_PORT "XPC key port"
#define XPC_EVENT_ROUTINE_KEY_STATE "XPC key state"
#define XPC_EVENT_ROUTINE_KEY_OP "XPC key op"
#define XPC_EVENT_ROUTINE_KEY_ERROR "XPC key error"
#define XPC_SERVICE_ENTITLEMENT_ATTACH "XPC service entitlement attach"
#define XPC_SERVICE_RENDEZVOUS_TOKEN "XPC service rendezvous token"
#define XPC_PROCESS_ROUTINE_KEY_NEW_INSTANCE_PORT "XPC process key new instance port"
#define XPC_SERVICE_ENV_ATTACHED "XPC service env attached"

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x8000
#endif

#define XPC_EVENT_FLAG_ENTITLEMENTS 0x000000000001

typedef enum {
	XPC_EVENT_GET_NAME,
	XPC_EVENT_SET,
	XPC_EVENT_COPY,
	XPC_EVENT_CHECK_IN,
	XPC_EVENT_LOOK_UP,
	XPC_EVENT_PROVIDER_CHECK_IN,
	XPC_EVENT_PROVIDER_SET_STATE,
	XPC_EVENT_COPY_ENTITLEMENTS
} xpc_event_t;

#define XPC_JETSAM_BAND_SUSPENDED 10
#define XPC_JETSAM_BAND_LAST 20
#define XPC_JETSAM_PRIORITY_RESERVED 1000

#define XPC_SERVICE_TYPE_BUNDLED 0xBADDCAFE
#define XPC_SERVICE_TYPE_LAUNCHD 0xBABECAFE
#define XPC_SERVICE_TYPE_APP     0xDEADCAFE
#define XPC_SERVICE_TYPE_ENTITLEMENT_ATTACH     0x00DEAD00

/* I/O type */
#define IOPOL_TYPE_DISK	0

/* scope */
#define IOPOL_SCOPE_PROCESS   0
#define IOPOL_SCOPE_THREAD    1
#define IOPOL_SCOPE_DARWIN_BG 2

/* I/O Priority */
#define IOPOL_DEFAULT		0
#define IOPOL_IMPORTANT		1
#define IOPOL_PASSIVE		2
#define IOPOL_THROTTLE		3
#define IOPOL_UTILITY		4
#define IOPOL_STANDARD		5

/* compatibility with older names */
#define IOPOL_APPLICATION       IOPOL_STANDARD
#define IOPOL_NORMAL            IOPOL_IMPORTANT

int      setiopolicy_np(int, int, int);

// _sjc_ because sallocx() isn't available on osx but malloc_size() is
//extern size_t sallocx(void *, int);
//static inline size_t
//malloc_size(void * ptr)
//{
//
//    return (sallocx(ptr, 0));
//}

/* domain.defs */
kern_return_t
xpc_domain_get_service_name(job_t j, event_name_t name);
kern_return_t
xpc_domain_load_services(job_t j, vm_offset_t services_buff, mach_msg_type_number_t services_sz);
kern_return_t
xpc_domain_check_in(job_t j, mach_port_t *bsport, mach_port_t *sbsport,
	mach_port_t *excport, mach_port_t *asport, uint32_t *uid, uint32_t *gid,
					int32_t *asid, vm_offset_t *ctx, mach_msg_type_number_t *ctx_sz);
kern_return_t
xpc_domain_set_environment(job_t j, mach_port_t rp, mach_port_t bsport, mach_port_t excport, vm_offset_t ctx, mach_msg_type_number_t ctx_sz);
kern_return_t
xpc_domain_import2(job_t j, mach_port_t reqport, mach_port_t dport);
kern_return_t
xpc_domain_add_services(job_t j, vm_offset_t services_buff, mach_msg_type_number_t services_sz);
#define XPC_LPI_VERSION 20141120


__inline int
posix_spawnattr_setprocesstype_np(posix_spawnattr_t * a __unused, const int b __unused)
{
	return 0;
}

__inline int
posix_spawnattr_set_importancewatch_port_np(posix_spawnattr_t * __restrict attr __unused,
											int count __unused, mach_port_t portarray[] __unused)
{
	return 0;
}

__inline int
posix_spawnattr_setcpumonitor_default(posix_spawnattr_t * __restrict a __unused)
{
	return 0;
}

#endif
