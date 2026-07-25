/*
 * pd-networkd - early PureDarwin IPv4 network bring-up.
 *
 * This is still static IPv4 by default, but it is now a launchd-started
 * service instead of a manually invoked boot hack. DHCP can replace the static
 * lease loader without changing the interface/route/resolver apply path.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SA_SIZE
#define SA_SIZE(sa) \
    (((sa)->sa_len == 0) ? sizeof(long) : \
    (1 + (((sa)->sa_len - 1) | (sizeof(long) - 1))))
#endif

#define PD_NETWORKD_DEFAULT_IFACE "en0"
#define PD_NETWORKD_DEFAULT_ADDR "10.0.2.15"
#define PD_NETWORKD_DEFAULT_MASK "255.255.255.0"
#define PD_NETWORKD_DEFAULT_ROUTER "10.0.2.2"
#define PD_NETWORKD_DEFAULT_DNS "10.0.2.3"
#define PD_NETWORKD_DEFAULT_BROADCAST "10.0.2.255"
#define PD_NETWORKD_DEFAULT_WAIT_SECONDS 30

struct pd_network_config {
    const char *ifname;
    const char *addr;
    const char *mask;
    const char *router;
    const char *dns;
    const char *broadcast;
    int wait_seconds;
    int write_resolver;
};

static void
pd_log(const char *fmt, ...)
{
    va_list ap;

    printf("PureDarwin network: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static const char *
env_default(const char *name, const char *fallback)
{
    const char *value = getenv(name);

    return (value && value[0]) ? value : fallback;
}

static int
parse_ipv4(const char *text, struct sockaddr_in *sin)
{
    memset(sin, 0, sizeof(*sin));
    sin->sin_len = sizeof(*sin);
    sin->sin_family = AF_INET;
    if (inet_aton(text, &sin->sin_addr) == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static void
copy_sockaddr(struct sockaddr *dst, const struct sockaddr_in *src)
{
    memset(dst, 0, sizeof(*dst));
    memcpy(dst, src, sizeof(*src));
}

static int
get_flags(int sock, const char *ifname, short *flags)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
        return -1;

    *flags = ifr.ifr_flags;
    return 0;
}

static int
wait_for_interface(int sock, const char *ifname, int wait_seconds)
{
    short flags;

    for (int i = 0; i <= wait_seconds; i++) {
        if (get_flags(sock, ifname, &flags) == 0) {
            pd_log("%s is present flags=0x%x", ifname, flags);
            return 0;
        }

        if (i == wait_seconds)
            break;
        sleep(1);
    }

    pd_log("%s did not appear after %d seconds: %s",
        ifname, wait_seconds, strerror(errno));
    return -1;
}

static int
set_flags(int sock, const char *ifname, short set, short clear)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        pd_log("%s SIOCGIFFLAGS failed: %s", ifname, strerror(errno));
        return -1;
    }

    ifr.ifr_flags |= set;
    ifr.ifr_flags &= (short)~clear;

    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        pd_log("%s SIOCSIFFLAGS failed: %s", ifname, strerror(errno));
        return -1;
    }
    return 0;
}

static int
add_addr(int sock, const char *ifname, const char *addr, const char *mask, const char *broadcast)
{
    struct ifaliasreq ifra;
    struct sockaddr_in sin;

    memset(&ifra, 0, sizeof(ifra));
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));

    if (parse_ipv4(addr, &sin) < 0) {
        pd_log("invalid address %s", addr);
        return -1;
    }
    copy_sockaddr(&ifra.ifra_addr, &sin);

    if (parse_ipv4(mask, &sin) < 0) {
        pd_log("invalid netmask %s", mask);
        return -1;
    }
    copy_sockaddr(&ifra.ifra_mask, &sin);

    if (broadcast && broadcast[0]) {
        if (parse_ipv4(broadcast, &sin) < 0) {
            pd_log("invalid broadcast %s", broadcast);
            return -1;
        }
        copy_sockaddr(&ifra.ifra_broadaddr, &sin);
    }

    if (ioctl(sock, SIOCAIFADDR, &ifra) < 0) {
        if (errno == EEXIST) {
            pd_log("%s already has %s", ifname, addr);
            return 0;
        }
        pd_log("%s SIOCAIFADDR %s failed: %s", ifname, addr, strerror(errno));
        return -1;
    }

    pd_log("%s inet %s netmask %s%s%s",
        ifname, addr, mask,
        (broadcast && broadcast[0]) ? " broadcast " : "",
        (broadcast && broadcast[0]) ? broadcast : "");
    return 0;
}

static char *
append_sockaddr(char *cursor, const struct sockaddr *sa)
{
    size_t padded = SA_SIZE(sa);

    memset(cursor, 0, padded);
    memcpy(cursor, sa, sa->sa_len);
    return cursor + padded;
}

static int
add_default_route(const char *gateway)
{
    struct {
        struct rt_msghdr hdr;
        char addrs[256];
    } msg;
    struct sockaddr_in dst;
    struct sockaddr_in gw;
    struct sockaddr_in mask;
    char *cursor;
    int sock;
    ssize_t written;

    if (parse_ipv4("0.0.0.0", &dst) < 0 ||
        parse_ipv4(gateway, &gw) < 0 ||
        parse_ipv4("0.0.0.0", &mask) < 0) {
        pd_log("invalid gateway %s", gateway);
        return -1;
    }

    sock = socket(PF_ROUTE, SOCK_RAW, AF_INET);
    if (sock < 0) {
        pd_log("PF_ROUTE socket failed: %s", strerror(errno));
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    cursor = msg.addrs;
    cursor = append_sockaddr(cursor, (struct sockaddr *)&dst);
    cursor = append_sockaddr(cursor, (struct sockaddr *)&gw);
    cursor = append_sockaddr(cursor, (struct sockaddr *)&mask);

    msg.hdr.rtm_msglen = (unsigned short)(cursor - (char *)&msg);
    msg.hdr.rtm_version = RTM_VERSION;
    msg.hdr.rtm_type = RTM_ADD;
    msg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
    msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    msg.hdr.rtm_pid = getpid();
    msg.hdr.rtm_seq = 1;

    written = write(sock, &msg, msg.hdr.rtm_msglen);
    if (written < 0) {
        if (errno == EEXIST) {
            close(sock);
            pd_log("default route already exists");
            return 0;
        }
        pd_log("add default route via %s failed: %s", gateway, strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);
    pd_log("default route via %s", gateway);
    return 0;
}

static int
write_resolver_config(const char *dns)
{
    char buf[160];
    ssize_t len;
    int fd;

    if (!dns || !dns[0])
        return 0;

    mkdir("/etc", 0755);

    fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        pd_log("open /etc/resolv.conf failed: %s", strerror(errno));
        return -1;
    }

    len = snprintf(buf, sizeof(buf), "nameserver %s\n", dns);
    if (len < 0 || (size_t)len >= sizeof(buf) || write(fd, buf, (size_t)len) != len) {
        pd_log("write /etc/resolv.conf failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    pd_log("resolver nameserver %s", dns);
    return 0;
}

static int
apply_network_config(const struct pd_network_config *cfg)
{
    int sock;
    int rc = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        pd_log("AF_INET socket failed: %s", strerror(errno));
        return 1;
    }

    if (add_addr(sock, "lo0", "127.0.0.1", "255.0.0.0", "127.255.255.255") < 0)
        rc = 1;
    if (set_flags(sock, "lo0", IFF_UP, 0) < 0)
        rc = 1;

    if (wait_for_interface(sock, cfg->ifname, cfg->wait_seconds) < 0) {
        close(sock);
        return 1;
    }

    if (add_addr(sock, cfg->ifname, cfg->addr, cfg->mask, cfg->broadcast) < 0)
        rc = 1;
    if (set_flags(sock, cfg->ifname, IFF_UP, 0) < 0)
        rc = 1;

    close(sock);

    if (add_default_route(cfg->router) < 0)
        rc = 1;
    if (cfg->write_resolver && write_resolver_config(cfg->dns) < 0)
        rc = 1;

    return rc;
}

static void
usage(const char *prog)
{
    printf("usage:\n");
    printf("  %s [--wait SECONDS] [IFNAME IFADDR NETMASK GATEWAY [DNS [BROADCAST]]]\n", prog);
    printf("environment overrides:\n");
    printf("  PUREDARWIN_NET_IFACE PUREDARWIN_NET_ADDR PUREDARWIN_NET_MASK\n");
    printf("  PUREDARWIN_NET_ROUTER PUREDARWIN_NET_DNS PUREDARWIN_NET_BROADCAST\n");
}

int
main(int argc, char **argv)
{
    struct pd_network_config cfg;
    int argi = 1;

    cfg.ifname = env_default("PUREDARWIN_NET_IFACE", PD_NETWORKD_DEFAULT_IFACE);
    cfg.addr = env_default("PUREDARWIN_NET_ADDR", PD_NETWORKD_DEFAULT_ADDR);
    cfg.mask = env_default("PUREDARWIN_NET_MASK", PD_NETWORKD_DEFAULT_MASK);
    cfg.router = env_default("PUREDARWIN_NET_ROUTER", PD_NETWORKD_DEFAULT_ROUTER);
    cfg.dns = env_default("PUREDARWIN_NET_DNS", PD_NETWORKD_DEFAULT_DNS);
    cfg.broadcast = env_default("PUREDARWIN_NET_BROADCAST", PD_NETWORKD_DEFAULT_BROADCAST);
    cfg.wait_seconds = PD_NETWORKD_DEFAULT_WAIT_SECONDS;
    cfg.write_resolver = 1;

    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        if (strcmp(argv[argi], "--no-resolver") == 0) {
            cfg.write_resolver = 0;
            argi++;
        } else if (strcmp(argv[argi], "--wait") == 0 && argi + 1 < argc) {
            cfg.wait_seconds = atoi(argv[argi + 1]);
            if (cfg.wait_seconds < 0)
                cfg.wait_seconds = 0;
            argi += 2;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (argc - argi == 4 || argc - argi == 5 || argc - argi == 6) {
        cfg.ifname = argv[argi++];
        cfg.addr = argv[argi++];
        cfg.mask = argv[argi++];
        cfg.router = argv[argi++];
        if (argi < argc)
            cfg.dns = argv[argi++];
        if (argi < argc)
            cfg.broadcast = argv[argi++];
    } else if (argc != argi) {
        usage(argv[0]);
        return 2;
    }

    pd_log("configuring %s addr=%s mask=%s router=%s dns=%s",
        cfg.ifname, cfg.addr, cfg.mask, cfg.router, cfg.dns ? cfg.dns : "");
    return apply_network_config(&cfg);
}
