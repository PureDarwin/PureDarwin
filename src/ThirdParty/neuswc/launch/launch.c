/* swc: launch/launch.c
 *
 * Copyright (c) 2013, 2014, 2016 Michael Forney
 *
 * Based in part upon weston-launch.c from weston which is:
 *
 *     Copyright © 2012 Benjamin Franzke
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

#include "devmajor.h"
#include "protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#endif

#if defined(__FreeBSD__)
#include <sys/consio.h>
#include <sys/kbio.h>
#include <dev/evdev/input.h> /* needed for eviocgrab */
#elif defined(__NetBSD__)
#include <dev/wscons/wsdisplay_usl_io.h>

#elif defined(__OpenBSD__)
#include <dev/wscons/wsdisplay_usl_io.h>

#elif defined(__linux__)
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>

#else
#include <dev/wscons/wsdisplay_usl_io.h>
#endif

#ifdef ENABLE_DRM
#include <xf86drm.h>
#endif

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array)[0])

static void
activate(void);
static void
deactivate(void);

static bool nflag;
static int sigfd[2], sock[2];
static int input_fds[128], num_input_fds;
#ifdef ENABLE_DRM
static int drm_fds[16], num_drm_fds;
#endif
static int tty_fd;
static bool active;

static struct {
	bool altered;
	int vt;
	long kb_mode;
	long console_mode;
} original_vt_state;

static void
cleanup(void);

static noreturn void
usage(const char *name)
{
	fprintf(stderr, "usage: %s [-n] [-t tty] [--] server [args...]\n", name);
	exit(2);
}

static noreturn void __attribute__((format(printf, 1, 2)))
die(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	if (format[0] && format[strlen(format) - 1] == ':') {
		fprintf(stderr, " %s", strerror(errno));
	}
	fputc('\n', stderr);

	cleanup();
	exit(EXIT_FAILURE);
}

static void
start_devices(void)
{
#ifdef ENABLE_DRM
	int i;

	for (i = 0; i < num_drm_fds; ++i) {
		if (drmSetMaster(drm_fds[i]) < 0) {
			die("failed to set DRM master");
		}
	}
#endif
}

static void
stop_devices(bool fatal)
{
	int i;

#ifdef ENABLE_DRM
	for (i = 0; i < num_drm_fds; ++i) {
		if (drmDropMaster(drm_fds[i]) < 0 && fatal) {
			die("drmDropMaster:");
		}
	}
#endif
	for (i = 0; i < num_input_fds; ++i) {
#ifdef EVIOCREVOKE
		if (ioctl(input_fds[i], EVIOCREVOKE, 0) < 0 && errno != ENODEV &&
		    fatal) {
			die("ioctl EVIOCREVOKE:");
		}
#endif
		close(input_fds[i]);
	}
	num_input_fds = 0;
}

static void
cleanup(void)
{
#ifndef __OpenBSD__
	struct vt_mode mode = {.mode = VT_AUTO};
#endif

	if (!original_vt_state.altered) {
		return;
	}

	/* Stop devices before switching the VT to make sure we have released the
	 * DRM device before the next session tries to claim it. */
	stop_devices(false);

	/* Cleanup VT */
#ifndef __OpenBSD__
	ioctl(tty_fd, VT_SETMODE, &mode);
	ioctl(tty_fd, KDSKBMODE, original_vt_state.kb_mode);
#endif
	ioctl(tty_fd, KDSETMODE, original_vt_state.console_mode);

#ifndef __OpenBSD__
	ioctl(tty_fd, VT_ACTIVATE, original_vt_state.vt);
#endif

	kill(0, SIGTERM);
}

static void
activate(void)
{
	struct swc_launch_event event = {.type = SWC_LAUNCH_EVENT_ACTIVATE};

	start_devices();
	send(sock[0], &event, sizeof(event), 0);
	active = true;
}

static void
deactivate(void)
{
	struct swc_launch_event event = {.type = SWC_LAUNCH_EVENT_DEACTIVATE};

	send(sock[0], &event, sizeof(event), 0);
	stop_devices(true);
	active = false;
}

static void
handle_signal(int sig)
{
	write(sigfd[1], (char[]){sig}, 1);
}

static void
handle_socket_data(int socket)
{
	struct swc_launch_request request;
	struct swc_launch_event response;
	char path[PATH_MAX];
	struct iovec request_iov[2] = {
	    {.iov_base = &request, .iov_len = sizeof(request)},
	    {.iov_base = path, .iov_len = sizeof(path)},
	};
	struct iovec response_iov[1] = {
	    {.iov_base = &response, .iov_len = sizeof(response)},
	};
	int fd = -1;
	struct stat st;
	ssize_t size;

	size = receive_fd(socket, &fd, request_iov, 2);
	if (size == -1 || size == 0 || size < sizeof(request)) {
		return;
	}
	size -= sizeof(request);

	response.type = SWC_LAUNCH_EVENT_RESPONSE;
	response.serial = request.serial;

	switch (request.type) {
	case SWC_LAUNCH_REQUEST_OPEN_DEVICE:
		if (size == 0 || path[size - 1] != '\0') {
			fprintf(stderr, "path is not NULL terminated\n");
			goto fail;
		}
		if ((request.flags & (O_ACCMODE | O_NONBLOCK | O_CLOEXEC)) !=
		    request.flags) {
			fprintf(stderr, "invalid open flags\n");
			goto fail;
		}

		fd = open(path, request.flags | O_CLOEXEC);
		if (fd == -1) {
			fprintf(stderr, "open %s: %s\n", path, strerror(errno));
			goto fail;
		}
		if (fstat(fd, &st) == -1) {
			fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
			goto fail;
		}

		if (device_is_input(st.st_rdev)) {
			if (!active) {
				goto fail;
			}
			if (num_input_fds == ARRAY_LENGTH(input_fds)) {
				fprintf(stderr, "too many input devices opened\n");
				goto fail;
			}
			#ifdef __FreeBSD__
			/* this (EVIOCGRAB) is the only way to
			 * prevent the TTY from recieving
			 * input from the neuswc session.
			 */
			if (ioctl(fd, EVIOCGRAB, (void*)1) == -1)
			  {
			    fprintf(stderr, "EVIOCGRAB %s: %s", path, strerror(errno));
			    goto fail;
			  }
			#endif
			input_fds[num_input_fds++] = fd;
		} else if (device_is_drm(st.st_rdev)) {
#ifdef ENABLE_DRM
			if (num_drm_fds == ARRAY_LENGTH(drm_fds)) {
				fprintf(stderr, "too many DRM devices opened\n");
				goto fail;
			}
			drm_fds[num_drm_fds++] = fd;
#else
			fprintf(stderr, "DRM device requested by non-DRM backend\n");
			goto fail;
#endif
#ifdef ENABLE_FBDEV
		} else if (device_is_fbdev(st.st_rdev)) {
			if (!active) {
				goto fail;
			}
#endif
#ifdef ENABLE_WSDISPLAY
		} else if (device_is_tty(st.st_rdev)) {
			if (!active) {
				goto fail;
			}
#endif
		} else {
			fprintf(stderr, "requested fd is not a video or input device\n");
			goto fail;
		}
		break;
	case SWC_LAUNCH_REQUEST_ACTIVATE_VT:
		if (!active) {
			goto fail;
		}

		if (ioctl(tty_fd, VT_ACTIVATE, request.vt) == -1) {
			fprintf(stderr, "failed to activate VT %d: %s\n", request.vt,
			        strerror(errno));
		}
		break;
	default:
		fprintf(stderr, "unknown request %u\n", request.type);
		goto fail;
	}

	response.success = true;
	goto done;

fail:
	response.success = false;
	if (fd != -1) {
		close(fd);
	}
	fd = -1;
done:
	send_fd(socket, fd, response_iov, 1);
}

static void
find_vt(char *vt, size_t size)
{
#if defined(__NetBSD__)
	if (snprintf(vt, size, "/dev/ttyE1") >= size) {
		die("VT number is too large");
	}
#elif defined(__OpenBSD__)
	const char *tty;
	tty = ttyname(STDIN_FILENO);
	if (!tty || strncmp(tty, "/dev/ttyC", 8) != 0) {
		die("must be run from wscons VT (/dev/ttyC*)");
	}
	if (snprintf(vt, size, "%s", tty) >= size) {
		die("VT number is too large");
	}
#else
	char *vtnr;
	int tty0_fd, vt_num;

	/* If we are running from an existing X or wayland session, always open a
	 * new VT instead of using the current one. */
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY") ||
	    !(vtnr = getenv("XDG_VTNR"))) {
#if defined(__FreeBSD__)
	  tty0_fd=open("/dev/ttyv0", O_RDWR | O_CLOEXEC);
	  #else
		tty0_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
		#endif
		if (tty0_fd == -1) {
			die("open /dev/tty0:");
		}
		if (ioctl(tty0_fd, VT_OPENQRY, &vt_num) != 0) {
			die("VT open query failed:");
		}
		close(tty0_fd);
#if defined(__FreeBSD__)
		if (snprintf(vt, size, "/dev/ttyv%d", vt_num - 1) >= size) {
		#else
		if (snprintf(vt, size, "/dev/tty%d", vt_num) >= size) {
		
		#endif
		  			die("VT number is too large");
		}
	} else {
#if defined(__FreeBSD__)
		  int n=atoi(vtnr);
		  if (snprintf(vt, size, "/dev/ttyv%d", n - 1) >= size) {
		  #else
		if (snprintf(vt, size, "/dev/tty%s", vtnr) >= size) {
		  #endif
			die("XDG_VTNR is too long");
		}
	}
#endif
}

static int
open_tty(const char *tty_name)
{
	char *stdin_tty;
	int fd;

	/* Check if we are already running on the desired VT */
	if ((stdin_tty = ttyname(STDIN_FILENO)) &&
	    strcmp(tty_name, stdin_tty) == 0) {
		return STDIN_FILENO;
	}

	fd = open(tty_name, O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd < 0) {
		die("open %s:", tty_name);
	}

	return fd;
}

static void
setup_tty(int fd)
{
	struct stat st;
	int vt;
#if !defined (__OpenBSD__) && !defined(__FreeBSD__)
	struct vt_stat state;
	#endif
#if !defined(__OpenBSD__)
	struct vt_mode mode = {
	    .mode = VT_PROCESS, .relsig = SIGUSR1, .acqsig = SIGUSR2};
#endif

	if (fstat(fd, &st) == -1) {
		die("failed to stat TTY fd:");
	}
	vt = minor(st.st_rdev);

#ifdef __OpenBSD__
	if (!device_is_tty(st.st_rdev)) {
		die("not a valid VT");
	}
#else
	if (!device_is_tty(st.st_rdev) || vt == 0) {
		die("not a valid VT");
	}
#endif

#if defined(__OpenBSD__)
	/* OpenBSD wscons has no VT_GETSTATE */
	original_vt_state.vt=vt;
#elif defined(__FreeBSD__)
	{
	  int vt_num;
	  if (ioctl(fd, VT_GETACTIVE, &vt_num) == -1)
	    {
	      die("failed to get current VT state:");
	    }
	  original_vt_state.vt=vt_num;
	}
#else
	if (ioctl(fd, VT_GETSTATE, &state) == -1) {
		die("failed to get the current VT state:");
	}
#endif

#if !defined (__OpenBSD__) && !defined(__FreeBSD__)
	original_vt_state.vt = state.v_active;
#else
	original_vt_state.vt = vt;
#endif

#ifdef KDGETMODE
	if (ioctl(fd, KDGKBMODE, &original_vt_state.kb_mode)) {
		die("failed to get keyboard mode:");
	}
	if (ioctl(fd, KDGETMODE, &original_vt_state.console_mode)) {
		die("failed to get console mode:");
	}
#else
	original_vt_state.kb_mode = K_XLATE;
	original_vt_state.console_mode = KD_TEXT;
#endif

#ifdef K_OFF
	if (ioctl(fd, KDSKBMODE, K_OFF) == -1) {
		die("failed to set keyboard mode to K_OFF:");
	}
#elif defined(__FreeBSD__)
	if (ioctl(fd, KDSKBMODE, K_CODE) == -1) {
	  	 die("failed to set keyboard mode to K_RAW:");
	}
#endif
	if (ioctl(fd, KDSETMODE, KD_GRAPHICS) == -1) {
		perror("KDSETMODE KD_GRAPHICS");
		goto error0;
	}

#if !defined (__OpenBSD__) && !defined (__FreeBSD__)
	if (ioctl(fd, VT_SETMODE, &mode) == -1) {
		perror("failed to set VT mode");
		goto error1;
	}
#endif

#ifdef __OpenBSD__
	/* OpenBSD wscons has no VT_PROCESS mode; just cont. on current ttyC* */
	activate();
#else
	if (vt == original_vt_state.vt) {
		activate();
	} else if (!nflag) {
		if (ioctl(fd, VT_ACTIVATE, vt) == -1) {
			perror("failed to activate VT");
			goto error2;
		}

		if (ioctl(fd, VT_WAITACTIVE, vt) == -1) {
			perror("failed to wait for VT to become active");
			goto error2;
		}
	}
#endif
	original_vt_state.altered = true;

	return;

#ifndef __OpenBSD__
error2:
	mode = (struct vt_mode){.mode = VT_AUTO};
	ioctl(fd, VT_SETMODE, &mode);
error1:
	ioctl(fd, KDSKBMODE, original_vt_state.kb_mode);
#endif
error0:
	ioctl(fd, KDSETMODE, original_vt_state.console_mode);
	exit(EXIT_FAILURE);
}

static void
run(int fd)
{
	struct pollfd fds[] = {
	    {.fd = fd, .events = POLLIN},
	    {.fd = sigfd[0], .events = POLLIN},
	};
	int status;
	char sig;

	for (;;) {
		if (poll(fds, ARRAY_LENGTH(fds), -1) < 0) {
			if (errno == EINTR) {
				continue;
			}
			die("poll:");
		}
		if (fds[0].revents) {
			handle_socket_data(fd);
		}
		if (fds[1].revents) {
			if (read(sigfd[0], &sig, 1) <= 0) {
				continue;
			}
			switch (sig) {
			case SIGCHLD:
				wait(&status);
				cleanup();
				exit(WEXITSTATUS(status));
			case SIGUSR1:
				deactivate();
				ioctl(tty_fd, VT_RELDISP, 1);
				break;
			case SIGUSR2:
				ioctl(tty_fd, VT_RELDISP, VT_ACKACQ);
				activate();
				break;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	extern char **environ;
	int option;
	char *vt = NULL, buf[64];
	struct sigaction action = {
	    .sa_handler = handle_signal,
	    .sa_flags = SA_RESTART,
	};
	sigset_t set;
	pid_t pid;
	posix_spawnattr_t attr;

	while ((option = getopt(argc, argv, "nt:")) != -1) {
		switch (option) {
		case 'n':
			nflag = true;
			break;
		case 't':
			vt = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (argc - optind < 1) {
		usage(argv[0]);
	}

	
	if (socketpair(AF_LOCAL,
		       #ifdef __linux__
		       SOCK_SEQPACKET,
		       #else
		       SOCK_STREAM,
		       #endif
		       0, sock) == -1) {
		die("socketpair:");
	}
	if (fcntl(sock[0], F_SETFD, FD_CLOEXEC) == -1) {
		die("failed set CLOEXEC on socket:");
	}

	if (pipe2(sigfd, O_CLOEXEC) == -1) {
		die("pipe:");
	}
	if (sigaction(SIGCHLD, &action, NULL) == -1) {
		die("sigaction SIGCHLD:");
	}
	if (sigaction(SIGUSR1, &action, NULL) == -1) {
		die("sigaction SIGUSR1:");
	}
	if (sigaction(SIGUSR2, &action, NULL) == -1) {
		die("sigaction SIGUSR2:");
	}

	sigfillset(&set);
	sigdelset(&set, SIGCHLD);
	sigdelset(&set, SIGUSR1);
	sigdelset(&set, SIGUSR2);
	sigprocmask(SIG_SETMASK, &set, NULL);

	if (!vt) {
		find_vt(buf, sizeof(buf));
		vt = buf;
	}

	fprintf(stderr, "running on %s\n", vt);
	tty_fd = open_tty(vt);
	setup_tty(tty_fd);
	if (setenv(SWC_LAUNCH_TTY_ENV, vt, 1) == -1) {
		die("setenv %s:", SWC_LAUNCH_TTY_ENV);
	}

	sprintf(buf, "%d", sock[1]);
	setenv(SWC_LAUNCH_SOCKET_ENV, buf, 1);

	/* make sure XDG_RUNTIME_DIR is set */
	if (!getenv("XDG_RUNTIME_DIR")) {
		uid_t uid = getuid();
		snprintf(buf, sizeof(buf), "/tmp/XDG_RUNTIME_DIR_%d", uid);
		if (mkdir(buf, 0700) == -1 && errno != EEXIST) {
			die("mkdir %s:", buf);
		}
		setenv("XDG_RUNTIME_DIR", buf, 1);
		fprintf(stderr, "set XDG_RUNTIME_DIR=%s\n", buf);
	}

	if ((errno = posix_spawnattr_init(&attr))) {
		die("posix_spawnattr_init:");
	}
	if ((errno = posix_spawnattr_setflags(&attr, POSIX_SPAWN_RESETIDS |
	                                                 POSIX_SPAWN_SETSIGMASK))) {
		die("posix_spawnattr_setflags:");
	}
	sigemptyset(&set);
	if ((errno = posix_spawnattr_setsigmask(&attr, &set))) {
		die("posix_spawnattr_setsigmask:");
	}
	if ((errno = posix_spawnp(&pid, argv[optind], NULL, &attr, argv + optind,
	                          environ))) {
		die("posix_spawnp %s:", argv[optind]);
	}
	posix_spawnattr_destroy(&attr);

	close(sock[1]);
	run(sock[0]);

	return EXIT_SUCCESS;
}
