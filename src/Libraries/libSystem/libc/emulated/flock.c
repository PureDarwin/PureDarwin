/*
 * PureDarwin: real Darwin's flock(2) (BSD advisory whole-file locking) is
 * implemented as a thin wrapper over fcntl(F_SETLK/F_SETLKW) with a
 * whole-file lock range - the same real fallback shape BSD-derived libc
 * has used since flock() was reimplemented on top of POSIX fcntl locking
 * (no native flock syscall exists in XNU's syscalls.master).
 */
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>

int
flock(int fd, int operation)
{
	struct flock fl = {
		.l_start = 0,
		.l_len = 0,
		.l_pid = 0,
		.l_type = F_UNLCK,
		.l_whence = SEEK_SET,
	};
	int cmd;

	if (operation & LOCK_UN) {
		fl.l_type = F_UNLCK;
		cmd = F_SETLK;
	} else if (operation & LOCK_EX) {
		fl.l_type = F_WRLCK;
		cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;
	} else if (operation & LOCK_SH) {
		fl.l_type = F_RDLCK;
		cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;
	} else {
		errno = EINVAL;
		return -1;
	}

	if (fcntl(fd, cmd, &fl) == -1) {
		if (errno == EAGAIN) {
			errno = EWOULDBLOCK;
		}
		return -1;
	}

	return 0;
}
