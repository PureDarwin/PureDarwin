/*
 * Copyright (c) 2022-2024 Apple Inc. All rights reserved.
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
#include <net/bpf.h>
#include <net/if.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "netstat.h"


static void
bd_flags(struct xbpf_d *bd, char *flagbuf, size_t len)
{
	snprintf(flagbuf, len, "%c%c%c%c%c%c%c%c%c%c%c%c",
		 bd->bd_promisc ? 'p' : '-',
		 bd->bd_immediate ? 'i' : '-',
		 bd->bd_hdrcmplt ? '-' : 'f',
		 bd->bd_async ? 'a' : '-',
		 (bd->bd_direction & BPF_D_IN) ? 'I' : '-',
		 (bd->bd_direction & BPF_D_OUT) ? 'O' : '-',
		 bd->bd_headdrop ? 'h' : '-',
		 bd->bh_compreq ? (bd->bh_compenabled ? 'C' : 'c') : '-',
		 bd->bd_exthdr ? 'x' : '-',
		 bd->bd_trunc ? 't' : '-',
		 bd->bd_pkthdrv2 ? '2' : '-',
#ifdef BIOCGDVRTIN
		 bd->bd_divert_in ? 'D' : '-'
#else /* BIOCGDVRTIN */
		 '-'
#endif /* BIOCGDVRTIN */
		 );
}

int
bpf_stats(struct netstat_parameters *params, char *interface)
{
	size_t len;
	void *buffer = NULL;
	struct xbpf_d *bd;
	int retval = 0;

	if (sysctlbyname("debug.bpf_stats", NULL, &len, NULL, 0) != 0) {
		warn("sysctlbyname debug.bpf_stats");
		retval = -1;
		goto done;
	}
	if (len == 0) {
		goto done;
	}
	buffer = malloc(2 * len);
	if (buffer == NULL) {
		warn("malloc");
		retval = -1;
		goto done;
	}
	if (sysctlbyname("debug.bpf_stats", buffer, &len, NULL, 0) != 0) {
		warn("sysctlbyname debug.bpf_stats");
		retval = -1;
		goto done;
	}
	printf("%-9s %-14s %-13s %9s %9s %9s %12s %9s %9s %9s %9s %9s %9s %9s %12s %9s %9s %s\n",
	       "Device", "Netif", "Flags",
	       "Recv", "RDrop", "RMatch", "RSize",
	       "ReadCnt",
	       "Bsize", "Sblen", "Scnt", "Hblen", "Hcnt",
	       "Ccnt", "Csize",
	       "Written", "WDrop",
	       "Command");
	for (bd = (struct xbpf_d *)buffer;
	     (void *)(bd + 1) <= buffer + len;
	     bd = (void *)bd + bd->bd_structsize) {
		char flagbuf[32];
		char namebuf[32];

		if (interface != NULL &&
		    strncmp(interface, bd->bd_ifname, sizeof(bd->bd_ifname)) != 0) {
			continue;
		}

		bd_flags(bd, flagbuf, sizeof(flagbuf));
		proc_name(bd->bd_pid, namebuf, sizeof(namebuf));
		printf("bpf%-6u %-14s %11s %9llu %9llu %9llu %12llu %9llu %9u %9u %9u %9u %9u %9llu %12llu %9llu %9llu %s.%d\n",
		       bd->bd_dev_minor, bd->bd_ifname, flagbuf,
		       bd->bd_rcount, bd->bd_dcount, bd->bd_fcount, bd->bd_fsize,
		       bd->bd_read_count,
		       bd->bd_bufsize, bd->bd_slen, bd->bd_scnt, bd->bd_hlen, bd->bd_hcnt,
		       bd->bd_comp_count, bd->bd_comp_size,
		       bd->bd_wcount, bd->bd_wdcount,
		       namebuf, bd->bd_pid);
	}
done:
	free(buffer);

	return retval;
}

void
bpf_help(void)
{
	printf("Meaning of the letters in the Flags column of the statistics about BPF devices:\n");

#define FMT "\t%c\t%s\n"

	printf(FMT, 'p', "promiscuous mode enabled");
	printf(FMT, 'i', "immediate mode enabled");
	printf(FMT, 'h', "header complete enabled");
	printf(FMT, 'a', "asynchronous mode enabled");
	printf(FMT, 'I', "see incoming packets");
	printf(FMT, 'O', "see outgoing packets");
	printf(FMT, 'h', "head drop mode");
	printf(FMT, 'C', "compression enabled");
	printf(FMT, 'c', "compression requested");
	printf(FMT, 'x', "use BPF extended header");
	printf(FMT, 't', "truncation requested");
	printf(FMT, '2', "pktap header v2");
	printf(FMT, 'B', "batch write enabled");
#ifdef BIOCGDVRTIN
	printf(FMT, 'D', "divert input mode");
#endif /* BIOCGDVRTIN */
	printf(FMT, '-', "option not enabled");

#undef FMT
}
