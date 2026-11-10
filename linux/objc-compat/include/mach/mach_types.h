// the Mach scalar types PureDarwin's frameworks name in structs and signatures, for Argon on Linux
#pragma once
#include <stdint.h>
#include <mach/boolean.h>
typedef unsigned int natural_t;
typedef int integer_t;
typedef natural_t mach_port_t;
typedef natural_t mach_port_name_t;
typedef int kern_return_t;
typedef uintptr_t vm_address_t;
typedef uintptr_t vm_size_t;
typedef uintptr_t vm_offset_t;
typedef mach_port_t task_t;
#define MACH_PORT_NULL ((mach_port_t)0)
#define KERN_SUCCESS 0
#define KERN_INVALID_ARGUMENT 4
#define KERN_FAILURE 5
