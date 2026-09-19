#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "wslproto.h"

#ifndef AF_VSOCK
#define AF_VSOCK 40
#endif
#define VMADDR_CID_ANY  0xffffffffU
#define VMADDR_CID_HOST 2U
#define VMADDR_PORT_ANY 0xffffffffU

struct sockaddr_vm {
	uint8_t svm_len;
	uint8_t svm_family;
	uint16_t svm_reserved1;
	uint32_t svm_port;
	uint32_t svm_cid;
} __attribute__((packed));

// A socket plus the non-transaction sequence counters WslService checks
struct channel {
	int fd;
	uint32_t sent;
	const char *name;
};

static void
logmsg(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	int n = snprintf(buf, sizeof(buf), "wslinit[%d]: ", getpid());
	va_start(ap, fmt);
	n += vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, ap);
	va_end(ap);
	if (n > (int)sizeof(buf) - 2) {
		n = (int)sizeof(buf) - 2;
	}
	buf[n++] = '\n';
	int fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	if (fd >= 0) {
		(void)write(fd, buf, (size_t)n);
		close(fd);
	}
}

static int
write_all(int fd, const void *data, size_t len)
{
	const uint8_t *p = data;
	while (len > 0) {
		ssize_t n = write(fd, p, len);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			return -1;
		}
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

static int
read_all(int fd, void *data, size_t len)
{
	uint8_t *p = data;
	while (len > 0) {
		ssize_t n = read(fd, p, len);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			return -1;
		}
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

static int
vsock_connect_host(uint32_t port)
{
	struct sockaddr_vm sa = {
		.svm_len = sizeof(sa), .svm_family = AF_VSOCK,
		.svm_port = port, .svm_cid = VMADDR_CID_HOST,
	};
	int fd = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		int err = errno;
		close(fd);
		errno = err;
		return -1;
	}
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return fd;
}

static int
vsock_listen_any(uint32_t *port, int backlog)
{
	struct sockaddr_vm sa = {
		.svm_len = sizeof(sa), .svm_family = AF_VSOCK,
		.svm_port = VMADDR_PORT_ANY, .svm_cid = VMADDR_CID_ANY,
	};
	socklen_t len = sizeof(sa);
	int fd = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 || listen(fd, backlog) < 0 ||
	    getsockname(fd, (struct sockaddr *)&sa, &len) < 0) {
		logmsg("listen: %s", strerror(errno));
		close(fd);
		return -1;
	}
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	*port = sa.svm_port;
	return fd;
}

static int
accept_timeout(int lfd, int seconds)
{
	struct pollfd p = { .fd = lfd, .events = POLLIN };
	if (poll(&p, 1, seconds * 1000) <= 0) {
		return -1;
	}
	int fd = accept(lfd, NULL, NULL);
	if (fd >= 0) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	}
	return fd;
}

// Receives one message. The caller frees *out. Returns its size or -1 when closed
static ssize_t
recv_msg(struct channel *ch, struct wsl_header **out)
{
	struct wsl_header hdr;
	if (read_all(ch->fd, &hdr, sizeof(hdr)) < 0 || hdr.size < sizeof(hdr) || hdr.size > (1U << 24)) {
		return -1;
	}
	struct wsl_header *msg = malloc(hdr.size + 1);
	if (msg == NULL) {
		return -1;
	}
	memcpy(msg, &hdr, sizeof(hdr));
	if (read_all(ch->fd, msg + 1, hdr.size - sizeof(hdr)) < 0) {
		free(msg);
		return -1;
	}
	((char *)msg)[hdr.size] = 0;
	*out = msg;
	return hdr.size;
}

static int
send_msg(struct channel *ch, struct wsl_header *msg)
{
	msg->transaction_id = ++ch->sent;
	msg->transaction_step = STEP_NONE;
	return write_all(ch->fd, msg, msg->size);
}

static int
send_reply(struct channel *ch, const struct wsl_header *request, struct wsl_header *msg)
{
	msg->transaction_id = request->transaction_id;
	msg->transaction_step = STEP_FIRST_REPLY;
	return write_all(ch->fd, msg, msg->size);
}

static int
send_reply_step(struct channel *ch, const struct wsl_header *request, struct wsl_header *msg, uint32_t step)
{
	msg->transaction_id = request->transaction_id;
	msg->transaction_step = step;
	return write_all(ch->fd, msg, msg->size);
}

static const char *
msg_string(const struct wsl_header *msg, size_t base, uint32_t offset)
{
	if (offset == 0 || base + offset >= msg->size) {
		return "";
	}
	return (const char *)msg + base + offset;
}

static void
run_child(const struct wsl_create_process *cp)
{
	const size_t common = offsetof(struct wsl_create_process, filename_offset);
	const struct wsl_header *hdr = &cp->hdr;
	char *argv[64];
	int argc = 0;

	const char *arg = msg_string(hdr, common, cp->command_line_offset);
	for (unsigned i = 0; i < cp->command_line_count && argc < 63; i++) {
		argv[argc++] = (char *)arg;
		arg += strlen(arg) + 1;
	}
	argv[argc] = NULL;

	const char *cwd = msg_string(hdr, common, cp->cwd_offset);
	if (cwd[0] == '/') {
		(void)chdir(cwd);
	} else {
		(void)chdir("/var/root");
	}
	setenv("TERM", "xterm-256color", 1);
	setenv("HOME", "/var/root", 1);
	setenv("USER", "root", 1);
	setenv("SHELL", "/bin/zsh", 1);
	setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);

	const char *file = msg_string(hdr, common, cp->filename_offset);
	if (argc == 0) {
		execl("/bin/zsh", "-zsh", (char *)NULL);
		execl("/bin/sh", "-sh", (char *)NULL);
	} else if (file[0] != 0) {
		execvp(file, argv);
	} else {
		execvp(argv[0], argv);
	}
	fprintf(stderr, "wslinit: exec failed: %s\n", strerror(errno));
	_exit(127);
}

// Relay one process between its pty and the service's stdio/control sockets
static void
relay_process(struct wsl_header *msg, int lfd)
{
	const struct wsl_create_process *cp = (const struct wsl_create_process *)msg;
	int nsock = WSL_PROCESS_SOCKETS + ((cp->flags & LxInitCreateProcessFlagAllowOOBE) ? 1 : 0);
	int socks[6];

	for (int i = 0; i < nsock; i++) {
		socks[i] = accept_timeout(lfd, 30);
		if (socks[i] < 0) {
			logmsg("process socket %d not connected", i);
			_exit(1);
		}
	}
	close(lfd);

	int master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
		logmsg("pty: %s", strerror(errno));
		_exit(1);
	}
	fcntl(master, F_SETFD, FD_CLOEXEC);
	struct winsize ws = { .ws_row = cp->rows, .ws_col = cp->columns };
	(void)ioctl(master, TIOCSWINSZ, &ws);
	const char *slave_name = ptsname(master);

	pid_t child = fork();
	if (child == 0) {
		setsid();
		int slave = open(slave_name, O_RDWR);
		if (slave < 0) {
			_exit(126);
		}
		(void)ioctl(slave, TIOCSCTTY, 0);
		dup2(slave, 0);
		dup2(slave, 1);
		dup2(slave, 2);
		if (slave > 2) {
			close(slave);
		}
		run_child(cp);
	}
	logmsg("process %d started (%ux%u)", child, cp->columns, cp->rows);

	struct channel control = { socks[4], 0, "Control" };
	char buf[16384];
	bool stdin_open = true;
	for (;;) {
		struct pollfd fds[3] = {
			{ .fd = stdin_open ? socks[0] : -1, .events = POLLIN },
			{ .fd = master, .events = POLLIN },
			{ .fd = socks[3], .events = POLLIN },
		};
		int status;
		if (waitpid(child, &status, WNOHANG) == child) {
			// Flush what the shell wrote before it exited
			ssize_t n;
			fcntl(master, F_SETFL, O_NONBLOCK);
			while ((n = read(master, buf, sizeof(buf))) > 0) {
				(void)write_all(socks[1], buf, (size_t)n);
			}
			struct wsl_exit_status es = {
				.hdr = { .type = LxInitMessageExitStatus, .size = sizeof(es) },
				.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status),
			};
			(void)send_msg(&control, &es.hdr);
			struct wsl_header *echo = NULL;
			if (recv_msg(&control, &echo) > 0) {
				free(echo);
			}
			shutdown(socks[0], SHUT_RD);
			shutdown(socks[1], SHUT_WR);
			shutdown(socks[2], SHUT_WR);
			logmsg("process %d exited", child);
			_exit(0);
		}
		if (poll(fds, 3, 200) < 0 && errno != EINTR) {
			break;
		}
		if (fds[0].revents & (POLLIN | POLLHUP)) {
			ssize_t n = read(socks[0], buf, sizeof(buf));
			if (n <= 0) {
				stdin_open = false;
			} else {
				(void)write_all(master, buf, (size_t)n);
			}
		}
		if (fds[1].revents & (POLLIN | POLLHUP)) {
			ssize_t n = read(master, buf, sizeof(buf));
			if (n > 0 && write_all(socks[1], buf, (size_t)n) < 0) {
				kill(child, SIGHUP);
			}
		}
		if (fds[2].revents & POLLIN) {
			struct channel tc = { socks[3], 0, "TerminalControl" };
			struct wsl_header *m = NULL;
			if (recv_msg(&tc, &m) < 0) {
				fds[2].fd = -1;
				socks[3] = -1;
			} else {
				if (m->type == LxInitMessageWindowSizeChanged && m->size >= sizeof(struct wsl_window_size)) {
					const struct wsl_window_size *w = (const struct wsl_window_size *)m;
					struct winsize nws = { .ws_row = w->rows, .ws_col = w->columns };
					(void)ioctl(master, TIOCSWINSZ, &nws);
				}
				free(m);
			}
		}
	}
	kill(child, SIGHUP);
	_exit(1);
}

// A session leader owns one service connection and spawns processes on request
static void
session_leader(int lfd)
{
	int fd = accept_timeout(lfd, 30);
	close(lfd);
	if (fd < 0) {
		logmsg("session connection not made");
		_exit(1);
	}
	struct channel ch = { fd, 0, "SessionLeader" };
	for (;;) {
		struct wsl_header *m = NULL;
		if (recv_msg(&ch, &m) < 0) {
			_exit(0);
		}
		if (m->type == LxInitMessageCreateProcessUtilityVm && m->size >= sizeof(struct wsl_create_process)) {
			uint32_t port = WSL_INVALID_PORT;
			int plfd = vsock_listen_any(&port, WSL_PROCESS_SOCKETS + 1);
			struct wsl_result_uint32 r = {
				.hdr = { .type = LxMessageResultUint32, .size = sizeof(r) },
				.result = plfd < 0 ? WSL_INVALID_PORT : port,
			};
			(void)send_reply(&ch, m, &r.hdr);
			if (plfd >= 0 && fork() == 0) {
				close(fd);
				relay_process(m, plfd);
			}
			if (plfd >= 0) {
				close(plfd);
			}
		} else {
			logmsg("session: unhandled message %u", m->type);
		}
		free(m);
	}
}


#define PIVOT_SYSCALL   537     /* pivot_root(new_root_before, old_root_after) */
#define DISTRO_MOUNT    "/mnt/root"
#define STUB_AFTER      "private/wsl-boot"
#define MAX_DISKS       64

// Copies the string value of "key" from a flat JSON object. Returns false when absent
static bool
json_string(const char *json, const char *key, char *out, size_t len)
{
	char pat[64];
	snprintf(pat, sizeof(pat), "\"%s\":\"", key);
	const char *p = strstr(json, pat);
	if (p == NULL) {
		return false;
	}
	p += strlen(pat);
	size_t n = 0;
	while (p[n] != 0 && p[n] != '"' && n + 1 < len) {
		n++;
	}
	memcpy(out, p, n);
	out[n] = 0;
	return true;
}

static long
json_number(const char *json, const char *key, long fallback)
{
	char pat[64];
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	const char *p = strstr(json, pat);
	return p != NULL ? strtol(p + strlen(pat), NULL, 10) : fallback;
}

// The NAT configuration from WslService. Applied once the interface exists
static struct {
	char ip[64], gateway[64], dns[256], mac[32];
	int prefix;
} net_config;

// Finds the interface whose link address matches "00-15-5D-.."
static bool
find_interface(const char *mac, char *name, size_t len)
{
	unsigned b[6];
	if (sscanf(mac, "%x-%x-%x-%x-%x-%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
		return false;
	}
	struct ifaddrs *list, *ifa;
	bool found = false;
	if (getifaddrs(&list) != 0) {
		return false;
	}
	for (ifa = list; ifa != NULL && !found; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_LINK) {
			continue;
		}
		const struct sockaddr_dl *sdl = (const struct sockaddr_dl *)ifa->ifa_addr;
		const unsigned char *lladdr = (const unsigned char *)LLADDR(sdl);
		if (sdl->sdl_alen != 6) {
			continue;
		}
		found = true;
		for (int i = 0; i < 6; i++) {
			found = found && lladdr[i] == b[i];
		}
		if (found) {
			strlcpy(name, ifa->ifa_name, len);
		}
	}
	freeifaddrs(list);
	return found;
}

static void
fill_sin(struct sockaddr_in *sin, in_addr_t addr)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
}

static int
add_default_route(in_addr_t gateway)
{
	struct {
		struct rt_msghdr hdr;
		struct sockaddr_in dst, gw, mask;
	} msg;
	memset(&msg, 0, sizeof(msg));
	msg.hdr.rtm_msglen = sizeof(msg);
	msg.hdr.rtm_version = RTM_VERSION;
	msg.hdr.rtm_type = RTM_ADD;
	msg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	msg.hdr.rtm_seq = 1;
	fill_sin(&msg.dst, INADDR_ANY);
	fill_sin(&msg.gw, gateway);
	fill_sin(&msg.mask, INADDR_ANY);
	int fd = socket(PF_ROUTE, SOCK_RAW, AF_INET);
	if (fd < 0) {
		return -1;
	}
	int ret = write(fd, &msg, sizeof(msg)) == (ssize_t)sizeof(msg) ? 0 : -1;
	close(fd);
	return ret;
}

// Brings the adapter up with the service's address and default route
static void
apply_network(void)
{
	char ifname[IFNAMSIZ];
	int tries;
	for (tries = 0; tries < 600 && !find_interface(net_config.mac, ifname, sizeof(ifname)); tries++) {
		sleep(1);
	}
	if (tries == 600) {
		logmsg("no interface with MAC %s", net_config.mac);
		return;
	}

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
		ifr.ifr_flags |= IFF_UP;
		(void)ioctl(fd, SIOCSIFFLAGS, &ifr);
	}

	struct ifaliasreq ifra;
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	in_addr_t addr = inet_addr(net_config.ip);
	in_addr_t mask = net_config.prefix <= 0 ? 0 : htonl(0xffffffffUL << (32 - net_config.prefix));
	fill_sin((struct sockaddr_in *)&ifra.ifra_addr, addr);
	fill_sin((struct sockaddr_in *)&ifra.ifra_mask, mask);
	fill_sin((struct sockaddr_in *)&ifra.ifra_broadaddr, addr | ~mask);
	int err = ioctl(fd, SIOCAIFADDR, &ifra) == 0 ? 0 : errno;
	close(fd);

	int rerr = net_config.gateway[0] != 0 && add_default_route(inet_addr(net_config.gateway)) < 0 ? errno : 0;
	logmsg("%s: %s/%d via %s (address %s, route %s)", ifname, net_config.ip, net_config.prefix,
	    net_config.gateway, err == 0 ? "ok" : strerror(err), rerr == 0 ? "ok" : strerror(rerr));
}

// resolv.conf lives on the distribution, so it is written after the pivot
static void
write_resolv_conf(void)
{
	if (net_config.dns[0] == 0) {
		return;
	}
	FILE *f = fopen("/etc/resolv.conf", "w");
	if (f == NULL) {
		return;
	}
	fprintf(f, "# Generated by wslinit from the WSL networking configuration.\n");
	char list[256];
	strlcpy(list, net_config.dns, sizeof(list));
	for (char *tok = strtok(list, ","); tok != NULL; tok = strtok(NULL, ",")) {
		fprintf(f, "nameserver %s\n", tok);
	}
	fclose(f);
}

// The networking channel: every request waits for a result, so answer each one
static void
gns_service(int fd)
{
	struct channel ch = { fd, 0, "GNS" };
	for (;;) {
		struct wsl_header *m = NULL;
		if (recv_msg(&ch, &m) < 0) {
			logmsg("networking channel closed");
			_exit(0);
		}
		size_t json = m->type == LxGnsMessageNotification ? sizeof(*m) + 16 : sizeof(*m);
		const char *body = m->size > json ? (const char *)m + json : "";
		if (m->type == LxGnsMessageInterfaceConfiguration &&
		    json_string(body, "IPAddress", net_config.ip, sizeof(net_config.ip)) &&
		    json_string(body, "MacAddress", net_config.mac, sizeof(net_config.mac))) {
			json_string(body, "GatewayAddress", net_config.gateway, sizeof(net_config.gateway));
			json_string(body, "DNSServerList", net_config.dns, sizeof(net_config.dns));
			net_config.prefix = (int)json_number(body, "PrefixLength", 24);
			if (fork() == 0) {
				close(fd);
				apply_network();
				// The stub shows up here once the distribution is root
				while (access("/" STUB_AFTER "/sbin/launchd", F_OK) != 0) {
					sleep(1);
				}
				write_resolv_conf();
				_exit(0);
			}
		} else {
			logmsg("gns %u: %.200s", m->type, body);
		}
		if (m->transaction_step == STEP_REQUEST) {
			struct wsl_gns_result r = {
				.hdr = { .type = LxGnsMessageResult, .size = sizeof(r) },
				.result = 0,
			};
			(void)send_reply(&ch, m, &r.hdr);
		}
		free(m);
	}
}

// The per-distribution init: the service's instance channel
static void
distro_init(int fd, int result)
{
	struct channel ch = { fd, 0, "Init" };
	struct wsl_create_instance_result res = {
		.hdr = { .type = LxMiniInitMessageCreateInstanceResult, .size = sizeof(res) },
		.result = result,
		.failure_step = result == 0 ? 0 : 1,       // LxInitCreateInstanceStepMountDisk
		.pid = (uint64_t)getpid(),
		.connect_port = 0,
	};
	if (send_msg(&ch, &res.hdr) < 0 || result != 0) {
		logmsg("instance not started (%d)", result);
		_exit(1);
	}
	for (;;) {
		struct wsl_header *m = NULL;
		if (recv_msg(&ch, &m) < 0) {
			logmsg("instance channel closed");
			_exit(0);
		}
		switch (m->type) {
		case LxInitMessageInitialize: {
			static const char flavor[] = "puredarwin";
			static const char version[] = "26.5";
			size_t base = offsetof(struct wsl_initialize_response, buffer);
			size_t size = base + sizeof(flavor) + sizeof(version);
			struct wsl_initialize_response *r = calloc(1, size);
			r->hdr.type = LxInitMessageInitializeResponse;
			r->hdr.size = (uint32_t)size;
			r->plan9_port = WSL_INVALID_PORT;
			r->default_uid = 0;
			r->interop_port = WSL_INVALID_PORT;
			r->flavor_index = (uint32_t)base;
			memcpy((char *)r + base, flavor, sizeof(flavor));
			r->version_index = (uint32_t)(base + sizeof(flavor));
			memcpy((char *)r + base + sizeof(flavor), version, sizeof(version));
			(void)send_reply(&ch, m, &r->hdr);
			free(r);
			logmsg("instance initialized");
			break;
		}
		case LxInitMessageCreateSession: {
			uint32_t port = WSL_INVALID_PORT;
			int lfd = vsock_listen_any(&port, 1);
			struct wsl_create_session_response r = {
				.hdr = { .type = LxInitMessageCreateSessionResponse, .size = sizeof(r) },
				.port = lfd < 0 ? WSL_INVALID_PORT : port,
			};
			(void)send_reply(&ch, m, &r.hdr);
			if (lfd >= 0 && fork() == 0) {
				close(fd);
				session_leader(lfd);
			}
			if (lfd >= 0) {
				close(lfd);
			}
			logmsg("session leader on port %u", port);
			break;
		}
		case LxInitMessageTerminateInstance: {
			struct wsl_result_bool r = {
				.hdr = { .type = LxMessageResultBool, .size = sizeof(r) },
				.result = true,
			};
			(void)send_reply(&ch, m, &r.hdr);
			break;
		}
		case LxInitMessageRemountDrvfs: {
			// No drvfs yet: report failure so the service logs it and carries on
			struct wsl_result_int32 r = {
				.hdr = { .type = LxMessageResultInt32, .size = sizeof(r) },
				.result = ENOTSUP,
			};
			(void)send_reply(&ch, m, &r.hdr);
			break;
		}
		case LxInitMessageTimezoneInformation:
			break;
		default:
			logmsg("init: unhandled message %u (step %u)", m->type, m->transaction_step);
			break;
		}
		free(m);
	}
}


struct generic_mount_args {
	char *fspec;
};

// Whole disks present when the stub started. The distribution's disk is the one that is new
static bool known_disks[MAX_DISKS];

static void
scan_disks(bool *present)
{
	DIR *d = opendir("/dev");
	struct dirent *e;
	memset(present, 0, MAX_DISKS * sizeof(*present));
	while (d != NULL && (e = readdir(d)) != NULL) {
		char *end;
		if (strncmp(e->d_name, "disk", 4) != 0) {
			continue;
		}
		long n = strtol(e->d_name + 4, &end, 10);
		if (end != e->d_name + 4 && *end == 0 && n >= 0 && n < MAX_DISKS) {
			present[n] = true;
		}
	}
	if (d != NULL) {
		closedir(d);
	}
}

static bool
is_ext4(int n)
{
	char path[32];
	uint8_t sb[4096];
	snprintf(path, sizeof(path), "/dev/rdisk%d", n);
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	bool ok = read(fd, sb, sizeof(sb)) == (ssize_t)sizeof(sb) && sb[1080] == 0x53 && sb[1081] == 0xef;
	close(fd);
	return ok;
}

// Waits for the disk WslService attached for LaunchInit to show up
static int
find_distro_disk(char *path, size_t len)
{
	for (int tries = 0; tries < 120; tries++) {
		bool now[MAX_DISKS];
		scan_disks(now);
		for (int n = 0; n < MAX_DISKS; n++) {
			if (now[n] && !known_disks[n] && is_ext4(n)) {
				snprintf(path, len, "/dev/disk%d", n);
				return 0;
			}
		}
		sleep(1);
	}
	return -1;
}

// PID 1 of the stub: mount the distribution, pivot onto it, become its launchd
static void
stub_pid1(int request_fd, int ack_fd)
{
	char dev[64];
	for (;;) {
		ssize_t n = read(request_fd, dev, sizeof(dev) - 1);
		if (n <= 0) {
			while (waitpid(-1, NULL, 0) > 0 || errno == EINTR) {
			}
			pause();
			continue;
		}
		dev[n] = 0;
		struct generic_mount_args args = { .fspec = dev };
		int err = 0;
		(void)mkdir("/mnt", 0755);
		(void)mkdir(DISTRO_MOUNT, 0755);
		if (mount("ext4", DISTRO_MOUNT, 0, &args) < 0) {
			err = errno;
			logmsg("mounting %s: %s", dev, strerror(err));
		} else {
			(void)mkdir(DISTRO_MOUNT "/private", 0755);
			(void)mkdir(DISTRO_MOUNT "/" STUB_AFTER, 0755);
			if (syscall(PIVOT_SYSCALL, DISTRO_MOUNT, STUB_AFTER) < 0) {
				err = errno;
				logmsg("pivot_root: %s", strerror(err));
			}
		}
		uint8_t ack = err == 0 ? 0 : (uint8_t)err;
		(void)write(ack_fd, &ack, 1);
		if (err == 0) {
			logmsg("%s is now /, starting launchd", dev);
			close(request_fd);
			close(ack_fd);
			execl("/sbin/launchd", "launchd", (char *)NULL);
			logmsg("exec /sbin/launchd: %s", strerror(errno));
		}
	}
}

static int pivot_request_fd = -1;
static int pivot_ack_fd = -1;

// Returns 0 once the distribution's disk is the root, or an errno
static int
prepare_distro_root(void)
{
	if (pivot_request_fd < 0) {
		return 0;       // Not the stub: already running on the distribution's root
	}
	char dev[64];
	if (find_distro_disk(dev, sizeof(dev)) < 0) {
		logmsg("no distribution disk appeared");
		return ENODEV;
	}
	if (write(pivot_request_fd, dev, strlen(dev)) < 0) {
		return EIO;
	}
	uint8_t ack;
	if (read(pivot_ack_fd, &ack, 1) != 1) {
		return EIO;
	}
	if (ack == 0) {
		close(pivot_request_fd);
		close(pivot_ack_fd);
		pivot_request_fd = pivot_ack_fd = -1;
	}
	return ack;
}

int
main(void)
{
	signal(SIGPIPE, SIG_IGN);

	if (getpid() == 1) {
		int req[2], ack[2];
		scan_disks(known_disks);
		if (pipe(req) < 0 || pipe(ack) < 0) {
			logmsg("pipe: %s", strerror(errno));
			for (;;) {
				pause();
			}
		}
		pid_t child = fork();
		if (child > 0) {
			close(req[1]);
			close(ack[0]);
			stub_pid1(req[0], ack[1]);
		}
		close(req[0]);
		close(ack[1]);
		pivot_request_fd = req[1];
		pivot_ack_fd = ack[0];
	}
	signal(SIGCHLD, SIG_IGN);

	// Not running under WSL (or the Hyper-V socket transport is missing): nothing to do
	int fd = -1;
	for (int i = 0; i < 10 && fd < 0; i++) {
		fd = vsock_connect_host(WSL_INIT_PORT);
		if (fd < 0) {
			sleep(1);
		}
	}
	if (fd < 0) {
		logmsg("no WSL service (%s)", strerror(errno));
		return 0;
	}
	struct channel mini = { fd, 0, "mini_init" };

	static const char release[] = "6.6.0";
	size_t caps_size = WSL_CAPS_VERSION_OFFSET_NEXT + sizeof(release);
	struct wsl_guest_capabilities *caps = calloc(1, caps_size);
	caps->hdr.type = LxMiniInitMessageGuestCapabilities;
	caps->hdr.size = (uint32_t)caps_size;
	memcpy((char *)caps + WSL_CAPS_VERSION_OFFSET_2_7, release, sizeof(release));
	memcpy((char *)caps + WSL_CAPS_VERSION_OFFSET_NEXT, release, sizeof(release));
	int notify = vsock_connect_host(WSL_INIT_PORT);
	if (send_msg(&mini, &caps->hdr) < 0 || notify < 0) {
		logmsg("handshake failed: %s", strerror(errno));
		return 1;
	}
	free(caps);
	logmsg("connected to WslService");

	int keep[8];
	int nkeep = 0;
	for (;;) {
		struct wsl_header *m = NULL;
		if (recv_msg(&mini, &m) < 0) {
			logmsg("service channel closed");
			return 0;
		}
		switch (m->type) {
		case LxMiniInitMessageEarlyConfig: {
			const struct wsl_early_config *ec = (const struct wsl_early_config *)m;
			// The service waits for the networking channel, and DNS tunnelling if enabled
			int gns = vsock_connect_host(WSL_INIT_PORT);
			if (gns >= 0 && fork() == 0) {
				close(mini.fd);
				gns_service(gns);
			}
			if (gns >= 0) {
				close(gns);
			}
			if (m->size >= sizeof(*ec) && ec->enable_dns_tunneling) {
				int dns = vsock_connect_host(WSL_INIT_PORT);
				if (dns >= 0 && nkeep < 8) {
					keep[nkeep++] = dns;
				}
			}
			logmsg("early config: hostname %s", msg_string(m, 0, ec->hostname_offset));
			break;
		}
		case LxMiniInitMessageInitialConfig: {
			const struct wsl_initial_config *ic = (const struct wsl_initial_config *)m;
			if (m->size >= sizeof(*ic) && ic->port_tracker_type != 0) {
				int pt = vsock_connect_host(WSL_INIT_PORT);
				if (pt >= 0 && nkeep < 8) {
					keep[nkeep++] = pt;
				}
			}
			logmsg("initial config: networking mode %u", ic->networking_mode);
			break;
		}
		case LxMiniInitMessageLaunchInit: {
			const struct wsl_launch_init *li = (const struct wsl_launch_init *)m;
			int ifd = vsock_connect_host(WSL_INIT_PORT);
			if (ifd < 0) {
				logmsg("init channel: %s", strerror(errno));
				break;
			}
			logmsg("launching distribution %s", msg_string(m, 0, li->distribution_name_offset));
			int root = prepare_distro_root();
			if (fork() == 0) {
				close(mini.fd);
				distro_init(ifd, root);
			}
			close(ifd);
			break;
		}
		case LxInitCreateProcess: {
			// Only the telemetry agent is asked for. accept its socket and let it idle
			const struct wsl_root_create_process *rp = (const struct wsl_root_create_process *)m;
			uint32_t port = WSL_INVALID_PORT;
			int lfd = vsock_listen_any(&port, 1);
			struct wsl_result_int32 r = {
				.hdr = { .type = LxMessageResultInt32, .size = sizeof(r) },
				.result = (int32_t)port,
			};
			(void)send_reply_step(&mini, m, &r.hdr, STEP_FIRST_REPLY);
			int pfd = lfd >= 0 ? accept_timeout(lfd, 30) : -1;
			if (lfd >= 0) {
				close(lfd);
			}
			if (pfd >= 0 && nkeep < 8) {
				keep[nkeep++] = pfd;
			}
			r.result = pfd >= 0 ? 0 : -1;
			(void)send_reply_step(&mini, m, &r.hdr, STEP_FIRST_REPLY + 1);
			logmsg("root process %s not run (socket %s)", msg_string(m, 0, rp->path_index),
			    pfd >= 0 ? "accepted" : "missing");
			break;
		}
		default:
			logmsg("mini_init: unhandled message %u (step %u)", m->type, m->transaction_step);
			break;
		}
		free(m);
	}
}
