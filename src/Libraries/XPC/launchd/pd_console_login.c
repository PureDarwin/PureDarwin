/*
 * PureDarwin console login helper.
 *
 * launchd opens StandardInPath with O_NOCTTY, so a direct /bin/zsh
 * LaunchDaemon has file descriptors for /dev/console but no controlling tty.
 * Keep the tty setup out of PID 1: launchd supervises this helper, and this
 * helper creates the console session then execs the real login shell.
 *
 * The tty setup follows the classic getty dance so it is robust on a serial
 * console (serial=3): open NON-BLOCKING so we never wedge in the tty open
 * waiting for carrier (DCD) - that carrier-wait is exactly the intermittent
 * "sometimes I get a shell, sometimes I don't" boot hang - then force CLOCAL
 * (ignore carrier), clear O_NONBLOCK, and acquire the controlling terminal.
 */
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int g_trace_fd = -1;

static void
ctrace(const char *msg)
{
	if (g_trace_fd >= 0) {
		(void)write(g_trace_fd, msg, strlen(msg));
	}
}

int
main(void)
{
	const char *tty = "/dev/console";

	g_trace_fd = open(tty, O_WRONLY | O_NOCTTY | O_NONBLOCK);
	ctrace("pd-console-login: start\n");

	if (setsid() < 0 && errno != EPERM) {
		ctrace("pd-console-login: setsid failed\n");
		_exit(126);
	}
	ctrace("pd-console-login: setsid ok\n");

	/* Non-blocking open: never wait on carrier. */
	int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		ctrace("pd-console-login: open failed\n");
		_exit(126);
	}
	ctrace("pd-console-login: open ok\n");

	/* Ignore modem carrier so reads/writes and the controlling-tty grab don't
	 * stall when DCD is deasserted on the serial line. */
	struct termios t;
	if (tcgetattr(fd, &t) == 0) {
		t.c_cflag |= CLOCAL;
		(void)tcsetattr(fd, TCSANOW, &t);
		ctrace("pd-console-login: CLOCAL set\n");
	} else {
		ctrace("pd-console-login: tcgetattr failed (continuing)\n");
	}

	/* Back to blocking for normal shell I/O. */
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0) {
		(void)fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
	}

	if (ioctl(fd, TIOCSCTTY, 0) < 0) {
		ctrace("pd-console-login: TIOCSCTTY failed (continuing)\n");
	} else {
		ctrace("pd-console-login: TIOCSCTTY ok\n");
	}

	(void)dup2(fd, STDIN_FILENO);
	(void)dup2(fd, STDOUT_FILENO);
	(void)dup2(fd, STDERR_FILENO);
	if (fd > STDERR_FILENO) {
		(void)close(fd);
	}

	setenv("HOME", "/var/root", 1);
	setenv("USER", "root", 1);
	setenv("LOGNAME", "root", 1);
	setenv("SHELL", "/bin/zsh", 1);
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
	setenv("TERM", "vt220", 0);

	char *argv[] = { "/bin/zsh", "-l", NULL };
	ctrace("pd-console-login: exec /bin/zsh\n");
	execv(argv[0], argv);
	ctrace("pd-console-login: execv failed\n");
	_exit(127);
}
