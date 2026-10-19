// SPDX-License-Identifier: MIT
// the messages between IOSurface.framework and iosurfaced, the broker for global surface IDs
#ifndef IOSURFACED_H
#define IOSURFACED_H

#include <mach/mach.h>

#define IOSURFACED_SERVICE "org.puredarwin.iosurfaced"

enum {
    IOSURFACED_REGISTER = 0x494f5301,
    IOSURFACED_LOOKUP = 0x494f5302,
};

// a send right to the memory entry, and a token whose receive right the creator holds
struct iosurfaced_register_request {
    mach_msg_header_t head;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t token;
    mach_msg_port_descriptor_t entry;
};

struct iosurfaced_reply {
    mach_msg_header_t head;
    kern_return_t result;
    uint32_t id;
    mach_msg_trailer_t trailer;
};

struct iosurfaced_lookup_request {
    mach_msg_header_t head;
    uint32_t id;
};

// complex with one port when the ID is known, a plain header otherwise
struct iosurfaced_lookup_reply {
    mach_msg_header_t head;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t entry;
    mach_msg_trailer_t trailer;
};

#endif
