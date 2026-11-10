// mach/message.h for Argon on Linux: the message layouts, which only appear as struct fields there
#pragma once
#include <mach/mach_types.h>
typedef natural_t mach_msg_size_t;
typedef natural_t mach_msg_bits_t;
typedef integer_t mach_msg_id_t;
typedef natural_t mach_msg_type_name_t;
typedef natural_t mach_msg_descriptor_type_t;
typedef natural_t mach_msg_return_t;
typedef struct {
    mach_msg_bits_t msgh_bits;
    mach_msg_size_t msgh_size;
    mach_port_t msgh_remote_port;
    mach_port_t msgh_local_port;
    mach_port_name_t msgh_voucher_port;
    mach_msg_id_t msgh_id;
} mach_msg_header_t;
typedef struct {
    mach_port_t name;
    mach_msg_size_t pad1;
    unsigned int pad2 : 16;
    mach_msg_type_name_t disposition : 8;
    mach_msg_descriptor_type_t type : 8;
} mach_msg_port_descriptor_t;

// message options and results; Argon on Linux has no Mach services, so sends find no receiver
#define MACH_SEND_MSG 0x00000001
#define MACH_RCV_MSG 0x00000002
#define MACH_SEND_TIMEOUT 0x00000010
#define MACH_RCV_TIMEOUT 0x00000100
#define MACH_MSG_TIMEOUT_NONE 0
#define MACH_MSG_SUCCESS 0x00000000
#define MACH_SEND_INVALID_DEST 0x10000003
#define MACH_RCV_TIMED_OUT 0x10004003
#define MACH_MSG_TYPE_MOVE_RECEIVE 16
#define MACH_MSG_TYPE_MOVE_SEND 17
#define MACH_MSG_TYPE_COPY_SEND 19
#define MACH_MSG_TYPE_MAKE_SEND 20
#define MACH_MSG_PORT_DESCRIPTOR 0
#define MACH_MSGH_BITS_COMPLEX 0x80000000U
#define MACH_MSGH_BITS_SET(remote, local, voucher, other) \
    ((remote) | ((local) << 8) | ((voucher) << 16) | ((other) & ~0x001f1f1fU))
typedef natural_t mach_msg_option_t;
typedef natural_t mach_msg_timeout_t;
mach_msg_return_t mach_msg(mach_msg_header_t *msg, mach_msg_option_t option, mach_msg_size_t send_size,
                           mach_msg_size_t rcv_size, mach_port_name_t rcv_name, mach_msg_timeout_t timeout,
                           mach_port_name_t notify);
