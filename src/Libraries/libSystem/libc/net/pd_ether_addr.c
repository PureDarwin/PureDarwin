#include <sys/types.h>
#include <net/ethernet.h>
#include <stdio.h>

struct ether_addr *
ether_aton(const char *a)
{
	static struct ether_addr addr;
	int i;
	unsigned int o0, o1, o2, o3, o4, o5;

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o3, &o4, &o5);
	if (i != 6) {
		return NULL;
	}
	addr.octet[0] = (unsigned char)o0;
	addr.octet[1] = (unsigned char)o1;
	addr.octet[2] = (unsigned char)o2;
	addr.octet[3] = (unsigned char)o3;
	addr.octet[4] = (unsigned char)o4;
	addr.octet[5] = (unsigned char)o5;
	return &addr;
}

char *
ether_ntoa(const struct ether_addr *n)
{
	static char a[18];

	snprintf(a, sizeof(a), "%x:%x:%x:%x:%x:%x",
	    n->octet[0], n->octet[1], n->octet[2],
	    n->octet[3], n->octet[4], n->octet[5]);
	return a;
}
