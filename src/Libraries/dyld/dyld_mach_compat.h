#ifndef PUREDARWIN_DYLD_MACH_COMPAT_H
#define PUREDARWIN_DYLD_MACH_COMPAT_H

#include <mach/mach_types.h>

extern "C" {
extern kern_return_t vm_allocate(vm_map_t, vm_address_t *, vm_size_t, int);
extern kern_return_t vm_deallocate(vm_map_t, vm_address_t, vm_size_t);
extern kern_return_t vm_protect(vm_map_t, vm_address_t, vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);
}

#endif
