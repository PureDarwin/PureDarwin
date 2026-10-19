/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H


#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>


typedef uint32_t socklen_t;
typedef uint8_t sa_family_t;

/* Address families */
#define AF_UNSPEC		0
#define AF_INET			1
#define AF_APPLETALK	2
#define AF_ROUTE		3
#define AF_LINK			4
#define AF_INET6		5
#define AF_DLI			6
#define AF_IPX			7
#define AF_NOTIFY		8
#define AF_LOCAL		9
#define AF_UNIX			AF_LOCAL
#define AF_BLUETOOTH	10
#define AF_MAX			11

/* Protocol families, deprecated */
#define PF_UNSPEC		AF_UNSPEC
#define PF_INET			AF_INET
#define PF_ROUTE		AF_ROUTE
#define PF_LINK			AF_LINK
#define PF_INET6		AF_INET6
#define PF_LOCAL		AF_LOCAL
#define PF_UNIX			AF_UNIX
#define PF_BLUETOOTH	AF_BLUETOOTH

/* Socket types */
#define	SOCK_STREAM	1
#define	SOCK_DGRAM	2
#define	SOCK_RAW	3
#define	SOCK_SEQPACKET	5
#define SOCK_MISC	255

#define SOCK_NONBLOCK 	0x00040000
#define SOCK_CLOEXEC 	0x00080000
#define SOCK_CLOFORK 	0x00100000

/* Socket options for SOL_SOCKET level */
#define	SOL_SOCKET		-1

#define SO_ACCEPTCONN	0x00000001	/* socket has had listen() */
#define SO_BROADCAST	0x00000002	/* permit sending of broadcast msgs */
#define	SO_DEBUG		0x00000004	/* turn on debugging info recording */
#define	SO_DONTROUTE	0x00000008	/* just use interface addresses */
#define	SO_KEEPALIVE	0x00000010	/* keep connections alive */
#define SO_OOBINLINE	0x00000020	/* leave received OOB data in line */
#define	SO_REUSEADDR	0x00000040	/* allow local address reuse */
#define SO_REUSEPORT	0x00000080	/* allow local address & port reuse */
#define SO_USELOOPBACK	0x00000100	/* bypass hardware when possible */
#define SO_LINGER		0x00000200	/* linger on close if data present */

#define SO_SNDBUF		0x40000001	/* send buffer size */
#define SO_SNDLOWAT		0x40000002	/* send low-water mark */
#define SO_SNDTIMEO		0x40000003	/* send timeout */
#define SO_RCVBUF		0x40000004	/* receive buffer size */
#define SO_RCVLOWAT		0x40000005	/* receive low-water mark */
#define SO_RCVTIMEO		0x40000006	/* receive timeout */
#define	SO_ERROR		0x40000007	/* get error status and clear */
#define	SO_TYPE			0x40000008	/* get socket type */
#define SO_NONBLOCK		0x40000009
#define SO_BINDTODEVICE	0x4000000a	/* binds the socket to a specific device index */
#define SO_PEERCRED		0x4000000b	/* get peer credentials, param: ucred */

/* Shutdown options */
#define SHUT_RD			0
#define SHUT_WR			1
#define SHUT_RDWR		2

#define SOMAXCONN		4096		/* Max listen queue for a socket */

struct linger {
	int			l_onoff;
	int			l_linger;
};

struct sockaddr {
	uint8_t		sa_len;
	sa_family_t	sa_family;
	uint8_t		sa_data[30];
};

struct sockaddr_storage {
	uint8_t		ss_len;			/* total length */
	sa_family_t	ss_family;		/* address family */
	uint8_t		__ss_pad1[6];	/* align to quad */
	uint64_t	__ss_pad2;		/* force alignment to 64 bit */
	uint8_t		__ss_pad3[112];	/* pad to a total of 128 bytes */
};

struct msghdr {
	void		*msg_name;		/* address we're using (optional) */
	socklen_t	msg_namelen;	/* length of address */
	struct iovec *msg_iov;		/* scatter/gather array we'll use */
	int			msg_iovlen;		/* # elements in msg_iov */
	void		*msg_control;	/* extra data */
	socklen_t	msg_controllen;	/* length of extra data */
	int			msg_flags;		/* flags */
};

/* Flags for the msghdr.msg_flags field */
#define MSG_OOB			0x0001	/* process out-of-band data */
#define MSG_PEEK		0x0002	/* peek at incoming message */
#define MSG_DONTROUTE	0x0004	/* send without using routing tables */
#define MSG_EOR			0x0008	/* data completes record */
#define MSG_TRUNC		0x0010	/* data discarded before delivery */
#define MSG_CTRUNC		0x0020	/* control data lost before delivery */
#define MSG_WAITALL		0x0040	/* wait for full request or error */
#define MSG_DONTWAIT	0x0080	/* this message should be nonblocking */
#define MSG_BCAST		0x0100	/* this message rec'd as broadcast */
#define MSG_MCAST		0x0200	/* this message rec'd as multicast */
#define	MSG_EOF			0x0400	/* data completes connection */
#define MSG_NOSIGNAL	0x0800	/* don't raise SIGPIPE if socket is closed */
#define MSG_CMSG_CLOEXEC	0x1000	/* set FD_CLOEXEC flag on FDs created via SCM_RIGHTS */
#define MSG_CMSG_CLOFORK	0x2000	/* set FD_CLOFORK flag on FDs created via SCM_RIGHTS */

struct cmsghdr {
	socklen_t	cmsg_len;
	int			cmsg_level;
	int			cmsg_type;
	/* data follows */
};

/* cmsghdr access macros */
#define	CMSG_DATA(cmsg) ((unsigned char *)(cmsg) \
	+ _ALIGN(sizeof(struct cmsghdr)))
#define	CMSG_NXTHDR(mhdr, cmsg)	\
	(((char *)(cmsg) + _ALIGN((cmsg)->cmsg_len) \
	+ _ALIGN(sizeof(struct cmsghdr)) \
		> (char *)(mhdr)->msg_control + (mhdr)->msg_controllen) \
		? (struct cmsghdr *)NULL \
		: (struct cmsghdr *)((char *)(cmsg) + _ALIGN((cmsg)->cmsg_len)))
#define	CMSG_FIRSTHDR(mhdr) \
	((mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
	? (struct cmsghdr *)(mhdr)->msg_control \
	: (struct cmsghdr *)NULL)
#define	CMSG_SPACE(len) (_ALIGN(sizeof(struct cmsghdr)) + _ALIGN(len))
#define	CMSG_LEN(len)	(_ALIGN(sizeof(struct cmsghdr)) + (len))
#define	CMSG_ALIGN(len)	_ALIGN(len)

/* SOL_SOCKET control message types */
#define SCM_RIGHTS	0x01

/* parameter to SO_PEERCRED */
struct ucred {
	pid_t	pid;	/* PID of sender */
	uid_t	uid;	/* UID of sender */
	gid_t	gid;	/* GID of sender */
};


#if __cplusplus
extern "C" {
#endif

int 	accept(int socket, struct sockaddr *address, socklen_t *_addressLength);
int 	accept4(int socket, struct sockaddr *address, socklen_t *_addressLength, int flags);
int		bind(int socket, const struct sockaddr *address,
			socklen_t addressLength);
int		connect(int socket, const struct sockaddr *address,
			socklen_t addressLength);
int     getpeername(int socket, struct sockaddr *address,
			socklen_t *_addressLength);
int     getsockname(int socket, struct sockaddr *address,
			socklen_t *_addressLength);
int     getsockopt(int socket, int level, int option, void *value,
			socklen_t *_length);
int		listen(int socket, int backlog);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t bufferLength, int flags,
			struct sockaddr *address, socklen_t *_addressLength);
ssize_t recvmsg(int socket, struct msghdr *message, int flags);
ssize_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t	sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags,
			const struct sockaddr *address, socklen_t addressLength);
int     setsockopt(int socket, int level, int option, const void *value,
			socklen_t length);
int		shutdown(int socket, int how);
int		socket(int domain, int type, int protocol);
int		sockatmark(int descriptor);
int		socketpair(int domain, int type, int protocol, int socketVector[2]);

#if __cplusplus
}
#endif

#endif	/* _SYS_SOCKET_H */
