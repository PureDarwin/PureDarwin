// servers/bootstrap.h for Argon on Linux: launchd's Mach bootstrap server does not exist there
#pragma once
#include <mach/mach_types.h>
#define BOOTSTRAP_UNKNOWN_SERVICE 1102
extern mach_port_t bootstrap_port;
kern_return_t bootstrap_look_up(mach_port_t bp, const char *service_name, mach_port_t *sp);
kern_return_t bootstrap_check_in(mach_port_t bp, const char *service_name, mach_port_t *sp);
