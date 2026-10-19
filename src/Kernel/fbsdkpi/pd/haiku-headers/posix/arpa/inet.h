/*
 * Copyright 2009-2020 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _INET_H_
#define	_INET_H_


#include <netinet/in.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

in_addr_t		inet_addr(const char* addressString);
int				inet_aton(const char* addressString, struct in_addr* address);
in_addr_t		inet_lnaof(struct in_addr address);
struct in_addr	inet_makeaddr(in_addr_t net, in_addr_t host);
char*			inet_net_ntop(int family, const void* source, int bits,
					char* dest, size_t destSize);
int				inet_net_pton(int family, const char* sourceString, void* dest,
					size_t destSize);
char*			inet_neta(u_long source, char* dest, size_t destSize);
in_addr_t		inet_netof(struct in_addr address);
in_addr_t		inet_network(const char* addressString);
char*			inet_ntoa(struct in_addr address);
const char*		inet_ntop(int family, const void* source, char* dest,
					socklen_t destSize);
int				inet_pton(int family, const char* sourceString, void* dest);
u_int			inet_nsap_addr(const char* sourceString, u_char* dest,
					int destSize);
char*			inet_nsap_ntoa(int sourceLength, const u_char* source,
					char* dest);

#ifdef __cplusplus
}
#endif

#endif	/* _INET_H_ */
