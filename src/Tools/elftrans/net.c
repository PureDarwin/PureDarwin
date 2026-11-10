// net.c - Linux sockets on Darwin's: addresses, families, options and the calls that take them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "elftrans.h"

#define LINUX_AF_UNIX 1
#define LINUX_AF_INET 2
#define LINUX_AF_INET6 10

// a Linux sockaddr as Darwin's: the family is a 16-bit field there, a length and an 8-bit family here
int
net_to_native(const void *guest, uint32_t length, struct sockaddr_storage *native, socklen_t *native_length)
{
    const uint8_t *in = guest;
    uint16_t family;

    if (!guest || length < 2)
        return EINVAL;
    memcpy(&family, in, 2);
    memset(native, 0, sizeof(*native));
    if (family == LINUX_AF_UNIX) {
        struct sockaddr_un *un = (struct sockaddr_un *)native;
        // the name runs to its first NUL: callers often pass the whole 110-byte structure
        size_t path = length > 2 && in[2] ? strnlen((const char *)in + 2, length - 2) : length - 2;

        // the abstract namespace (a leading NUL) has no Darwin counterpart
        if (path && !in[2])
            return EOPNOTSUPP;
        if (path > sizeof(un->sun_path))
            return ENAMETOOLONG;
        un->sun_family = AF_UNIX;
        memcpy(un->sun_path, in + 2, path);
        un->sun_len = (uint8_t)(offsetof(struct sockaddr_un, sun_path) + path);
        *native_length = un->sun_len;
        return 0;
    }
    if (family == LINUX_AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)native;

        if (length < 16)
            return EINVAL;
        sin->sin_len = sizeof(*sin), sin->sin_family = AF_INET;
        memcpy(&sin->sin_port, in + 2, 2);
        memcpy(&sin->sin_addr, in + 4, 4);
        *native_length = sizeof(*sin);
        return 0;
    }
    if (family == LINUX_AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)native;

        if (length < 24)
            return EINVAL;
        sin6->sin6_len = sizeof(*sin6), sin6->sin6_family = AF_INET6;
        memcpy(&sin6->sin6_port, in + 2, 2);
        memcpy(&sin6->sin6_flowinfo, in + 4, 4);
        memcpy(&sin6->sin6_addr, in + 8, 16);
        if (length >= 28)
            memcpy(&sin6->sin6_scope_id, in + 24, 4);
        *native_length = sizeof(*sin6);
        return 0;
    }
    return EAFNOSUPPORT;
}

// a Darwin sockaddr back as Linux's, truncated to *length as the kernel does, *length set to its full size
void
net_to_linux(const struct sockaddr *native, socklen_t native_length, void *guest, uint32_t *length)
{
    uint8_t out[128] = { 0 };
    uint32_t full = 0;
    uint16_t family = 0;

    if (!native_length) {
        full = 0;
    } else if (native->sa_family == AF_UNIX) {
        const struct sockaddr_un *un = (const struct sockaddr_un *)native;
        size_t path = native_length > offsetof(struct sockaddr_un, sun_path) ?
                          strnlen(un->sun_path, native_length - offsetof(struct sockaddr_un, sun_path)) : 0;

        family = LINUX_AF_UNIX;
        memcpy(out + 2, un->sun_path, path);
        // a named socket's length counts its terminating NUL, an unnamed one is just the family
        full = (uint32_t)(2 + (path ? path + 1 : 0));
    } else if (native->sa_family == AF_INET) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)native;

        family = LINUX_AF_INET;
        memcpy(out + 2, &sin->sin_port, 2), memcpy(out + 4, &sin->sin_addr, 4);
        full = 16;
    } else if (native->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)native;

        family = LINUX_AF_INET6;
        memcpy(out + 2, &sin6->sin6_port, 2), memcpy(out + 4, &sin6->sin6_flowinfo, 4);
        memcpy(out + 8, &sin6->sin6_addr, 16), memcpy(out + 24, &sin6->sin6_scope_id, 4);
        full = 28;
    }
    memcpy(out, &family, 2);
    if (guest && length)
        memcpy(guest, out, *length < full ? *length : full);
    if (length)
        *length = full;
}

static int
native_family(int linux_family)
{
    return linux_family == LINUX_AF_UNIX ? AF_UNIX : linux_family == LINUX_AF_INET ? AF_INET :
           linux_family == LINUX_AF_INET6 ? AF_INET6 : -1;
}

// SOCK_NONBLOCK and SOCK_CLOEXEC on a new fd, and no SIGPIPE: Linux raises it only without MSG_NOSIGNAL,
// which the send paths turn into the signal themselves
static long
finish_fd(int fd, int linux_flags)
{
    int on = 1;

    if (fd < 0)
        return -linux_error(errno);
    if ((linux_flags & 0x800) && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK))
        goto fail;
    if ((linux_flags & 0x80000) && fcntl(fd, F_SETFD, FD_CLOEXEC))
        goto fail;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    return fd;
fail:;
    int error = errno;

    close(fd);
    return -linux_error(error);
}

long
net_socket(int domain, int linux_type, int protocol)
{
    int family = native_family(domain), type = linux_type & 0xf;

    if (family < 0)
        return -97;
    if (linux_type & ~(0xf | 0x800 | 0x80000))
        return -22;
    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET && type != SOCK_RAW)
        return -94;
    // IPPROTO_IP and the protocols both number the same
    return finish_fd(socket(family, type, family == AF_UNIX ? 0 : protocol), linux_type);
}

long
net_bind_connect(int fd, const void *address, uint32_t length, int connecting)
{
    struct sockaddr_storage native;
    socklen_t native_length;
    int error = net_to_native(address, length, &native, &native_length);

    if (error)
        return -linux_error(error);
    if (connecting)
        return connect(fd, (struct sockaddr *)&native, native_length) ? -linux_error(errno) : 0;
    return bind(fd, (struct sockaddr *)&native, native_length) ? -linux_error(errno) : 0;
}

long
net_accept(int fd, void *address, uint32_t *length, int linux_flags)
{
    struct sockaddr_storage native;
    socklen_t native_length = sizeof(native);
    int out;

    if (linux_flags & ~(0x800 | 0x80000))
        return -22;
    out = accept(fd, (struct sockaddr *)&native, &native_length);
    if (out < 0)
        return -linux_error(errno);
    if (address && length)
        net_to_linux((struct sockaddr *)&native, native_length, address, length);
    return finish_fd(out, linux_flags);
}

long
net_name(int fd, void *address, uint32_t *length, int peer)
{
    struct sockaddr_storage native;
    socklen_t native_length = sizeof(native);

    if (!length)
        return -14;
    if ((peer ? getpeername : getsockname)(fd, (struct sockaddr *)&native, &native_length))
        return -linux_error(errno);
    net_to_linux((struct sockaddr *)&native, native_length, address, length);
    return 0;
}

static int
send_flags(int linux_flags)
{
    int flags = 0;

    // MSG_OOB, MSG_PEEK, MSG_DONTROUTE, MSG_DONTWAIT, MSG_EOR, MSG_WAITALL, MSG_NOSIGNAL, MSG_MORE
    if (linux_flags & ~(0x1 | 0x2 | 0x4 | 0x40 | 0x80 | 0x100 | 0x4000 | 0x8000 | 0x40000000))
        return -1;
    flags |= linux_flags & 0x1 ? MSG_OOB : 0;
    flags |= linux_flags & 0x2 ? MSG_PEEK : 0;
    flags |= linux_flags & 0x4 ? MSG_DONTROUTE : 0;
    flags |= linux_flags & 0x40 ? MSG_DONTWAIT : 0;
    flags |= linux_flags & 0x80 ? MSG_EOR : 0;
    flags |= linux_flags & 0x100 ? MSG_WAITALL : 0;
    return flags;
}

long
net_sendto(int fd, const void *buf, size_t n, int linux_flags, const void *address, uint32_t length)
{
    struct sockaddr_storage native;
    socklen_t native_length = 0;
    int flags = send_flags(linux_flags);
    ssize_t sent;

    if (flags < 0)
        return -22;
    if (address) {
        int error = net_to_native(address, length, &native, &native_length);

        if (error)
            return -linux_error(error);
    }
    sent = sendto(fd, buf, n, flags, address ? (struct sockaddr *)&native : NULL, native_length);
    if (sent < 0 && errno == EPIPE && !(linux_flags & 0x4000))
        kill(getpid(), SIGPIPE);
    return sent < 0 ? -linux_error(errno) : sent;
}

long
net_recvfrom(int fd, void *buf, size_t n, int linux_flags, void *address, uint32_t *length)
{
    struct sockaddr_storage native;
    socklen_t native_length = sizeof(native);
    int flags = send_flags(linux_flags & ~0x40000000);
    ssize_t got;

    if (flags < 0)
        return -22;
    got = recvfrom(fd, buf, n, flags, address ? (struct sockaddr *)&native : NULL, address ? &native_length : NULL);
    if (got < 0)
        return -linux_error(errno);
    if (address && length)
        net_to_linux((struct sockaddr *)&native, native_length, address, length);
    return got;
}

// a Linux socket option as Darwin's level and number, 0 when there is none
int
net_option(int level, int option, int *native_level, int *native_option)
{
    static const int sol_socket[][2] = {
        { 2, SO_REUSEADDR }, { 3, SO_TYPE }, { 4, SO_ERROR }, { 5, SO_DONTROUTE }, { 6, SO_BROADCAST },
        { 7, SO_SNDBUF }, { 8, SO_RCVBUF }, { 9, SO_KEEPALIVE }, { 10, SO_OOBINLINE }, { 13, SO_LINGER },
        { 15, SO_REUSEPORT }, { 18, SO_RCVLOWAT }, { 19, SO_SNDLOWAT }, { 20, SO_RCVTIMEO }, { 21, SO_SNDTIMEO },
        { 30, SO_ACCEPTCONN }, { 32, SO_SNDBUF }, { 33, SO_RCVBUF },
    };
    static const int ip[][2] = {
        { 1, IP_TOS }, { 2, IP_TTL }, { 3, IP_HDRINCL }, { 4, IP_OPTIONS }, { 8, IP_RECVTTL }, { 32, IP_MULTICAST_IF },
        { 33, IP_MULTICAST_TTL }, { 34, IP_MULTICAST_LOOP }, { 35, IP_ADD_MEMBERSHIP }, { 36, IP_DROP_MEMBERSHIP },
    };
    static const int ip6[][2] = {
        { 16, IPV6_UNICAST_HOPS }, { 17, IPV6_MULTICAST_IF }, { 18, IPV6_MULTICAST_HOPS }, { 19, IPV6_MULTICAST_LOOP },
        { 20, IPV6_JOIN_GROUP }, { 21, IPV6_LEAVE_GROUP }, { 26, IPV6_V6ONLY }, { 67, IPV6_TCLASS },
    };
    static const int tcp[][2] = {
        { 1, TCP_NODELAY }, { 2, TCP_MAXSEG }, { 4, TCP_KEEPALIVE }, { 5, TCP_KEEPINTVL }, { 6, TCP_KEEPCNT },
    };
    const int (*table)[2] = NULL;
    unsigned n = 0;

    switch (level) {
    case 1: table = sol_socket, n = sizeof(sol_socket) / sizeof(sol_socket[0]), *native_level = SOL_SOCKET; break;
    case 0: table = ip, n = sizeof(ip) / sizeof(ip[0]), *native_level = IPPROTO_IP; break;
    case 41: table = ip6, n = sizeof(ip6) / sizeof(ip6[0]), *native_level = IPPROTO_IPV6; break;
    case 6: table = tcp, n = sizeof(tcp) / sizeof(tcp[0]), *native_level = IPPROTO_TCP; break;
    default: return 0;
    }
    for (unsigned i = 0; i < n; i++) {
        if (table[i][0] == option) {
            *native_option = table[i][1];
            return 1;
        }
    }
    return 0;
}

// options Linux has and Darwin does not, that programs set and move on from: accepted, nothing changes
int
net_option_ignored(int level, int option)
{
    return (level == 1 && (option == 12 || option == 16 || option == 25 || option == 47)) ||
           (level == 0 && (option == 10 || option == 11)) || (level == 6 && (option == 9 || option == 12 ||
                                                                             option == 18 || option == 23));
}
