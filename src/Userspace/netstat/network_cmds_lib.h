/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef network_cmds_lib_h
#define network_cmds_lib_h

/*
 * @function   clean_non_printable
 * @discussion Modifies a string to replace the non-printable ASCII characters
 *             with '?'
 * @param str  The string to be cleaned up
 * @param len  The length of the string
 * @result     Returns 'str'
 */
extern char *clean_non_printable(char *str, size_t len);

/*
 * @function   dump_hex
 * @discussion Dump hex bytes to stdout
 * @param ptr  The buffer to dump
 * @param len  The length of the string
 */
extern void dump_hex(const unsigned char *ptr, size_t len);

extern uint16_t in_cksum(uint16_t *addr, uint16_t len);

extern void proc_name(pid_t pid, char *buf, size_t buf_len);

#endif /* network_cmds_lib_h */
