/*
 * sandbox_profile.h - compiled sandbox profile shared by libsandbox and Sandbox.kext.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#ifndef PUREDARWIN_SANDBOX_PROFILE_H
#define PUREDARWIN_SANDBOX_PROFILE_H

#include <sys/types.h>

/* __mac_syscall(SB_POLICY_NAME, SB_CALL_SET_PROFILE, struct sb_set_profile_args *) */
#define SB_POLICY_NAME        "Sandbox"
#define SB_CALL_SET_PROFILE   0

#define SB_PROFILE_MAGIC      0x42535044 /* "PDSB" */
#define SB_PROFILE_VERSION    1
#define SB_PROFILE_MAX_SIZE   (4 * 1024 * 1024)

/* Operations a rule covers. */
#define SB_OP_FILE_READ_DATA      0x0001
#define SB_OP_FILE_READ_METADATA  0x0002
#define SB_OP_FILE_WRITE_DATA     0x0004
#define SB_OP_FILE_WRITE_CREATE   0x0008
#define SB_OP_FILE_WRITE_UNLINK   0x0010
#define SB_OP_FILE_WRITE_SETUGID  0x0020
#define SB_OP_FILE_WRITE_ATTR     0x0040 /* mode, owner, flags, times, xattrs, ACLs */
#define SB_OP_PROCESS_EXEC        0x0080
#define SB_OP_NETWORK_OUTBOUND    0x0100
#define SB_OP_NETWORK_INBOUND     0x0200

#define SB_OP_FILE_READ_ALL   (SB_OP_FILE_READ_DATA | SB_OP_FILE_READ_METADATA)
#define SB_OP_FILE_WRITE_ALL  (SB_OP_FILE_WRITE_DATA | SB_OP_FILE_WRITE_CREATE | \
                               SB_OP_FILE_WRITE_UNLINK | SB_OP_FILE_WRITE_SETUGID | \
                               SB_OP_FILE_WRITE_ATTR)
#define SB_OP_FILE_ALL        (SB_OP_FILE_READ_ALL | SB_OP_FILE_WRITE_ALL)
#define SB_OP_NETWORK_ALL     (SB_OP_NETWORK_OUTBOUND | SB_OP_NETWORK_INBOUND)
#define SB_OP_ALL             (SB_OP_FILE_ALL | SB_OP_PROCESS_EXEC | SB_OP_NETWORK_ALL)

/* What a rule matches. Path filters also match AF_UNIX socket paths. */
#define SB_FILTER_ANY          0
#define SB_FILTER_LITERAL      1 /* path is exactly the string */
#define SB_FILTER_SUBPATH      2 /* path is the string or below it */
#define SB_FILTER_REGEX        3 /* path matches: ^ $ . [] * + ? and \ escapes */
#define SB_FILTER_IP           4 /* any AF_INET or AF_INET6 address */
#define SB_FILTER_IP_LOCALHOST 5 /* a loopback address */
#define SB_FILTER_UNIX_LITERAL 6 /* AF_UNIX socket path is exactly the string */
#define SB_FILTER_MAX          SB_FILTER_UNIX_LITERAL

#define SB_ACTION_ALLOW 0
#define SB_ACTION_DENY  1

/*
 * Layout: header, rule_count rules, then strings_size bytes of NUL-terminated
 * strings. Rules are checked in order and the last one that matches decides.
 */
struct sb_profile_header {
	uint32_t magic;
	uint32_t version;
	uint32_t default_action;
	uint32_t rule_count;
	uint32_t strings_size;
	uint32_t reserved;
};

struct sb_rule {
	uint32_t ops;
	uint8_t  action;
	uint8_t  filter;
	uint16_t reserved;
	uint32_t str_offset;
	uint32_t str_length;
};

struct sb_set_profile_args {
	uint64_t profile;
	uint64_t size;
};

#endif /* PUREDARWIN_SANDBOX_PROFILE_H */
