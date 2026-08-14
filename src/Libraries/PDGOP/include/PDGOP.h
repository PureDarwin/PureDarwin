#ifndef _PD_GOP_H
#define _PD_GOP_H

#include <mach/kern_return.h>
#include <stdint.h>

__BEGIN_DECLS

typedef unsigned int mach_port_t;
typedef uint64_t mach_vm_address_t;
typedef uint64_t mach_vm_size_t;

typedef struct PDGOPFramebuffer {
    mach_port_t masterPort;
    mach_port_t service;
    mach_port_t connect;
    mach_vm_address_t address;
    mach_vm_size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
    uint32_t pixelType;
    uint32_t componentMasks[3];
} PDGOPFramebuffer;

kern_return_t PDGOPOpen(PDGOPFramebuffer *fb);
void PDGOPClose(PDGOPFramebuffer *fb);
kern_return_t PDGOPPresent(PDGOPFramebuffer *fb,
                           uint32_t x, uint32_t y,
                           uint32_t width, uint32_t height);
const char *PDGOPLastErrorStage(void);

__END_DECLS

#endif /* _PD_GOP_H */
