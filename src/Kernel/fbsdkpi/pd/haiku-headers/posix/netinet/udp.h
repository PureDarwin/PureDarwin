/*
 * Copyright 2020, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NETINET_UDP_H
#define _NETINET_UDP_H

#include <stdint.h>

struct udphdr {
	uint16_t uh_sport;
	uint16_t uh_dport;
	uint16_t uh_ulen;
	uint16_t uh_sum;
};

#endif /* _NETINET_UDP_H */
