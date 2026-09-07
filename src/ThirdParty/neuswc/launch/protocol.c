#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

ssize_t
send_fd(int socket, int fd, struct iovec *iov, int iovlen)
{
	char control[CMSG_SPACE(sizeof(fd))];
	struct msghdr message = {
	    .msg_name = NULL,
	    .msg_namelen = 0,
	    .msg_iov = iov,
	    .msg_iovlen = iovlen,
	};
	struct cmsghdr *cmsg;

	if (fd != -1) {
		message.msg_control = control, message.msg_controllen = sizeof(control);

		cmsg = CMSG_FIRSTHDR(&message);
		cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;

		memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	} else {
		message.msg_control = NULL;
		message.msg_controllen = 0;
	}

	return sendmsg(socket, &message, 0);
}

ssize_t
receive_fd(int socket, int *fd, struct iovec *iov, int iovlen)
{
	ssize_t size;
	char control[CMSG_SPACE(sizeof(*fd))];
	struct msghdr message = {
	    .msg_name = NULL,
	    .msg_namelen = 0,
	    .msg_iov = iov,
	    .msg_iovlen = iovlen,
	};
	struct cmsghdr *cmsg;

	if (fd) {
		*fd = -1;
		message.msg_control = &control;
		message.msg_controllen = sizeof(control);
	}

	size = recvmsg(socket, &message, MSG_CMSG_CLOEXEC);
	if (size < 0) {
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&message);
	if (fd && cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(*fd)) &&
	    cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
		memcpy(fd, CMSG_DATA(cmsg), sizeof(*fd));
	}

	return size;
}
