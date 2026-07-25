#include <bootstrap.h>
#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <paths.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _PATH_LOG
#define _PATH_LOG "/var/run/syslog"
#endif

#define PD_LOGD_SERVICE "com.apple.logd"
#define PD_LOGD_LOGFILE "/var/log/system.log"

static int console_fd = -1;
static int logfile_fd = -1;

static void
pd_logd_write_fd(int fd, const char *prefix, const char *msg, ssize_t len)
{
	if (fd < 0 || msg == NULL || len <= 0) {
		return;
	}

	if (prefix) {
		(void)write(fd, prefix, strlen(prefix));
	}
	(void)write(fd, msg, (size_t)len);
	if (msg[len - 1] != '\n') {
		(void)write(fd, "\n", 1);
	}
}

static void
pd_logd_note(const char *msg)
{
	pd_logd_write_fd(console_fd, "PureDarwin logd: ", msg, (ssize_t)strlen(msg));
}

static void
pd_logd_note_kr(const char *msg, kern_return_t kr)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "%s: %d", msg, kr);
	pd_logd_note(buf);
}

static void
pd_logd_prepare_path(const char *path)
{
	char tmp[256];
	char *slash;

	if (strlen(path) >= sizeof(tmp)) {
		return;
	}

	strcpy(tmp, path);
	slash = strrchr(tmp, '/');
	if (slash == NULL || slash == tmp) {
		return;
	}
	*slash = '\0';
	(void)mkdir(tmp, 0755);
}

static int
pd_logd_bind_syslog_socket(void)
{
	struct sockaddr_un sun;
	int fd;

	pd_logd_prepare_path(_PATH_LOG);
	(void)unlink(_PATH_LOG);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		pd_logd_note("socket(AF_UNIX, SOCK_DGRAM) failed");
		return -1;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, _PATH_LOG, sizeof(sun.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		pd_logd_note("bind(/var/run/syslog) failed");
		(void)close(fd);
		return -1;
	}

	(void)chmod(_PATH_LOG, 0666);
	return fd;
}

static void
pd_logd_check_in(void)
{
	mach_port_t service_port = MACH_PORT_NULL;
	kern_return_t kr;

	kr = bootstrap_check_in(bootstrap_port, PD_LOGD_SERVICE, &service_port);
	if (kr == KERN_SUCCESS) {
		pd_logd_note("checked in com.apple.logd");
		return;
	}

	pd_logd_note_kr("bootstrap_check_in(com.apple.logd) failed; continuing with legacy syslog socket", kr);
}

int
main(void)
{
	char buf[8192];
	int syslog_fd;

	(void)signal(SIGPIPE, SIG_IGN);

	console_fd = open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY);
	pd_logd_prepare_path(PD_LOGD_LOGFILE);
	logfile_fd = open(PD_LOGD_LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);

	pd_logd_check_in();

	syslog_fd = pd_logd_bind_syslog_socket();
	if (syslog_fd < 0) {
		return 1;
	}

	pd_logd_note("listening on " _PATH_LOG);

	for (;;) {
		ssize_t len = recv(syslog_fd, buf, sizeof(buf) - 1, 0);
		if (len < 0) {
			if (errno == EINTR) {
				continue;
			}
			pd_logd_note("recv(/var/run/syslog) failed");
			sleep(1);
			continue;
		}
		if (len == 0) {
			continue;
		}

		buf[len] = '\0';
		pd_logd_write_fd(console_fd, NULL, buf, len);
		pd_logd_write_fd(logfile_fd, NULL, buf, len);
	}
}
