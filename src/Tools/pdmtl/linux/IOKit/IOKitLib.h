#pragma once
// linux stand-in: the bridge user client never exists, every call fails
#include <stddef.h>
#include <stdint.h>
#include <mach/port.h>
typedef mach_port_t io_connect_t;
static inline int IOConnectCallStructMethod(io_connect_t c, uint32_t s, const void *i, size_t il, void *o, size_t *ol) { (void)c; (void)s; (void)i; (void)il; (void)o; (void)ol; return -1; }
