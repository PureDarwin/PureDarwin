// SPDX-License-Identifier: MIT
// haiku's network buffer module, only the name and an empty module
// the boundary copies straight into xnu packets
#ifndef _PD_HAIKU_NET_BUFFER_H
#define _PD_HAIKU_NET_BUFFER_H
#include <module.h>
#define NET_BUFFER_MODULE_NAME	"network/stack/buffer/v1"
typedef struct net_buffer net_buffer;
struct net_buffer_module_info {
	module_info	info;
};
#endif
