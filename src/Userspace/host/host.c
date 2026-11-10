// host(1): DNS lookups through libresolv, with the output format of BIND's host
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int verbose;
static int all_types;

static const struct { const char *name; int type; } types[] = {
	{ "A", ns_t_a }, { "NS", ns_t_ns }, { "CNAME", ns_t_cname },
	{ "SOA", ns_t_soa }, { "PTR", ns_t_ptr }, { "HINFO", ns_t_hinfo },
	{ "MX", ns_t_mx }, { "TXT", ns_t_txt }, { "AAAA", ns_t_aaaa },
	{ "SRV", ns_t_srv }, { "NAPTR", ns_t_naptr }, { "DS", 43 },
	{ "RRSIG", 46 }, { "DNSKEY", 48 }, { "CAA", 257 }, { "HTTPS", 65 },
	{ "SVCB", 64 }, { "ANY", ns_t_any },
};

static const char *rcodes[] = {
	"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMP", "REFUSED",
};

static int
type_from_name(const char *name)
{
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		if (strcasecmp(types[i].name, name) == 0)
			return types[i].type;
	if (strncasecmp(name, "TYPE", 4) == 0)
		return atoi(name + 4);
	return -1;
}

static const char *
type_name(int type)
{
	static char buf[16];

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		if (types[i].type == type)
			return types[i].name;
	snprintf(buf, sizeof(buf), "TYPE%d", type);
	return buf;
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: host [-aCdilrTvVw] [-c class] [-N ndots] [-R number] [-t type]\n"
	    "            [-W time] [-m flag] [-4] [-6] [-s] hostname [server]\n");
	exit(1);
}

// a name as BIND prints it, ending in one dot, the root as "."
static const char *
fqdn(const char *name)
{
	static char buf[NS_MAXDNAME + 2];
	size_t len = strlen(name);

	if (len == 0 || name[len - 1] == '.')
		return len ? name : ".";
	snprintf(buf, sizeof(buf), "%s.", name);
	return buf;
}

// prints one answer record the way BIND's host does
static void
print_rr(const ns_msg *msg, const ns_rr *rr)
{
	const u_char *rd = ns_rr_rdata(*rr);
	char name[NS_MAXDNAME], text[4096];
	const char *owner = ns_rr_name(*rr);

	if (verbose || all_types) {
		if (ns_sprintrr(msg, rr, NULL, NULL, text, sizeof(text)) > 0)
			printf("%s\n", text);
		return;
	}

	switch (ns_rr_type(*rr)) {
	case ns_t_a:
		inet_ntop(AF_INET, rd, text, sizeof(text));
		printf("%s has address %s\n", owner, text);
		return;
	case ns_t_aaaa:
		inet_ntop(AF_INET6, rd, text, sizeof(text));
		printf("%s has IPv6 address %s\n", owner, text);
		return;
	case ns_t_cname:
		if (ns_name_uncompress(ns_msg_base(*msg), ns_msg_end(*msg), rd,
		    name, sizeof(name)) >= 0)
			printf("%s is an alias for %s\n", owner, fqdn(name));
		return;
	case ns_t_ptr:
		if (ns_name_uncompress(ns_msg_base(*msg), ns_msg_end(*msg), rd,
		    name, sizeof(name)) >= 0)
			printf("%s domain name pointer %s\n", owner, fqdn(name));
		return;
	case ns_t_ns:
		if (ns_name_uncompress(ns_msg_base(*msg), ns_msg_end(*msg), rd,
		    name, sizeof(name)) >= 0)
			printf("%s name server %s\n", owner, fqdn(name));
		return;
	case ns_t_mx:
		if (ns_name_uncompress(ns_msg_base(*msg), ns_msg_end(*msg), rd + 2,
		    name, sizeof(name)) >= 0)
			printf("%s mail is handled by %u %s\n", owner,
			    ns_get16(rd), fqdn(name));
		return;
	case ns_t_txt: {
		const u_char *p = rd, *end = rd + ns_rr_rdlen(*rr);

		printf("%s descriptive text", owner);
		while (p < end) {
			int len = *p++;

			if (p + len > end)
				break;
			printf(" \"%.*s\"", len, p);
			p += len;
		}
		printf("\n");
		return;
	}
	default:
		if (ns_sprintrr(msg, rr, NULL, NULL, text, sizeof(text)) > 0) {
			// drop the owner, ttl and class columns that ns_sprintrr prints
			char *data = strstr(text, type_name(ns_rr_type(*rr)));

			printf("%s has %s record %s\n", owner,
			    type_name(ns_rr_type(*rr)),
			    data ? data + strlen(type_name(ns_rr_type(*rr))) +
			    strspn(data + strlen(type_name(ns_rr_type(*rr))), " \t") : text);
		}
		return;
	}
}

// 0 when the name exists, 1 for NXDOMAIN or a failure
static int
lookup(res_state res, const char *name, int type, int show_missing)
{
	u_char answer[65535];
	ns_msg msg;
	ns_rr rr;
	int len, rcode, count, printed = 0;

	len = res_nsearch(res, name, ns_c_in, type, answer, sizeof(answer));
	if (len < 0) {
		switch (h_errno) {
		case HOST_NOT_FOUND:
			printf("Host %s not found: 3(NXDOMAIN)\n", name);
			return 1;
		case NO_DATA:
			if (show_missing)
				printf("%s has no %s record\n", name, type_name(type));
			return 0;
		case TRY_AGAIN:
			printf("Host %s not found: 2(SERVFAIL)\n", name);
			return 1;
		default:
			printf(";; connection timed out; no servers could be reached\n");
			return 1;
		}
	}
	if (ns_initparse(answer, len, &msg) < 0) {
		fprintf(stderr, "host: malformed reply for %s\n", name);
		return 1;
	}
	rcode = ns_msg_getflag(msg, ns_f_rcode);
	if (rcode != ns_r_noerror) {
		printf("Host %s not found: %d(%s)\n", name, rcode,
		    rcode < (int)(sizeof(rcodes) / sizeof(rcodes[0])) ? rcodes[rcode] : "ERROR");
		return 1;
	}
	if (verbose) {
		printf("Trying \"%s\"\n;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: %u\n",
		    name, ns_msg_id(msg));
		printf("\n;; ANSWER SECTION:\n");
	}
	count = ns_msg_count(msg, ns_s_an);
	for (int i = 0; i < count; i++) {
		if (ns_parserr(&msg, ns_s_an, i, &rr) < 0)
			continue;
		// a plain lookup shows the alias chain once, from the A query
		if (!verbose && !all_types && type != ns_t_a &&
		    ns_rr_type(rr) == ns_t_cname)
			continue;
		print_rr(&msg, &rr);
		printed++;
	}
	if (verbose)
		printf("\n");
	if (!printed && show_missing)
		printf("%s has no %s record\n", name, type_name(type));
	return 0;
}

// builds the in-addr.arpa or ip6.arpa name for an address, 0 if it is not one
static int
reverse_name(const char *addr, char *out, size_t outlen)
{
	struct in_addr a4;
	struct in6_addr a6;

	if (inet_pton(AF_INET, addr, &a4) == 1) {
		const u_char *b = (const u_char *)&a4;

		snprintf(out, outlen, "%u.%u.%u.%u.in-addr.arpa", b[3], b[2], b[1], b[0]);
		return 1;
	}
	if (inet_pton(AF_INET6, addr, &a6) == 1) {
		char *p = out;

		for (int i = 15; i >= 0; i--) {
			p += snprintf(p, outlen - (p - out), "%x.%x.",
			    a6.s6_addr[i] & 0xf, a6.s6_addr[i] >> 4);
		}
		snprintf(p, outlen - (p - out), "ip6.arpa");
		return 1;
	}
	return 0;
}

static int
use_server(res_state res, const char *server)
{
	struct addrinfo hints, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(server, "domain", &hints, &ai) != 0) {
		fprintf(stderr, "host: couldn't get address for '%s': not found\n", server);
		return 0;
	}
	memcpy(&res->nsaddr_list[0], ai->ai_addr, sizeof(struct sockaddr_in));
	res->nscount = 1;
	printf("Using domain server:\nName: %s\nAddress: %s#53\nAliases: \n\n", server,
	    inet_ntoa(((struct sockaddr_in *)ai->ai_addr)->sin_addr));
	freeaddrinfo(ai);
	return 1;
}

int
main(int argc, char **argv)
{
	struct __res_state res;
	char rname[NS_MAXDNAME];
	const char *name;
	int ch, type = -1, status = 0, wait_secs = -1, tries = -1;

	while ((ch = getopt(argc, argv, "46aCc:dilm:N:rR:sTt:UvVwW:")) != -1) {
		switch (ch) {
		case 'a':
			all_types = 1;
			verbose = 1;
			type = ns_t_any;
			break;
		case 'd':
		case 'v':
			verbose = 1;
			break;
		case 't':
			if ((type = type_from_name(optarg)) < 0) {
				fprintf(stderr, "host: invalid type: %s\n", optarg);
				return 1;
			}
			break;
		case 'W':
			wait_secs = atoi(optarg);
			break;
		case 'R':
			tries = atoi(optarg);
			break;
		case 'w':
			wait_secs = 3600;
			break;
		case 'V':
			printf("host (PureDarwin, libresolv)\n");
			return 0;
		case '4': case '6': case 'C': case 'c': case 'i': case 'l':
		case 'm': case 'N': case 'r': case 's': case 'T': case 'U':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1 || argc > 2)
		usage();
	name = argv[0];

	memset(&res, 0, sizeof(res));
	if (res_ninit(&res) != 0) {
		fprintf(stderr, "host: resolver initialisation failed\n");
		return 1;
	}
	if (wait_secs >= 0)
		res.retrans = wait_secs;
	if (tries >= 0)
		res.retry = tries;
	if (argc == 2 && !use_server(&res, argv[1]))
		return 1;

	if (reverse_name(name, rname, sizeof(rname))) {
		status = lookup(&res, rname, type < 0 ? ns_t_ptr : type, type >= 0);
	} else if (type >= 0) {
		status = lookup(&res, name, type, 1);
	} else {
		// with no type, BIND's host asks for A, AAAA and MX
		status = lookup(&res, name, ns_t_a, 0);
		if (status == 0) {
			lookup(&res, name, ns_t_aaaa, 0);
			lookup(&res, name, ns_t_mx, 0);
		}
	}
	res_ndestroy(&res);
	return status;
}
