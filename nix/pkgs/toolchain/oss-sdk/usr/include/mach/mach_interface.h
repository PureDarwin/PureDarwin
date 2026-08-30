#ifndef _MACH_INTERFACE_H_
#define _MACH_INTERFACE_H_

/* The generated client headers below use types declared by mach_types.h
 * before they include their own generated type headers. */
#include <mach/mach_types.h>
extern kern_return_t vm_allocate(vm_map_t, vm_address_t *, vm_size_t, int);
extern kern_return_t vm_deallocate(vm_map_t, vm_address_t, vm_size_t);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);
/* Public client interfaces generated from XNU's Mach definitions. */
#include <mach/clock.h>
#include <mach/clock_priv.h>
#include <mach/host_priv.h>
#include <mach/host_security.h>
#include <mach/lock_set.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/processor.h>
#include <mach/processor_set.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#endif
