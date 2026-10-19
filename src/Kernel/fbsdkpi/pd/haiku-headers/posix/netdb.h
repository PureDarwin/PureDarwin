/*
 * Copyright 2023, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NETDB_H_
#define _NETDB_H_

#include <stdint.h>
#include <netinet/in.h>


#ifdef __cplusplus
extern "C" {
#endif


struct hostent {
	char* h_name;
	char** h_aliases;
	int h_addrtype;
	int h_length;
	char** h_addr_list;
#define	h_addr	h_addr_list[0]
};

struct netent {
	char* n_name;
	char** n_aliases;
	int n_addrtype;
	in_addr_t n_net;
};

struct servent {
	char* s_name;
	char** s_aliases;
	int s_port;
	char* s_proto;
};

struct protoent {
	char* p_name;
	char** p_aliases;
	int p_proto;
};

struct addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	char* ai_canonname;
	struct sockaddr* ai_addr;
	struct addrinfo* ai_next;
};


#if defined(_DEFAULT_SOURCE)
extern int * __h_errno(void);
#define	h_errno (*__h_errno())

void herror(const char *);
const char *hstrerror(int);

#define	NETDB_INTERNAL	-1
#define	NETDB_SUCCESS	0
#define	HOST_NOT_FOUND	1
#define	TRY_AGAIN		2
#define	NO_RECOVERY		3
#define	NO_DATA			4
#define	NO_ADDRESS		NO_DATA
#endif


/* getaddrinfo, getnameinfo */
#define	AI_PASSIVE		0x00000001
#define	AI_CANONNAME	0x00000002
#define AI_NUMERICHOST	0x00000004
#define AI_NUMERICSERV	0x00000008

#define AI_ALL			0x00000100
#define AI_V4MAPPED_CFG	0x00000200
#define AI_ADDRCONFIG	0x00000400
#define AI_V4MAPPED		0x00000800

#define AI_MASK \
	(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG | AI_ALL \
		| AI_V4MAPPED)

/* getnameinfo */
#if defined(_DEFAULT_SOURCE)
#define	NI_MAXHOST		1025
#define	NI_MAXSERV		32

#define SCOPE_DELIMITER	'%'
#endif

#define	NI_NOFQDN		0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD		0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM		0x00000010
#define NI_NUMERICSCOPE	0x00000040


/* getaddrinfo */
#define	EAI_ADDRFAMILY	1
#define	EAI_AGAIN	 	2
#define	EAI_BADFLAGS	3
#define	EAI_FAIL		4
#define	EAI_FAMILY		5
#define	EAI_MEMORY		6
#define	EAI_NODATA		7
#define	EAI_NONAME		8
#define	EAI_SERVICE		9
#define	EAI_SOCKTYPE	10
#define	EAI_SYSTEM		11
#define EAI_BADHINTS	12
#define EAI_PROTOCOL	13
#define EAI_OVERFLOW	14
#define EAI_MAX			15


void sethostent(int);
void endhostent(void);
struct hostent* gethostent(void);

void setnetent(int);
void endnetent(void);
struct netent* getnetbyaddr(uint32_t, int);
struct netent* getnetbyname(const char *);
struct netent* getnetent(void);

void setprotoent(int);
void endprotoent(void);
struct protoent* getprotoent(void);
struct protoent* getprotobyname(const char *);
struct protoent* getprotobynumber(int);

void setservent(int);
void endservent(void);
struct servent* getservent(void);
struct servent* getservbyname(const char *, const char *);
struct servent* getservbyport(int, const char *);

struct hostent* gethostbyaddr(const void *address, socklen_t length, int type);
struct hostent* gethostbyname(const char *name);
struct hostent* gethostbyname2(const char *name, int type);

int getaddrinfo(const char *, const char *, const struct addrinfo *,
	struct addrinfo **);
int getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t,
	char *, socklen_t, int);
void freeaddrinfo(struct addrinfo *);

const char* gai_strerror(int);


#ifdef __cplusplus
}
#endif

#endif /* _NETDB_H_ */
