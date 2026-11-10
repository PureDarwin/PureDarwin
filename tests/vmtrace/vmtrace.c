// DYLD_INSERT_LIBRARIES helper: logs who deallocates large VM regions, with a backtrace
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { const void *replacement; const void *replacee; } _interpose_##_replacee \
    __attribute__((section("__DATA,__interpose"))) = { (const void *)(unsigned long)&_replacement, (const void *)(unsigned long)&_replacee };

static void trace(const char *what, unsigned long addr, unsigned long size) {
    if (size < 16384)
        return;
    void *frames[24];
    int n = backtrace(frames, 24);
    char **names = backtrace_symbols(frames, n);
    fprintf(stderr, "VMTRACE %s %lx size %lu\n", what, addr, size);
    for (int i = 1; i < n && names; i++)
        fprintf(stderr, "VMTRACE   %s\n", names[i]);
    free(names);
}

static kern_return_t my_vm_deallocate(vm_map_t task, vm_address_t addr, vm_size_t size) {
    trace("vm_deallocate", addr, size);
    return vm_deallocate(task, addr, size);
}
static kern_return_t my_mach_vm_deallocate(vm_map_t task, mach_vm_address_t addr, mach_vm_size_t size) {
    trace("mach_vm_deallocate", addr, size);
    return mach_vm_deallocate(task, addr, size);
}
static int my_munmap(void *addr, size_t size) {
    trace("munmap", (unsigned long)addr, size);
    return munmap(addr, size);
}
DYLD_INTERPOSE(my_vm_deallocate, vm_deallocate)
DYLD_INTERPOSE(my_mach_vm_deallocate, mach_vm_deallocate)
DYLD_INTERPOSE(my_munmap, munmap)

#include <malloc/malloc.h>
// a page-aligned free is either a large block or a pointer malloc does not own
static void my_free(void *p) {
    if (p != NULL && ((unsigned long)p & 0xfff) == 0) {
        size_t size = malloc_size(p);
        if (size == 0 || size >= 16384)
            trace(size == 0 ? "free-unowned" : "free", (unsigned long)p, size == 0 ? 16384 : size);
    }
    free(p);
}
DYLD_INTERPOSE(my_free, free)
