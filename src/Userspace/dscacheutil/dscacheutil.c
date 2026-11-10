// dscacheutil(1): queries through Libinfo, which reads PD's dslocal accounts and
// the resolver. PD keeps no lookup cache, so the cache options have nothing to do.
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
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: dscacheutil -h\n"
	    "       dscacheutil -q category [-a key value]\n"
	    "       dscacheutil -cachedump [-buckets] [-entries [category]]\n"
	    "       dscacheutil -configuration\n"
	    "       dscacheutil -flushcache\n"
	    "       dscacheutil -statistics\n"
	    "\n"
	    "categories: group, host, mount, protocol, rpc, service, user\n");
	exit(1);
}

static void
print_user(const struct passwd *pw)
{
	printf("name: %s\npassword: %s\nuid: %u\ngid: %u\ndir: %s\nshell: %s\ngecos: %s\n\n",
	    pw->pw_name, pw->pw_passwd ? pw->pw_passwd : "*", (unsigned)pw->pw_uid,
	    (unsigned)pw->pw_gid, pw->pw_dir ? pw->pw_dir : "",
	    pw->pw_shell ? pw->pw_shell : "", pw->pw_gecos ? pw->pw_gecos : "");
}

static void
print_group(const struct group *gr)
{
	printf("name: %s\npassword: %s\ngid: %u\n", gr->gr_name,
	    gr->gr_passwd ? gr->gr_passwd : "*", (unsigned)gr->gr_gid);
	if (gr->gr_mem && gr->gr_mem[0]) {
		printf("users:");
		for (char **m = gr->gr_mem; *m; m++)
			printf(" %s", *m);
		printf("\n");
	}
	printf("\n");
}

static int
query_user(const char *key, const char *value)
{
	struct passwd *pw;

	if (!key) {
		setpwent();
		while ((pw = getpwent()) != NULL)
			print_user(pw);
		endpwent();
		return 0;
	}
	if (strcmp(key, "name") == 0)
		pw = getpwnam(value);
	else if (strcmp(key, "uid") == 0)
		pw = getpwuid((uid_t)strtoul(value, NULL, 10));
	else
		return 1;
	if (pw)
		print_user(pw);
	return 0;
}

static int
query_group(const char *key, const char *value)
{
	struct group *gr;

	if (!key) {
		setgrent();
		while ((gr = getgrent()) != NULL)
			print_group(gr);
		endgrent();
		return 0;
	}
	if (strcmp(key, "name") == 0)
		gr = getgrnam(value);
	else if (strcmp(key, "gid") == 0)
		gr = getgrgid((gid_t)strtoul(value, NULL, 10));
	else
		return 1;
	if (gr)
		print_group(gr);
	return 0;
}

// macOS prints the IPv6 entry first, then the IPv4 one
static void
print_host_family(const char *name, int family)
{
	struct addrinfo hints, *res, *ai;
	char addr[INET6_ADDRSTRLEN];
	int any = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if (getaddrinfo(name, NULL, &hints, &res) != 0)
		return;
	for (ai = res; ai; ai = ai->ai_next) {
		const void *a = family == AF_INET6
		    ? (const void *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr
		    : (const void *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;

		if (ai->ai_family != family)
			continue;
		if (!any) {
			printf("name: %s\n", res->ai_canonname ? res->ai_canonname : name);
			if (res->ai_canonname && strcmp(res->ai_canonname, name) != 0)
				printf("alias: %s\n", name);
			any = 1;
		}
		inet_ntop(family, a, addr, sizeof(addr));
		printf("%s: %s\n", family == AF_INET6 ? "ipv6_address" : "ip_address", addr);
	}
	if (any)
		printf("\n");
	freeaddrinfo(res);
}

static int
query_host(const char *key, const char *value)
{
	if (!key) {
		struct hostent *he;

		sethostent(0);
		while ((he = gethostent()) != NULL) {
			char addr[INET6_ADDRSTRLEN];

			printf("name: %s\n", he->h_name);
			for (char **a = he->h_aliases; a && *a; a++)
				printf("alias: %s\n", *a);
			for (char **a = he->h_addr_list; a && *a; a++) {
				inet_ntop(he->h_addrtype, *a, addr, sizeof(addr));
				printf("%s: %s\n", he->h_addrtype == AF_INET6 ? "ipv6_address" : "ip_address", addr);
			}
			printf("\n");
		}
		endhostent();
		return 0;
	}
	if (strcmp(key, "name") == 0) {
		print_host_family(value, AF_INET6);
		print_host_family(value, AF_INET);
		return 0;
	}
	if (strcmp(key, "ip_address") == 0 || strcmp(key, "ipv6_address") == 0) {
		int family = key[2] == 'v' ? AF_INET6 : AF_INET;
		unsigned char buf[sizeof(struct in6_addr)];
		struct hostent *he;
		char addr[INET6_ADDRSTRLEN];

		if (inet_pton(family, value, buf) != 1)
			return 1;
		he = gethostbyaddr((const char *)buf,
		    family == AF_INET6 ? sizeof(struct in6_addr) : sizeof(struct in_addr), family);
		if (!he)
			return 0;
		printf("name: %s\n", he->h_name);
		for (char **a = he->h_aliases; a && *a; a++)
			printf("alias: %s\n", *a);
		inet_ntop(family, buf, addr, sizeof(addr));
		printf("%s: %s\n\n", key, addr);
		return 0;
	}
	return 1;
}

static int
query_service(const char *key, const char *value)
{
	struct servent *se;

	if (!key) {
		setservent(0);
		while ((se = getservent()) != NULL)
			printf("name: %s\nport: %d\nprotocol: %s\n\n", se->s_name,
			    ntohs(se->s_port), se->s_proto);
		endservent();
		return 0;
	}
	if (strcmp(key, "name") == 0)
		se = getservbyname(value, NULL);
	else if (strcmp(key, "port") == 0)
		se = getservbyport(htons(atoi(value)), NULL);
	else
		return 1;
	if (se)
		printf("name: %s\nport: %d\nprotocol: %s\n\n", se->s_name,
		    ntohs(se->s_port), se->s_proto);
	return 0;
}

static int
query_protocol(const char *key, const char *value)
{
	struct protoent *pe;

	if (!key) {
		setprotoent(0);
		while ((pe = getprotoent()) != NULL)
			printf("name: %s\nnumber: %d\n\n", pe->p_name, pe->p_proto);
		endprotoent();
		return 0;
	}
	if (strcmp(key, "name") == 0)
		pe = getprotobyname(value);
	else if (strcmp(key, "number") == 0)
		pe = getprotobynumber(atoi(value));
	else
		return 1;
	if (pe)
		printf("name: %s\nnumber: %d\n\n", pe->p_name, pe->p_proto);
	return 0;
}

int
main(int argc, char **argv)
{
	const char *category = NULL, *key = NULL, *value = NULL;
	int rc;

	if (argc < 2)
		usage();
	if (strcmp(argv[1], "-flushcache") == 0)
		return 0;
	if (strcmp(argv[1], "-statistics") == 0 || strcmp(argv[1], "-cachedump") == 0 ||
	    strcmp(argv[1], "-configuration") == 0) {
		printf("No lookup cache, PureDarwin answers every query directly.\n");
		return 0;
	}
	if (strcmp(argv[1], "-q") != 0 || argc < 3)
		usage();
	category = argv[2];
	if (argc >= 4) {
		if (strcmp(argv[3], "-a") != 0 || argc != 6)
			usage();
		key = argv[4];
		value = argv[5];
	}

	if (strcmp(category, "user") == 0)
		rc = query_user(key, value);
	else if (strcmp(category, "group") == 0)
		rc = query_group(key, value);
	else if (strcmp(category, "host") == 0)
		rc = query_host(key, value);
	else if (strcmp(category, "service") == 0)
		rc = query_service(key, value);
	else if (strcmp(category, "protocol") == 0)
		rc = query_protocol(key, value);
	else if (strcmp(category, "mount") == 0 || strcmp(category, "rpc") == 0)
		rc = 0;
	else
		usage();
	if (rc)
		fprintf(stderr, "dscacheutil: unknown key %s for %s\n", key, category);
	return rc;
}
