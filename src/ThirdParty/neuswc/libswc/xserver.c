/* swc: libswc/xserver.c
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Based in part upon xwayland/launcher.c from weston, which is
 *
 *     Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "xserver.h"
#include "internal.h"
#include "util.h"
#include "xwm.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server.h>

#define LOCK_FMT "/tmp/.X%d-lock"
#define SOCKET_DIR "/tmp/.X11-unix"
#define SOCKET_FMT SOCKET_DIR "/X%d"

static struct {
	struct wl_resource *resource;
	struct wl_event_source *usr1_source;
	int display;
	char display_name[16];
	int abstract_fd, unix_fd, wm_fd;
	bool xwm_initialized;
} xserver;

struct swc_xserver swc_xserver;

static int
open_socket(struct sockaddr_un *addr)
{
	int fd;

	if ((fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0)) < 0) {
		goto error0;
	}

	/* Unlink the socket location in case it was being used by a process which
	 * left around a stale lockfile. */
	unlink(addr->sun_path);

	if (bind(fd, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
		goto error1;
	}

	if (listen(fd, 1) < 0) {
		goto error2;
	}

	return fd;

error2:
	if (addr->sun_path[0]) {
		unlink(addr->sun_path);
	}
error1:
	close(fd);
error0:
	return -1;
}

static bool
open_display(void)
{
	char lock_name[64], pid[12];
	int lock_fd;
	struct sockaddr_un addr = {.sun_family = AF_LOCAL};

	xserver.display = 0;

	/* Create X lockfile and server sockets */
	goto begin;

retry2:
	close(xserver.abstract_fd);
retry1:
	unlink(lock_name);
retry0:
	if (++xserver.display > 32) {
		ERROR("No open display in first 32\n");
		return false;
	}

begin:
	snprintf(lock_name, sizeof(lock_name), LOCK_FMT, xserver.display);
	lock_fd = open(lock_name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0444);

	if (lock_fd == -1) {
		char *end;
		pid_t owner;

		/* Check if the owning process is still alive. */
		if ((lock_fd = open(lock_name, O_RDONLY)) == -1) {
			goto retry0;
		}

		if (read(lock_fd, pid, sizeof(pid) - 1) != sizeof(pid) - 1) {
			goto retry0;
		}

		owner = strtol(pid, &end, 10);

		if (end != pid + 10) {
			goto retry0;
		}

		if (kill(owner, 0) == 0 || errno != ESRCH) {
			goto retry0;
		}

		if (unlink(lock_name) != 0) {
			goto retry0;
		}

		goto begin;
	}

	snprintf(pid, sizeof(pid), "%10d\n", getpid());
	if (write(lock_fd, pid, sizeof(pid) - 1) != sizeof(pid) - 1) {
		ERROR("Failed to write PID file\n");
		unlink(lock_name);
		close(lock_fd);
		return false;
	}

	close(lock_fd);

	#ifdef __linux__
	/* Bind to abstract socket */
	addr.sun_path[0] = '\0';
	snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, SOCKET_FMT,
	         xserver.display);
	if ((xserver.abstract_fd = open_socket(&addr)) < 0) {
		goto retry1;
	}
	#else
	xserver.abstract_fd=-1;
	#endif

	/* Bind to unix socket */
	mkdir(SOCKET_DIR, 0777);
	snprintf(addr.sun_path, sizeof(addr.sun_path), SOCKET_FMT, xserver.display);
	if ((xserver.unix_fd = open_socket(&addr)) < 0) {
		goto retry2;
	}

	snprintf(xserver.display_name, sizeof(xserver.display_name), ":%d",
	         xserver.display);
	setenv("DISPLAY", xserver.display_name, true);

	return true;
}

static void
close_display(void)
{
	char path[64];

	#ifdef __linux__
	close(xserver.abstract_fd);
	#endif
	close(xserver.unix_fd);

	snprintf(path, sizeof(path), SOCKET_FMT, xserver.display);
	unlink(path);
	snprintf(path, sizeof(path), LOCK_FMT, xserver.display);
	unlink(path);

	unsetenv("DISPLAY");
}

static int
handle_usr1(int signal_number, void *data)
{
	if (xwm_initialize(xserver.wm_fd)) {
		xserver.xwm_initialized = true;
	} else {
		ERROR("Failed to initialize X window manager\n");
		/* XXX: How do we handle this case? */
	}

	wl_event_source_remove(xserver.usr1_source);

	return 0;
}

static void
handle_client_destroy(struct wl_listener *listener, void *data)
{
	swc_xserver.client = NULL;
}

static struct wl_listener client_destroy_listener = {
    .notify = handle_client_destroy,
};

bool
xserver_initialize(void)
{
	int wl[2], wm[2];

	/* Open an X display */
	if (!open_display()) {
		ERROR("Failed to get X lockfile and sockets\n");
		goto error0;
	}

	xserver.usr1_source =
	    wl_event_loop_add_signal(swc.event_loop, SIGUSR1, &handle_usr1, NULL);

	if (!xserver.usr1_source) {
		ERROR("Failed to create SIGUSR1 event source\n");
		goto error1;
	}

	/* Open a socket for the Wayland connection from Xwayland. */
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wl) != 0) {
		ERROR("Failed to create socketpair: %s\n", strerror(errno));
		goto error2;
	}

	/* Open a socket for the X connection to Xwayland. */
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm) != 0) {
		ERROR("Failed to create socketpair: %s\n", strerror(errno));
		goto error3;
	}

	if (!(swc_xserver.client = wl_client_create(swc.display, wl[0]))) {
		goto error4;
	}

	wl_client_add_destroy_listener(swc_xserver.client,
	                               &client_destroy_listener);
	xserver.wm_fd = wm[0];

	/* Start the X server */
	switch (fork()) {
	case 0: {
		int fds[] = {wl[1], wm[1], xserver.abstract_fd, xserver.unix_fd};
		char strings[ARRAY_LENGTH(fds)][16];
		unsigned index;
		struct sigaction action = {.sa_handler = SIG_IGN};

		/* Unset the FD_CLOEXEC flag on the FDs that will get passed to
		 * Xwayland. */
		for (index = 0; index < ARRAY_LENGTH(fds); ++index) {
#if defined(__FreeBSD__)
		  if (fds[index]==-1) continue;
#endif
			if (fcntl(fds[index], F_SETFD, 0) != 0) {
				ERROR("fcntl() failed: %s\n", strerror(errno));
				goto fail;
			}

			if (snprintf(strings[index], sizeof(strings[index]), "%d",
			             fds[index]) >= sizeof(strings[index])) {
				ERROR("FD is too large\n");
				goto fail;
			}
		}

		/* Ignore the USR1 signal so that Xwayland will send a USR1 signal to
		 * the parent process (us) after it finishes initializing. See
		 * Xserver(1) for more details. */
		if (sigaction(SIGUSR1, &action, NULL) != 0) {
			ERROR("Failed to set SIGUSR1 handler to SIG_IGN: %s\n",
			      strerror(errno));
			goto fail;
		}

		setenv("WAYLAND_SOCKET", strings[0], true);

		#ifdef __linux__
		execlp("Xwayland", "Xwayland", xserver.display_name, "-rootless",
		       "-terminate", "-listen", strings[2], "-listen", strings[3],
		       "-wm", strings[1], NULL);
		#else
		execlp("Xwayland", "Xwayland", xserver.display_name, "-rootless",
		       "-terminate", "-listen", strings[3],
		       "-wm", strings[1], NULL);
		#endif

	fail:
		exit(EXIT_FAILURE);
	}
	case -1:
		ERROR("fork() failed when trying to start X server: %s\n",
		      strerror(errno));
		goto error5;
	}

	close(wl[1]);
	close(wm[1]);

	return true;

error5:
	wl_client_destroy(swc_xserver.client);
error4:
	close(wm[1]);
	close(wm[0]);
error3:
	close(wl[1]);
	close(wl[0]);
error2:
	wl_event_source_remove(xserver.usr1_source);
error1:
	close_display();
error0:
	return false;
}

void
xserver_finalize(void)
{
	if (xserver.xwm_initialized) {
		xwm_finalize();
	}
	if (swc_xserver.client) {
		wl_client_destroy(swc_xserver.client);
	}
	close_display();
}
