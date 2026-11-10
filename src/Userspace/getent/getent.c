// getent(1): prints entries from the account, host and network databases
// through Libinfo, in the same layout as the files they come from
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FOUND = 0, USAGE = 1, NOTFOUND = 2, NOENUM = 3 };

static int
parse_number(const char *s, unsigned long *out)
{
	char *end;

	*out = strtoul(s, &end, 10);
	return *s && !*end;
}

static void
print_passwd(const struct passwd *pw)
{
	printf("%s:%s:%u:%u:%s:%s:%s\n", pw->pw_name, pw->pw_passwd ? pw->pw_passwd : "*",
	    (unsigned)pw->pw_uid, (unsigned)pw->pw_gid, pw->pw_gecos ? pw->pw_gecos : "",
	    pw->pw_dir ? pw->pw_dir : "", pw->pw_shell ? pw->pw_shell : "");
}

static int
db_passwd(int argc, char **argv)
{
	struct passwd *pw;
	unsigned long n;
	int rc = FOUND;

	if (argc == 0) {
		setpwent();
		while ((pw = getpwent()) != NULL)
			print_passwd(pw);
		endpwent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		pw = parse_number(argv[i], &n) ? getpwuid((uid_t)n) : getpwnam(argv[i]);
		if (pw)
			print_passwd(pw);
		else
			rc = NOTFOUND;
	}
	return rc;
}

static void
print_group(const struct group *gr)
{
	printf("%s:%s:%u:", gr->gr_name, gr->gr_passwd ? gr->gr_passwd : "*",
	    (unsigned)gr->gr_gid);
	for (char **m = gr->gr_mem; m && *m; m++)
		printf("%s%s", m == gr->gr_mem ? "" : ",", *m);
	printf("\n");
}

static int
db_group(int argc, char **argv)
{
	struct group *gr;
	unsigned long n;
	int rc = FOUND;

	if (argc == 0) {
		setgrent();
		while ((gr = getgrent()) != NULL)
			print_group(gr);
		endgrent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		gr = parse_number(argv[i], &n) ? getgrgid((gid_t)n) : getgrnam(argv[i]);
		if (gr)
			print_group(gr);
		else
			rc = NOTFOUND;
	}
	return rc;
}

static void
print_hostent(const struct hostent *he)
{
	char addr[INET6_ADDRSTRLEN];

	for (char **a = he->h_addr_list; a && *a; a++) {
		inet_ntop(he->h_addrtype, *a, addr, sizeof(addr));
		printf("%-15s %s", addr, he->h_name);
		for (char **al = he->h_aliases; al && *al; al++)
			printf(" %s", *al);
		printf("\n");
	}
}

// hosts and ahosts: an address key is a reverse lookup, a name goes through getaddrinfo
static int
db_hosts(int argc, char **argv, int family)
{
	struct hostent *he;
	int rc = FOUND;

	if (argc == 0) {
		sethostent(0);
		while ((he = gethostent()) != NULL)
			print_hostent(he);
		endhostent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		unsigned char buf[sizeof(struct in6_addr)];
		struct addrinfo hints, *res, *ai;
		char addr[INET6_ADDRSTRLEN];

		if (inet_pton(AF_INET, argv[i], buf) == 1 || inet_pton(AF_INET6, argv[i], buf) == 1) {
			int af = strchr(argv[i], ':') ? AF_INET6 : AF_INET;

			he = gethostbyaddr((const char *)buf,
			    af == AF_INET6 ? sizeof(struct in6_addr) : sizeof(struct in_addr), af);
			if (he)
				print_hostent(he);
			else
				rc = NOTFOUND;
			continue;
		}
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_CANONNAME;
		if (getaddrinfo(argv[i], NULL, &hints, &res) != 0) {
			rc = NOTFOUND;
			continue;
		}
		for (ai = res; ai; ai = ai->ai_next) {
			const void *a = ai->ai_family == AF_INET6
			    ? (const void *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr
			    : (const void *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;

			inet_ntop(ai->ai_family, a, addr, sizeof(addr));
			printf("%-15s %s\n", addr, res->ai_canonname ? res->ai_canonname : argv[i]);
		}
		freeaddrinfo(res);
	}
	return rc;
}

static void
print_service(const struct servent *se)
{
	printf("%-21s %d/%s", se->s_name, ntohs(se->s_port), se->s_proto);
	for (char **a = se->s_aliases; a && *a; a++)
		printf(" %s", *a);
	printf("\n");
}

static int
db_services(int argc, char **argv)
{
	struct servent *se;
	int rc = FOUND;

	if (argc == 0) {
		setservent(0);
		while ((se = getservent()) != NULL)
			print_service(se);
		endservent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		char key[256], *proto;
		unsigned long n;

		// a key may name the protocol as in /etc/services, http/tcp or 80/tcp
		snprintf(key, sizeof(key), "%s", argv[i]);
		if ((proto = strchr(key, '/')) != NULL)
			*proto++ = '\0';
		se = parse_number(key, &n) ? getservbyport(htons((int)n), proto)
		                           : getservbyname(key, proto);
		if (se)
			print_service(se);
		else
			rc = NOTFOUND;
	}
	return rc;
}

static void
print_proto(const struct protoent *pe)
{
	printf("%-21s %d", pe->p_name, pe->p_proto);
	for (char **a = pe->p_aliases; a && *a; a++)
		printf(" %s", *a);
	printf("\n");
}

static int
db_protocols(int argc, char **argv)
{
	struct protoent *pe;
	int rc = FOUND;

	if (argc == 0) {
		setprotoent(0);
		while ((pe = getprotoent()) != NULL)
			print_proto(pe);
		endprotoent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		unsigned long n;

		pe = parse_number(argv[i], &n) ? getprotobynumber((int)n) : getprotobyname(argv[i]);
		if (pe)
			print_proto(pe);
		else
			rc = NOTFOUND;
	}
	return rc;
}

static void
print_net(const struct netent *ne)
{
	struct in_addr a;

	a.s_addr = htonl(ne->n_net);
	printf("%-21s %s", ne->n_name, inet_ntoa(a));
	for (char **al = ne->n_aliases; al && *al; al++)
		printf(" %s", *al);
	printf("\n");
}

static int
db_networks(int argc, char **argv)
{
	struct netent *ne;
	int rc = FOUND;

	if (argc == 0) {
		setnetent(0);
		while ((ne = getnetent()) != NULL)
			print_net(ne);
		endnetent();
		return FOUND;
	}
	for (int i = 0; i < argc; i++) {
		in_addr_t net = inet_network(argv[i]);

		ne = net != INADDR_NONE ? getnetbyaddr(net, AF_INET) : getnetbyname(argv[i]);
		if (ne)
			print_net(ne);
		else
			rc = NOTFOUND;
	}
	return rc;
}

static void
usage(void)
{
	fprintf(stderr, "usage: getent database [key ...]\n"
	    "databases: ahosts ahostsv4 ahostsv6 group hosts networks passwd protocols services\n");
	exit(USAGE);
}

int
main(int argc, char **argv)
{
	const char *db;

	if (argc < 2)
		usage();
	db = argv[1];
	argc -= 2;
	argv += 2;

	if (strcmp(db, "passwd") == 0)
		return db_passwd(argc, argv);
	if (strcmp(db, "group") == 0)
		return db_group(argc, argv);
	if (strcmp(db, "hosts") == 0 || strcmp(db, "ahosts") == 0)
		return db_hosts(argc, argv, AF_UNSPEC);
	if (strcmp(db, "ahostsv4") == 0)
		return db_hosts(argc, argv, AF_INET);
	if (strcmp(db, "ahostsv6") == 0)
		return db_hosts(argc, argv, AF_INET6);
	if (strcmp(db, "services") == 0)
		return db_services(argc, argv);
	if (strcmp(db, "protocols") == 0)
		return db_protocols(argc, argv);
	if (strcmp(db, "networks") == 0)
		return db_networks(argc, argv);
	fprintf(stderr, "Unknown database: %s\n", db);
	usage();
	return USAGE;
}
