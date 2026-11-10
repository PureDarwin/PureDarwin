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

#include <sys/sysctl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network_cmds_lib.h"

char *
clean_non_printable(char *str, const size_t len)
{
	size_t i = 0;

	if (str == NULL || len == 0) {
		return str;
	}

	for (i = 0; i < len && str[i] != 0; i++) {
		if (isprint(str[i]) == 0) {
			str[i] = '?';
		}
	}

	return str;
}

void
dump_hex(const unsigned char *ptr, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		printf("%02x", ptr[i]);
		if (i % 16 == 15) {
			printf("\n");
		} else if (i % 2 == 1) {
			printf(" ");
		}
	}
	if (i % 16 != 0) {
			printf("\n");
	}
}

uint16_t
in_cksum(uint16_t *addr, uint16_t len)
{
	int nleft = len;
	uint16_t *w = addr;
	uint16_t answer;
	uint32_t sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		sum += *(uint8_t *)w;
	}
	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

void
proc_name(pid_t pid, char *buf, size_t buf_len)
{
	int name[4];
	u_int namelen;
	size_t infolen;
	struct kinfo_proc info;

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_PID;
	name[3] = pid;
	namelen = 4;
	infolen = sizeof(info);
	if (sysctl(name, namelen, &info, &infolen, 0, 0) != 0) {
		snprintf(buf, buf_len, "");
		return;
	}
	snprintf(buf, buf_len, "%s", info.kp_proc.p_comm);
}
