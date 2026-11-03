/*
 * pdadbd: the device side of ADB over the USB gadget's /dev/adb.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257
#define A_VERSION 0x01000001
#define MAX_PAYLOAD (256 * 1024)
#define MAX_STREAMS 16
#define ADB_DEV "/dev/adb"

struct amessage {
	uint32_t command, arg0, arg1, data_length, data_check, magic;
};

struct stream {
	uint32_t local, remote;
	int fd;			// shell: pty master or stdout pipe; -1 for sync
	int in;			// where host data goes (same as fd for a pty)
	pid_t pid;
	int sync;
	int canWrite;		// the host acknowledged our last WRTE
	uint8_t *out;		// queued bytes for the host (sync replies)
	size_t outLen, outCap;
	uint8_t *req;		// sync requests being assembled from WRTE payloads
	size_t reqLen, reqCap;
	int sendFd;		// SEND in progress
	char sendPath[1024];
	int recvFd;		// RECV in progress
	int closing;
};

static int sAdb = -1;
static uint32_t sMaxData = 4096;
static uint32_t sNextLocal = 1;
static struct stream sStreams[MAX_STREAMS];
static uint8_t sBuf[MAX_PAYLOAD];

static int
readFull(int fd, void *p, size_t n)
{
	uint8_t *b = p;

	while (n > 0) {
		ssize_t r = read(fd, b, n);

		if (r < 0 && errno == EINTR) {
			continue;
		}
		if (r <= 0) {
			return -1;
		}
		b += r;
		n -= (size_t)r;
	}
	return 0;
}

static int
writeFull(int fd, const void *p, size_t n)
{
	const uint8_t *b = p;

	while (n > 0) {
		ssize_t r = write(fd, b, n);

		if (r < 0 && errno == EINTR) {
			continue;
		}
		if (r <= 0) {
			return -1;
		}
		b += r;
		n -= (size_t)r;
	}
	return 0;
}

// Header and payload go out as separate writes, so each is one USB transfer
static int
sendPacket(uint32_t cmd, uint32_t arg0, uint32_t arg1, const void *data, uint32_t len)
{
	struct amessage m = { cmd, arg0, arg1, len, 0, cmd ^ 0xffffffff };
	const uint8_t *d = data;

	for (uint32_t i = 0; i < len; i++) {
		m.data_check += d[i];
	}
	if (writeFull(sAdb, &m, sizeof(m)) != 0) {
		return -1;
	}
	return len > 0 ? writeFull(sAdb, data, len) : 0;
}

static struct stream *
findStream(uint32_t local)
{
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (sStreams[i].local == local && local != 0) {
			return &sStreams[i];
		}
	}
	return NULL;
}

static void
closeStream(struct stream *s, int tellHost)
{
	if (tellHost) {
		sendPacket(A_CLSE, s->local, s->remote, NULL, 0);
	}
	if (s->pid > 0) {
		kill(s->pid, SIGHUP);
	}
	if (s->in >= 0 && s->in != s->fd) {
		close(s->in);
	}
	if (s->fd >= 0) {
		close(s->fd);
	}
	if (s->sendFd >= 0) {
		close(s->sendFd);
	}
	if (s->recvFd >= 0) {
		close(s->recvFd);
	}
	free(s->out);
	free(s->req);
	memset(s, 0, sizeof(*s));
	s->fd = s->in = s->sendFd = s->recvFd = -1;
}

static void
resetAll(void)
{
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (sStreams[i].local != 0) {
			closeStream(&sStreams[i], 0);
		}
	}
}

static struct stream *
newStream(uint32_t remote)
{
	for (int i = 0; i < MAX_STREAMS; i++) {
		struct stream *s = &sStreams[i];

		if (s->local == 0) {
			memset(s, 0, sizeof(*s));
			s->fd = s->in = s->sendFd = s->recvFd = -1;
			s->local = sNextLocal++;
			if (sNextLocal == 0) {
				sNextLocal = 1;
			}
			s->remote = remote;
			s->canWrite = 1;
			return s;
		}
	}
	return NULL;
}

static void
childEnv(void)
{
	setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
	setenv("HOME", "/var/root", 1);
	setenv("TERM", "vt220", 0);
	setenv("SHELL", "/bin/zsh", 1);
	if (chdir("/var/root") != 0) {
		chdir("/");
	}
}

// Interactive shell on a pty; a command runs on pipes so its output stays byte-exact
static int
startShell(struct stream *s, const char *cmd)
{
	if (cmd == NULL || *cmd == 0) {
		int m = posix_openpt(O_RDWR | O_NOCTTY);

		if (m >= 0 && grantpt(m) == 0 && unlockpt(m) == 0) {
			const char *name = ptsname(m);
			pid_t pid = fork();

			if (pid == 0) {
				int t;

				setsid();
				t = open(name, O_RDWR);
				if (t < 0) {
					_exit(127);
				}
				ioctl(t, TIOCSCTTY, 0);
				dup2(t, 0);
				dup2(t, 1);
				dup2(t, 2);
				if (t > 2) {
					close(t);
				}
				close(m);
				childEnv();
				execl("/bin/zsh", "-zsh", (char *)NULL);
				execl("/bin/sh", "-sh", (char *)NULL);
				_exit(127);
			}
			if (pid > 0) {
				s->fd = s->in = m;
				s->pid = pid;
				return 0;
			}
		}
		if (m >= 0) {
			close(m);
		}
	}
	int outp[2], inp[2];

	if (pipe(outp) != 0 || pipe(inp) != 0) {
		return -1;
	}
	pid_t pid = fork();

	if (pid == 0) {
		dup2(inp[0], 0);
		dup2(outp[1], 1);
		dup2(outp[1], 2);
		close(inp[0]);
		close(inp[1]);
		close(outp[0]);
		close(outp[1]);
		setsid();
		childEnv();
		if (cmd != NULL && *cmd != 0) {
			execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		} else {
			execl("/bin/sh", "sh", "-i", (char *)NULL);
		}
		_exit(127);
	}
	close(inp[0]);
	close(outp[1]);
	if (pid < 0) {
		close(inp[1]);
		close(outp[0]);
		return -1;
	}
	s->fd = outp[0];
	s->in = inp[1];
	s->pid = pid;
	return 0;
}

static void
queueOut(struct stream *s, const void *p, size_t n)
{
	if (s->outLen + n > s->outCap) {
		size_t cap = s->outCap ? s->outCap : 4096;

		while (cap < s->outLen + n) {
			cap *= 2;
		}
		s->out = realloc(s->out, cap);
		s->outCap = cap;
	}
	memcpy(s->out + s->outLen, p, n);
	s->outLen += n;
}

static void
syncHeader(struct stream *s, const char *id, uint32_t a)
{
	uint8_t h[8];

	memcpy(h, id, 4);
	memcpy(h + 4, &a, 4);
	queueOut(s, h, 8);
}

static void
syncFail(struct stream *s, const char *msg)
{
	syncHeader(s, "FAIL", (uint32_t)strlen(msg));
	queueOut(s, msg, strlen(msg));
}

static void
syncStat(struct stream *s, const char *path)
{
	struct stat st;
	uint32_t v[3] = { 0, 0, 0 };

	if (lstat(path, &st) == 0) {
		v[0] = st.st_mode;
		v[1] = (uint32_t)st.st_size;
		v[2] = (uint32_t)st.st_mtime;
	}
	queueOut(s, "STAT", 4);
	queueOut(s, v, sizeof(v));
}

static void
syncList(struct stream *s, const char *path)
{
	DIR *d = opendir(path);
	struct dirent *e;
	uint32_t z[4] = { 0, 0, 0, 0 };

	while (d != NULL && (e = readdir(d)) != NULL) {
		char full[2048];
		struct stat st;
		uint32_t v[4];

		snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
		if (lstat(full, &st) != 0) {
			continue;
		}
		v[0] = st.st_mode;
		v[1] = (uint32_t)st.st_size;
		v[2] = (uint32_t)st.st_mtime;
		v[3] = (uint32_t)strlen(e->d_name);
		queueOut(s, "DENT", 4);
		queueOut(s, v, sizeof(v));
		queueOut(s, e->d_name, v[3]);
	}
	if (d != NULL) {
		closedir(d);
	}
	queueOut(s, "DONE", 4);
	queueOut(s, z, sizeof(z));
}

// Consumes whole sync requests from the assembled WRTE payloads
static void
syncProcess(struct stream *s)
{
	for (;;) {
		uint32_t len;
		char id[5] = { 0 };

		if (s->reqLen < 8) {
			return;
		}
		memcpy(id, s->req, 4);
		memcpy(&len, s->req + 4, 4);
		if (s->sendFd >= 0 && strcmp(id, "DATA") == 0) {
			if (s->reqLen < 8 + (size_t)len) {
				return;
			}
			if (writeFull(s->sendFd, s->req + 8, len) != 0) {
				close(s->sendFd);
				s->sendFd = -1;
				syncFail(s, "write failed");
			}
		} else if (strcmp(id, "DONE") == 0 && s->sendPath[0] != 0) {
			// len is the mtime
			if (s->sendFd >= 0) {
				close(s->sendFd);
				s->sendFd = -1;
				syncHeader(s, "OKAY", 0);
			}
			s->sendPath[0] = 0;
			len = 0;
		} else {
			char path[1024];

			if (len >= sizeof(path)) {
				syncFail(s, "path too long");
				s->closing = 1;
				return;
			}
			if (s->reqLen < 8 + (size_t)len) {
				return;
			}
			memcpy(path, s->req + 8, len);
			path[len] = 0;
			if (strcmp(id, "STAT") == 0) {
				syncStat(s, path);
			} else if (strcmp(id, "LIST") == 0) {
				syncList(s, path);
			} else if (strcmp(id, "SEND") == 0) {
				char *comma = strrchr(path, ',');
				mode_t mode = 0644;

				if (comma != NULL) {
					*comma = 0;
					mode = (mode_t)strtoul(comma + 1, NULL, 10) & 07777;
				}
				snprintf(s->sendPath, sizeof(s->sendPath), "%s", path);
				s->sendFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode ? mode : 0644);
				if (s->sendFd < 0) {
					syncFail(s, strerror(errno));
				}
			} else if (strcmp(id, "RECV") == 0) {
				s->recvFd = open(path, O_RDONLY);
				if (s->recvFd < 0) {
					syncFail(s, strerror(errno));
				}
			} else if (strcmp(id, "QUIT") == 0) {
				s->closing = 1;
				return;
			} else {
				syncFail(s, "unknown sync request");
			}
		}
		memmove(s->req, s->req + 8 + len, s->reqLen - 8 - len);
		s->reqLen -= 8 + len;
	}
}

static void
hostData(struct stream *s, const uint8_t *p, uint32_t n)
{
	if (s->sync) {
		if (s->reqLen + n > s->reqCap) {
			s->reqCap = (s->reqLen + n) * 2;
			s->req = realloc(s->req, s->reqCap);
		}
		memcpy(s->req + s->reqLen, p, n);
		s->reqLen += n;
		syncProcess(s);
	} else if (s->in >= 0) {
		writeFull(s->in, p, n);
	}
}

static void
handleOpen(uint32_t remote, char *service)
{
	struct stream *s = newStream(remote);

	if (s == NULL) {
		sendPacket(A_CLSE, 0, remote, NULL, 0);
		return;
	}
	if (strncmp(service, "shell:", 6) == 0 || strncmp(service, "exec:", 5) == 0) {
		const char *cmd = strchr(service, ':') + 1;

		if (startShell(s, cmd) != 0) {
			closeStream(s, 0);
			sendPacket(A_CLSE, 0, remote, NULL, 0);
			return;
		}
	} else if (strncmp(service, "sync:", 5) == 0) {
		s->sync = 1;
	} else if (strncmp(service, "reboot:", 7) == 0) {
		// Acknowledge first so the host sees the request land, then go
		sendPacket(A_OKAY, s->local, s->remote, NULL, 0);
		sendPacket(A_CLSE, s->local, s->remote, NULL, 0);
		closeStream(s, 0);
		sync();
		usleep(200000);
		reboot(RB_AUTOBOOT);
		return;
	} else {
		closeStream(s, 0);
		sendPacket(A_CLSE, 0, remote, NULL, 0);
		return;
	}
	sendPacket(A_OKAY, s->local, s->remote, NULL, 0);
}

static void
handlePacket(struct amessage *m, uint8_t *data)
{
	struct stream *s;

	switch (m->command) {
	case A_CNXN: {
		static const char banner[] =
		    "device::ro.product.name=luckfox;ro.product.model=Luckfox Pico;"
		    "ro.product.device=luckfox;features=";

		resetAll();
		sMaxData = m->arg1 < MAX_PAYLOAD ? m->arg1 : MAX_PAYLOAD;
		sendPacket(A_CNXN, A_VERSION, MAX_PAYLOAD, banner, sizeof(banner));
		break;
	}
	case A_OPEN:
		data[m->data_length ? m->data_length - 1 : 0] = 0;
		handleOpen(m->arg0, (char *)data);
		break;
	case A_OKAY:
		if ((s = findStream(m->arg1)) != NULL) {
			s->remote = m->arg0;
			s->canWrite = 1;
		}
		break;
	case A_WRTE:
		if ((s = findStream(m->arg1)) != NULL) {
			hostData(s, data, m->data_length);
			sendPacket(A_OKAY, s->local, s->remote, NULL, 0);
		}
		break;
	case A_CLSE:
		if ((s = findStream(m->arg1)) != NULL) {
			closeStream(s, 0);
		}
		break;
	default:
		break;
	}
}

// Moves device-side output to the host, one WRTE per stream at a time
static void
pump(struct stream *s, int readable)
{
	if (!s->canWrite) {
		return;
	}
	if (s->sync) {
		// Refill from a RECV in progress while the queue runs low
		if (s->recvFd >= 0 && s->outLen < sMaxData) {
			uint32_t chunk = 64 * 1024 - 8;
			uint8_t *tmp = malloc(chunk);
			ssize_t r = read(s->recvFd, tmp, chunk);

			if (r > 0) {
				syncHeader(s, "DATA", (uint32_t)r);
				queueOut(s, tmp, (size_t)r);
			} else {
				close(s->recvFd);
				s->recvFd = -1;
				syncHeader(s, r == 0 ? "DONE" : "FAIL", 0);
			}
			free(tmp);
		}
		if (s->outLen > 0) {
			uint32_t n = s->outLen < sMaxData ? (uint32_t)s->outLen : sMaxData;

			sendPacket(A_WRTE, s->local, s->remote, s->out, n);
			memmove(s->out, s->out + n, s->outLen - n);
			s->outLen -= n;
			s->canWrite = 0;
		} else if (s->closing) {
			closeStream(s, 1);
		}
		return;
	}
	if (readable && s->fd >= 0) {
		ssize_t r = read(s->fd, sBuf, sMaxData);

		if (r > 0) {
			sendPacket(A_WRTE, s->local, s->remote, sBuf, (uint32_t)r);
			s->canWrite = 0;
		} else if (r == 0 || errno != EINTR) {
			closeStream(s, 1);
		}
	}
}

// One host session: runs until the gadget goes away (read fails with ENXIO)
static void
serve(void)
{
	for (;;) {
		fd_set rf;
		int maxfd = sAdb;

		FD_ZERO(&rf);
		FD_SET(sAdb, &rf);
		for (int i = 0; i < MAX_STREAMS; i++) {
			struct stream *s = &sStreams[i];

			if (s->local != 0 && !s->sync && s->canWrite && s->fd >= 0) {
				FD_SET(s->fd, &rf);
				if (s->fd > maxfd) {
					maxfd = s->fd;
				}
			}
		}
		if (select(maxfd + 1, &rf, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				continue;
			}
			return;
		}
		if (FD_ISSET(sAdb, &rf)) {
			struct amessage m;

			if (readFull(sAdb, &m, sizeof(m)) != 0) {
				return;
			}
			if (m.magic != (m.command ^ 0xffffffff) || m.data_length > MAX_PAYLOAD) {
				continue;
			}
			if (m.data_length > 0 && readFull(sAdb, sBuf, m.data_length) != 0) {
				return;
			}
			handlePacket(&m, sBuf);
		}
		for (int i = 0; i < MAX_STREAMS; i++) {
			struct stream *s = &sStreams[i];

			if (s->local != 0) {
				pump(s, !s->sync && s->fd >= 0 && FD_ISSET(s->fd, &rf));
			}
		}
	}
}

int
main(int argc, char **argv)
{
	struct sigaction sa;

	(void)argc;
	(void)argv;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
	for (int i = 0; i < MAX_STREAMS; i++) {
		sStreams[i].fd = sStreams[i].in = sStreams[i].sendFd = sStreams[i].recvFd = -1;
	}
	for (;;) {
		sAdb = open(ADB_DEV, O_RDWR);
		if (sAdb < 0) {
			sleep(1);
			continue;
		}
		serve();
		resetAll();
		close(sAdb);
		sAdb = -1;
		sleep(1);
	}
}
