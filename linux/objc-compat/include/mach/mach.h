// mach/mach.h for Argon on Linux: types only, there is no Mach kernel underneath
#pragma once
#include <mach/mach_types.h>
#include <mach/message.h>

// port names are handed out so callers that only hold a port can start, nothing ever arrives on them
#define MACH_PORT_RIGHT_SEND 0
#define MACH_PORT_RIGHT_RECEIVE 1
typedef natural_t mach_port_right_t;
mach_port_t mach_task_self(void);
kern_return_t mach_port_allocate(task_t task, mach_port_right_t right, mach_port_name_t *name);
kern_return_t mach_port_insert_right(task_t task, mach_port_name_t name, mach_port_t poly, natural_t type);
kern_return_t mach_port_deallocate(task_t task, mach_port_name_t name);
kern_return_t mach_port_mod_refs(task_t task, mach_port_name_t name, mach_port_right_t right, int delta);

// host_statistics64's VM counters, from /proc/meminfo in pages
typedef natural_t mach_msg_type_number_t;
typedef mach_port_t host_t;
typedef integer_t *host_info64_t;
typedef struct vm_statistics64 {
    uint64_t free_count, active_count, inactive_count, wire_count, compressor_page_count, speculative_count;
} vm_statistics64_data_t;
#define HOST_VM_INFO64 4
#define HOST_VM_INFO64_COUNT ((mach_msg_type_number_t)(sizeof(vm_statistics64_data_t) / sizeof(integer_t)))
host_t mach_host_self(void);
kern_return_t host_statistics64(host_t host, int flavor, host_info64_t info, mach_msg_type_number_t *count);
