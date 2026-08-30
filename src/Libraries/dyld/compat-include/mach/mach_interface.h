#ifndef PUREDARWIN_DYLD_COMPAT_MACH_INTERFACE_H
#define PUREDARWIN_DYLD_COMPAT_MACH_INTERFACE_H

#include "../../../libSystem/libsystem_kernel/mach/mach/mach_interface.h"

/* The pinned SDK omits these MIG-generated declarations. */
#include <mach/mach_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern kern_return_t vm_allocate(vm_map_t, vm_address_t *, vm_size_t, int);
extern kern_return_t vm_deallocate(vm_map_t, vm_address_t, vm_size_t);
extern kern_return_t vm_protect(vm_map_t, vm_address_t, vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);

#ifdef __cplusplus
}
#endif

#endif /* PUREDARWIN_DYLD_COMPAT_MACH_INTERFACE_H */
