/* <unistd.h> plus pipe2.
 *
 * pipe2 is Linux/FreeBSD's pipe with flags applied atomically at creation.
 * Darwin has no such call, so this is pipe + fcntl. The difference is only
 * observable if the process forks between the two, which is exactly the race
 * pipe2 exists to close - callers that need O_CLOEXEC to be race-free against
 * a concurrent fork do not get that guarantee here.
 */
#ifndef PD_UNISTD_COMPAT_H
#define PD_UNISTD_COMPAT_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-include-next"
#include_next <unistd.h>
#pragma clang diagnostic pop

#include <fcntl.h>

static inline int
pipe2(int fildes[2], int flags)
{
    if (pipe(fildes) != 0)
        return -1;

    for (int i = 0; i < 2; i++) {
        if (flags & O_CLOEXEC) {
            int fd_flags = fcntl(fildes[i], F_GETFD);
            if (fd_flags < 0 || fcntl(fildes[i], F_SETFD, fd_flags | FD_CLOEXEC) < 0)
                goto fail;
        }
        if (flags & O_NONBLOCK) {
            int fl = fcntl(fildes[i], F_GETFL);
            if (fl < 0 || fcntl(fildes[i], F_SETFL, fl | O_NONBLOCK) < 0)
                goto fail;
        }
    }
    return 0;

fail:
    close(fildes[0]);
    close(fildes[1]);
    return -1;
}

#endif /* PD_UNISTD_COMPAT_H */
