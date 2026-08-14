/*
 * Copyright (c) 2014-2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * IPv4ClasslessRoute.c
 * - handle IPv4 route lists in DHCP options
 */

/*
 * Modification History
 *
 * June 5, 2014			Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <mach/boolean.h>
#include <stddef.h>
#include <arpa/inet.h>
#include "IPConfigurationLog.h"
#include "IPv4ClasslessRoute.h"
#include "symbol_scope.h"
#include "cfutil.h"
#include "util.h"

/**
 ** IPv4ClasslessRouteListBuf
 **/
#ifdef TEST_IPV4ROUTE
#define DEBUG 1
#endif


#define N_BITS_PER_BYTE			8

#define ROUTELIST_BUF_INITIAL_SIZE	64
typedef struct {
    uint8_t	rb_buf_s[ROUTELIST_BUF_INITIAL_SIZE];
    bool	rb_buf_user_supplied;
    uint8_t *	rb_buf;
    int		rb_buf_used;
    int		rb_buf_size;
} IPv4ClasslessRouteListBuf, * IPv4ClasslessRouteListBufRef;

STATIC void
IPv4ClasslessRouteListBufInit(IPv4ClasslessRouteListBufRef rb, 
			      uint8_t * buf, int buf_size)
{
    bzero(rb, sizeof(*rb));
    if (buf != NULL) {
	rb->rb_buf_user_supplied = TRUE;
	rb->rb_buf = buf;
	rb->rb_buf_size = buf_size;
    }
    else {
	rb->rb_buf = rb->rb_buf_s;
	rb->rb_buf_size = sizeof(rb->rb_buf_s);
    }
    return;
}

STATIC void
IPv4ClasslessRouteListBufFree(IPv4ClasslessRouteListBufRef rb)
{
    if (rb->rb_buf_user_supplied == FALSE) {
	if (rb->rb_buf != NULL && rb->rb_buf != rb->rb_buf_s) {
	    free(rb->rb_buf);
	}
    }
    bzero(rb, sizeof(*rb));
    return;
}

STATIC __inline__ int
IPv4ClasslessRouteListBufUsed(IPv4ClasslessRouteListBufRef rb)
{
    return (rb->rb_buf_used);
}

STATIC __inline__ uint8_t *
IPv4ClasslessRouteListBufBuffer(IPv4ClasslessRouteListBufRef rb)
{
    return (rb->rb_buf);
}

STATIC bool
IPv4ClasslessRouteListBufAddData(IPv4ClasslessRouteListBufRef rb,
				 const void * data, int data_len)
{
#ifdef DEBUG
    printf("Adding: ");
    print_data((void *)data, data_len);
#endif /* DEBUG */

    if (data_len > (rb->rb_buf_size - rb->rb_buf_used)) {
	if (rb->rb_buf_user_supplied) {
	    IPConfigLogFL(LOG_NOTICE,
			  "user-supplied buffer failed to add data with"
			  " length %d (> %d)",
			  data_len, (rb->rb_buf_size - rb->rb_buf_used));
	    return (FALSE);
	}
#ifdef DEBUG
	printf("Buffer growing from %d to %d\n", rb->rb_buf_size,
	       rb->rb_buf_size + ((data_len > ROUTELIST_BUF_INITIAL_SIZE) 
	       ? data_len : ROUTELIST_BUF_INITIAL_SIZE));
#endif /* DEBUG */
	rb->rb_buf_size += (data_len > ROUTELIST_BUF_INITIAL_SIZE) 
	    ? data_len : ROUTELIST_BUF_INITIAL_SIZE;
	if (rb->rb_buf == rb->rb_buf_s) {
	    rb->rb_buf = malloc(rb->rb_buf_size);
	    memcpy(rb->rb_buf, rb->rb_buf_s, rb->rb_buf_used);
	}
	else {
	    rb->rb_buf = reallocf(rb->rb_buf, rb->rb_buf_size);
	    if (rb->rb_buf == NULL) {
		return (FALSE);
	    }
	}
    }
    memcpy(rb->rb_buf + rb->rb_buf_used, data, data_len);
    rb->rb_buf_used += data_len;
    return (TRUE);
}

STATIC bool
IPv4ClasslessRouteListBufAddRoute(IPv4ClasslessRouteListBufRef rb,
				  IPv4ClasslessRouteRef element)
{
    uint8_t	prefix_length;
    bool	added = FALSE;

    prefix_length = element->prefix_length;
    if (prefix_length > 32) {
	return (FALSE);
    }
    added = IPv4ClasslessRouteListBufAddData(rb, &prefix_length,
					     sizeof(prefix_length));
    if (added) {
	if (prefix_length > 0) {
	    int		length;
	    
	    length = roundup(prefix_length, N_BITS_PER_BYTE)
		/ N_BITS_PER_BYTE;
	    added = IPv4ClasslessRouteListBufAddData(rb, &element->dest,
						     length);
	}
	if (added) {
	    added = IPv4ClasslessRouteListBufAddData(rb, &element->gate,
					    sizeof(element->gate));
	}
    }
    return (added);
}

PRIVATE_EXTERN uint8_t *
IPv4ClasslessRouteListBufferCreate(IPv4ClasslessRouteRef list, int list_count,
				   uint8_t * buffer, int * buffer_size)
{
    int				bufsize = *buffer_size;
    int				i;
    IPv4ClasslessRouteListBuf	rb;
    int				used = 0;

    if (list == NULL || list_count == 0) {
	goto failed;
    }
    if (buffer == NULL) {
	bufsize = 0;
    }
    else if (bufsize == 0) {
	goto failed;
    }
    IPv4ClasslessRouteListBufInit(&rb, buffer, bufsize);
    for (i = 0; i < list_count; i++) {
	if (IPv4ClasslessRouteListBufAddRoute(&rb, list + i) == FALSE) {
	    IPConfigLogFL(LOG_NOTICE, "failed to add " IP_FORMAT,
			  IP_LIST(&list[i].dest));
	    IPv4ClasslessRouteListBufFree(&rb);
	    if (buffer != NULL) {
		buffer = NULL;
	    }
	    goto failed;
	}
    }
    used = IPv4ClasslessRouteListBufUsed(&rb);
    if (buffer == NULL) {
	/* make a copy of just the buffer and return it */
	buffer = malloc(used);
	memcpy(buffer, IPv4ClasslessRouteListBufBuffer(&rb), used);
    }
    IPv4ClasslessRouteListBufFree(&rb);

 failed:
    *buffer_size = used;
    return (buffer);
}

#ifdef TEST_IPV4ROUTE
STATIC void
IPv4ClasslessRouteListPrint(IPv4ClasslessRouteRef list, int list_count)
{
    int		i;

    printf("%d routes\n", list_count);
    for (i = 0; i < list_count; i++) {
	char	buf[32];
       
	snprintf(buf, sizeof(buf), "%s/%d",
	       inet_ntoa(list[i].dest), list[i].prefix_length);
	printf("%d. %-20s%-20s\n", i + 1, buf, inet_ntoa(list[i].gate));
    }
    return;
}
#endif /* TEST_IPV4ROUTE */

STATIC int
IPv4ClasslessRouteListParse(const uint8_t * buffer, int buffer_size,
			    IPv4ClasslessRouteRef list, int list_count)
{
    int			count = 0;
    int			left;
    const uint8_t * 	scan;

    for (left = buffer_size, scan = buffer;
	 left > 0; ) {
	int		length;
	int		needed;
	uint8_t		prefix_length = scan[0];

	if (prefix_length > 32) {
	    IPConfigLogFL(LOG_NOTICE,
			  "prefix length is %d (> 32)",
			  prefix_length);
	    count = 0;
	    break;
	}
	length = roundup(prefix_length, N_BITS_PER_BYTE) / N_BITS_PER_BYTE;
	needed = 1 + length + sizeof(struct in_addr);
	if (left < needed) {
	    IPConfigLogFL(LOG_NOTICE,
			  "truncated descriptor %d < %d",
			  left, needed);
	    count = 0;
	    break;
	}
	if (list != NULL) {
	    if (count >= list_count) {
		IPConfigLogFL(LOG_NOTICE,
			      "supplied route list too small (%d elements)",
			      list_count);
		count = 0;
		break;
	    }
	    list[count].prefix_length = prefix_length;
	    list[count].dest.s_addr = 0;
	    memcpy(&list[count].dest, scan + 1, length);
	    memcpy(&list[count].gate, scan + 1 + length,
		   sizeof(list[count].gate));
	}
	count++;
	left -= needed;
	scan += needed;
    }
    return (count);
}

PRIVATE_EXTERN IPv4ClasslessRouteRef
IPv4ClasslessRouteListCreate(const uint8_t * buffer, int buffer_size,
			     int * list_count)
{
    int				count = 0;
    IPv4ClasslessRouteRef	list = NULL;

    if (buffer == NULL || buffer_size == 0) {
	goto failed;
    }
    count = IPv4ClasslessRouteListParse(buffer, buffer_size, NULL, 0);
    if (count != 0) {
	list = (IPv4ClasslessRouteRef)malloc(sizeof(*list) * count);
	if (IPv4ClasslessRouteListParse(buffer, buffer_size, list, count)
	    != count) {
	    free(list);
	    list = NULL;
	    count = 0;
	}
    }
 failed:
    *list_count = count;
    return (list);
}

STATIC bool
parse_destination_descriptor(CFStringRef str, struct in_addr * ip,
			     int * ret_prefix_length)
{
    char *	descr;
    bool	parsed = FALSE;
    int		prefix_length = -1;
    char *	slash;

    descr = my_CFStringToCString(str, kCFStringEncodingASCII);
    if (descr == NULL) {
	goto failed;
    }
    slash = strchr(descr, '/');
    if (slash != NULL) {
	*slash = '\0';
	prefix_length = strtoul(slash + 1, NULL, 0);
	if (prefix_length < 0 || prefix_length > 32) {
	    IPConfigLogFL(LOG_NOTICE,
			  "invalid prefix length %d", prefix_length);
	    goto failed;
	}
    }
    if (inet_aton(descr, ip) == 1) {
	uint32_t 	ipval;

	ipval = ntohl(ip->s_addr);	
	if (prefix_length == -1) {
	    if (IN_CLASSA(ipval)) {
		prefix_length = 8;
	    }
	    else if (IN_CLASSB(ipval)) {
		prefix_length = 16;
	    }
	    else {
		prefix_length = 24;
	    }
	    
	}
	if ((ipval & prefix_to_mask32(prefix_length)) == 0) {
	    /* make sure default route (0.0.0.0) prefix length is zero */
	    ip->s_addr = 0;
	    prefix_length = 0;
	}
	*ret_prefix_length = prefix_length;
	parsed = TRUE;
    }

 failed:
    if (descr != NULL) {
	free(descr);
    }
    return (parsed);
    
}

PRIVATE_EXTERN IPv4ClasslessRouteRef
IPv4ClasslessRouteListCreateWithArray(CFArrayRef string_list,
				      int * ret_count)
{
    CFIndex			count;
    bool			has_default = FALSE;
    CFIndex			i;
    IPv4ClasslessRouteRef	list = NULL;
    CFIndex			list_count = 0;
    IPv4ClasslessRouteRef	scan;

    count = CFArrayGetCount(string_list);
    if (count == 0) {
	goto done;
    }
    if (count & 1) {
	/* must appear in pairs */
	IPConfigLogFL(LOG_NOTICE,
		      "Classless route requires pairs of IP address values");
	goto done;
    }
    count = count / 2;

    /* allocate an array one larger to hold default route */
    list = (IPv4ClasslessRouteRef)malloc(sizeof(*list) * (count + 1));
    for (scan = list + 1, i = 0; i < count; i++) {
	CFStringRef	dest = CFArrayGetValueAtIndex(string_list, 2 * i); 
	CFStringRef	gate = CFArrayGetValueAtIndex(string_list, 2 * i + 1);

	if (isA_CFString(dest) == NULL
	    || isA_CFString(gate) == NULL) {
	    /* each element needs to be a string */
	    IPConfigLogFL(LOG_NOTICE, "Classless route array contains non-string");
	    list_count = 0;
	    break;
	}
	/* destination is expressed as "ip[/prefix-length]" */
	if (parse_destination_descriptor(dest, &scan->dest,
					 &scan->prefix_length)
	    == FALSE) {
	    IPConfigLogFL(LOG_NOTICE,
			  "Invalid route destination descriptor '%@'",
			  dest);
	    list_count = 0;
	    break;
	}
	if (my_CFStringToIPAddress(gate, &scan->gate) == FALSE) {
	    IPConfigLogFL(LOG_NOTICE,
			  "Invalid route gateway address '%@'", gate);
	    list_count = 0;
	    break;
	}
	if (scan->dest.s_addr == 0 && has_default == FALSE) {
	    has_default = TRUE;
	    list[0] = *scan;
	}
	else {
	    scan++;
	}
	list_count++;
    }
    if (list_count == 0) {
	free(list);
	list = NULL;
    }
    else if (has_default == FALSE) {
	/* insert the default route at [0] */
	list[0].dest.s_addr = 0;
	list[0].gate.s_addr = 0;
	list[0].prefix_length = 0;
	list_count++;
    }

 done:
    *ret_count = list_count;
    return (list);
}

PRIVATE_EXTERN IPv4ClasslessRouteRef
IPv4ClasslessRouteListGetDefault(IPv4ClasslessRouteRef list, int list_count)
{
    int				i;	
    IPv4ClasslessRouteRef	scan;

    if (list == NULL) {
	return (NULL);
    }
    for (i = 0, scan = list; i < list_count; i++, scan++) {
	if (scan->dest.s_addr == 0) {
	    return (scan);
	}
    }
    return (NULL);
}


#ifdef TEST_IPV4ROUTE

typedef struct {
    const char *		dest;
    const char *		gate;
    uint8_t			prefix_length;
} TestRoute, * TestRouteRef;

typedef struct {
    const TestRouteRef	list;
    int				count;
} TestEntry;

typedef struct {
    const uint8_t *		buf;
    int				buf_size;
    const char *		description;
    bool			good;
} TestBufEntry;

STATIC IPv4ClasslessRouteRef
TestRouteCreateIPv4ClasslessRoute(TestRoute * list, int count)
{
    int			i;
    IPv4ClasslessRouteRef	ret_list;

    ret_list = (IPv4ClasslessRouteRef)
	malloc(sizeof(IPv4ClasslessRoute) * count);
    for (i = 0; i < count; i++) {
	inet_aton(list[i].dest, &ret_list[i].dest);
	inet_aton(list[i].gate, &ret_list[i].gate);
	ret_list[i].prefix_length = list[i].prefix_length;
    }
    return (ret_list);
}

STATIC TestRoute test_routes_1[] = {
    { "0.0.0.0",	"192.0.1.1", 	0 },
    { "10.0.1.1",	"1.2.3.4", 	24 },
    { "10.0.2.1",	"2.3.4.5",	24 },
    { "10.0.3.1",	"3.4.5.6",	24 },
    { "10.0.4.1",	"0.0.0.0",	24 },
    { "1.1.1.1",	"1.2.3.4",	1 },
    { "66.66.66.254",	"192.0.1.1",	31 },
    { "192.168.2.0",	"192.0.1.1",	30 },
    { "172.16.1.0",	"192.0.1.1",	30 },
};

STATIC TestRoute test_routes_2[] = {
    { "0.0.0.0",	"9.9.9.9",	0 },
    { "10.0.0.0",	"1.1.1.1",	8 },
    { "10.0.0.0",	"2.2.2.2",	24 },
    { "10.17.0.0",	"3.3.3.3",	16, },
    { "10.27.129.0",	"4.4.4.4",	24 },
    { "10.229.0.128",	"5.5.5.5",	25 },
    { "10.198.122.47",	"6.6.6.6",  	32 },
    { "129.210.177.132", "7.7.7.7",	25 }
};

STATIC TestEntry tests[] = {
    { test_routes_1, sizeof(test_routes_1) / sizeof(test_routes_1[0]) },
    { test_routes_2, sizeof(test_routes_2) / sizeof(test_routes_2[0]) },
};

STATIC const uint8_t test_bad_buf_1[] = {
    24,
    10, 0, 0
};

STATIC const uint8_t test_bad_buf_2[] = {
    0,
    1, 2, 3, 4,
    24
};

STATIC const uint8_t test_bad_buf_3[] = {
    0,
    1, 2, 3, 4,
    24,
    192, 168, 1,
    5, 6, 7, 8,
    255
};

STATIC const uint8_t test_good_buf_1[] = {
    1,
    10,
    1, 2, 3, 4,
    24,
    192, 168, 1,
    5, 6, 7, 8,
};

STATIC TestBufEntry test_bufs[] = {
    { test_bad_buf_1, sizeof(test_bad_buf_1), "too short", FALSE },
    { test_bad_buf_2, sizeof(test_bad_buf_2), "too short 2", FALSE },
    { test_bad_buf_3, sizeof(test_bad_buf_3), "bad prefix length", FALSE },
    { test_good_buf_1, sizeof(test_good_buf_1), "good 1", TRUE },
};

int
main(int argc, char * argv[])
{
    int			count;
    int			i;

    count = sizeof(tests) / sizeof(tests[0]);
    for (i = 0; i < count; i++) {
	IPv4ClasslessRouteRef	list;
	int			list_count;
	IPv4ClasslessRouteRef	list_copy;
	int			list_copy_count;
	uint8_t *		routes_buf;
	int			routes_buf_size;

	printf("______________________________\n");
	printf("test #%d.\n", i + 1);
	list_count = tests[i].count;
	list = TestRouteCreateIPv4ClasslessRoute(tests[i].list, list_count);
	routes_buf = IPv4ClasslessRouteListBufferCreate(list, list_count,
							NULL, &routes_buf_size);
	IPv4ClasslessRouteListPrint(list, list_count);
	print_data(routes_buf, routes_buf_size);
	list_copy = IPv4ClasslessRouteListCreate(routes_buf, routes_buf_size,
						 &list_copy_count);
	if (list_copy == NULL || list_copy_count != list_count) {
	    fprintf(stderr,
		    "%d: list_copy = %p, list_copy_count %d, list_count %d\n",
		    i, list_copy, list_copy_count, list_count);
	    exit(1);
	}
	IPv4ClasslessRouteListPrint(list_copy, list_copy_count);
	free(routes_buf);
	free(list_copy);
	free(list);
    }

    count = sizeof(test_bufs) / sizeof(test_bufs[0]);
    for (i = 0; i < count; i++) {
	IPv4ClasslessRouteRef	list;
	int			list_count;

	list = IPv4ClasslessRouteListCreate(test_bufs[i].buf,
					    test_bufs[i].buf_size,
					    &list_count);
	if (list == NULL) {
	    if (test_bufs[i].good == TRUE) {
		fprintf(stderr, "Test %d (%s) FAILED\n", i + 1,
			test_bufs[i].description);
		fprintf(stderr, "good data failed to parse\n");
		exit(1);
	    }
	    else {
		printf("Test %d (%s) SUCCESS\n", i + 1,
		       test_bufs[i].description);

	    }
	}
	else {
	    if (test_bufs[i].good == FALSE) {
		fprintf(stderr, "Test %d (%s) FAILED\n", i + 1,
			test_bufs[i].description);

		fprintf(stderr, "bad data parsed OK\n");
		exit(1);
	    }
	    else {
		printf("Test %d (%s) SUCCESS\n", i + 1,
		       test_bufs[i].description);

	    }
	    IPv4ClasslessRouteListPrint(list, list_count);
	    free(list);
	}
    }

    exit(0);
    return (0);
}

#endif /* TEST_IPV4ROUTE */
