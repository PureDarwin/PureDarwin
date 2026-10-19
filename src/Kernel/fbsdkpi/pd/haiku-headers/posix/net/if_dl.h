/*
 * Copyright 2006-2018 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NET_IF_DL_H
#define _NET_IF_DL_H


#include <stdint.h>


/* Link level sockaddr structure */
struct sockaddr_dl {
	uint8_t		sdl_len;		/* Total length of sockaddr */
	uint8_t		sdl_family;		/* AF_LINK */
	uint16_t	sdl_e_type;		/* link level frame type */
	uint32_t	sdl_index;		/* index for interface */
	uint8_t		sdl_type;		/* interface type */
	uint8_t		sdl_nlen;		/* interface name length (not terminated with a null byte) */
	uint8_t		sdl_alen;		/* link level address length */
	uint8_t		sdl_slen;		/* link layer selector length */
	uint8_t		sdl_data[46];	/* minimum work area, can be larger */
};

/* Macro to get a pointer to the link level address */
#define LLADDR(s)	((uint8_t *)((s)->sdl_data + (s)->sdl_nlen))

#endif	/* _NET_IF_DL_H */
