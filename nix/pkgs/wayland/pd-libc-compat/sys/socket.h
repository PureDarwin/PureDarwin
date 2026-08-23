/* <sys/socket.h> plus Linux's accept4 and the SOCK_* creation flags.
 *
 * Linux folds O_CLOEXEC/O_NONBLOCK into socket() and accept4() so the flags
 * are applied atomically. Darwin has neither, so these apply them afterwards
 * with fcntl - the same non-atomic emulation as pipe2 in <unistd.h> here, with
 * the same caveat about a concurrent fork.
 */
#ifndef PD_SYS_SOCKET_COMPAT_H
#define PD_SYS_SOCKET_COMPAT_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-include-next"
#include_next <sys/socket.h>
#pragma clang diagnostic pop

#include <fcntl.h>
#include <unistd.h>

/* Bits outside Darwin's SOCK_* range, stripped before the real call. */
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC   0x10000000
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK  0x20000000
#endif

static inline int
pd_socket_apply_flags(int fd, int flags)
{
    if (fd < 0)
        return fd;

    if (flags & SOCK_CLOEXEC) {
        int fd_flags = fcntl(fd, F_GETFD);
        if (fd_flags < 0 || fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC) < 0)
            goto fail;
    }
    if (flags & SOCK_NONBLOCK) {
        int fl = fcntl(fd, F_GETFL);
        if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0)
            goto fail;
    }
    return fd;

fail:
    close(fd);
    return -1;
}

static inline int
pd_socket(int domain, int type, int protocol)
{
    return pd_socket_apply_flags(
        socket(domain, type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK), protocol), type);
}
#define socket(domain, type, protocol) pd_socket((domain), (type), (protocol))

static inline int
accept4(int fd, struct sockaddr *addr, socklen_t *addr_len, int flags)
{
    return pd_socket_apply_flags(accept(fd, addr, addr_len), flags);
}

#endif /* PD_SYS_SOCKET_COMPAT_H */
